#pragma once

#include <string>
#include <unordered_map>

namespace lilvm
{
//---------------------------------------------------------------------------

struct Sym
{
    std::string m_str;

    Sym(const std::string &s) : m_str(s) { }
};
//---------------------------------------------------------------------------

class SymHash
{
    public:
        size_t operator()(const Sym *&s) const
        { return std::hash<std::string>()(s->m_str); }
};
//---------------------------------------------------------------------------

typedef std::unordered_map<std::string, Sym *>            StrSymMap;

// For mapping symbols to their position in an environment array.
typedef std::unordered_map<Sym *, size_t, SymHash>        SymEnvMap;

class SymTable
{
    private:
        uint64_t  m_next_sym_id;
        StrSymMap m_syms;

        SymTable(const SymTable &that) { }
        SymTable &operator=(const SymTable &) { }

    public:

        SymTable(uint64_t auto_assign_offs)
            : m_next_sym_id(auto_assign_offs + 1)
        {
        }

        ~SymTable()
        {
            for (auto &sym_pair : m_syms)
                delete sym_pair.second;
        }

        Sym *str2sym(const std::string &s)
        {
            auto it = m_syms.find(s);
            if (it == m_syms.end())
            {
                Sym *newsym = new Sym(s);
                m_syms.insert(
                    std::pair<std::string, Sym *>(s, newsym));
                return newsym;
            }
            else
                return it->second;
        }
};
//---------------------------------------------------------------------------

}
