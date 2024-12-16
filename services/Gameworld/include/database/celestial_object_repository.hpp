#pragma once

#include <span>
#include <vector>

#include "core/logger.hpp"
#include "database/repository_base.hpp"

#include "gameworld.pb.h"

namespace gameworld::database {
using namespace gameworld::v1;

class ICelestialObjectRepository : public IRepository<CelestialObject> {
public:
    explicit ICelestialObjectRepository(
        std::shared_ptr<Database> db,
        std::shared_ptr<core::Logger> logger
    ) : IRepository<CelestialObject>(std::move(db))
      , logger_(std::move(logger)) {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
    }

    virtual std::vector<CelestialObject> findByType(CelestialObjectType type) = 0;
    virtual std::vector<CelestialObject> findByParent(const std::string& parentId) = 0;
    virtual std::vector<CelestialObject> findInRegion(const GlobalCoordinates& center, double radius) = 0;
    
    virtual std::vector<CelestialObject> getChildren(const std::string& parentId) = 0;
    virtual std::optional<CelestialObject> getParent(const std::string& childId) = 0;
    
    virtual bool updateProperties(
        const std::string& id, 
        const CelestialObjectProperty& properties,
        std::span<const std::string> updateMask) = 0;

    virtual std::optional<CelestialObjectProperty> getProperties(const std::string& id) = 0;

    virtual std::vector<v1::GetObjectTypesResponse::TypeInfo> getObjectTypes(std::optional<CelestialObjectType> parentType = std::nullopt) = 0;
    virtual std::vector<std::string> getAvailableProperties(CelestialObjectType type) = 0;

protected:
    std::shared_ptr<core::Logger> logger_;
};

}  // namespace gameworld::database