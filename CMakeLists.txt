cmake_minimum_required(VERSION 2.6)
project(BukaLISP)
cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0008 OLD)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(bukalisp_lib STATIC
    external/compat_endian.cpp
    external/JSON.cpp
    src/atom_printer.cpp
    src/atom.cpp
    src/ports.cpp
    src/atom_cpp_serializer.cpp
    src/atom_userdata.cpp
    src/interpreter.cpp
    src/buklivm.cpp
    src/runtime.cpp
    src/util.cpp
    src/mempool.cpp
)

add_executable(bklisp main.cpp)

target_link_libraries(bklisp bukalisp_lib)

include(external/modules/modules.cmake)

include_directories(src/ external/)

set_target_properties(bklisp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}
)
