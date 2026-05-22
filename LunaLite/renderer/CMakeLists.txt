
add_library(LunaLiteRenderer
    default_renderer/renderer.cpp
)

target_link_libraries(
    LunaLiteRenderer
    PUBLIC
        glm::glm
        LunaLiteRHI
)
