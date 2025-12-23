cmake_minimum_required(VERSION 3.20)
project(Graphic_Assignment2 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 让你之后如果需要也能生成 compile_commands.json（可选）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 关键：如果 glad.c 不存在，直接让 CMake 配置失败，避免“悄悄没编进来”
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c")
    message(FATAL_ERROR "glad.c not found at: ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c")
endif()

add_executable(Graphic_Assignment2
        src/main.cpp
        src/glad.c

        src/render/Shader.cpp
        src/render/Mesh.cpp
        src/render/LightingSystem.cpp
        src/render/TextureUtils.cpp

        src/core/Input.cpp

        src/scene/Camera.cpp
        src/scene/Player.cpp
        src/scene/Terrain.cpp


        src/environment/TimeOfDay.cpp
        src/environment/Sky.cpp
        src/environment/Sun.cpp
        src/environment/Environment.cpp
        src/environment/Water.cpp
        src/environment/Particles.cpp

        src/stb_image_impl.cpp
)

# 强制把 glad.c 加进同一个 target
target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c
)

# 强制 glad.c 按 C 编译
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c
        PROPERTIES LANGUAGE C)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        OpenGL::GL
)

target_include_directories(Graphic_Assignment2 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(glm CONFIG REQUIRED)
target_link_libraries(Graphic_Assignment2 PRIVATE glm::glm)