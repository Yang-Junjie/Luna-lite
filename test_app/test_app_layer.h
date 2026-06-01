#pragma once
#include "../LunaLite/core/layer.h"

namespace lunalite::test_app {
class TestAppLayer final : public core::Layer {
public:
    TestAppLayer();

    void onRender() override;
};
} // namespace lunalite::test_app
