#include "lilvm.h"

using namespace std;

int test() { return 11; }

namespace lilvm
{
//---------------------------------------------------------------------------

const char *OPCODE_NAMES[] = {
    "NOP",
    "TRC",
    "PUSH_I",
    "PUSH_D",
    "DBG_DUMP_STACK",
    "DBG_DUMP_ENVSTACK",
    "ADD_I",
    "ADD_D",

    "JMP",
    "BR",
    "BR_IF",
    "LT_D",
    "LE_D",
    "GT_D",
    "GE_D",
    "LT_I",
    "LE_I",
    "GT_I",
    "GE_I",

    "PUSH_ENV",
    "POP_ENV",
    "SET_ENV",
    "GET_ENV",
    "PUSH_FUNC",
    "CALL_FUNC",
    "RETURN",
    ""
};
//---------------------------------------------------------------------------

void VM::log(const string &error)
{
    cout << error << endl;
}
//---------------------------------------------------------------------------

void VM::append(Operation *op)
{
    m_ops.push_back(op);
}
//---------------------------------------------------------------------------

const char *OPCODE2NAME(OPCODE c)
{
    return OPCODE_NAMES[c];
    switch (c)
    {
        case NOP:               return "NOP";
        case TRC:               return "TRC";
        case PUSH_I:            return "PUSH_I";
        case PUSH_D:            return "PUSH_D";
        case DBG_DUMP_STACK:    return "DBG_DUMP_STACK";
        case DBG_DUMP_ENVSTACK: return "DBG_DUMP_ENVSTACK";
        case ADD_I:             return "ADD_I";
        case ADD_D:             return "ADD_D";
        case JMP:               return "JMP";
        case BR_IF:             return "BRANCH_IF";
        case LT_D:              return "LT_D";
        case LE_D:              return "LE_D";
        case GT_D:              return "GT_D";
        case GE_D:              return "GE_D";
        case LT_I:              return "LT_I";
        case LE_I:              return "LE_I";
        case GT_I:              return "GT_I";
        case GE_I:              return "GE_I";
        case PUSH_ENV:          return "PUSH_ENV";
        case POP_ENV:           return "POP_ENV";
        case SET_ENV:           return "SET_ENV";
        case GET_ENV:           return "GET_ENV";
        default:                return "unknown";
    }
}
//---------------------------------------------------------------------------

void VM::dump_envstack(const std::string &msg)
{
    int offs = 0;

    cout << "### BEGIN ENV STACK DUMP {" << msg << "}" << endl;
    for (int i = m_env_stack.size() - 1; i >= 0; i--)
    {
        cout << "    [" << i << "] ";
        SimpleVec &sv = m_env_stack[i]->m_d.vec;
        for (size_t d = 0; d < sv.len; d++)
        {
            if (sv.elems[d])
                cout << "(" << d << ": " << sv.elems[d]->to_string() << ")";
            else
                cout << "(" << d << "  " << "null" << ")";
        }
        cout << endl;
    }
    cout << "### END ENV STACK DUMP {" << msg << "}" << endl;
}
//---------------------------------------------------------------------------

void VM::dump_stack(const std::string &msg)
{
    int offs = 0;

    cout << "### BEGIN STACK DUMP {" << msg << "}" << endl;
    for (size_t i = 0; i < m_stack.size(); i++)
    {
        Datum *d = m_stack[i];
        cout << "    " << i << ": " << d->to_string() << endl;
    }
    cout << "### END STACK DUMP {" << msg << "}" << endl;
}
//---------------------------------------------------------------------------

void DatumPool::grow()
{
    size_t new_chunk_size = (m_allocated.size() + 1) * 2;
    Datum *dt = new Datum[new_chunk_size];
    for (size_t i = 0; i < new_chunk_size; i++)
    {
        m_allocated.push_back(dt);
        dt->m_next = m_free_list;
        m_free_list = dt;

        dt++;
    }

    cout << "Datum pool grew to " << m_allocated.size() << endl;
}
//---------------------------------------------------------------------------

Datum *VM::new_dt_int(int64_t i)
{
    Datum *dt = m_datum_pool.new_datum(T_INT);
    dt->m_d.i = i;
    return dt;
}
//---------------------------------------------------------------------------

Datum *VM::new_dt_dbl(double d)
{
    Datum *dt = m_datum_pool.new_datum(T_DBL);
    dt->m_d.d = d;
    return dt;
}
//---------------------------------------------------------------------------

Datum *VM::run()
{
    int op_max_ip = m_ops.size() - 1;

    while (m_ip <= op_max_ip)
    {
        if (check_stack_overflow())
        {
            cout << "ERROR: STACK OVERFLOW!" << endl;
            break;
        }

        Operation &op = *(m_ops[m_ip]);

        if (m_enable_trace_log)
            cout << "TRC (stks=" << GET_STK_SIZE << ") IP="
                 << m_ip << ": " << OPCODE2NAME(op.m_op)
                 << endl;

        switch (op.m_op)
        {
            case NOP:
                break;

            case TRC:
                m_enable_trace_log = op.m_1.i == 1;
                break;

            case PUSH_I:
            {
                STK_PUSH(new_dt_int(op.m_1.i));
                break;
            }

            case PUSH_D:
            {
                STK_PUSH(new_dt_dbl(op.m_1.d));
                break;
            }

            case ADD_D:
            {
                Datum *new_dt =
                    new_dt_dbl(  STK_AT(1)->to_dbl()
                               + STK_AT(2)->to_dbl());
                // TODO: Optimize, by pop only 1, and overwrite the next?
                STK_POP();
                STK_POP();
                STK_PUSH(new_dt);

                // FIXME: Implement mark & sweep garbage collector!?
                break;
            }

            case ADD_I:
            {
                Datum *new_dt =
                    new_dt_int(  STK_AT(1)->to_int()
                               + STK_AT(2)->to_int());
                // TODO: Optimize, by pop only 1, and overwrite the next?
                STK_POP();
                STK_POP();
                STK_PUSH(new_dt);

                // FIXME: Implement mark & sweep garbage collector!?
                break;
            }

            case PUSH_ENV:
            {
                // TODO: Sometime optimize this with vector pools,
                //       because functions are called regularily.
                m_env_stack.push_back(
                    m_datum_pool.new_vector(T_VEC, (size_t) op.m_1.i));
                break;
            }

            case POP_ENV:
            {
                m_env_stack.pop_back();
                break;
            }

            case SET_ENV:
            case GET_ENV:
            {
                if (m_env_stack.empty())
                {
                    cout << "ERROR: Access to empty env stack" << endl;
                    return nullptr;
                }

                size_t stkoffs = (size_t) op.m_1.i;
                size_t idx     = (size_t) op.m_2.i;

                if (stkoffs >= m_env_stack.size())
                {
                    cout << "ERROR: Access to non existing env frame" << endl;
                    return nullptr;
                }

                SimpleVec &sv =
                    m_env_stack[m_env_stack.size() - (stkoffs + 1)]->m_d.vec;
                if (idx >= sv.len)
                {
                    cout << "ERROR: Access outside of env frame" << endl;
                    return nullptr;
                }

                if (op.m_op == SET_ENV)
                {
                    sv.elems[idx] = STK_AT(1);
                    STK_POP();
                }
                else
                {
                    Datum *dt = sv.elems[idx];
                    if (!dt) dt = new_dt_int(0);
                    STK_PUSH(dt);
                }
                break;
            }

            case JMP:
            {
                size_t pos = (size_t) op.m_1.i;

                if (pos < 0 || pos > m_ops.size())
                {
                    cout << "ERROR: Invalid JMP adress: " << pos << endl;
                    return nullptr;
                }

                m_ip = pos - 1;
                break;
            }

            case BR:
            {
                int offs = (int) op.m_1.i;
                int dest = m_ip + offs;

                if (dest < 0 || dest >= (int) m_ops.size())
                {
                    cout << "ERROR: Invalid BR offset: " << offs << endl;
                    return nullptr;
                }

                m_ip = m_ip + (offs - 1); // -1 because of m_ip++ at the end
                break;
            }

            case DBG_DUMP_STACK:
            {
                dump_stack("DBG_DUMP_STACK");
                break;
            }

            case DBG_DUMP_ENVSTACK:
            {
                dump_envstack("DBG_DUMP_ENVSTACK");
                break;
            }

            default:
                log("Unkown op: " + std::string(OPCODE2NAME(op.m_op)));
                return nullptr;
        }

        m_ip++;
    }
    if (GET_STK_SIZE <= 0)
        return nullptr;
    return STK_AT(1);
}
//---------------------------------------------------------------------------

}
