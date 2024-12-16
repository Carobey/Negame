#pragma once

#include <grpcpp/grpcpp.h>
#include "grpcpp/impl/codegen/service_type.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include <memory>
#include <string>
#include <thread>

#include "core/logger.hpp"

namespace gameworld::network {

class GrpcServer {
public:
    explicit GrpcServer(std::shared_ptr<core::Logger> logger) 
        : logger_(std::move(logger)) {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
    }
    ~GrpcServer();

    bool start(const std::string& address, int port, int threads);
    void stop();
    [[nodiscard]] bool isRunning() const;

    template<typename ServiceType>
    void registerService(std::unique_ptr<ServiceType> service) {
        services_.push_back(std::move(service));
    }

private:
    void configureServer();
    void setupTLS();

    std::shared_ptr<core::Logger> logger_;
    std::unique_ptr<grpc::Server> server_;
    std::vector<std::unique_ptr<grpc::Service>> services_;
    std::vector<std::thread> threads_;
    bool running_{false};
};

}  // namespace gameworld::network