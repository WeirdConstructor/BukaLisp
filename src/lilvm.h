#include <iostream>
#include <vector>
#include <string>

namespace lilvm
{
//---------------------------------------------------------------------------

enum OPCODE
{
    NOP,
    PUSH_I,
    PUSH_D,
    DBG_DUMP_STACK,
    ADD_I,
    ADD_D,

    // TODO:
    GOTO,
    BRANCH_IF,  // x != 0
    LT_D,       // (DBL) <
    LE_D,       // (DBL) <=
    GT_D,       // (DBL) >
    GE_D,       // (DBL) >=
    LT_I,       // (INT) <
    LE_I,       // (INT) <=
    GT_I,       // (INT) >
    GE_I,       // (INT) >=

    DEF_ARGS, // pop data from the stack, put them into an array
    CALL,
    RET
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

    Operation  *m_next;

    Operation(OPCODE o)
        : m_op(o),
          m_next(nullptr)
    {
        m_1.i = 0;
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

    inline int64_t to_int()
    { return m_type == T_INT ? m_d.i : static_cast<int64_t>(m_d.d); }
    inline double to_dbl()
    { return m_type == T_DBL ? m_d.d : static_cast<double>(m_d.i); }

    Datum *m_next;

    Datum(Type t) : m_type(t), m_next(nullptr)
    {
        m_d.i = 0;
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
};
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

class VM
{
    private:
        Operation           *m_prog;
        Operation           *m_last;
        Operation           *m_cur;

        Datum              **m_stack;
        size_t               m_stack_size;
        size_t               m_stack_safetyzone;
        Datum              **m_stack_top;
        Datum              **m_stack_max;
        Datum              **m_stack_bottom;

    public:
        VM()
            : m_prog(nullptr),
              m_last(nullptr),
              m_cur(nullptr),
              m_stack_size(1024),

              // needs to be bigger than one operation can push onto
              // the stack!
              m_stack_safetyzone(10)
        {
            size_t stack_init_size = m_stack_size + m_stack_safetyzone;
            m_stack_bottom = m_stack = new Datum*[stack_init_size];

            for (size_t i = 0; i < stack_init_size; i++)
                m_stack[i] = nullptr;

            m_stack_top = m_stack;
            m_stack_max = &(m_stack[m_stack_size - 1]);
        }

#       define STK_PUSH(datum)  do { *m_stack_top = (datum); m_stack_top++; } while(0);
#       define STK_POPN(n)      do { m_stack_top -= n; } while(0);
#       define STK_AT(o)        (*(m_stack_top - o))
#       define GET_STK_SIZE     ((size_t) (m_stack_top - m_stack_bottom))

        inline bool check_stack_overflow()
        {
            return m_stack_top >= m_stack_max;
        }

        Datum *new_datum(Type t);
        Datum *new_dt_int(int64_t i);
        Datum *new_dt_dbl(double d);
        void dump_stack(const std::string &msg);
        void log(const std::string &error);
        void append(Operation *op);
        void run();

        ~VM() { }
};
//---------------------------------------------------------------------------

}
