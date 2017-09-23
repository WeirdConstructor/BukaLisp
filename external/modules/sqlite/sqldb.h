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

#pragma once

#include <string>
#include "vval.h"

extern "C"
{
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
}

namespace sqldb
{

class DatabaseException : public std::exception
{
    private:
        std::string m_msg;
    public:
        DatabaseException(const std::string &msg) : m_msg(msg) {}

        virtual const char *what() const noexcept { return m_msg.c_str(); }
};
//---------------------------------------------------------------------------

class Session
{
    public:
        static Session *connect(const VVal::VV &options);

        Session() { }
        virtual ~Session() { }
        virtual void init(const VVal::VV &options) = 0;
        virtual bool execute(const VVal::VV &sqlTemplate) = 0;
        virtual VVal::VV row() = 0;
        virtual bool next() = 0;
        virtual void close() = 0;
};
//---------------------------------------------------------------------------

class SQLite3Session : public Session
{
    private:
        sqlite3        *m_sqlite3;
        sqlite3_stmt   *m_stmt;
        std::string     m_file;

    public:
        SQLite3Session() : m_sqlite3(0), m_stmt(0) { }
        virtual ~SQLite3Session();

        virtual void init(const VVal::VV &options);
        virtual bool execute(const VVal::VV &sqlTemplate);
        virtual VVal::VV row();
        virtual bool next();
        virtual void close();
};
//---------------------------------------------------------------------------

};
