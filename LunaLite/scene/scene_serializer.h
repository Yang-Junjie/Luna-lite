#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace lunalite::scene {

class Scene;

class SceneSerializer {
public:
    static constexpr const char* FileExtension = ".lunascene";
    static bool serialize(const Scene& scene, const std::filesystem::path& scene_path);
    static bool deserialize(Scene& scene, const std::filesystem::path& scene_path);
    static std::string serializeToString(const Scene& scene);
    static bool deserializeFromString(Scene& scene, const std::string& scene_data);
};

} // namespace lunalite::scene
