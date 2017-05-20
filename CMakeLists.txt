cmake_minimum_required(VERSION 2.6)
project(LALRT)
cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0008 OLD)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(lilvm_lib STATIC
    external/endian.cpp
    external/JSON.cpp
    src/lilvm.cpp
)

add_executable(lilvm main.cpp)
TARGET_LINK_LIBRARIES(lilvm_lib)
target_link_libraries(lilvm lilvm_lib)
include_directories(src/ external/)

set_target_properties(lilvm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}
)
