/**
 * @file gameworld_service.cpp
 * @brief Реализация сервиса игрового мира
 */

#include "service/gameworld_service.hpp"

namespace gameworld::service {

namespace {
    // Константы пагинации
    constexpr int DEFAULT_PAGE_SIZE = 100;
    constexpr int MAX_PAGE_SIZE = 1000;

    // Общие сообщения об ошибках
    constexpr std::string_view ERROR_INVALID_REQUEST = "Invalid request data";
    constexpr std::string_view ERROR_NOT_FOUND = "Object not found";
    constexpr std::string_view ERROR_VALIDATION = "Validation failed";

}

GameWorldService::GameWorldService(
    std::shared_ptr<database::ICelestialObjectRepository> repository,
    std::shared_ptr<core::ErrorHandler> error_handler,
    core::Logger& logger
) : repository_(std::move(repository))
  , error_handler_(std::move(error_handler))
  ,logger_(logger)
{
    if (!repository_) throw std::invalid_argument("Repository cannot be null");
    if (!error_handler_) throw std::invalid_argument("Error handler cannot be null");
}

grpc::Status GameWorldService::GetObjectTypes(
    grpc::ServerContext* context,
    const v1::GetObjectTypesRequest* request,
    v1::GetObjectTypesResponse* response
) {
    return handleGrpcRequest("GetObjectTypes", [&]() {
        std::string validation_error;
        if (!validateRequest(request, validation_error)) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, validation_error);
        }

        auto types = repository_->getObjectTypes(
            request->has_parent_type() ? 
                std::make_optional(request->parent_type()) : 
                std::nullopt
        );

        for (const auto& type : types) {
            *response->add_types() = type;
        }

        return grpc::Status::OK;
    });
}
grpc::Status GameWorldService::GetCelestialObject(
    grpc::ServerContext* context,
    const v1::GetCelestialObjectRequest* request,
    v1::CelestialObject* response
) {
    return handleGrpcRequest("GetCelestialObject", [&]() {
        if (request->id().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Object ID cannot be empty");
        }

        auto object = repository_->getById(request->id());
        if (!object) {
            logger_.warn_loc(std::source_location::current(), "Object not found: {}", request->id());
            return grpc::Status(grpc::StatusCode::NOT_FOUND, 
                boost::str(boost::format("Object with ID %1% not found") % request->id()));
        }

        *response = *object;
        return grpc::Status::OK;
    });
}

grpc::Status GameWorldService::ListCelestialObjects(
    grpc::ServerContext* context,
    const v1::ListCelestialObjectsRequest* request,
    v1::ListCelestialObjectsResponse* response
) {
    return handleGrpcRequest("ListCelestialObjects", [&]() {
        // Валидация параметров пагинации
        int page_size = request->page_size();
        if (page_size <= 0) {
            page_size = DEFAULT_PAGE_SIZE;
        } else if (page_size > MAX_PAGE_SIZE) {
            page_size = MAX_PAGE_SIZE;
        }

        // Построение фильтра
        std::string filter;
        if (request->has_filter()) {
            const auto& req_filter = request->filter();
            std::vector<std::string> conditions;

            // Фильтрация по типам
            if (!req_filter.types().empty()) {
                std::vector<std::string> type_conditions;
                for (const auto& type : req_filter.types()) {
                    type_conditions.push_back(
                        boost::str(boost::format("type = %d") % static_cast<int>(type))
                    );
                }
                conditions.push_back(
                    "(" + boost::algorithm::join(type_conditions, " OR ") + ")"
                );
            }

            // Фильтрация по родительскому объекту
            if (req_filter.has_parent_id()) {
                conditions.push_back(
                    boost::str(boost::format("parent_id = '%s'") % req_filter.parent_id())
                );
            }

            // Поиск по имени
            if (!req_filter.name_pattern().empty()) {
                conditions.push_back(
                    boost::str(boost::format("name ILIKE '%%%s%%'") % req_filter.name_pattern())
                );
            }

            if (!conditions.empty()) {
                filter = boost::algorithm::join(conditions, " AND ");
            }
        }

        // Получение данных с учетом пагинации
        auto objects = repository_->list(filter);
        
        // Реализация пагинации на уровне сервиса
        int total_count = objects.size();
        int offset = 0;
        if (!request->page_token().empty()) {
            try {
                offset = std::stoi(request->page_token());
            }
            catch (...) {
                return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid page token");
            }
        }

        // Добавляем объекты текущей страницы в ответ
        for (int i = offset; i < std::min(offset + page_size, total_count); ++i) {
            *response->add_objects() = objects[i];
        }

        // Формируем токен следующей страницы
        if (offset + page_size < total_count) {
            response->set_next_page_token(std::to_string(offset + page_size));
        }

        response->set_total_count(total_count);
        return grpc::Status::OK;
    });
}

grpc::Status GameWorldService::CreateCelestialObject(
    grpc::ServerContext* context,
    const v1::CreateCelestialObjectRequest* request,
    v1::CelestialObject* response
) {
    return handleGrpcRequest("CreateCelestialObject", [&]() {
        if (!request->has_object()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Object data is required");
        }

        std::string validation_error;
        if (!validateRequest(&request->object(), validation_error)) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, validation_error);
        }

        // Проверка существования родительского объекта
        if (request->object().has_parent_id()) {
            auto parent = repository_->getById(request->object().parent_id());
            if (!parent) {
                return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, 
                    "Parent object not found");
            }
        }

        *response = repository_->create(request->object());
        
        logger_.info("Created celestial object: {} ({})", 
            response->name(), response->id());

        return grpc::Status::OK;
    });
}

grpc::Status GameWorldService::UpdateCelestialObject(
    grpc::ServerContext* context,
    const v1::UpdateCelestialObjectRequest* request,
    v1::CelestialObject* response
) {
    return handleGrpcRequest("UpdateCelestialObject", [&]() {
        if (request->id().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Object ID is required");
        }

        if (!request->has_object()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Object data is required");
        }

        std::string validation_error;
        if (!validateRequest(&request->object(), validation_error)) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, validation_error);
        }

        // Проверяем существование объекта
        auto existing = repository_->getById(request->id());
        if (!existing) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, 
                boost::str(boost::format("Object with ID %1% not found") % request->id()));
        }

        if (existing->properties().at("version") != request->object().properties().at("version")) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, 
                "Object version mismatch. Please reload the object and try again.");
        }

        std::vector<std::string> update_mask;
        if (!request->update_mask().empty()) {
            update_mask.assign(
                request->update_mask().begin(),
                request->update_mask().end()
            );
        }

        v1::CelestialObject updated_object = request->object();
        updated_object.set_id(request->id());

        if (!repository_->update(updated_object)) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to update object");
        }

        // Получаем обновленный объект
        auto result = repository_->getById(request->id());
        if (!result) {
            return grpc::Status(grpc::StatusCode::INTERNAL, 
                "Object not found after update");
        }

        *response = *result;
        
        logger_.info("Updated celestial object: {} ({})", 
            response->name(), response->id());

        return grpc::Status::OK;
    });
}

grpc::Status GameWorldService::DeleteCelestialObject(
    grpc::ServerContext* context,
    const v1::DeleteCelestialObjectRequest* request,
    google::protobuf::Empty* response
) {
    return handleGrpcRequest("DeleteCelestialObject", [&]() {
        if (request->id().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Object ID is required");
        }

        auto existing = repository_->getById(request->id());
        if (!existing) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, 
                boost::str(boost::format("Object with ID %1% not found") % request->id()));
        }

        auto children = repository_->findByParent(request->id());
        if (!children.empty() && request->hard_delete()) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                "Cannot hard delete object with children. Delete children first or use soft delete.");
        }

        bool success = repository_->remove(request->id());
        if (!success) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to delete object");
        }

        logger_.info("Deleted celestial object: {} ({})", 
            existing->name(), existing->id());

        return grpc::Status::OK;
    });
}

grpc::Status GameWorldService::HealthCheck(
    grpc::ServerContext* context,
    const google::protobuf::Empty* request,
    google::protobuf::Empty* response
) {
    return handleGrpcRequest("HealthCheck", [&]() {
        try {
            repository_->list("LIMIT 1");
            return grpc::Status::OK;
        }
        catch (const std::exception& e) {
            logger_.error_loc(std::source_location::current(), "Health check failed: {}", e.what());
            return grpc::Status(grpc::StatusCode::INTERNAL, "Service unhealthy");
        }
    });
}

bool GameWorldService::validateRequest(
    const google::protobuf::Message* request,
    std::string& error
) {
    if (!request) {
        error = "Request cannot be null";
        return false;
    }

    // Проверяем тип запроса и выполняем соответствующую валидацию
    if (auto celestial_object = dynamic_cast<const v1::CelestialObject*>(request)) {
        return validateCelestialObject(*celestial_object, error);
    }
    else if (auto create_request = dynamic_cast<const v1::CreateCelestialObjectRequest*>(request)) {
        return validateCreateRequest(*create_request, error);
    }
    else if (auto update_request = dynamic_cast<const v1::UpdateCelestialObjectRequest*>(request)) {
        return validateUpdateRequest(*update_request, error);
    }

    return true;
}

bool GameWorldService::validateCelestialObject(
    const v1::CelestialObject& object,
    std::string& error
) {
    // Валидация имени
    if (object.name().empty()) {
        error = "Object name is required";
        return false;
    }

    if (object.name().length() > 255) {
        error = "Object name is too long (max 255 characters)";
        return false;
    }

    // Валидация типа
    if (object.type() == v1::CelestialObjectType::CELESTIAL_OBJECT_TYPE_UNSPECIFIED) {
        error = "Object type must be specified";
        return false;
    }

    // Валидация координат
    if (object.has_globcoordinates()) {
        const auto& coords = object.globcoordinates();
        // Проверяем на разумные пределы (в парсеках)
        if (std::abs(coords.x()) > 1e6 || 
            std::abs(coords.y()) > 1e6 || 
            std::abs(coords.z()) > 1e6) {
            error = "Coordinates are out of reasonable bounds (±1e6 parsecs)";
            return false;
        }
    }

    auto props = object.properties();
    if (std::stod(props.at("mass_solar_masses")) < 0) {
        error = "Mass cannot be negative";
        return false;
    }

    if (std::stod(props.at("radius_solar_radii")) < 0) {
        error = "Radius cannot be negative";
        return false;
    }

    if (std::stod(props.at("temperature_kelvin")) < 0) {
        error = "Temperature cannot be negative";
        return false;
    }

    // Валидация зависимостей между полями в зависимости от типа объекта
    switch (object.type()) {
        case v1::CelestialObjectType::PLANET:
            if (!object.has_parent_id()) {
                error = "Planets must have a parent object";
                return false;
            }
            break;

        case v1::CelestialObjectType::STAR:
            // if (object.properties().at("temperature_kelvin"). < 2000 || 
            //     object.temperature_kelvin() > 50000) {
            //     error = "Invalid star temperature range";
            //     return false;
            // }
            break;

        // Добавляем проверки для других типов объектов
        default:
            break;
    }

    return true;
}

bool GameWorldService::validateCreateRequest(
    const v1::CreateCelestialObjectRequest& request,
    std::string& error
) {
    if (!request.has_object()) {
        error = "Create request must contain object data";
        return false;
    }

    // Проверяем, что ID не задан (генерируется автоматически)
    if (!request.object().id().empty()) {
        error = "Object ID should not be specified in create request";
        return false;
    }

    return validateCelestialObject(request.object(), error);
}

bool GameWorldService::validateUpdateRequest(
    const v1::UpdateCelestialObjectRequest& request,
    std::string& error
) {
    if (request.id().empty()) {
        error = "Update request must specify object ID";
        return false;
    }

    if (!request.has_object()) {
        error = "Update request must contain object data";
        return false;
    }

    // Проверяем согласованность ID
    if (!request.object().id().empty() && 
        request.object().id() != request.id()) {
        error = "Inconsistent object IDs in update request";
        return false;
    }

    return validateCelestialObject(request.object(), error);
}

} // namespace gameworld::service