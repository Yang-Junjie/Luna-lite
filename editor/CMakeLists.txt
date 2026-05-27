add_executable(LunaLiteEditor
    application.cpp
    editor_layer.cpp
)

target_link_libraries(LunaLiteEditor
    PRIVATE
        LunaLite
)
