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
};

} // namespace lunalite::scene
