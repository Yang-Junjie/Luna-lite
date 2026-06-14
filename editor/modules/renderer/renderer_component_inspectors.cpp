#include "../../../LunaLite/asset/asset_manager.h"
#include "../../../LunaLite/asset/builtin/builtin_assets.h"
#include "../../../LunaLite/renderer/default_renderer/components/renderer_component_registry.h"
#include "../../../LunaLite/renderer/default_renderer/components/renderer_components.h"
#include "../../../LunaLite/renderer/interface/mesh.h"
#include "../../../LunaLite/scene/component_registry.h"
#include "../../../LunaLiteTooling/commands/scene_commands.h"
#include "../../component_inspector_widgets.h"
#include "../../editor_actions.h"
#include "renderer_component_inspectors.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <string>

namespace lunalite::editor {
namespace {
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

bool drawMeshRendererInspector(scene::Scene& scene, scene::Entity selectedEntity)
{
    if (!scene.hasComponent<scene::MeshRendererComponent>(selectedEntity)) {
        return false;
    }

    bool changed = false;
    auto& meshRenderer = scene.getComponent<scene::MeshRendererComponent>(selectedEntity);
    const BuiltinAssetOption builtinMeshes[] = {
        {"Cube", asset::builtin::cubeMeshHandle()},
        {"Plane", asset::builtin::planeMeshHandle()},
    };
    const BuiltinAssetOption builtinMaterials[] = {
        {"Default", asset::builtin::defaultMaterialHandle()},
        {"Error", asset::builtin::errorMaterialHandle()},
    };

    changed |= drawAssetHandleControl(
        "Mesh", scene, actions::EditMeshRendererCommandId, asset::AssetType::Mesh, meshRenderer.mesh, builtinMeshes);

    auto* meshAsset = meshRenderer.mesh.isValid()
                          ? asset::AssetManager::get().getAsset<renderer::interface::Mesh>(meshRenderer.mesh)
                          : nullptr;
    if (meshAsset == nullptr) {
        ImGui::TextDisabled("No mesh asset loaded");
    } else {
        ImGui::TextDisabled("Submeshes: %zu", meshAsset->getSubMeshes().size());
    }

    auto castShadow = meshRenderer.cast_shadow;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditMeshRendererCommandId,
        [&]() {
            return ImGui::Checkbox("Cast Shadow", &castShadow);
        },
        [&]() {
            meshRenderer.cast_shadow = castShadow;
        });

    if (ImGui::Button("Add Material")) {
        changed |= actions::addMaterialSlot(scene, selectedEntity, asset::builtin::defaultMaterialHandle());
    }

    int materialToDelete = -1;
    for (size_t materialIndex = 0; materialIndex < meshRenderer.materials.size(); ++materialIndex) {
        ImGui::PushID(static_cast<int>(materialIndex));
        ImGui::Text("Material %zu", materialIndex);
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete##MaterialSlotDelete")) {
            materialToDelete = static_cast<int>(materialIndex);
        }
        changed |= drawAssetHandleControl("Material",
                                          scene,
                                          actions::EditMeshRendererCommandId,
                                          asset::AssetType::Material,
                                          meshRenderer.materials[materialIndex],
                                          builtinMaterials);
        ImGui::PopID();
    }

    if (materialToDelete >= 0) {
        changed |= actions::removeMaterialSlot(scene, selectedEntity, static_cast<size_t>(materialToDelete));
    }

    return changed;
}

bool drawSpriteRendererInspector(scene::Scene& scene, scene::Entity selectedEntity)
{
    if (!scene.hasComponent<scene::SpriteRendererComponent>(selectedEntity)) {
        return false;
    }

    bool changed = false;
    auto& spriteRenderer = scene.getComponent<scene::SpriteRendererComponent>(selectedEntity);
    changed |= drawAssetHandleControl(
        "Sprite", scene, actions::EditSpriteRendererCommandId, asset::AssetType::Sprite, spriteRenderer.sprite, {});
    auto spriteColor = spriteRenderer.color;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditSpriteRendererCommandId,
        [&]() {
            return ImGui::ColorEdit4("Color", &spriteColor.x);
        },
        [&]() {
            spriteRenderer.color = spriteColor;
        });

    int sortingLayer = spriteRenderer.sorting_layer;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditSpriteRendererCommandId,
        [&]() {
            return ImGui::DragInt("Sorting Layer", &sortingLayer, 1.0f);
        },
        [&]() {
            spriteRenderer.sorting_layer = sortingLayer;
        });

    int orderInLayer = spriteRenderer.order_in_layer;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditSpriteRendererCommandId,
        [&]() {
            return ImGui::DragInt("Order In Layer", &orderInLayer, 1.0f);
        },
        [&]() {
            spriteRenderer.order_in_layer = orderInLayer;
        });

    auto depthTest = spriteRenderer.depth_test;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditSpriteRendererCommandId,
        [&]() {
            return ImGui::Checkbox("Depth Test", &depthTest);
        },
        [&]() {
            spriteRenderer.depth_test = depthTest;
        });

    return changed;
}

bool drawCameraInspector(scene::Scene& scene, scene::Entity selectedEntity)
{
    if (!scene.hasComponent<scene::CameraComponent>(selectedEntity)) {
        return false;
    }

    bool changed = false;
    auto& camera = scene.getComponent<scene::CameraComponent>(selectedEntity);
    auto primary = camera.primary;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditCameraCommandId,
        [&]() {
            return ImGui::Checkbox("Primary", &primary);
        },
        [&]() {
            camera.primary = primary;
        });
    float exposure = camera.camera.getExposure();
    changed |= drawLiveSceneEdit(
        scene,
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
    changed |= drawLiveSceneEdit(
        scene,
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

    return changed;
}

bool drawDirectionalLightInspector(scene::Scene& scene, scene::Entity selectedEntity)
{
    if (!scene.hasComponent<scene::DirectionalLightComponent>(selectedEntity)) {
        return false;
    }

    bool changed = false;
    auto& light = scene.getComponent<scene::DirectionalLightComponent>(selectedEntity);
    auto lightColor = light.color;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditDirectionalLightCommandId,
        [&]() {
            return ImGui::ColorEdit3("Color", &lightColor.x);
        },
        [&]() {
            light.color = lightColor;
        });
    auto intensity = light.intensity;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditDirectionalLightCommandId,
        [&]() {
            return ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 1000.0f, "%.3f");
        },
        [&]() {
            light.intensity = intensity;
        });

    if (ImGui::TreeNodeEx("Shadow", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto shadowEnabled = light.shadow.enabled;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::Checkbox("Enabled", &shadowEnabled);
            },
            [&]() {
                light.shadow.enabled = shadowEnabled;
            });

        auto mapSize = light.shadow.map_size;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return drawShadowMapSizeControl(mapSize);
            },
            [&]() {
                light.shadow.map_size = std::max(mapSize, 1u);
            });

        auto maxDistance = light.shadow.max_distance;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::DragFloat("Max Distance", &maxDistance, 0.5f, 0.0f, 10000.0f, "%.2f");
            },
            [&]() {
                light.shadow.max_distance = std::max(maxDistance, 0.0f);
            });

        auto bias = light.shadow.bias;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::DragFloat("Bias", &bias, 0.0001f, 0.0f, 0.1f, "%.6f");
            },
            [&]() {
                light.shadow.bias = std::max(bias, 0.0f);
            });

        auto normalBias = light.shadow.normal_bias;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::DragFloat("Normal Bias", &normalBias, 0.001f, 0.0f, 1.0f, "%.4f");
            },
            [&]() {
                light.shadow.normal_bias = std::max(normalBias, 0.0f);
            });

        int pcfRadius = static_cast<int>(light.shadow.pcf_radius);
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::SliderInt("PCF Radius", &pcfRadius, 0, 4);
            },
            [&]() {
                light.shadow.pcf_radius = static_cast<uint32_t>(std::clamp(pcfRadius, 0, 4));
            });

        int cascadeCount = static_cast<int>(light.shadow.cascade_count);
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::SliderInt("Cascade Count", &cascadeCount, 1, 4);
            },
            [&]() {
                light.shadow.cascade_count = static_cast<uint32_t>(std::clamp(cascadeCount, 1, 4));
            });

        auto cascadeSplitLambda = light.shadow.cascade_split_lambda;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::DragFloat("Cascade Split Lambda", &cascadeSplitLambda, 0.01f, 0.0f, 1.0f, "%.3f");
            },
            [&]() {
                light.shadow.cascade_split_lambda = std::clamp(cascadeSplitLambda, 0.0f, 1.0f);
            });

        auto cascadeSeamBlend = light.shadow.cascade_seam_blend;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditDirectionalLightCommandId,
            [&]() {
                return ImGui::DragFloat("Cascade Seam Blend", &cascadeSeamBlend, 0.1f, 0.0f, 1000.0f, "%.2f");
            },
            [&]() {
                light.shadow.cascade_seam_blend = std::max(cascadeSeamBlend, 0.0f);
            });

        auto cascadeCasterDepthPadding = light.shadow.cascade_caster_depth_padding;
        changed |= drawLiveSceneEdit(
            scene,
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

    return changed;
}
} // namespace

void registerRendererComponentInspectors(scene::ComponentRegistry& registry)
{
    registry.setInspector(renderer::MeshRendererComponentType, &drawMeshRendererInspector);
    registry.setInspector(renderer::SpriteRendererComponentType, &drawSpriteRendererInspector);
    registry.setInspector(renderer::CameraComponentType, &drawCameraInspector);
    registry.setInspector(renderer::DirectionalLightComponentType, &drawDirectionalLightInspector);
}

} // namespace lunalite::editor
