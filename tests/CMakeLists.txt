add_executable(AssetManagerTest
    asset_manager_test.cpp
)

target_link_libraries(AssetManagerTest
    PRIVATE
        LunaLite
)

add_test(NAME AssetManagerTest COMMAND AssetManagerTest)

add_executable(SceneSerializerTest
    scene_serializer_test.cpp
)

target_link_libraries(SceneSerializerTest
    PRIVATE
        LunaLite
)

add_test(NAME SceneSerializerTest COMMAND SceneSerializerTest)

add_executable(MeshBoundsTest
    mesh_bounds_test.cpp
)

target_link_libraries(MeshBoundsTest
    PRIVATE
        LunaLite
)

add_test(NAME MeshBoundsTest COMMAND MeshBoundsTest)

add_executable(DebugRendererTest
    debug_renderer_test.cpp
)

target_link_libraries(DebugRendererTest
    PRIVATE
        LunaLite
)

add_test(NAME DebugRendererTest COMMAND DebugRendererTest)

add_executable(FrustumTest
    frustum_test.cpp
)

target_link_libraries(FrustumTest
    PRIVATE
        LunaLite
)

add_test(NAME FrustumTest COMMAND FrustumTest)

add_executable(SpriteCullingTest
    sprite_culling_test.cpp
)

target_link_libraries(SpriteCullingTest
    PRIVATE
        LunaLite
)

add_test(NAME SpriteCullingTest COMMAND SpriteCullingTest)

add_executable(SpriteAnimationSystemTest
    sprite_animation_system_test.cpp
)

target_link_libraries(SpriteAnimationSystemTest
    PRIVATE
        LunaLite
)

add_test(NAME SpriteAnimationSystemTest COMMAND SpriteAnimationSystemTest)

add_executable(ToolingCommandTest
    tooling_command_test.cpp
)

target_link_libraries(ToolingCommandTest
    PRIVATE
        LunaLiteTooling
)

add_test(NAME ToolingCommandTest COMMAND ToolingCommandTest)

add_executable(ProjectCommandsTest
    project_commands_test.cpp
)

target_link_libraries(ProjectCommandsTest
    PRIVATE
        LunaLiteTooling
)

add_test(NAME ProjectCommandsTest COMMAND ProjectCommandsTest)

add_executable(SelectionContextTest
    selection_context_test.cpp
)

target_link_libraries(SelectionContextTest
    PRIVATE
        LunaLiteTooling
)

add_test(NAME SelectionContextTest COMMAND SelectionContextTest)

add_executable(SceneCommandsTest
    scene_commands_test.cpp
)

target_link_libraries(SceneCommandsTest
    PRIVATE
        LunaLiteTooling
)

add_test(NAME SceneCommandsTest COMMAND SceneCommandsTest)

add_executable(CommandHistoryTest
    command_history_test.cpp
)

target_link_libraries(CommandHistoryTest
    PRIVATE
        LunaLiteTooling
)

add_test(NAME CommandHistoryTest COMMAND CommandHistoryTest)

add_executable(SceneEditTransactionTest
    scene_edit_transaction_test.cpp
)

target_link_libraries(SceneEditTransactionTest
    PRIVATE
        LunaLiteTooling
)

add_test(NAME SceneEditTransactionTest COMMAND SceneEditTransactionTest)

add_executable(ScriptRuntimeTest
    script_runtime_test.cpp
)

target_link_libraries(ScriptRuntimeTest
    PRIVATE
        LunaLite
)

add_test(NAME ScriptRuntimeTest COMMAND ScriptRuntimeTest)
