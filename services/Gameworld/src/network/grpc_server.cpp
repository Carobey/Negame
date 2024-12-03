#include "network/grpc_server.hpp"
#include "core/logger.hpp"

namespace gameworld::network {

GrpcServer::GrpcServer() = default;

GrpcServer::~GrpcServer() {
    stop();
}

bool GrpcServer::start(const std::string& address, int port, int threads) {
    try {
        if (running_) {
            return true;
        }

        grpc::ServerBuilder builder;
        
        // Включаем reflection service
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        
        // Настройка TLS если включено
        setupTLS();
        
        // Настройка адреса
        auto server_address = address + ":" + std::to_string(port);
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

        // Регистрация сервисов
        for (const auto& service : services_) {
            builder.RegisterService(service.get());
        }

        // Настройка параметров сервера
        builder.SetMaxReceiveMessageSize(64 * 1024 * 1024); // 64MB
        builder.SetMaxSendMessageSize(64 * 1024 * 1024);    // 64MB
        
        // Создание сервера
        server_ = builder.BuildAndStart();
        if (!server_) {
            throw std::runtime_error("Failed to start gRPC server");
        }

        // Запуск рабочих потоков
        threads_.reserve(threads);
        for (int i = 0; i < threads; ++i) {
            threads_.emplace_back([this] {
                server_->Wait();
            });
        }

        running_ = true;
        core::getLogger().info("gRPC server started on {} with {} threads", 
            server_address, threads);
        
        return true;
    }
    catch (const std::exception& e) {
        core::getLogger().error("Failed to start gRPC server: {}", e.what());
        return false;
    }
}

void GrpcServer::stop() {
    if (!running_) {
        return;
    }

    server_->Shutdown();
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
    
    running_ = false;
    core::getLogger().info("gRPC server stopped");
}

bool GrpcServer::isRunning() const {
    return running_;
}

void GrpcServer::setupTLS() {
    // Реализация TLS конфигурации
}

} // namespace gameworld::network
