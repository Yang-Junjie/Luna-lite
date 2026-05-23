add_executable(LunaLiteRuntime
    application.cpp
    demo_layer.cpp
)

target_link_libraries(LunaLiteRuntime
    PRIVATE
        LunaLite
)
