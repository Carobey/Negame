#include "network/grpc_server.hpp"

namespace gameworld::network {

GrpcServer::~GrpcServer() {
    stop();
}

bool GrpcServer::start(const std::string& address, int port, int threads) {
    try {
        if (running_) {
            return true;
        }

        grpc::ServerBuilder builder;
        
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        setupTLS();
        
        auto server_address = address + ":" + std::to_string(port);
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

        for (const auto& service : services_) {
            builder.RegisterService(service.get());
        }

        builder.SetMaxReceiveMessageSize(64 * 1024 * 1024);
        builder.SetMaxSendMessageSize(64 * 1024 * 1024);
        
        server_ = builder.BuildAndStart();
        if (!server_) {
            throw std::runtime_error("Failed to start gRPC server");
        }

        threads_.reserve(threads);
        for (int i = 0; i < threads; ++i) {
            threads_.emplace_back([this] {
                server_->Wait();
            });
        }

        running_ = true;
        logger_->info("gRPC server started on {} with {} threads", server_address, threads);
        
        return true;
    }
    catch (const std::exception& e) {
        logger_->error("Failed to start gRPC server: {}", e.what());
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
    logger_->info("gRPC server stopped");
}

bool GrpcServer::isRunning() const {
    return running_;
}

void GrpcServer::setupTLS() {
    // TODO TLS configuration implementation
}

} // namespace gameworld::network