#include "base/sqldb.h"
#include "rt/log.h"
#include "sqlite3/sqlite3.h"
#include "base/vval_util.h"

using namespace std;
using namespace VVal;

namespace sqldb
{
//---------------------------------------------------------------------------

Session *Session::connect(const VVal::VV &options)
{
    if (options->_s("driver") == "sqlite3")
    {
        SQLite3Session *s = new SQLite3Session;
        s->init(options);
        return s;
    }
    else
        throw DatabaseException("Unknown driver: " + options->_s("driver"));
}
//---------------------------------------------------------------------------

void SQLite3Session::init(const VVal::VV &options)
{
    m_file = options->_s("file");
    int r =
        sqlite3_open_v2(
            m_file.c_str(),
            &m_sqlite3,
              SQLITE_OPEN_READWRITE
            | SQLITE_OPEN_CREATE
            | SQLITE_OPEN_URI,
            NULL);

    if (r != SQLITE_OK)
    {
        L_ERROR << "DB: SQLITE3: init/connect for "
                << options
                << " failed: " << sqlite3_errmsg(m_sqlite3);

        sqlite3_close_v2(m_sqlite3);
        m_sqlite3 = 0;
        return;
    }

    L_INFO << "DB: SQLITE3: connected to file '" << m_file << "'";
}
//---------------------------------------------------------------------------

static void free_cpp_buf(void *ptr)
{
    if (ptr)
        delete[] (char *) ptr;
}
//---------------------------------------------------------------------------

bool SQLite3Session::execute(const VVal::VV &sqlTemplate)
{
    VV params(vv_list());
    stringstream ss;
    for (auto i : *sqlTemplate)
    {
        if (i->is_list() || i->is_map())
        {
            ss << "?";

            if (i->_(0)->is_bytes())
                params << i->_(0);
            else if (i->_(0)->is_string())
                params << i->_(0);
            else if (i->_(0)->is_undef())
                params << i->_(0);
            else if (i->_(0)->is_double())
                params << i->_(0);
            else if (i->_(0)->is_int())
                params << i->_(0);
            else if (i->_(0)->is_boolean())
                params << i->_(0);
            else if (i->_(0)->is_datetime())
                params << i->_(0);
            else
                params << vv(i->_s(0));
        }
        else if (i->is_boolean())
        {
            ss << "?";
            params << i;
        }
        else if (i->is_bytes())
        {
            ss << "?";
            params << i;
        }
        else if (i->is_int())
        {
            ss << "?";
            params << i;
        }
        else if (i->is_undef())
        {
            ss << "?";
            params << i;
        }
        else if (i->is_double())
        {
            ss << "?";
            params << i;
        }
        else if (i->is_datetime())
        {
            ss << "?";
            params << i;
        }
        else
        {
            ss << i->s();
        }

        ss << " ";
    }
    string sql = ss.str();

    m_stmt = 0;
    int r =
        sqlite3_prepare_v2(
            m_sqlite3, sql.c_str(), (int) sql.size(), &m_stmt, NULL);
    if (r != SQLITE_OK)
    {
        string err = "prepare: " + string(sqlite3_errstr(r));
        L_ERROR << "DB: SQLITE3: " << err << ", SQL=[" << sql << "]";
        this->close();
        throw DatabaseException(err);
    }

    int idx = 1;
    if (params->size() > 0)
    {
        for (auto p : *params)
        {
            r = SQLITE_OK;

            if (p->is_bytes())
            {
                size_t l = 0;
                char *buf = p->s_buffer(l);
                r = sqlite3_bind_blob(m_stmt, idx, buf, (int) l, &free_cpp_buf);
            }
            else if (p->is_double())
                r = sqlite3_bind_double(m_stmt, idx, p->d());
            else if (p->is_int())
                r = sqlite3_bind_int64(m_stmt, idx, p->i());
            else if (p->is_undef())
                r = sqlite3_bind_null(m_stmt, idx);
            else
            {
                size_t l = 0;
                char *buf = p->s_buffer(l);
                r = sqlite3_bind_text(m_stmt, idx, buf, (int) l, &free_cpp_buf);
            }

            if (r != SQLITE_OK)
            {
                string err = "bind: @" + to_string(idx) + ": "
                           + string(sqlite3_errstr(r));
                L_ERROR << "DB: SQLITE3: " << err << ", SQL=[" << sql << "]";
                this->close();
                throw DatabaseException(err);
            }

            idx++;
        }
    }

    return this->next();
}
//---------------------------------------------------------------------------

VVal::VV SQLite3Session::row()
{
    if (!m_stmt)
        return vv_undef();

    int cc = sqlite3_column_count(m_stmt);
    if (cc == 0)
        return vv_undef();

    VV row(vv_map());

    for (int i = 0; i < cc; i++)
    {
        const char *cname = sqlite3_column_name(m_stmt, i);
        int type = sqlite3_column_type(m_stmt, i);
        // L_TRACE << "SQL " << cname << " TYPE: " << type;
        VV value;
        switch (type)
        {
            case SQLITE_INTEGER:
            {
                value = vv((int64_t) sqlite3_column_int64(m_stmt, i));
                break;
            }
            case SQLITE_FLOAT:
            {
                value = vv(sqlite3_column_double(m_stmt, i));
                break;
            }
            case SQLITE_BLOB:
            {
                const void *c = sqlite3_column_blob(m_stmt, i);
                int len = sqlite3_column_bytes(m_stmt, i);
                value = vv_bytes(string((char *) c, (size_t) len));
                break;
            }
            case SQLITE_NULL:
            {
                value = vv_undef();
                break;
            }
            case SQLITE_TEXT:
            default:
            {
                const unsigned char *c = sqlite3_column_text(m_stmt, i);
                int len = sqlite3_column_bytes(m_stmt, i);
                value = vv(string((const char *) c, len));
                break;
            }
        }

        row << vv_kv(to_lower(string(cname, strlen(cname))), value);
    }

    return row;
}
//---------------------------------------------------------------------------

bool SQLite3Session::next()
{
    if (!m_stmt)
        return false;

    int r = sqlite3_step(m_stmt);
    if (r == SQLITE_DONE)
    {
        this->close();
        return true;
    }
    else if (r == SQLITE_ROW)
    {
        return true;
    }
    else
    {
        string err = "step: " + string(sqlite3_errmsg(m_sqlite3));
        L_ERROR << "DB: SQLITE3: " << err;
        this->close();
        throw DatabaseException(err);
    }
}
//---------------------------------------------------------------------------

void SQLite3Session::close()
{
    if (m_stmt)
    {
        sqlite3_finalize(m_stmt);
        m_stmt = 0;
    }
}
//---------------------------------------------------------------------------

SQLite3Session::~SQLite3Session()
{
    if (m_stmt)
        this->close();

    if (m_sqlite3)
        sqlite3_close_v2(m_sqlite3);
}
//---------------------------------------------------------------------------

} // namespace sqldb
