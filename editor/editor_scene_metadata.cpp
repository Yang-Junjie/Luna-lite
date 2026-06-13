#include "../LunaLite/asset/metadata/asset_metadata_store.h"
#include "../LunaLite/core/log.h"
#include "editor_scene_metadata.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <yaml-cpp/yaml.h>

namespace lunalite::editor {
namespace {
constexpr const char* kEditorsNode = "Editors";
constexpr const char* kEditorNamespace = "LunaLite";
constexpr const char* kCameraNode = "Camera";

using ProjectionType = EditorCamera::ProjectionType;

YAML::Node writeVec3(const glm::vec3& value)
{
    YAML::Node node;
    node.push_back(value.x);
    node.push_back(value.y);
    node.push_back(value.z);
    return node;
}

glm::vec3 readVec3(const YAML::Node& node, const glm::vec3& fallback)
{
    if (!node || !node.IsSequence() || node.size() != 3) {
        return fallback;
    }

    return {
        node[0].as<float>(),
        node[1].as<float>(),
        node[2].as<float>(),
    };
}

const char* projectionTypeToString(ProjectionType type)
{
    switch (type) {
        case ProjectionType::Orthographic:
            return "Orthographic";
        case ProjectionType::Perspective:
        default:
            return "Perspective";
    }
}

ProjectionType stringToProjectionType(const std::string& type)
{
    if (type == "Orthographic") {
        return ProjectionType::Orthographic;
    }

    return ProjectionType::Perspective;
}

void ensureMap(YAML::Node& node)
{
    if (!node || !node.IsMap()) {
        node = YAML::Node(YAML::NodeType::Map);
    }
}

void writeCameraNode(YAML::Node& config, const EditorCamera& editor_camera)
{
    ensureMap(config);

    auto editorsNode = config[kEditorsNode];
    ensureMap(editorsNode);

    auto editorNode = editorsNode[kEditorNamespace];
    ensureMap(editorNode);

    auto cameraNode = editorNode[kCameraNode];
    ensureMap(cameraNode);

    cameraNode["ProjectionType"] = projectionTypeToString(editor_camera.getProjectionType());
    cameraNode["Position"] = writeVec3(editor_camera.getPosition());
    cameraNode["Yaw"] = editor_camera.getYaw();
    cameraNode["Pitch"] = editor_camera.getPitch();
    cameraNode["Exposure"] = editor_camera.getExposure();

    editorNode[kCameraNode] = cameraNode;
    editorsNode[kEditorNamespace] = editorNode;
    config[kEditorsNode] = editorsNode;
}

bool readCameraNode(const YAML::Node& config, EditorCamera& editor_camera)
{
    const auto editorsNode = config[kEditorsNode];
    if (!editorsNode || !editorsNode.IsMap()) {
        return false;
    }

    const auto editorNode = editorsNode[kEditorNamespace];
    if (!editorNode || !editorNode.IsMap()) {
        return false;
    }

    const auto cameraNode = editorNode[kCameraNode];
    if (!cameraNode || !cameraNode.IsMap()) {
        return false;
    }

    const auto projectionType = stringToProjectionType(
        cameraNode["ProjectionType"].as<std::string>(projectionTypeToString(editor_camera.getProjectionType())));
    const auto position = readVec3(cameraNode["Position"], editor_camera.getPosition());
    const auto yaw = cameraNode["Yaw"].as<float>(editor_camera.getYaw());
    const auto pitch = cameraNode["Pitch"].as<float>(editor_camera.getPitch());
    const auto exposure = cameraNode["Exposure"].as<float>(editor_camera.getExposure());

    editor_camera.resetSceneState();
    editor_camera.setPosition(position);
    editor_camera.setYaw(yaw);
    editor_camera.setPitch(pitch);
    editor_camera.setExposure(exposure);
    editor_camera.setProjectionType(projectionType);
    editor_camera.clearSceneStateDirty();
    return true;
}
} // namespace

bool loadEditorSceneCamera(const std::filesystem::path& scene_path, EditorCamera& editor_camera)
{
    if (scene_path.empty()) {
        editor_camera.resetSceneState();
        return false;
    }

    const auto metadataPath = asset::AssetMetadataStore::metadataFilePathForAsset(scene_path);
    std::error_code error;
    if (!std::filesystem::exists(metadataPath, error)) {
        if (error) {
            LUNA_CORE_ERROR("Failed to check scene metadata '{}': {}", metadataPath.string(), error.message());
        }
        editor_camera.resetSceneState();
        return false;
    }

    asset::AssetMetadataStore metadataStore;
    const auto metadata = metadataStore.readMetadataFile(metadataPath);
    if (!metadata) {
        editor_camera.resetSceneState();
        return false;
    }

    const auto loaded = readCameraNode(metadata->SpecializedConfig, editor_camera);
    if (!loaded) {
        editor_camera.resetSceneState();
    }
    return loaded;
}

bool saveEditorSceneCamera(const std::filesystem::path& scene_path, const EditorCamera& editor_camera)
{
    if (scene_path.empty()) {
        return false;
    }

    asset::AssetMetadataStore metadataStore;
    auto metadata = metadataStore.createOrLoadMetadata(scene_path, asset::AssetType::Scene);
    metadata.Type = asset::AssetType::Scene;
    writeCameraNode(metadata.SpecializedConfig, editor_camera);
    const bool written = metadataStore.writeMetadataFile(metadata);
    if (!written) {
        LUNA_CORE_ERROR("Failed to write editor camera metadata for scene '{}'", scene_path.string());
    }
    return written;
}

} // namespace lunalite::editor
