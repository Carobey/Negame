#include "database/connection_pool.hpp"
#include "core/logger.hpp"

namespace gameworld::database {

ConnectionPool::ConnectionPool(
    const std::string& connection_string,
    size_t max_connections)
    : connection_string_(connection_string)
    , max_connections_(max_connections) {
    
    auto& logger = core::getLogger();
    logger.info("Initializing connection pool with max {} connections", max_connections);
    
    //начальный пул, треть от максимального
    for (size_t i = 0; i < max_connections_ / 3; ++i) {
        try {
            available_.push(std::make_shared<pqxx::connection>(connection_string_));
            logger.debug("Created initial connection {}/{}", i + 1, max_connections_ / 3);
        }
        catch (const std::exception& e) {
            logger.error("Failed to create initial connection: {}", e.what());
            throw;
        }
    }
}

ConnectionPool::~ConnectionPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    core::getLogger().debug("Destroying connection pool");
    while (!available_.empty()) {
        available_.pop();
    }
    in_use_.clear();
}

std::shared_ptr<pqxx::connection> ConnectionPool::acquire() {
    auto& logger = core::getLogger();
    std::unique_lock<std::mutex> lock(mutex_);
    
    cv_.wait(lock, [this] {
        return !available_.empty() || in_use_.size() < max_connections_;
    });
    
    std::shared_ptr<pqxx::connection> conn;
    
    try {
        if (!available_.empty()) {
            conn = available_.front();
            available_.pop();
            logger.debug("Acquired existing connection from pool");
        } else {
            conn = std::make_shared<pqxx::connection>(connection_string_);
            logger.debug("Created new connection (total: {})", in_use_.size() + 1);
        }
        
        in_use_.insert(conn);
        return conn;
    }
    catch (const std::exception& e) {
        logger.error("Failed to acquire connection: {}", e.what());
        throw;
    }
}

void ConnectionPool::release(const std::shared_ptr<pqxx::connection>& conn) {
    auto& logger = core::getLogger();
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = in_use_.find(conn);
    if (it != in_use_.end()) {
        try {
            if (conn->is_open()) {
                available_.push(conn);
                logger.debug("Released connection back to pool");
            } else {
                logger.warn("Discarding closed connection");
            }
            in_use_.erase(it);
        }
        catch (const std::exception& e) {
            logger.error("Error releasing connection: {}", e.what());
        }
    } else {
        logger.warn("Attempting to release unmanaged connection");
    }
    cv_.notify_one();
}

} // namespace gameworld::database