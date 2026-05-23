#pragma once
#include "window.h"

#include <string>

namespace lunalite::core {

struct ApplicationCreateInfo {
    std::string name{"Luna-Lite"};
    uint32_t width{800};
    uint32_t height{600};
};

class Application {
public:
    explicit Application(const ApplicationCreateInfo& info);
    ~Application();

    void run();

private:
    bool m_is_running{true};
    std::unique_ptr<Window> m_window;
};
} // namespace lunalite::core
