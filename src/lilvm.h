#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <functional>
#include "symtbl.h"

namespace lilvm
{
//---------------------------------------------------------------------------

// What do I need?
/*
    arithmetics, functions, primitives, external field objects,
    looping (by count, by elements of list), list manipulation,
    mathematic primitives, inclusion of rng somehow?
    modules/include/import
    (maybe rudimentary (define-library?) and (import ...)?)

    a way to call into a bukli-function!
        => global registry
            (garbage collector honors it)
        => function in VM() that calls a globally registered function
*/

// TODO: Add IS_REF
// TODO: Add PREPEND
// TODO: Add APPEND (O(n))
// TODO: Add SET_TAIL (ohne automatisches SET_REF!)
// TODO: Add SHALLOWCLONE
// TODO: Add DEEPCLONE

#define OPCODE_DEF(X) \
    X(NOP,               "NOP"              , 0) \
    X(TRC,               "TRC"              , 0) \
    X(DUP,               "DUP"              , 1) \
    X(SWAP,              "SWAP"             , 2) \
    X(POP,               "POP"              , 1) \
    X(PUSH_I,            "PUSH_I"           , 0) \
    X(PUSH_D,            "PUSH_D"           , 0) \
    X(PUSH_NIL,          "PUSH_NIL"         , 0) \
    X(PUSH_REF,          "PUSH_REF"         , 1) \
    X(PUSH_SYM,          "PUSH_SYM"         , 0) \
    X(DBG_DUMP_STACK,    "DBG_DUMP_STACK"   , 0) \
    X(DBG_DUMP_ENVSTACK, "DBG_DUMP_ENVSTACK", 0) \
    X(PUSH_GC_USE_COUNT, "PUSH_GC_USE_COUNT", 0) \
    \
    X(SET_REF,           "SET_REF"          , 2) \
    X(GET_REF,           "GET_REF"          , 1) \
    \
    X(ADD_I,             "ADD_I"            , 2) \
    X(ADD_D,             "ADD_D"            , 2) \
    X(SUB_I,             "SUB_I"            , 2) \
    X(SUB_D,             "SUB_D"            , 2) \
    X(MUL_I,             "MUL_I"            , 2) \
    X(MUL_D,             "MUL_D"            , 2) \
    X(DIV_I,             "DIV_I"            , 2) \
    X(DIV_D,             "DIV_D"            , 2) \
    \
    X(JMP,               "JMP"              , 0) \
    X(BR,                "BR"               , 0) \
    X(BR_IF,             "BR_IF"            , 1) \
    X(BR_NIF,            "BR_NIF"           , 1) \
    X(EQ_I,              "EQ_I"             , 2) \
    X(EQ_D,              "EQ_D"             , 2) \
    X(LT_D,              "LT_D"             , 2) \
    X(LE_D,              "LE_D"             , 2) \
    X(GT_D,              "GT_D"             , 2) \
    X(GE_D,              "GE_D"             , 2) \
    X(LT_I,              "LT_I"             , 2) \
    X(LE_I,              "LE_I"             , 2) \
    X(GT_I,              "GT_I"             , 2) \
    X(GE_I,              "GE_I"             , 2) \
    \
    X(IS_LIST,           "IS_LIST"          , 1) \
    X(IS_NIL,            "IS_NIL"           , 1) \
    X(PUSH_LIST,         "PUSH_LIST"        , 0) \
    X(TAIL,              "TAIL"             , 1) \
    X(LIST_LEN,          "LIST_LEN"         , 1) \
    X(IS_VEC,            "IS_VEC"           , 1) \
    X(VEC_LEN,           "VEC_LEN"          , 1) \
    X(PUSH_VEC,          "PUSH_VEC"         , 0) \
    X(AT_VEC,            "AT_VEC"           , 2) \
    \
    X(PUSH_ENV,          "PUSH_ENV"         , 0) \
    X(POP_ENV,           "POP_ENV"          , 0) \
    X(FILL_ENV,          "FILL_ENV"         , 0) \
    X(SET_ENV,           "SET_ENV"          , 0) \
    X(GET_ENV,           "GET_ENV"          , 0) \
    X(PUSH_CLOS_CONT,    "PUSH_CLOS_CONT"   , 0) \
    X(PUSH_PRIM,         "PUSH_PRIM"        , 0) \
    \
    X(GET,               "GET"              , 1) \
    X(SET,               "SET"              , 1) \
    X(CALL,              "CALL"             , 1) \
    X(TAILCALL,          "TAILCALL"         , 1) \
    X(RETURN,            "RETURN"           , 2) \

enum OPCODE : int8_t
{
#define X(a, b, c) a,
    OPCODE_DEF(X)
#undef X
    __LAST_OPCODE
};
//---------------------------------------------------------------------------

const char *OPCODE_NAMES[];

const char *OPCODE2NAME(OPCODE c);

struct Operation;

struct Operation
{
    OPCODE  m_op;
    size_t  m_min_arg_cnt;
    Sym    *m_debug_sym;

    static size_t min_arg_cnt_for_op(OPCODE o);
    static size_t min_arg_cnt_for_op(OPCODE o, Operation &op);

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

    Operation *m_next;

    Operation(OPCODE o)
        : m_op(o),
          m_next(nullptr)
    {
        m_min_arg_cnt = min_arg_cnt_for_op(o);
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
    T_CLOS,
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
        virtual Datum *call(VM *vm, Datum *field, Datum *args)   = 0;
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
    typedef std::function<
        Datum*(VM *vm, std::vector<Datum *> &stack, int argc)> PrimFunc;

    static unsigned int s_instance_counter;

    Type    m_type;
#define DT_MARK_FREE    0x08
#define DT_MARK_WHITE   0x01
#define DT_MARK_BLACK   0x00
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

    void release_memory()
    {
        if (m_type == T_VEC && m_d.vec.elems)
            delete[] m_d.vec.elems;
        else if (m_type == T_CLOS && m_d.vec.elems)
            delete[] m_d.vec.elems;
        else if (m_type == T_EXT && m_d.ext)
        {
            if (m_d.ext->unref())
                delete m_d.ext;
        }
    }

    ~Datum()
    {
        release_memory();
        s_instance_counter--;
    }

    // TODO: pass more state about serialization! Extra method (to_string_list...?)
    std::string to_string(bool in_list = false)
    {
        std::string ret;

        if (!in_list && m_next)
            ret = "[";

        if (m_type == T_INT)
            ret += std::to_string(m_d.i);
        else if (m_type == T_DBL)
            ret += std::to_string(m_d.d);
        else if (m_type == T_SYM)
            ret += m_d.sym->m_str;
        else if (m_type == T_PRIM)
            ret += "#<primitive:" + std::to_string((uint64_t) m_d.func) + ">";
        else if (m_type == T_CLOS)
            ret += "#<closure:" + std::to_string((uint64_t) this) + ":" + std::to_string(m_d.vec.len) + ">";
        else if (m_type == T_VEC)
        {
            SimpleVec &sv = m_d.vec;
            ret += "#[";
            for (size_t i = 0; i < sv.len; i++)
            {
                if (sv.elems[i]) ret += sv.elems[i]->to_string();
                else             ret += "NIL";
                if (i != (sv.len - 1))
                {
                    ret += ", ";
                }
            }
            ret += "]";
        }
        else if (m_type == T_REF)
        {
            if (m_next || in_list)
                ret += (m_d.ref ? m_d.ref->to_string() : "NIL");
            else
                ret += "[" + (m_d.ref ? m_d.ref->to_string() : "NIL") + "]";
        }
        else // T_NIL
        {
            ret += "NIL";
        }

        if (in_list)
            ret = " " + ret;

        if (m_next)
        {
            if (in_list)
                return ret + m_next->to_string(true);
            else
                return ret + m_next->to_string(true) + "]";
        }

        return ret;
    }

    void for_each_in_list(std::function<bool(Datum *dt)> func)
    {
        Datum *dt = this;
        if (!dt->m_next)
        {
            if (dt->m_type != T_NIL)
                func(dt);
            return;
        }

        while (dt)
        {
            if (!func(dt))
                break;
            dt = dt->m_next;
        }
    }

    void convert_to_ref(Datum *dt)
    {
        release_memory();
        m_type = T_REF;
        m_d.ref = dt;
    }

    inline void clear_contents()
    {
        m_next  = nullptr;
        m_marks = DT_MARK_BLACK;
        m_d.i   = 0;
    }

    inline void mark(uint8_t mark)
    {
        if ((m_marks & 0x0F) == (0x0F & mark))
            return;
        m_marks = (m_marks & 0xF0) | (0x0F & mark);
        if (m_next) m_next->mark(mark);

        if (m_type == T_VEC || m_type == T_CLOS)
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

        // TODO: Mark T_SYM!
    }

    inline uint8_t get_mark() { return m_marks & 0x0F; }

    inline int64_t to_int()
    { return m_type == T_INT ? m_d.i
             : m_type == T_DBL ? static_cast<int64_t>(m_d.d)
             : m_type == T_SYM ? (int64_t) m_d.sym
             : m_type == T_NIL ? 0
             : m_type == T_REF ? m_d.ref->to_int()
             :                   m_d.i; }
    inline double to_dbl()
    { return m_type == T_DBL ? m_d.d
             : m_type == T_INT ? static_cast<double>(m_d.i)
             : m_type == T_SYM ? (double) (int64_t) m_d.sym
             : m_type == T_NIL ? 0.0
             : m_type == T_REF ? m_d.ref->to_dbl()
             :                   m_d.d; }
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

        size_t                                 m_next_collect_countdown;
        bool                                   m_need_collect;
        size_t                                 m_min_pool_size;

        inline void put_on_free_list(Datum *dt)
        {
            dt->release_memory();
            dt->m_marks = DT_MARK_FREE;
            dt->m_next = m_free_list;
            m_free_list = dt;
            m_datums_in_use--;
        }

        void grow();
        void reclaim(uint8_t mark);


    public:
        DatumPool(size_t min_pool_size)
            : m_free_list(nullptr), m_datums_in_use(0), m_need_collect(false),
              m_min_pool_size(min_pool_size),
              m_next_collect_countdown(0)
        {
        }

        void collect_if_needed()
        {
            if (m_need_collect)
            {
                collect();
                m_need_collect = false;
            }
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
            if (m_next_collect_countdown == 0)
            {
                m_need_collect = true;
            }
            else
                m_next_collect_countdown--;

            if (!m_free_list)
            {
                m_need_collect = true;
                grow();
            }

            Datum *newdt = m_free_list;
            m_free_list = newdt->m_next;
            m_datums_in_use++;

            newdt->clear_contents();
            newdt->mark(m_cur_mark_color);
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
            m_cur_mark_color =
                m_cur_mark_color == DT_MARK_BLACK
                ? DT_MARK_WHITE
                : DT_MARK_BLACK;

            for (auto &root_set : m_root_sets)
            {
                for (auto &dt : *root_set)
                {
                    dt->mark(m_cur_mark_color);
                }
            }

            reclaim(m_cur_mark_color);

            // TODO: Reclaim and set new mark in SymTbl!
        }
};
//---------------------------------------------------------------------------

class DatumException : public std::exception
{
    private:
        std::string m_err;
    public:
        DatumException(const std::string &err)
        {
            m_err = err;
        }

        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~DatumException() { }
};
//---------------------------------------------------------------------------

class VMException : public std::exception
{
    private:
        std::string m_err;
    public:
        VMException(const std::string &err, Operation &op)
        {
            std::stringstream ss;
            ss << "[" << op.m_debug_sym->m_str << "]@"
               << OPCODE2NAME(op.m_op)
               << ": " << err;
            m_err = ss.str();
        }

        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~VMException() { }
};
//---------------------------------------------------------------------------

#define MAX_STACK_SIZE 1000000
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
              m_ext_factory(ext_fac),
              m_datum_pool(25)
        {
            m_datum_pool.add_root_sets(&m_stack);
            m_datum_pool.add_root_sets(&m_env_stack);

            init_core_primitives();
        }

#       define STK_PUSH(datum)  do { m_stack.push_back(datum); } while(0)
#       define STK_POP()        do { m_stack.pop_back(); } while(0)
#       define STK_AT(o)        (m_stack[local_stack_size - (o)])
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

        Datum *new_dt_clos(int64_t op_idx);
        Datum *new_dt_int(int64_t i);
        Datum *new_dt_dbl(double d);
        Datum *new_dt_external(Sym *ext_type, Datum *args);
        Datum *new_dt_prim(Datum::PrimFunc *func);
        Datum *new_dt_sym(Sym *sym);
        Datum *new_dt_nil();
        Datum *new_dt_ref(Datum *dt);

        void pop(size_t cnt);
        void push(Datum *dt);

        void dump_stack(const std::string &msg, Operation &op);
        void dump_envstack(const std::string &msg, Operation &op);
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
