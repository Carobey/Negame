#include "core/application.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        
        auto logger = std::make_shared<gameworld::core::Logger>(); 
        logger->init("service.log");

        auto config_handler = std::make_unique<gameworld::core::ConfigHandler>(logger);
        auto error_handler = std::make_unique<gameworld::core::ErrorHandler>(logger); //

        gameworld::core::Application app(
            argc, argv,
            std::move(logger),
            std::move(config_handler),
            std::move(error_handler)
        );
        
        if (!app.initialize()) {
            return 1;
        }
        
        return app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}