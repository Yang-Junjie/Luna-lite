#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/factory/asset_factory_manager.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/project/project_manager.h"
#include "../../LunaLiteTooling/commands/asset_commands.h"
#include "../../LunaLiteTooling/commands/command_registry.h"
#include "../../LunaLiteTooling/context/tool_context.h"
#include "content_browser_panel.h"

#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>

namespace lunalite::editor {
namespace {
struct PendingCommandRequest {
    std::string command_id;
    asset::AssetHandle source{0};
    std::filesystem::path target_directory;
};
} // namespace

ContentBrowserPanel::ContentBrowserPanel(tooling::SelectionContext& selection)
    : m_selection(selection)
{}

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

    std::optional<PendingCommandRequest> pendingCommandRequest;
    for (const auto& [_, metadata] : asset::AssetManager::get().getMetadataRegistry()) {
        if (!metadata.Handle.isValid() || metadata.Type == asset::AssetType::None || metadata.MemoryOnly) {
            continue;
        }

        const auto name = metadata.Name.empty() ? metadata.FilePath.filename().string() : metadata.Name;
        const auto label = name + " (" + asset::assetTypeToString(metadata.Type) + ")";
        const auto id = metadata.Handle.toString();
        ImGui::PushID(id.c_str());
        const bool selected = m_selection.isAsset() && m_selection.selectedAsset() == metadata.Handle;
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selection.selectAsset(metadata.Handle);
        }

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
                const auto commandId = tooling::commandIdForAssetFactory(factory->id());
                if (!commandId) {
                    continue;
                }

                const auto factoryLabel = std::string{factory->label()};
                if (ImGui::MenuItem(factoryLabel.c_str())) {
                    pendingCommandRequest = PendingCommandRequest{
                        .command_id = std::string{*commandId},
                        .source = metadata.Handle,
                        .target_directory = metadata.FilePath.parent_path(),
                    };
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (pendingCommandRequest) {
        tooling::ToolContext context;
        tooling::CommandArgs args;
        args.set("source_asset", pendingCommandRequest->source);
        args.set("target_directory", pendingCommandRequest->target_directory);

        const auto result =
            tooling::CommandRegistry::get().execute(pendingCommandRequest->command_id, context, args);
        if (!result.success) {
            LUNA_CORE_ERROR("Failed to execute command '{}': {}",
                            pendingCommandRequest->command_id,
                            result.message.empty() ? "unknown error" : result.message);
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
