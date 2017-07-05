#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "atom.h"

namespace lilvm
{
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
//---------------------------------------------------------------------------

class SymTable
{
    private:
        StrSymMap m_syms;

        GC       *m_gc;

        SymTable(const SymTable &that) { }
        SymTable &operator=(const SymTable &) { }

    public:

        SymTable(GC *gc)
            : m_gc(gc)
        {
        }

        ~SymTable()
        {
        }

        Sym *str2sym(const std::string &s)
        {
            auto it = m_syms.find(s);
            if (it == m_syms.end())
            {
                Sym *newsym = m_gc->allocate_sym();
                newsym->m_str = s;
                m_syms.insert(std::pair<std::string, Sym *>(s, newsym));
                return newsym;
            }
            else
                return it->second;
        }
};
//---------------------------------------------------------------------------
typedef std::shared_ptr<SymTable>  SymTblPtr;

//---------------------------------------------------------------------------

}
