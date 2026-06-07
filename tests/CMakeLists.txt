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

add_executable(ScriptRuntimeTest
    script_runtime_test.cpp
)

target_link_libraries(ScriptRuntimeTest
    PRIVATE
        LunaLite
)
