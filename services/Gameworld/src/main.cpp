#include <iostream>
#include <memory>
#include <csignal>
#include <future>

#include <boost/signals2.hpp>
#include <boost/program_options.hpp>

#include "core/logger.hpp"
#include "core/config.hpp"
#include "core/config_handler.hpp"
#include "core/error_handler.hpp"
#include "network/grpc_server.hpp"
#include "service/gameworld_service.hpp"
#include "database/database.hpp"
#include "database/celestial_object_repository_impl.hpp"

namespace po = boost::program_options;
using namespace gameworld;

namespace {
    std::promise<void>* exit_signal_promise = nullptr;

    void signal_handler(int) {
        if (exit_signal_promise) {
            core::getLogger().info("Received shutdown signal");
            exit_signal_promise->set_value();
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("config", po::value<std::string>()->default_value("/etc/gameworld/config.json"), "path to configuration file");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        // Инициализация логгера
        auto& logger = core::getLogger();
        logger.init("service.log");
        logger.info("Starting {} version {}", SERVICE_NAME, PROJECT_VERSION);

        // Загрузка конфигурации
        auto& config = core::ConfigHandler::getInstance();
        if (!config.loadConfig(vm["config"].as<std::string>())) {
            logger.error("Failed to load configuration");
            return 1;
        }

        // Инициализация сервисов
        auto db = std::make_shared<database::Database>(
            config.get<std::string>("database.host", "localhost"),
            config.get<int>("database.port", 5432),
            config.get<std::string>("database.name", "gameworld"),
            config.get<std::string>("database.user", "postgres"),
            config.get<std::string>("database.password", "")
        );

        auto error_handler = std::make_shared<core::ErrorHandler>();
        auto repository = std::make_shared<database::CelestialObjectRepositoryImpl>(db, logger);
        auto service = std::make_unique<service::GameWorldService>(repository, error_handler, logger);

        // Запуск gRPC сервера
        network::GrpcServer grpc_server;
        grpc_server.registerService(std::move(service));
        
        if (!grpc_server.start(
            config.get<std::string>("grpc.address"),
            config.get<int>("grpc.port"),
            config.get<int>("grpc.threads"))) {
            logger.error("Failed to start gRPC server");
            return 1;
        }

        // Ожидание сигнала завершения
        std::promise<void> exit_signal;
        exit_signal_promise = &exit_signal;
        auto future = exit_signal.get_future();
        std::signal(SIGINT, signal_handler);
        
        future.wait();

        //grpc_server.stop();
        logger.info("Service stopped");
        exit_signal_promise = nullptr;
        return 0;
    }
    catch (const std::exception& e) {
        if (core::Logger* logger = dynamic_cast<core::Logger*>(&core::getLogger())) {
            logger->fatal("Fatal error: {}", e.what());
        } else {
            std::cerr << "Fatal error: " << e.what() << std::endl;
        }
        return 1;
    }
}