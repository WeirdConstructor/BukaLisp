#include <iostream>
#include <vector>
#include <string>

namespace lilvm
{
//---------------------------------------------------------------------------

enum OPCODE : int8_t
{
    NOP,
    TRC,
    PUSH_I,
    PUSH_D,
    DBG_DUMP_STACK,
    DBG_DUMP_ENVSTACK,
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

    PUSH_ENV,
    POP_ENV,
    SET_ENV,   // saves into env-stack
    GET_ENV,   // reads from env-stack
    PUSH_FUNC, // captures env-stack
    CALL_FUNC, // restores env-stack, needs to push a new env for storing args
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
    T_DBL,
    T_VEC,
    T_FUNC
};
//---------------------------------------------------------------------------

struct Datum;

struct SimpleVec
{
    size_t  len;
    Datum **elems;
};
//---------------------------------------------------------------------------


struct Datum
{
    Type    m_type;
    uint8_t m_marks;

    union {
        int64_t i;
        double d;
        // TODO: Think about elevating env frames to fulltypes, maybe
        //       just use datum vectors for them? just so we can reuse
        //       the garbace collector.
        SimpleVec vec;
    } m_d;

    Datum *m_next;

    Datum() : m_type(T_NIL), m_next(nullptr), m_marks(0)
    {
        m_d.i = 0;
    }

    Datum(Type t) : m_type(t), m_next(nullptr), m_marks(0)
    {
        m_d.i = 0;
    }

    ~Datum()
    {
        if (   m_type == T_VEC
            && m_d.vec.elems)
            delete[] m_d.vec.elems;
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
    }

    inline uint8_t get_mark() { return m_marks & 0x0F; }

    inline int64_t to_int()
    { return m_type == T_INT ? m_d.i : static_cast<int64_t>(m_d.d); }
    inline double to_dbl()
    { return m_type == T_DBL ? m_d.d : static_cast<double>(m_d.i); }
};
//---------------------------------------------------------------------------

class DatumPool
{
    private:
        std::vector<Datum *> m_allocated;
        Datum               *m_free_list;

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
        }

        void grow();

    public:
        DatumPool()
            : m_free_list(nullptr)
        {
        }

        void reclaim(uint8_t mark)
        {
            for (auto dt : m_allocated)
            {
                if (dt->get_mark() != mark)
                    put_on_free_list(dt);
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
            if (!m_free_list) grow();
            Datum *newdt = m_free_list;
            m_free_list = newdt->m_next;

            newdt->clear_contents();
            newdt->m_type = t;
            return newdt;
        }

        ~DatumPool()
        {
            for (auto dt : m_allocated)
            {
                delete dt;
            }
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

        int                      m_ip;

        bool                     m_enable_trace_log;
        uint8_t                  m_cur_mark_color;

    public:
        VM()
            : m_enable_trace_log(false),
              m_ip(0),
              m_cur_mark_color(0)
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

        Datum *new_dt_int(int64_t i);
        Datum *new_dt_dbl(double d);

        void dump_stack(const std::string &msg);
        void dump_envstack(const std::string &msg);
        void log(const std::string &error);
        void append(Operation *op);
        Datum *run();

        ~VM() { }
};
//---------------------------------------------------------------------------

}
