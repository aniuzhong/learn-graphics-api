cmake_minimum_required(VERSION 3.22)
project(learn-graphics-api VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WARN_DEPRECATED OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if(MSVC)
    add_compile_options(/utf-8 /W3 /wd4005)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX)
endif()

include(FetchContent)

# =============================================================================
# GLFW
# =============================================================================
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# =============================================================================
# OpenGL
# =============================================================================
find_package(OpenGL REQUIRED)

# =============================================================================
# glad (OpenGL 4.6 loader, generated at configure time)
# =============================================================================
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(GLAD_OUT_DIR "${CMAKE_BINARY_DIR}/_generated/glad")
set(GLAD_SRC     "${GLAD_OUT_DIR}/src/glad.c")
set(GLAD_INC     "${GLAD_OUT_DIR}/include")

if(NOT EXISTS "${GLAD_SRC}")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m pip install glad --quiet --disable-pip-version-check
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE PIP_RESULT
    )
    if(NOT PIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install glad. Try: pip install glad")
    endif()
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m glad
            --api       gl=4.6
            --profile   core
            --generator c
            --out-path  ${GLAD_OUT_DIR}
            --reproducible
        RESULT_VARIABLE GLAD_RESULT
    )
    if(NOT GLAD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate GLAD loader")
    endif()
    message(STATUS "GLAD loader generated for OpenGL 4.6 core")
endif()

add_library(glad STATIC ${GLAD_SRC})
target_include_directories(glad PUBLIC ${GLAD_INC})
if(MSVC)
    target_compile_options(glad PRIVATE "/MD$<$<CONFIG:Debug>:d>" "/utf-8")
endif()
target_link_libraries(glad PUBLIC OpenGL::GL)

# =============================================================================
# glm (header-only math library)
# =============================================================================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# =============================================================================
# stb_image — single-header image loader
# =============================================================================
set(STB_DIR    "${CMAKE_BINARY_DIR}/_generated/stb")
set(STB_HEADER "${STB_DIR}/stb_image.h")
if(NOT EXISTS "${STB_HEADER}")
    file(MAKE_DIRECTORY "${STB_DIR}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
        "${STB_HEADER}"
        SHOW_PROGRESS
    )
    message(STATUS "stb_image.h downloaded")
endif()

add_library(stb_image STATIC src/stb_image.cpp)
target_include_directories(stb_image PUBLIC "${STB_DIR}")

# =============================================================================
# Common renderer libraries  (linked by every example)
# =============================================================================
set(RENDERER_LIBS glfw glad OpenGL::GL d3d11 dxgi d3dcompiler)
set(RENDERER_SRCS
    src/glfw_window.cpp
    src/renderer.cpp
    src/renderer_gl.cpp
    src/renderer_d3d11.cpp
)

# =============================================================================
# Hello Triangle
# =============================================================================
add_executable(hello-triangle src/Example_Hello_Triangle.cpp ${RENDERER_SRCS})
target_link_libraries(hello-triangle PRIVATE ${RENDERER_LIBS})

# =============================================================================
# Textures
# =============================================================================
add_executable(textures src/Example_Textures.cpp ${RENDERER_SRCS})
target_link_libraries(textures PRIVATE ${RENDERER_LIBS} stb_image)
target_include_directories(textures PRIVATE "${STB_DIR}")

# =============================================================================
# Camera
# =============================================================================
add_executable(camera src/Example_Camera.cpp ${RENDERER_SRCS})
target_link_libraries(camera PRIVATE ${RENDERER_LIBS} stb_image glm)
target_include_directories(camera PRIVATE "${STB_DIR}")

# =============================================================================
# Framebuffer
# =============================================================================
add_executable(framebuffer src/Example_Framebuffer.cpp ${RENDERER_SRCS})
target_link_libraries(framebuffer PRIVATE ${RENDERER_LIBS} stb_image glm)
target_include_directories(framebuffer PRIVATE "${STB_DIR}")

# =============================================================================
# Hello Triangle D3D12
# =============================================================================
add_executable(hello-triangle-d3d12 src/Hello_Triangle_D3D12.cpp ${RENDERER_SRCS})
target_link_libraries(hello-triangle-d3d12 PRIVATE ${RENDERER_LIBS} stb_image glm)
target_include_directories(hello-triangle-d3d12 PRIVATE "${STB_DIR}")

