#include "application.h"

#include <exception>
#include <iostream>
#include <memory>

int main(int argc, char** argv)
{
    try {
        std::unique_ptr<lunalite::core::Application> app(lunalite::core::createApplication(argc, argv));
        if (!app) {
            std::cerr << "Failed to create application." << std::endl;
            return -1;
        }

        app->run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}
