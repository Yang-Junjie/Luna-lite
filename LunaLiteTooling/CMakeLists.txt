add_library(LunaLiteTooling STATIC
    commands/asset_commands.cpp
    commands/command_registry.cpp
    commands/command_manager.cpp
    commands/scene_commands.cpp
)

target_link_libraries(LunaLiteTooling
    PUBLIC
        LunaLite
)
