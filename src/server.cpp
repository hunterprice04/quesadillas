// QUES headers
#include "quesadillas.h"
#include "QuesadillaServer.h"
#include "utils.h"

// Rasty headers
#include "rasty.h"
#include "Camera.h"
#include "Configuration.h"
#include "Renderer.h"
#include "Raster.h"

#include <dirent.h>
#include <iostream>
#include <cstdlib>
#include <map>
#include <tuple>
#include <csignal>

using namespace Pistache;

static volatile int doShutdown = 0;

void sigintHandler(int sig) 
{
    doShutdown = 1;
}

void waitForShutdown(ques::QuesadillaServer *ques) 
{
    std::signal(SIGINT, sigintHandler);

    while (!doShutdown) 
    {
        sleep(1);
    }

    ques->shutdown();
}

int main(int argc, const char **argv)
{
    /*
     * Check the input parameters
     */
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: " 
            << argv[0] 
            << " <configuration directory>" 
            << " <port>"
            << " [application directory]";

        std::cerr << std::endl;
        return 1;
    }

    std::string app_dir = ".";
    if (argc == 4)
    {
        app_dir = argv[3];
    }
    std::cout << "Application directory: " << app_dir << std::endl;
    // Variables for parsing the directory files
    std::string config_dir = argv[1];
    DIR *directory;
    if ((directory = opendir(config_dir.c_str())) == NULL)
    {
        std::cerr<<"Could not open configuration directory"<<std::endl;
        exit(-1);
    }
    struct dirent *dirp;


    std::cout<<"Loading configuration files..."<<std::endl;

    /*
     * A volume hash table that keeps RASTY objects of a dataset
     * in one place 
     */
    std::map<std::string, ques::rasty_container> rasty_map;
    rasty::rastyInit(argc, argv);

    // Parse the configuration files
    while ((dirp = readdir(directory)) != NULL)
    {
        std::string filename(dirp->d_name);
        std::string::size_type index = filename.rfind(".");
        // if the filename doesn't have an extension
        if (index == std::string::npos) 
        {
            continue;
        }
        std::string extension = filename.substr(index);
        std::string config_name = filename.substr(0, index);
        if (extension.compare(".json") == 0)
        {
            rasty::ConfigReader *reader = new rasty::ConfigReader();
            rapidjson::Document json; 
            reader->parseConfigFile(config_dir + "/" + filename, json);
            rasty::Configuration *config = new rasty::Configuration(json);
            apply_config(config_name, config, &rasty_map);
        }
    }

    std::cout << "Configuration files loaded" << std::endl;
    std::cout << "Starting server..." << std::endl;
    Pistache::Port port(9080);
    port = std::stol(argv[2]);

    Pistache::Address addr(Pistache::Ipv4::any(), port);

    ques::QuesadillaServer eserver(addr, rasty_map);
    eserver.init(app_dir, 1);

    eserver.start();

    std::thread waitForShutdownThread(waitForShutdown, &eserver);
    waitForShutdownThread.join();
    std::cout << "Server stopped" << std::endl;
    rasty::rastyDestroy();
    return 0;
}

