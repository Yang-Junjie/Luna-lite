
add_executable(LunaLite
    entry_point.cpp
    window.cpp
    uuid.cpp
    ../asset/mesh_asset_loader.cpp
    ../scene/scene.cpp
    ../scene/scene_renderer.cpp
)

target_link_libraries(LunaLite
        PUBLIC
        glfw
        glad::glad
        glm::glm
        EnTT::EnTT
        tiny_obj_loader
        LunaLiteRHI
        LunaLiteRenderer
)
