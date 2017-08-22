#include "buklivm.h"
#include "atom_printer.h"
#include <chrono>

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
    instr.o  = (int32_t) av->m_data[1].to_int();
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

    PROG *new_prog = new PROG(data.m_d.vec->m_len, prog.m_d.vec->m_len + 1);
    new_prog->set_debug_info(debug);
    new_prog->set_data_from(data.m_d.vec);

    for (size_t i = 0; i < prog.m_d.vec->m_len + 1; i++)
    {
        INST instr;
        if (i == (prog.m_d.vec->m_len))
        {
            instr.op = OP_END;
            instr.o = 0;
            instr._.x.a = 0;
            instr._.l.a = 0;
            instr._.l.b = 0;
        }
        else
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
    using namespace std::chrono;

    cout << "vm start" << endl;

    if (at_ud.m_type != T_UD || at_ud.m_d.ud->type() != "VM-PROG")
    {
//        error("Bad input type to run-vm-prog, expected VM-PROG.", A0);
        cout << "VM-ERROR: Bad input type to run-vm-prog, expected VM-PROG."
             << at_ud.to_write_str() << endl;
        return Atom();
    }

    PROG *prog = dynamic_cast<PROG*>(at_ud.m_d.ud);
    INST *pc   = &(prog->m_instructions[0]);

    VMProgStateGuard psg(m_prog, m_pc, prog, pc);

    Atom *data      = m_prog->data_array();
    size_t data_len = m_prog->data_array_len();

    // TODO: To be recusively usabe, we need to save:
    //       - cont_stack
    //       - env_stack
    //       - m_pc
    //       - m_prog

    AtomVec *env_stack = m_rt->m_gc.allocate_vector(10);
    AtomVecPush avpvenvst(m_root_stack, Atom(T_VEC, env_stack));

    AtomVec *cont_stack = m_rt->m_gc.allocate_vector(10);
    AtomVecPush avpvcontst(m_root_stack, Atom(T_VEC, cont_stack));

    AtomVec *cur_env = args;
    env_stack->m_len = 0;
    env_stack->push(Atom(T_VEC, cur_env));

    cont_stack->push(at_ud);

//    cout << "VM PROG: " << at_ud.to_write_str() << endl;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

#define SET_O(env, val) do { if (m_pc->o >= (env)->m_len) (env)->check_size(m_pc->o); (env)->m_data[m_pc->o] = (val); } while (0)

    Atom ret;

    bool alloc = false;
    while (m_pc->op != OP_END)
    {
        cout << "VMTRC FRMS(" << cont_stack->m_len << "/" << env_stack->m_len << "): ";
        m_pc->to_string(cout);
        cout << endl;

        cout << " {ENV= " << Atom(T_VEC, cur_env).to_write_str() << "}" << endl;

        switch (m_pc->op)
        {
            case OP_DUMP_ENV_STACK:
            {
                for (size_t i = 0; i < env_stack->m_len; i++)
                {
                    cout << "ENV@" << (env_stack->m_len - (i + 1)) << ": ";
                    AtomVec *env = env_stack->at(i).m_d.vec;
                    for (size_t j = 0; j < env->m_len; j++)
                        cout << "[" << j << "=" << env->at(j).to_write_str() << "] ";
                    cout << endl;
                }
                break;
            }

            case OP_MOV:
            {
                size_t idx = m_pc->_.x.a;
                SET_O(cur_env, idx < cur_env->m_len ? cur_env->m_data[idx] : Atom());
                break;
            }

            case OP_MOV_FROM:
            {
                size_t env_idx = m_pc->_.l.a;
                if (env_idx >= env_stack->m_len)
                {
                    cout << "VM-ERROR: MOVX src env-idx out of range"
                         << env_idx << endl;
                    return Atom();
                }
                AtomVec *src_env =
                    env_stack->at(env_stack->m_len - (env_idx + 1)).m_d.vec;

                size_t idx = m_pc->_.l.b;
                SET_O(cur_env, idx < src_env->m_len ? src_env->m_data[idx] : Atom());
                break;
            }

            case OP_MOV_TO:
            {
                size_t env_idx = m_pc->_.l.a;
                if (env_idx >= env_stack->m_len)
                {
                    cout << "VM-ERROR: MOVY dst env-idx out of range"
                         << env_idx << endl;
                    return Atom();
                }
                AtomVec *dst_env =
                    env_stack->at(env_stack->m_len - (env_idx + 1)).m_d.vec;

                size_t idx = m_pc->_.l.b;
                SET_O(dst_env, idx < cur_env->m_len ? cur_env->m_data[idx] : Atom());
                break;
            }

            case OP_NEW_VEC:
            {
                alloc = true;
                SET_O(cur_env, Atom(T_VEC, m_rt->m_gc.allocate_vector(m_pc->_.x.a)));
                break;
            }

            case OP_NEW_MAP:
            {
                alloc = true;
                SET_O(cur_env, Atom(T_MAP, m_rt->m_gc.allocate_map()));
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
                if (vec.m_type == T_VEC)
                {
                    vec.m_d.vec->set(idx, cur_env->at(eidx));
                    cur_env->m_data[eidx].clear();
                }
                else if (vec.m_type == T_MAP)
                {
                    vec.m_d.map->set(cur_env->at(idx), cur_env->at(eidx));
                    cur_env->m_data[idx].clear();
                    cur_env->m_data[eidx].clear();
                }
                else
                    error("Can CSET_VEC on vector and map", vec);

                break;
            }

            case OP_LOAD_PRIM:
            {
                size_t p_nr = m_pc->_.x.a;
                SET_O(cur_env,
                    p_nr < m_prim_table->m_len
                    ? m_prim_table->m_data[p_nr] : Atom());
                break;
            }

            case OP_LOAD_NIL:
            {
                SET_O(cur_env, Atom());
                break;
            }

            case OP_LOAD_STATIC:
            {
                size_t idx = m_pc->_.x.a;
                SET_O(cur_env, idx < data_len ? data[idx] : Atom());
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
//                env_stack->push(vec);
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
//                env_stack->push(Atom(T_VEC, av));
//                break;
//            }
//
            case OP_PUSH_ENV:
            {
                alloc = true;
                env_stack->push(
                    Atom(T_VEC, cur_env = m_rt->m_gc.allocate_vector(m_pc->_.x.a)));
                break;
            }

            case OP_POP_ENV:
            {
                env_stack->pop();
                Atom *l = env_stack->last();
                if (l) cur_env = l->m_d.vec;
                break;
            }

            case OP_NEW_CLOSURE:
            {
                alloc = true;

                Atom prog = cur_env->at(m_pc->_.x.a);
                if (prog.m_type != T_UD || prog.m_d.ud->type() != "VM-PROG")
                    error("VM can't make closure from non VM-PROG", prog);

                Atom cl(T_CLOS);
                cl.m_d.vec = m_rt->m_gc.allocate_vector(2);
//                std::cout << "NEW CLOS " << ((void *) cl.m_d.vec) << "@" << ((void *) cl.m_d.vec->m_data) << std::endl;
                cl.m_d.vec->set(0, prog); // XXX: m_data[0] = prog;
                cl.m_d.vec->set(1,        //      m_data[1] =
                    Atom(T_VEC, m_rt->m_gc.clone_vector(env_stack)));

                SET_O(cur_env, cl);
                break;
            }

            case OP_PACK_VA:
            {
                size_t pack_idx = m_pc->_.x.a;
                if (pack_idx < cur_env->m_len)
                {
                    size_t va_len = cur_env->m_len - pack_idx;
                    AtomVec *v = m_rt->m_gc.allocate_vector(va_len);
                    for (size_t i = 0; i < va_len; i++)
                        v->m_data[i] = cur_env->m_data[i + pack_idx];
                    SET_O(cur_env, Atom(T_VEC, v));
                }
                else
                {
                    SET_O(cur_env, Atom(T_VEC, m_rt->m_gc.allocate_vector(0)));
                }
                break;
            }

            case OP_CALL:
            {
                size_t out_idx  = m_pc->o;
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
//                    cout << "argv: " << argv.to_write_str() << "; RET PRIM: "  << ret.to_write_str() << endl;
                    cur_env->set(out_idx, ret);

                    cur_env->m_data[func_idx].clear();
                    cur_env->m_data[argv_idx].clear();
                }
                else if (func.m_type == T_CLOS)
                {
                    if (func.m_d.vec->m_len < 2)
                        error("Bad closure found", func);
                    if (func.m_d.vec->m_data[0].m_type != T_UD)
                        error("Bad closure found", func);

                    alloc = true;
                    AtomVec *cont = m_rt->m_gc.allocate_vector(5);

                    Atom prog(T_UD);
                    prog.m_d.ud = (UserData *) m_prog;
                    Atom pc(T_C_PTR);
                    pc.m_d.ptr = m_pc;

                    cont->m_data[0] = prog;
                    cont->m_data[1] = Atom(T_VEC, env_stack);
                    cont->m_data[2] = pc;
                    cont->m_data[3] = Atom(T_INT, out_idx);
                    // just for keepin a reference to the called function:
                    cont->m_data[4] = func;

                    // replace current function with continuation
                    *cont_stack->last() = Atom(T_VEC, cont);
                    cont_stack->push(func); // save the current function

                    if (func_idx >= cur_env->m_len)
                        error("Bad func idx", func);
                    if (argv_idx >= cur_env->m_len)
                        error("Bad func idx", argv);

                    cur_env->m_data[func_idx].clear();
                    cur_env->m_data[argv_idx].clear();

                    m_prog   = dynamic_cast<PROG*>(func.m_d.vec->m_data[0].m_d.ud);
                    data     = m_prog->data_array();
                    data_len = m_prog->data_array_len();
                    m_pc     = &(m_prog->m_instructions[0]);
                    m_pc--;

                    env_stack = func.m_d.vec->m_data[1].m_d.vec;
                    env_stack->push(Atom(T_VEC, cur_env = argv.m_d.vec));
                }
                else
                    error("CALL does not support that function type yet", func);

                break;
            }

            case OP_RETURN:
            {
                Atom retval = cur_env->at(m_pc->_.x.a);

                cont_stack->pop(); // the current function can be discarded
                // retrieve the continuation:
                Atom *c = cont_stack->last();
                if (!c || c->m_type == T_NIL)
                {
                    ret = retval;
                    m_pc = &(m_prog->m_instructions[m_prog->m_instructions_len - 2]);
                    break;
                }

                Atom cont = *c;
                cont_stack->pop();
                if (cont.m_type != T_VEC || cont.m_d.vec->m_len < 4)
                    error("Empty or bad continuation stack item!", cont);

                Atom proc   = cont.m_d.vec->m_data[0];
                Atom envs   = cont.m_d.vec->m_data[1];
                Atom pc     = cont.m_d.vec->m_data[2];
                size_t oidx = (size_t) cont.m_d.vec->m_data[3].m_d.i;

                // save current function for gc:
                cont_stack->push(proc);
                m_prog   = dynamic_cast<PROG*>(proc.m_d.ud);
                data     = m_prog->data_array();
                data_len = m_prog->data_array_len();
                m_pc     = (INST *) pc.m_d.ptr;

                env_stack = envs.m_d.vec;
                cur_env     = env_stack->last()->m_d.vec;
                cur_env->set(oidx, retval);

//                cout << "VMRETURN VAL: " << retval.to_write_str() << endl;

                break;
            }

            case OP_SET_RETURN:
            {
                ret = cur_env->at(m_pc->_.x.a);
                break;
            }

            case OP_BR:
            {
                m_pc += m_pc->o;
                if (m_pc >= (m_prog->m_instructions + m_prog->m_instructions_len))
                    error("BR out of PROG", Atom());
                break;
            }

            case OP_BRIF:
            {
                Atom a = cur_env->at(m_pc->_.x.a);
                if (!a.is_false())
                {
                    m_pc += m_pc->o;
                    if (m_pc >= (m_prog->m_instructions + m_prog->m_instructions_len))
                        error("BRIF out of PROG", Atom());
                }
                break;
            }

            case OP_BRNIF:
            {
                Atom a = cur_env->at(m_pc->_.x.a);
                if (a.is_false())
                {
                    m_pc += m_pc->o;
                    if (m_pc >= (m_prog->m_instructions + m_prog->m_instructions_len))
                        error("BRIF out of PROG", Atom());
                }
                break;
            }

            case OP_INC:
            {
                size_t idx_a = m_pc->_.x.a;
                if (idx_a >= cur_env->m_len)
                    error("INC can't work with an undefined increment", Atom());

                size_t idx_o = m_pc->o;
                if (idx_o >= cur_env->m_len)
                    cur_env->set(idx_o, Atom(T_INT));

                Atom *o = &(cur_env->m_data[idx_o]);
                if (o->m_type == T_DBL)
                    o->m_d.d += cur_env->m_data[idx_a].m_d.d;
                else
                    o->m_d.i += cur_env->m_data[idx_a].m_d.i;
                break;
            }

            case OP_GE:
            {
                size_t idx_a = m_pc->_.l.a;
                size_t idx_b = m_pc->_.l.b;
                Atom a;
                if (idx_a < cur_env->m_len) a = cur_env->m_data[idx_a];
                Atom b;
                if (idx_b < cur_env->m_len) b = cur_env->m_data[idx_b];
                Atom o(T_BOOL);
                if (a.m_type == T_DBL)
                    o.m_d.b = a.to_dbl() >= b.to_dbl();
                else
                    o.m_d.b = a.to_int() >= b.to_int();
                break;
            }

            default:
                throw VMException("Unknown VM opcode: " + to_string(m_pc->op));
        }
        m_pc++;

        if (alloc)
        {
            m_rt->m_gc.collect_maybe();
        }
    }

    cont_stack->pop();

//    cout << "STACKS: " << cont_stack->m_len << "; " << env_stack->m_len << endl;

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    duration<double> time_span
        = duration_cast<duration<double>>(t2 - t1);
    std::cout << "vm eval run time: "
              << (time_span.count() * 1000)
              << " ms" << std::endl;

    return ret;
}
//---------------------------------------------------------------------------


}
