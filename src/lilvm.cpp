#include "lilvm.h"

using namespace std;

int test() { return 11; }

namespace lilvm
{
//---------------------------------------------------------------------------

unsigned int Datum::s_instance_counter = 0;

//---------------------------------------------------------------------------

const char *OPCODE_NAMES[] = {
    "NOP",
    "TRC",
    "PUSH_I",
    "PUSH_D",
    "PUSH_SYM",
    "DBG_DUMP_STACK",
    "DBG_DUMP_ENVSTACK",
    "PUSH_GC_USE_COUNT",
    "ADD_I",
    "ADD_D",
    "SUB_I",
    "SUB_D",
    "MUL_I",
    "MUL_D",
    "DIV_I",
    "DIV_D",

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

    "POP",
    "PUSH_ENV",
    "POP_ENV",
    "SET_ENV",
    "GET_ENV",
    "PUSH_FUNC",
    "PUSH_PRIM",
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
        case PUSH_SYM:          return "PUSH_SYM";
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
        case POP:               return "POP";
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
    auto dt = new Datum[new_chunk_size];
    m_chunks.push_back(std::move(std::unique_ptr<Datum[]>(dt)));

    Datum *dt_ptr = &dt[0];
    for (size_t i = 0; i < new_chunk_size; i++)
    {
        m_allocated.push_back(dt_ptr);
        dt_ptr->m_next = m_free_list;
        m_free_list = dt_ptr;

        dt_ptr++;
    }

    cout << "Datum pool grew to " << m_allocated.size() << endl;
}
//---------------------------------------------------------------------------

void DatumPool::reclaim(uint8_t mark)
{
    unsigned int in_use_before = m_datums_in_use;

    for (auto &dt : m_allocated)
    {
        if (dt->get_mark() != mark)
            put_on_free_list(dt);
    }

    cout << "Datums in use before gc: " << in_use_before
        << ", after: " << m_datums_in_use << endl;
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

Datum *VM::new_dt_sym(Sym *sym)
{
    Datum *dt = m_datum_pool.new_datum(T_SYM);
    dt->m_d.sym = sym;
    return dt;
}
//---------------------------------------------------------------------------

Datum *VM::new_dt_external(Sym *ext_type, Datum *args)
{
    if (!m_ext_factory)
    {
        std::cout << "ERROR: Unknown external type: "
            << ext_type->m_str << std::endl;
        return new_dt_int(0);
    }
    External *ext = (*m_ext_factory)(ext_type->m_str, args);
    if (!ext)
    {
        std::cout << "ERROR: Unknown external type: "
            << ext_type->m_str << std::endl;
        return new_dt_int(0);
    }
    Datum *dt = m_datum_pool.new_datum(T_EXT);
    dt->m_d.ext = ext;
    return dt;
}
//---------------------------------------------------------------------------

Datum *VM::new_dt_prim(Datum::PrimFunc *func)
{
    Datum *dt = m_datum_pool.new_datum(T_PRIM);
    dt->m_d.func = func;
    return dt;
}
//---------------------------------------------------------------------------

void VM::pop(size_t cnt)
{
    for (size_t i = 0; i < cnt; i++)
        m_stack.pop_back();
}
//---------------------------------------------------------------------------

void VM::push(Datum *dt)
{
    m_stack.push_back(dt);
}
//---------------------------------------------------------------------------

void VM::init_core_primitives()
{
    register_primitive(0, [this](VM *vm, std::vector<Datum *> &s, size_t ac, bool retval){
        m_datum_pool.collect();
        return retval ? new_dt_int(0) : nullptr;
    });

//    register_primitive(1, [](VM *vm, std::vector<Datum *> &s, size_t ac){
//        vm->pop(ac);
//        POP_STACK_ARGS(vm, s, ac);
//        m_datum_pool.collect();
//        vm->push(new_dt_int(0));
//    });
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

            case PUSH_SYM:
                {
                    STK_PUSH(new_dt_sym(op.m_1.sym));
                    break;
                }

            case SUB_D:
                {
                    Datum *new_dt =
                        new_dt_dbl(  STK_AT(2)->to_dbl()
                                   - STK_AT(1)->to_dbl());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);

                    // FIXME: Implement mark & sweep garbage collector!?
                    break;
                }

            case SUB_I:
                {
                    Datum *new_dt =
                        new_dt_int(  STK_AT(2)->to_int()
                                   - STK_AT(1)->to_int());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);

                    // FIXME: Implement mark & sweep garbage collector!?
                    break;
                }

            case ADD_D:
                {
                    Datum *new_dt =
                        new_dt_dbl(  STK_AT(2)->to_dbl()
                                   + STK_AT(1)->to_dbl());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);

                    // FIXME: Implement mark & sweep garbage collector!?
                    break;
                }

            case ADD_I:
                {
                    Datum *new_dt =
                        new_dt_int(  STK_AT(2)->to_int()
                                   + STK_AT(1)->to_int());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);

                    // FIXME: Implement mark & sweep garbage collector!?
                    break;
                }

            case MUL_D:
                {
                    Datum *new_dt =
                        new_dt_dbl(  STK_AT(2)->to_dbl()
                                   * STK_AT(1)->to_dbl());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);
                    break;
                }

            case MUL_I:
                {
                    Datum *new_dt =
                        new_dt_int(  STK_AT(2)->to_int()
                                   * STK_AT(1)->to_int());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);
                    break;
                }

            case DIV_D:
                {
                    Datum *new_dt =
                        new_dt_dbl(  STK_AT(2)->to_dbl()
                                   / STK_AT(1)->to_dbl());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);
                    break;
                }

            case DIV_I:
                {
                    Datum *new_dt =
                        new_dt_int(  STK_AT(1)->to_int()
                                   / STK_AT(2)->to_int());
                    STK_POP();
                    STK_POP();
                    STK_PUSH(new_dt);
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

            case POP:
                {
                    for (int i = 0; i < op.m_1.i; i++)
                    {
                        STK_POP();
                    }
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

            case PUSH_PRIM:
                {
                    size_t id = (size_t) op.m_1.i;
                    if (m_primitives.size() <= id)
                    {
                        cout << "ERROR: Unknown primitive id=" << id << endl;
                        return nullptr;
                    }

                    if (!m_primitives[id])
                    {
                        cout << "ERROR: Invalid primitive id=" << id << endl;
                        return nullptr;
                    }

                    STK_PUSH(new_dt_prim(m_primitives[id]));
                    break;
                };

            case CALL_FUNC:
                {
                    size_t argc   = (size_t) op.m_1.i;
                    bool   retval = op.m_2.i == 0 ? false : true;

                    auto &dt = STK_AT(1);
                    if (dt->m_type != T_PRIM)
                    {
                        cout << "ERROR: Uncallable value on top of the stack!"
                             << endl;
                        dump_stack("CALL_FUNC");
                        return nullptr;
                    }

                    Datum *ret = (*dt->m_d.func)(this, m_stack, argc, retval);

                    for (size_t i = 0; i < argc + 1; i++)
                        STK_POP();

                    if (retval)
                    {
                        if (!ret) ret = new_dt_int(0);
                        STK_PUSH(ret);
                    }

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

            case PUSH_GC_USE_COUNT:
                {
                    STK_PUSH(new_dt_int(m_datum_pool.get_datums_in_use_count()));
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
