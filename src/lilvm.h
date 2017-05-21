#include <iostream>
#include <vector>
#include <string>

namespace lilvm
{
//---------------------------------------------------------------------------

enum OPCODE
{
    NOP,
    TRC,
    PUSH_I,
    PUSH_D,
    DBG_DUMP_STACK,
    ADD_I,
    ADD_D,

    // TODO:
    JMP,
    BR_IF,  // x != 0
    LT_D,   // (DBL) <
    LE_D,   // (DBL) <=
    GT_D,   // (DBL) >
    GE_D,   // (DBL) >=
    LT_I,   // (INT) <
    LE_I,   // (INT) <=
    GT_I,   // (INT) >
    GE_I,   // (INT) >=

    PUSH_ENV,
    POP_ENV,
    SET_ENV,   // saves into env-stack
    GET_ENV,   // reads from env-stack
    PUSH_FUNC, // captures env-stack
    CALL_FUNC, // restores env-stack, needs to push a new env for storing args
    RETURN
};
//---------------------------------------------------------------------------

struct Operation;

struct Operation
{
    OPCODE  m_op;

    union {
        int64_t i;
        double d;
    } m_1;

    union {
        int64_t i;
        double d;
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
    T_DBL
};
//---------------------------------------------------------------------------

struct Datum;

struct Datum
{
    Type m_type;

    union {
        int64_t i;
        double d;
    } m_d;

    Datum *m_next;

    Datum(Type t) : m_type(t), m_next(nullptr)
    {
        m_d.i    = 0;
    }

    std::string to_string()
    {
        if (m_type == T_INT)
            return std::to_string(m_d.i);
        else if (m_type == T_DBL)
            return std::to_string(m_d.d);
        else // T_NIL
        {
            return "NIL";
        }
    }

    inline int64_t to_int()
    { return m_type == T_INT ? m_d.i : static_cast<int64_t>(m_d.d); }
    inline double to_dbl()
    { return m_type == T_DBL ? m_d.d : static_cast<double>(m_d.i); }
};
//---------------------------------------------------------------------------

struct EnvFrame
{
    size_t     m_return_adr;
    size_t     m_len;
    Datum    **m_env;

    EnvFrame(size_t s)
        : m_return_adr(0), m_len(s), m_env(nullptr)
    {
        m_env = new Datum*[m_len];
        for (int i = 0; i < m_len; i++)
            m_env[i] = nullptr;
    }

    ~EnvFrame() { delete m_env; }
};
//---------------------------------------------------------------------------

class VM
{
    private:
        std::vector<Operation *> m_ops;
        std::vector<Datum *>     m_stack;
        std::vector<EnvFrame *>  m_env_stack;

        size_t                   m_ip;

        bool                     m_enable_trace_log;

    public:
        VM()
            : m_enable_trace_log(false),
              m_ip(0)
        {
        }

#       define STK_PUSH(datum)  do { m_stack.push_back(datum); } while(0);
#       define STK_POP()        do { m_stack.pop_back(); } while(0);
#       define STK_AT(o)        (m_stack[m_stack.size() - o])
#       define GET_STK_SIZE     (m_stack.size())

        inline bool check_stack_overflow()
        {
            // TODO: Check some limit, so we dont gobble up the machine!
            return false;
        }

        Datum *new_datum(Type t);
        Datum *new_dt_int(int64_t i);
        Datum *new_dt_dbl(double d);
        void dump_stack(const std::string &msg);
        void log(const std::string &error);
        void append(Operation *op);
        Datum *run();

        ~VM() { }
};
//---------------------------------------------------------------------------

}
