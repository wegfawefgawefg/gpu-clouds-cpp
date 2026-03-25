cmake_minimum_required(VERSION 3.28)

project(gpu_clouds_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)
find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_ALSA OFF CACHE BOOL "" FORCE)
set(SDL_JACK OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.2
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.6
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3 SDL3_ttf imgui nlohmann_json)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui
  PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui
  PUBLIC
    SDL3::SDL3
    Vulkan::Vulkan
)

set(CLOUDS_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/clouds.comp)
set(CLOUDS_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/clouds.comp.spv)
set(PRESENT_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/present.comp)
set(PRESENT_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/present.comp.spv)

add_custom_command(
  OUTPUT ${CLOUDS_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S comp -o ${CLOUDS_SHADER_OUTPUT} ${CLOUDS_SHADER_SOURCE}
  DEPENDS ${CLOUDS_SHADER_SOURCE}
  VERBATIM
)

add_custom_command(
  OUTPUT ${PRESENT_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S comp -o ${PRESENT_SHADER_OUTPUT} ${PRESENT_SHADER_SOURCE}
  DEPENDS ${PRESENT_SHADER_SOURCE}
  VERBATIM
)

add_custom_target(gpu-clouds-shaders DEPENDS ${CLOUDS_SHADER_OUTPUT} ${PRESENT_SHADER_OUTPUT})

add_executable(gpu-clouds-cpp
  src/app.cpp
  src/camera.cpp
  src/main.cpp
  src/settings_io.cpp
  src/vulkan_helpers.cpp
  src/vulkan_renderer_frame.cpp
  src/vulkan_renderer_setup.cpp
)

target_compile_options(gpu-clouds-cpp
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-missing-field-initializers
)

target_include_directories(gpu-clouds-cpp
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(gpu-clouds-cpp
  PRIVATE
    imgui
    nlohmann_json::nlohmann_json
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    Vulkan::Vulkan
)

target_compile_definitions(gpu-clouds-cpp
  PRIVATE
    GPU_CLOUDS_SHADER_PATH="${CLOUDS_SHADER_OUTPUT}"
    GPU_PRESENT_SHADER_PATH="${PRESENT_SHADER_OUTPUT}"
)

add_dependencies(gpu-clouds-cpp gpu-clouds-shaders)
