add_executable(LunaLiteTestApp
    application.cpp
    test_app_layer.cpp
)

target_link_libraries(LunaLiteTestApp
    PRIVATE
        LunaLite
)
