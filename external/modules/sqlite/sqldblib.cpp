/******************************************************************************
* Copyright (C) 2017 Weird Constructor
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#include "base/sqldb.h"
#include "rt/sqldblib.h"
#include "rt/lua_thread.h"
#include "rt/lua_thread_helper.h"

using namespace sqldb;
using namespace VVal;
using namespace std;

namespace lal_rt
{
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(sqldb_session,
"@sql:rt-sql procedure (sql-session _options-data_)\n\n"
"Returns a database handle for making SQL-Queries.\n"
"If there is an error, an exception will be thrown.\n"
"\n"
"    (let ((db (sql-session\n"
"               { :driver \"sqlite3\" :file \":memory:\" }))\n"
"           (sql [\"SELECT * FROM kunden\"]))\n"
"      (do ((ok  (sql-execute! db sql) (sql-next db))\n"
"           (row (sql-row db) (sql-row db)))\n"
"          (ok (sql-close db))\n"
"        (display row))\n"
"      (sql-destroy db))\n"
)
{
    Session *s = Session::connect(vv_args->_(0));
    LT->register_resource(s);
    return vv_ptr((void *) s, "sqldb::Session");
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(sqldb_execute_M,
"@sql procedure (sql-execute! _db-handle_ _sql-data-struct_)\n\n"
"Returns a boolean whether there are any results to be fetched.\n"
"If it returns `#false` there are no results to be fetched. But the\n"
"statement was executed successfully.\n"
"If there is an error, an exception will be thrown.\n"
"\n"
"   (let ((db (sql-session { #(...) }))\n"
"         (has-users (sql-execute! db [\"SELECT * FROM users\"])))\n"
"     (if has-users (display \"got users!\")\n"
"                   (display \"no users?!\")))\n"
)
{
    LTRES(s, 0, sqldb::Session);
    return vv_bool(s->execute(vv_args->_(1)));
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(sqldb_row,
"@sql procedure (sql-row _db-handle_)\n\n"
"Returns the columns of row as map, with the column names\n"
"in lowercase as keys.\n"
"If there is an error, an exception will be thrown.\n"
"\n"
"For usage, see `sql-session`.\n"
)
{
    LTRES(s, 0, sqldb::Session);
    return s->row();
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(sqldb_next,
"@sql procedure (sql-next _db-handle_)\n\n"
"Advances the cursor to the next result row. Returning `#true` if a next\n"
"row is available. `#false` if no further row is available.\n"
"If there is an error, an exception will be thrown.\n"
"\n"
"For usage, see `sql-session`.\n"
)
{
    LTRES(s, 0, sqldb::Session);
    return vv_bool(s->next());
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(sqldb_close,
"@sql procedure (sql-close _db-handle_)\n\n"
"Closes any open SQL statement.\n"
"If there is an error, an exception will be thrown.\n"
"\n"
"For usage, see `sql-session`.\n"
)
{
    LTRES(s, 0, sqldb::Session);
    s->close();
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(sqldb_destroy,
"@sql procedure (sql-destroy _db-handle_)\n\n"
"Destroys the database handle. Any further usage of it is an evil error!\n"
"\n"
"For usage, see `sql-session`.\n"
)
{
    LTRES(s, 0, sqldb::Session);
    LT->delete_resource((void *) s);
    delete s;
    return vv_undef();
}
//---------------------------------------------------------------------------

void init_sqldblib(LuaThread *t, Lua::Instance &lua)
{
    VV obj(vv_list() << vv_ptr(t, "LuaThread"));

    LUA_REG(lua, "sql", "session",   obj, sqldb_session);
    LUA_REG(lua, "sql", "executeM",  obj, sqldb_execute_M);
    LUA_REG(lua, "sql", "row",       obj, sqldb_row);
    LUA_REG(lua, "sql", "next",      obj, sqldb_next);
    LUA_REG(lua, "sql", "close",     obj, sqldb_close);
    LUA_REG(lua, "sql", "destroy",   obj, sqldb_destroy);
}

} // namespace lal_rt
