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

add_library(LunaLiteImGui
    imgui/imgui_renderer.cpp
    platform/imgui/glfw_imgui_platform.cpp
)

target_link_libraries(LunaLiteImGui
    PUBLIC
        imgui::imgui
        TinyRHI::TinyRHI
)

add_library(LunaLite STATIC
    core/entry_point.cpp
    core/application.cpp
    core/input.cpp
    platform/window/glfw_window.cpp
    core/layer_stack.cpp
    core/log.cpp
    core/uuid.cpp
    asset/asset_importer.cpp
    asset/asset_manager.cpp
    asset/mesh_asset_importer.cpp
    asset/mesh_asset_loader.cpp
    asset/script_asset_importer.cpp
    platform/common/file_dialogs.h
    project/project_manager.cpp
    scene/scene.cpp
    scene/scene_serializer.cpp
    scene/scene_renderer.cpp
    script/glue/entity_api.cpp
    script/glue/input_api.cpp
    script/glue/log_api.cpp
    script/lua/lua_script_runtime.cpp
    script/glue/scene_api.cpp
    script/script_runtime.cpp
    script/glue/time_api.cpp
)

if(WIN32)
    target_sources(LunaLite PRIVATE platform/windows/win_file_dialogs.cpp)
else()
    target_sources(LunaLite PRIVATE platform/common/file_dialogs.cpp)
endif()

target_link_libraries(LunaLite
    PUBLIC
        glm::glm
        EnTT::EnTT
        spdlog::spdlog
        tiny_obj_loader
        yaml-cpp::yaml-cpp
        TinyRHI::TinyRHI
        LunaLiteRenderer
        LunaLiteImGui
        sol2::sol2
    PRIVATE
        glfw
)
