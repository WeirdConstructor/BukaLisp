#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <functional>
#include "atom.h"
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
// TODO: Add SHALLOWCLONE
// TODO: Add DEEPCLONE

#define OPCODE_DEF(X) \
    X(LOAD_NIL,          "LOAD_NIL"         , 0) \
    X(LOAD_I,            "LOAD_I"           , 0) \
    X(LOAD_D,            "LOAD_D"           , 0) \
    X(LOAD_SYM,          "LOAD_SYM"         , 0) \
    X(LOAD_PRIM,         "LOAD_PRIM"        , 0) \
    X(LOAD_VEC,          "LOAD_VEC"         , 0) \
    X(ADD,               "ADD"              , 0) \
    X(SUB,               "SUB"              , 0) \
    X(NOP,               "NOP"              , 0) \
    X(TRC,               "TRC"              , 0) \
    X(RETURN,            "RETURN"           , 0) \
    X(JMP,               "JMP"              , 0) \
    X(VEC_SET,           "VEC_SET"          , 0) \
    X(VEC_GET,           "VEC_GET"          , 0) \
    \
    \
    \
    \
    \
    \
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
    X(ADD_I,             "ADD_I"            , 2) \
    X(ADD_D,             "ADD_D"            , 2) \
    X(SUB_I,             "SUB_I"            , 2) \
    X(SUB_D,             "SUB_D"            , 2) \
    X(MUL_I,             "MUL_I"            , 2) \
    X(MUL_D,             "MUL_D"            , 2) \
    X(DIV_I,             "DIV_I"            , 2) \
    X(DIV_D,             "DIV_D"            , 2) \
    \
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
    \
    X(IS_MAP,            "IS_MAP"           , 1) \
    X(MAP_LEN,           "MAP_LEN"          , 1) \
    X(PUSH_MAP,          "PUSH_MAP"         , 0) \
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

enum OPCODE : int8_t
{
#define X(a, b, c) a,
    OPCODE_DEF(X)
#undef X
    __LAST_OPCODE
};
//---------------------------------------------------------------------------

const char *OPCODE_NAMES[];

struct Operation;

struct Operation
{
    OPCODE  m_op;
    size_t  m_min_arg_cnt;
    Sym    *m_debug_sym;

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

    union {
        int64_t i;
        double d;
        Sym   *sym;
    } m_3;

    Operation *m_next;

    const char *name() const { return OPCODE_NAMES[m_op]; }

    Operation(OPCODE o)
        : m_op(o),
          m_next(nullptr)
    {
        m_1.i = 0;
        m_2.i = 0;
        m_3.i = 0;
    }
};
//---------------------------------------------------------------------------

class VM;

//class External
//{
//    private:
//        unsigned int m_ref_count;
//
//    public:
//        External() : m_ref_count(0) { }
//        virtual ~External() { }
//
//        void ref() { m_ref_count++; }
//        bool unref()
//        {
//            if (m_ref_count >= 1) m_ref_count--;
//            return m_ref_count == 0;
//        }
//
//        virtual Datum *get(VM *vm, Datum *field)                = 0;
//        virtual void   set(VM *vm, Datum *field, Datum *val)    = 0;
//        virtual Datum *call(VM *vm, Datum *field, Datum *args)  = 0;
//};
////---------------------------------------------------------------------------
//
//class ExternalFactory
//{
//    public:
//        ExternalFactory() { }
//        virtual ~ExternalFactory() { }
//        virtual External *operator()(const std::string &type, Datum *args)
//        { return nullptr; }
//};
////---------------------------------------------------------------------------

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
        std::vector<Datum *>     m_env_stack;
        std::vector<Datum::PrimFunc *>
                                 m_primitives;

        int                      m_ip;

        bool                     m_enable_trace_log;

//        ExternalFactory         *m_ext_factory;
        SymTblPtr                m_symtbl;

        OpsMarker                m_opsmarker;

    public:
        VM() // ExternalFactory *ext_fac = nullptr)
            : m_enable_trace_log(false),
              m_ip(0),
//              m_ext_factory(ext_fac),
              m_datum_pool(1000)
        {
            m_opsmarker.m_ops = &m_ops;

            m_datum_pool.add_root_sets(&m_env_stack);

            m_env_stack.reserve(128);

            m_symtbl = std::make_shared<SymTable>(1000);
            m_datum_pool.add_markable(m_symtbl.get());
            m_datum_pool.add_markable(&m_opsmarker);

            init_core_primitives();
        }

        SymTblPtr               get_symtbl() { return m_symtbl; }

//        void init_core_primitives();
//
//        void register_primitive(size_t id, Datum::PrimFunc prim)
//        {
//            if (id >= m_primitives.size())
//            {
//                size_t old_size = m_primitives.size();
//                m_primitives.resize((id + 1) * 2);
//                for (size_t i = old_size; i < m_primitives.size(); i++)
//                    m_primitives[i] = nullptr;
//            }
//
//            m_primitives[id] = new Datum::PrimFunc(prim);
//        }

//        Datum *new_dt_clos(int64_t op_idx);
//        Datum *new_dt_int(int64_t i);
//        Datum *new_dt_dbl(double d);
//        Datum *new_dt_external(Sym *ext_type, Datum *args);
//        Datum *new_dt_prim(Datum::PrimFunc *func);
//        Datum *new_dt_sym(Sym *sym);
//        Datum *new_dt_nil();
//        Datum *new_dt_ref(Datum *dt);
//        Datum *new_dt_map(size_t reserve);

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
