#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include "symtbl.h"

namespace lilvm
{
//---------------------------------------------------------------------------

enum OPCODE : int8_t
{
    NOP,
    TRC,
    PUSH_I,
    PUSH_D,
    PUSH_SYM,
    DBG_DUMP_STACK,
    DBG_DUMP_ENVSTACK,
    PUSH_GC_USE_COUNT,
    ADD_I,
    ADD_D,
    SUB_I,
    SUB_D,
    MUL_I,
    MUL_D,
    DIV_I,
    DIV_D,

    // TODO:
    JMP,
    BR,
    BR_IF,  // x != 0
    LT_D,   // (DBL) <
    LE_D,   // (DBL) <=
    GT_D,   // (DBL) >
    GE_D,   // (DBL) >=
    LT_I,   // (INT) <
    LE_I,   // (INT) <=
    GT_I,   // (INT) >
    GE_I,   // (INT) >=

    POP,
    PUSH_ENV,
    POP_ENV,
    SET_ENV,   // saves into env-stack
    GET_ENV,   // reads from env-stack
    PUSH_FUNC, // captures env-stack
    PUSH_PRIM, // pushes primitive operation on the stack for calling
    CALL_FUNC,
    // for funcs: restores env-stack, needs to push a new env for storing args
    // for prims: internally it needs to pop the given args off the stack
    RETURN
};
//---------------------------------------------------------------------------

const char *OPCODE_NAMES[];

struct Operation;

struct Operation
{
    OPCODE  m_op;

    union {
        int64_t i;
        double d;
        Sym   *sym;
    } m_1;

    union {
        int64_t i;
        double d;
        Sym   *sym;
    } m_2;

    Operation  *m_next;

    Operation(OPCODE o)
        : m_op(o),
          m_next(nullptr)
    {
        m_1.i = 0;
        m_2.i = 0;
    }
};
//---------------------------------------------------------------------------

enum Type
{
    T_NIL,
    T_INT,
    T_DBL,
    T_VEC,
    T_REF,
    T_FUNC,
    T_PRIM,
    T_SYM,
    T_EXT
};
//---------------------------------------------------------------------------

struct Datum;

struct SimpleVec
{
    size_t  len;
    Datum **elems;
};
//---------------------------------------------------------------------------

class VM;

class External
{
    private:
        unsigned int m_ref_count;

    public:
        External() : m_ref_count(0) { }
        virtual ~External() { }

        void ref() { m_ref_count++; }
        bool unref()
        {
            if (m_ref_count >= 1) m_ref_count--;
            return m_ref_count == 0;
        }

        virtual Datum *get(VM *vm, Datum *field)                = 0;
        virtual void   set(VM *vm, Datum *field, Datum *val)    = 0;
        virtual void  call(VM *vm, Datum *field, Datum *args)   = 0;
};
//---------------------------------------------------------------------------

class ExternalFactory
{
    public:
        ExternalFactory() { }
        virtual ~ExternalFactory() { }
        virtual External *operator()(const std::string &type, Datum *args)
        { return nullptr; }
};
//---------------------------------------------------------------------------

struct Datum
{
    typedef std::function<Datum*(VM *vm, std::vector<Datum *> &stack,
                                 int argc, bool retval)> PrimFunc;

    static unsigned int s_instance_counter;

    Type    m_type;
    uint8_t m_marks;

    union {
        int64_t    i;
        double     d;
        SimpleVec  vec;
        PrimFunc  *func;
        Datum     *ref;
        External  *ext;
        Sym       *sym;
    } m_d;

    Datum *m_next;

    Datum() : m_type(T_NIL), m_next(nullptr), m_marks(0)
    {
        s_instance_counter++;
        m_d.i   = 0;
        m_d.ref = nullptr;
    }

    Datum(Type t) : m_type(t), m_next(nullptr), m_marks(0)
    {
        s_instance_counter++;
        m_d.i   = 0;
        m_d.ref = nullptr;
    }

    ~Datum()
    {
        s_instance_counter--;

        if (m_type == T_VEC && m_d.vec.elems)
            delete[] m_d.vec.elems;
        else if (m_type == T_EXT && m_d.ext)
        {
            if (m_d.ext->unref())
                delete m_d.ext;
        }
    }

    std::string to_string()
    {
        if (m_type == T_INT)
            return std::to_string(m_d.i);
        else if (m_type == T_DBL)
            return std::to_string(m_d.d);
        else if (m_type == T_SYM)
            return m_d.sym->m_str;
        else // T_NIL
        {
            return "NIL";
        }
    }

    inline void clear_contents()
    {
        m_next  = nullptr;
        m_marks = 0;
        m_d.i   = 0;
    }

    inline void mark(uint8_t mark)
    {
        if ((m_marks & 0x0F) == (0x0F & mark))
            return;
        m_marks = (m_marks & 0xF0) | (0x0F & mark);
        if (m_next) m_next->mark(mark);

        if (m_type == T_VEC)
        {
            for (size_t i = 0; i < m_d.vec.len; i++)
            {
                Datum *dt = m_d.vec.elems[i];
                if (dt) dt->mark(mark);
            }
        }
        else if (m_type == T_REF)
        {
            if (m_d.ref)
                m_d.ref->mark(mark);
        }
    }

    inline uint8_t get_mark() { return m_marks & 0x0F; }

    inline int64_t to_int()
    { return m_type == T_INT ? m_d.i :
             m_type == T_DBL ? static_cast<int64_t>(m_d.d) :
             m_type == T_SYM ? (int64_t) m_d.sym
                             : m_d.i; }
    inline double to_dbl()
    { return m_type == T_DBL ? m_d.d :
             m_type == T_INT ? static_cast<double>(m_d.i) :
             m_type == T_SYM ? (double) (int64_t) m_d.sym
                             : m_d.d; }
};
//---------------------------------------------------------------------------

class DatumPool
{
    private:
        std::vector<std::unique_ptr<Datum[]>>  m_chunks;
        // XXX: m_allocated just keeps extra pointers to Datum in m_chunks!
        // no need to free it!
        std::vector<Datum *>                   m_allocated;
        Datum                                 *m_free_list;

        std::vector<std::vector<Datum *> *>    m_root_sets;

        unsigned int                           m_datums_in_use;
        int                                    m_cur_mark_color;

        inline void put_on_free_list(Datum *dt)
        {
            if (dt->m_type == T_VEC)
            {
                if (dt->m_d.vec.elems)
                    delete[] dt->m_d.vec.elems;

                dt->m_d.vec.len   = 0;
                dt->m_d.vec.elems = nullptr;
            }

            dt->m_next = m_free_list;
            m_free_list = dt;
            m_datums_in_use--;
        }

        void grow();
        void reclaim(uint8_t mark);


    public:
        DatumPool()
            : m_free_list(nullptr), m_datums_in_use(0)
        {
        }

        Datum *new_vector(Type t, size_t len)
        {
            Datum *dt         = new_datum(t);
            dt->m_d.vec.len   = len;
            Datum **elems     = (Datum **) new Datum*[len];
            dt->m_d.vec.elems = elems;
            for (size_t i = 0; i < len; i++)
                elems[i] = nullptr;
            return dt;
        }

        Datum *new_datum(Type t)
        {
            if (!m_free_list)
            {
                collect();

                if (!m_free_list)
                    grow();
            }
            m_datums_in_use++;

            Datum *newdt = m_free_list;
            m_free_list = newdt->m_next;

            newdt->clear_contents();
            newdt->m_type = t;
            return newdt;
        }

        unsigned int get_datums_in_use_count() const { return m_datums_in_use; }

        void add_root_sets(std::vector<Datum *> *rs)
        {
            m_root_sets.push_back(rs);
        }

        void collect()
        {
            m_cur_mark_color = m_cur_mark_color == 0 ? 1 : 0;

            for (auto &root_set : m_root_sets)
            {
                for (auto &dt : *root_set)
                {
                    dt->mark(m_cur_mark_color);
                }
            }

            reclaim(m_cur_mark_color);
        }
};
//---------------------------------------------------------------------------

class VM
{
    private:
        DatumPool                m_datum_pool;

        std::vector<Operation *> m_ops;
        std::vector<Datum *>     m_stack;
        std::vector<Datum *>     m_env_stack;
        std::vector<Datum::PrimFunc *>
                                 m_primitives;

        int                      m_ip;

        bool                     m_enable_trace_log;
        uint8_t                  m_cur_mark_color;

        ExternalFactory         *m_ext_factory;

    public:
        VM(ExternalFactory *ext_fac = 0)
            : m_enable_trace_log(false),
              m_ip(0),
              m_cur_mark_color(0),
              m_ext_factory(ext_fac)
        {
            m_datum_pool.add_root_sets(&m_stack);
            m_datum_pool.add_root_sets(&m_env_stack);

            init_core_primitives();
        }

#       define STK_PUSH(datum)  do { m_stack.push_back(datum); } while(0);
#       define STK_POP()        do { m_stack.pop_back(); } while(0);
#       define STK_AT(o)        (m_stack[m_stack.size() - o])
#       define GET_STK_SIZE     (m_stack.size())

        void init_core_primitives();

        void register_primitive(size_t id, Datum::PrimFunc prim)
        {
            if (id >= m_primitives.size())
            {
                size_t old_size = m_primitives.size();
                m_primitives.resize((id + 1) * 2);
                for (size_t i = old_size; i < m_primitives.size(); i++)
                    m_primitives[i] = nullptr;
            }

            m_primitives[id] = new Datum::PrimFunc(prim);
        }

        inline bool check_stack_overflow()
        {
            // TODO: Check some limit, so we dont gobble up the machine!
            return false;
        }

        Datum *new_dt_int(int64_t i);
        Datum *new_dt_dbl(double d);
        Datum *new_dt_external(Sym *ext_type, Datum *args);
        Datum *new_dt_prim(Datum::PrimFunc *func);
        Datum *new_dt_sym(Sym *sym);

        void pop(size_t cnt);
        void push(Datum *dt);

        void dump_stack(const std::string &msg);
        void dump_envstack(const std::string &msg);
        void log(const std::string &error);
        void append(Operation *op);
        Datum *run();

        ~VM()
        {
            for (auto &p : m_primitives) if (p) delete p;
        }
};
//---------------------------------------------------------------------------

}
