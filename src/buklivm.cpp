// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "buklivm.h"
#include "atom_printer.h"
#include "atom_cpp_serializer.h"
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
    for (size_t i = 0; i < m_atom_data.m_d.vec->m_len; i++)
    {
        ss << " [" << i << "]: " << write_atom(m_atom_data.m_d.vec->m_data[i]) << "\n";
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
    instr.c  = (int32_t) av->m_data[7].to_int();
    instr.ce = (int32_t) av->m_data[8].to_int();
}
//---------------------------------------------------------------------------

Atom make_prog(GC &gc, Atom prog_info)
{
    if (prog_info.m_type != T_VEC)
        return Atom();

    Atom data     = prog_info.m_d.vec->m_data[0];
    Atom prog     = prog_info.m_d.vec->m_data[1];
    Atom debug    = prog_info.m_d.vec->m_data[2];

    if (   data.m_type      != T_VEC
        || prog.m_type      != T_VEC
        || debug.m_type     != T_MAP)
        return Atom();

    PROG *new_prog =
        new PROG(gc,
                 data.m_d.vec->m_len,
                 prog.m_d.vec->m_len + 1);
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
            instr.c  = 0;
            instr.ce = 0;
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

#define END_PRIM(name) \
    }; \
    m_prim_sym_table->push(Atom(T_SYM, m_rt->m_gc.new_symbol(#name)));

#define END_PRIM_DOC(name, docstr) \
    }; \
    m_prim_sym_table->push(Atom(T_SYM, m_rt->m_gc.new_symbol(#name))); \
    m_vm->set_documentation(#name, docstr);

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
        func_desc.m_d.vec->push(m->get_func_name(this, i));
        func_desc.m_d.vec->push(Atom(T_INT, m_prim_table->m_len));
        func_desc.m_d.vec->push(m->get_func(this, i));
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

#define CHECK_ARITY(arity, argc)             \
    ((((arity).m_type == T_NIL)              \
       || ((arity).m_d.i == (argc))) ? 0     \
    : ((arity).m_d.i < 0)                    \
    ? (((argc) < -((arity).m_d.i)) ? -1 : 0) \
    : ((arity).m_d.i > (argc)) ? -1          \
    : ((arity).m_d.i < (argc)) ? 1           \
    : 0)
//---------------------------------------------------------------------------

void VM::report_arity_error(Atom &arity, size_t argc)
{
    int err = CHECK_ARITY(arity, argc);
    int exp = (int) abs(arity.m_d.i);
    if (err < 0)
    {
        error("lambda call with too few arguments, expected "
              + to_string(exp) + " but got",
              Atom(T_INT, argc));
    }
    else if (err > 0)
    {
        error("lambda call with too many arguments, expected "
              + to_string(exp) + " but got",
              Atom(T_INT, argc));
    }
}
//---------------------------------------------------------------------------

#define VM_JUMP_FRAME_SIZE 3
#define VM_JMP_PC    0
#define VM_JMP_OUT_I 1
#define VM_JMP_OUT_E 2

#define VM_CALL_FRAME_SIZE 7
#define VM_CF_CLOS  0
#define VM_CF_PROG  1
#define VM_CF_FRAME 2
#define VM_CF_ROOT  3
#define VM_CF_PC    4
#define VM_CF_OUT_I 5
#define VM_CF_OUT_E 6

//---------------------------------------------------------------------------

Atom VM::eval(Atom callable, AtomMap *root_env_map, AtomVec *args)
{
    using namespace std::chrono;

    GC_ROOT_MAP(m_rt->m_gc, root_env_map_gcroot) = root_env_map;

    AtomVec *root_env = nullptr;
    if (root_env_map)
    {
        Atom root_env_regs =
            root_env_map->at(
                Atom(T_STR, m_rt->m_gc.new_symbol(" REGS ")));
        if (root_env_regs.m_type == T_NIL)
        {
            root_env = m_rt->m_gc.allocate_vector(0);
            root_env_map->set(
                Atom(T_STR, m_rt->m_gc.new_symbol(" REGS ")),
                Atom(T_VEC, root_env));
        }
        else if (root_env_regs.m_type != T_VEC)
        {
            error("Bad registers from root_env in VM", root_env_regs);
        }
        else
        {
            root_env = root_env_regs.m_d.vec;
        }
    }
    else
        root_env = m_rt->m_gc.allocate_vector(0);

    AtomVec *rr_frame   = nullptr;
    AtomVec *rr_data    = nullptr;
    Atom    *rr_v_frame = nullptr;
    Atom    *rr_v_data  = nullptr;
    Atom    *rr_v_root  = root_env->m_data;
    GC_ROOT(m_rt->m_gc, reg_rows_ref) =
        Atom(T_UD, new RegRowsReference(&rr_frame, &rr_data, &root_env));

#define SET_FRAME_ROW(row_ptr)  rr_frame = row_ptr;  rr_v_frame = rr_frame->m_data;
#define SET_DATA_ROW(row_ptr)   rr_data  = row_ptr;  rr_v_data  = rr_data->m_data;
#define SET_ROOT_ENV(vecptr)    root_env = (vecptr); rr_v_root  = root_env->m_data;

    PROG *prog = nullptr;
    INST *pc   = nullptr;

    if (!args) args = m_rt->m_gc.allocate_vector(0);

    GC_ROOT_VEC(m_rt->m_gc, args_root)     = args;
    GC_ROOT_VEC(m_rt->m_gc, cont_stack)    = nullptr;
    GC_ROOT_VEC(m_rt->m_gc, empty_upv_row) = m_rt->m_gc.allocate_vector(0);
    AtomVec *clos_upvalues = empty_upv_row;

    if (callable.m_type == T_UD && callable.m_d.ud->type() == "VM-PROG")
    {
        prog      = dynamic_cast<PROG*>(callable.m_d.ud);
        pc        = &(prog->m_instructions[0]);
    }
    else if (callable.m_type == T_CLOS)
    {
        if (   callable.m_d.vec->m_len == VM_CLOS_SIZE
            && callable.m_d.vec->m_data[0].m_type == T_UD)
        {
            prog = dynamic_cast<PROG*>(callable.m_d.vec->m_data[0].m_d.ud);
            pc   = &(prog->m_instructions[0]);
            if (CHECK_ARITY(callable.m_d.vec->m_data[3], args->m_len) != 0)
                report_arity_error(callable.m_d.vec->m_data[3], args->m_len);
            if (callable.m_d.vec->m_data[1].m_d.vec)
                clos_upvalues = callable.m_d.vec->m_data[1].m_d.vec;
            // TODO: Handle calling a coroutine!
            // FIXME: Handle copying upvalues into current frame (aka 'args')!
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

    SET_DATA_ROW(m_prog->data_array());
    SET_FRAME_ROW(args);
//    SET_UPV_ROW(clos_upvalues);
//    SET_PRIM_ROW(m_prim_table);

    // XXX: The root-call-frame actually keeps alive the whole code-tree, including
    //      any callable sub-m_prog objects. Thus, we don't have to explicitly
    //      keep alive the m-prog or it's data_array().
    //
    //      => We just need to keep alive the UPV_ROW and the FRAME_ROW.
    cont_stack = m_rt->m_gc.allocate_vector(VM_CALL_FRAME_SIZE);
    {
        AtomVec *call_frame = m_rt->m_gc.allocate_vector(7);
        call_frame->set(VM_CF_CLOS, callable);
        call_frame->set(VM_CALL_FRAME_SIZE - 1, Atom());
        cont_stack->push(Atom(T_VEC, call_frame));
    }

//    cout << "VM PROG: " << callable.to_write_str() << endl;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    Atom static_nil_atom;
#define P_A  ((m_pc->a))
#define P_B  ((m_pc->b))
#define P_C  ((m_pc->c))
#define P_O  ((m_pc->o))
#define PE_A ((m_pc->ae))
#define PE_B ((m_pc->be))
#define PE_C ((m_pc->ce))
#define PE_O ((m_pc->oe))

#define E_SET_CHECK_REALLOC_D(env_idx, idx) do { \
    switch ((env_idx)) { \
    case REG_ROW_UPV: \
    case REG_ROW_FRAME: if (rr_frame->m_len <= (size_t) (idx)) { rr_frame->check_size((idx)); rr_v_frame = rr_frame->m_data; } break; \
    case REG_ROW_ROOT: if (root_env->m_len <= (size_t) (idx)) { root_env->check_size((idx)); rr_v_root = root_env->m_data; } break; \
    case REG_ROW_DATA: if (rr_data->m_len <= (size_t) (idx)) { rr_data->check_size((idx)); rr_v_data = rr_data->m_data; } break; \
    default: \
        error("bad reg_row reference (" #env_idx ")", Atom(T_INT, (env_idx))); \
    }\
} while (0)
#define E_SET_CHECK_REALLOC(env_idx_reg, idx_reg) \
    E_SET_CHECK_REALLOC_D(PE_##env_idx_reg, P_##idx_reg)

#define E_SET_D_PTR(env_idx, idx, val_ptr) do { \
    switch ((env_idx)) { \
    case REG_ROW_FRAME: val_ptr = &(rr_v_frame[(idx)]); break; \
    case REG_ROW_DATA:  val_ptr = &(rr_v_data[(idx)]); break; \
    case REG_ROW_ROOT:  val_ptr = &(rr_v_root[(idx)]); break; \
    case REG_ROW_UPV: \
    { \
        val_ptr = &(rr_v_frame[(idx)]); \
        if (env_idx == REG_ROW_UPV) val_ptr = &(val_ptr->m_d.vec->m_data[0]); \
        break; \
    } \
    case REG_ROW_PRIM: \
    { \
        val_ptr = &(m_prim_table->m_data[(idx)]); \
        break; \
    } \
    } \
} while (0)

#define E_SET_D(env_idx, idx, val) do { \
    switch ((env_idx)) { \
    case REG_ROW_FRAME: rr_v_frame[(idx)] = (val); break; \
    case REG_ROW_DATA: rr_v_data[(idx)] = (val); break; \
    case REG_ROW_ROOT: rr_v_root[(idx)] = (val); break; \
    case REG_ROW_UPV: rr_v_frame[(idx)].m_d.vec->m_data[0] = (val); break; \
    } \
} while (0)

#define E_SET(reg, val) E_SET_D(PE_##reg, P_##reg, (val))

#define E_GET_D(out_ptr, eidx, idx) do { \
    switch ((eidx)) { \
    case REG_ROW_FRAME: (out_ptr) = &(rr_v_frame[(idx)]); break; \
    case REG_ROW_DATA: (out_ptr) = &(rr_v_data[(idx)]); break; \
    case REG_ROW_ROOT: (out_ptr) = &(rr_v_root[(idx)]); break; \
    case REG_ROW_UPV: \
    { \
        (out_ptr) = &(rr_v_frame[(idx)]); \
        if (out_ptr->m_type != T_VEC || out_ptr->m_d.vec->m_len <= 0) \
            error("Bad upvalue access", Atom(T_INT, (idx))); \
        (out_ptr) = &(out_ptr->m_d.vec->m_data[0]); \
        break; \
    } \
    case REG_ROW_PRIM: \
        (out_ptr) = &(m_prim_table->m_data[(idx)]); \
    } \
} while (0)

#define E_GET(out_ptr, reg) E_GET_D(out_ptr, PE_##reg, P_##reg)

#define RESTORE_FROM_CALL_FRAME(call_frame, ret_val)  \
    do {                                              \
        Atom *cv = (call_frame).m_d.vec->m_data;      \
        if (cv[1].m_type == T_NIL)                    \
        {                                             \
            ret = (ret_val);                          \
            m_pc =                                    \
                &(m_prog->m_instructions[             \
                    m_prog->m_instructions_len - 2]); \
            break;                                    \
        }                                             \
        Atom proc    = cv[VM_CF_PROG];                \
        Atom frame   = cv[VM_CF_FRAME];               \
        Atom r_env   = cv[VM_CF_ROOT];                \
        Atom pc      = cv[VM_CF_PC];                  \
        int32_t oidx = (int32_t) cv[VM_CF_OUT_I].m_d.i; \
        int8_t  eidx = (int8_t)  cv[VM_CF_OUT_E].m_d.i; \
                                                      \
        m_prog   = static_cast<PROG*>(proc.m_d.ud);   \
        m_pc     = (INST *) pc.m_d.ptr;               \
        SET_DATA_ROW(m_prog->data_array());           \
        SET_FRAME_ROW(frame.m_d.vec);                 \
        SET_ROOT_ENV(r_env.m_d.vec);                  \
                                                      \
        E_SET_CHECK_REALLOC_D(eidx, oidx);            \
        E_SET_D(eidx, oidx, (ret_val));               \
        if (m_trace)                                  \
            cout << "RETURN(" << oidx << ":" << eidx  \
                 << ") => "                           \
                 << (ret_val).to_write_str() << endl; \
    } while(0);

#define SEARCH_AND_POP_TO_LAST_CALL_FRAME(c)                     \
    c = cont_stack->last();                           \
    while (   c                                       \
           && c->m_type == T_VEC                      \
           && c->m_d.vec->m_len < VM_CALL_FRAME_SIZE) \
    {                                                 \
        cont_stack->pop();                            \
        c = cont_stack->last();                       \
    }

#define RECORD_CALL_FRAME(func, call_frame)             \
    Atom call_frame(T_VEC,                              \
        m_rt->m_gc.allocate_vector(VM_CALL_FRAME_SIZE));\
    do {                                                \
        AtomVec *cv = call_frame.m_d.vec;               \
        cv->m_len = VM_CALL_FRAME_SIZE;                 \
        Atom *data = cv->m_data;                        \
                                                        \
        data[VM_CF_CLOS].set_clos((func).m_d.vec);      \
        data[VM_CF_PROG].set_ud((UserData *) m_prog);   \
        data[VM_CF_FRAME].set_vec(rr_frame);            \
        data[VM_CF_ROOT].set_vec(root_env);             \
        data[VM_CF_PC].set_ptr(m_pc);                   \
        data[VM_CF_OUT_I].set_int(P_O);                 \
        data[VM_CF_OUT_E].set_int(PE_O);                \
    } while(0)

    Atom ret;
    Atom *tmp = nullptr;

    bool alloc = false;
    try
    {
        while (m_pc->op != OP_END)
        {
//            INST &PC = *m_pc;

            if (m_trace)
            {
                cout << "VMTRC FRMS(" << cont_stack->m_len << "): ";
                m_pc->to_string(cout);
                cout << "ROOT: (" << Atom(T_VEC, root_env).id() << ")";
                cout << " {ENV= ";
                for (size_t i = 0; i < rr_frame->m_len; i++)
                {
                    Atom &a = rr_v_frame[i];
                    if (a.m_type == T_UD && a.m_d.ud->type() == "VM-PROG")
                        cout << i << "[#<prog>] ";
                    else if (a.m_type == T_MAP && a.size() > 5)
                        cout << i << "[map:" << ((void *) a.m_d.map) << "] ";
                    else if (a.m_type == T_VEC && a.size() > 5)
                        cout << i << "[vec:" << ((void *) a.m_d.vec) << "] ";
                    else
                        cout << i << "[" << a.to_write_str() << "] ";
                }
                cout << "}" << get_current_debug_info().to_write_str() << endl;
            }

            switch (m_pc->op)
            {
                case OP_DUMP_ENV_STACK:
                {
                // TODO: Check
//                    for (size_t i = 0; i < frm_stack->m_len; i++)
//                    {
//                        cout << "ENV@" << (frm_stack->m_len - (i + 1)) << ": ";
//                        AtomVec *env = frm_stack->at(i).m_d.vec;
//                        for (size_t j = 0; j < env->m_len; j++)
//                            cout << "[" << j << "=" << env->at(j).to_write_str() << "] ";
//                        cout << endl;
//                    }
                    break;
                }

                case OP_MOV:
                {
                    E_SET_CHECK_REALLOC(O, O);
                    E_GET(tmp, A);
                    E_SET(O, *tmp);
                    break;
                }

                case OP_NEW_ARG_VEC:
                {
                    E_SET_CHECK_REALLOC(O, O);
                    alloc = true;

                    AtomVec *out_av = m_rt->m_gc.allocate_vector(P_A);
                    E_SET(O, Atom(T_VEC, out_av));

                    E_GET(tmp, B);
                    if (tmp->m_type != T_VEC)
                        error("Bad argument index vector!", *tmp);
                    AtomVec *av = tmp->m_d.vec;

                    if (P_A > 0)
                        out_av->check_size(P_A - 1);
                    for (size_t i = 0, j = 0; i < av->m_len; i += 2, j++)
                    {
                        E_GET_D(
                            tmp,
                            av->m_data[i + 1].m_d.i,
                            av->m_data[i].m_d.i);
                        out_av->m_data[j] = *tmp;
                    }

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

                    AtomMap *out_am = m_rt->m_gc.allocate_map();
                    E_SET(O, Atom(T_VEC, out_am));

                    E_GET(tmp, A);
                    if (tmp->m_type != T_VEC)
                        error("Bad argument index vector!", *tmp);
                    AtomVec *av = tmp->m_d.vec;

                    for (size_t i = 0; i < av->m_len; i += 4)
                    {
                        Atom *key_idx  = &(av->m_data[i]);
                        Atom *key_eidx = &(av->m_data[i + 1]);
                        Atom *val_idx  = &(av->m_data[i + 2]);
                        Atom *val_eidx = &(av->m_data[i + 3]);
                        Atom *key, *val;
                        E_GET_D(key, key_eidx->m_d.i, key_idx->m_d.i);
                        E_GET_D(val, val_eidx->m_d.i, val_idx->m_d.i);
                        out_am->set(*key, *val);
                    }

                    E_SET(O, Atom(T_MAP, out_am));
                    break;
                }

                case OP_CSET_VEC:
                {
                    Atom *vec;
                    E_GET(vec, O);

                    E_GET(tmp, B);
                    switch (vec->m_type)
                    {
                        case T_VEC:
                            // XXX: Attention, *tmp is taken as reference and assigned
                            //      to a vector it is itself part of. In this case, the
                            //      vector might get reallocated and the *tmp reference
                            //      might be invalidated!
                            //      However, as *tmp comes from the current environment
                            //      frame, this should not happen, until we get
                            //      direct access to the environment/frame vector.
                            vec->m_d.vec->set(P_A, *tmp);
                            break;
                        case T_MAP:
                        {
                            Atom *key;
                            E_GET(key, A);
                            vec->m_d.map->set(*key, *tmp);
                            break;
                        }
                        default:
                            error("Can CSET_VEC on vector and map", *vec);
                    }
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

                case OP_NEW_CLOSURE:
                {
                    E_SET_CHECK_REALLOC(O, O);

                    alloc = true;

                    E_GET(tmp, A);
                    Atom &prog = *tmp;
                    if (prog.m_type != T_UD || prog.m_d.ud->type() != "VM-PROG")
                        error("VM can't make closure from non VM-PROG", prog);

                    E_GET(tmp, B);
                    if (tmp->m_type != T_VEC)
                        error("Can't create closure without info", *tmp);
                    bool is_coroutine = !(tmp->at(0).is_false());

                    AtomVec *upvalues = nullptr;
                    if (tmp->at(2).m_type == T_VEC)
                    {
                        AtomVec *clos_upv_idxes = tmp->at(2).m_d.vec;
                        upvalues =
                            m_rt->m_gc.allocate_vector(clos_upv_idxes->m_len);
                        for (size_t i = 0; i < clos_upv_idxes->m_len; i++)
                        {
                            if (i % 2 == 0)
                            {
                                AtomVec *upv_reg_row = rr_frame;
                                size_t upv_idx =
                                    (size_t) clos_upv_idxes->m_data[i].m_d.i;
                                if (upv_idx >= upv_reg_row->m_len)
                                    error("Bad upvalue index", Atom(T_INT, upv_idx));
                                upvalues->set(i, upv_reg_row->m_data[upv_idx]);
                            }
                            else
                                upvalues->set(i, clos_upv_idxes->m_data[i]);
                        }
                    }

    //                std::cout << "NEW CLOS " << ((void *) cl.m_d.vec) << "@" << ((void *) cl.m_d.vec->m_data) << std::endl;

                    Atom cl(T_CLOS, m_rt->m_gc.allocate_vector(VM_CLOS_SIZE));
                    cl.m_d.vec->m_len = VM_CLOS_SIZE;
                    cl.m_d.vec->m_data[0].set_ud(prog.m_d.ud);
                    cl.m_d.vec->m_data[1].set_vec(upvalues);
                    cl.m_d.vec->m_data[2].set_bool(is_coroutine);
                    cl.m_d.vec->m_data[3] = tmp->at(1);

                    E_SET(O, cl);
                    break;
                }

                case OP_NEW_UPV:
                {
                    E_GET(tmp, A);
                    Atom &val = *tmp;
                    if (PE_O != REG_ROW_UPV)
                        error("Can't set upvalue on non upvalue register row",
                              Atom(T_INT, PE_O));

                    Atom upv(T_VEC, m_rt->m_gc.allocate_vector(1));
                    upv.m_d.vec->set(0, val);
                    rr_frame->set(P_O, upv);
                    rr_v_frame = rr_frame->m_data;
                    break;
                }

                case OP_CALL:
                {
                    E_SET_CHECK_REALLOC(O, O);

                    Atom *func;
                    E_GET(func, A);
                    E_GET(tmp, B);
                    if (tmp->m_type != T_VEC)
                        error("Bad argument index vector!", *tmp);
                    AtomVec *av    = tmp->m_d.vec;
                    AtomVec *frame = nullptr;

                    if (PE_B == REG_ROW_DATA)
                    {
                        size_t len = av->m_len;
                        frame = m_rt->m_gc.allocate_vector(len);
                        frame->m_len = len >> 1;
                        for (size_t i = 0, j = 0; i < av->m_len; i += 2, j++)
                        {
                            E_GET_D(
                                tmp,
                                av->m_data[i + 1].m_d.i,
                                av->m_data[i].m_d.i);
                            frame->m_data[j] = *tmp;
                        }
                    }
                    else
                    {
                        frame = av;
                    }

                    switch (func->m_type)
                    {
                        case T_PRIM:
                        {
                            Atom *ot;
                            E_SET_D_PTR(PE_O, P_O, ot);
                            (*func->m_d.func)(*(frame), *ot);
        //                    cout << "frame: " << frame.to_write_str() << "; RET PRIM: "  << ret.to_write_str() << endl;
                            if (m_trace) cout << "CALL=> " << ot->to_write_str() << endl;

        //                    m_rt->m_gc.give_back_vector(frame.m_d.vec, true);
                            break;
                        }
                        case T_CLOS:
                        {
                            if (func->m_d.vec->m_len < VM_CLOS_SIZE)
                                error("Bad closure found", *func);
                            if (func->m_d.vec->m_data[0].m_type != T_UD)
                                error("Bad closure found", *func);

                            Atom &arity = func->m_d.vec->m_data[3];
                            if (CHECK_ARITY(arity, frame->m_len) != 0)
                                report_arity_error(arity, frame->m_len);

//                            std::cout << "ARITY" << func->m_d.vec->m_data[3].to_write_str() << std::endl;
                            if (arity.m_type == T_NIL
                                || (arity.m_type == T_INT && arity.m_d.i < 0))
                            {
                                unsigned int va_pos =
                                    arity.m_type == T_NIL ? 0
                                    : (arity.m_type == T_INT && arity.m_d.i < 0)
                                    ? ((unsigned int) (-arity.m_d.i))
                                    : frame->m_len;

//                                    std::cout << "VAVA" << frame->m_len << "," << va_pos << std::endl;

                                AtomVec *v = nullptr;
                                if (frame->m_len > va_pos)
                                {
                                    size_t va_len = frame->m_len - va_pos;
                                    v = m_rt->m_gc.allocate_vector(va_len);
                                    for (size_t i = 0; i < va_len; i++)
                                        v->m_data[i] = frame->m_data[i + va_pos];
                                    v->m_len = va_len;
                                }
                                else
                                    v = m_rt->m_gc.allocate_vector(0);
                                frame->set(va_pos, Atom(T_VEC, v));
                            }

                            alloc = true;
                            // save the current execution context:
                            RECORD_CALL_FRAME(*func, call_frame);
                            cont_stack->push(call_frame);

                            if (func->m_d.vec->m_data[2].m_type == T_VEC)
                            {
                                // XXX: Check if T_CLOS is a coroutine (!func->at(2).is_false())
                                //      Then instead of a new entry point in PROG, we restore
                                //      the former execution context by restoring the
                                //      call frames and the execution context
                                //      (m_prog, m_pc, frm_stack, output-register)
                                //      We also need to set the output-register to the
                                //      value of frame.at(0).

                                Atom ret_val = frame->at(0);

                                AtomVec *coro_cont_stack =
                                    func->m_d.vec->m_data[2].m_d.vec;
                                func->m_d.vec->m_data[2].set_bool(true);

                                Atom restore_frame = *(coro_cont_stack->last());
                                coro_cont_stack->pop();
                                for (size_t i = 0; i < coro_cont_stack->m_len; i++)
                                    cont_stack->push(coro_cont_stack->m_data[i]);

                                RESTORE_FROM_CALL_FRAME(restore_frame, ret_val);
                            }
                            else
                            {
                                m_prog   = dynamic_cast<PROG*>(func->m_d.vec->m_data[0].m_d.ud);
                                m_pc     = &(m_prog->m_instructions[0]);
                                m_pc--;

                                SET_DATA_ROW(m_prog->data_array());
                                AtomVec *upvs = func->m_d.vec->m_data[1].m_d.vec;
                                if (upvs->m_len > 0)
                                {
                                    for (size_t i = 0; i < upvs->m_len; i += 2)
                                    {
                                        frame->set(
                                            (size_t) upvs->m_data[i + 1].m_d.i,
                                            upvs->m_data[i]);
                                    }
                                }
                                SET_FRAME_ROW(frame);
                            }
                            break;
                        }
                        default:
                            error("CALL does not support that function type yet", *func);
                    }

                    break;
                }

                case OP_EVAL:
                {
                    E_SET_CHECK_REALLOC(O, O);

                    Atom *code;
                    E_GET(code, A);
                    Atom *env;
                    E_GET(env, B);

                    if (!root_env->m_meta
                        || root_env->m_meta->at(1).m_type != T_MAP)
                    {
                        error("Bad environment passed, REGS has no meta env attached",
                              Atom(T_VEC, root_env));
                    }
                    AtomMap *eval_env = root_env->m_meta->at(1).m_d.map;
                    if (env->m_type != T_NIL)
                    {
                        if (env->m_type != T_MAP)
                            error("Bad environment (not a map) to 'eval'", *env);
                        eval_env = env->m_d.map;
                    }

                    GC_ROOT(m_rt->m_gc, prog) =
                        m_compiler_call(*code, eval_env, "<vm-op-eval>", true);
                    if (prog.m_type != T_UD || prog.m_d.ud->type() != "VM-PROG")
                        error("In 'eval' compiler did not return proper VM-PROG",
                              prog);

                    // save the current execution context:
                    // FIXME: we should save the root-env-map in the call stack too!
                    RECORD_CALL_FRAME(prog, call_frame);
                    // XXX: We should record some kind of fake-closure here for
                    //      debugging purposes. or some kind of other marker, so
                    //      we can generate sensible stacktraces.
                    call_frame.m_d.vec->set(VM_CF_CLOS, prog);
                    cont_stack->push(call_frame);

//                    atom_tree_walker(call_frame, [](std::function<void(Atom &a)> con, unsigned int indent, Atom a)
//                    {
//                        for (unsigned int i = 0; i < indent; i++)
//                            std::cout << "  ";
//                        std::cout << "WALK " << a.to_shallow_str() << std::endl;
//                        con(a);
//                    });

                    m_prog   = dynamic_cast<PROG*>(prog.m_d.ud);
                    m_pc     = &(m_prog->m_instructions[0]);
                    m_pc--;
//                    std::cout << "PUT PROG: " << ((void *) m_pc) << ";" << ((void *) m_prog) << std::endl;

                    SET_DATA_ROW(m_prog->data_array());
                    AtomVec *eval_frame = m_rt->m_gc.allocate_vector(0);
                    SET_FRAME_ROW(eval_frame);
                    Atom cf_root_env =
                    eval_env->at(Atom(T_STR, m_rt->m_gc.new_symbol(" REGS ")));
                    if (cf_root_env.m_type != T_VEC)
                        error("Bad environment with 'eval', no [\" REGS \"] given",
                              cf_root_env);
                    SET_ROOT_ENV(cf_root_env.m_d.vec);
                    break;
                }

                case OP_RETURN:
                {

                    // We get the returnvalue here, so we don't get corrupt
                    // addresses when the later E_SET_D is maybe reallocating
                    // some address space we might be refering to.
                    Atom ret_val;
                    E_GET(tmp, O);
                    ret_val = *tmp;

                    // retrieve the continuation and skip non-call-frames:
                    Atom *c = nullptr;
                    SEARCH_AND_POP_TO_LAST_CALL_FRAME(c);

                    if (!c || c->m_type == T_NIL)
                    {
                        ret = ret_val;
                        m_pc = &(m_prog->m_instructions[m_prog->m_instructions_len - 2]);
                        break;
                    }

                    Atom call_frame = *c;
                    cont_stack->pop();
                    if (call_frame.m_type != T_VEC || call_frame.m_d.vec->m_len < VM_CALL_FRAME_SIZE)
                        error("Empty or bad call frame stack item!", call_frame);

                    RESTORE_FROM_CALL_FRAME(call_frame, ret_val);
                    break;
                }

                case OP_YIELD:
                {
                    E_GET(tmp, A);
                    Atom ret_val = *tmp;

                    Atom *c = nullptr;
                    for (size_t i = cont_stack->m_len; i > 0; i--)
                    {
                        c = &(cont_stack->m_data[i - 1]);
                        if (   c->m_type == T_VEC
                            && c->m_d.vec->m_len == VM_CALL_FRAME_SIZE)
                        {
                            break;
                        }
                    }

                    if (!c || c->m_type == T_NIL)
                        error("Can't yield from direct top level. "
                              "VM/Compiler ERROR!?!?!", *tmp);

                    Atom cur_func = c->at(0);
                    RECORD_CALL_FRAME(cur_func, coro_cont_frame);

                    AtomVec *cont_copy =
                        m_rt->m_gc.allocate_vector(cont_stack->m_len);

                    // We need to copy all call frames except the one
                    // with the coroutine in it (as that is the one,
                    // where we need to return to now).
                    // Additionally we need to record a "return point"
                    // which we point to when recalling the coroutine.
                    // This might be inside the call of a completely
                    // other closure in fact.

                    // =>
                    // 1. record current program state (m_prog, m_pc, frm_stack)
                    //    (current T_CLOS is part of the call frame on top of cont_stack)
                    // 1b. save the O register where to put the continuation-value
                    //     with the program state!
                    // 2. copy all cont_stack frames except the one with the coroutine in it.
                    // 3. save the prog-state and the cont_stack in the T_CLOS of
                    //    the coroutine.
                    // 4. return from the T_CLOS call as "usual" with the value
                    //    passed into YIELD as 'A'.
                    Atom ret_call_frame;
                    Atom func;
                    size_t ret_cont_stack_idx = 0;
                    for (size_t i = cont_stack->m_len; i > 0; i--)
                    {
                        Atom &frm = cont_stack->m_data[i - 1];
                        //d// cout << "FRM: " << frm.m_type << " F: " << frm.at(0).at(2).to_write_str() << endl;
                        if (!frm.at(0).at(2).is_false())
                        {
                            //d// cout << "FOUND COROUTINE CALL AT DEPTH="
                            //d//      << i << " of " << cont_stack->m_len << endl;
                            ret_call_frame = frm;
                            func           = frm.at(0);
                            ret_cont_stack_idx = i - 1;
                            break;
                        }

                        cont_copy->set((cont_stack->m_len - i), frm);
                    }

                    if (func.m_type == T_CLOS)
                    {
                        cont_copy->push(coro_cont_frame);
                        func.m_d.vec->set(2, Atom(T_VEC, cont_copy));
                    }
                    else
                        error("Can't 'yield' from a non-coroutine call!",
                              ret_val);

                    cont_stack->m_len = ret_cont_stack_idx;
                    RESTORE_FROM_CALL_FRAME(ret_call_frame, ret_val);
                    break;
                }

                case OP_PUSH_JMP:
                {
                    AtomVec *jmp_frame =
                        m_rt->m_gc.allocate_vector(
                            VM_JUMP_FRAME_SIZE);
                    jmp_frame->m_len = VM_JUMP_FRAME_SIZE;
                    jmp_frame->m_data[VM_JMP_PC].set_ptr(m_pc + P_A);
                    jmp_frame->m_data[VM_JMP_OUT_I].set_int(P_O);
                    jmp_frame->m_data[VM_JMP_OUT_E].set_int(PE_O);
                    cont_stack->push(Atom(T_VEC, jmp_frame));
                    break;
                }

                case OP_POP_JMP:
                {
                    Atom *c = cont_stack->last();
                    if (   !c
                        || c->m_type != T_VEC
                        || c->m_d.vec->m_len != VM_JUMP_FRAME_SIZE)
                        error("Bad exception handler frame on stack",
                              c ? *c : Atom());

                    cont_stack->pop();
                    break;
                }

                case OP_CTRL_JMP:
                {
                    Atom raised_val;
                    E_GET(tmp, O);
                    raised_val = *tmp;

                    Atom *c = cont_stack->last();
                    while (c && (   c->m_type != T_VEC
                                 || c->m_d.vec->m_len != VM_JUMP_FRAME_SIZE))
                    {
                        if (c->m_d.vec->m_len == VM_CALL_FRAME_SIZE)
                        {
                            Atom call_frame = *c;
                            Atom nil_val;
                            RESTORE_FROM_CALL_FRAME(call_frame, nil_val);
                        }
                        cont_stack->pop();
                        c = cont_stack->last();
                    }

                    // XXX: Idea for (with-cleanup ...), in the CTRL-JMP
                    //      we push a re-raise frame onto the control stack,
                    //      and create a new op, called OP_CHK_CTRL_JMP,
                    //      which looks for a marked re-raise frame and pops it,
                    //      and returns a flag and a value (.op-chk-ctrl-jmp cond-adr val-adr)
                    //      which can then be used by brnif and op-ctrl-jmp to reinitiate
                    //      a new throw. (We handle this case in the compiler!)
                    //
                    //      The with-cleanup is a new kind of control frame,
                    //      which is checked by RETURN and does the same as above.
                    //      But this leads to new problems: How to continue a return!?!?!?

                    if (   !c
                        || c->m_type == T_NIL
                        || (   c->m_type != T_VEC
                            && c->m_d.vec->m_len != VM_JUMP_FRAME_SIZE))
                        throw VMRaise(m_rt->m_gc, *tmp);

                    AtomVec *exh_frame = c->m_d.vec;
                    if (   exh_frame->at(VM_JMP_PC).m_type    != T_C_PTR
                        || exh_frame->at(VM_JMP_OUT_I).m_type != T_INT
                        || exh_frame->at(VM_JMP_OUT_E).m_type != T_INT)
                        error("Bad exception handler frame on call stack", *c);

                    E_SET_CHECK_REALLOC_D(
                        exh_frame->at(VM_JMP_OUT_E).m_d.i,
                        exh_frame->at(VM_JMP_OUT_I).m_d.i);
                    E_SET_D(
                        exh_frame->at(VM_JMP_OUT_E).m_d.i,
                        exh_frame->at(VM_JMP_OUT_I).m_d.i,
                        raised_val);

                    m_pc = (INST *) exh_frame->at(VM_JMP_PC).m_d.ptr;
                    if (m_pc >= (m_prog->m_instructions + m_prog->m_instructions_len))
                        error("CTRL_JMP out of PROG", Atom());
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

                    Atom *step;
                    E_GET(step, A);
                    E_GET(tmp, B);

                    Atom *ot;
                    E_GET(ot, C);

                    Atom *condt;
                    E_SET_D_PTR(PE_O, P_O, condt);

                    bool update = condt->m_type == T_BOOL;

                    switch (ot->m_type)
                    {
                        case T_INT:
                        {
                            int64_t step_i = step->m_d.i;
                            int64_t i = ot->m_d.i;
                            if (update) i += step_i;
                            condt->m_type = T_BOOL;
                            condt->m_d.b =
                                step_i > 0
                                ? i > tmp->m_d.i
                                : i < tmp->m_d.i;
                            ot->m_d.i = i;
                            break;
                        }
                        case T_DBL:
                        {
                            double step_i = step->m_d.d;
                            double i = ot->m_d.d;
                            if (update) i += step_i;
                            condt->m_type = T_BOOL;
                            condt->m_d.b =
                                step_i > 0.0
                                ? i > tmp->m_d.d
                                : i < tmp->m_d.d;
                            ot->m_d.d = i;
                            break;
                        }
                        default:
                        {
                            int64_t step_i = step->to_int();
                            int64_t i = ot->m_d.i;
                            if (update) i += step_i;
                            condt->m_type = T_BOOL;
                            condt->m_d.b =
                                step_i > 0
                                ? i > tmp->to_int()
                                : i < tmp->to_int();
                            ot->m_d.i = i;
                            break;
                        }
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

                case OP_EQV:
                {
                    E_SET_CHECK_REALLOC(O, O);

                    E_GET(tmp, A);
                    Atom &a = *tmp;
                    E_GET(tmp, B);
                    Atom &b = *tmp;

                    Atom o(T_BOOL);
                    o.m_d.b = a.eqv(b);
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
                    E_GET(tmp, A);
                    Atom &iter = *tmp;
                    if (iter.m_type != T_VEC && iter.m_d.vec->m_len < 2)
                        error("Bad iterator found in IKEY", iter);

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
                    else
                        error("Bad iterator found in IKEY", iter);
                    break;
                }

    #define     DEFINE_NUM_OP_BOOL(opname, oper)              \
                case OP_##opname:                             \
                {                                             \
                    E_SET_CHECK_REALLOC(O, O); \
                    Atom *a, *b; \
                    E_GET(a, A);                            \
                    E_GET(b, B);                            \
                                                              \
                    Atom o(T_BOOL);                           \
                    if (a->m_type == T_DBL)                    \
                        o.m_d.b = a->m_d.d    oper b->to_dbl(); \
                    else if (a->m_type == T_INT)               \
                        o.m_d.b = a->m_d.i    oper b->to_int(); \
                    else                                      \
                        o.m_d.b = a->to_int() oper b->to_int(); \
                    E_SET(O, o);                              \
                    break;                                    \
                }

    #define     DEFINE_NUM_OP_NUM(opname, oper, neutr)                   \
                case OP_##opname:                                        \
                {                                                        \
                    E_SET_CHECK_REALLOC(O, O);      \
                    Atom *a, *b; \
                    E_GET(a, A);                                       \
                    E_GET(b, B);                                       \
                                                                         \
                    Atom *ot;                                            \
                    E_SET_D_PTR(PE_O, P_O, ot);                          \
                    if (a->m_type == T_DBL)                               \
                    {                                                    \
                        ot->m_d.d = a->m_d.d    oper b->to_dbl();            \
                        ot->m_type = T_DBL;                                \
                    }                                                    \
                    else if (a->m_type == T_INT)                          \
                    {                                                    \
                        ot->m_d.i = a->m_d.i    oper b->to_int();            \
                        ot->m_type = T_INT;                                \
                    }                                                    \
                    else if (a->m_type == T_NIL)                          \
                    {                                                    \
                        if (b->m_type == T_DBL)                           \
                        {                                                \
                            ot->m_d.d = (double) neutr;                    \
                            ot->m_d.d = ((double) neutr) oper b->to_dbl();  \
                        }                                                \
                        else if (b->m_type == T_INT)                      \
                        {                                                \
                            ot->m_d.i = (int64_t) neutr;                    \
                            ot->m_d.i = ((int64_t) neutr) oper b->m_d.i;     \
                        }                                                \
                        else                                             \
                        {                                                \
                            ot->m_d.i = (int64_t) neutr;                   \
                            ot->m_d.i = ((int64_t) neutr) oper b->to_int(); \
                        }                                                \
                        ot->m_type = b->m_type;                             \
                    }                                                    \
                    else                                                 \
                    {                                                    \
                        ot->m_d.i = a->to_int() oper b->to_int();            \
                        ot->m_type = T_INT;                                \
                    }                                                    \
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

                case OP_NOP:
                    break;

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
    }
    catch (BukaLISPException &)
    {
        throw;
    }
    catch (std::exception &e)
    {
        throw
            add_stack_trace_error(
                BukaLISPException(
                    std::string("Exception in VM: ")
                    + e.what()));
    }

    cont_stack->pop();

//    cout << "STACKS: " << cont_stack->m_len << "; " << frm_stack->m_len << endl;

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
