#pragma once

#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <array>

namespace gameworld::core::utils {

inline std::vector<std::string> splitPath(std::string_view path) {
    std::vector<std::string> parts;
    std::string part;
    
    for (char c : path) {
        if (c == '.') {
            if (!part.empty()) {
                parts.push_back(std::move(part));
                part.clear();
            }
        } else {
            part += c;
        }
    }
    
    if (!part.empty()) {
        parts.push_back(std::move(part));
    }
    
    return parts;
}

inline boost::json::value getValueByPath(
    const boost::json::object& obj,
    const std::vector<std::string>& path
) {
    if (path.empty()) {
        throw std::invalid_argument("Empty path");
    }

    const auto& key = path.front();
    if (!obj.contains(key)) {
        throw std::runtime_error(std::string("Path element not found: ") + key);
    }

    if (path.size() == 1) {
        return obj.at(key);
    }

    if (!obj.at(key).is_object()) {
        throw std::runtime_error(std::string("Path element is not an object: ") + key);
    }

    return getValueByPath(
        obj.at(key).as_object(),
        std::vector<std::string>(path.begin() + 1, path.end())
    );
}

inline bool validateDatabaseConfig(const boost::json::object& db) {
    static const std::array<std::string_view, 5> required = {
        "host", "port", "name", "user", "password"
    };

    for (const auto& field : required) {
        if (!db.contains(field)) {
            return false;
        }
    }

    return !(db.at("port").as_int64() <= 0 || db.at("port").as_int64() > 65535);
}

}  // namespace gameworld::core::utils