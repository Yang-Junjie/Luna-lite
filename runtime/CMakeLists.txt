add_executable(LunaLiteRuntime
    application.cpp
)

target_link_libraries(LunaLiteRuntime
    PRIVATE
        LunaLite
)
