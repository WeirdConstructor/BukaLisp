// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include <unordered_map>
#include <sstream>

namespace bukalisp
{
//---------------------------------------------------------------------------

struct Atom;

struct AtomVec;

struct Sym;

//---------------------------------------------------------------------------

class AtomHash
{
    public:
        size_t operator()(const Atom &a) const;
};
//---------------------------------------------------------------------------

typedef std::unordered_map<Atom, Atom, AtomHash>    UnordAtomMap;
//---------------------------------------------------------------------------

#define ATOM_HASH_TABLE_SIZE_COUNT 29
extern const size_t HASH_TABLE_SIZES[ATOM_HASH_TABLE_SIZE_COUNT];

//---------------------------------------------------------------------------

#if WITH_STD_UNORDERED_MAP

template<typename Atom, typename HashFunc>
struct HashTable
{
    uint8_t         m_gc_color;
    HashTable<Atom, HashFunc>
                   *m_gc_next;
    UnordAtomMap    m_map;
    AtomVec        *m_meta;

    //---------------------------------------------------------------------------

    HashTable() : m_gc_next(nullptr), m_gc_color(0), m_meta(nullptr) { }

    void clear()
    {
        m_map.clear();
    }

    //---------------------------------------------------------------------------

    bool empty() { return m_map.empty(); }
    size_t size() { return m_map.size(); }

    //---------------------------------------------------------------------------

    void set(Sym *s, const Atom &a)
    {
        Atom sym(T_SYM, s);
        m_map[sym] = a;
    }
    //---------------------------------------------------------------------------

    void set(const Atom &k, const Atom &a)
    {
        m_map[k] = a;
    }
    //---------------------------------------------------------------------------

    Atom at(const Atom &k)
    {
        auto it = m_map.find(k);
        if (it == m_map.end()) return Atom();
        return it->second;
    }
    //---------------------------------------------------------------------------

    Atom at(const Atom &k, bool &defined)
    {
        defined = false;
        auto it = m_map.find(k);
        if (it == m_map.end()) return Atom();
        defined = true;
        return it->second;
    }
    //---------------------------------------------------------------------------

    std::string debug_dump() { return ""; }
};
//---------------------------------------------------------------------------

#else // NOT WITH_STD_UNORDERED_MAP

#define HT_INIT_SIZE    23
#define HT_NEXT_TBL_IDX 3
template<typename Atom, typename HashFunc>
struct HashTable
{
    //---------------------------------------------------------------------------

    HashFunc        m_hash_func;
    size_t          m_table_size;
    size_t          m_item_count;
    size_t          m_next_size_tbl_idx;

    Atom            m_initial_buffer[3 * HT_INIT_SIZE];

    //---------------------------------------------------------------------------

    Atom           *m_begin;
    Atom           *m_end;

    bool            m_inhibit_grow;

    uint8_t         m_gc_color;
    HashTable<Atom, HashFunc>
                   *m_gc_next;
    AtomVec        *m_meta;

    //---------------------------------------------------------------------------

    HashTable(bool is_debug)
        : m_table_size(0),
          m_next_size_tbl_idx(0),
          m_item_count(0),
          m_begin(nullptr),
          m_end(nullptr),
          m_inhibit_grow(false),
          m_meta(nullptr)
    {
    }

    HashTable()
        : m_table_size(HT_INIT_SIZE),
          m_next_size_tbl_idx(HT_NEXT_TBL_IDX),
          m_item_count(0),
          m_inhibit_grow(false),
          m_meta(nullptr)
    {
        m_begin = &m_initial_buffer[0];
        m_end   = m_begin + (m_table_size * 3);
    }

    void clear()
    {
        free_tbl(m_begin);
        m_table_size        = HT_INIT_SIZE;
        m_next_size_tbl_idx = HT_NEXT_TBL_IDX;
        m_item_count        = 0;
        m_begin             = &m_initial_buffer[0];
        m_end               = m_begin + (m_table_size * 3);

        Atom *cur = m_begin;
        while (cur != m_end)
        {
            cur->clear();
            cur += 3;
        }
    }

    ~HashTable() { free_tbl(m_begin); }

    //---------------------------------------------------------------------------

    void free_tbl(Atom *tbl)
    {
        if (tbl && tbl != &(m_initial_buffer[0]))
        {
#          if WITH_MEM_POOL
               g_atom_array_pool.free(tbl);
#          else
               delete[] tbl;
#          endif
        }
    }
    //---------------------------------------------------------------------------

    Atom *alloc_new_tbl(size_t val_count)
    {
        Atom *data;
#       if WITH_MEM_POOL
           data = g_atom_array_pool.allocate(val_count * 3);
#       else
           data = new Atom[val_count * 3];
#       endif
        return data;
    }
    //---------------------------------------------------------------------------

    #define AT_HT_CALC_IDX_HASH(key, hash, idx)         \
            size_t hash = m_hash_func(key);             \
            size_t idx  = hash % m_table_size;

    #define AT_HT_ITER_START(cur, search_begin, idx)    \
            Atom *cur = &(m_begin[3 * idx]);            \
            Atom *search_begin = cur;

    #define AT_HT_ITER_NEXT(cur)       cur += 3;        if (cur >= m_end) cur = m_begin;
    #define AT_HT_ITER_NEXTI(cur, idx) cur += 3; idx++; if (cur >= m_end) { cur = m_begin; idx = 0; }

    //---------------------------------------------------------------------------
    void set(Sym *s, const Atom &a)
    {
        this->set(Atom(T_SYM, s), a);
    }
    //---------------------------------------------------------------------------

    void set(const Atom &str, const Atom &a)
    {
        Atom *pair = find_pair(str);
        if (pair) pair[1] = a;
        else      insert(str, a);
    }
    //---------------------------------------------------------------------------

    Atom at(const Atom &a)
    {
        Atom *pair = find_pair(a);
        if (pair) return pair[1];
        else      return Atom();
    }
    //---------------------------------------------------------------------------

    Atom at(const Atom &k, bool &defined)
    {
        Atom *pair = find_pair(k);
        if (pair)
        {
            defined = true;
            return pair[1];
        }
        else
        {
            defined = false;
            return Atom();
        }
    }
    //---------------------------------------------------------------------------

    bool empty() { return m_item_count == 0; }
    size_t size() { return m_item_count; }

    //---------------------------------------------------------------------------

    Atom *find_pair(const Atom &key)
    {
        if (!m_begin) return nullptr;

//        std::cout << "FIND PARI " << debug_dump();
//        std::cout << "@" << key.to_write_str() << std::endl;

        AT_HT_CALC_IDX_HASH(key, hash, idx);

        AT_HT_ITER_START(cur, search_begin, idx);

        if (   cur[0].m_type == T_HPAIR
            && cur[0].m_d.hpair.key == hash
            && cur[1] == key)
            return cur + 1;

        AT_HT_ITER_NEXT(cur);
        while (   cur           != search_begin
               && cur[0].m_type != T_NIL)
        {
            if (   cur[0].m_type == T_HPAIR
                && cur[0].m_d.hpair.key == hash)
            {
                if (cur[1] == key)
                    return cur + 1;
                continue;
            }

            AT_HT_ITER_NEXT(cur);
        }

        return nullptr;
    }
    //---------------------------------------------------------------------------

    Atom *next(Atom *cur)
    {
        if (cur == nullptr)
            cur = m_begin + 1;
        else
            cur += 3;
        cur--;

        while (   cur < m_end
               && cur[0].m_type != T_HPAIR)
            cur += 3;

        return
            cur >= m_end
            ? nullptr
            : cur + 1;
    }
    //---------------------------------------------------------------------------

    void grow()
    {
        if (m_begin && m_inhibit_grow) return;

        size_t new_size = HASH_TABLE_SIZES[m_next_size_tbl_idx++];

    //    cout << "GROW " << m_table_size << " => " << new_size << endl;

        if (new_size <= m_table_size)
            throw BukaLISPException("Can't grow hash table anymore, too big!");

        Atom *old_begin = m_begin;
        Atom *old_end   = m_end;
        m_begin         = alloc_new_tbl(new_size);
        m_end           = m_begin + (new_size * 3);
        m_table_size    = new_size;
        m_item_count    = 0; // item count is updated by insert_at on rehashing

        if (old_begin)
        {
    //        cout << "GROW " << m_table_size << " => " << new_size << endl;
            Atom *cur = old_begin;
            while (cur != old_end)
            {
    //            cout << " REHASH " << i << " ; " << cur[1].to_write_str() << endl;
                if (cur[0].m_type == T_HPAIR)
                {
                    size_t new_idx = cur[0].m_d.hpair.key % new_size;
                    insert_at(
                        cur[0].m_d.hpair.key,
                        new_idx,
                        cur[1],
                        cur[2]);
                }

                cur += 3;
            }

            free_tbl(old_begin);
        }
    }
    //---------------------------------------------------------------------------

    void insert_at(
        size_t hash, size_t idx,
        const Atom &key, const Atom &data)
    {
        using namespace std;
        AT_HT_ITER_START(cur, search_begin, idx);
        //cout << "START=" << ((void *) m_begin)
        //     << "; END=" << ((void *) m_end) << endl;
        if (cur[0].m_type == T_HPAIR)
        {
            AT_HT_ITER_NEXT(cur);
            while (   cur           != search_begin
                   && cur[0].m_type == T_HPAIR)
            {
                AT_HT_ITER_NEXT(cur);
                // cout << "NEXT CUR: " << ((void *) cur) << endl;
            }

            if (   cur[0].m_type == T_HPAIR
                && cur == search_begin)
            {
                throw BukaLISPException(
                    "Error while inserting into atom hash table");
                return;
            }
        }

        if (cur[0].m_type != T_INT)
            m_item_count++;

        cur[0].m_type        = T_HPAIR;
        cur[0].m_d.hpair.key = hash;
        cur[0].m_d.hpair.idx = idx;
        cur[1]               = key;
        cur[2]               = data;
    }
    //---------------------------------------------------------------------------

    void insert(const Atom &key, const Atom &data)
    {
        if (   m_table_size == 0
            || m_item_count >= ((m_table_size * 3) / 4))
            grow();

        AT_HT_CALC_IDX_HASH(key, hash, idx);
        // std::cout << "hash: " << hash << " idx: " << idx << std::endl;
        insert_at(hash, idx, key, data);
    }
    //---------------------------------------------------------------------------

    void delete_key(const Atom &key)
    {
        Atom *cur = find_pair(key);
        if (!cur) return;
        cur--;
        cur[0].set_int(42);
        cur[1].clear();
        cur[2].clear();
    }
    //---------------------------------------------------------------------------

    std::string debug_dump()
    {
        std::stringstream ss;

        ss << "#<AtomHashTable size=" << m_table_size
           << ", items=" << m_item_count << " [" << std::endl;

        Atom *cur = m_begin;
        size_t i = 0;
        while (cur != m_end)
        {
            ss << " {" << i << "} " << ((void *) cur) << " [";
            if (cur[0].m_type == T_HPAIR)
                ss << cur[0].m_d.hpair.idx << " / " << cur[0].m_d.hpair.key;
            else if (cur[0].m_type == T_INT)
                ss << "deleted";
            else
                ss << "free";

            ss << "] "
               << cur[1].to_write_str()
               << " => "
               << cur[2].to_write_str() << std::endl;
            cur += 3;
            i++;
        }
        ss << "]>" << std::endl;

        return ss.str();
    }
    //---------------------------------------------------------------------------
};

#endif

//---------------------------------------------------------------------------

#if WITH_STD_UNORDERED_MAP

#   define ATOM_MAP_FOR(i, map) \
       for (UnordAtomMap::iterator i = (map)->m_map.begin(); \
            i != (map)->m_map.end(); \
            i++)

#   define MAP_ITER_KEY(i) (i->first)
#   define MAP_ITER_VAL(i) (i->second)

//#define ATOM_MAP_ITERATOR_NEW(mi, map) \
//    AtomMapIterator *mi = new AtomMapIterator((map));
//#define ATOM_MAP_ITERATOR_NEXT_FROM_UD(mi, atom) \
//    AtomMapIterator *mi = \
//        dynamic_cast<AtomMapIterator *>((atom)); \

#else // NOT WITH_STD_UNORDERED_MAP

#    define ATOM_MAP_FOR(i, map) \
        for (Atom *i = (map)->next(nullptr); i; i = (map)->next(i))

#    define MAP_ITER_KEY(i) ((i)[0])
#    define MAP_ITER_VAL(i) ((i)[1])

#endif

//---------------------------------------------------------------------------

} // namespace bukalisp

//---------------------------------------------------------------------------

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
