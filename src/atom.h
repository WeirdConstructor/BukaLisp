#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace lilvm
{
//---------------------------------------------------------------------------

enum Type
{
    T_NIL,
    T_INT,
    T_DBL,
    T_BOOL,
    T_STR,
    T_SYM,
    T_KW,
    T_VEC,
    T_MAP,
    T_PRIM,
    T_CLOS
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
    uint8_t  m_gc_color;
    AtomVec *m_gc_next;

    size_t  m_alloc;
    size_t  m_len;
    Atom   *m_data;
    Sym    *m_debug_info;

    static size_t   s_alloc_count;

    AtomVec()
        : m_gc_next(nullptr), m_gc_color(0), m_alloc(0),
          m_len(0), m_data(nullptr), m_debug_info(nullptr)
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

    ~AtomVec();
};
//---------------------------------------------------------------------------

struct AtomMap
{
    uint8_t  m_gc_color;
    AtomMap *m_gc_next;
    std::unordered_map<std::string, Atom> m_map;

    AtomMap() : m_gc_next(nullptr), m_gc_color(0) { }

    void set(Sym *s, Atom &a);
    void set(const std::string &str, Atom &a);

    Atom at(Sym *s);
    Atom at(const std::string &str);
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
    } m_d;

    Atom() : m_type(T_NIL)
    {
        m_d.i = 0;
    }

    Atom(Type t) : m_type(t)
    {
        m_d.i = 0;
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

    Atom at(size_t i)
    {
        if (m_type != T_VEC) return Atom();
        return m_d.vec->at(i);
    }

    Atom at(const std::string &s)
    {
        if (m_type != T_MAP) return Atom();
        return m_d.map->at(s);
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
T *gc_list_sweep(T *list, uint8_t current_color, std::function<void(T *)> free_func)
{
    T *alive_list = nullptr;

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
    }

    return alive_list;
}
//---------------------------------------------------------------------------

typedef std::unordered_map<std::string, Sym *> StrSymMap;

class GC
{
    private:
        AtomVec *m_vectors;
        AtomMap *m_maps;
        Sym     *m_syms;

        uint8_t  m_current_color;

        std::vector<AtomVec *>  m_roots;
        std::vector<Sym *>      m_root_syms;
        StrSymMap               m_symtbl;

#define GC_SMALL_VEC_LEN     10
#define GC_MEDIUM_VEC_LEN    100

        AtomVec     *m_small_vectors;
        AtomVec     *m_medium_vectors;
        size_t       m_num_small_vectors;
        size_t       m_num_medium_vectors;

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

            num += new_num;
        }

        void mark_atom(Atom &at)
        {
            switch (at.m_type)
            {
                case T_MAP:
                    mark_map(at.m_d.map);
                    break;

                case T_VEC:
                    mark_vector(at.m_d.vec);
                    break;

                case T_SYM:
                    if (at.m_d.sym)
                        at.m_d.sym->m_gc_color = m_current_color;
                    break;
            }
        }

        void mark_vector(AtomVec *&vec)
        {
            if (!vec) return;

            if (vec->m_gc_color == m_current_color)
                return;

            vec->m_gc_color = m_current_color;

            for (size_t i = 0; i < vec->m_len; i++)
                mark_atom(vec->m_data[i]);
        }

        void mark_map(AtomMap *&map)
        {
            if (!map) return;

            if (map->m_gc_color == m_current_color)
                return;

            map->m_gc_color = m_current_color;

            for (auto p : map->m_map)
                mark_atom(p.second);
        }

        void mark()
        {
            m_current_color =
                m_current_color == GC_COLOR_WHITE
                ? GC_COLOR_BLACK
                : GC_COLOR_WHITE;

            for (auto &sym : m_root_syms)
                sym->m_gc_color = m_current_color;

            for (auto &root : m_roots)
                mark_vector(root);
        }

        void sweep()
        {
            m_syms =
                gc_list_sweep<Sym>(
                    m_syms,
                    m_current_color,
                    [this](Sym *cur)
                    {
                        auto it = m_symtbl.find(cur->m_str);
                        if (it != m_symtbl.end())
                            m_symtbl.erase(it);
                        delete cur;
                    });

            m_maps =
                gc_list_sweep<AtomMap>(
                    m_maps,
                    m_current_color,
                    [this](AtomMap *cur) { delete cur; });

            m_vectors =
                gc_list_sweep<AtomVec>(
                    m_vectors,
                    m_current_color,
                    [this](AtomVec *cur)
                    {
                        if (cur->m_alloc > GC_MEDIUM_VEC_LEN)
                        {
                            delete cur;
                        }
                        else if (cur->m_alloc > GC_SMALL_VEC_LEN)
                        {
                            cur->m_gc_next   = m_medium_vectors;
                            cur->m_gc_color  = GC_COLOR_FREE;
                            cur->m_len       = 0;
                            m_medium_vectors = cur;
                        }
                        else
                        {
                            cur->m_gc_next  = m_small_vectors;
                            cur->m_gc_color = GC_COLOR_FREE;
                            cur->m_len      = 0;
                            m_small_vectors = cur;
                        }
                    });
        }

        Sym *allocate_sym()
        {
            Sym *new_sym        = new Sym;
            new_sym->m_gc_color = m_current_color;
            new_sym->m_gc_next  = m_syms;
            m_syms              = new_sym;
            return new_sym;
        }

        Sym *allocate_permanent_sym()
        {
            Sym *new_sym = allocate_sym();
            m_root_syms.push_back(new_sym);

            return new_sym;
        }

    public:
        GC()
            : m_vectors(nullptr),
              m_maps(nullptr),
              m_syms(nullptr),
              m_current_color(GC_COLOR_WHITE),
              m_small_vectors(nullptr),
              m_medium_vectors(nullptr),
              m_num_small_vectors(0),
              m_num_medium_vectors(0)
        {
        }

        void add_root(AtomVec *root) { m_roots.push_back(root); }

        void collect()
        {
            mark();
            sweep();
        }

        Sym *new_symbol(const std::string &str)
        {
            auto it = m_symtbl.find(str);
            if (it == m_symtbl.end())
            {
                Sym *newsym = allocate_sym();
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

            return new_map;
        }

        AtomVec *allocate_vector(size_t vec_len)
        {
            AtomVec *new_vec = nullptr;

            if (vec_len > GC_MEDIUM_VEC_LEN)
            {
                new_vec = new AtomVec;
                new_vec->alloc(vec_len);
            }
            else if (vec_len > GC_SMALL_VEC_LEN)
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

            new_vec->init(m_current_color, vec_len);

            new_vec->m_gc_next = m_vectors;
            m_vectors          = new_vec;

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
            gc_list_sweep<Sym>(
                m_syms,
                GC_COLOR_DELETE,
                [](Sym *cur) { delete cur; });

            gc_list_sweep<AtomVec>(
                m_medium_vectors,
                GC_COLOR_DELETE,
                [this](AtomVec *cur) { delete cur; });

            gc_list_sweep<AtomVec>(
                m_small_vectors,
                GC_COLOR_DELETE,
                [this](AtomVec *cur) { delete cur; });

            gc_list_sweep<AtomVec>(
                m_vectors,
                GC_COLOR_DELETE,
                [this](AtomVec *cur) { delete cur; });

            gc_list_sweep<AtomMap>(
                m_maps,
                GC_COLOR_DELETE,
                [this](AtomMap *cur) { delete cur; });
        }
};
//---------------------------------------------------------------------------

}
