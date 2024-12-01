#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

#include <memory>

#include "gameworld.grpc.pb.h"
#include "database/celestial_object_repository.hpp"
#include "core/error_handler.hpp"
#include "core/logger.hpp"


namespace gameworld::service {

class GameWorldService final : public v1::GameWorldService::Service {
public:
    explicit GameWorldService(
        std::shared_ptr<database::ICelestialObjectRepository> repository,
        std::shared_ptr<core::ErrorHandler> error_handler,
        std::shared_ptr<core::Logger> logger
    ) : repository_(std::move(repository))
      , error_handler_(std::move(error_handler))
      , logger_(std::move(logger)) {
        if (!repository_ || !error_handler_ || !logger_) {
            throw std::invalid_argument("Dependencies cannot be null");
        }
    }
 
    grpc::Status GetObjectTypes(
        grpc::ServerContext* context,
        const v1::GetObjectTypesRequest* request,
        v1::GetObjectTypesResponse* response
    ) override;

    grpc::Status GetCelestialObject(
        grpc::ServerContext* context,
        const v1::GetCelestialObjectRequest* request,
        v1::CelestialObject* response
    ) override;

    grpc::Status ListCelestialObjects(
        grpc::ServerContext* context,
        const v1::ListCelestialObjectsRequest* request,
        v1::ListCelestialObjectsResponse* response
    ) override;

    grpc::Status CreateCelestialObject(
        grpc::ServerContext* context,
        const v1::CreateCelestialObjectRequest* request,
        v1::CelestialObject* response
    ) override;

    grpc::Status UpdateCelestialObject(
        grpc::ServerContext* context,
        const v1::UpdateCelestialObjectRequest* request,
        v1::CelestialObject* response
    ) override;

    grpc::Status DeleteCelestialObject(
        grpc::ServerContext* context,
        const v1::DeleteCelestialObjectRequest* request,
        google::protobuf::Empty* response
    ) override;

    grpc::Status HealthCheck(
        grpc::ServerContext* context,
        const google::protobuf::Empty* request,
        google::protobuf::Empty* response
    ) override;

private:
   std::shared_ptr<database::ICelestialObjectRepository> repository_;
    std::shared_ptr<core::ErrorHandler> error_handler_;
    std::shared_ptr<core::Logger> logger_;

    template<typename Func>
    grpc::Status handleGrpcRequest(const char* method_name, Func&& func) {
        try {
            return func();
        }
        // TODO
        // catch (const DatabaseException& e) {
        // logger_->error("[{}] Database error: {}", method_name, e.what());
        // return error_handler_->handleDatabaseError(e, method_name);
        // }
        // catch (const ValidationException& e) {
        //     logger_->error("[{}] Validation error: {}", method_name, e.what());
        //     return error_handler_->handleValidationError(e, method_name);
        // }
        catch (const std::exception& e) {
            logger_->error("[{}] Unexpected error: {}", method_name, e.what());
            return error_handler_->handleUnexpectedError(e, method_name);
        }
    }

    bool validateRequest(const google::protobuf::Message* request, std::string& error);
    bool validateCelestialObject(const v1::CelestialObject& object, std::string& error);
    bool validateCreateRequest(const v1::CreateCelestialObjectRequest& request, std::string& error);
    bool validateUpdateRequest(const v1::UpdateCelestialObjectRequest& request, std::string& error);

};

}  // namespace gameworld::service