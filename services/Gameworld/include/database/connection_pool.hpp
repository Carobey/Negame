#pragma once

#include <pqxx/pqxx>

#include <memory>
#include <queue>
#include <mutex>
#include <set>
#include <condition_variable>
#include <utility>

#include "core/logger.hpp"

namespace gameworld::database {

class ConnectionPool {
public:
    explicit ConnectionPool(
        std::string  connection_string,
        std::shared_ptr<core::Logger> logger,
        size_t max_connections
    ) : connection_string_(std::move(connection_string))
      , logger_(std::move(logger))
      , max_connections_(max_connections) {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
        initializePool();
    }
    
    ~ConnectionPool();
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    [[nodiscard]] std::shared_ptr<pqxx::connection> acquire();
    void release(const std::shared_ptr<pqxx::connection>& conn);

private:
    void initializePool();

    const std::string connection_string_;
    std::shared_ptr<core::Logger> logger_;
    const size_t max_connections_;
    
    std::queue<std::shared_ptr<pqxx::connection>> available_;
    std::set<std::shared_ptr<pqxx::connection>> in_use_;
    
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace gameworld::database