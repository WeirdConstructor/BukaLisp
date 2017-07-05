#include "atom_printer.h"
#include <sstream>
#include <iomanip>

namespace lilvm
{
//---------------------------------------------------------------------------

void dump_string_to_ostream(std::ostream &out, const std::string &s)
{
    out << "\"";
    for (auto i : s)
    {
        if (i == '"')        out << "\\\"";
        else if (isgraph((unsigned char) i)) out << i;
        else if (i == ' ')   out << " ";
        else if (i == '\t')  out << "\\t";
        else if (i == '\n')  out << "\\n";
        else if (i == '\v')  out << "\\v";
        else if (i == '\f')  out << "\\f";
        else if (i == '\r')  out << "\\r";
        else                 out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (int) (unsigned char) i << std::dec;
    }
    out << "\"";
}
//---------------------------------------------------------------------------

void write_atom(Atom &a, std::ostream &o)
{
    switch (a.m_type)
    {
        case T_NIL:  o << "nil";                                  break;
        case T_INT:  o << a.m_d.i;                                break;
        case T_DBL:  o << a.m_d.d;                                break;
        case T_BOOL: o << (a.m_d.b ? "#t" : "#f");                break;
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
                dump_string_to_ostream(o, p.first);
                o << " ";
                write_atom(p.second, o);
            }
            o << "}";
            break;
        }
        default:
            o << "#<unprintable unknown?>";
            break;
    }
}
//---------------------------------------------------------------------------

std::string write_atom(Atom &a)
{
    std::stringstream ss;
    write_atom(a, ss);
    return ss.str();
}
//---------------------------------------------------------------------------

}
