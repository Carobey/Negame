// Реализация репозитория для работы с космическими объектами

#include <boost/format.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string/join.hpp>

#include "database/celestial_object_repository_impl.hpp"

namespace gameworld::database {
using namespace gameworld::v1;

namespace {  // SQL  //TODO transfer requests to an external file
constexpr std::string_view SELECT_OBJECT = R"(
    WITH RECURSIVE object_hierarchy AS (
        -- Базовый запрос для корневого объекта
        SELECT 
            c.id,
            c.parent_id,
            c.type,
            c.subtype,
            c.name,
            c.designation,
            c.coordinates,
            c.local_coordinates,
            c.mass_solar_masses,
            c.radius_solar_radii,
            c.temperature_kelvin,
            c.properties,
            c.discovered,
            c.discovery_date,
            c.created_at,
            c.updated_at,
            c.version,
            c.is_deleted,
            p.object_id as prop_object_id,
            p.proper_motion_ra,
            p.proper_motion_dec,
            p.radial_velocity,
            p.parallax,
            p.metallicity,
            p.age_years,
            p.properties as prop_properties,
            p.discovery_info,
            EXTRACT(EPOCH FROM c.discovery_date)::bigint as discovery_date_epoch,
            1 as level,
            ARRAY[c.id] as path
        FROM celestial_objects c
        LEFT JOIN celestial_object_properties p ON c.id = p.object_id
        WHERE c.id = $1 AND NOT c.is_deleted

        UNION ALL

        SELECT 
            c.id,
            c.parent_id,
            c.type,
            c.subtype,
            c.name,
            c.designation,
            c.coordinates,
            c.local_coordinates,
            c.mass_solar_masses,
            c.radius_solar_radii,
            c.temperature_kelvin,
            c.properties,
            c.discovered,
            c.discovery_date,
            c.created_at,
            c.updated_at,
            c.version,
            c.is_deleted,
            p.object_id as prop_object_id,
            p.proper_motion_ra,
            p.proper_motion_dec,
            p.radial_velocity,
            p.parallax,
            p.metallicity,
            p.age_years,
            p.properties as prop_properties,
            p.discovery_info,
            EXTRACT(EPOCH FROM c.discovery_date)::bigint as discovery_date_epoch,
            oh.level + 1,
            path || c.id
        FROM celestial_objects c
        LEFT JOIN celestial_object_properties p ON c.id = p.object_id
        INNER JOIN object_hierarchy oh ON c.parent_id = oh.id
        WHERE NOT c.is_deleted
    )
    SELECT * FROM object_hierarchy
    ORDER BY level, name;
)";

    constexpr std::string_view INSERT_OBJECT = R"(
        WITH inserted_object AS (
            INSERT INTO celestial_objects (
                id,
                parent_id, 
                type,
                subtype,
                name, 
                designation,
                coordinates,
                local_coordinates,
                mass_solar_masses,
                radius_solar_radii,
                temperature_kelvin,
                properties,
                discovered,
                discovery_date,
                created_at,
                updated_at
            ) VALUES (
                COALESCE($1, gen_random_uuid()),
                $2, $3, $4, $5, $6,
                ST_MakePoint($7, $8, $9),
                ST_MakePoint($10, $11, $12),
                $13, $14, $15, $16,
                $17,
                $18,
                CURRENT_TIMESTAMP,
                CURRENT_TIMESTAMP
            )
            RETURNING *
        )
        SELECT 
            o.*,
            p.*,
            EXTRACT(EPOCH FROM o.discovery_date) as discovery_date_epoch
        FROM inserted_object o
        LEFT JOIN celestial_object_properties p ON o.id = p.object_id;
    )";

    constexpr std::string_view UPDATE_OBJECT = R"(
        WITH updated_object AS (
            UPDATE celestial_objects
            SET 
                parent_id = $2,
                type = $3,
                subtype = $4,
                name = $5,
                designation = $6,
                coordinates = ST_MakePoint($7, $8, $9),
                local_coordinates = ST_MakePoint($10, $11, $12),
                mass_solar_masses = $13,
                radius_solar_radii = $14,
                temperature_kelvin = $15,
                properties = $16,
                discovered = $17,
                discovery_date = $18,
                updated_at = CURRENT_TIMESTAMP,
                version = version + 1
            WHERE id = $1 
              AND NOT is_deleted 
              AND version = $19
            RETURNING *
        )
        SELECT 
            o.*,
            p.*,
            EXTRACT(EPOCH FROM o.discovery_date) as discovery_date_epoch
        FROM updated_object o
        LEFT JOIN celestial_object_properties p ON o.id = p.object_id;
    )";

    constexpr std::string_view LIST_OBJECTS = R"(
        SELECT 
            c.*,
            p.*,
            EXTRACT(EPOCH FROM c.discovery_date) as discovery_date_epoch
        FROM celestial_objects c
        LEFT JOIN celestial_object_properties p ON c.id = p.object_id
        WHERE NOT c.is_deleted
        ORDER BY c.name;
    )";

    constexpr std::string_view FIND_BY_TYPE = R"(
        SELECT 
            c.*,
            p.*,
            EXTRACT(EPOCH FROM c.discovery_date) as discovery_date_epoch
        FROM celestial_objects c
        LEFT JOIN celestial_object_properties p ON c.id = p.object_id
        WHERE c.type = $1 AND NOT c.is_deleted
        ORDER BY c.name;
    )";

    constexpr std::string_view FIND_IN_REGION = R"(
        SELECT 
            c.*,
            p.*,
            EXTRACT(EPOCH FROM c.discovery_date) as discovery_date_epoch
        FROM celestial_objects c
        LEFT JOIN celestial_object_properties p ON c.id = p.object_id
        WHERE ST_DWithin(
            c.coordinates,
            ST_MakePoint($1, $2, $3),
            $4
        ) 
        AND NOT c.is_deleted
        ORDER BY 
            ST_Distance(c.coordinates, ST_MakePoint($1, $2, $3));
    )";

    constexpr std::string_view GET_CHILDREN = R"(
        SELECT 
            c.*,
            p.*,
            EXTRACT(EPOCH FROM c.discovery_date) as discovery_date_epoch
        FROM celestial_objects c
        LEFT JOIN celestial_object_properties p ON c.id = p.object_id
        WHERE c.parent_id = $1 
        AND NOT c.is_deleted
        ORDER BY c.name;
    )";

    constexpr std::string_view GET_PARENT = R"(
        SELECT 
            p.*,
            pp.*,
            EXTRACT(EPOCH FROM p.discovery_date) as discovery_date_epoch
        FROM celestial_objects c
        JOIN celestial_objects p ON c.parent_id = p.id
        LEFT JOIN celestial_object_properties pp ON p.id = pp.object_id
        WHERE c.id = $1 AND NOT p.is_deleted;
    )";

    constexpr std::string_view GET_PROPERTIES = R"(
        SELECT p.*,
            EXTRACT(EPOCH FROM c.discovery_date) as discovery_date_epoch
        FROM celestial_object_properties p
        JOIN celestial_objects c ON p.object_id = c.id
        WHERE p.object_id = $1;
    )";

    constexpr std::string_view UPDATE_PROPERTIES = R"(
        WITH updated_props AS (
            UPDATE celestial_object_properties
            SET
                proper_motion_ra = COALESCE($2, proper_motion_ra),
                proper_motion_dec = COALESCE($3, proper_motion_dec),
                radial_velocity = COALESCE($4, radial_velocity),
                parallax = COALESCE($5, parallax),
                metallicity = COALESCE($6, metallicity),
                age_years = COALESCE($7, age_years),
                properties = COALESCE($8, properties),
                discovery_info = COALESCE($9, discovery_info),
                updated_at = CURRENT_TIMESTAMP
            WHERE object_id = $1
            RETURNING *
        )
        SELECT 
            up.*,
            EXTRACT(EPOCH FROM c.discovery_date) as discovery_date_epoch
        FROM updated_props up
        JOIN celestial_objects c ON c.id = up.object_id;
    )";

constexpr std::string_view GET_OBJECT_TYPES = R"(
    SELECT 
        type,
        subtype,
        properties->>'description' as description,
        properties->>'parent_type' as parent_type,
        EXTRACT(EPOCH FROM created_at)::bigint as creation_date_epoch
    FROM celestial_object_types
    WHERE ($1::celestial_object_type IS NULL 
           OR properties->>'parent_type' = $1::text)
    ORDER BY type;
)";

    constexpr std::string_view REMOVE_OBJECT = R"(
        UPDATE celestial_objects
        SET 
            is_deleted = true,
            updated_at = CURRENT_TIMESTAMP,
            version = version + 1
        WHERE id = $1 AND NOT is_deleted
        RETURNING id;
    )";
}

std::optional<CelestialObject> CelestialObjectRepositoryImpl::getById(const std::string& id) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) -> std::optional<CelestialObject> {
            pqxx::work txn{conn};
            
            auto result = txn.exec_params(
                SELECT_OBJECT.data(),
                id
            );
            
            if (result.empty()) {
                logger_->debug("Celestial object not found: {}", id);
                return std::nullopt;
            }
            
            auto obj = rowToObject(result[0], logger_);
            logger_->debug("Retrieved celestial object: {} ({})", obj.name(), id);
            return obj;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to get celestial object by ID: {}", e.what());
        throw;
    }
}

std::vector<CelestialObject> CelestialObjectRepositoryImpl::list(const std::string& filter) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) {
            pqxx::work txn{conn};
            
            auto result = txn.exec(LIST_OBJECTS.data());
            
            std::vector<CelestialObject> objects;
            objects.reserve(result.size());
            
            for (const auto& row : result) {
                objects.push_back(rowToObject(row, logger_));
            }
            
            logger_->debug_loc(std::source_location::current(), "Listed {} celestial objects", objects.size());
            return objects;
        });
    }
    catch (const std::exception& e) {
        logger_->error_loc(std::source_location::current(), "Failed to list celestial objects: {}", e.what());
        throw;
    }
}

CelestialObject CelestialObjectRepositoryImpl::create(const CelestialObject& entity) {
    try {
        if (!validateObject(entity)) {
            throw std::invalid_argument("Invalid celestial object data");
        }

        return db_->executeTransaction([&](pqxx::work& txn) {
            std::string id = entity.id().empty() ? 
                boost::uuids::to_string(boost::uuids::random_generator()()) : 
                entity.id();

            auto glob_coords = entity.has_globcoordinates() ? 
                entity.globcoordinates() : GlobalCoordinates{};
            
            auto loc_coords = entity.has_loccoordinates() ? 
                entity.loccoordinates() : LocalCoordinates{};

            auto result = txn.exec_params(
                INSERT_OBJECT.data(),
                id,
                entity.has_parent_id() ? entity.parent_id() : nullptr,
                static_cast<int>(entity.type()),
                entity.subtype(),
                entity.name(),
                entity.has_designation() ? entity.designation() : nullptr,
                glob_coords.x(), glob_coords.y(), glob_coords.z(),  
                loc_coords.x(), loc_coords.y(), loc_coords.z(),    
                entity.properties().contains("mass_solar_masses") ? 
                    std::stod(entity.properties().at("mass_solar_masses")) : 0.0,
                entity.properties().contains("radius_solar_radii") ? 
                    std::stod(entity.properties().at("radius_solar_radii")) : 0.0,
                entity.properties().contains("temperature_kelvin") ? 
                    std::stod(entity.properties().at("temperature_kelvin")) : 0.0,
                propsToJson(entity.properties(), logger_),
                entity.has_discovered() ? entity.discovered() : false,
                entity.has_discovery_date() ? 
                    std::to_string(entity.discovery_date().seconds()) : nullptr
            );

            if (result.empty()) {
                throw std::runtime_error("Failed to create celestial object");
            }

            auto created = rowToObject(result[0], logger_);
            logger_->info("Created celestial object: {} ({})", created.name(), created.id());
            return created;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to create celestial object: {}", e.what());
        throw;
    }
}

bool CelestialObjectRepositoryImpl::update(const CelestialObject& entity) {
    try {
        if (!validateObject(entity)) {
            logger_->warn("Invalid object data for update: {}", entity.id());
            throw std::invalid_argument("Invalid celestial object data");
        }

        return db_->executeTransaction([&](pqxx::work& txn) {
            auto glob_coords = entity.has_globcoordinates() ? 
                entity.globcoordinates() : GlobalCoordinates{};
            
            auto loc_coords = entity.has_loccoordinates() ? 
                entity.loccoordinates() : LocalCoordinates{};

            auto result = txn.exec_params(
                UPDATE_OBJECT.data(),
                entity.id(),
                entity.has_parent_id() ? entity.parent_id() : nullptr,
                static_cast<int>(entity.type()),
                entity.subtype(),
                entity.name(),
                entity.has_designation() ? entity.designation() : nullptr,
                glob_coords.x(), glob_coords.y(), glob_coords.z(),
                loc_coords.x(), loc_coords.y(), loc_coords.z(),
                entity.properties().contains("mass_solar_masses") ? 
                    std::stod(entity.properties().at("mass_solar_masses")) : 0.0,
                entity.properties().contains("radius_solar_radii") ? 
                    std::stod(entity.properties().at("radius_solar_radii")) : 0.0,
                entity.properties().contains("temperature_kelvin") ? 
                    std::stod(entity.properties().at("temperature_kelvin")) : 0.0,
                propsToJson(entity.properties(), logger_),
                entity.has_discovered() ? entity.discovered() : false,
                entity.has_discovery_date() ? 
                    std::to_string(entity.discovery_date().seconds()) : nullptr,
                entity.properties().contains("version") ? 
                    std::stoi(entity.properties().at("version")) : 1
            );

            if (result.empty()) {
                logger_->warn("Celestial object not found or version mismatch: {}", entity.id());
                return false;
            }

            logger_->info("Updated celestial object: {} ({})", entity.name(), entity.id());
            return true;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to update celestial object: {}", e.what());
        throw;
    }
}

bool CelestialObjectRepositoryImpl::remove(const std::string& id) {
    try {
        return db_->executeTransaction([&](pqxx::work& txn) {
           auto result = txn.exec_params(REMOVE_OBJECT.data(),
                id
            );

            bool success = !result.empty();
            if (success) {
                logger_->info("Removed celestial object: {}", id);
            } else {
                logger_->warn("Celestial object not found for removal: {}", id);
            }
            return success;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to remove celestial object: {}", e.what());
        throw;
    }
}

std::vector<CelestialObject> CelestialObjectRepositoryImpl::findByType(CelestialObjectType type) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) {
            pqxx::work txn{conn};
            
            auto result = txn.exec_params(
                FIND_BY_TYPE.data(),
                static_cast<int>(type)
            );
            
            std::vector<CelestialObject> objects;
            objects.reserve(result.size());
            
            for (const auto& row : result) {
                objects.push_back(rowToObject(row, logger_));
            }
            
            logger_->debug_loc(std::source_location::current(), "Found {} objects of type {}", objects.size(), static_cast<int>(type));
            return objects;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to find objects by type: {}", e.what());
        throw;
    }
}

std::vector<CelestialObject> CelestialObjectRepositoryImpl::findInRegion(
    const GlobalCoordinates& center, 
    double radius
) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) {
            pqxx::work txn{conn};
            
            auto result = txn.exec_params(
                FIND_IN_REGION.data(),
                center.x(),
                center.y(),
                center.z(),
                radius
            );
            
            std::vector<CelestialObject> objects;
            objects.reserve(result.size());
            
            for (const auto& row : result) {
                objects.push_back(rowToObject(row, logger_));
            }
            
            logger_->debug("Found {} objects in region (center: {},{},{}, radius: {})", 
                objects.size(), center.x(), center.y(), center.z(), radius);
            return objects;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to find objects in region: {}", e.what());
        throw;
    }
}

bool CelestialObjectRepositoryImpl::updateProperties(
    const std::string& id,
    const CelestialObjectProperty& properties,
    std::span<const std::string> updateMask
) {
    try {
        if (!validateProperties(properties)) {
            logger_->warn("Invalid properties data for update: {}", id);
            throw std::invalid_argument("Invalid properties data");
        }

        return db_->executeTransaction([&](pqxx::work& txn) {
            std::vector<std::string> setColumns;
            std::vector<std::string> paramValues;
            int paramIndex = 2;  // Первый параметр - id

            for (const auto& field : updateMask) {
                setColumns.push_back(boost::str(
                    boost::format("%1% = $%2%") % 
                    txn.esc_like(field) % 
                    paramIndex++
                ));
                
                // TODO: add mapping of the destination properties fields
                //paramValues.push_back(getValue(properties, field));
            }
            
            if (setColumns.empty()) {
                logger_->warn("No fields to update for object: {}", id);
                return false;
            }

            auto query = boost::str(boost::format(UPDATE_PROPERTIES.data()) % 
                boost::algorithm::join(setColumns, ", "));

            auto result = txn.exec_params(
                query,
                id
                // TODO: add parameters from ParamValues
            );

            bool success = !result.empty();
            if (success) {
                logger_->info("Updated properties for object: {}", id);
            } else {
                logger_->warn("Failed to update properties for object: {}", id);
            }
            return success;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to update object properties: {}", e.what());
        throw;
    }
}

bool CelestialObjectRepositoryImpl::validateObject(const CelestialObject& object) const {
    if (object.name().empty()) {
        logger_->warn("Object name cannot be empty");
        return false;
    }

    if (object.type() == CelestialObjectType::CELESTIAL_OBJECT_TYPE_UNSPECIFIED) {
        logger_->warn("Object type must be specified");
        return false;
    }

    if (object.has_globcoordinates()) {
        const auto& coords = object.globcoordinates();
        if (std::abs(coords.x()) > 1e6 || 
            std::abs(coords.y()) > 1e6 || 
            std::abs(coords.z()) > 1e6) {
            logger_->warn("Global coordinates out of valid range");
            return false;
        }
    }

    switch (object.type()) {
        case CelestialObjectType::PLANET:
            if (!object.has_parent_id()) {
                logger_->warn("Planets must have a parent object");
                return false;
            }
            break;
        // TODO: Add checks for other types
        default:
            break;
    }

    return true;
}

bool CelestialObjectRepositoryImpl::validateProperties([[maybe_unused]] const CelestialObjectProperty& props) const {
    // TODO: Add checks
    return true;
}

void CelestialObjectRepositoryImpl::mapBasicFields(CelestialObject& obj, const pqxx::row& row) {
    obj.set_id(row["id"].as<std::string>());
    
    if (!row["parent_id"].is_null()) {
        obj.set_parent_id(row["parent_id"].as<std::string>());
    }

    obj.set_name(row["name"].as<std::string>());
    
    if (!row["designation"].is_null()) {
        obj.set_designation(row["designation"].as<std::string>());
    }
    
    if (!row["subtype"].is_null()) {
        obj.set_subtype(row["subtype"].as<std::string>());
    }
}

void CelestialObjectRepositoryImpl::mapObjectType(CelestialObject& obj, const pqxx::row& row) {
    static const std::unordered_map<std::string, CelestialObjectType> type_mapping = {
        {"GALAXY_CLUSTER", GALAXY_CLUSTER},
        {"GALAXY", GALAXY},
        {"GALAXY_ARM", GALAXY_ARM},
        {"MOLECULAR_CLOUD", MOLECULAR_CLOUD},
        {"STAR_SYSTEM_MULTIPLE", STAR_SYSTEM_MULTIPLE},
        {"STAR_SYSTEM_BINARY", STAR_SYSTEM_BINARY}, 
        {"STAR_SYSTEM_SINGLE", STAR_SYSTEM_SINGLE},
        {"STAR", STAR},
        {"BLACK_HOLE_STELLAR", BLACK_HOLE_STELLAR},
        {"BROWN_DWARF", BROWN_DWARF},
        {"PLANET", PLANET},
        {"DWARF_PLANET", DWARF_PLANET},
        {"PLANETOID", PLANETOID},
        {"ASTEROID", ASTEROID},
        {"ASTEROID_BELT", ASTEROID_BELT},
        {"COMET", COMET},
        {"KUIPER_BELT_OBJECT", KUIPER_BELT_OBJECT},
        {"OORT_CLOUD_OBJECT", OORT_CLOUD_OBJECT},
        {"DYSON_SPHERE", DYSON_SPHERE},
        {"DYSON_SWARM", DYSON_SWARM},
        {"ARTIFICIAL_HABITAT", ARTIFICIAL_HABITAT},
        {"SPACE_STATION", SPACE_STATION},
        {"STELLAR_ENGINE", STELLAR_ENGINE},
        {"WORMHOLE", WORMHOLE},
        {"QUANTUM_VACUUM_MINE", QUANTUM_VACUUM_MINE}
    };
    
        if (!row["type"].is_null()) {
        std::string type_str = row["type"].as<std::string>();
        auto it = type_mapping.find(type_str);
        obj.set_type(it != type_mapping.end() ? 
            it->second : CELESTIAL_OBJECT_TYPE_UNSPECIFIED);
    } else {
        obj.set_type(CELESTIAL_OBJECT_TYPE_UNSPECIFIED);
    }
}

void CelestialObjectRepositoryImpl::mapCoordinates(CelestialObject& obj, const pqxx::row& row) {
    if (!row["coordinates"].is_null()) {
        auto* coords = obj.mutable_globcoordinates();
        auto global_coords = extractCoordinates(row);
        coords->CopyFrom(global_coords);
    }

    if (!row["local_coordinates"].is_null()) {
        auto* coords = obj.mutable_loccoordinates();
        auto local_coords = extractLocalCoordinates(row);
        coords->CopyFrom(local_coords);
    }
}

void CelestialObjectRepositoryImpl::mapPhysicalProperties(CelestialObject& obj, const pqxx::row& row) {
    auto* props = obj.mutable_properties();

    mapNumericProperty(props, row, "mass_solar_masses");
    mapNumericProperty(props, row, "radius_solar_radii");
    mapNumericProperty(props, row, "temperature_kelvin");
}

void CelestialObjectRepositoryImpl::mapNumericProperty(google::protobuf::Map<std::string, std::string>* props, 
                                const pqxx::row& row, 
                                const std::string& field) {
    if (!row[field].is_null()) {
        (*props)[field] = std::to_string(row[field].as<double>());
    }
}

void CelestialObjectRepositoryImpl::mapDiscoveryInfo(CelestialObject& obj, const pqxx::row& row) {
    if (!row["discovered"].is_null()) {
        obj.set_discovered(row["discovered"].as<bool>());
    }

    if (!row["discovery_date"].is_null() && !row["discovery_date_epoch"].is_null()) {
        auto* timestamp = obj.mutable_discovery_date();
        int64_t epoch_seconds = std::llround(row["discovery_date_epoch"].as<double>());
        timestamp->set_seconds(epoch_seconds);
        timestamp->set_nanos(0);
    }
}

void CelestialObjectRepositoryImpl::mapJsonProperties(CelestialObject& obj, const pqxx::row& row, 
                            std::shared_ptr<core::Logger>& logger) {
    if (!row["properties"].is_null()) {
        try {
            parseAndMapJsonProperties(obj, row["properties"].as<std::string>());
        } catch (const std::exception& e) {
            logger->warn("Failed to parse properties JSON: {}", e.what());
        }
    }
}

void CelestialObjectRepositoryImpl::parseAndMapJsonProperties(CelestialObject& obj, const std::string& json_str) {
    auto props_json = boost::json::parse(json_str);
    auto* props = obj.mutable_properties();
    
    if (props_json.is_object()) {
        const auto& json_obj = props_json.as_object();
        for (const auto& [key, value] : json_obj) {
            mapJsonValue(props, key, value);
        }
    }
}

void CelestialObjectRepositoryImpl::mapJsonValue(google::protobuf::Map<std::string, std::string>* props, 
                        const boost::json::string& key, 
                        const boost::json::value& value) {
    std::string prop_key = std::string(key);
    
    if (value.is_string()) {
        (*props)[prop_key] = std::string(value.as_string());
    } else if (value.is_number()) {
        (*props)[prop_key] = boost::json::serialize(value);
    } else if (value.is_bool()) {
        (*props)[prop_key] = value.as_bool() ? "true" : "false";
    } else {
        (*props)[prop_key] = boost::json::serialize(value);
    }
}

CelestialObject CelestialObjectRepositoryImpl::rowToObject(const pqxx::row& row, std::shared_ptr<core::Logger>& logger) {
    try {
        CelestialObject obj;
        
        mapBasicFields(obj, row);
        mapObjectType(obj, row);
        mapCoordinates(obj, row);
        mapPhysicalProperties(obj, row);
        mapDiscoveryInfo(obj, row);
        mapJsonProperties(obj, row, logger);

        // Версионность для оптимистичной блокировки
        if (!row["version"].is_null()) {
            auto* props = obj.mutable_properties();
            (*props)["version"] = std::to_string(row["version"].as<int>());
        }
        
        return obj;
    }
    catch (const std::exception& e) {
        logger->error("Failed to convert row to CelestialObject: {}", e.what());
        throw;
    }
}

CelestialObjectProperty CelestialObjectRepositoryImpl::rowToProperties(const pqxx::row& row, std::shared_ptr<core::Logger>& logger) {
    try {
        CelestialObjectProperty props;
        
        if (!row["proper_motion_ra"].is_null()) {
            props.set_proper_motion_ra(row["proper_motion_ra"].as<double>());
        }
        if (!row["proper_motion_dec"].is_null()) {
            props.set_proper_motion_dec(row["proper_motion_dec"].as<double>());
        }
        if (!row["radial_velocity"].is_null()) {
            props.set_radial_velocity(row["radial_velocity"].as<double>());
        }
        if (!row["parallax"].is_null()) {
            props.set_parallax(row["parallax"].as<double>());
        }
        
        if (!row["metallicity"].is_null()) {
            props.set_metallicity(row["metallicity"].as<double>());
        }
        
        if (!row["research_data"].is_null()) { //убрать
            auto research_json = boost::json::parse(row["research_data"].as<std::string>());
            /*auto* research_data = props.mutable_research_data();
            
            if (research_json.is_object()) {
                const auto& obj = research_json.as_object();
                for (const auto& [key, value] : obj) {
                    (*research_data)[std::string(key)] = boost::json::serialize(value);
                }
            }*/
        }
        
        return props;
    }
    catch (const std::exception& e) {
        logger->error("Failed to convert row to CelestialObjectProperty: {}", e.what());
        throw;
    }
}

std::string CelestialObjectRepositoryImpl::propsToJson(
    const google::protobuf::Map<std::string, std::string>& props, 
     std::shared_ptr<core::Logger>& logger
) {
    try {
        boost::json::object obj;
        for (const auto& [key, value] : props) {
            try {
                obj[key] = boost::json::parse(value);
            }
            catch (...) {
                obj[key] = value;
            }
        }
        return boost::json::serialize(obj);
    }
    catch (const std::exception& e) {
        logger->error("Failed to convert properties to JSON: {}", e.what());
        throw;
    }
}

std::vector<CelestialObject> CelestialObjectRepositoryImpl::findByParent(const std::string& parentId) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) {
            pqxx::work txn{conn};
            
            auto result = txn.exec_params(
                GET_CHILDREN.data(),
                parentId
            );
            
            std::vector<CelestialObject> objects;
            objects.reserve(result.size());
            
            for (const auto& row : result) {
                objects.push_back(rowToObject(row, logger_));
            }
            
            logger_->debug("Found {} child objects for parent {}", objects.size(), parentId);
            return objects;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to find objects by parent: {}", e.what());
        throw;
    }
} 

std::vector<v1::GetObjectTypesResponse::TypeInfo> CelestialObjectRepositoryImpl::getObjectTypes(
    std::optional<CelestialObjectType> parentType
) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) {
            pqxx::work txn{conn};
            
            pqxx::result result;
            if (parentType) {
                result = txn.exec_params(GET_OBJECT_TYPES.data(), 
                    static_cast<int>(*parentType));
            } else {
                result = txn.exec_params(GET_OBJECT_TYPES.data(), nullptr);
            }
            
            std::vector<v1::GetObjectTypesResponse::TypeInfo> types;
            types.reserve(result.size());
            
            for (const auto& row : result) {
                v1::GetObjectTypesResponse::TypeInfo type;
                type.set_type(static_cast<CelestialObjectType>(row["type"].as<int>()));
                if (!row["description"].is_null()) {
                    type.set_description(row["description"].as<std::string>());
                }
                if (!row["parent_type"].is_null()) {
                    // Преобразуем строку в enum 
                    // to-do сделать нормально
                    static const std::unordered_map<std::string, CelestialObjectType> type_mapping = {
                        {"GALAXY_CLUSTER", GALAXY_CLUSTER},
                        {"GALAXY", GALAXY},
                        {"GALAXY_ARM", GALAXY_ARM},
                        {"MOLECULAR_CLOUD", MOLECULAR_CLOUD},
                        {"STAR_SYSTEM_MULTIPLE", STAR_SYSTEM_MULTIPLE},
                        {"STAR_SYSTEM_BINARY", STAR_SYSTEM_BINARY},
                        {"STAR_SYSTEM_SINGLE", STAR_SYSTEM_SINGLE},
                        {"STAR", STAR},
                        {"BLACK_HOLE_STELLAR", BLACK_HOLE_STELLAR},
                        {"BROWN_DWARF", BROWN_DWARF},
                        {"PLANET", PLANET},
                        {"DWARF_PLANET", DWARF_PLANET},
                        {"PLANETOID", PLANETOID},
                        {"ASTEROID", ASTEROID},
                        {"ASTEROID_BELT", ASTEROID_BELT},
                        {"COMET", COMET},
                        {"KUIPER_BELT_OBJECT", KUIPER_BELT_OBJECT},
                        {"OORT_CLOUD_OBJECT", OORT_CLOUD_OBJECT},
                        {"DYSON_SPHERE", DYSON_SPHERE},
                        {"DYSON_SWARM", DYSON_SWARM},
                        {"ARTIFICIAL_HABITAT", ARTIFICIAL_HABITAT},
                        {"SPACE_STATION", SPACE_STATION},
                        {"STELLAR_ENGINE", STELLAR_ENGINE},
                        {"WORMHOLE", WORMHOLE},
                        {"QUANTUM_VACUUM_MINE", QUANTUM_VACUUM_MINE}
                    };
                    
                    std::string parent_type_str = row["parent_type"].as<std::string>();
                    auto it = type_mapping.find(parent_type_str);
                    if (it != type_mapping.end()) {
                        type.set_parent_type(it->second);
                    }
                }
                types.push_back(std::move(type));
            }
            
            return types;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to get object types: {}", e.what());
        throw;
    }
}

std::vector<CelestialObject> CelestialObjectRepositoryImpl::getChildren(const std::string& parentId) {
    return findByParent(parentId);
}

std::optional<CelestialObject> CelestialObjectRepositoryImpl::getParent(const std::string& childId) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) -> std::optional<CelestialObject> {
            pqxx::work txn{conn};
            
            auto result = txn.exec_params( GET_PARENT.data(),
                childId
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            return rowToObject(result[0], logger_);
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to get parent object: {}", e.what());
        throw;
    }
}

std::optional<CelestialObjectProperty> CelestialObjectRepositoryImpl::getProperties(const std::string& id) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) -> std::optional<CelestialObjectProperty> {
            pqxx::work txn{conn};
            
            auto result = txn.exec_params(GET_PROPERTIES.data(),
                id
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            return rowToProperties(result[0], logger_);
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to get object properties: {}", e.what());
        throw;
    }
}

std::vector<std::string> CelestialObjectRepositoryImpl::getAvailableProperties(CelestialObjectType type) {
    try {
        return db_->executeQuery([&](pqxx::connection& conn) {
            pqxx::work txn{conn};
            
            auto result = txn.exec_params(GET_PROPERTIES.data(),
                static_cast<int>(type)
            );
            
            std::vector<std::string> properties;
            properties.reserve(result.size());
            
            for (const auto& row : result) {
                properties.push_back(row[0].as<std::string>());
            }
            
            return properties;
        });
    }
    catch (const std::exception& e) {
        logger_->error("Failed to get available properties: {}", e.what());
        throw;
    }
}

GlobalCoordinates CelestialObjectRepositoryImpl::extractCoordinates(const pqxx::row& row) {
    GlobalCoordinates coords;
    if (!row["coordinates"].is_null()) {
        std::string point_str = row["coordinates"].as<std::string>();
        double x = 0, y = 0, z = 0;
        if (sscanf(point_str.c_str(), "POINT(%lf %lf %lf)", &x, &y, &z) == 3) {
            coords.set_x(x);
            coords.set_y(y);
            coords.set_z(z);
        }
    }
    return coords;
}

LocalCoordinates CelestialObjectRepositoryImpl::extractLocalCoordinates(const pqxx::row& row) {
    LocalCoordinates coords;
    if (!row["local_coordinates"].is_null()) {
        std::string point_str = row["local_coordinates"].as<std::string>();
        double x = 0, y = 0, z = 0;
        if (sscanf(point_str.c_str(), "POINT(%lf %lf %lf)", &x, &y, &z) == 3) {
            coords.set_x(x);
            coords.set_y(y);
            coords.set_z(z);
        }
    }
    return coords;
}

} // namespace gameworld::database