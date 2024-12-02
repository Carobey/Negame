#pragma once

#include <boost/json.hpp>
#include <boost/signals2.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <optional>
#include <fstream>
#include "core/logger.hpp"

namespace gameworld::core {

class ConfigHandler {
public:
    using ConfigChangeSignal = boost::signals2::signal<void(const boost::json::object&)>;

    static ConfigHandler& getInstance() {
        static ConfigHandler instance;
        return instance;
    }

    ConfigHandler(const ConfigHandler&) = delete;
    ConfigHandler& operator=(const ConfigHandler&) = delete;

    bool loadConfig(const std::filesystem::path& configPath) {
        try {
            auto& logger = getLogger();
            
            if (!std::filesystem::exists(configPath)) {
                logger.error("Config file does not exist: {}", configPath.string());
                return false;
            }

            std::ifstream file(configPath);
            if (!file.is_open()) {
                logger.error("Cannot open config file: {}", configPath.string());
                return false;
            }

            std::string jsonStr((std::istreambuf_iterator<char>(file)), 
                               std::istreambuf_iterator<char>());
            
            auto value = boost::json::parse(jsonStr);
            if (!value.is_object()) {
                logger.error("Root JSON value is not an object");
                return false;
            }

            config_ = value.get_object();
            
            if (!validateConfig()) {
                return false;
            }

            configChanged_(config_); // Оповещаем подписчиков об изменении конфигурации
            logger.info("Configuration loaded successfully from {}", configPath.string());
            return true;
        }
        catch (const std::exception& e) {
            getLogger().error("Error loading config: {}", e.what());
            return false;
        }
    }

    template<typename T>
    T get(std::string_view path, const T& defaultValue) const noexcept {
        try {
            return get<T>(path);
        } 
        catch(const std::exception& e) {
            getLogger().warn("Failed to get config value for path '{}': {}. Using default value.", 
                path, e.what());
            return defaultValue;
        }
    }

    template<typename T>
    T get(std::string_view path) const {
        auto parts = splitPath(path);
        auto value = getValueByPath(config_, parts);
        
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string(value.as_string());
        } 
        else if constexpr (std::is_same_v<T, int>) {
            return static_cast<int>(value.is_int64() ? value.as_int64() : value.as_double());
        } 
        else if constexpr (std::is_same_v<T, double>) {
            return value.is_double() ? value.as_double() : static_cast<double>(value.as_int64());
        } 
        else if constexpr (std::is_same_v<T, bool>) {
            return value.as_bool();
        } 
        else {
            return boost::json::value_to<T>(value);
        }
    }

    template<typename T>
    std::optional<T> getValue(std::string_view key) const noexcept {
        try {
            if (auto ptr = config_.if_contains(key)) {
                return boost::json::value_to<T>(*ptr);
            }
            return std::nullopt;
        }
        catch (const std::exception& e) {
            getLogger().warn("Error getting config value for key '{}': {}", key, e.what());
            return std::nullopt;
        }
    }

    boost::signals2::connection subscribeToChanges(
        const std::function<void(const boost::json::object&)>& callback) {
        return configChanged_.connect(callback);
    }

private:
    ConfigHandler() = default;
    boost::json::object config_;
    ConfigChangeSignal configChanged_;

    static std::vector<std::string> splitPath(std::string_view path) {
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

    bool validateConfig() {
        auto& logger = getLogger();
        
        static const std::array<std::string_view, 3> requiredFields = {
            "database",
            "service",
            "logging"
        };

        for (const auto& field : requiredFields) {
            if (!config_.contains(field)) {
                logger.error("Required field missing in config: {}", field);
                return false;
            }
        }

        return validateDatabaseConfig();
    }

    bool validateDatabaseConfig() {
        auto& logger = getLogger();
        const auto& db = config_.at("database").as_object();
        
        static const std::array<std::string_view, 5> required = {
            "host", "port", "name", "user", "password"
        };

        for (const auto& field : required) {
            if (!db.contains(field)) {
                logger.error("Required database field missing: {}", field);
                return false;
            }
        }

        if (db.at("port").as_int64() <= 0 || db.at("port").as_int64() > 65535) {
            logger.error("Invalid database port");
            return false;
        }

        return true;
    }

    static boost::json::value getValueByPath(
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
};

} // namespace gameworld::core