#include "content_browser_panel.h"

#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/project/project_manager.h"

#include <imgui.h>

namespace lunalite::editor {

void ContentBrowserPanel::onImGuiRender()
{
    ImGui::Begin("Content Browser");

    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot) {
        ImGui::TextUnformatted("No project loaded");
        ImGui::End();
        return;
    }

    const auto projectRootText = projectRoot->string();
    ImGui::TextUnformatted(projectRootText.c_str());
    ImGui::Separator();

    for (const auto& [_, metadata] : asset::AssetManager::get().getMetadataRegistry()) {
        if (!metadata.Handle.isValid() || metadata.Type == asset::AssetType::None) {
            continue;
        }

        const auto name = metadata.Name.empty() ? metadata.FilePath.filename().string() : metadata.Name;
        const auto label = name + " (" + asset::assetTypeToString(metadata.Type) + ")";
        ImGui::Selectable(label.c_str());

        if (ImGui::BeginDragDropSource()) {
            const AssetDragDropPayload payload{
                .handle = metadata.Handle,
                .type = metadata.Type,
            };
            ImGui::SetDragDropPayload(AssetDragDropPayloadName, &payload, sizeof(payload));
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
