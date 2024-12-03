#pragma once

#include "core/error_handler.hpp"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <memory>
#include <string>
#include <thread>

namespace gameworld::network {

class GrpcServer {
public:
    GrpcServer();
    ~GrpcServer();

    bool start(const std::string& address, int port, int threads);
    void stop();
    bool isRunning() const;

    template<typename ServiceType>
    void registerService(std::unique_ptr<ServiceType> service) {
        services_.push_back(std::move(service));
    }

private:
    void configureServer();
    void setupTLS();

    std::unique_ptr<grpc::Server> server_;
    std::vector<std::unique_ptr<grpc::Service>> services_;
    std::vector<std::thread> threads_;
    bool running_{false};
};

} // namespace gameworld::network