#pragma once

#include <optional>
#include <memory>
#include <vector>
#include <string>

#include "database/database.hpp"

namespace gameworld::database {

template<typename Entity>
class IRepository {
public:
    explicit IRepository(std::shared_ptr<Database> db) 
        : db_(std::move(db)) {
        if (!db_) {
            throw std::invalid_argument("Database cannot be null");
        }
    }
    
    virtual ~IRepository() = default;
    IRepository(const IRepository&) = delete;
    IRepository& operator=(const IRepository&) = delete;
    IRepository(IRepository&&) = delete;
    IRepository& operator=(IRepository&&) = delete;

    virtual std::optional<Entity> getById(const std::string& id) = 0;
    virtual std::vector<Entity> list(const std::string& filter = "") = 0;
    virtual Entity create(const Entity& entity) = 0;
    virtual bool update(const Entity& entity) = 0;
    virtual bool remove(const std::string& id) = 0;

protected:
    std::shared_ptr<Database> db_;
};

}  // namespace gameworld::database