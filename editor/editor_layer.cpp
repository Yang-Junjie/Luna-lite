#include "../LunaLite/asset/mesh_asset_loader.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/imgui/imgui_renderer.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene_renderer.h"
#include "editor_layer.h"

#include <cstdint>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <imgui.h>

namespace lunalite::editor {

EditorLayer::EditorLayer()
    : Layer("EditorLayer")
{}

void EditorLayer::onAttach()
{
    const auto obj_handle = asset::MeshAssetLoader::loadObj("../../assets/stanford-bunny.obj");

    {
        auto entity = m_scene.createEntity();
        m_model_entity = entity;
        auto& transformComp = m_scene.addComponent<scene::TransformComponent>(entity);
        transformComp.scale = glm::vec3(8.0f, 8.0f, 8.0f);
        auto& meshComp = m_scene.addComponent<scene::MeshComponent>(entity);
        meshComp.mesh = obj_handle;
    }

    {
        auto entity = m_scene.createEntity();
        auto& light = m_scene.addComponent<scene::DirectionalLightComponent>(entity);
        light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    }
}

void EditorLayer::onUpdate(core::Timestep dt)
{
    m_editor_camera.onUpdate(dt, m_viewport_hovered);
}

void EditorLayer::onRender()
{
    core::Application::get().getSceneRenderer().onRenderEditor(m_scene, m_editor_camera);
}

void EditorLayer::onImGuiRender()
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    }
    drawViewport();
}

void EditorLayer::drawViewport()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");
    m_viewport_hovered = false;

    const auto& frame_image = core::Application::get().getFrameImage();
    const ImTextureID texture = core::Application::get().getImGuiRenderer().textureId(frame_image);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (texture != ImTextureID_Invalid && frame_image.width > 0 && frame_image.height > 0 && available.x > 1.0f &&
        available.y > 1.0f) {
        core::Application::get().getSceneRenderer().setViewportSize(static_cast<uint32_t>(available.x),
                                                                    static_cast<uint32_t>(available.y));
        ImGui::Image(texture, available, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        m_viewport_hovered = ImGui::IsItemHovered();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace lunalite::editor
