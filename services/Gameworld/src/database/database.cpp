#include "database/database.hpp"
#include "core/logger.hpp"
#include <stdlib.h>

namespace gameworld::database {

Database::Database(const std::string& connection_string) {
    try {
        pool_ = std::make_unique<ConnectionPool>(connection_string);
        core::getLogger().info("Database connection pool initialized with connection string: {}",
            connection_string);
    }
    catch (const std::exception& e) {
        core::getLogger().error("Failed to initialize database connection pool: {}", e.what());
        throw;
    }
}

} // namespace gameworld::database