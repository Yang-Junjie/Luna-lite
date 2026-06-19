set(TINYRHI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYRHI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(TinyRHI)

add_library(LunaLiteRenderer
    renderer/debug_renderer.cpp
    renderer/default_renderer/environment_map_cache.cpp
    renderer/default_renderer/gbuffer_resource.cpp
    renderer/default_renderer/gpu_profiler.cpp
    renderer/default_renderer/material_gpu_cache.cpp
    renderer/default_renderer/passes/debug_line_pass.cpp
    renderer/default_renderer/passes/geometry_pass.cpp
    renderer/default_renderer/passes/lighting_pass.cpp
    renderer/default_renderer/passes/shadow_pass.cpp
    renderer/default_renderer/passes/skybox_pass.cpp
    renderer/default_renderer/passes/sprite_pass.cpp
    renderer/default_renderer/renderer.cpp
    renderer/default_renderer/renderer_common.cpp
    renderer/default_renderer/renderer_pipeline_resources.cpp
    renderer/default_renderer/shadow_map_resource.cpp
    renderer/default_renderer/texture_gpu_cache.cpp
    renderer/frame_presenter/rhi_frame_presenter.cpp
)

target_link_libraries(LunaLiteRenderer
    PUBLIC
        glm::glm
        TinyRHI::TinyRHI
    PRIVATE
        yaml-cpp::yaml-cpp
        spdlog::spdlog
)

target_compile_definitions(LunaLiteRenderer
    PRIVATE
        LUNALITE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
        $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
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

target_compile_definitions(LunaLiteImGui
    PRIVATE
        LUNALITE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

add_library(LunaLite STATIC
    core/entry_point.cpp
    core/application.cpp
    core/input.cpp
    platform/window/glfw_window.cpp
    core/layer_stack.cpp
    core/log.cpp
    core/uuid.cpp
    animation/sprite_animation.cpp
    animation/sprite_animator.cpp
    asset/asset_cache.cpp
    asset/asset_importer.cpp
    asset/asset_importer_registry.cpp
    asset/asset_loader_registry.cpp
    asset/asset_manager.cpp
    asset/metadata/asset_metadata_serializer.cpp
    asset/metadata/asset_metadata_store.cpp
    asset/metadata/asset_path_resolver.cpp
    asset/asset_scanner.cpp
    asset/builtin/material_factory.cpp
    asset/builtin/mesh_factory.cpp
    asset/factory/asset_factory_manager.cpp
    asset/factory/sprite_asset_factory.cpp
    asset/importers/material_asset_importer.cpp
    asset/importers/mesh/material_asset_definition_writer.cpp
    asset/importers/mesh/prefab_asset_definition_writer.cpp
    asset/importers/mesh/mesh_asset_importer.cpp
    asset/importers/mesh/obj_mesh_derived_asset_generator.cpp
    asset/importers/mesh/gltf_mesh_derived_asset_generator.cpp
    asset/importers/prefab_asset_importer.cpp
    asset/importers/scene_asset_importer.cpp
    asset/importers/script_asset_importer.cpp
    asset/importers/sprite_animation_clip_importer.cpp
    asset/importers/sprite_animator_controller_importer.cpp
    asset/importers/sprite_asset_importer.cpp
    asset/importers/texture_asset_importer.cpp
    asset/loaders/material_asset_loader.cpp
    asset/loaders/mesh_asset_loader.cpp
    asset/loaders/prefab_asset_loader.cpp
    asset/loaders/sprite_animation_clip_loader.cpp
    asset/loaders/sprite_animator_controller_loader.cpp
    asset/loaders/sprite_asset_loader.cpp
    asset/loaders/texture_asset_loader.cpp
    platform/common/file_dialogs.h
    project/project_manager.cpp
    scene/scene.cpp
    scene/sprite_animation_system.cpp
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
        fastgltf::fastgltf
        yaml-cpp::yaml-cpp
        TinyRHI::TinyRHI
        LunaLiteRenderer
        LunaLiteImGui
        sol2::sol2
    PRIVATE
        glfw
        stb::stb
)

target_compile_definitions(LunaLite
    PUBLIC
        $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
)
