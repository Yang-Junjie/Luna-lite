add_executable(AssetManagerTest
    asset_manager_test.cpp
)

target_link_libraries(AssetManagerTest
    PRIVATE
        LunaLite
)

add_executable(SceneSerializerTest
    scene_serializer_test.cpp
)

target_link_libraries(SceneSerializerTest
    PRIVATE
        LunaLite
)

add_executable(MeshBoundsTest
    mesh_bounds_test.cpp
)

target_link_libraries(MeshBoundsTest
    PRIVATE
        LunaLite
)

add_executable(DebugRendererTest
    debug_renderer_test.cpp
)

target_link_libraries(DebugRendererTest
    PRIVATE
        LunaLite
)

add_executable(FrustumTest
    frustum_test.cpp
)

target_link_libraries(FrustumTest
    PRIVATE
        LunaLite
)

add_executable(SpriteCullingTest
    sprite_culling_test.cpp
)

target_link_libraries(SpriteCullingTest
    PRIVATE
        LunaLite
)

add_executable(ToolingCommandTest
    tooling_command_test.cpp
)

target_link_libraries(ToolingCommandTest
    PRIVATE
        LunaLiteTooling
)

add_executable(SelectionContextTest
    selection_context_test.cpp
)

target_link_libraries(SelectionContextTest
    PRIVATE
        LunaLiteTooling
)

add_executable(SceneCommandsTest
    scene_commands_test.cpp
)

target_link_libraries(SceneCommandsTest
    PRIVATE
        LunaLiteTooling
)

add_executable(CommandHistoryTest
    command_history_test.cpp
)

target_link_libraries(CommandHistoryTest
    PRIVATE
        LunaLiteTooling
)

add_executable(SceneEditTransactionTest
    scene_edit_transaction_test.cpp
)

target_link_libraries(SceneEditTransactionTest
    PRIVATE
        LunaLiteTooling
)

add_executable(ScriptRuntimeTest
    script_runtime_test.cpp
)

target_link_libraries(ScriptRuntimeTest
    PRIVATE
        LunaLite
)
