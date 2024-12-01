#pragma once

#include <boost/json.hpp>
#include <boost/signals2.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include <fstream>

#include "core/logger.hpp"
#include "core/utils.hpp"

namespace gameworld::core {

class ConfigHandler {
public:
    using ConfigChangeSignal = boost::signals2::signal<void(const boost::json::object&)>;
    
    explicit ConfigHandler(std::shared_ptr<Logger> logger) 
        : logger_(std::move(logger)) {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
    }

    bool loadConfig(const std::filesystem::path& configPath) {
        try {
            if (!validateConfigPath(configPath)) {
                return false;
            }

            auto jsonStr = readConfigFile(configPath);
            if (!jsonStr) {
                return false;
            }

            auto config = parseJsonConfig(*jsonStr);
            if (!config) {
                return false;
            }

            if (!validateConfigFields(*config)) {
                return false;
            }

            config_ = *config;
            configChanged_(config_);
            
            logger_->info("Configuration loaded successfully from {}", configPath.string());
            return true;
        }
        catch (const std::exception& e) {
            logger_->error("Error loading config: {}", e.what());
            return false;
        }
    }

    template<typename T>
    T get(std::string_view path, const T& defaultValue) const noexcept {
        try {
            return get<T>(path);
        } 
        catch(const std::exception& e) {
            logger_->warn("Failed to get config value for path '{}': {}. Using default value.", 
                path, e.what());
            return defaultValue;
        }
    }

    template<typename T>
    T get(std::string_view path) const {
        auto parts = utils::splitPath(path);
        auto value = utils::getValueByPath(config_, parts);
        
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

    boost::signals2::connection subscribeToChanges(
        const std::function<void(const boost::json::object&)>& callback) {
        return configChanged_.connect(callback);
    }

private:
    ConfigHandler() = default;

    bool validateConfigPath(const std::filesystem::path& configPath) const {
        if (!std::filesystem::exists(configPath)) {
            logger_->error("Config file does not exist: {}", configPath.string());
            return false;
        }
        return true;
    }

    std::optional<std::string> readConfigFile(const std::filesystem::path& configPath) const {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            logger_->error("Cannot open config file: {}", configPath.string());
            return std::nullopt;
        }

        return std::string(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
    }

    std::optional<boost::json::object> parseJsonConfig(const std::string& jsonStr) const {
        try {
            auto value = boost::json::parse(jsonStr);
            if (!value.is_object()) {
                logger_->error("Root JSON value is not an object");
                return std::nullopt;
            }
            return value.get_object();
        }
        catch (const std::exception& e) {
            logger_->error("JSON parsing error: {}", e.what());
            return std::nullopt;
        }
    }

    bool validateConfigFields(const boost::json::object& config) const {
        static const std::array<std::string_view, 3> requiredFields = {
            "database", "service", "logging"
        };

        for (const auto& field : requiredFields) {
            if (!config.contains(field)) {
                logger_->error("Required field missing in config: {}", field);
                return false;
            }
        }

        if (!utils::validateDatabaseConfig(config.at("database").as_object())) {
            logger_->error("Invalid database configuration");
            return false;
        }

        return true;
    }
    
    bool validateConfig() {
        static const std::array<std::string_view, 3> requiredFields = {
            "database", "service", "logging"
        };

        for (const auto& field : requiredFields) {
            if (!config_.contains(field)) {
                logger_->error("Required field missing in config: {}", field);
                return false;
            }
        }

        if (!utils::validateDatabaseConfig(config_.at("database").as_object())) {
            logger_->error("Invalid database configuration");
            return false;
        }

        return true;
    }

    std::shared_ptr<Logger> logger_; 
    boost::json::object config_;
    ConfigChangeSignal configChanged_;
};

} // namespace gameworld::core