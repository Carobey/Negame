#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <queue>
#include <mutex>
#include <set>
#include <condition_variable>

namespace gameworld::database {

class ConnectionPool {
public:
    explicit ConnectionPool(
        const std::string& connection_string,
        size_t max_connections = 10);
    
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    [[nodiscard]] std::shared_ptr<pqxx::connection> acquire();
    void release(const std::shared_ptr<pqxx::connection>& conn);

private:
    const std::string connection_string_;
    const size_t max_connections_;
    
    std::queue<std::shared_ptr<pqxx::connection>> available_;
    std::set<std::shared_ptr<pqxx::connection>> in_use_;
    
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace gameworld::database