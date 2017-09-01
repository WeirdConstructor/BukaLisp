// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "atom.h"
#include "atom_printer.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

size_t AtomVec::s_alloc_count = 0;

//---------------------------------------------------------------------------

std::string Atom::to_write_str() const
{
     return write_atom(*this);
}

//---------------------------------------------------------------------------
std::string Atom::to_display_str() const
{
    return m_type == T_SYM ? m_d.sym->m_str
         : m_type == T_KW  ? m_d.sym->m_str
         : m_type == T_STR ? m_d.sym->m_str
         : write_atom(*this);
}
//---------------------------------------------------------------------------

size_t AtomHash::operator()(const Atom &a) const
{
    // TODO: use X macro
    switch (a.m_type)
    {
        case T_NIL:
        case T_INT:
            return std::hash<int64_t>()(a.m_d.i);

        case T_BOOL:
            return std::hash<bool>()(a.m_d.b);

        case T_DBL:  return std::hash<double>()(a.m_d.d);

        case T_PRIM: return std::hash<void *>()((void *) a.m_d.func);

        case T_VEC:
        case T_CLOS:
            return std::hash<void *>()((void *) a.m_d.vec);

        case T_MAP:
            return std::hash<void *>()((void *) a.m_d.map);

        case T_SYNTAX:
        case T_SYM:
        case T_KW:
        case T_STR:
            return std::hash<std::string>()(a.m_d.sym->m_str);

        case T_UD:
            return std::hash<void *>()((void *) a.m_d.ud);
    }

    return std::hash<void *>()((void *) a.m_d.vec);
}
//---------------------------------------------------------------------------

void AtomVec::alloc(size_t len)
{
    m_alloc = len;
    m_len   = 0;
    m_data  = new Atom[len];
//    std::cout << "ALLOC:" << ((void *) this) << "@" << ((void *) m_data) << std::endl;
}
//---------------------------------------------------------------------------

void AtomVec::init(uint8_t current_gc_color, size_t len)
{
    m_len      = len;
    m_gc_color = current_gc_color;

    for (size_t i = 0; i < m_len; i++)
    {
        m_data[i].m_type = T_NIL;
        m_data[i].m_d.i  = 0;
    }
}
//---------------------------------------------------------------------------

Atom AtomVec::at(size_t idx)
{
    if (idx >= m_len)
        return Atom();
    return m_data[idx];
}
//---------------------------------------------------------------------------

Atom *AtomVec::first()
{
    if (m_len <= 0) return nullptr;
    return m_data;
}
//---------------------------------------------------------------------------

Atom *AtomVec::last()
{
    if (m_len <= 0) return nullptr;
    return &(m_data[m_len - 1]);
}
//---------------------------------------------------------------------------

void AtomVec::pop()
{
    if (m_len > 0) m_len--;
}
//---------------------------------------------------------------------------

void AtomVec::set(size_t idx, Atom &a)
{
    if (idx >= m_len)
        check_size(idx);
//    std::cout << "SET @(" << idx << ")" << ((void *) m_data) << std::endl;
    m_data[idx] = a;
}
//---------------------------------------------------------------------------

void AtomVec::push(const Atom &a)
{
    size_t new_idx = m_len;
    check_size(new_idx);
//    std::cout << "PUSH @(" << new_idx << ")" << ((void *) m_data) << std::endl;
    m_data[new_idx] = a;
}
//---------------------------------------------------------------------------

void AtomVec::check_size(size_t idx)
{
    if (idx >= m_alloc)
    {
        Atom *old_data = m_data;
        m_alloc = idx * 2;
        m_data = new Atom[m_alloc];
//        std::cout << "ALLOC:" << ((void *) this) << "@" << ((void *) m_data) << " ; " << m_alloc << std::endl;
        for (size_t i = 0; i < m_len; i++)
            m_data[i] = old_data[i];
//        std::cout << "DELETE AR " << ((void *) this) << "@" << ((void *) old_data) << std::endl;
        if (old_data)
            delete[] old_data;
    }
    else
    {
        for (size_t i = m_len; i < (idx + 1); i++)
            m_data[i] = Atom();
    }

    m_len = idx + 1;
}
//---------------------------------------------------------------------------

AtomVec::~AtomVec()
{
    s_alloc_count--;

    if (m_data)
        delete[] m_data;
}
//---------------------------------------------------------------------------

void AtomMap::set(Sym *s, Atom &a)
{
    Atom sym(T_SYM, s);
    m_map[sym] = a;
}
//---------------------------------------------------------------------------

void AtomMap::set(const Atom &k, Atom &a)
{
    m_map[k] = a;
}
//---------------------------------------------------------------------------

Atom AtomMap::at(const Atom &k)
{
    auto it = m_map.find(k);
    if (it == m_map.end()) return Atom();
    return it->second;
}
//---------------------------------------------------------------------------

Atom AtomMap::at(const Atom &k, bool &defined)
{
    defined = false;
    auto it = m_map.find(k);
    if (it == m_map.end()) return Atom();
    defined = true;
    return it->second;
}
//---------------------------------------------------------------------------

ExternalGCRoot::ExternalGCRoot(GC *gc)
    : m_gc(gc)
{
}
//---------------------------------------------------------------------------

void ExternalGCRoot::init()
{
    m_gc->add_external_root(this);
}
//---------------------------------------------------------------------------

ExternalGCRoot::~ExternalGCRoot()
{
    m_gc->remove_external_root(this);
}
//---------------------------------------------------------------------------

};

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
