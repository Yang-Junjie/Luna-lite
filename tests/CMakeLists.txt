add_executable(SoftRasterizationRendererTest
    soft_rasterization_renderer_test.cpp
)

target_link_libraries(SoftRasterizationRendererTest
    PRIVATE
        LunaLite
        LunaLiteSoftRasterizationRenderer
)

add_executable(AssetManagerTest
    asset_manager_test.cpp
)

target_link_libraries(AssetManagerTest
    PRIVATE
        LunaLite
)

add_executable(PathTracingRendererTest
    path_tracing_renderer_test.cpp
)

target_link_libraries(PathTracingRendererTest
    PRIVATE
        LunaLite
        LunaLitePathTracingRenderer
)
