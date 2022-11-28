#ifndef QUES_SERVER_H
#define QUES_SERVER_H

#include <quesadillas.h>

#include "rasty.h"
#include "Renderer.h"
#include "Cbar.h"
#include "Configuration.h"
#include "Camera.h"
#include "DataFile.h"
#include "Raster.h"

#include "pistache/http.h"
#include "pistache/router.h"
#include "pistache/endpoint.h"

namespace ques {
    using namespace Pistache;

    struct Dataset
    {
        rasty::Raster* raster;
        rasty::DataFile* data;
    };

    typedef std::tuple<rasty::Configuration*, ques::Dataset, 
            rasty::Camera*, rasty::Renderer*> rasty_container;

    class QuesadillaServer 
    {
        public:
            QuesadillaServer(Pistache::Address addr, std::map<std::string, rasty_container> vm);

            void init(std::string app_dir, size_t threads=2);
            void start();
            void shutdown();

        private:

            void setupRoutes();
            void serveFile(Pistache::Http::ResponseWriter &response,
                    std::string filename);
            void handleRoot(const Rest::Request &request,
                    Pistache::Http::ResponseWriter response);
            void handleJS(const Rest::Request &request, 
                    Pistache::Http::ResponseWriter response);
            void handleCSS(const Rest::Request &request, 
                    Pistache::Http::ResponseWriter response);
            void handleImage(const Rest::Request &request,
                    Pistache::Http::ResponseWriter response);
            void handleExternalCommand(const Rest::Request &request,
                    Pistache::Http::ResponseWriter response);
            void handleConfiguration(const Rest::Request &request,
                    Pistache::Http::ResponseWriter response);
            void handleAppData(const Rest::Request &request,
                    Pistache::Http::ResponseWriter response);

            void calculateTileRegion(int tile_index, int num_tiles,
                    int n_cols, std::vector<float> &region);

            std::shared_ptr<Pistache::Http::Endpoint> httpEndpoint;
            Rest::Router router;
            std::map<std::string, rasty_container> rasty_map;
            std::string app_dir;
    };
}

#endif
