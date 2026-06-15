#include "drag_drop.h"

#include <imgui.h>

namespace lunalite::editor::drag_drop {
namespace {
template <typename Payload> std::optional<Payload> acceptPayload(const char* payloadName)
{
    std::optional<Payload> accepted;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadName)) {
            if (payload->DataSize == sizeof(Payload)) {
                accepted = *static_cast<const Payload*>(payload->Data);
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
}
} // namespace

void setAssetPayload(const AssetPayload& payload, const char* previewText)
{
    if (!ImGui::BeginDragDropSource()) {
        return;
    }

    ImGui::SetDragDropPayload(AssetPayloadName, &payload, sizeof(payload));
    if (previewText != nullptr) {
        ImGui::TextUnformatted(previewText);
    }
    ImGui::EndDragDropSource();
}

void setEntityPayload(const EntityPayload& payload, const char* previewText)
{
    if (!ImGui::BeginDragDropSource()) {
        return;
    }

    ImGui::SetDragDropPayload(EntityPayloadName, &payload, sizeof(payload));
    if (previewText != nullptr) {
        ImGui::TextUnformatted(previewText);
    }
    ImGui::EndDragDropSource();
}

std::optional<AssetPayload> acceptAssetPayload()
{
    return acceptPayload<AssetPayload>(AssetPayloadName);
}

std::optional<EntityPayload> acceptEntityPayload()
{
    return acceptPayload<EntityPayload>(EntityPayloadName);
}

bool acceptAssetHandle(asset::AssetType type, asset::AssetHandle& handle)
{
    const auto payload = acceptAssetPayload();
    if (!payload || payload->type != type || !payload->handle.isValid()) {
        return false;
    }

    handle = payload->handle;
    return true;
}

} // namespace lunalite::editor::drag_drop
