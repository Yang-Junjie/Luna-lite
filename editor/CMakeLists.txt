add_executable(LunaLiteEditor
    application.cpp
    editor_camera.cpp
    editor_layer.cpp
    panels/content_browser_panel.cpp
    panels/editor_setting_panel.cpp
    panels/hierarchy_panel.cpp
    panels/inspector_panel.cpp
    panels/material_editor_panel.cpp
)

target_link_libraries(LunaLiteEditor
    PRIVATE
        LunaLite
)
