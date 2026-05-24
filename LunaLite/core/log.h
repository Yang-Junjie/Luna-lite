#pragma once

#include <cstdint>
#include <cstdlib>

#include <memory>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace lunalite::core {

class Logger {
public:
    enum class Type : uint8_t {
        Core = 0,
        RHI = 1
    };

    enum class Level : uint8_t {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off
    };

    static void init(const std::string& log_file = {}, Level level = Level::Info);
    static void shutdown();

    static bool isInitialized();
    static void setLevel(Level level);
    static Level getLevel();

    static const std::shared_ptr<spdlog::logger>& get(Type type);
    static const std::shared_ptr<spdlog::logger>& core();
    static const std::shared_ptr<spdlog::logger>& rhi();
    static void flush();

private:
    static void ensureInitialized();
    static void configureLogger(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level);
    static spdlog::level::level_enum toSpdlogLevel(Level level);

private:
    static inline bool m_s_initialized = false;
    static inline Level m_s_level = Level::Info;
    static inline std::string m_s_log_file;
    static inline std::vector<spdlog::sink_ptr> m_s_sinks;
    static inline std::shared_ptr<spdlog::logger> m_s_core_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_rhi_logger;
};

} // namespace lunalite::core

#define LUNA_CORE_TRACE(...) SPDLOG_LOGGER_TRACE(::lunalite::core::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_DEBUG(...) SPDLOG_LOGGER_DEBUG(::lunalite::core::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_INFO(...)  SPDLOG_LOGGER_INFO(::lunalite::core::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_WARN(...)  SPDLOG_LOGGER_WARN(::lunalite::core::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_ERROR(...) SPDLOG_LOGGER_ERROR(::lunalite::core::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_FATAL(...) SPDLOG_LOGGER_CRITICAL(::lunalite::core::Logger::core().get(), __VA_ARGS__)

#define LUNA_RHI_TRACE(...) SPDLOG_LOGGER_TRACE(::lunalite::core::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_DEBUG(...) SPDLOG_LOGGER_DEBUG(::lunalite::core::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_INFO(...)  SPDLOG_LOGGER_INFO(::lunalite::core::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_WARN(...)  SPDLOG_LOGGER_WARN(::lunalite::core::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_ERROR(...) SPDLOG_LOGGER_ERROR(::lunalite::core::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_FATAL(...) SPDLOG_LOGGER_CRITICAL(::lunalite::core::Logger::rhi().get(), __VA_ARGS__)

#ifndef LUNA_ENABLE_ASSERTS
#ifdef NDEBUG
#define LUNA_ENABLE_ASSERTS 0
#else
#define LUNA_ENABLE_ASSERTS 1
#endif
#endif

#if LUNA_ENABLE_ASSERTS
#define LUNA_ASSERT(condition, ...)                              \
    do {                                                         \
        if (!(condition)) {                                      \
            LUNA_CORE_FATAL("Assertion failed: {}", #condition); \
            __VA_OPT__(LUNA_CORE_FATAL(__VA_ARGS__);)            \
            std::abort();                                        \
        }                                                        \
    } while (false)
#else
#define LUNA_ASSERT(condition, ...) ((void) 0)
#endif
