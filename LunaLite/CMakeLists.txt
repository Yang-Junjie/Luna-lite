set(TINYRHI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYRHI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(TinyRHI)

add_library(LunaLiteRenderer
    renderer/controller/renderer_controller.cpp
    renderer/default_renderer/renderer.cpp
    renderer/frame_presenter/rhi_frame_presenter.cpp
)

target_link_libraries(LunaLiteRenderer
    PUBLIC
        glm::glm
        TinyRHI::TinyRHI
        LunaLiteSoftRasterizationRenderer
    PRIVATE
        spdlog::spdlog
)

add_library(LunaLiteSoftRasterizationRenderer
    renderer/soft_rasterization_renderer/soft_rasterization_renderer.cpp
)

target_link_libraries(LunaLiteSoftRasterizationRenderer
    PUBLIC
        glm::glm
        TinyRHI::TinyRHI
)

add_library(LunaLitePathTracingRenderer
    renderer/path_tracing_renderer/path_tracing_renderer.cpp
)

target_link_libraries(LunaLitePathTracingRenderer
    PUBLIC
        glm::glm
        TinyRHI::TinyRHI
)

add_library(LunaLite STATIC
    core/entry_point.cpp
    core/application.cpp
    core/window.cpp
    core/layer_stack.cpp
    core/log.cpp
    core/uuid.cpp
    asset/mesh_asset_loader.cpp
    scene/scene.cpp
    scene/scene_renderer.cpp
)

target_link_libraries(LunaLite
    PUBLIC
        glfw
        glm::glm
        EnTT::EnTT
        spdlog::spdlog
        tiny_obj_loader
        TinyRHI::TinyRHI
        LunaLiteRenderer
)
