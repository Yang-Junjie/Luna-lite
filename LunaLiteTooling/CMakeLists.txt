add_library(LunaLiteTooling STATIC
    commands/asset_commands.cpp
    commands/command_registry.cpp
)

target_link_libraries(LunaLiteTooling
    PUBLIC
        LunaLite
)
