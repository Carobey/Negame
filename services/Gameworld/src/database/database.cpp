#include "database/database.hpp"
#include <cstddef>

namespace gameworld::database {

Database::Database(
    const std::string& connection_string, 
    const size_t max_connections,
    std::shared_ptr<core::Logger> logger
) : logger_(std::move(logger)) , max_connections_(max_connections) {
    if (!logger_) {
        throw std::invalid_argument("Logger cannot be null");
    }

    try {
        pool_ = std::make_unique<ConnectionPool>(connection_string, logger_, max_connections_);
        logger_->info("Database connection pool initialized with connection string: {}", 
            connection_string);
    }
    catch (const std::exception& e) {
        logger_->error("Failed to initialize database connection pool: {}", e.what());
        throw;
    }
}

} // namespace gameworld::database