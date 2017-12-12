// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include "atom.h"

namespace bukalisp
{

struct Runtime;

class Port : public UserData
{
    private:
        Runtime      *m_rt;
        std::string   m_file;
        std::ostream *m_ostream;
        std::istream *m_istream;
        std::fstream *m_fstream;

        Port() : m_rt(0) { }

    public:
        Port(Runtime *rt)
            : m_ostream(nullptr),
              m_istream(nullptr),
              m_fstream(nullptr),
              m_rt(rt)
        {
        }

        void open_input_file(const std::string &path, bool is_text);
        void open_stdin();
        void close();

        Atom read();

        virtual std::string type() { return "Port"; }
        virtual std::string as_string()
        {
            return "#<port:" + std::to_string((uint64_t) this) + ">";
        }

        virtual void mark(GC *gc, uint8_t clr) { UserData::mark(gc, clr); }

        virtual ~Port() { this->close(); }
};

}
//---------------------------------------------------------------------------

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
