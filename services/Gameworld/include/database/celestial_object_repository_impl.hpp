#pragma once

#include <boost/json.hpp>

#include "database/celestial_object_repository.hpp"

namespace gameworld::database {

class CelestialObjectRepositoryImpl : public ICelestialObjectRepository {
public:
    explicit CelestialObjectRepositoryImpl(
        std::shared_ptr<Database> db,
        std::shared_ptr<core::Logger> logger
    ) : ICelestialObjectRepository(std::move(db), std::move(logger)) {}


    std::optional<v1::CelestialObject> getById(const std::string& id) override;
    std::vector<v1::CelestialObject> list(const std::string& filter = "") override;
    v1::CelestialObject create(const v1::CelestialObject& entity) override;
    bool update(const v1::CelestialObject& entity) override;
    bool remove(const std::string& id) override;

    std::vector<v1::CelestialObject> findByType(v1::CelestialObjectType type) override;
    std::vector<v1::CelestialObject> findByParent(const std::string& parentId) override;
    std::vector<v1::CelestialObject> findInRegion(const v1::GlobalCoordinates& center, double radius) override;
    std::vector<v1::CelestialObject> getChildren(const std::string& parentId) override;
    std::optional<v1::CelestialObject> getParent(const std::string& childId) override;

    bool updateProperties(
        const std::string& id,
        const v1::CelestialObjectProperty& properties,
        std::span<const std::string> updateMask) override;
    
    std::optional<v1::CelestialObjectProperty> getProperties(const std::string& id) override;
    std::vector<v1::GetObjectTypesResponse::TypeInfo> getObjectTypes(
        std::optional<v1::CelestialObjectType> parentType) override;
    std::vector<std::string> getAvailableProperties(v1::CelestialObjectType type) override;

private:
    static v1::CelestialObject rowToObject(const pqxx::row& row, std::shared_ptr<core::Logger>& logger);
    static v1::CelestialObjectProperty rowToProperties(const pqxx::row& row, std::shared_ptr<core::Logger>& logger); 
    static std::string propsToJson(const google::protobuf::Map<std::string, std::string>& props, std::shared_ptr<core::Logger>& logger);
    static v1::GlobalCoordinates extractCoordinates(const pqxx::row& row);
    static v1::LocalCoordinates extractLocalCoordinates(const pqxx::row& row);

    static void mapObjectType(CelestialObject& obj, const pqxx::row& row);
    static void mapBasicFields(CelestialObject& obj, const pqxx::row& row);
    static void mapCoordinates(CelestialObject& obj, const pqxx::row& row);
    static void mapPhysicalProperties(CelestialObject& obj, const pqxx::row& row);
    static void mapNumericProperty(google::protobuf::Map<std::string, std::string>* props,const pqxx::row& row, const std::string& field);
    static void mapDiscoveryInfo(CelestialObject& obj, const pqxx::row& row);
    static void mapJsonProperties(CelestialObject& obj, const pqxx::row& row, std::shared_ptr<core::Logger>& logger);
    static void parseAndMapJsonProperties(CelestialObject& obj, const std::string& json_str);
    static void mapJsonValue(google::protobuf::Map<std::string, std::string>* props, const boost::json::string& key, const boost::json::value& value); 

    [[nodiscard]] std::string buildFilterQuery(const std::string& baseFilter = "") const;
    [[nodiscard]] std::string buildUpdateQuery(std::span<const std::string> updateFields) const;
    
    [[nodiscard]] bool validateObject(const v1::CelestialObject& object) const;
    [[nodiscard]] bool validateProperties(const v1::CelestialObjectProperty& props) const;
};

}  // namespace gameworld::database