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
    op->m_next = nullptr;

    if (m_last)
    {
        m_last->m_next = op;
        m_last = op;
    }
    else m_prog = m_last = op;
}
//---------------------------------------------------------------------------

const char *OPCODE2NAME(OPCODE c)
{
    switch (c)
    {
        case NOP:            return "NOP";
        case PUSH_I:         return "PUSH_I";
        case PUSH_D:         return "PUSH_D";
        case DBG_DUMP_STACK: return "DBG_DUMP_STACK";
        case ADD_I:          return "ADD_I";
        case ADD_D:          return "ADD_D";
        case GOTO:           return "GOTO";
        case BRANCH_IF:      return "BRANCH_IF";
        case LT_D:           return "LT_D";
        case LE_D:           return "LE_D";
        case GT_D:           return "GT_D";
        case GE_D:           return "GE_D";
        case LT_I:           return "LT_I";
        case LE_I:           return "LE_I";
        case GT_I:           return "GT_I";
        case GE_I:           return "GE_I";
        case DEF_ARGS:       return "DEF_ARGS";
        case CALL:           return "CALL";
        case RET:            return "RET";
        default:             return "unknown";
    }
}
//---------------------------------------------------------------------------

void VM::dump_stack(const std::string &msg)
{
    int offs = 0;

    cout << "### BEGIN STACK DUMP {" << msg << "}" << endl;
    if (m_stack_top == m_stack_bottom)
        return;

    Datum **stk_cur = m_stack_top;
    do
    {
        stk_cur--;
        Datum *d = *stk_cur;
        cout << "    " << offs << " @" << stk_cur << ": " << d->to_string() << endl;
        offs++;
    }
    while (stk_cur != m_stack_bottom);

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

void VM::run()
{
    m_cur = m_prog;

    while (m_cur)
    {
        if (check_stack_overflow())
        {
            cout << "ERROR: STACK OVERFLOW!" << endl;
            break;
        }

        cout << "(stks=" << GET_STK_SIZE << ") TRC: " << OPCODE2NAME(m_cur->m_op)
             << endl;
        Operation &op = *m_cur;

        switch (op.m_op)
        {
            case NOP:
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
                STK_POPN(2);
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
                STK_POPN(2);
                STK_PUSH(new_dt);

                // FIXME: Implement mark & sweep garbage collector!?
                break;
            }

            case DBG_DUMP_STACK:
            {
                dump_stack("DBG_DUMP_STACK");
                break;
            }

            default:
                log("Unkown op: " + to_string(m_cur->m_op));
                return;
        }

        m_cur = m_cur->m_next;

    }
}
//---------------------------------------------------------------------------

}
