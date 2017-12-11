// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "atom_printer.h"
#include <sstream>
#include <iomanip>

using namespace std;

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

void write_simple_atom(const Atom &a, std::ostream &o)
{
    switch (a.m_type)
    {
        case T_NIL:  o << "nil";                                  break;
        case T_INT:  o << a.m_d.i;                                break;
        case T_DBL:  o << std::setprecision(15) << a.m_d.d;       break;
        case T_BOOL: o << (a.m_d.b ? "#true" : "#false");         break;
        case T_SYM:  o << a.m_d.sym->m_str;                       break;
        case T_KW:   o << a.m_d.sym->m_str << ":";                break;
        case T_STR:  dump_string_to_ostream(o, a.m_d.sym->m_str); break;
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
        o << "#<unprintable unknown?>";
        break;
    }
}
//---------------------------------------------------------------------------

void write_atom(const Atom &a, std::ostream &o)
{
    switch (a.m_type)
    {
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
            bool is_first = true;
            o << "{";
            ATOM_MAP_FOR(i, a.m_d.map)
            {
                if (is_first) is_first = false;
                else          o << " ";
                write_atom(MAP_ITER_KEY(i), o);
                o << " ";
                write_atom(MAP_ITER_VAL(i), o);
            }
            o << "}";
            break;
        }
        default:
            write_simple_atom(a, o);
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

void print_indent(std::stringstream &o, size_t indent)
{
    for (size_t j = 0; j < indent; j++)
        o << " ";
}
//---------------------------------------------------------------------------

void write_atom_pp_rec(const Atom &a, std::stringstream &o, size_t indent)
{
    switch (a.m_type)
    {
        case T_VEC:
        {
            if (a.size() <= 12)
            {
                write_atom(a, o);
                return;
            }

            AtomVec &v = *a.m_d.vec;
            o << "(";
            indent += 2;
            if (v.m_len > 0)
            {
                write_atom_pp_rec(v.m_data[0], o, indent);
                bool last_is_simple_and_short =
                    v.m_data[0].is_simple() && v.m_data[0].size() < 12;

                if (v.m_len > 1)
                {
                    for (size_t i = 1; i < v.m_len; i++)
                    {
                        if (i == 1
                            && last_is_simple_and_short
                            && !(v.m_data[i].is_simple()))
                        {
                            o << " ";
                        }
                        else
                        {
                            o << "\n";
                            print_indent(o, indent);
                        }
                        write_atom_pp_rec(v.m_data[i], o, indent);
                    }
                }
            }
            o << ")";
            break;
        }
        case T_MAP:
        {
            if (a.size() <= 6)
            {
                write_atom(a, o);
                return;
            }

            bool is_first = true;
            o << "{";
            indent += 2;
            ATOM_MAP_FOR(p, a.m_d.map)
            {
                o << "\n";
                print_indent(o, indent);
                write_atom_pp_rec(MAP_ITER_KEY(p), o, indent);
                if (!MAP_ITER_KEY(p).is_simple())
                {
                    o << "\n";
                    print_indent(o, indent);
                    write_atom_pp_rec(MAP_ITER_VAL(p), o, indent);
                }
                else
                {
                    o << " ";
                    size_t fs = MAP_ITER_KEY(p).size();
                    indent += fs + 1;
                    write_atom_pp_rec(MAP_ITER_VAL(p), o, indent);
                    indent -= fs + 1;
                }
            }
            o << "}";
            break;
        }
        default:
            write_simple_atom(a, o);
            break;
    }
}
//---------------------------------------------------------------------------

std::string write_atom_pp(const Atom &a)
{
    std::stringstream ss;
    if (a.size() > 12) write_atom_pp_rec(a, ss, 0);
    else               write_atom(a, ss);
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
