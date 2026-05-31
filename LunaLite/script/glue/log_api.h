#pragma once

#include <string>

namespace lunalite::script {
class LogAPI {
public:
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);
};
} // namespace lunalite::script
