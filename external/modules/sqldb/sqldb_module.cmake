add_library(bklisp_mod_sqldb STATIC
    ${BKL_MOD_DIR}/sqldb/sqlite3/sqlite3.c
    ${BKL_MOD_DIR}/sqldb/sqldb.cpp
    ${BKL_MOD_DIR}/sqldb/sqldblib.cpp
)

target_include_directories(bklisp_mod_sqldb PUBLIC ${BKL_MOD_DIR}/sqldb/sqlite3/)
