#include "lilvm.h"

using namespace std;

int test() { return 11; }

namespace lilvm
{
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
    switch (c)
    {
        case NOP:            return "NOP";
        case TRC:            return "TRC";
        case PUSH_I:         return "PUSH_I";
        case PUSH_D:         return "PUSH_D";
        case DBG_DUMP_STACK: return "DBG_DUMP_STACK";
        case ADD_I:          return "ADD_I";
        case ADD_D:          return "ADD_D";
        case JMP:            return "JMP";
        case BR_IF:          return "BRANCH_IF";
        case LT_D:           return "LT_D";
        case LE_D:           return "LE_D";
        case GT_D:           return "GT_D";
        case GE_D:           return "GE_D";
        case LT_I:           return "LT_I";
        case LE_I:           return "LE_I";
        case GT_I:           return "GT_I";
        case GE_I:           return "GE_I";
        case PUSH_ENV:       return "PUSH_ENV";
        case POP_ENV:        return "POP_ENV";
        case SET_ENV:        return "SET_ENV";
        case GET_ENV:        return "GET_ENV";
        default:             return "unknown";
    }
}
//---------------------------------------------------------------------------

void VM::dump_stack(const std::string &msg)
{
    int offs = 0;

    cout << "### BEGIN STACK DUMP {" << msg << "}" << endl;
    for (int i = 0; i < m_stack.size(); i++)
    {
        Datum *d = m_stack[i];
        cout << "    " << i << ": " << d->to_string() << endl;
    }
    cout << "### END STACK DUMP {" << msg << "}" << endl;
}
//---------------------------------------------------------------------------

Datum *VM::new_dt_int(int64_t i)
{
    Datum *dt = new_datum(T_INT);
    dt->m_d.i = i;
    return dt;
}
//---------------------------------------------------------------------------

Datum *VM::new_dt_dbl(double d)
{
    Datum *dt = new_datum(T_DBL);
    dt->m_d.d = d;
    return dt;
}
//---------------------------------------------------------------------------

Datum *VM::new_datum(Type t)
{
    return new Datum(t);
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
            cout << "(stks=" << GET_STK_SIZE << ") TRC: " << OPCODE2NAME(op.m_op)
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
                // TODO: Sometime optimize this with EnvFrame pools,
                //       because functions are called regularily.
                m_env_stack.push_back(new EnvFrame(op.m_1.i));
                break;
            }

            case POP_ENV:
            {
                // TODO: Sometime optimize this with EnvFrame pools,
                //       because functions are called regularily.
                EnvFrame *frm = m_env_stack.back();
                m_env_stack.pop_back();
                delete frm;
                break;
            }

            case SET_ENV:
            {
                if (m_env_stack.empty())
                {
                    cout << "ERROR: Access to empty env stack" << endl;
                    return nullptr;
                }
                EnvFrame *ef = m_env_stack.back();

                int cnt   = op.m_1.i;
                int start = op.m_2.i;
                int stkoffs = 1;
                for (int i = start; i < (start + cnt); i++)
                {
                    if (i >= ef->m_len)
                    {
                        cout << "ERROR: Access outside of env frame" << endl;
                        return nullptr;
                    }

                    ef->m_env[i] = STK_AT(stkoffs);
                    stkoffs++;
                }
                for (int i = 1; i < stkoffs; i++)
                {
                    STK_POP();
                }
                break;
            }

            case GET_ENV:
            {
                if (m_env_stack.empty())
                {
                    cout << "ERROR: Access to empty env stack" << endl;
                    return nullptr;
                }

                int stkoffs = op.m_1.i;
                int idx     = op.m_2.i;

                if (stkoffs >= m_env_stack.size())
                {
                    cout << "ERROR: Access to non existing env frame" << endl;
                    return nullptr;
                }

                EnvFrame *ef = m_env_stack[m_env_stack.size() - (stkoffs + 1)];
                if (idx >= ef->m_len)
                {
                    cout << "ERROR: Access outside of env frame" << endl;
                    return nullptr;
                }

                Datum *dt = ef->m_env[idx];
                if (!dt) dt = new_dt_int(0);
                STK_PUSH(dt);
                break;
            }

            case DBG_DUMP_STACK:
            {
                dump_stack("DBG_DUMP_STACK");
                break;
            }

            default:
                log("Unkown op: " + to_string(op.m_op));
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
