#pragma once

#include <grpcpp/grpcpp.h>
#include <pqxx/pqxx>

#include <string>

#include "core/logger.hpp"

namespace gameworld::core {

class ErrorHandler {
public:
    explicit ErrorHandler(std::shared_ptr<Logger> logger) 
        : logger_(std::move(logger)) {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
    }

    grpc::Status handleGrpcError(const std::exception& e, std::string_view context) {
        logger_->error("[{}] Error: {}", context, e.what());

        if (auto* db_error = dynamic_cast<const pqxx::sql_error*>(&e)) {
            return handleDatabaseError(*db_error);
        }
        if (auto* validation_error = dynamic_cast<const std::invalid_argument*>(&e)) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, validation_error->what());
        }
        if (auto* not_found = dynamic_cast<const std::out_of_range*>(&e)) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, not_found->what());
        }

        return grpc::Status(
            grpc::StatusCode::INTERNAL, 
            std::string(context) + ": " + e.what()
        );
    }

private:
    std::shared_ptr<Logger> logger_;

    grpc::Status handleDatabaseError(const pqxx::sql_error& e) const {
        
        auto sqlstate = e.sqlstate();
        if (sqlstate == "23505") { // unique_violation
            return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, 
                "Object already exists: " + std::string(e.what()));
        }
        if (sqlstate == "23503") { // foreign_key_violation
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, 
                "Referenced object does not exist: " + std::string(e.what()));
        }
        if (sqlstate == "23502") { // not_null_violation
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, 
                "Required field is null: " + std::string(e.what()));
        }

        return grpc::Status(grpc::StatusCode::INTERNAL, 
            "Database error: " + std::string(e.what()));
    }
};

} // namespace gameworld::core