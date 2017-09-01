// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "atom_printer.h"
#include <sstream>
#include <iomanip>

namespace bukalisp
{
//---------------------------------------------------------------------------

void dump_string_to_ostream(std::ostream &out, const std::string &s)
{
    out << "\"";
    for (auto i : s)
    {
        if (i == '"')        out << "\\\"";
        else if (i == '\\')  out << "\\\\";
        else if (isgraph((unsigned char) i)) out << i;
        else if (i == ' ')   out << " ";
        else if (i == '\t')  out << "\\t";
        else if (i == '\n')  out << "\\n";
        else if (i == '\a')  out << "\\a";
        else if (i == '\b')  out << "\\b";
        else if (i == '\v')  out << "\\v";
        else if (i == '\f')  out << "\\f";
        else if (i == '\r')  out << "\\r";
        else                 out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (int) (unsigned char) i << ";" << std::dec;
    }
    out << "\"";
}
//---------------------------------------------------------------------------

void write_atom(const Atom &a, std::ostream &o)
{
    switch (a.m_type)
    {
        case T_NIL:  o << "nil";                                  break;
        case T_INT:  o << a.m_d.i;                                break;
        case T_DBL:  o << a.m_d.d;                                break;
        case T_BOOL: o << (a.m_d.b ? "#true" : "#false");         break;
        case T_SYM:  o << a.m_d.sym->m_str;                       break;
        case T_KW:   o << a.m_d.sym->m_str << ":";                break;
        case T_STR:  dump_string_to_ostream(o, a.m_d.sym->m_str); break;
        case T_VEC:
        {
            AtomVec &v = *a.m_d.vec;
            o << "(";
            for (size_t i = 0; i < v.m_len; i++)
            {
                if (i > 0)
                    o << " ";
                write_atom(v.m_data[i], o);
            }
            o << ")";
            break;
        }
        case T_MAP:
        {
            AtomMap &m = *a.m_d.map;
            bool is_first = true;
            o << "{";
            for (auto p : m.m_map)
            {
                if (is_first) is_first = false;
                else          o << " ";
                write_atom(p.first, o);
                o << " ";
                write_atom(p.second, o);
            }
            o << "}";
            break;
        }
        case T_SYNTAX:
            o << "#<syntax:" << a.m_d.sym->m_str << ">";
            break;
        case T_PRIM:
            o << "#<primitive:" << ((void *) a.m_d.func) << ">";
            break;
        case T_CLOS:
            o << "#<closure:" << ((void *) a.m_d.vec) << ">";
            break;
        case T_C_PTR:
            o << "#<cpointer:" << a.m_d.ptr << ">";
            break;
        case T_UD:
            {
                std::string s = a.m_d.ud->as_string();
                if (a.m_d.ud) o << s;
                else          o << "#<userdata:null>";
                break;
            }
        default:
            o << "#<unprintable unknown?>";
            break;
    }
}
//---------------------------------------------------------------------------

std::string write_atom(const Atom &a)
{
    std::stringstream ss; 
    write_atom(a, ss);
    return ss.str();
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
