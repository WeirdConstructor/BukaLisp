// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include <iostream>
#include <string>

namespace bukalisp
{
//---------------------------------------------------------------------------

class GC;

struct Atom;

class UserData
{
    public:
        uint8_t   m_gc_color;
        UserData *m_gc_next;

        UserData()
            : m_gc_color(), m_gc_next(nullptr)
        {
//            std::cout << "NEW USERDATA" << this << std::endl;
        }

        virtual std::string type() { return "unknown"; }

        virtual void to_atom(Atom &a) { }

        virtual std::string as_string(bool pretty = false)
        {
            return std::string("#<userdata:unknown>");
        }
        virtual void mark(GC *gc, uint8_t clr)
        {
            m_gc_color = clr;
        }

        virtual ~UserData()
        {
        }
};
//---------------------------------------------------------------------------

Atom expand_userdata_to_atoms(GC *gc, Atom in);
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
