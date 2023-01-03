#include "QuesadillaServer.h"
#include "utils.h"
#include "date.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <map>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <chrono>

#include "pistache/http.h"
#include "pistache/router.h"
#include "pistache/endpoint.h"
#include "pistache/client.h"

#include "rasty.h"
#include "Camera.h"
#include "Configuration.h"
#include "Renderer.h"
#include "TransferFunction.h"

namespace ques {

using namespace Pistache;

QuesadillaServer::QuesadillaServer(Pistache::Address addr, std::map<std::string, 
        ques::rasty_container> vm):  
    httpEndpoint(std::make_shared<Pistache::Http::Endpoint>(addr)), rasty_map(vm)
{
}

void QuesadillaServer::init(std::string app_dir, size_t threads)
{
    auto opts = Pistache::Http::Endpoint::options()
        .threads(threads)
        .flags(Pistache::Tcp::Options::ReuseAddr | 
                Pistache::Tcp::Options::ReusePort);
                
    this->httpEndpoint->init(opts);
    this->app_dir = app_dir;

    setupRoutes();
}

void QuesadillaServer::start()
{
    std::cout << "Listening..." << std::endl;
    this->httpEndpoint->setHandler(router.handler());
    this->httpEndpoint->serveThreaded();
}

void QuesadillaServer::shutdown()
{
    std::cout<<"Shutting down."<<std::endl;
    this->httpEndpoint->shutdown();
}

void QuesadillaServer::setupRoutes()
{
    using namespace Pistache::Rest;
    // serving html files
    Routes::Get(router, "/:filename?", Routes::bind(&QuesadillaServer::handleRoot, this));
    // serving static js files
    Routes::Get(router, "/js/:filename", 
            Routes::bind(&QuesadillaServer::handleJS, this));
    // serving static css files
    Routes::Get(router, "/css/:filename",
            Routes::bind(&QuesadillaServer::handleCSS, this));
    // serving variable names
    Routes::Get(router, "/var/:dataset",
            Routes::bind(&QuesadillaServer::handleVarList, this));
    // setting options endpoint (for CORS)
    Routes::Options(router, "/var/:dataset",
        Routes::bind(&QuesadillaServer::handleVarList, this));
    // serving renders  
    Routes::Get(router, "/image/:dataset/:x/:y/:z/:upx/:upy/:upz/:vx/:vy/:vz/:imagesize/:options?",
            Routes::bind(&QuesadillaServer::handleImage, this));
    // routing to plugins
    Routes::Get(router, "/extern/:plugin/:args?", 
            Routes::bind(&QuesadillaServer::handleExternalCommand, this));
    Routes::Post(router, "/config/:configname", 
            Routes::bind(&QuesadillaServer::handleConfiguration, this));
    Routes::Get(router, "/app/data/:filename", 
            Routes::bind(&QuesadillaServer::handleAppData, this));

}

void QuesadillaServer::serveFile(Pistache::Http::ResponseWriter &response,
        std::string filename) 
{
    std::cout << "INFO [" << this->getLogTime() << "] serveFile (" << filename << ")" << std::endl; 
    try
    {
        std::string app_path = this->app_dir + "/" + filename;
        Http::serveFile(response, app_path.c_str());
    }
    catch(...)
    {
        Http::serveFile(response, filename.c_str());
    }
}


void QuesadillaServer::handleRoot(const Rest::Request &request,
        Pistache::Http::ResponseWriter response) 
{
    std::cout << "INFO [" << this->getLogTime() << "] handleRoot" << std::endl; 

    std::string filename = "index.html"; // default root
    if (request.hasParam(":filename"))
    {
        filename = request.param(":filename").as<std::string>();
    }
    serveFile(response, filename.c_str());
}

void QuesadillaServer::handleJS(const Rest::Request &request, 
        Pistache::Http::ResponseWriter response)
{
    std::cout << "INFO [" << this->getLogTime() << "] handleJS" << std::endl; 
    auto filename = request.param(":filename").as<std::string>();
    filename = "js/" + filename;
    serveFile(response, filename.c_str());
}

void QuesadillaServer::handleCSS(const Rest::Request &request, 
        Pistache::Http::ResponseWriter response)
{
    std::cout << "INFO [" << this->getLogTime() << "] handleCSS" << std::endl; 
    auto filename = request.param(":filename").as<std::string>();
    filename = "css/" + filename;
    serveFile(response, filename.c_str());
}

void QuesadillaServer::handleExternalCommand(const Rest::Request &request, 
        Pistache::Http::ResponseWriter response) 
{

    std::string program = request.param(":plugin").as<std::string>();
    std::string args = "";

    if (request.hasParam(":args"))
    {
        args = request.param(":args").as<std::string>(); 
    }

    std::string command = this->app_dir + "/plugins/" + program + " " + args;
    std::cout << "INFO [" << this->getLogTime() << "] handleExternalCommand (" << command << ")" << std::endl; 

    std::string json_results = exec(command.c_str());
    response.send(Http::Code::Ok, json_results);
}

/**
 * Returns the list of variables and the allowed timesteps for the given dataset
*/
void QuesadillaServer::handleVarList(const Rest::Request &request,
        Pistache::Http::ResponseWriter response)
{
    std::string dataset = "";

    // needed for preflight requests (CORS)
    if (request.method() == Pistache::Http::Method::Options)
    {
        std::cout << "INFO [" << this->getLogTime() << "] handleVarList:OPTIONS" << std::endl; 
        response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
        response.headers().add<Pistache::Http::Header::AccessControlAllowMethods>("GET,OPTIONS");
        response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>("Access-Control-Allow-Origin, Access-Control-Allow-Headers, Origin,Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers");
        response.headers().add<Pistache::Http::Header::ContentType>(MIME(Text, Plain));
        response.send(Http::Code::Ok, "OK");
        return;
    }

    // ensure dataset is valid
    if (request.hasParam(":dataset"))
    {
        dataset = request.param(":dataset").as<std::string>();
    }
    std::cout << "INFO [" << this->getLogTime() << "] handleVarList:GET (" << dataset << ")" << std::endl; 
    if (rasty_map.count(dataset) == 0)
    {
        std::cout << "ERROR [" << this->getLogTime() << "] handleVarList:GET (Datset does not exist)" << std::endl; 
        response.send(Http::Code::Not_Found, "Datset does not exist");
        return;
    }

    // retrieve the datset
    rasty::Configuration *config = std::get<0>(rasty_map[dataset]);
    ques::Dataset *udataset = std::get<1>(rasty_map[dataset]);
    rasty::Camera *camera = std::get<2>(rasty_map[dataset]);
    rasty::Renderer *renderer = std::get<3>(rasty_map[dataset]);
    rasty::DataFile *temp_data;
    temp_data = udataset->data;

    // build the json response
    std::string json_results = "{ \"variables\": [";
    for (int i = 0; i < temp_data->variables.size(); i++)
    {
        json_results += "\"" + temp_data->variables[i] + "\"";
        if (i < temp_data->variables.size() - 1)
        {
            json_results += ",";
        }
    }
    json_results += "], ";
    json_results += "\"timesteps\": ";
    json_results += std::to_string(temp_data->timeDim);
    json_results += " }";
    
    response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
    response.send(Http::Code::Ok, json_results);
}

/**
 * Renders the requested image 
*/
void QuesadillaServer::handleImage(const Rest::Request &request,
        Pistache::Http::ResponseWriter response)
{

    std::string request_uri = "/image/";
    std::string dataset = "";
    int imagesize = 0;
    std::string format = "png";
    std::string varname = "";

    int camera_x = 0;
    int camera_y = 0;
    int camera_z = 0;

    float up_x = 0;
    float up_y = 1;
    float up_z = 0;

    float view_x = 0;
    float view_y = 0;
    float view_z = 1;

    // extract the parameters from the request
    if (request.hasParam(":dataset"))
    {
        dataset = request.param(":dataset").as<std::string>();
        imagesize = request.param(":imagesize").as<int>();

        camera_x = request.param(":x").as<std::int32_t>(); 
        camera_y = request.param(":y").as<std::int32_t>(); 
        camera_z = request.param(":z").as<std::int32_t>(); 

        up_x = request.param(":upx").as<float>();
        up_y = request.param(":upy").as<float>();
        up_z = request.param(":upz").as<float>();

        view_x = request.param(":vx").as<float>();
        view_y = request.param(":vy").as<float>();
        view_z = request.param(":vz").as<float>();

        request_uri += dataset + "/" + 
            std::to_string(camera_x) + "/" + std::to_string(camera_y) + "/" + std::to_string(camera_z) + "/" +
            std::to_string(up_x) + "/" + std::to_string(up_y) + "/" + std::to_string(up_z) + "/" + 
            std::to_string(view_x) + "/" + std::to_string(view_y) + "/" + std::to_string(view_z) + "/"
            + std::to_string(imagesize) + "/"; 
    }
    std::cout << "INFO [" << this->getLogTime() << "] handleImage (" << dataset << ")" << std::endl; 

    // Check if this dataset exists in the loaded datasets
    if (rasty_map.count(dataset) == 0)
    {
        std::cout << "ERROR [" << this->getLogTime() << "] handleImage (Datset does not exist)" << std::endl; 
        response.send(Http::Code::Not_Found, "Datset does not exist");
        return;
    }

    // retrieve the dataset
    rasty::Configuration *config = std::get<0>(rasty_map[dataset]);
    ques::Dataset *udataset = std::get<1>(rasty_map[dataset]);
    rasty::Camera *camera = std::get<2>(rasty_map[dataset]);
    rasty::Renderer *renderer = std::get<3>(rasty_map[dataset]);
    std::vector<unsigned char> image_data;

    bool onlysave = false;
    bool do_isosurface = false;
    std::string filename = "";
    std::string save_filename;
    rasty::Raster *temp_raster;  
    rasty::DataFile *temp_data;
    bool has_timesteps = false;
    bool do_tiling = false;
    int n_cols = 1;
    int timestep = 0;

    // set a default volume based on the dataset type
    // it'll be either a timeseries volume from timestep 0
    // or the only single volume that exists
    // rasty::CONFSTATE single_multi = config->getGeoConfigState();
    // if (single_multi == rasty::CONFSTATE::SINGLE_NOVAR 
    //         || single_multi == rasty::CONFSTATE::SINGLE_VAR)
    // {
        // std::cout << "single volume" << std::endl;
    temp_raster = udataset->raster; //by default
    temp_data = udataset->data; //by default
    if (temp_data->timeDim > 1)
    {
        has_timesteps = true;
    }
    // }

    varname = temp_data->variables[0];

    // set the full region of the image as the default
    camera->setRegion(1., 1., 0., 0.);

    // parse the request's extra options
    if (request.hasParam(":options"))
    {
        std::string options_line = request.param(":options").as<std::string>();
        request_uri += options_line;
        std::vector<std::string> options;
        const char *options_chars = options_line.c_str();
        do
        {
            const char *begin = options_chars;
            while(*options_chars != ',' && *options_chars)
                options_chars++;
            options.push_back(std::string(begin, options_chars));

        } while(0 != *options_chars++);

        for (auto it = options.begin(); it != options.end(); it++)
        {
            if (*it == "tiling")
            {
                it++;
                std::string tile_str = *it;
                const char *tile_char = tile_str.c_str();
                std::vector<int> tile_values;

                // get the tile index and number of tiles
                // "tiling,3-16" -> tile index 3 of 16 tiles
                do {
                    const char *t_begin = tile_char;
                    while(*tile_char != '-' && *tile_char)
                        tile_char++;
                    tile_values.push_back(std::stoi(std::string(t_begin, tile_char)));
                } while(0 != *tile_char++);

                // calculate the region of the image for this tile
                // and tell the camera to only render that region
                n_cols = (int) sqrtf(tile_values[1]);
                std::vector<float> region;
                this->calculateTileRegion(tile_values[0], tile_values[1], n_cols, region);
                camera->setRegion(region[0], region[1], region[2], region[3]);
            }

            if (*it == "format") {
                it++;
                format = *it;
            }

            if (*it == "variable") {
                it++;
                varname = *it;
            }

            if (*it == "timestep")
            {
                it++;
                timestep = std::stoi(*it);
            }

            if (*it == "onlysave")
            {
                it++;
                onlysave = true;
                save_filename = *it;
            }

        }
    }

    // load request variable if sepecified
    if (varname != ""){
        // ensure variable exists and load its metadata
        auto it = temp_data->varmap.find(varname);
        if (it->second.isNull()) {
            std::cout << "ERROR [" << this->getLogTime() << "] handleImage (Invalid Variable: " << varname << ")" << std::endl; 
            response.send(Http::Code::Not_Found, "Variable does not exist");
            return;
        }
        temp_data->loadVariable(varname);

        // load timestep here ( this is what actually loads data into memory )
        if (has_timesteps && timestep >= 0 && 
                timestep < udataset->data->timeDim)
        {
            temp_data->loadTimeStep(timestep);
        }
        else
        {
            std::cout << "ERROR [" << this->getLogTime() << "] handleImage (Invalid Timestep: " << timestep << ")" << std::endl; 
        }
    }

    // set the camera's position and view
    camera->setImageSize(imagesize/n_cols, imagesize/n_cols);
    camera->setPosition(camera_x, camera_y, camera_z);
    camera->setUpVector(up_x, up_y, up_z);
    camera->setView(view_x, view_y, view_z);

    // set the renderer's data
    renderer->setData(temp_data);

    if (onlysave)
    {
        save_filename = "test";
        // no filters are supported for the onlysave option at the moment
        renderer->renderImage("/app/data/" + save_filename + "." + format);
        // renderer->renderImage(save_filename + "." + format);
        response.send(Http::Code::Ok, "saved");
    }
    else
    {
        // render the image 
        std::string encoded_image_data;
        if (format == "jpg")
        {
            renderer->renderToJPGObject(image_data, 100);
        }
        else if (format == "png")
        {
            renderer->renderToPNGObject(image_data);
        }
        encoded_image_data = std::string(image_data.begin(), image_data.end());

        // Pad the request_uri
        request_uri.insert(0, 2000 - request_uri.size(), ' ');

        auto mime = Http::Mime::MediaType::fromString("image/" + format);
        response.send(Http::Code::Ok, encoded_image_data, mime);
    }
}

void QuesadillaServer::handleConfiguration(const Rest::Request &request, 
        Pistache::Http::ResponseWriter response)
{
    std::string config_name = request.param(":configname").as<std::string>();
    std::cout << "INFO [" << this->getLogTime() << "] handleConfiguration (" << config_name << ")" << std::endl; 
    std::string encoded_json = request.body();
    std::string json_string = (encoded_json);
    rapidjson::Document json;
    json.Parse(json_string.c_str());
    rasty::Configuration *config = new rasty::Configuration(json);
    apply_config(config_name, config, &rasty_map);
    response.send(Http::Code::Ok, "changed");
}

void QuesadillaServer::handleAppData(const Rest::Request &request, 
        Pistache::Http::ResponseWriter response)
{
    std::cout << "INFO [" << this->getLogTime() << "] handleAppData" << std::endl; 
    auto mime = Http::Mime::MediaType::fromString("video/mp4");
    response.headers()
        .add<Http::Header::ContentType>(mime);
    auto filename = request.param(":filename").as<std::string>();
    filename = "/app/data/" + filename;
    serveFile(response, filename.c_str());
}

void QuesadillaServer::calculateTileRegion(int tile_index, int num_tiles,
        int n_cols, std::vector<float> &region)
{
    int tile_x = tile_index % n_cols;
    int tile_y = tile_index / n_cols;
    //std::cerr << "tile " << tile_index << " of " << num_tiles << ", " << n_cols << " cols" << std::endl;
    //std::cerr << "x,y " << tile_x << " " << tile_y << std::endl;
    region.push_back( ((float) n_cols - tile_y) / n_cols );      // top
    region.push_back( ((float) tile_x + 1) / n_cols );           // right
    region.push_back( ((float) n_cols - tile_y - 1) / n_cols );  // bottom
    region.push_back( ((float) tile_x) / n_cols );               // left
    /*
    std::cerr << "region " << region[0] << " "
                           << region[1] << " "
                           << region[2] << " "
                           << region[3] << std::endl;
    */
}

std::string QuesadillaServer::getLogTime(){
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return std::ctime(&now);
}

}
