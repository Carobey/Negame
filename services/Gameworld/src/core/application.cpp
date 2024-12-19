#include <boost/program_options.hpp>

#include <csignal>
#include <iostream>

#include "core/config.hpp"
#include "core/application.hpp"
#include "service/gameworld_service.hpp"

namespace gameworld::core {

namespace {
    Application* g_app_instance = nullptr;
    
    void signal_handler(int) {
        if (g_app_instance) {
            g_app_instance->shutdown();
        }
    }
}

bool Application::parseCommandLine() {
    try {
        namespace po = boost::program_options;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("config", po::value<std::string>()->default_value("/etc/gameworld/config.json"), 
             "path to configuration file");

        po::variables_map vm;
        po::store(po::parse_command_line(argc_, argv_, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return false;
        }

        return config_->loadConfig(vm["config"].as<std::string>());
    }
    catch (const std::exception& e) {
        logger_->error("Command line parsing failed: {}", e.what());
        return false;
    }
}

bool Application::initializeServices() {
    try {
        logger_->info("Starting {} version {}", SERVICE_NAME, PROJECT_VERSION);

        db_ = std::make_shared<database::Database>(
            config_->get<std::string>("database.host"),
            config_->get<int>("database.port"),
            config_->get<std::string>("database.name"),
            config_->get<std::string>("database.user"),
            config_->get<std::string>("database.password"),
            config_->get<size_t>("database.pool.max_connections"),
            logger_
        );

        repository_ = std::make_shared<database::CelestialObjectRepositoryImpl>(
            db_, logger_
        );

        auto service = std::make_unique<service::GameWorldService>(
            repository_, error_handler_, logger_
        );

        grpc_server_ = std::make_unique<network::GrpcServer>(logger_);
        grpc_server_->registerService(std::move(service));
        
        if (!grpc_server_->start(
                config_->get<std::string>("grpc.address"),
                config_->get<int>("grpc.port"),
                config_->get<int>("grpc.threads"))) {
            logger_->error("Failed to start gRPC server");
            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        logger_->error("Failed to initialize services: {}", e.what());
        return false;
    }
}

void Application::setupSignalHandlers() {
    g_app_instance = this;
    std::signal(SIGINT, signal_handler);
}

bool Application::initialize() {
    return parseCommandLine() && initializeServices();
}

int Application::run() {
    setupSignalHandlers();
    auto future = exit_signal_.get_future();
    future.wait();
    return 0;
}

void Application::shutdown() {
    logger_->info("Shutting down application...");
    if (grpc_server_) {
        grpc_server_->stop();
    }
    exit_signal_.set_value();
}

}  // namespace gameworld::core