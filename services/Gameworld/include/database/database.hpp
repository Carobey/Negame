#pragma once

#include <pqxx/pqxx>

#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "connection_pool.hpp"

namespace gameworld::database {
class Database {
public:
    Database(const std::string& host,
            const int port,
            const std::string& dbname,
            const std::string& user,
            const std::string& password)
        : Database(buildConnectionString(host, port, dbname, user, password)) {}

    explicit Database(const std::string& connection_string);
    
    template<typename Func> 
    auto executeTransaction(Func&& func) {
        auto conn = pool_->acquire();
        try {
            pqxx::work txn{*conn};
            auto result = func(txn);
            txn.commit();
            pool_->release(conn);
            return result;
        }
        catch (...) {
            pool_->release(conn);
            throw;
        }
    }

    template<typename Func>
    auto executeQuery(Func&& func) {
        auto conn = pool_->acquire();
        try {
            auto result = func(*conn);
            pool_->release(conn);
            return result;
        }
        catch (...) {
            pool_->release(conn);
            throw;
        }
    }

private:
    std::unique_ptr<ConnectionPool> pool_;
    static std::string buildConnectionString(
        const std::string& host,
        const int port,
        const std::string& dbname,
        const std::string& user,
        const std::string& password) {
        return "host=" + host +
               " port=" + std::to_string(port) +
               " dbname=" + dbname +
               " user=" + user +
               " password=" + password;
    }
};

} // namespace gameworld::database