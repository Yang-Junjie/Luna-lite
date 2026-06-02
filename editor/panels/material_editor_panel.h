#pragma once

#include "../../LunaLite/asset/asset.h"

namespace lunalite::editor {

class MaterialEditorPanel {
public:
    void onImGuiRender();

private:
    asset::AssetHandle m_material{0};
    bool m_dirty{false};
};

} // namespace lunalite::editor
