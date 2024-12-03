// Основной сервис для работы с игровым миром

#pragma once

#include <memory>
#include <grpcpp/grpcpp.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

#include "gameworld.grpc.pb.h"
#include "database/celestial_object_repository.hpp"
#include "core/error_handler.hpp"
#include "core/logger.hpp"


namespace gameworld::service {

class GameWorldService final : public v1::GameWorldService::Service {
public:
    /**
     * @brief Конструктор сервиса
     * 
     * @param repository Репозиторий для работы с космическими объектами
     * @param error_handler Обработчик ошибок
     * @param logger logger
     * @param metrics Сборщик метрик (опционально)
     */
    explicit GameWorldService(
        std::shared_ptr<database::ICelestialObjectRepository> repository,
        std::shared_ptr<core::ErrorHandler> error_handler,
        core::Logger& logger
    );

    // gRPC методы для работы с типами объектов
    grpc::Status GetObjectTypes(
        grpc::ServerContext* context,
        const v1::GetObjectTypesRequest* request,
        v1::GetObjectTypesResponse* response
    ) override;

    // gRPC методы для CRUD операций с объектами
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
    core::Logger& logger_;

    template<typename Func>
    grpc::Status handleGrpcRequest(const char* method_name, Func&& func) {
        try {
            return func();
        }
        catch (const std::exception& e) {
            logger_.error_loc(std::source_location::current(), "[{}] Error: {}", method_name, e.what());
            return error_handler_->handleGrpcError(e, method_name);
        }
    }

    bool validateRequest(const google::protobuf::Message* request, std::string& error);
    bool validateCelestialObject(const v1::CelestialObject& object, std::string& error);
    bool validateCreateRequest(const v1::CreateCelestialObjectRequest& request, std::string& error);
    bool validateUpdateRequest(const v1::UpdateCelestialObjectRequest& request, std::string& error);

};

} // namespace gameworld::service