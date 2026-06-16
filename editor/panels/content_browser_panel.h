#pragma once

#include "../../LunaLiteTooling/context/selection_context.h"

#include <filesystem>
#include <memory>
#include <string>

namespace lunalite::editor {

class ContentBrowserPanel {
public:
    explicit ContentBrowserPanel(tooling::SelectionContext& selection);
    ~ContentBrowserPanel();

    void onImGuiRender();

private:
    class CreateFolderModal;

    void startCreateFolder(std::filesystem::path target_directory);

    tooling::SelectionContext& m_selection;
    std::filesystem::path m_current_directory;
    std::unique_ptr<CreateFolderModal> m_create_folder_modal;
};

} // namespace lunalite::editor
