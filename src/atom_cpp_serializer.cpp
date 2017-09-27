// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "atom_cpp_serializer.h"
#include "buklivm.h"
#include <unordered_map>
#include <sstream>
#include <iomanip>

using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

class Atom2CPP
{
    private:
//        unordered_map<string, Atom>     m_?;
        size_t                          m_varcnt;
//        vector<pair<string, Atom>>      m_to_serialize_atoms;

    public:
        Atom2CPP() : m_varcnt(0)
        {
        }
//
//        void push(const string &name, const Atom &a)
//        {
//            m_to_serialize_atoms.push_back(pair<string, Atom>(name, a));
//        }

        string genname()
        {
            return string("a_") + to_string(m_varcnt++);
        }

        void write_string_decl(
            const string &name,
            const string &type,
            const string &value,
            ostream &o)
        {
            o << "Atom " << name << "(" << type << ");";
            o << name << ".m_d.sym = gc.new_symbol(";
            o << "\"";
            for (auto i : value)
            {
                if (i == '"')        o << "\\\"";
                else if (i == '\\')  o << "\\\\";
                else if (isgraph((unsigned char) i)) o << i;
                else if (i == ' ')   o << " ";
                else if (i == '\t')  o << "\\t";
                else if (i == '\n')  o << "\\n";
                else if (i == '\a')  o << "\\a";
                else if (i == '\b')  o << "\\b";
                else if (i == '\v')  o << "\\v";
                else if (i == '\f')  o << "\\f";
                else if (i == '\r')  o << "\\r";
                else                 o << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (int) (unsigned char) i << "\"\"" << std::dec;
            }
            o << "\");\n";
        }
        //---------------------------------------------------------------------------


        void write_decl(
            const string &name,
            const string &type,
            const string &m_d_name,
            const string &value,
            ostream &o)
        {
            o << "Atom " << name << "(" << type << "); "
              << name << ".m_d." << m_d_name << " = " << value << ";\n";
        }

        string write(const string &func_name, const Atom &a)
        {
            std::stringstream ss;
            ss << "Atom " << func_name << "(bukalisp::GC &gc)\n{\n";
            string nn = write_atom(a, ss);
            ss << "return " << nn << ";\n";
            ss << "}\n";
            return ss.str();
        }

        string write_atom(const Atom &a, std::ostream &o)
        {
            string nn = genname();
            switch (a.m_type)
            {
                case T_NIL:  write_decl(nn, "T_NIL", "i", "0", o);                break;
                case T_INT:  write_decl(nn, "T_INT", "i", to_string(a.m_d.i), o); break;
                case T_DBL:  write_decl(nn, "T_DBL", "d", to_string(a.m_d.d), o); break;
                case T_BOOL: write_decl(nn, "T_BOOL","b", to_string(a.m_d.b), o); break;
                case T_SYM:  write_string_decl(nn, "T_SYM", a.m_d.sym->m_str, o); break;
                case T_KW:   write_string_decl(nn, "T_KW",  a.m_d.sym->m_str, o); break;
                case T_STR:  write_string_decl(nn, "T_STR", a.m_d.sym->m_str, o); break;
                case T_VEC:
                {
                    AtomVec &v = *a.m_d.vec;
                    o << "Atom " << nn
                      << "(T_VEC, gc.allocate_vector(" << v.m_len << "));\n";
                    for (size_t i = 0; i < v.m_len; i++)
                    {
                        string ne = write_atom(v.m_data[i], o);
                        o << nn << ".m_d.vec->push(" << ne << ");\n";
                    }
                    break;
                }
                case T_MAP:
                {
                    AtomMap &m = *a.m_d.map;
                    o << "Atom " << nn
                      << "(T_MAP, gc.allocate_map());\n";
                    for (auto p : m.m_map)
                    {
                        string nk = write_atom(p.first, o);
                        string nv = write_atom(p.second, o);
                        o << nn
                          << ".m_d.vec->set(" << nk << ", " << nv << ");\n";
                    }
                    break;
                }
                case T_SYNTAX:
                    throw BukaLISPException("atom2cpp", "", 0, "write_atom",
                                            "Can't serialize T_SYNTAX");
                case T_PRIM:
                    throw BukaLISPException("atom2cpp", "", 0, "write_atom",
                                            "Can't serialize T_PRIM");
                case T_CLOS:
                    throw BukaLISPException("atom2cpp", "", 0, "write_atom",
                                            "Can't serialize T_CLOS");
                case T_C_PTR:
                    throw BukaLISPException("atom2cpp", "", 0, "write_atom",
                                            "Can't serialize T_C_PTR");
                case T_UD:
                {
                    if (a.m_d.ud->type() != "VM-PROG")
                    {
                        throw BukaLISPException(
                            "atom2cpp", "", 0, "write_atom",
                            "Can only serialize VM-PROG T_UD");
                    };
                    PROG *prog = dynamic_cast<PROG *>(a.m_d.ud);

                    string a_d_m = write_atom(prog->m_debug_info_map, o);
                    o << "PROG *" << nn << "_ud = new PROG(gc, "
                      << prog->m_atom_data_len
                      << ", "
                      << prog->m_instructions_len << ");\n";

                    for (size_t i = 0; i < prog->m_atom_data_len; i++)
                    {
                        string ne = write_atom(prog->m_atom_data[i], o);
                        o << nn << "_ud->m_atom_data[" << i << "] = "
                          << ne << ";\n";
                    }

                    for (size_t i = 0; i < prog->m_instructions_len; i++)
                    {
                        o << "INST &" << nn << "_i_" << i << " = "
                          << nn << "_ud->m_instructions[" << i << "]";
                        o << nn << "_i_" << i << ".o  = " << prog->m_instructions[i].o  << ";\n";
                        o << nn << "_i_" << i << ".oe = " << prog->m_instructions[i].oe << ";\n";
                        o << nn << "_i_" << i << ".a  = " << prog->m_instructions[i].a  << ";\n";
                        o << nn << "_i_" << i << ".b  = " << prog->m_instructions[i].b  << ";\n";
                        o << nn << "_i_" << i << ".ae = " << prog->m_instructions[i].ae << ";\n";
                        o << nn << "_i_" << i << ".be = " << prog->m_instructions[i].be << ";\n";
                    }

                    o << "gc.reg_userdata(" << nn << "_ud);\n";
                    o << "Atom " << nn << "(T_UD); " << nn << ".m_d.ud = " << nn << "_ud;\n";


                    break;
                }
                default:
                    throw BukaLISPException("atom2cpp", "", 0, "write_atom",
                                            "Can't serialize unknown");
            }
            return nn;
        }

};
//---------------------------------------------------------------------------

std::string atom2cpp(const std::string &func_name, const Atom &a)
{
    Atom2CPP a2c;
    return a2c.write(func_name, a);
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
