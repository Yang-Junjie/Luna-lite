#include "log.h"

#include <exception>
#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <utility>

namespace lunalite::core {

namespace {

constexpr const char* console_pattern = "%^[%n] [%l] %v%$";
constexpr const char* file_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v";

} // namespace

void Logger::init(const std::string& log_file, Level level)
{
    shutdown();

    m_s_log_file = log_file;
    m_s_level = level;

    try {
        m_s_sinks.clear();

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(console_pattern);
        m_s_sinks.push_back(console_sink);

        if (!m_s_log_file.empty()) {
            const std::filesystem::path log_path{m_s_log_file};
            if (log_path.has_parent_path()) {
                std::filesystem::create_directories(log_path.parent_path());
            }

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
            file_sink->set_pattern(file_pattern);
            m_s_sinks.push_back(std::move(file_sink));
        }

        const auto spdlog_level = toSpdlogLevel(level);
        for (const auto& sink : m_s_sinks) {
            sink->set_level(spdlog_level);
        }

        m_s_core_logger = std::make_shared<spdlog::logger>("LunaCore", m_s_sinks.begin(), m_s_sinks.end());
        m_s_rhi_logger = std::make_shared<spdlog::logger>("LunaRHI", m_s_sinks.begin(), m_s_sinks.end());

        configureLogger(m_s_core_logger, spdlog_level);
        configureLogger(m_s_rhi_logger, spdlog_level);

        spdlog::register_logger(m_s_core_logger);
        spdlog::register_logger(m_s_rhi_logger);

        m_s_initialized = true;

        if (m_s_log_file.empty()) {
            m_s_core_logger->info("Logger initialized (console only)");
        } else {
            m_s_core_logger->info("Logger initialized, output file: {}", m_s_log_file);
        }
    } catch (const std::exception& ex) {
        try {
            if (m_s_core_logger) {
                m_s_core_logger->error("Logger initialization failed: {}", ex.what());
                m_s_core_logger->flush();
            } else if (!m_s_sinks.empty()) {
                auto fallback_logger =
                    std::make_shared<spdlog::logger>("LunaCoreBootstrap", m_s_sinks.begin(), m_s_sinks.end());
                configureLogger(fallback_logger, toSpdlogLevel(level));
                fallback_logger->error("Logger initialization failed: {}", ex.what());
                fallback_logger->flush();
            }
        } catch (...) {
        }

        m_s_initialized = false;
        m_s_sinks.clear();
        m_s_core_logger.reset();
        m_s_rhi_logger.reset();
    }
}

void Logger::shutdown()
{
    if (!m_s_initialized) {
        return;
    }

    if (m_s_core_logger) {
        m_s_core_logger->info("Logger shutdown");
        m_s_core_logger->flush();
    }

    if (m_s_rhi_logger) {
        m_s_rhi_logger->flush();
    }

    spdlog::drop("LunaCore");
    spdlog::drop("LunaRHI");

    m_s_core_logger.reset();
    m_s_rhi_logger.reset();
    m_s_sinks.clear();
    m_s_initialized = false;
}

bool Logger::isInitialized()
{
    return m_s_initialized;
}

void Logger::setLevel(Level level)
{
    m_s_level = level;
    if (!m_s_initialized) {
        return;
    }

    const auto spdlog_level = toSpdlogLevel(level);

    for (const auto& sink : m_s_sinks) {
        sink->set_level(spdlog_level);
    }

    configureLogger(m_s_core_logger, spdlog_level);
    configureLogger(m_s_rhi_logger, spdlog_level);
}

Logger::Level Logger::getLevel()
{
    return m_s_level;
}

const std::shared_ptr<spdlog::logger>& Logger::get(Type type)
{
    ensureInitialized();
    switch (type) {
        case Type::Core:
            return m_s_core_logger;
        case Type::RHI:
            return m_s_rhi_logger;
        default:
            return m_s_core_logger;
    }
}

const std::shared_ptr<spdlog::logger>& Logger::core()
{
    return get(Type::Core);
}

const std::shared_ptr<spdlog::logger>& Logger::rhi()
{
    return get(Type::RHI);
}

void Logger::flush()
{
    if (!m_s_initialized) {
        return;
    }

    if (m_s_core_logger) {
        m_s_core_logger->flush();
    }
    if (m_s_rhi_logger) {
        m_s_rhi_logger->flush();
    }
}

void Logger::ensureInitialized()
{
    if (!m_s_initialized) {
        init(m_s_log_file, m_s_level);
    }
}

void Logger::configureLogger(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level)
{
    if (!logger) {
        return;
    }

    logger->set_level(level);
    logger->flush_on(spdlog::level::err);
}

spdlog::level::level_enum Logger::toSpdlogLevel(Level level)
{
    switch (level) {
        case Level::Trace:
            return spdlog::level::trace;
        case Level::Debug:
            return spdlog::level::debug;
        case Level::Info:
            return spdlog::level::info;
        case Level::Warn:
            return spdlog::level::warn;
        case Level::Error:
            return spdlog::level::err;
        case Level::Fatal:
            return spdlog::level::critical;
        case Level::Off:
            return spdlog::level::off;
        default:
            return spdlog::level::info;
    }
}

} // namespace lunalite::core
