set(BKL_MOD_DIR ${PROJECT_SOURCE_DIR}/external/modules/)
include(${BKL_MOD_DIR}/external_libs.cmake)

add_library(bklisp_module_support STATIC
    ${BKL_MOD_DIR}/vval.cpp
    ${BKL_MOD_DIR}/vval_util.cpp
    ${BKL_MOD_DIR}/../compat_endian.cpp
    ${BKL_MOD_DIR}/../JSON.cpp
    ${BKL_MOD_DIR}/bklisp_module_wrapper.cpp
)

add_library(bklisp_mod_util STATIC
    ${BKL_MOD_DIR}/util/crc.cpp
    ${BKL_MOD_DIR}/util/csv.cpp
    ${BKL_MOD_DIR}/util/utillib.cpp
)

add_library(bklisp_mod_sys  STATIC
    ${BKL_MOD_DIR}/sys/syslib.cpp
)

add_library(bklisp_mod_test STATIC
    ${BKL_MOD_DIR}/testlib.cpp
)

add_library(bklisp_mod_ev_loop STATIC
    ${BKL_MOD_DIR}/ev_loop/ev_loop_lib.cpp
)

#include(${BKL_MOD_DIR}/discount/discount_module.cmake)
include(${BKL_MOD_DIR}/sqldb/sqldb_module.cmake)
include(${BKL_MOD_DIR}/poco_http/http_module.cmake)
include(${BKL_MOD_DIR}/costraeng/costraeng_module.cmake)

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

#    bklisp_mod_test
#    bklisp_mod_util
#    bklisp_mod_sys
#    bklisp_mod_ev_loop
#    bklisp_mod_sqldb
#    bklisp_mod_http
#    bklisp_mod_costraeng

#    bklisp_mod_discount

    ${Boost_LIBRARIES}
    ${BOOST_SUPPORT_LIBS}
    ${POCO_LIBRARIES})
set_property(TARGET bklisp_mod_http PROPERTY CXX_STANDARD 11)
set_property(TARGET bukalisp_lib PROPERTY CXX_STANDARD 11)
