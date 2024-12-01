#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <format>
#include <memory>
#include <string>
#include <mutex>
#include <source_location>

namespace gameworld::core {

class Logger {
public:
    using severity_level = boost::log::trivial::severity_level;

    Logger() = default;
    ~Logger() noexcept = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void init(const std::string& logFile, severity_level minLevel = severity_level::info) {
        try {
            namespace logging = boost::log;
            namespace keywords = boost::log::keywords;
            namespace expr = boost::log::expressions;

            // Настраиваем форматирование
            logging::formatter fmt = expr::stream
                << "["   << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << "] [" << expr::attr<std::string>("ThreadID")
                << "] [" << logging::trivial::severity
                << "] "  << expr::smessage;

            // Настраиваем логирование в файл
            logging::add_file_log(
                keywords::file_name = logFile,
                keywords::format = fmt,
                keywords::auto_flush = true,
                keywords::rotation_size = 10 * 1024 * 1024,
                keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(0, 0, 0)
            );

            // Настраиваем логирование в консоль
            logging::add_console_log(
                std::cout,
                keywords::format = fmt
            );

            // Добавляем глобальные атрибуты
            logging::core::get()->add_global_attribute("ThreadID", logging::attributes::current_thread_id());
            logging::core::get()->set_filter(logging::trivial::severity >= minLevel);
            logging::add_common_attributes();

            // Создаем логгер
            logger_ = std::make_unique<logging::sources::severity_logger<logging::trivial::severity_level>>();
        }
        catch (const std::exception& e) {
            throw std::runtime_error(std::format("Failed to initialize logger: {}", e.what()));
        }
    }

    template<typename... Args>
    void log(severity_level level, std::format_string<Args...> fmt, Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            auto message = std::format(fmt, std::forward<Args>(args)...);
            BOOST_LOG_SEV(*logger_, level) << message;
        }
        catch (const std::exception& e) {
            BOOST_LOG_SEV(*logger_, severity_level::error) 
                << std::format("Format error: {}. Original format: {}", e.what(), fmt.get());
        }
    }

    // Логирование с указанием места в коде
    template<typename... Args>
    void log_loc(severity_level level, 
                 std::format_string<Args...> fmt, 
                 const std::source_location& loc,
                 Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            auto message = std::format("[{}:{}] {}", 
                loc.file_name(), 
                loc.line(), 
                std::format(fmt, std::forward<Args>(args)...)
            );
            BOOST_LOG_SEV(*logger_, level) << message;
        }
        catch (const std::exception& e) {
            BOOST_LOG_SEV(*logger_, severity_level::error) 
                << std::format("Format error at {}:{}: {}. Original format: {}", 
                    loc.file_name(), loc.line(), e.what(), fmt.get());
        }
    }

    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) {
        log(severity_level::debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        log(severity_level::info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) {
        log(severity_level::warning, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        log(severity_level::error, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args) {
        log(severity_level::fatal, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug_loc(const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args
                  ) {
        log_loc(severity_level::debug, fmt, loc, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info_loc(const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
        log_loc(severity_level::info, fmt, loc, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn_loc(const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
        log_loc(severity_level::warning, fmt, loc, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error_loc(const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
        log_loc(severity_level::error, fmt, loc, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void fatal_loc(const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
        log_loc(severity_level::fatal, fmt, loc, std::forward<Args>(args)...);
    }

private:
    std::unique_ptr<boost::log::sources::severity_logger<boost::log::trivial::severity_level>> logger_;
    mutable std::mutex mutex_;
};

// Глобальный доступ к логгеру через синглтон
inline Logger& getLogger() {
    static Logger instance;
    return instance;
}

} // namespace gameworld::core