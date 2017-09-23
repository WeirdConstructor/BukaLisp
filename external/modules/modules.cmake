include(external/modules/external_libs.cmake)

add_library(bklisp_module_support STATIC
    external/modules/vval.cpp
    external/modules/vval_util.cpp
    external/modules/bklisp_module_wrapper.cpp
)
#add_library(bklisp_mod_sqlite STATIC
#    external/modules/sqlite.cpp
#)
add_library(bklisp_mod_util STATIC
    external/modules/util/crc.cpp
    external/modules/util/csv.cpp
    external/modules/util/utillib.cpp
)

add_library(bklisp_mod_test STATIC
    external/modules/testlib.cpp
)

if (WIN32)
    if (MSVC)
        set(PLATFORM_LIBS_ADD) # -lws2_32)
    else()
        set(PLATFORM_LIBS_ADD -lws2_32)
    endif()
else()
    set(PLATFORM_LIBS_ADD -ldl)
endif()
target_link_libraries(bklisp_module_support ${PLATFORM_LIBS_ADD})
target_link_libraries(bukalisp_lib
    bklisp_module_support

    bklisp_mod_test
    bklisp_mod_util

    ${Boost_LIBRARIES}
    ${BOOST_SUPPORT_LIBS}
    ${POCO_LIBRARIES})
