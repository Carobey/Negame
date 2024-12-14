#include "core/application.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        gameworld::core::Application app(argc, argv);
        
        if (!app.initialize()) { 
            return 1;
        }
        
        return app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Gameworld fatal error: " << e.what() << std::endl;
        return 1;
    }
}