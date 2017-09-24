// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "atom.h"
#include "atom_printer.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

size_t AtomVec::s_alloc_count = 0;

//---------------------------------------------------------------------------

#if WITH_MEM_POOL
MemoryPool<Atom> g_atom_array_pool;
#endif

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
#if WITH_MEM_POOL
    m_data  = g_atom_array_pool.allocate(len);
#else
    m_data  = new Atom[len];
#endif
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
#if WITH_MEM_POOL
        m_data  = g_atom_array_pool.allocate(m_alloc);
#else
        m_data  = new Atom[m_alloc];
#endif
//        std::cout << "ALLOC:" << ((void *) this) << "@" << ((void *) m_data) << " ; " << m_alloc << std::endl;
        for (size_t i = 0; i < m_len; i++)
            m_data[i] = old_data[i];
//        std::cout << "DELETE AR " << ((void *) this) << "@" << ((void *) old_data) << std::endl;

#if WITH_MEM_POOL
        if (old_data)
            g_atom_array_pool.free(old_data);
#else
        if (old_data)
            delete[] old_data;
#endif
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

#if WITH_MEM_POOL
    if (m_data)
        g_atom_array_pool.free(m_data);
#else
    if (m_data)
        delete[] m_data;
#endif
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

GCRootRefPool::GCRootRef::~GCRootRef()
{
    m_pool.unreg(m_idx);
}
//---------------------------------------------------------------------------

Atom GC::get_statistics()
{
    AtomVec *v = this->allocate_vector(0);
    AtomVec *e = nullptr;

#define     BKLISP_GC_NEW_ST_ENTRY(name, num) \
    e = this->allocate_vector(0); \
    e->push(Atom(T_KW,  this->new_symbol(name))); \
    e->push(Atom(T_INT, num)); \
    v->push(Atom(T_VEC, e));

    BKLISP_GC_NEW_ST_ENTRY("alive-vectors", m_num_alive_vectors);
    BKLISP_GC_NEW_ST_ENTRY("alive-maps",    m_num_alive_maps);
    BKLISP_GC_NEW_ST_ENTRY("alive-syms",    m_num_alive_syms);

    size_t n_alive_vector_bytes = 0;
    AtomVec *alive_v = m_vectors;
    while (alive_v)
    {
        n_alive_vector_bytes +=
            sizeof(AtomVec) + alive_v->m_alloc * sizeof(Atom);
        alive_v = alive_v->m_gc_next;
    }

    size_t n_syms_size = 0;
    Sym *s = m_syms;
    while (s)
    {
        n_syms_size += sizeof(Sym) + s->m_str.size();
        s = s->m_gc_next;
    }

    size_t n_medium_bytes = 0;
    size_t n_small_bytes = 0;
    size_t n_medium = 0;
    size_t n_small  = 0;
    AtomVec *mv = m_medium_vectors;
    while (mv)
    {
        n_medium_bytes += sizeof(AtomVec) + mv->m_alloc * sizeof(Atom);
        n_medium++;
        mv = mv->m_gc_next;
    }

    mv = m_small_vectors;
    while (mv)
    {
        n_small_bytes += sizeof(AtomVec) + mv->m_alloc * sizeof(Atom);
        n_small++;
        mv = mv->m_gc_next;
    }

    BKLISP_GC_NEW_ST_ENTRY("medium-vector-pool",       n_medium);
    BKLISP_GC_NEW_ST_ENTRY("small-vector-pool",        n_small);

    BKLISP_GC_NEW_ST_ENTRY("small-vector-pool-bytes",  n_small_bytes);
    BKLISP_GC_NEW_ST_ENTRY("medium-vector-pool-bytes", n_medium_bytes);
    BKLISP_GC_NEW_ST_ENTRY("alive-vectors-bytes",      n_alive_vector_bytes);
    BKLISP_GC_NEW_ST_ENTRY("alive-syms-bytes",         n_syms_size);

    return Atom(T_VEC, v);
}
//---------------------------------------------------------------------------

AtomMapIterator::AtomMapIterator(Atom &map)
    : m_map(map),
      m_init(true),
      m_map_ref(map.m_d.map->m_map),
      m_iterator(m_map_ref.begin())
{
}
//---------------------------------------------------------------------------

bool AtomMapIterator::ok()
{
    return m_iterator != m_map_ref.end();
}
//---------------------------------------------------------------------------

void AtomMapIterator::next()
{
    if (m_init) { m_init = false; return; }
    m_iterator++;
}
//---------------------------------------------------------------------------

Atom AtomMapIterator::key()   { return m_iterator->first; }
Atom AtomMapIterator::value() { return m_iterator->second; }
//---------------------------------------------------------------------------

void AtomMapIterator::mark(GC *gc, uint8_t clr)
{
    UserData::mark(gc, clr);
    gc->mark_atom(m_map);
}
//---------------------------------------------------------------------------

BukaLISPException &BukaLISPException::push(Atom &err_stack)
{
    if (err_stack.m_type != T_VEC)
        return *this;
    for (size_t i = 0; i < err_stack.m_d.vec->m_len; i++)
    {
        push(
            err_stack.at(i).at(0).to_display_str(),
            err_stack.at(i).at(1).to_display_str(),
            err_stack.at(i).at(2).to_int(),
            err_stack.at(i).at(3).to_display_str());
    }
    return *this;
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
