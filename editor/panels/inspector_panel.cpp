#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/builtin/builtin_assets.h"
#include "../../LunaLite/renderer/interface/mesh.h"
#include "../../LunaLite/scene/components.h"
#include "../editor_actions.h"
#include "content_browser_panel.h"
#include "inspector_panel.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace lunalite::editor {
namespace {
struct BuiltinAssetOption {
    const char* label{nullptr};
    asset::AssetHandle handle{0};
};

template <typename DrawFn, typename ApplyFn>
bool drawLiveSceneEdit(scene::Scene& scene, std::string_view commandId, DrawFn&& draw, ApplyFn&& apply)
{
    const bool changed = draw();
    const bool activated = ImGui::IsItemActivated();
    const bool active = ImGui::IsItemActive();
    const bool deactivated = ImGui::IsItemDeactivated();

    if (activated && (active || changed)) {
        actions::beginSceneEdit(scene, commandId);
    } else if (changed && !actions::hasActiveSceneEdit()) {
        actions::beginSceneEdit(scene, commandId);
    }

    if (changed) {
        apply();
    }

    if ((deactivated || (changed && !active)) && actions::hasActiveSceneEdit()) {
        actions::commitSceneEdit(scene);
    }

    return changed;
}

std::string getAssetDisplayName(asset::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "None";
    }

    if (const auto* metadata = asset::AssetManager::get().getMetadata(handle)) {
        const auto name = metadata->Name.empty() ? metadata->FilePath.filename().string() : metadata->Name;
        return name + " (" + asset::assetTypeToString(metadata->Type) + ")";
    }

    return "Missing asset " + handle.toString();
}

bool acceptAssetHandleDrop(asset::AssetType type, asset::AssetHandle& handle)
{
    bool accepted = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadName)) {
            if (payload->DataSize == sizeof(AssetDragDropPayload)) {
                const auto& assetPayload = *static_cast<const AssetDragDropPayload*>(payload->Data);
                if (assetPayload.type == type && assetPayload.handle.isValid()) {
                    handle = assetPayload.handle;
                    accepted = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
}

bool drawAssetHandleControl(const char* label,
                            scene::Scene& scene,
                            std::string_view commandId,
                            asset::AssetType type,
                            asset::AssetHandle& handle,
                            std::span<const BuiltinAssetOption> builtinOptions)
{
    bool changed = false;
    ImGui::PushID(label);

    uint64_t rawHandle = static_cast<uint64_t>(handle);
    changed |= drawLiveSceneEdit(
        scene,
        commandId,
        [&]() {
            return ImGui::InputScalar(label, ImGuiDataType_U64, &rawHandle);
        },
        [&]() {
            handle = asset::AssetHandle{rawHandle};
        });

    auto droppedHandle = handle;
    if (acceptAssetHandleDrop(type, droppedHandle)) {
        if (actions::beginSceneEdit(scene, commandId)) {
            handle = droppedHandle;
            actions::commitSceneEdit(scene);
        }
        changed = true;
    }

    if (!builtinOptions.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Builtin")) {
            ImGui::OpenPopup("BuiltinAssets");
        }

        if (ImGui::BeginPopup("BuiltinAssets")) {
            for (const auto& option : builtinOptions) {
                if (ImGui::MenuItem(option.label)) {
                    if (actions::beginSceneEdit(scene, commandId)) {
                        handle = option.handle;
                        actions::commitSceneEdit(scene);
                    }
                    changed = true;
                }
            }
            ImGui::EndPopup();
        }
    }

    const auto displayName = getAssetDisplayName(handle);
    ImGui::TextDisabled("%s", displayName.c_str());
    ImGui::PopID();
    return changed;
}

float quatLengthSquared(const glm::quat& rotation)
{
    return rotation.w * rotation.w + rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z;
}

glm::quat safeNormalize(glm::quat rotation)
{
    if (quatLengthSquared(rotation) <= 0.000001f) {
        return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    }

    return glm::normalize(rotation);
}

bool sameOrientation(const glm::quat& lhs, const glm::quat& rhs)
{
    const auto a = safeNormalize(lhs);
    const auto b = safeNormalize(rhs);
    return std::abs(glm::dot(a, b)) > 0.99999f;
}

bool drawShadowMapSizeControl(uint32_t& map_size)
{
    static constexpr std::array<uint32_t, 5> mapSizeOptions{512u, 1'024u, 2'048u, 4'096u, 8'192u};

    bool changed = false;
    const auto preview = std::to_string(map_size);
    if (ImGui::BeginCombo("Map Size", preview.c_str())) {
        for (const auto option : mapSizeOptions) {
            const bool selected = map_size == option;
            const auto label = std::to_string(option);
            if (ImGui::Selectable(label.c_str(), selected)) {
                map_size = option;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}
} // namespace

InspectorPanel::InspectorPanel(scene::Scene& scene, tooling::SelectionContext& selection)
    : m_scene(scene),
      m_selection(selection)
{}

void InspectorPanel::onImGuiRender()
{
    ImGui::Begin("Inspector");

    if (!m_selection.isEntity() || !m_scene.isValidEntity(m_selection.selectedEntity())) {
        ImGui::TextUnformatted("No entity selected");
        ImGui::End();
        return;
    }

    const auto selectedEntity = m_selection.selectedEntity();

    ImGui::Text("Entity %u", static_cast<uint32_t>(selectedEntity.getHandle()));
    ImGui::Separator();

    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponent");
    }

    if (ImGui::BeginPopup("AddComponent")) {
        if (!m_scene.hasComponent<scene::MeshRendererComponent>(selectedEntity) && ImGui::MenuItem("Mesh Renderer")) {
            actions::addComponent(m_scene, selectedEntity, tooling::MeshRendererComponentType);
        }

        if (!m_scene.hasComponent<scene::SpriteRendererComponent>(selectedEntity) &&
            ImGui::MenuItem("Sprite Renderer")) {
            actions::addComponent(m_scene, selectedEntity, tooling::SpriteRendererComponentType);
        }

        if (!m_scene.hasComponent<scene::ScriptComponent>(selectedEntity) && ImGui::MenuItem("Script")) {
            actions::addComponent(m_scene, selectedEntity, tooling::ScriptComponentType);
        }

        if (!m_scene.hasComponent<scene::CameraComponent>(selectedEntity) && ImGui::MenuItem("Camera")) {
            actions::addComponent(m_scene, selectedEntity, tooling::CameraComponentType);
        }

        if (!m_scene.hasComponent<scene::DirectionalLightComponent>(selectedEntity) &&
            ImGui::MenuItem("Directional Light")) {
            actions::addComponent(m_scene, selectedEntity, tooling::DirectionalLightComponentType);
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    if (m_scene.hasComponent<scene::TagComponent>(selectedEntity)) {
        const bool open = ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen);
        if (open && m_scene.hasComponent<scene::TagComponent>(selectedEntity)) {
            auto& tag = m_scene.getComponent<scene::TagComponent>(selectedEntity);
            std::array<char, 256> buffer{};
            const size_t copySize = tag.tag.size() < buffer.size() - 1 ? tag.tag.size() : buffer.size() - 1;
            std::memcpy(buffer.data(), tag.tag.data(), copySize);
            drawLiveSceneEdit(
                m_scene,
                actions::EditTagCommandId,
                [&]() {
                    return ImGui::InputText("Name", buffer.data(), buffer.size());
                },
                [&]() {
                    tag.tag = buffer.data();
                });
        }
    }

    if (m_scene.hasComponent<scene::TransformComponent>(selectedEntity)) {
        const bool open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
        if (open && m_scene.hasComponent<scene::TransformComponent>(selectedEntity)) {
            auto& transform = m_scene.getComponent<scene::TransformComponent>(selectedEntity);
            auto translation = transform.translation;
            drawLiveSceneEdit(
                m_scene,
                actions::EditTransformCommandId,
                [&]() {
                    return ImGui::DragFloat3("Translation", &translation.x, 0.1f);
                },
                [&]() {
                    transform.translation = translation;
                });
            syncRotationEditor(selectedEntity, transform.rotation);
            auto rotation = m_rotation_edit_degrees;
            drawLiveSceneEdit(
                m_scene,
                actions::EditTransformCommandId,
                [&]() {
                    return ImGui::DragFloat3("Rotation", &rotation.x, 1.0f);
                },
                [&]() {
                    const auto delta = rotation - m_rotation_edit_degrees;
                    transform.rotation = safeNormalize(transform.rotation * glm::quat{glm::radians(delta)});
                    m_rotation_edit_degrees = rotation;
                    m_rotation_edit_source = transform.rotation;
                });
            auto scale = transform.scale;
            drawLiveSceneEdit(
                m_scene,
                actions::EditTransformCommandId,
                [&]() {
                    return ImGui::DragFloat3("Scale", &scale.x, 0.1f);
                },
                [&]() {
                    transform.scale = scale;
                });
        }
    }

    if (m_scene.hasComponent<scene::MeshRendererComponent>(selectedEntity)) {
        const bool open = ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("MeshRendererPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                actions::removeComponent(m_scene, selectedEntity, tooling::MeshRendererComponentType);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::MeshRendererComponent>(selectedEntity)) {
            auto& meshRenderer = m_scene.getComponent<scene::MeshRendererComponent>(selectedEntity);
            const BuiltinAssetOption builtinMeshes[] = {
                {"Cube", asset::builtin::cubeMeshHandle()},
                {"Plane", asset::builtin::planeMeshHandle()},
            };
            const BuiltinAssetOption builtinMaterials[] = {
                {"Default", asset::builtin::defaultMaterialHandle()},
                {"Error", asset::builtin::errorMaterialHandle()},
            };

            drawAssetHandleControl("Mesh",
                                   m_scene,
                                   actions::EditMeshRendererCommandId,
                                   asset::AssetType::Mesh,
                                   meshRenderer.mesh,
                                   builtinMeshes);

            auto* meshAsset = meshRenderer.mesh.isValid()
                                  ? asset::AssetManager::get().getAsset<renderer::interface::Mesh>(meshRenderer.mesh)
                                  : nullptr;
            if (meshAsset == nullptr) {
                ImGui::TextDisabled("No mesh asset loaded");
            } else {
                ImGui::TextDisabled("Submeshes: %zu", meshAsset->getSubMeshes().size());
            }

            auto castShadow = meshRenderer.cast_shadow;
            drawLiveSceneEdit(
                m_scene,
                actions::EditMeshRendererCommandId,
                [&]() {
                    return ImGui::Checkbox("Cast Shadow", &castShadow);
                },
                [&]() {
                    meshRenderer.cast_shadow = castShadow;
                });

            if (ImGui::Button("Add Material")) {
                actions::addMaterialSlot(m_scene, selectedEntity, asset::builtin::defaultMaterialHandle());
            }

            int materialToDelete = -1;
            for (size_t materialIndex = 0; materialIndex < meshRenderer.materials.size(); ++materialIndex) {
                ImGui::PushID(static_cast<int>(materialIndex));
                ImGui::Text("Material %zu", materialIndex);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    materialToDelete = static_cast<int>(materialIndex);
                }
                drawAssetHandleControl("Material",
                                       m_scene,
                                       actions::EditMeshRendererCommandId,
                                       asset::AssetType::Material,
                                       meshRenderer.materials[materialIndex],
                                       builtinMaterials);
                ImGui::PopID();
            }

            if (materialToDelete >= 0) {
                actions::removeMaterialSlot(m_scene, selectedEntity, static_cast<size_t>(materialToDelete));
            }
        }
    }

    if (m_scene.hasComponent<scene::SpriteRendererComponent>(selectedEntity)) {
        const bool open = ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("SpriteRendererPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                actions::removeComponent(m_scene, selectedEntity, tooling::SpriteRendererComponentType);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::SpriteRendererComponent>(selectedEntity)) {
            auto& spriteRenderer = m_scene.getComponent<scene::SpriteRendererComponent>(selectedEntity);
            drawAssetHandleControl("Sprite",
                                   m_scene,
                                   actions::EditSpriteRendererCommandId,
                                   asset::AssetType::Sprite,
                                   spriteRenderer.sprite,
                                   {});
            auto spriteColor = spriteRenderer.color;
            drawLiveSceneEdit(
                m_scene,
                actions::EditSpriteRendererCommandId,
                [&]() {
                    return ImGui::ColorEdit4("Color", &spriteColor.x);
                },
                [&]() {
                    spriteRenderer.color = spriteColor;
                });

            int sortingLayer = spriteRenderer.sorting_layer;
            drawLiveSceneEdit(
                m_scene,
                actions::EditSpriteRendererCommandId,
                [&]() {
                    return ImGui::DragInt("Sorting Layer", &sortingLayer, 1.0f);
                },
                [&]() {
                    spriteRenderer.sorting_layer = sortingLayer;
                });

            int orderInLayer = spriteRenderer.order_in_layer;
            drawLiveSceneEdit(
                m_scene,
                actions::EditSpriteRendererCommandId,
                [&]() {
                    return ImGui::DragInt("Order In Layer", &orderInLayer, 1.0f);
                },
                [&]() {
                    spriteRenderer.order_in_layer = orderInLayer;
                });

            auto depthTest = spriteRenderer.depth_test;
            drawLiveSceneEdit(
                m_scene,
                actions::EditSpriteRendererCommandId,
                [&]() {
                    return ImGui::Checkbox("Depth Test", &depthTest);
                },
                [&]() {
                    spriteRenderer.depth_test = depthTest;
                });
        }
    }

    if (m_scene.hasComponent<scene::ScriptComponent>(selectedEntity)) {
        const bool open = ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("ScriptPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                actions::removeComponent(m_scene, selectedEntity, tooling::ScriptComponentType);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::ScriptComponent>(selectedEntity)) {
            auto& script = m_scene.getComponent<scene::ScriptComponent>(selectedEntity);
            if (ImGui::Button("Add Script")) {
                actions::addScriptBinding(m_scene, selectedEntity);
            }

            int scriptToDelete = -1;
            for (size_t i = 0; i < script.scripts.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::Separator();
                ImGui::Text("Script %zu", i);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    scriptToDelete = static_cast<int>(i);
                }

                auto& binding = script.scripts[i];
                auto enabled = binding.enabled;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditScriptCommandId,
                    [&]() {
                        return ImGui::Checkbox("Enabled", &enabled);
                    },
                    [&]() {
                        binding.enabled = enabled;
                    });
                uint64_t scriptHandle = static_cast<uint64_t>(binding.script);
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditScriptCommandId,
                    [&]() {
                        return ImGui::InputScalar("Handle", ImGuiDataType_U64, &scriptHandle);
                    },
                    [&]() {
                        binding.script = asset::AssetHandle{scriptHandle};
                    });
                auto droppedScript = binding.script;
                if (acceptAssetHandleDrop(asset::AssetType::Script, droppedScript)) {
                    if (actions::beginSceneEdit(m_scene, actions::EditScriptCommandId)) {
                        binding.script = droppedScript;
                        actions::commitSceneEdit(m_scene);
                    }
                }
                ImGui::PopID();
            }

            if (scriptToDelete >= 0) {
                actions::removeScriptBinding(m_scene, selectedEntity, static_cast<size_t>(scriptToDelete));
            }
        }
    }

    if (m_scene.hasComponent<scene::CameraComponent>(selectedEntity)) {
        const bool open = ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("CameraPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                actions::removeComponent(m_scene, selectedEntity, tooling::CameraComponentType);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::CameraComponent>(selectedEntity)) {
            auto& camera = m_scene.getComponent<scene::CameraComponent>(selectedEntity);
            auto primary = camera.primary;
            drawLiveSceneEdit(
                m_scene,
                actions::EditCameraCommandId,
                [&]() {
                    return ImGui::Checkbox("Primary", &primary);
                },
                [&]() {
                    camera.primary = primary;
                });
            float exposure = camera.camera.getExposure();
            drawLiveSceneEdit(
                m_scene,
                actions::EditCameraCommandId,
                [&]() {
                    return ImGui::DragFloat("Exposure", &exposure, 0.05f, 0.0f, 64.0f, "%.3f");
                },
                [&]() {
                    camera.camera.setExposure(exposure);
                });

            using ProjectionType = renderer::interface::Camera::ProjectionType;
            int projectionType = camera.camera.getProjectionType() == ProjectionType::Orthographic ? 1 : 0;
            const char* projectionTypes[] = {"Perspective", "Orthographic"};
            drawLiveSceneEdit(
                m_scene,
                actions::EditCameraCommandId,
                [&]() {
                    return ImGui::Combo("Projection", &projectionType, projectionTypes, 2);
                },
                [&]() {
                    if (projectionType == 1) {
                        camera.camera.setOrthographic(10.0f, 0.1f, 100.0f);
                    } else {
                        camera.camera.setPerspective(glm::radians(45.0f), 0.1f, 100.0f);
                    }
                });
        }
    }

    if (m_scene.hasComponent<scene::DirectionalLightComponent>(selectedEntity)) {
        const bool open = ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("DirectionalLightPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                actions::removeComponent(m_scene, selectedEntity, tooling::DirectionalLightComponentType);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::DirectionalLightComponent>(selectedEntity)) {
            auto& light = m_scene.getComponent<scene::DirectionalLightComponent>(selectedEntity);
            auto lightColor = light.color;
            drawLiveSceneEdit(
                m_scene,
                actions::EditDirectionalLightCommandId,
                [&]() {
                    return ImGui::ColorEdit3("Color", &lightColor.x);
                },
                [&]() {
                    light.color = lightColor;
                });
            auto intensity = light.intensity;
            drawLiveSceneEdit(
                m_scene,
                actions::EditDirectionalLightCommandId,
                [&]() {
                    return ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 1000.0f, "%.3f");
                },
                [&]() {
                    light.intensity = intensity;
                });

            if (ImGui::TreeNodeEx("Shadow", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto shadowEnabled = light.shadow.enabled;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::Checkbox("Enabled", &shadowEnabled);
                    },
                    [&]() {
                        light.shadow.enabled = shadowEnabled;
                    });

                auto mapSize = light.shadow.map_size;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return drawShadowMapSizeControl(mapSize);
                    },
                    [&]() {
                        light.shadow.map_size = std::max(mapSize, 1u);
                    });

                auto maxDistance = light.shadow.max_distance;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::DragFloat("Max Distance", &maxDistance, 0.5f, 0.0f, 10000.0f, "%.2f");
                    },
                    [&]() {
                        light.shadow.max_distance = std::max(maxDistance, 0.0f);
                    });

                auto bias = light.shadow.bias;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::DragFloat("Bias", &bias, 0.0001f, 0.0f, 0.1f, "%.6f");
                    },
                    [&]() {
                        light.shadow.bias = std::max(bias, 0.0f);
                    });

                auto normalBias = light.shadow.normal_bias;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::DragFloat("Normal Bias", &normalBias, 0.001f, 0.0f, 1.0f, "%.4f");
                    },
                    [&]() {
                        light.shadow.normal_bias = std::max(normalBias, 0.0f);
                    });

                int pcfRadius = static_cast<int>(light.shadow.pcf_radius);
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::SliderInt("PCF Radius", &pcfRadius, 0, 4);
                    },
                    [&]() {
                        light.shadow.pcf_radius = static_cast<uint32_t>(std::clamp(pcfRadius, 0, 4));
                    });

                int cascadeCount = static_cast<int>(light.shadow.cascade_count);
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::SliderInt("Cascade Count", &cascadeCount, 1, 4);
                    },
                    [&]() {
                        light.shadow.cascade_count = static_cast<uint32_t>(std::clamp(cascadeCount, 1, 4));
                    });

                auto cascadeSplitLambda = light.shadow.cascade_split_lambda;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::DragFloat("Cascade Split Lambda", &cascadeSplitLambda, 0.01f, 0.0f, 1.0f, "%.3f");
                    },
                    [&]() {
                        light.shadow.cascade_split_lambda = std::clamp(cascadeSplitLambda, 0.0f, 1.0f);
                    });

                auto cascadeSeamBlend = light.shadow.cascade_seam_blend;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::DragFloat("Cascade Seam Blend", &cascadeSeamBlend, 0.1f, 0.0f, 1000.0f, "%.2f");
                    },
                    [&]() {
                        light.shadow.cascade_seam_blend = std::max(cascadeSeamBlend, 0.0f);
                    });

                auto cascadeCasterDepthPadding = light.shadow.cascade_caster_depth_padding;
                drawLiveSceneEdit(
                    m_scene,
                    actions::EditDirectionalLightCommandId,
                    [&]() {
                        return ImGui::DragFloat(
                            "Cascade Caster Depth Padding", &cascadeCasterDepthPadding, 0.5f, 0.0f, 10000.0f, "%.2f");
                    },
                    [&]() {
                        light.shadow.cascade_caster_depth_padding = std::max(cascadeCasterDepthPadding, 0.0f);
                    });

                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

void InspectorPanel::syncRotationEditor(scene::Entity entity, const glm::quat& rotation)
{
    const auto normalizedRotation = safeNormalize(rotation);
    if (m_rotation_edit_entity.getHandle() == entity.getHandle() &&
        sameOrientation(m_rotation_edit_source, normalizedRotation)) {
        return;
    }

    m_rotation_edit_entity = entity;
    m_rotation_edit_source = normalizedRotation;
    m_rotation_edit_degrees = glm::degrees(glm::eulerAngles(normalizedRotation));
}

} // namespace lunalite::editor
