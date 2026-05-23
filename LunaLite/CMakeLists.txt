add_library(LunaLiteRHI
    rhi/opengl/instance.cpp
    rhi/opengl/device.cpp
    rhi/opengl/command_context.cpp
    rhi/backend_factory.cpp
)

target_link_libraries(LunaLiteRHI
    PUBLIC
        glfw
        glad::glad
        glm::glm
)

add_library(LunaLiteRenderer
    renderer/default_renderer/renderer.cpp
)

target_link_libraries(LunaLiteRenderer
    PUBLIC
        glm::glm
        LunaLiteRHI
)

add_executable(LunaLite
    core/entry_point.cpp
    core/window.cpp
    core/uuid.cpp
    asset/mesh_asset_loader.cpp
    scene/scene.cpp
    scene/scene_renderer.cpp
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
