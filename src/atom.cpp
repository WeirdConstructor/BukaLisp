// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "atom.h"
#include "atom_printer.h"
#include <sstream>
#include "parse_util.h"


using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

size_t AtomVec::s_alloc_count = 0;

//---------------------------------------------------------------------------

const size_t HASH_TABLE_SIZES[] = {
    7,          // Used only by "bklisp tests"
    11,         // Used only by "bklisp tests"
    23,         // Used only by "bklisp tests"
    53,         // First size the table actually grows to, as the
    97,         // initial included table size is 23!
    193,
    389,
    769,
    1543,
    3079,
    6151,
    12289,
    24593,
    49157,
    98317,
    196613,
    393241,
    786433,
    1572869,
    3145739,
    6291469,
    12582917,
    25165843,
    50331653,
    100663319,
    201326611,
    402653189,
    805306457,
    1610612741
};
//---------------------------------------------------------------------------

#if WITH_MEM_POOL
thread_local MemoryPool<Atom> g_atom_array_pool;
#endif

//---------------------------------------------------------------------------

size_t count_elements(const Atom &a)
{
    AtomMap refmap;
    std::vector<Atom> to_count;
    to_count.push_back(a);

    size_t s = 0;

    while (!to_count.empty())
    {
        Atom at = to_count.back();
        to_count.pop_back();

        if (at.m_type == T_VEC || at.m_type == T_MAP)
        {
            if (refmap.at(at).m_type != T_NIL)
            {
                s++;
                continue;
            }
            else
                refmap.set(at, Atom(T_INT, 1));
        }

        switch (at.m_type)
        {
            case T_VEC:
                s++;
                for (size_t i = 0; i < at.m_d.vec->m_len; i++)
                    to_count.push_back(at.m_d.vec->m_data[i]);
                break;
            case T_MAP:
                {
                    s++;
                    ATOM_MAP_FOR(i, at.m_d.map)
                    {
                        to_count.push_back(MAP_ITER_KEY(i));
                        to_count.push_back(MAP_ITER_VAL(i));
                    }
                }
                break;
            case T_UD:
                {
                    Atom a;
                    if (at.m_d.ud)
                        at.m_d.ud->to_atom(a);
                    to_count.push_back(a);
                }
                break;
            default:
                s++;
                break;
        }
    }

    s--; // don't count the initial list/map that was pushed
    return s;
}
//---------------------------------------------------------------------------

Atom Atom::at(const Atom &a)
{
    if (m_type != T_MAP) return Atom();
    return m_d.map->at(a);
}
//---------------------------------------------------------------------------

void Atom::set(const Atom &a, const Atom &v)
{
    if (m_type == T_VEC)
        m_d.vec->set((size_t) a.to_int(), v);
    else if (m_type == T_MAP)
        m_d.map->set(a, v);
}
//---------------------------------------------------------------------------

void Atom::set(size_t i, const Atom &v)
{
    if (m_type == T_VEC)
        m_d.vec->set(i, v);
    else if (m_type == T_MAP)
        m_d.map->set(Atom(T_INT, i), v);
}
//---------------------------------------------------------------------------

Atom Atom::meta() const 
{
    if      (m_type == T_VEC && m_d.vec->m_meta)  return Atom(T_VEC, m_d.vec->m_meta);
    else if (m_type == T_CLOS && m_d.vec->m_meta) return Atom(T_VEC, m_d.vec->m_meta);
    else if (m_type == T_MAP && m_d.map->m_meta)  return Atom(T_VEC, m_d.map->m_meta);
    else return Atom();
}
//---------------------------------------------------------------------------

bool Atom::is_simple() const
{
    switch (m_type)
    {
        case T_UD:
        case T_VEC:
        case T_CLOS:
        case T_MAP:
            return false;
    }
    return true;
}
//---------------------------------------------------------------------------

size_t Atom::size() const
{
    switch (m_type)
    {
        case T_NIL:     return 0;
        case T_INT:     return sizeof(m_d.i);
        case T_BOOL:    return sizeof(m_d.b);
        case T_DBL:     return sizeof(m_d.d);
        case T_PRIM:    return sizeof(m_d.func);
        case T_C_PTR:   return sizeof(m_d.ptr);

        case T_UD:
        case T_VEC:
        case T_CLOS:
        case T_MAP:
            return count_elements(*this);

        case T_SYNTAX:
        case T_SYM:
        case T_KW:
        case T_STR:
            return m_d.sym->m_str.size();

        default:
            return 0;
    }
}
//---------------------------------------------------------------------------

std::string Atom::to_write_str(bool pretty) const
{
    if (pretty)
        return write_atom_pp(*this);
    else
        return write_atom(*this);
}
//---------------------------------------------------------------------------

Atom Atom::any_to_number(int base) const
{
    Atom out;
    switch (m_type)
    {
        case T_NIL:
            out.set_int(0);
            break;
        case T_INT:
        case T_DBL:
            out = *this;
            break;
        case T_SYM:
        case T_KW:
        case T_STR:
        {
            std::string str = m_d.sym->m_str;
			if (base)
				str = std::to_string(base) + "r" + str;
            UTF8Buffer u8P(str.data(), str.size());
            double d_val;
            int64_t i_val;
            bool is_double = false;
            bool is_bad = false;
            u8BufParseNumber(u8P, d_val, i_val, is_double, is_bad);
            if (is_bad)
                break;

            if (is_double) out.set_dbl(d_val);
            else           out.set_int(i_val);
            break;
        }
        default:
            out.set_int(this->to_int());
            break;
    }
    return out;
}
//---------------------------------------------------------------------------

int64_t Atom::any_to_exact() const
{
    Atom n = any_to_number(0);
    return n.to_int();
}
//---------------------------------------------------------------------------

double Atom::any_to_inexact() const
{
    Atom n = any_to_number(0);
    return n.to_dbl();
}
//---------------------------------------------------------------------------

std::string Atom::to_shallow_str() const
{
    std::stringstream ss;
    ss << "#<atom:";
    const Atom &a = *this;
    switch (a.m_type)
    {
        case T_NIL:   ss << "nil";                                                           break;
        case T_INT:   ss << "int:"    << a.m_d.i;                                            break;
        case T_DBL:   ss << "dbl:"    << a.m_d.d;                                            break;
        case T_BOOL:  ss << "bool:"   <<a.m_d.b;                                             break;
        case T_STR:   ss << "str:"    << a.m_d.sym->m_str;                                   break;
        case T_SYM:   ss << "sym:"    << a.m_d.sym->m_str;                                   break;
        case T_KW:    ss << "kw:"     << a.m_d.sym->m_str;                                   break;
        case T_VEC:   ss << "vec:"    << ((void *) a.m_d.vec) << "," << a.m_d.vec->m_len;    break;
        case T_MAP:   ss << "map:"    << ((void *) a.m_d.map) << "," << a.m_d.map->size();   break;
        case T_PRIM:  ss << "prim:"   << ((void *) a.m_d.func);                              break;
        case T_SYNTAX:ss << "syntax"  << a.m_d.sym->m_str;                                   break;
        case T_CLOS:  ss << "clos:"   << ((void *) a.m_d.vec) << "," << (a.m_d.vec->m_len);  break;
        case T_UD:    ss << "ud:"     << a.m_d.ud->type() << "," << ((void *) a.m_d.ud);     break;
        case T_C_PTR: ss << "cptr:"   << ((void *) a.m_d.ptr);                               break;
        case T_HPAIR: ss << "hpair";                                                         break;
        default: ss << "?";
    }
    ss << ">";
    return ss.str();
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
    m_meta     = nullptr;

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

Atom AtomVec::pop_last()
{
    Atom a;
    if (m_len > 0)
        a = m_data[--m_len];
    return a;
}
//---------------------------------------------------------------------------

void AtomVec::set(size_t idx, const Atom &a)
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

void AtomVec::unshift(const Atom &a)
{
    size_t new_idx = m_len;
    check_size(new_idx);
    for (size_t i = new_idx; i > 0; i--)
        m_data[i] = m_data[i - 1];
    m_data[0] = a;
}
//---------------------------------------------------------------------------

void AtomVec::shift()
{
    for (size_t i = 1; i < m_len; i++)
        m_data[i - 1] = m_data[i];
    m_len--;
}
//---------------------------------------------------------------------------

void AtomVec::check_size(size_t idx)
{
    if (idx < m_len)
        return;

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

void AtomVec::clear_alloc()
{
#if WITH_MEM_POOL
    if (m_data)
        g_atom_array_pool.free(m_data);
#else
    if (m_data)
        delete[] m_data;
#endif
    m_data  = nullptr;
    m_len   = 0;
    m_alloc = 0;
    m_meta  = nullptr;
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

#define     BKLISP_GC_NEW_ST_ENTRY_STR(name, str) \
    e = this->allocate_vector(0); \
    e->push(Atom(T_KW,  this->new_symbol(name))); \
    e->push(Atom(T_STR, this->new_symbol(str))); \
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
    size_t n_tiny_bytes = 0;
    size_t n_medium = 0;
    size_t n_small  = 0;
    size_t n_tiny   = 0;
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

    mv = m_tiny_vectors;
    while (mv)
    {
        n_tiny_bytes += sizeof(AtomVec) + mv->m_alloc * sizeof(Atom);
        n_tiny++;
        mv = mv->m_gc_next;
    }

#if WITH_MEM_POOL
    BKLISP_GC_NEW_ST_ENTRY_STR("atom-array-pool",      g_atom_array_pool.statistics());
#endif

    BKLISP_GC_NEW_ST_ENTRY("medium-vector-pool",       n_medium);
    BKLISP_GC_NEW_ST_ENTRY("small-vector-pool",        n_small);
    BKLISP_GC_NEW_ST_ENTRY("tiny-vector-pool",         n_tiny);

    BKLISP_GC_NEW_ST_ENTRY("tiny-vector-pool-bytes",   n_tiny_bytes);
    BKLISP_GC_NEW_ST_ENTRY("small-vector-pool-bytes",  n_small_bytes);
    BKLISP_GC_NEW_ST_ENTRY("medium-vector-pool-bytes", n_medium_bytes);
    BKLISP_GC_NEW_ST_ENTRY("alive-vectors-bytes",      n_alive_vector_bytes);
    BKLISP_GC_NEW_ST_ENTRY("alive-syms-bytes",         n_syms_size);

    return Atom(T_VEC, v);
}
//---------------------------------------------------------------------------

RegRowsReference::RegRowsReference(
            AtomVec **rr0,
            AtomVec **rr1,
            AtomVec **rr2)
    :
    m_rr0(rr0),
    m_rr1(rr1),
    m_rr2(rr2)
{
}
//---------------------------------------------------------------------------

void RegRowsReference::mark(GC *gc, uint8_t clr)
{
    UserData::mark(gc, clr);
    gc->mark_atom(Atom(T_VEC, *m_rr0));
    gc->mark_atom(Atom(T_VEC, *m_rr1));
    gc->mark_atom(Atom(T_VEC, *m_rr2));
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
            (size_t) err_stack.at(i).at(2).to_int(),
            err_stack.at(i).at(3).to_display_str());
    }
    return *this;
}
//---------------------------------------------------------------------------

void atom_tree_walker(
    Atom a,
    std::function<void(std::function<void(Atom &a)> cont, unsigned int indent, Atom a)> cb)
{
    unsigned int indent = 0;
    std::function<void(Atom &a)> walker;
    walker = [&](Atom &a)
    {
        switch (a.m_type)
        {
            case T_MAP:
                {
                    AtomMap *map = a.m_d.map;
                    if (map && map->m_meta)
                    {
                        indent++;
                        cb(walker, indent, Atom(T_VEC, map->m_meta));
                        indent--;
                    }

                    ATOM_MAP_FOR(i, map)
                    {
                        indent++;
                        cb(walker, indent, MAP_ITER_KEY(i));
                        cb(walker, indent, MAP_ITER_VAL(i));
                        indent--;
                    }
                }
                break;

            case T_CLOS:
            case T_VEC:
                for (size_t i = 0; i < a.m_d.vec->m_len; i++)
                {
                    indent++;
                    cb(walker, indent, a.m_d.vec->m_data[i]);
                    indent--;
                }
                break;
        }
    };

    cb(walker, indent, a);
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
