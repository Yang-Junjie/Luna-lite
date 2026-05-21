add_library(LunaLiteRHI
    opengl/instance.cpp
)

target_link_libraries(LunaLiteRHI
    PUBLIC
        glfw
        glad::glad
)
