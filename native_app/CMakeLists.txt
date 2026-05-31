add_executable(LunaLiteNativeApp
    application.cpp
    native_app_layer.cpp
)

target_link_libraries(LunaLiteNativeApp
    PRIVATE
        LunaLite
)
