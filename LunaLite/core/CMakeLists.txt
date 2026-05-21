
set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)

add_executable(LunaLite
    entry_point.cpp
    window.cpp
)

target_include_directories(
    LunaLite
    SYSTEM PUBLIC
        ${THIRD_PARTY_DIR}/glfw/include
)

target_link_libraries(LunaLite
        PUBLIC
        glfw
        glad::glad
        LunaLiteRHI
)
