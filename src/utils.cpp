#include "QuesadillaServer.h"
#include "utils.h"
#include <vector>

namespace ques {
    /*
     * MOA::TODO This needs to be smarter. For starters, 
     * it should free the old stuff. Also, it should check
     * and only update a rasty_container if it needs to. 
     */
    void apply_config(std::string config_name, 
            rasty::Configuration *config,
            std::map<std::string, ques::rasty_container>* rasty_map)
    {
        std::cout << "Applying configuration: " << config_name << std::endl;
        
        std::cout << "Creating new camera" << std::endl;
        rasty::Camera *camera = new rasty::Camera(
                config->imageWidth, 
                config->imageHeight);

        // Let's keep a renderer per volume to support time series for now
        rasty::Renderer *renderer; 
        // rasty::Renderer *renderer = new rasty::Renderer();
        rasty::CONFSTATE single_multi = config->getGeoConfigState();
        ques::Dataset *dataset = new ques::Dataset();

        // centerView has to be called before setCamera because the light position
        // depends on it. 
        camera->setPosition(config->cameraX, config->cameraY, config->cameraZ);
        camera->centerView();

        /*
         * If we have a single volume at hand
         */
        if (single_multi == rasty::CONFSTATE::SINGLE_NOVAR 
                || single_multi == rasty::CONFSTATE::SINGLE_VAR)
        {
            std::cout << "Creating new Raster from " << config->geoFilename << std::endl;
            // rasty::Raster *raster = new rasty::Raster(config->geoFilename);
            dataset->raster = new rasty::Raster(config->geoFilename);
            dataset->raster->setElevationScale(config->elevationScale);
            dataset->raster->setHeightWidthScale(config->heightWidthScale);

            dataset->data = new rasty::DataFile();
            dataset->data->loadFromFile(config->dataFilename);
            if (config->dataVariable == "")
            {
                std::vector<std::string> vars = dataset->data->getVariableNames();
                dataset->data->loadVariable(vars[0]);
            }
            else
            {
                dataset->data->loadVariable(config->dataVariable);
            }

            dataset->data->loadTimeStep(0);
            
            // std::cout << "Creating new Cbar from " << config->colorMap << std::endl;
            rasty::Cbar *cbar = new rasty::Cbar(config->colorMap);

            // std::cout << "Creating new Renderer" << std::endl;
            renderer = new rasty::Renderer();
            // std::cout << renderer<< std::endl;
            // std::cout << "Setting raster" << std::endl;
            renderer->setRaster(dataset->raster);
            // std::cout << "Setting cbar " << cbar << std::endl;
            renderer->setCbar(cbar);
            // std::cout << "Setting data" << std::endl;
            renderer->setData(dataset->data);
            // std::cout << "Setting camera" << std::endl;
            renderer->setCamera(camera);
            // std::cout << "Adding light" << std::endl;
            renderer->addLight();

        }
        else
        {
            std::cerr<<"Cannot open this type of RASTY file: "<< config_name << std::endl;
            return;
        }

        renderer->setupWorld();

        (*rasty_map)[config_name] = std::make_tuple(config, dataset, camera, renderer);
    }

    pid_t pcreate(int fds[2], const char* cmd)
    {
        pid_t pid;
        int pipes[4];

        pipe(&pipes[0]); // parent read, child write
        pipe(&pipes[2]); // child read, parent write

        if ((pid = fork()) > 0)
        {
            // parent process
            fds[0] = pipes[0];
            fds[1] = pipes[3];

            if (close(pipes[1]) < 0)
            {
                std::cerr<<"Error while closing pipes[1]\n";
            }

            if (close(pipes[2]) < 0)
            {
                std::cerr<<"Error while closing pipes[2]\n";
            }

            return pid;
        }
        else if (pid == 0)
        {
            // child process
            // close the other two descriptors
            if (close(pipes[0]) < 0)
            {
                std::cerr<<"Error while closing pipes[0]\n";
            }

            if (close(pipes[3]) < 0)
            {
                std::cerr<<"Error while closing pipes[3]\n";    
            }

            dup2(pipes[1], STDOUT_FILENO);
            dup2(pipes[2], STDIN_FILENO);

            // run an external plugin and give it the image 
            // no args added yet
            if (execvp(cmd, NULL) < 0)
            {
                std::cerr<<"Could not run external program "<<errno<<std::endl;
            }

            exit(0); // exit from the child
        }
        std::cerr<<"Could not fork child process\n";
        return -1;
    } 

    std::string exec_filter(const char* cmd, std::string request, 
            std::string data) 
    {
        int fds[2];
        pid_t pid = pcreate(fds, cmd);

        long unsigned int data_size = data.length();
        int max_req_length = 2000;
        if (write(fds[1], request.c_str(), max_req_length) < 0)
        {
            std::cerr<<"Error when writing request to child\n"; 
        }
        if (write(fds[1], data.c_str(), data.length()) < 0) // write the data itself
        {
            std::cerr<<"Error when writing to child\n";
        }
        close(fds[1]);

        std::string filtered_data = "";
        char temp;
        int err; 
        while(err = read(fds[0], &temp, sizeof(char)) > 0)
        {
            filtered_data += temp; 
        }

        if (err < 0)
        {
            std::cerr<<"Error when reading data from child\n";
        }

        return filtered_data;
    }

    std::string exec(const char* cmd) 
    {
        std::array<char, 128> buffer;
        std::string result;
        std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
        if (!pipe) throw std::runtime_error("popen() failed!");
        while (!feof(pipe.get())) {
            if (fgets(buffer.data(), 128, pipe.get()) != NULL)
                result += buffer.data();
        }
        return result;
    }
}
