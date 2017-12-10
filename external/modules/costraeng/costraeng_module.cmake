if (WIN32)
    set(SDL2_PATH     ${BKL_MOD_DIR}/costraeng/SDL2-devel-2.0.7/)
    set(SDL2_img_PATH ${BKL_MOD_DIR}/costraeng/SDL2_image-2.0.2/)
    set(SDL2_mix_PATH ${BKL_MOD_DIR}/costraeng/SDL2_mixer-2.0.2/)
    set(SDL2_lib_dirs ${SDL2_PATH}/lib/x86/
                      ${SDL2_img_PATH}/lib/x86/
                      ${SDL2_mix_PATH}/lib/x86/)

    find_library(SDL2_core  NAMES SDL2       HINTS ${SDL2_lib_dirs})
    find_library(SDL2_image NAMES SDL2_image HINTS ${SDL2_lib_dirs})
    find_library(SDL2_mixer NAMES SDL2_mixer HINTS ${SDL2_lib_dirs})

    set(SDL2_LIBRARIES
        ${SDL2_core}
        ${SDL2_mixer}
        ${SDL2_image})

    set(SDL2_INCLUDE_DIRS
        ${SDL2_PATH}/include/
        ${SDL2_img_PATH}/include/
        ${SDL2_mix_PATH}/include/)
else()
    find_package(SDL2 REQUIRED)
endif()


add_library(bklisp_mod_costraeng STATIC
   ${BKL_MOD_DIR}/costraeng/costraenglib.cpp
)

target_link_libraries(bklisp_mod_costraeng ${SDL2_LIBRARIES})
include_directories(bklisp_mod_costraeng ${SDL2_INCLUDE_DIRS})
