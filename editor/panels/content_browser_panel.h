#pragma once

#include "../../LunaLiteTooling/context/selection_context.h"

namespace lunalite::editor {

class ContentBrowserPanel {
public:
    explicit ContentBrowserPanel(tooling::SelectionContext& selection);

    void onImGuiRender();

private:
    tooling::SelectionContext& m_selection;
};

} // namespace lunalite::editor
