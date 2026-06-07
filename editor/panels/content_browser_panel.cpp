#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/factory/asset_factory_manager.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/project/project_manager.h"
#include "content_browser_panel.h"

#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>

namespace lunalite::editor {
namespace {
struct PendingFactoryRequest {
    std::string factory_id;
    asset::AssetHandle source{0};
    std::filesystem::path target_directory;
};
} // namespace

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

    std::optional<PendingFactoryRequest> pendingFactoryRequest;
    for (const auto& [_, metadata] : asset::AssetManager::get().getMetadataRegistry()) {
        if (!metadata.Handle.isValid() || metadata.Type == asset::AssetType::None || metadata.MemoryOnly) {
            continue;
        }

        const auto name = metadata.Name.empty() ? metadata.FilePath.filename().string() : metadata.Name;
        const auto label = name + " (" + asset::assetTypeToString(metadata.Type) + ")";
        const auto id = metadata.Handle.toString();
        ImGui::PushID(id.c_str());
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

        const asset::AssetFactoryContext factoryContext{
            .source = &metadata,
            .target_directory = metadata.FilePath.parent_path(),
        };
        const auto factories = asset::AssetFactoryManager::get().factoriesFor(factoryContext);
        if (!factories.empty() && ImGui::BeginPopupContextItem()) {
            for (const auto* factory : factories) {
                const auto factoryLabel = std::string{factory->label()};
                if (ImGui::MenuItem(factoryLabel.c_str())) {
                    pendingFactoryRequest = PendingFactoryRequest{
                        .factory_id = std::string{factory->id()},
                        .source = metadata.Handle,
                        .target_directory = metadata.FilePath.parent_path(),
                    };
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (pendingFactoryRequest) {
        const auto* source = asset::AssetManager::get().getMetadata(pendingFactoryRequest->source);
        if (source == nullptr) {
            LUNA_CORE_ERROR("Failed to run asset factory '{}': source asset is missing",
                            pendingFactoryRequest->factory_id);
        } else {
            const asset::AssetFactoryContext context{
                .source = source,
                .target_directory = pendingFactoryRequest->target_directory,
            };
            const auto result = asset::AssetFactoryManager::get().create(pendingFactoryRequest->factory_id, context);
            if (!result.handle.isValid() && !result.error.empty()) {
                LUNA_CORE_ERROR(
                    "Failed to run asset factory '{}': {}", pendingFactoryRequest->factory_id, result.error);
            }
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
