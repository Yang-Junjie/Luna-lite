
#include "uuid.h"

#include <random>

namespace lunalite::core {

// 全局单例随机数生成器
static std::mt19937_64& getRng()
{
    static std::random_device rd;
    static std::mt19937_64 rng(rd());
    return rng;
}

UUID::UUID()
{
    auto& rng = getRng();
    do {
        m_UUID = rng();
    } while (m_UUID == 0);
}

UUID::UUID(uint64_t uuid)
    : m_UUID(uuid)
{}

} // namespace lunalite::core
