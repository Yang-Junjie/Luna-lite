add_library(LunaLiteRHI
    rhi/opengl/instance.cpp
    rhi/opengl/device.cpp
    rhi/opengl/buffer.cpp
    rhi/opengl/texture.cpp
    rhi/opengl/shader.cpp
    rhi/opengl/pipeline.cpp
    rhi/opengl/gl_convert.cpp
    rhi/opengl/command_list.cpp
    rhi/opengl/swapchain.cpp
    rhi/backend_factory.cpp
)

target_link_libraries(LunaLiteRHI
    PUBLIC
        glad::glad
        glm::glm
)

add_library(LunaLiteRenderer
    renderer/controller/renderer_controller.cpp
    renderer/default_renderer/renderer.cpp
    renderer/frame_presenter/rhi_frame_presenter.cpp
)

target_link_libraries(LunaLiteRenderer
    PUBLIC
        glm::glm
        LunaLiteRHI
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
)

add_library(LunaLitePathTracingRenderer
    renderer/path_tracing_renderer/path_tracing_renderer.cpp
)

target_link_libraries(LunaLitePathTracingRenderer
    PUBLIC
        glm::glm
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
        glad::glad
        glm::glm
        EnTT::EnTT
        spdlog::spdlog
        tiny_obj_loader
        LunaLiteRHI
        LunaLiteRenderer
)
