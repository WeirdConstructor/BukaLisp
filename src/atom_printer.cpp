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

void write_simple_atom(const Atom &a, std::ostream &o, bool pretty)
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
                std::string s = a.m_d.ud->as_string(pretty);
                if (a.m_d.ud) o << s;
                else          o << "#<userdata:null>";
                break;
            }
        o << "#<unprintable unknown?>";
        break;
    }
}
//---------------------------------------------------------------------------

void fill_ref_map(const Atom &a, AtomMap &refmap, AtomMap &idxmap, int64_t &curidx, bool ordered_maps)
{
    switch (a.m_type)
    {
        case T_VEC:
        {
            if (refmap.at(a).m_type != T_NIL)
            {
                if (idxmap.at(a).m_type == T_NIL)
                    idxmap.set(a, Atom(T_INT, curidx++));
                break;
            }
            else
                refmap.set(a, Atom(T_INT, 1));

            AtomVec &v = *a.m_d.vec;
            for (size_t i = 0; i < v.m_len; i++)
            {
                fill_ref_map(v.m_data[i], refmap, idxmap, curidx, ordered_maps);
            }
            break;
        }
        case T_MAP:
        {
            if (refmap.at(a).m_type != T_NIL)
            {
                if (idxmap.at(a).m_type == T_NIL)
                    idxmap.set(a, Atom(T_INT, curidx++));
                break;
            }
            else
                refmap.set(a, Atom(T_INT, 1));

            if (ordered_maps)
            {
                AtomVec map_keys;
                ATOM_MAP_FOR(i, a.m_d.map)
                {
                    map_keys.push(MAP_ITER_KEY(i));
                }
                map_keys.sort();
                for (size_t i = 0; i < map_keys.m_len; i++)
                {
                    fill_ref_map(map_keys.m_data[i], refmap, idxmap, curidx, ordered_maps);
                    Atom val = a.m_d.map->at(map_keys.m_data[i]);
                    fill_ref_map(val, refmap, idxmap, curidx, ordered_maps);
                }
            }
            else
            {
                ATOM_MAP_FOR(i, a.m_d.map)
                {
                    fill_ref_map(MAP_ITER_KEY(i), refmap, idxmap, curidx, ordered_maps);
                    fill_ref_map(MAP_ITER_VAL(i), refmap, idxmap, curidx, ordered_maps);
                }
            }
            break;
        }
        default:
            break;
    }
}
//---------------------------------------------------------------------------

void write_atom(const Atom &a, std::ostream &o, AtomMap &idxmap)
{
#define PRINT_INDEX_OR_BREAK(idxmap, atm)         \
    Atom ma = (idxmap).at((atm));                 \
    if (ma.m_type == T_INT)                       \
    {                                             \
        if (ma.m_d.i > 0)                         \
        {                                         \
            o << "#" << ma.m_d.i << "=";          \
            idxmap.set(a, Atom(T_INT, -ma.m_d.i));\
        }                                         \
        else                                      \
        {                                         \
            o << "#" << -ma.m_d.i << "#";         \
            break;                                \
        }                                         \
    }

    switch (a.m_type)
    {
        case T_VEC:
        {
            PRINT_INDEX_OR_BREAK(idxmap, a);

            AtomVec &v = *a.m_d.vec;
            o << "(";
            for (size_t i = 0; i < v.m_len; i++)
            {
                if (i > 0)
                    o << " ";
                write_atom(v.m_data[i], o, idxmap);
            }
            o << ")";
            break;
        }
        case T_MAP:
        {
            PRINT_INDEX_OR_BREAK(idxmap, a);

            bool is_first = true;
            o << "{";
            ATOM_MAP_FOR(i, a.m_d.map)
            {
                if (is_first) is_first = false;
                else          o << " ";
                write_atom(MAP_ITER_KEY(i), o, idxmap);
                o << " ";
                write_atom(MAP_ITER_VAL(i), o, idxmap);
            }
            o << "}";
            break;
        }
        default:
            write_simple_atom(a, o, false);
            break;
    }
}
//---------------------------------------------------------------------------

void write_atom(const Atom &a, std::ostream &o)
{
    AtomMap m;
    AtomMap i;
    int64_t idx = 1;
    fill_ref_map(a, m, i, idx, false);
    write_atom(a, o, i);
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

void write_atom_pp_rec(const Atom &a, std::stringstream &o, size_t indent, AtomMap &idxmap)
{
    switch (a.m_type)
    {
        case T_VEC:
        {
            if (a.size() <= 12)
            {
                write_atom(a, o, idxmap);
                return;
            }

            PRINT_INDEX_OR_BREAK(idxmap, a);

            AtomVec &v = *a.m_d.vec;
            o << "(";
            indent += 2;
            if (v.m_len > 0)
            {
                write_atom_pp_rec(v.m_data[0], o, indent, idxmap);
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
                        write_atom_pp_rec(v.m_data[i], o, indent, idxmap);
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
                write_atom(a, o, idxmap);
                return;
            }

            PRINT_INDEX_OR_BREAK(idxmap, a);

            bool is_first = true;
            o << "{";
            indent += 2;

            AtomVec map_keys;
            ATOM_MAP_FOR(p, a.m_d.map)
            {
                map_keys.push(MAP_ITER_KEY(p));
            }
            map_keys.sort();

            for (size_t ki = 0; ki < map_keys.m_len; ki++)
            {
                Atom key = map_keys.m_data[ki];
                Atom val = a.m_d.map->at(key);

                o << "\n";
                print_indent(o, indent);
                write_atom_pp_rec(key, o, indent, idxmap);
                if (!key.is_simple())
                {
                    o << "\n";
                    print_indent(o, indent);
                    write_atom_pp_rec(val, o, indent, idxmap);
                }
                else
                {
                    o << " ";
                    size_t fs = key.size();
                    indent += fs + 1;
                    write_atom_pp_rec(val, o, indent, idxmap);
                    indent -= fs + 1;
                }
            }
            o << "}";
            break;
        }
        default:
            write_simple_atom(a, o, true);
            break;
    }
}
//---------------------------------------------------------------------------


std::string write_atom_pp(const Atom &a)
{
    std::stringstream ss;
    if (a.size() > 12)
    {
        AtomMap m;
        AtomMap i;
        int64_t idx = 1;
        fill_ref_map(a, m, i, idx, true);
        write_atom_pp_rec(a, ss, 0, i);
    }
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
