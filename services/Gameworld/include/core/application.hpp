#pragma once

#include <memory>
#include <future>
#include "core/logger.hpp"
#include "core/config_handler.hpp"
#include "core/error_handler.hpp"
#include "network/grpc_server.hpp"
#include "database/database.hpp"
#include "database/celestial_object_repository_impl.hpp"
namespace gameworld::core {

class Application {
public:
    
    Application(int argc, char* argv[]);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    bool initialize();
    int run();
    void shutdown();

private:
    bool parseCommandLine();
    bool initializeServices();
    void setupSignalHandlers();
    
    int argc_;
    char** argv_;
    Logger& logger_;
    ConfigHandler& config_;
    std::promise<void> exit_signal_;

    std::unique_ptr<network::GrpcServer> grpc_server_;
    std::shared_ptr<database::Database> db_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<database::CelestialObjectRepositoryImpl> repository_;
};

}  // namespace gameworld::core