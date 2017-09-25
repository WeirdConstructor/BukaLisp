// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include "atom_userdata.h"

#define WITH_MEM_POOL 1

#if WITH_MEM_POOL
#include "mempool.h"
#endif

namespace bukalisp
{
//---------------------------------------------------------------------------

struct Atom;

class BukaLISPException : public std::exception
{
    private:
        struct ErrorFrame
        {
            std::string m_place;
            std::string m_file_name;
            size_t      m_line;
            std::string m_func_name;

            std::string to_string()
            {
                return "{" + m_place + "} "
                       + m_func_name
                       + " [" + m_file_name
                              + ":" + std::to_string(m_line) + "]\n";
            }
        };
        std::vector<ErrorFrame> m_frames;
        std::string m_error_message;
        std::string m_err;

    public:
        BukaLISPException(const std::string &err)
        {
            m_error_message = err;
        }
        BukaLISPException(const std::string place,
                  const std::string file_name,
                  size_t line,
                  const std::string &func_name,
                  const std::string &err)
        {
            m_error_message = err;
            push(place, file_name, line, func_name);
        }

        BukaLISPException &push(Atom &err_stack);

        BukaLISPException &push(const std::string place,
                  const std::string file_name,
                  size_t line,
                  const std::string &func_name)
        {
            ErrorFrame frm;
            frm.m_place     = place;
            frm.m_file_name = file_name;
            frm.m_line      = line;
            frm.m_func_name = func_name;
            m_frames.push_back(frm);
            m_err = "";
            for (auto &frm : m_frames)
                m_err = frm.to_string() + m_err;
            m_err += "Error: " + m_error_message;
            m_err = "\n" + m_err;
            return *this;
        }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~BukaLISPException() { }
};
//---------------------------------------------------------------------------

enum Type : int16_t
{
    T_NIL,
    T_INT,
    T_DBL,
    T_BOOL,
    T_STR,      // m_d.sym
    T_SYM,      // m_d.sym
    T_KW,       // m_d.sym
    T_VEC,      // m_d.vec
    T_MAP,      // m_d.map
    T_PRIM,     // m_d.func
    T_SYNTAX,   // m_d.sym
    T_CLOS,     // m_d.vec
    T_UD,       // m_d.ud
    T_C_PTR     // m_d.ptr
};
//---------------------------------------------------------------------------

struct Sym
{
    uint8_t     m_gc_color;
    Sym        *m_gc_next;
    std::string m_str;
};
//---------------------------------------------------------------------------

struct Atom;

struct AtomVec;

struct AtomVec
{
    uint8_t     m_gc_color;
    AtomVec    *m_gc_next;

    size_t      m_alloc;
    size_t      m_len;
    Atom       *m_data;
    AtomVec    *m_meta;

    static size_t   s_alloc_count;

    AtomVec()
        : m_gc_next(nullptr), m_gc_color(0), m_alloc(0),
          m_len(0), m_data(nullptr), m_meta(nullptr)
    {
        s_alloc_count++;
    }

    void alloc(size_t len);
    void init(uint8_t current_gc_color, size_t len);

    Atom *first();
    Atom *last();
    void pop();
    void push(const Atom &a);
    void check_size(size_t idx);

    Atom at(size_t idx);
    void set(size_t idx, Atom &a);

    ~AtomVec();
};
//---------------------------------------------------------------------------

class AtomHash
{
    public:
        size_t operator()(const Atom &a) const;
};
//---------------------------------------------------------------------------

typedef std::unordered_map<Atom, Atom, AtomHash>    UnordAtomMap;
//---------------------------------------------------------------------------

struct AtomMap
{
    uint8_t         m_gc_color;
    AtomMap        *m_gc_next;
    UnordAtomMap    m_map;
    AtomVec        *m_meta;

    AtomMap() : m_gc_next(nullptr), m_gc_color(0), m_meta(nullptr) { }

    void set(Sym *s, Atom &a);
    void set(const Atom &str, Atom &a);

    Atom at(const Atom &a);
    Atom at(const Atom &k, bool &defined);
};
//---------------------------------------------------------------------------

struct Atom
{
    typedef std::function<void(AtomVec &args, Atom &out)> PrimFunc;

    Type m_type;

    union {
        int64_t      i;
        double       d;
        bool         b;

        PrimFunc    *func;
        Sym         *sym;
        AtomVec     *vec;
        AtomMap     *map;
        UserData    *ud;
        void        *ptr;
    } m_d;

    Atom() : m_type(T_NIL)
    {
        m_d.i = 0;
    }

    Atom(Type t) : m_type(t)
    {
        m_d.i = 0;
    }

    Atom(Type t, int64_t i) : m_type(t)
    {
        m_d.i = i;
    }

    Atom(Type t, AtomVec *av) : m_type(t)
    {
        m_d.vec = av;
    }

    Atom(Type t, AtomMap *am) : m_type(t)
    {
        m_d.map = am;
    }

    Atom(Type t, Sym *s) : m_type(t)
    {
        m_d.sym = s;
    }

    Atom(Type t, PrimFunc *f) : m_type(t)
    {
        m_d.func = f;
    }

    inline void clear()
    {
        m_type = T_NIL;
        m_d.i  = 0;
    }

    inline int64_t to_int()
    { return m_type == T_INT   ? m_d.i
             : m_type == T_DBL ? static_cast<int64_t>(m_d.d)
             : m_type == T_SYM ? (int64_t) m_d.sym
             : m_type == T_NIL ? 0
             :                   m_d.i; }
    inline double to_dbl()
    { return m_type == T_DBL   ? m_d.d
             : m_type == T_INT ? static_cast<double>(m_d.i)
             : m_type == T_SYM ? (double) (int64_t) m_d.sym
             : m_type == T_NIL ? 0.0
             :                   m_d.d; }

    std::string to_display_str() const;
    std::string to_write_str() const;

    bool inline is_false()
    {
        return     m_type == T_NIL
               || (m_type == T_BOOL && !m_d.b);
    }

    Atom at(size_t i)
    {
        if (m_type != T_VEC) return Atom();
        return m_d.vec->at(i);
    }

    Atom at(const Atom &a)
    {
        if (m_type != T_MAP) return Atom();
        return m_d.map->at(a);
    }

    Atom meta()
    {
        if      (m_type == T_VEC && m_d.vec->m_meta) return Atom(T_VEC, m_d.vec->m_meta);
        else if (m_type == T_MAP && m_d.map->m_meta) return Atom(T_VEC, m_d.map->m_meta);
        else return Atom();
    }

    bool eqv(const Atom &o)
    {
        switch (m_type)
        {
            case T_BOOL: return o.m_type == T_BOOL
                                && m_d.b == o.m_d.b;
            case T_KW:   return o.m_type == T_KW
                                && m_d.sym == o.m_d.sym;
            case T_SYM:  return o.m_type == T_SYM
                                && m_d.sym == o.m_d.sym;
            case T_STR:  return o.m_type == T_STR
                                && m_d.sym == o.m_d.sym;
            case T_SYNTAX:
                         return o.m_type == T_SYNTAX
                                && m_d.sym == o.m_d.sym;
            case T_INT:  return o.m_type == T_INT
                                && m_d.i == o.m_d.i;
            case T_DBL:  return o.m_type == T_DBL
                                && m_d.d == o.m_d.d;
            case T_VEC:  return o.m_type == T_VEC
                                && m_d.vec == o.m_d.vec;
            case T_MAP:  return o.m_type == T_MAP
                                && m_d.map == o.m_d.map;
            case T_UD:   return o.m_type == T_UD
                                && m_d.ud == o.m_d.ud;
            case T_C_PTR:return o.m_type == T_UD
                                && m_d.ptr == o.m_d.ptr;
            case T_PRIM: return o.m_type == T_PRIM
                                && m_d.func == o.m_d.func;
            case T_NIL:  return o.m_type == T_NIL;
            default: return false;
        }
        return false;
    }

    int64_t id()
    {
        // TODO use X macro
        switch (m_type)
        {
            case T_INT:
            case T_DBL:
            case T_BOOL:
                return m_d.i;

            case T_STR:
            case T_KW:
            case T_SYNTAX:
            case T_SYM:
                return (int64_t) AtomHash()(*this);

            case T_PRIM:
                return (int64_t) m_d.func;

            case T_MAP:
                return (int64_t) m_d.map;

            case T_UD:
                return (int64_t) m_d.ud;

            case T_C_PTR:
                return (int64_t) m_d.ptr;

            case T_VEC:
            case T_CLOS:
            default:
                return (int64_t) m_d.vec;
        }
    }

    bool operator==(const Atom &other) const
    {
//        std::cout << "EQ " << this->to_write_str() << "<=>" << other.to_write_str() << std::endl;
        if (m_type != other.m_type) return false;
        // TODO use X macro
        switch (m_type)
        {
            case T_INT:  return m_d.i == other.m_d.i;
            case T_DBL:  return m_d.d == other.m_d.d;
            case T_BOOL: return m_d.b == other.m_d.b;

            case T_STR:
            case T_KW:
            case T_SYNTAX:
            case T_SYM:
                return m_d.sym == other.m_d.sym;

            case T_PRIM:
                return m_d.func == other.m_d.func;

            case T_VEC:
            case T_MAP:
            case T_CLOS:
                return m_d.vec == other.m_d.vec;

            case T_UD:
                return m_d.ud == other.m_d.ud;

            case T_C_PTR:
                return m_d.ptr == other.m_d.ptr;

            default:
                // pointer comparsion
                return m_d.vec == other.m_d.vec;
        }
    }
};
//---------------------------------------------------------------------------

class AtomException : public std::exception
{
    private:
        std::string m_err;
    public:
        AtomException(const std::string &err)
        {
            m_err = err;
        }

        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~AtomException() { }
};
//---------------------------------------------------------------------------

#define GC_COLOR_FREE    0x08
#define GC_COLOR_DELETE  0x04
#define GC_COLOR_WHITE   0x01
#define GC_COLOR_BLACK   0x00
//---------------------------------------------------------------------------

template<typename T>
T *gc_list_sweep(T *list, size_t &num_alive, uint8_t current_color, std::function<void(T *)> free_func)
{
    T *alive_list = nullptr;
    num_alive = 0;

    while (list)
    {
        T *cur = list;
        list = cur->m_gc_next;

        if (cur->m_gc_color != current_color)
        {
            free_func(cur);
            continue;
        }

        cur->m_gc_next = alive_list;
        alive_list = cur;
        num_alive++;
    }

    return alive_list;
}
//---------------------------------------------------------------------------

class AtomVecPush
{
    private:
        AtomVec *m_av;
    public:
        AtomVecPush(AtomVec *av, const Atom &a) : m_av(av)
        {
            m_av->push(a);
//            std::cout << "PUSH AVP: " << m_av << ":" << m_av->m_len << std::endl;
        }

        ~AtomVecPush()
        {
//            std::cout << "POP AVP: " << m_av << ":" << m_av->m_len << std::endl;
            m_av->pop();
        }
};
//---------------------------------------------------------------------------

class GCRootRefPool;

#define GC_ROOT(gc, name) \
    bukalisp::GCRootRefPool::GCRootRef gc_ref_##name((gc).get_root_ref_pool()); \
    Atom &name = *(gc_ref_##name.m_ref); name
#define GC_ROOT_SHARED_PTR(gc, name) \
    std::shared_ptr<bukalisp::GCRootRefPool::GCRootRef> name = \
        std::make_shared<bukalisp::GCRootRefPool::GCRootRef>((gc).get_root_ref_pool()); \
    *(name->m_ref)
#define GC_ROOT_SHP_REF(name) (*(name->m_ref))

#define GC_ROOT_VEC(gc, name) \
    bukalisp::GCRootRefPool::GCRootRef gc_ref_##name(T_VEC, (gc).get_root_ref_pool()); \
    AtomVec *&name = gc_ref_##name.m_ref->m_d.vec; name

#define GC_ROOT_MAP(gc, name) \
    bukalisp::GCRootRefPool::GCRootRef gc_ref_##name(T_MAP, (gc).get_root_ref_pool()); \
    AtomMap *&name = gc_ref_##name.m_ref->m_d.map; name

#define GC_ROOT_MEMBER(name) \
    bukalisp::GCRootRefPool::GCRootRef gc_ref_##name; \
    Atom &name;
#define GC_ROOT_MEMBER_INITALIZE(gc, name) \
    gc_ref_##name((gc).get_root_ref_pool()), \
    name(*(gc_ref_##name.m_ref))

#define GC_ROOT_MEMBER_MAP(name) \
    bukalisp::GCRootRefPool::GCRootRef gc_ref_##name; \
    AtomMap *&name;
#define GC_ROOT_MEMBER_INITALIZE_MAP(gc, name) \
    gc_ref_##name(T_MAP, (gc).get_root_ref_pool()), \
    name(gc_ref_##name.m_ref->m_d.map)

#define GC_ROOT_MEMBER_VEC(name) \
    bukalisp::GCRootRefPool::GCRootRef gc_ref_##name; \
    AtomVec *&name;
#define GC_ROOT_MEMBER_INITALIZE_VEC(gc, name) \
    gc_ref_##name(T_VEC, (gc).get_root_ref_pool()), \
    name(gc_ref_##name.m_ref->m_d.vec)


class GCRootRefPool
{
    private:
        AtomVec                         *m_slots;
        std::function<AtomVec*(size_t)>  m_allocator;
        std::vector<size_t>              m_free_idx_list;
        size_t                           m_stripe_len;

    public:
        struct GCRootRef
        {
            GCRootRefPool &m_pool;
            Atom          *m_ref;
            size_t         m_idx;

            GCRootRef(GCRootRefPool &rp)
                : m_pool(rp)
            {
                m_idx = m_pool.reg();
                m_ref = &(m_pool.get_ref_at(m_idx));
                *m_ref = Atom();
            }

            GCRootRef(Type t, GCRootRefPool &rp)
                : m_pool(rp)
            {
                m_idx = m_pool.reg();
                m_ref = &(m_pool.get_ref_at(m_idx));
                *m_ref = Atom(t);
            }
            ~GCRootRef();
        };

    public:
        GCRootRefPool(const std::function<AtomVec*(size_t)> &allocator)
            : m_slots(nullptr), m_allocator(allocator),
              m_stripe_len(10)
        {
        }

        void set_pool(AtomVec *p) { m_slots = p; m_slots->m_len = 0; }
        AtomVec *get_pool() { return m_slots; }

        void unreg(size_t idx)
        {
            get_ref_at(idx) = Atom();
            m_free_idx_list.push_back(idx);
//            std::cout << "*** IDX DEL: " << idx << std::endl;
        }

        Atom &get_ref_at(size_t idx)
        {
            size_t slot_idx   = idx / m_stripe_len;
            size_t stripe_idx = idx % m_stripe_len;
//            std::cout << "SLOT: " << slot_idx << ", STRIPE: " << stripe_idx << std::endl;
            Atom stripe = m_slots->m_data[slot_idx];
            Atom &ref = stripe.m_d.vec->m_data[stripe_idx];
            return ref;
        }

        size_t reg()
        {
            size_t idx = 0;
            if (m_free_idx_list.size() > 0)
            {
                idx = m_free_idx_list.back();
                m_free_idx_list.pop_back();
            }
            else
            {
                Atom *last_stripe = m_slots->last();
                if ((!last_stripe)
                    || (last_stripe->m_d.vec->m_len >= m_stripe_len))
                {
                    m_slots->push(Atom(T_VEC, m_allocator(m_stripe_len)));
                    last_stripe = m_slots->last();
                }

                last_stripe->m_d.vec->m_len++;
                idx = ((m_slots->m_len - 1) * m_stripe_len)
                      + (last_stripe->m_d.vec->m_len - 1);
            }

            return idx;
        }

};
//---------------------------------------------------------------------------

typedef std::unordered_map<std::string, Sym *> StrSymMap;

class GC
{
    private:
        AtomVec         *m_vectors;
        AtomMap         *m_maps;
        UserData        *m_userdata;
        Sym             *m_syms;
        GCRootRefPool   m_root_pool;

        uint8_t  m_current_color;

        std::vector<Sym *>              m_perm_syms;
        StrSymMap                       m_symtbl;

#define GC_SMALL_VEC_LEN     10
#define GC_MEDIUM_VEC_LEN    100
#define GC_BIG_VEC_LEN       200

        std::vector<AtomVec *> m_gc_vec_stack;
        std::vector<AtomMap *> m_gc_map_stack;

        AtomVec     *m_small_vectors;
        AtomVec     *m_medium_vectors;
        size_t       m_num_small_vectors;
        size_t       m_num_medium_vectors;

        size_t       m_num_alive_syms;
        size_t       m_num_alive_vectors;
        size_t       m_num_alive_maps;
        size_t       m_num_alive_userdata;
        size_t       m_num_new_syms;
        size_t       m_num_new_vectors;
        size_t       m_num_new_maps;
        size_t       m_num_new_userdata;

        void allocate_new_vectors(AtomVec *&list, size_t &num, size_t len)
        {
            size_t new_num = (num + 1) * 2;
            for (size_t i = 0; i < new_num; i++)
            {
                AtomVec *new_vec = new AtomVec;
                new_vec->alloc(len);

                new_vec->m_gc_next  = list;
                new_vec->m_gc_color = GC_COLOR_FREE;
                list = new_vec;
            }

            AtomVec *cur = list;
            int i = 0;
            while (cur)
            {
                i++;
                cur = cur->m_gc_next;
            }

            num = i;
        }

        void mark_vector(AtomVec *&vec)
        {
            if (!vec) return;

            // Vector was put back on free list:
            if (vec->m_gc_color == GC_COLOR_FREE)
                return;

            if (vec->m_gc_color == m_current_color)
                return;

            vec->m_gc_color = m_current_color;

            if (vec->m_meta && vec->m_meta)
                m_gc_vec_stack.push_back(vec->m_meta);

            for (size_t i = 0; i < vec->m_len; i++)
            {
                mark_atom(vec->m_data[i]);
            }
        }

        void mark_map(AtomMap *&map)
        {
            if (!map) return;

            if (map->m_gc_color == m_current_color)
                return;

            map->m_gc_color = m_current_color;

            if (map && map->m_meta)
                m_gc_vec_stack.push_back(map->m_meta);

            for (UnordAtomMap::iterator i = map->m_map.begin();
                 i != map->m_map.end();
                 i++)
            {
                mark_atom(i->first);
                mark_atom(i->second);
            }
        }

        void mark()
        {
            m_current_color =
                m_current_color == GC_COLOR_WHITE
                ? GC_COLOR_BLACK
                : GC_COLOR_WHITE;

//            std::cout << "* GC MARK CLR = " << ((int) m_current_color) << std::endl;

            for (auto &sym : m_perm_syms)
                sym->m_gc_color = m_current_color;

            m_gc_vec_stack.clear();
            m_gc_map_stack.clear();

            m_gc_vec_stack.push_back(m_root_pool.get_pool());

            // We use an explicit stack, to prevent C++ stack overflow
            // when marking.
            while (!(   m_gc_vec_stack.empty()
                     && m_gc_map_stack.empty()))
            {
                if (!m_gc_vec_stack.empty())
                {
                    AtomVec *vec = m_gc_vec_stack.back();
                    m_gc_vec_stack.pop_back();

                    mark_vector(vec);
                }
                else // m_gc_map_stack
                {
                    AtomMap *map = m_gc_map_stack.back();
                    m_gc_map_stack.pop_back();

                    mark_map(map);
                }
            }
        }

        void sweep()
        {
            m_syms =
                gc_list_sweep<Sym>(
                    m_syms,
                    m_num_alive_syms,
                    m_current_color,
                    [this](Sym *cur)
                    {
//                        std::cout << "SWPSYM[" << cur->m_str << "]" << std::endl;
                        auto it = m_symtbl.find(cur->m_str);
                        if (it != m_symtbl.end())
                            m_symtbl.erase(it);
                        delete cur;
                    });

            m_maps =
                gc_list_sweep<AtomMap>(
                    m_maps,
                    m_num_alive_maps,
                    m_current_color,
                    [this](AtomMap *cur)
                    {
//                        std::cout << "SWPMAP[" << Atom(T_MAP, cur).to_write_str() << "]" << std::endl;
                        delete cur;
                    });

            m_userdata =
                gc_list_sweep<UserData>(
                    m_userdata,
                    m_num_alive_userdata,
                    m_current_color,
                    [this](UserData *cur)
                    {
//                        std::cout << "SWEEP USERDATA: " << cur << std::endl;
                        delete cur;
                    });

            m_vectors =
                gc_list_sweep<AtomVec>(
                    m_vectors,
                    m_num_alive_vectors,
                    m_current_color,
                    [this](AtomVec *cur) { give_back_vector(cur); });
        }

        Sym *allocate_sym()
        {
            Sym *new_sym        = new Sym;
            new_sym->m_gc_color = m_current_color;
            new_sym->m_gc_next  = m_syms;
            m_syms              = new_sym;
            m_num_new_syms++;
            return new_sym;
        }

    public:
        GC()
            : m_vectors(nullptr),
              m_maps(nullptr),
              m_syms(nullptr),
              m_userdata(nullptr),
              m_current_color(GC_COLOR_WHITE),
              m_small_vectors(nullptr),
              m_medium_vectors(nullptr),
              m_num_small_vectors(0),
              m_num_medium_vectors(0),
              m_num_alive_syms(0),
              m_num_alive_userdata(0),
              m_num_alive_maps(0),
              m_num_alive_vectors(0),
              m_num_new_syms(0),
              m_num_new_userdata(0),
              m_num_new_vectors(0),
              m_num_new_maps(0),
              m_root_pool([=](size_t len) { return this->allocate_vector(len); })
        {
            m_root_pool.set_pool(this->allocate_vector(100));
        }

        GCRootRefPool &get_root_ref_pool()
        {
            return m_root_pool;
        }

        void add_permanent(Sym *sym) { m_perm_syms.push_back(sym); }

        Atom get_statistics();

        void give_back_vector(AtomVec *cur, bool dec = false)
        {
//          std::cout << "SWEEP VEC " << cur << std::endl;
            if (cur->m_alloc >= GC_BIG_VEC_LEN)
            {
                delete cur;
            }
            else if (cur->m_alloc > GC_MEDIUM_VEC_LEN)
            {
                // FIXME: We need to limit the number of medium sized
                //        vectors. or else small vectors will just
                //        end up here. the problem is, that then
                //        new small vectors will be allocated.
                cur->m_gc_next   = m_medium_vectors;
                cur->m_gc_color  = GC_COLOR_FREE;
                cur->m_len       = 0;
                m_medium_vectors = cur;
            }
            else if (cur->m_alloc >= GC_SMALL_VEC_LEN)
            {
                cur->m_gc_next  = m_small_vectors;
                cur->m_gc_color = GC_COLOR_FREE;
                cur->m_len      = 0;
                m_small_vectors = cur;
            }
            else
            {
                std::cerr << "WARNING: Got a smaller than 10 vector to delete!"
                          << std::endl;
                delete cur;
            }

            if (dec)
                m_num_new_vectors--;
        }

        void reg_userdata(UserData *ud)
        {
            ud->m_gc_next = m_userdata;
            m_userdata = ud;
            ud->mark(this, m_current_color);
            m_num_new_userdata++;
        }

        void mark_atom(const Atom &at)
        {
            switch (at.m_type)
            {
                case T_MAP:
                    m_gc_map_stack.push_back(at.m_d.map);
                    break;

                case T_CLOS:
                case T_VEC:
                    m_gc_vec_stack.push_back(at.m_d.vec);
                    break;

                case T_UD:
                    if (at.m_d.ud)
                        at.m_d.ud->mark(this, m_current_color);
                    break;

                case T_SYNTAX:
                case T_KW:
                case T_SYM:
                case T_STR:
                    if (at.m_d.sym)
                        at.m_d.sym->m_gc_color = m_current_color;
                    break;
            }
        }

        void collect_maybe()
        {
            if (   (m_num_new_maps    > m_num_alive_maps   )
                || (m_num_new_vectors > m_num_alive_vectors)
                || (m_num_new_syms    > m_num_alive_syms   ))
            {
//                std::cout << "GC collect at " << m_num_new_vectors
//                          << " <=> " << m_num_alive_vectors << std::endl;
//                std::cout << "GC collect at " << m_num_new_maps
//                          << " <=> " << m_num_alive_maps << std::endl;
//                std::cout << "GC collect at " << m_num_new_syms
//                          << " <=> " << m_num_alive_syms << std::endl;
                collect();
            }
        }

        void collect()
        {
//            std::cout << "**** gc collect start **** " << AtomVec::s_alloc_count
//                << ",v:" << count_potentially_alive_vectors()
//                << ",m:" << count_potentially_alive_maps()
//                << ",s:" << count_potentially_alive_syms()
//                << std::endl;
            mark();
            sweep();

            m_num_new_userdata = 0;
            m_num_new_maps     = 0;
            m_num_new_syms     = 0;
            m_num_new_vectors  = 0;
//            std::cout << "**** gc collect end **** " << AtomVec::s_alloc_count
//                << ",v:" << count_potentially_alive_vectors()
//                << ",m:" << count_potentially_alive_maps()
//                << ",s:" << count_potentially_alive_syms()
//                << std::endl;
        }

        Sym *new_symbol(const std::string &str)
        {
            auto it = m_symtbl.find(str);
            if (it == m_symtbl.end())
            {
                Sym *newsym = allocate_sym();
//                std::cout << "NEW SYM: " << str << std::endl;
                newsym->m_str = str;
                m_symtbl.insert(std::pair<std::string, Sym *>(str, newsym));
                return newsym;
            }
            else
                return it->second;
        }

        AtomMap *allocate_map()
        {
            AtomMap *new_map = new AtomMap;

            new_map->m_gc_color = m_current_color;
            new_map->m_gc_next  = m_maps;
            m_maps              = new_map;
            m_num_new_maps++;

            return new_map;
        }

        AtomVec *allocate_vector(size_t alloc_len)
        {
            AtomVec *new_vec = nullptr;

            if (alloc_len > GC_MEDIUM_VEC_LEN)
            {
                new_vec = new AtomVec;
//                new_vec->alloc(alloc_len == 0 ? 10 : alloc_len);
                new_vec->alloc(alloc_len);
//                std::cout << "NEWVEC " << new_vec << std::endl;
            }
            else if (alloc_len > GC_SMALL_VEC_LEN)
            {
                if (!m_medium_vectors)
                {
                    allocate_new_vectors(
                        m_medium_vectors, m_num_medium_vectors, GC_MEDIUM_VEC_LEN);
                }

                new_vec          = m_medium_vectors;
                m_medium_vectors = m_medium_vectors->m_gc_next;
            }
            else
            {
                if (!m_small_vectors)
                {
                    allocate_new_vectors(
                        m_small_vectors, m_num_small_vectors, GC_SMALL_VEC_LEN);
                }

                new_vec         = m_small_vectors;
                m_small_vectors = m_small_vectors->m_gc_next;
            }

            new_vec->init(m_current_color, 0);

            new_vec->m_gc_next = m_vectors;
            m_vectors          = new_vec;
            m_num_new_vectors++;

            return new_vec;
        }

        void set_meta_register(Atom &a, size_t i, Atom &meta)
        {
            if (a.m_type == T_VEC)
            {
                if (!a.m_d.vec->m_meta)
                    a.m_d.vec->m_meta = this->allocate_vector(i + 1);
                a.m_d.vec->m_meta->set(i, meta);
            }
            else if (a.m_type == T_MAP)
            {
                if (!a.m_d.map->m_meta)
                    a.m_d.map->m_meta = this->allocate_vector(i + 1);
                a.m_d.map->m_meta->set(i, meta);
            }
        }

        AtomVec *clone_vector(AtomVec *av)
        {
            AtomVec *new_vec = allocate_vector(av->m_len);
            for (size_t i = 0; i < av->m_len; i++)
                new_vec->m_data[i] = av->m_data[i];
            new_vec->m_len = av->m_len;
            return new_vec;
        }

        size_t count_potentially_alive_vectors()
        {
            size_t i = 0;
            AtomVec *v = m_vectors;
            while (v) { i++; v = v->m_gc_next; }
            return i;
        }

        size_t count_potentially_alive_maps()
        {
            size_t i = 0;
            AtomMap *v = m_maps;
            while (v) { i++; v = v->m_gc_next; }
            return i;
        }

        size_t count_potentially_alive_syms()
        {
            return m_symtbl.size();
        }

        ~GC()
        {
            size_t dummy;

            gc_list_sweep<Sym>(
                m_syms,
                dummy,
                GC_COLOR_DELETE,
                [](Sym *cur) {
//                    std::cout << "DELSYM[" << cur->m_str << "]" << std::endl;
                    delete cur; });

            gc_list_sweep<AtomVec>(
                m_medium_vectors,
                dummy,
                GC_COLOR_DELETE,
                [this](AtomVec *cur) { delete cur; });

            gc_list_sweep<AtomVec>(
                m_small_vectors,
                dummy,
                GC_COLOR_DELETE,
                [this](AtomVec *cur) { delete cur; });

            gc_list_sweep<AtomVec>(
                m_vectors,
                dummy,
                GC_COLOR_DELETE,
                [this](AtomVec *cur) { delete cur; });

            gc_list_sweep<AtomMap>(
                m_maps,
                dummy,
                GC_COLOR_DELETE,
                [this](AtomMap *cur) { delete cur; });

            gc_list_sweep<UserData>(
                m_userdata,
                dummy,
                GC_COLOR_DELETE,
                [this](UserData *cur)
                {
//                    std::cout << "SWEEP USERDATA: " << cur << std::endl;
                    delete cur;
                });
        }
};
//---------------------------------------------------------------------------

class AtomMapIterator : public UserData
{
    public:
        Atom                     m_map;
        UnordAtomMap            &m_map_ref;
        UnordAtomMap::iterator   m_iterator;
        bool                     m_init;

    public:
        AtomMapIterator(Atom &map);

        bool ok();
        void next();

        Atom key();
        Atom value();

        virtual std::string type()      { return "MAP-ITER"; }
        virtual std::string as_string() { return "#<map-iterator>"; }

        virtual void mark(GC *gc, uint8_t clr);

        virtual ~AtomMapIterator()
        {
        }
};
//---------------------------------------------------------------------------

#if WITH_MEM_POOL
extern MemoryPool<Atom> g_atom_array_pool;
#endif

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
