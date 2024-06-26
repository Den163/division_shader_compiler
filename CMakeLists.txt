CMAKE_MINIMUM_REQUIRED(VERSION 3.22.2)
project(division_shader_compiler LANGUAGES CXX)

include(FetchContent)
include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 17)

if(NOT DIVISION_SHADER_COMPILER_EXECUTABLE)
        set(ENABLE_GLSLANG_INSTALL)
endif()

FetchContent_Declare(
        glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang
        GIT_TAG 12.1.0
)

FetchContent_Declare(
        spirv-cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross
        GIT_TAG sdk-1.3.243.0
)

FetchContent_MakeAvailable(glslang)
FetchContent_MakeAvailable(spirv-cross)

add_library(division_shader_compiler src/interface.cpp)

target_include_directories(
        division_shader_compiler 
        PUBLIC 
        ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR})
target_link_libraries(
        division_shader_compiler
        glslang SPIRV glslang-default-resource-limits spirv-cross-cpp spirv-cross-msl)

target_link_directories(division_shader_compiler PUBLIC ${PROJECT_BINARY_DIR})
GENERATE_EXPORT_HEADER(division_shader_compiler EXPORT_MACRO_NAME DIVISION_EXPORT)

if(DIVISION_SHADER_COMPILER_EXECUTABLE)
        add_executable(division_shader_compiler_exec src/main.cpp)
        target_link_libraries(division_shader_compiler_exec division_shader_compiler)
else()
        install(TARGETS division_shader_compiler DESTINATION ${CMAKE_INSTALL_NAME_DIR})
endif()