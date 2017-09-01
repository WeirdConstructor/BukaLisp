// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "atom.h"

namespace bukalisp
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
