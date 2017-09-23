// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "buklivm.h"
#include "atom_printer.h"
#include "util.h"
#include "modules/bklisp_module_wrapper.h"
#include <chrono>

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
    if (av->m_len < 7)
    {
        cout << "Too few elements in op desc (7 needed): "
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

    instr.op = op_code;
    instr.o  = (int32_t) av->m_data[1].to_int();
    instr.oe = (int32_t) av->m_data[2].to_int();
    instr.a  = (int32_t) av->m_data[3].to_int();
    instr.ae = (int32_t) av->m_data[4].to_int();
    instr.b  = (int32_t) av->m_data[5].to_int();
    instr.be = (int32_t) av->m_data[6].to_int();
}
//---------------------------------------------------------------------------

Atom make_prog(GC &gc, Atom prog_info)
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

    PROG *new_prog = new PROG(gc, data.m_d.vec->m_len, prog.m_d.vec->m_len + 1);
    new_prog->set_debug_info(debug);
    new_prog->set_data_from(data.m_d.vec);

    for (size_t i = 0; i < prog.m_d.vec->m_len + 1; i++)
    {
        INST instr;
        if (i == (prog.m_d.vec->m_len))
        {
            instr.op = OP_END;
            instr.o  = 0;
            instr.oe = 0;
            instr.a  = 0;
            instr.ae = 0;
            instr.b  = 0;
            instr.be = 0;
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

void VM::load_module(BukaLISPModule *m)
{
    Atom funcs(T_MAP, m_rt->m_gc.allocate_map());
    m_modules->set(m->module_name(this), funcs);

    Atom init_func;
    for (size_t i = 0; i < m->function_count(); i++)
    {
        Atom func_desc(T_VEC, m_rt->m_gc.allocate_vector(3));
        func_desc.m_d.vec->set(0, m->get_func_name(this, i));
        func_desc.m_d.vec->set(1, Atom(T_INT, m_prim_table->m_len));
        func_desc.m_d.vec->set(2, m->get_func(this, i));
        funcs.m_d.map->set(func_desc.at(0), func_desc);

        m_prim_table->push(func_desc.at(2));

        if (func_desc.at(0).m_d.sym->m_str == "__INIT__")
            init_func = func_desc.at(2);
    }

    if (init_func.m_type == T_PRIM)
    {
        GC_ROOT_VEC(m_rt->m_gc, args) = m_rt->m_gc.allocate_vector(0);
        Atom ret;
        (*init_func.m_d.func)(*args, ret);
    }
    else if (init_func.m_type != T_NIL)
    {
        throw VMException(
                "Bad __INIT__ in module initialization of '"
                + m->module_name(this).m_d.sym->m_str
                + "'");
    }
}
//---------------------------------------------------------------------------

AtomMap *VM::loaded_modules()
{
    return m_modules;
}
//---------------------------------------------------------------------------

Atom VM::eval(Atom callable, AtomVec *args)
{
    using namespace std::chrono;

    PROG *prog = nullptr;
    INST *pc   = nullptr;

    if (!args) args = m_rt->m_gc.allocate_vector(0);

    GC_ROOT_VEC(m_rt->m_gc, env_stack)  = nullptr;
    GC_ROOT_VEC(m_rt->m_gc, cont_stack) = nullptr;

    if (callable.m_type == T_UD && callable.m_d.ud->type() == "VM-PROG")
    {
        prog      = dynamic_cast<PROG*>(callable.m_d.ud);
        pc        = &(prog->m_instructions[0]);
        env_stack = m_rt->m_gc.allocate_vector(10);
        env_stack->m_len = 0;
    }
    else if (callable.m_type == T_CLOS)
    {
        if (callable.m_d.vec->m_len == 2)
        {
            prog = dynamic_cast<PROG*>(callable.m_d.vec->m_data[0].m_d.ud);
            pc   = &(prog->m_instructions[0]);
            env_stack = callable.m_d.vec->m_data[1].m_d.vec;
        }
        else if (callable.m_d.vec->m_len == 3)
        {
            if (!m_interpreter_call)
            {
                cout << "VM-ERROR: Can't call eval from interpreter in VM. "
                        "No interpreter?" << endl;
                return Atom();
            }
            return m_interpreter_call(callable, args);
        }
        else
        {
            cout << "VM-ERROR: Broken closure input to run-vm-prog! "
                 << callable.to_write_str() << endl;
            return Atom();
        }
    }
    else if (callable.m_type == T_PRIM)
    {
        Atom ret;
        (*callable.m_d.func)(*args, ret);
        return ret;
    }
    else
    {
//        error("Bad input type to run-vm-prog, expected VM-PROG.", A0);
        cout << "VM-ERROR: Bad input type to run-vm-prog, expected VM-PROG or closure, got: "
             << callable.to_write_str() << endl;
        return Atom();
    }

    cout << "vm start" << endl;

    VMProgStateGuard psg(m_prog, m_pc, prog, pc);

    Atom *data      = m_prog->data_array();
    size_t data_len = m_prog->data_array_len();

    AtomVec *cur_env = args;
    env_stack->push(Atom(T_VEC, cur_env));

    cont_stack = m_rt->m_gc.allocate_vector(0);
    cont_stack->push(callable);

//    cout << "VM PROG: " << callable.to_write_str() << endl;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    Atom static_nil_atom;
#define P_A  ((PC.a))
#define P_B  ((PC.b))
#define P_O  ((PC.o))
#define PE_A ((PC.ae))
#define PE_B ((PC.be))
#define PE_O ((PC.oe))

#define E_SET_CHECK_REALLOC_D(env_idx, idx) do { \
    if ((env_idx) == 0) \
    { \
        if (((size_t) idx) >= cur_env->m_len) \
            cur_env->check_size((idx)); \
    } \
    else if ((env_idx) > 0) \
    { \
        if (((size_t) env_idx) >= env_stack->m_len) \
            error("out of env stack range (" #env_idx ")", Atom(T_INT, (env_idx))); \
        AtomVec &v = *(env_stack->m_data[env_stack->m_len - ((env_idx) + 1)].m_d.vec); \
        if (((size_t) idx) >= v.m_len) \
            v.check_size((idx)); \
    } \
} while (0)
#define E_SET_CHECK_REALLOC(env_idx_reg, idx_reg) \
    E_SET_CHECK_REALLOC_D(PE_##env_idx_reg, P_##idx_reg)

#define E_SET_D_PTR(env_idx, idx, val_ptr) do { \
    if ((env_idx) == 0) \
    { \
        val_ptr = &(cur_env->m_data[(idx)]); \
    } \
    else if ((env_idx) > 0) \
    { \
        AtomVec &v = *(env_stack->m_data[env_stack->m_len - ((env_idx) + 1)].m_d.vec); \
        val_ptr = &(v.m_data[(idx)]); \
    } \
} while (0)

#define E_SET_D(env_idx, idx, val) do { \
    if ((env_idx) == 0) \
    { \
        cur_env->m_data[(idx)] = (val); \
    } \
    else if ((env_idx) > 0) \
    { \
        AtomVec &v = *(env_stack->m_data[env_stack->m_len - ((env_idx) + 1)].m_d.vec); \
        v.m_data[(idx)] = (val); \
    } \
} while (0)

#define E_SET(reg, val) E_SET_D(PE_##reg, P_##reg, (val))

#define E_GET(out_ptr, reg) do { \
    if (PE_##reg == 0) \
    { \
        if (P_##reg >= (int32_t) cur_env->m_len) \
            (out_ptr) = &static_nil_atom; \
        else \
            (out_ptr) = &(cur_env->m_data[P_##reg]); \
    } \
    else if (PE_##reg > 0) \
    { \
        if (PE_##reg >= (int32_t) env_stack->m_len) \
            error("out of env stack range (" #reg ")", Atom(T_INT, PE_##reg)); \
        AtomVec &v = *(env_stack->m_data[env_stack->m_len - (PE_##reg + 1)].m_d.vec); \
        if (P_##reg >= (int32_t) v.m_len) \
            (out_ptr) = &static_nil_atom; \
        else \
            (out_ptr) = &(v.m_data[P_##reg]); \
    } \
    else if (PE_##reg == -1) \
    { \
        if (P_##reg >= (int32_t) data_len) \
            error("static data address out of range", Atom(T_INT, P_##reg)); \
        (out_ptr) = &(data[P_##reg]); \
    } \
    else if (PE_##reg == -2) \
    { \
        if (P_##reg >= (int32_t) m_prim_table->m_len) \
            error("primitive address out of range", Atom(T_INT, P_##reg)); \
        (out_ptr) = &(m_prim_table->m_data[P_##reg]); \
    } \
} while (0)

    Atom ret;
    Atom *tmp = nullptr;

    bool alloc = false;
    while (m_pc->op != OP_END)
    {
        INST &PC = *m_pc;

        if (m_trace)
        {
            cout << "VMTRC FRMS(" << cont_stack->m_len << "/" << env_stack->m_len << "): ";
            m_pc->to_string(cout);
            cout << " {ENV= ";
            for (size_t i = 0; i < cur_env->m_len; i++)
            {
                Atom &a = cur_env->m_data[i];
                if (a.m_type == T_UD && a.m_d.ud->type() == "VM-PROG")
                    cout << i << "[#<prog>] ";
                else
                    cout << i << "[" << a.to_write_str() << "] ";
            }
            cout << "}" << endl;
        }

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
                E_SET_CHECK_REALLOC(O, O);
                E_GET(tmp, A);
                E_SET(O, *tmp);
                break;
            }

            case OP_NEW_VEC:
            {
                E_SET_CHECK_REALLOC(O, O);
                alloc = true;
                E_SET(O, Atom(T_VEC, m_rt->m_gc.allocate_vector(P_A)));
                break;
            }

            case OP_NEW_MAP:
            {
                E_SET_CHECK_REALLOC(O, O);
                alloc = true;
                E_SET(O, Atom(T_MAP, m_rt->m_gc.allocate_map()));
                break;
            }

            case OP_CSET_VEC:
            {
                E_GET(tmp, O);
                Atom &vec = *tmp;

                E_GET(tmp, B);
                if (vec.m_type == T_VEC)
                {
                    // XXX: Attention, *tmp is taken as reference and assigned
                    //      to a vector it is itself part of. In this case, the
                    //      vector might get reallocated and the *tmp reference
                    //      might be invalidated!
                    //      However, as *tmp comes from the current environment
                    //      frame, this should not happen, until we get
                    //      direct access to the environment/frame vector.
                    vec.m_d.vec->set(P_A, *tmp);
                }
                else if (vec.m_type == T_MAP)
                {
                    Atom *key;
                    E_GET(key, A);
                    vec.m_d.map->set(*key, *tmp);
                }
                else
                    error("Can CSET_VEC on vector and map", vec);

                break;
            }

            case OP_SET:
            {
                E_GET(tmp, O);
                Atom &vec = *tmp;
                Atom *key;
                E_GET(key, A);

                E_GET(tmp, B);
                if (vec.m_type == T_VEC)
                {
                    // XXX: Attention, *tmp is taken as reference and assigned
                    //      to a vector it is itself part of. In this case, the
                    //      vector might get reallocated and the *tmp reference
                    //      might be invalidated!
                    //      However, as *tmp comes from the current environment
                    //      frame, this should not happen, until we get
                    //      direct access to the environment/frame vector.
                    vec.m_d.vec->set((size_t) key->to_int(), *tmp);
                }
                else if (vec.m_type == T_MAP)
                {
                    vec.m_d.map->set(*key, *tmp);
                }
                else
                    error("Can SET on vector and map", vec);

                break;
            }

            case OP_GET:
            {
                E_SET_CHECK_REALLOC(O, O);

                E_GET(tmp, A);
                Atom &vec = *tmp;

                Atom *key;
                E_GET(key, B);

                if (vec.m_type == T_VEC)
                {
                    E_SET(O, vec.m_d.vec->at((size_t) key->to_int()));
                }
                else if (vec.m_type == T_MAP)
                {
                    E_SET(O, vec.m_d.map->at(*key));
                }
                else
                    error("Can GET on vector and map", vec);

                break;
            }

            case OP_LOAD_NIL:
            {
                E_SET_CHECK_REALLOC(O, O);
                E_SET(O, Atom());
                break;
            }

            case OP_PUSH_ENV:
            {
                alloc = true;
                env_stack->push(
                    Atom(T_VEC, cur_env = m_rt->m_gc.allocate_vector(P_A)));
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
                E_SET_CHECK_REALLOC(O, O);

                alloc = true;

                E_GET(tmp, A);
                Atom &prog = *tmp;
                if (prog.m_type != T_UD || prog.m_d.ud->type() != "VM-PROG")
                    error("VM can't make closure from non VM-PROG", prog);

                Atom cl(T_CLOS);
                cl.m_d.vec = m_rt->m_gc.allocate_vector(2);
//                std::cout << "NEW CLOS " << ((void *) cl.m_d.vec) << "@" << ((void *) cl.m_d.vec->m_data) << std::endl;
                cl.m_d.vec->set(0, prog); // XXX: m_data[0] = prog;
                cl.m_d.vec->set(1,        //      m_data[1] =
                    Atom(T_VEC, m_rt->m_gc.clone_vector(env_stack)));

                E_SET(O, cl);
                break;
            }

            case OP_PACK_VA:
            {
                size_t pack_idx = P_A;
                if (pack_idx < cur_env->m_len)
                {
                    size_t va_len = cur_env->m_len - pack_idx;
                    AtomVec *v = m_rt->m_gc.allocate_vector(va_len);
                    for (size_t i = 0; i < va_len; i++)
                        v->m_data[i] = cur_env->m_data[i + pack_idx];
                    Atom vv(T_VEC, v);
                    E_SET_CHECK_REALLOC(O, O);
                    E_SET(O, vv);
                }
                else
                {
                    E_SET_CHECK_REALLOC(O, O);
                    E_SET(O, Atom(T_VEC, m_rt->m_gc.allocate_vector(0)));
                }
                break;
            }

            case OP_CALL:
            {
                E_SET_CHECK_REALLOC(O, O);

                E_GET(tmp, A);
                Atom &func = *tmp;
                E_GET(tmp, B);
                Atom &argv = *tmp;

                if (argv.m_type != T_VEC)
                    error("CALL argv not a vector", argv);

                if (func.m_type == T_PRIM)
                {
                    Atom *ot;
                    E_SET_D_PTR(PE_O, P_O, ot);
                    (*func.m_d.func)(*(argv.m_d.vec), *ot);
//                    cout << "argv: " << argv.to_write_str() << "; RET PRIM: "  << ret.to_write_str() << endl;
                    if (m_trace) cout << "CALL=> " << ot->to_write_str() << endl;

//                    m_rt->m_gc.give_back_vector(argv.m_d.vec, true);
                }
                else if (func.m_type == T_CLOS)
                {
                    if (func.m_d.vec->m_len < 2)
                        error("Bad closure found", func);
                    if (func.m_d.vec->m_data[0].m_type != T_UD)
                        error("Bad closure found", func);

                    alloc = true;
                    AtomVec *cont = m_rt->m_gc.allocate_vector(6);

                    Atom prog(T_UD);
                    prog.m_d.ud = (UserData *) m_prog;
                    Atom pc(T_C_PTR);
                    pc.m_d.ptr = m_pc;

                    cont->m_data[0] = prog;
                    cont->m_data[1] = Atom(T_VEC, env_stack);
                    cont->m_data[2] = pc;
                    cont->m_data[3] = Atom(T_INT, P_O);
                    cont->m_data[4] = Atom(T_INT, PE_O);
                    // just for keepin a reference to the called function:
                    cont->m_data[5] = func;

                    // replace current function with continuation
                    *cont_stack->last() = Atom(T_VEC, cont);
                    cont_stack->push(func); // save the current function

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

                // We get the returnvalue here, so we don't get corrupt
                // addresses when the later E_SET_D is maybe reallocating
                // some address space we might be refering to.
                Atom ret_val;
                {
                    Atom *tmp = nullptr;
                    E_GET(tmp, O);
                    ret_val = *tmp;
                }

                cont_stack->pop(); // the current function can be discarded
                // retrieve the continuation:
                Atom *c = cont_stack->last();
                if (!c || c->m_type == T_NIL)
                {
                    ret = ret_val;
                    m_pc = &(m_prog->m_instructions[m_prog->m_instructions_len - 2]);
                    break;
                }

                Atom cont = *c;
                cont_stack->pop();
                if (cont.m_type != T_VEC || cont.m_d.vec->m_len < 4)
                    error("Empty or bad continuation stack item!", cont);

                env_stack->pop();

                Atom *cv = cont.m_d.vec->m_data;

                Atom proc    = cv[0];
                Atom envs    = cv[1];
                Atom pc      = cv[2];
                int32_t oidx = (int32_t) cv[3].m_d.i;
                int32_t eidx = (int32_t) cv[4].m_d.i;

                // save current function for gc:
                cont_stack->push(proc);
                m_prog   = dynamic_cast<PROG*>(proc.m_d.ud);
                data     = m_prog->data_array();
                data_len = m_prog->data_array_len();
                m_pc     = (INST *) pc.m_d.ptr;

                env_stack = envs.m_d.vec;
                cur_env   = env_stack->last()->m_d.vec;

                E_SET_CHECK_REALLOC_D(eidx, oidx);
                E_SET_D(eidx, oidx, ret_val);
                if (m_trace) cout << "RETURN(" << oidx << ":" << eidx << ") => " << ret_val.to_write_str() << endl;

//                cout << "VMRETURN VAL: " << retval.to_write_str() << endl;

                break;
            }

            case OP_SET_RETURN:
            {
                E_GET(tmp, A);
                ret = *tmp;
                if (m_trace) cout << "SET_RETURN=> " << ret.to_write_str() << endl;
                break;
            }

            case OP_BR:
            {
                m_pc += P_O;
                if (m_pc >= (m_prog->m_instructions + m_prog->m_instructions_len))
                    error("BR out of PROG", Atom());
                break;
            }

            case OP_BRIF:
            {
                E_GET(tmp, A);
                if (!tmp->is_false())
                {
                    m_pc += P_O;
                    if (m_pc >= (m_prog->m_instructions + m_prog->m_instructions_len))
                        error("BRIF out of PROG", Atom());
                }
                break;
            }

            case OP_BRNIF:
            {
                E_GET(tmp, A);
                if (tmp->is_false())
                {
                    m_pc += P_O;
                    if (m_pc >= (m_prog->m_instructions + m_prog->m_instructions_len))
                        error("BRIF out of PROG", Atom());
                }
                break;
            }

            case OP_FORINC:
            {
                E_SET_CHECK_REALLOC(O, O);

                size_t idx_o = P_O + 1;
                if (idx_o >= cur_env->m_len)
                    cur_env->set(idx_o, Atom(T_INT));

                Atom *step;
                E_GET(step, A);
                E_GET(tmp, B);

                Atom *ot;
                E_SET_D_PTR(PE_O, (P_O + 1), ot);
                Atom &o = *ot;

                Atom *condt;
                E_SET_D_PTR(PE_O, P_O, condt);
                Atom &cond = *condt;

                bool update = cond.m_type == T_BOOL;

                if (o.m_type == T_DBL)
                {
                    double step_i = step->to_dbl();
                    double i = o.m_d.d;
                    if (update) i += step_i;
                    cond = Atom(T_BOOL, step_i > 0.0
                                        ? i > tmp->to_dbl()
                                        : i < tmp->to_dbl());
                    o.m_d.d = i;
                }
                else
                {
                    int64_t step_i = step->to_int();
                    int64_t i = o.m_d.i;
                    if (update) i += step_i;
                    cond = Atom(T_BOOL, step_i > 0
                                        ? i > tmp->to_int()
                                        : i < tmp->to_int());
                    o.m_d.i = i;
                }

                break;
            }

            case OP_NOT:
            {
                E_SET_CHECK_REALLOC(O, O);

                E_GET(tmp, A);
                Atom o(T_BOOL);
                o.m_d.b = tmp->is_false();
                E_SET(O, o);
                break;
            }

            case OP_EQ:
            {
                E_SET_CHECK_REALLOC(O, O);

                E_GET(tmp, A);
                Atom &a = *tmp;
                E_GET(tmp, B);
                Atom &b = *tmp;

                Atom o(T_BOOL);
                if (a.m_type == T_DBL)
                    o.m_d.b = std::fabs(a.to_dbl() - b.to_dbl())
                              < std::numeric_limits<double>::epsilon();
                else
                    o.m_d.b = a.to_int() == b.to_int();
                E_SET(O, o);
                break;
            }

            case OP_NEQ:
            {
                E_SET_CHECK_REALLOC(O, O);

                E_GET(tmp, A);
                Atom &a = *tmp;
                E_GET(tmp, B);
                Atom &b = *tmp;

                Atom o(T_BOOL);
                if (a.m_type == T_DBL)
                    o.m_d.b = std::fabs(a.to_dbl() - b.to_dbl())
                              >= std::numeric_limits<double>::epsilon();
                else
                    o.m_d.b = a.to_int() != b.to_int();
                E_SET(O, o);
                break;
            }

            case OP_ITER:
            {
                E_SET_CHECK_REALLOC(O, O);

                E_GET(tmp, A);
                Atom &vec = *tmp;

                alloc = true;
                Atom iter(T_VEC, m_rt->m_gc.allocate_vector(2));
                if (vec.m_type == T_VEC)
                {
                    iter.m_d.vec->m_data[0] = Atom(T_INT, -1);
                }
                else if (vec.m_type == T_MAP)
                {
                    AtomMapIterator *mi = new AtomMapIterator(vec);
                    Atom ud(T_UD);
                    ud.m_d.ud = mi;
                    iter.m_d.vec->m_data[0] = ud;
                }
                else
                    error("Can't ITER on non map or non vector", vec);

                iter.m_d.vec->m_data[1] = vec;
                E_SET(O, iter);
                break;
            }

            case OP_NEXT:
            {
                E_SET_CHECK_REALLOC(O, O);
                E_SET_CHECK_REALLOC(A, A);

                E_GET(tmp, B);
                Atom &iter = *tmp;
                if (iter.m_type != T_VEC && iter.m_d.vec->m_len < 2)
                    error("Bad iterator found in NEXT", iter);

                Atom *iter_elems = iter.m_d.vec->m_data;

                if (iter_elems[0].m_type == T_INT)
                {
                    int64_t &i = iter_elems[0].m_d.i;
                    i++;
                    Atom b(T_BOOL);
                    b.m_d.b = i >= iter_elems[1].m_d.vec->m_len;
                    E_SET(O, b);
                    if (!b.m_d.b)
                    {
                        E_SET(A, iter_elems[1].m_d.vec->at((size_t) i));
                    }
                }
                else if (iter_elems[0].m_type == T_UD)
                {
                    AtomMapIterator *mi =
                        dynamic_cast<AtomMapIterator *>(iter_elems[0].m_d.ud);
                    mi->next();
                    Atom b(T_BOOL);
                    b.m_d.b = !mi->ok();
                    E_SET(O, b);
                    if (!b.m_d.b)
                    {
                        E_SET(A, mi->value());
                    }
                }
                break;
            }

            case OP_IKEY:
            {
                E_SET_CHECK_REALLOC(O, O);
                E_GET(tmp, B);
                Atom &iter = *tmp;
                if (iter.m_type != T_VEC && iter.m_d.vec->m_len < 2)
                    error("Bad iterator found in NEXT", iter);

                Atom *iter_elems = iter.m_d.vec->m_data;

                if (iter_elems[0].m_type == T_INT)
                {
                    E_SET(O, iter_elems[0]);
                }
                else if (iter_elems[0].m_type == T_UD)
                {
                    AtomMapIterator *mi =
                        dynamic_cast<AtomMapIterator *>(iter_elems[0].m_d.ud);
                    E_SET(O, mi->key());
                }
                break;
            }

#define     DEFINE_NUM_OP_BOOL(opname, oper)              \
            case OP_##opname:                             \
            {                                             \
                E_SET_CHECK_REALLOC(O, O); \
                E_GET(tmp, A);                            \
                Atom &a = *tmp;                           \
                E_GET(tmp, B);                            \
                Atom &b = *tmp;                           \
                                                          \
                Atom o(T_BOOL);                           \
                if (a.m_type == T_DBL)                    \
                    o.m_d.b = a.to_dbl() oper b.to_dbl(); \
                else                                      \
                    o.m_d.b = a.to_int() oper b.to_int(); \
                E_SET(O, o);                              \
                break;                                    \
            }

#define     DEFINE_NUM_OP_NUM(opname, oper, neutr)                   \
            case OP_##opname:                                        \
            {                                                        \
                E_SET_CHECK_REALLOC(O, O);      \
                E_GET(tmp, A);                                       \
                Atom &a = *tmp;                                      \
                E_GET(tmp, B);                                       \
                Atom &b = *tmp;                                      \
                                                                     \
                Atom *ot;                                            \
                E_SET_D_PTR(PE_O, P_O, ot);                          \
                Atom &o = *ot;                                       \
                o.m_type = a.m_type;                                 \
                if (a.m_type == T_DBL)                               \
                    o.m_d.d = a.to_dbl() oper b.to_dbl();            \
                else if (a.m_type == T_NIL)                          \
                {                                                    \
                    o.m_type = b.m_type;                             \
                    if (b.m_type == T_DBL)                           \
                    {                                                \
                        o.m_d.d = (double) neutr;                    \
                        o.m_d.d = ((double) neutr) oper b.to_dbl();  \
                    }                                                \
                    else                                             \
                    {                                                \
                        o.m_d.i = (int64_t) neutr;                   \
                        o.m_d.i = ((int64_t) neutr) oper b.to_int(); \
                    }                                                \
                }                                                    \
                else                                                 \
                    o.m_d.i = a.to_int() oper b.to_int();            \
                break;                                               \
            }

            DEFINE_NUM_OP_NUM(ADD, +, 0)
            DEFINE_NUM_OP_NUM(SUB, -, 0)
            DEFINE_NUM_OP_NUM(MUL, *, 1)
            DEFINE_NUM_OP_NUM(DIV, /, 1)

            case OP_MOD:
            {
                E_SET_CHECK_REALLOC(O, O);

                E_GET(tmp, A);
                Atom &a = *tmp;
                E_GET(tmp, B);
                Atom &b = *tmp;

                Atom *ot;
                E_SET_D_PTR(PE_O, P_O, ot);
                Atom &o = *ot;
                o.m_type = a.m_type;
                if (a.m_type == T_DBL)
                    o.m_d.d = (double) (((int64_t) a.to_dbl()) % ((int64_t) b.to_dbl()));
                else
                    o.m_d.i = a.to_int() % b.to_int();
                break;
            }

            DEFINE_NUM_OP_BOOL(GE, >=)
            DEFINE_NUM_OP_BOOL(LE, <=)
            DEFINE_NUM_OP_BOOL(LT, <)
            DEFINE_NUM_OP_BOOL(GT, >)

            default:
                throw VMException("Unknown VM opcode: " + to_string(m_pc->op));
        }
        m_pc++;

        if (alloc)
        {
            m_rt->m_gc.collect_maybe();
            alloc = false;
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
#undef P_A
#undef P_B
#undef P_OUT
}
//---------------------------------------------------------------------------


}

/******************************************************************************
* Copyright (C) 2017 Weird Constructor
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
