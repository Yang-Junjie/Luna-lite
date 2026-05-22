
add_executable(LunaLite
    entry_point.cpp
    window.cpp
    uuid.cpp
    ../scene/scene_renderer.cpp
)

target_link_libraries(LunaLite
        PUBLIC
        glfw
        glad::glad
        glm::glm
        LunaLiteRHI
        LunaLiteRenderer
)
