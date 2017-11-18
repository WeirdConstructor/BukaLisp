set(DISCOUNT_PATH ${BKL_MOD_DIR}/discount/discount-2.2.2/)
add_library(bklisp_mod_discount STATIC
    ${DISCOUNT_PATH}/mkdio.c
    ${DISCOUNT_PATH}/markdown.c
    ${DISCOUNT_PATH}/dumptree.c
    ${DISCOUNT_PATH}/generate.c
    ${DISCOUNT_PATH}/resource.c
    ${DISCOUNT_PATH}/docheader.c
    ${DISCOUNT_PATH}/version.c
    ${DISCOUNT_PATH}/toc.c
    ${DISCOUNT_PATH}/css.c
    ${DISCOUNT_PATH}/xml.c
    ${DISCOUNT_PATH}/Csio.c
    ${DISCOUNT_PATH}/xmlpage.c
    ${DISCOUNT_PATH}/basename.c
    ${DISCOUNT_PATH}/emmatch.c
    ${DISCOUNT_PATH}/github_flavoured.c
    ${DISCOUNT_PATH}/setup.c
    ${DISCOUNT_PATH}/tags.c
    ${DISCOUNT_PATH}/html5.c
    ${DISCOUNT_PATH}/flags.c
)

target_include_directories(bklisp_mod_discount PUBLIC ${DISCOUNT_PATH})

if (WIN32)
    set(DWORD       DWORD)
    set(WORD        WORD)
#    add_executable(mktags WIN32 ${DISCOUNT_PATH}/mktags.c)
#    set_target_properties(mktags PROPERTIES LINK_FLAGS "/ENTRY:\"mainCRTStartup\" /SUBSYSTEM:CONSOLE")
else()
    set(DWORD       unsigned int)
    set(WORD        unsigned short)
endif()
set(TABSTOP 4)
configure_file(
    ${BKL_MOD_DIR}/discount/blocktags
    ${BKL_MOD_DIR}/discount/discount-2.2.2/blocktags
)
configure_file(
    ${BKL_MOD_DIR}/discount/config.h
    ${BKL_MOD_DIR}/discount/discount-2.2.2/config.h
)
configure_file(
    ${BKL_MOD_DIR}/discount/discount-2.2.2/mkdio.h.in
    ${BKL_MOD_DIR}/discount/discount-2.2.2/mkdio.h
)
configure_file(
    ${BKL_MOD_DIR}/discount/discount-2.2.2/version.c.in
    ${BKL_MOD_DIR}/discount/discount-2.2.2/version.c
)
