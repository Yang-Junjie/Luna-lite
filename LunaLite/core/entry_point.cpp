#include "application.h"
#include "log.h"

#include <memory>

int main(int argc, char** argv)
{
    lunalite::core::Logger::init();

    std::unique_ptr<lunalite::core::Application> app(lunalite::core::createApplication(argc, argv));
    LUNA_ASSERT(app, "Failed to create application instance.");

    app->run();
    lunalite::core::Logger::shutdown();
    return 0;
}
