#ifndef QUES_SERVER_H
#define QUES_SERVER_H

#include <quesadillas.h>

#include "pbnj.h"
#include "Renderer.h"
#include "Configuration.h"
#include "Camera.h"
#include "TimeSeries.h"

#include "pistache/http.h"
#include "pistache/router.h"
#include "pistache/endpoint.h"

namespace ques {
    using namespace Pistache;

    union Dataset
    {
        pbnj::Volume* volume;
        pbnj::TimeSeries *timeseries;
    };

    typedef std::tuple<pbnj::Configuration*, ques::Dataset, 
            pbnj::Camera*, pbnj::Renderer**> pbnj_container;

    class QuesadillaServer 
    {
        public:
            QuesadillaServer(Pistache::Address addr, std::map<std::string, pbnj_container> vm);

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
            std::map<std::string, pbnj_container> volume_map;
            std::string app_dir;
    };
}

#endif
