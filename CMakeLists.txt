cmake_minimum_required(VERSION 3.14)
project(Compresser C)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        master # Puoi bloccare una versione specifica, es. "5.0"
)
FetchContent_MakeAvailable(raylib)

add_executable(${PROJECT_NAME} src/main.c)

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

target_link_libraries(${PROJECT_NAME} PRIVATE m)