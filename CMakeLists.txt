cmake_minimum_required(VERSION 3.16)
project(lunalite)

enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_USE_STRICT_FLAGS OFF CACHE BOOL "" FORCE)

set(SIMDJSON_INSTALL OFF CACHE BOOL "" FORCE)
set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
set(SIMDJSON_ENABLE_THREADS OFF CACHE BOOL "" FORCE)

set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/glm)
add_subdirectory(third_party/entt)
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/yaml-cpp)
add_subdirectory(third_party/stb)
add_subdirectory(third_party/simdjson)
add_subdirectory(third_party/fastgltf)

add_library(lua STATIC
    third_party/lua/lapi.c
    third_party/lua/lauxlib.c
    third_party/lua/lbaselib.c
    third_party/lua/lcode.c
    third_party/lua/lcorolib.c
    third_party/lua/lctype.c
    third_party/lua/ldblib.c
    third_party/lua/ldebug.c
    third_party/lua/ldo.c
    third_party/lua/ldump.c
    third_party/lua/lfunc.c
    third_party/lua/lgc.c
    third_party/lua/linit.c
    third_party/lua/liolib.c
    third_party/lua/llex.c
    third_party/lua/lmathlib.c
    third_party/lua/lmem.c
    third_party/lua/loadlib.c
    third_party/lua/lobject.c
    third_party/lua/lopcodes.c
    third_party/lua/loslib.c
    third_party/lua/lparser.c
    third_party/lua/lstate.c
    third_party/lua/lstring.c
    third_party/lua/lstrlib.c
    third_party/lua/ltable.c
    third_party/lua/ltablib.c
    third_party/lua/ltm.c
    third_party/lua/lundump.c
    third_party/lua/lutf8lib.c
    third_party/lua/lvm.c
    third_party/lua/lzio.c
)
add_library(lua::lua ALIAS lua)

target_include_directories(lua PUBLIC third_party/lua)

if(WIN32)
    target_compile_definitions(lua PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(sol2 INTERFACE)
add_library(sol2::sol2 ALIAS sol2)

target_include_directories(sol2 SYSTEM INTERFACE third_party/sol2/include)
target_link_libraries(sol2 INTERFACE lua::lua)

add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
)
add_library(imgui::imgui ALIAS imgui)

target_include_directories(imgui
    PUBLIC
        third_party/imgui
        third_party/imgui/backends
)
target_sources(imgui PRIVATE third_party/imgui/backends/imgui_impl_glfw.cpp)
target_link_libraries(imgui PUBLIC glfw)

if(WIN32)
    target_link_libraries(imgui PUBLIC gdi32 dwmapi)
endif()

add_library(tiny_obj_loader INTERFACE)
target_include_directories(tiny_obj_loader INTERFACE third_party/tiny_obj_loader)

add_subdirectory(LunaLite)
add_subdirectory(LunaLiteTooling)
add_subdirectory(runtime)
add_subdirectory(editor)
add_subdirectory(test_app)
add_subdirectory(tests)
