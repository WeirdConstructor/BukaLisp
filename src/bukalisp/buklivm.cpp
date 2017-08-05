#include "buklivm.h"
#include "atom_printer.h"

using namespace lilvm;
using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

std::string PROG::as_string()
{
    std::stringstream ss;
    ss << "#<prog:\n";
    for (size_t i = 0; i < m_atom_data_len; i++)
    {
        ss << " [" << i << "]: " << write_atom(m_atom_data[i]) << "\n";
    }

    for (size_t i = 0; i < m_instructions_len; i++)
    {
        ss << " {" << i << "}: ";
        m_instructions[i].to_string(ss);
        ss << "\n";
    }
    ss << ">";
    return ss.str();
}
//---------------------------------------------------------------------------

void parse_op_desc(INST &instr, Atom &desc)
{
    if (desc.m_type != T_VEC)
        return;

    AtomVec *av = desc.m_d.vec;
    if (av->m_len < 3)
    {
        cout << "Too few elements in op desc (3 needed): "
             << desc.to_write_str() << endl;
        return;
    }

    std::string op_name;
    Atom op_name_s = av->m_data[0];
    if (   op_name_s.m_type != T_SYM
        && op_name_s.m_type != T_KW)
        return;

    op_name = op_name_s.m_d.sym->m_str;

    uint8_t op_code = 0;

#define X(name, code) else if (op_name == #name) op_code = code;
    if (op_name == "***") op_code = 0;
    OP_CODE_DEF(X)
    else
    {
        cout << "unknown op name: " << op_name << endl;
    }
#undef X

//    cout << "op code: " << (int) op_code << endl;
    instr.op = op_code;
    instr.o  = (uint16_t) av->m_data[1].to_int();
    if (av->m_len == 4)
    {
        instr._.l.a = (uint16_t) av->m_data[2].to_int();
        instr._.l.b = (uint16_t) av->m_data[3].to_int();
    }
    else if (av->m_len == 3)
    {
        instr._.x.a = (uint32_t) av->m_data[2].to_int();
    }
}
//---------------------------------------------------------------------------

lilvm::Atom make_prog(lilvm::Atom prog_info)
{
    if (prog_info.m_type != T_VEC)
        return Atom();

    Atom data  = prog_info.m_d.vec->m_data[0];
    Atom prog  = prog_info.m_d.vec->m_data[1];
    Atom debug = prog_info.m_d.vec->m_data[2];

    if (   data.m_type  != T_VEC
        || prog.m_type  != T_VEC
        || debug.m_type != T_MAP)
        return Atom();

    PROG *new_prog = new PROG(data.m_d.vec->m_len, prog.m_d.vec->m_len);
    new_prog->set_debug_info(debug);
    new_prog->set_data_from(data.m_d.vec);

    for (size_t i = 0; i < prog.m_d.vec->m_len; i++)
    {
        INST instr;
        parse_op_desc(instr, prog.m_d.vec->m_data[i]);
        new_prog->set(i, instr);
    }

    Atom ud(T_UD);
    ud.m_d.ud = new_prog;
    Atom newud = ud;
    return ud;
}
//---------------------------------------------------------------------------

void VM::init_prims()
{
    Atom tmp;

#define START_PRIM() \
    tmp = Atom(T_PRIM); \
    tmp.m_d.func = new Atom::PrimFunc; \
    m_prim_table->push(tmp); \
    (*tmp.m_d.func) = [this](AtomVec &args, Atom &out) {

#define END_PRIM(name) };

    #include "primitives.cpp"
}
//---------------------------------------------------------------------------

lilvm::Atom VM::eval(Atom at_ud, AtomVec *args)
{
    Atom ret;

    if (at_ud.m_type != T_UD || at_ud.m_d.ud->type() != "VM-PROG")
    {
//        error("Bad input type to run-vm-prog, expected VM-PROG.", A0);
        cout << "VM-ERROR: Bad input type to run-vm-prog, expected VM-PROG."
             << at_ud.to_write_str() << endl;
        return Atom();
    }

    AtomVecPush avpvmprog(m_root_stack, at_ud);
    AtomVecPush avpvmargs(m_root_stack, Atom(T_VEC, args));

    m_prog          = dynamic_cast<PROG*>(at_ud.m_d.ud);
    Atom *data      = m_prog->data_array();
    size_t data_len = m_prog->data_array_len();

    m_pc = &(m_prog->m_instructions[0]);

    AtomVec *cur_env = args;

    while (m_pc->op != OP_END)
    {
        switch (m_pc->op)
        {
            case OP_DUMP_ENV_STACK:
            {
                for (size_t i = m_env_stack->m_len; i > 0; i--)
                {
                    cout << "ENV@" << (i - 1) << ": ";
                    AtomVec *env = m_env_stack->at(i - 1).m_d.vec;
                    for (size_t j = 0; j < env->m_len; j++)
                        cout << "[" << j << "=" << env->at(j).to_write_str() << "] ";
                    cout << endl;
                }
                break;
            }

            case OP_MOV:
            {
                size_t idx = m_pc->_.x.a;
                cur_env->set(
                    m_pc->o,
                    idx < cur_env->m_len ? cur_env->m_data[idx] : Atom());
                break;
            }

            case OP_MOV_FROM:
            {
                size_t env_idx = m_pc->_.l.a;
                if (env_idx >= m_env_stack->m_len)
                {
                    cout << "VM-ERROR: MOVX src env-idx out of range"
                         << env_idx << endl;
                    return Atom();
                }
                AtomVec *src_env =
                    m_env_stack->at(m_env_stack->m_len - (env_idx + 1)).m_d.vec;

                size_t idx = m_pc->_.l.b;
                cur_env->set(
                    m_pc->o,
                    idx < src_env->m_len ? src_env->m_data[idx] : Atom());
                break;
            }

            case OP_MOV_TO:
            {
                size_t env_idx = m_pc->_.l.a;
                if (env_idx >= m_env_stack->m_len)
                {
                    cout << "VM-ERROR: MOVY dst env-idx out of range"
                         << env_idx << endl;
                    return Atom();
                }
                AtomVec *dst_env =
                    m_env_stack->at(m_env_stack->m_len - (env_idx + 1)).m_d.vec;

                size_t idx = m_pc->_.l.b;
                dst_env->set(
                    m_pc->o,
                    idx < cur_env->m_len ? cur_env->m_data[idx] : Atom());
                break;
            }

            case OP_NEW_VEC:
            {
                cur_env->set(
                    m_pc->o,
                    Atom(T_VEC, m_rt->m_gc.allocate_vector(m_pc->_.x.a)));
                break;
            }

            case OP_CSET_VEC:
            {
                size_t vidx = m_pc->o;
                size_t idx  = m_pc->_.l.a;
                size_t eidx = m_pc->_.l.b;

                if (eidx >= cur_env->m_len)
                    error("Bad index for source value", Atom(T_INT, eidx));
                if (vidx >= cur_env->m_len)
                    error("Bad env index for SET_VEC vector", Atom(T_INT, vidx));

                Atom vec = cur_env->m_data[vidx];
                if (vec.m_type != T_VEC)
                    error("Can't SET_VEC on non vector", vec);

                auto &v = *(vec.m_d.vec);
                v.set(idx, cur_env->at(eidx));

                cur_env->m_data[eidx] = Atom();
                break;
            }

            case OP_LOAD_PRIM:
            {
                size_t p_nr = m_pc->_.x.a;
                cur_env->set(
                    m_pc->o,
                    p_nr < m_prim_table->m_len
                    ? m_prim_table->m_data[p_nr] : Atom());
                break;
            }

            case OP_LOAD_STATIC:
            {
                size_t idx = m_pc->_.x.a;
                cur_env->set(
                    m_pc->o,
                    idx < data_len ? data[idx] : Atom());
                break;
            }

//            case OP_PUSH_VEC_ENV:
//            {
//                size_t idx = m_pc->_.x.a;
//                if (idx >= cur_env->m_len)
//                    error("Bad vector index for PUSH_VEC_ENV",
//                          Atom(T_INT, idx));
//                Atom vec = cur_env->m_data[idx];
//                if (vec.m_type != T_VEC)
//                    error("Can't push non vector as env in PUSH_VEC_ENV", vec);
//                m_env_stack->push(vec);
//                break;
//            }
//
//            case OP_PUSH_RNG_ENV:
//            {
//                size_t from = m_pc->_.l.a;
//                size_t to   = m_pc->_.l.b;
//                if (from >= cur_env->m_len)
//                    error("Bad env from idx", Atom(T_INT, from));
//                if (to >= cur_env->m_len)
//                    error("Bad env to idx", Atom(T_INT, to));
//
//                AtomVec *av = m_rt->m_gc.allocate_vector(to - from);
//                for (size_t i = from; i < to; i++)
//                    av->m_data[i - from] = cur_env->m_data[i];
//                m_env_stack->push(Atom(T_VEC, av));
//                break;
//            }
//
            case OP_PUSH_ENV:
            {
                m_env_stack->push(
                    Atom(T_VEC, cur_env = m_rt->m_gc.allocate_vector(m_pc->_.x.a)));
                break;
            }

            case OP_POP_ENV:
            {
                m_env_stack->pop();
                Atom *l = m_env_stack->last();
                if (l) cur_env = l->m_d.vec;
                else   cur_env = args;
                break;
            }

            case OP_CALL:
            {
                size_t out_idx = m_pc->o;
                size_t func_idx = m_pc->_.l.a;
                size_t argv_idx = m_pc->_.l.b;

                if (func_idx >= cur_env->m_len)
                    error("CALL function index out of range",
                          Atom(T_INT, func_idx));
                if (argv_idx >= cur_env->m_len)
                    error("CALL argv index out of range",
                          Atom(T_INT, argv_idx));

                Atom argv = cur_env->m_data[argv_idx];
                if (argv.m_type != T_VEC)
                    error("CALL argv not a vector", argv);

                Atom func = cur_env->m_data[func_idx];
                if (func.m_type == T_PRIM)
                {
                    Atom ret;
                    (*func.m_d.func)(*(argv.m_d.vec), ret);
                    cur_env->set(out_idx, ret);
                }
                else
                    error("CALL does not support that function type yet", func);

                cur_env->m_data[func_idx] = Atom();
                cur_env->m_data[argv_idx] = Atom();
                break;
            }

            case OP_SET_RETURN:
            {
                ret = cur_env->at(m_pc->_.x.a);
                break;
            }

            default:
                throw VMException("Unknown VM opcode: " + to_string(m_pc->op));
        }
        m_pc++;
    }

    return ret;
}
//---------------------------------------------------------------------------


}
