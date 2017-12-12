// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "ports.h"
#include "runtime.h"
#include <fstream>

using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

Atom Port::read()
{
    if (m_istream)
    {
        std::string data(std::istreambuf_iterator<char>(*m_istream), {});
        return m_rt->read(m_file, data);
    }
    else
        return Atom();

}
//---------------------------------------------------------------------------

void Port::open_stdin()
{
    m_file = "<stdin>";
    m_istream = &cin;
}
//---------------------------------------------------------------------------

void Port::open_input_file(const string &path, bool is_text)
{
    m_file = path;
    m_fstream = new fstream;
    m_istream = (istream *) m_fstream;

    if (is_text)
        m_fstream->open(m_file.c_str(), fstream::in);
    else
        m_fstream->open(m_file.c_str(), fstream::in | fstream::binary);

    if (m_fstream->fail())
    {
        this->close();
        throw BukaLISPException("Couldn't open file '" + path + "'.");
        return;
    }
}
//---------------------------------------------------------------------------

void Port::close()
{
    if (m_fstream)
    {
        m_fstream->close();
        delete m_fstream;
    }

    m_fstream = nullptr;
    m_istream = nullptr;
    m_ostream = nullptr;
    m_file    = "";
}
//---------------------------------------------------------------------------

}

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
