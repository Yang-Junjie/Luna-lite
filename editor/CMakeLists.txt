add_executable(LunaLiteEditor
    application.cpp
    content_browser_panel.cpp
    editor_camera.cpp
    editor_layer.cpp
    hierarchy_panel.cpp
    inspector_panel.cpp
)

target_link_libraries(LunaLiteEditor
    PRIVATE
        LunaLite
)
