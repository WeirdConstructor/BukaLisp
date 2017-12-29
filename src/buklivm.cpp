// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "config.h"
#include "buklivm.h"
#include "atom_printer.h"
#include "atom_cpp_serializer.h"
#include "util.h"
#include <chrono>
#include <cmath>

#if USE_MODULES
#include "modules/bklisp_module_wrapper.h"
#endif

using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

#define P_A  ((m_pc->a))
#define P_B  ((m_pc->b))
#define P_C  ((m_pc->c))
#define P_O  ((m_pc->o))
#define PE_A ((m_pc->ae))
#define PE_B ((m_pc->be))
#define PE_C ((m_pc->ce))
#define PE_O ((m_pc->oe))

//---------------------------------------------------------------------------

#define SET_FRAME_ROW(row_ptr)  rr_frame = row_ptr;  rr_v_frame = rr_frame->m_data;
#define SET_DATA_ROW(row_ptr)   rr_data  = row_ptr;  rr_v_data  = rr_data->m_data;
#define SET_ROOT_ENV(vecptr)    root_env = (vecptr); rr_v_root  = root_env->m_data;

//---------------------------------------------------------------------------

#define E_SET_CHECK_REALLOC_D(env_idx, idx) do { \
    switch ((env_idx)) { \
        case REG_ROW_UPV: \
        case REG_ROW_FRAME: if (rr_frame->m_len <= (size_t) (idx)) { rr_frame->check_size((size_t) (idx)); rr_v_frame = rr_frame->m_data; } break; \
        case REG_ROW_ROOT:  if (root_env->m_len <= (size_t) (idx)) { root_env->check_size((size_t) (idx)); rr_v_root = root_env->m_data; } break; \
        case REG_ROW_DATA:  if (rr_data->m_len <= (size_t) (idx)) { rr_data->check_size((size_t) (idx)); rr_v_data = rr_data->m_data; } break; \
        case REG_ROW_RREF: \
        { \
            if (rr_data->m_len <= (size_t) (idx)) { rr_data->check_size((size_t) (idx)); rr_v_data = rr_data->m_data; } break; \
            size_t ridx   = (size_t) rr_v_root[(size_t) (idx)].m_d.vec->m_data[0].m_d.i; \
            AtomVec *dest = rr_v_root[(size_t) (idx)].m_d.vec->m_data[1].m_d.vec; \
            if (dest->m_len <= ridx) { dest->check_size(ridx); } \
            break; \
        } \
        default: \
            error("bad reg_row reference (" #env_idx ")", Atom(T_INT, (env_idx))); \
    }\
} while (0)
//---------------------------------------------------------------------------

#define E_SET_CHECK_REALLOC(env_idx_reg, idx_reg) \
    E_SET_CHECK_REALLOC_D(PE_##env_idx_reg, P_##idx_reg)
//---------------------------------------------------------------------------

#define E_SET_D_PTR(env_idx, idx, val_ptr) do { \
    switch ((env_idx)) { \
        case REG_ROW_FRAME: val_ptr = &(rr_v_frame[(idx)]); break; \
        case REG_ROW_DATA:  val_ptr = &(rr_v_data[(idx)]); break; \
        case REG_ROW_ROOT:  val_ptr = &(rr_v_root[(idx)]); break; \
        case REG_ROW_UPV: \
        { \
            val_ptr = &(rr_v_frame[(idx)]); \
            val_ptr = &(val_ptr->m_d.vec->m_data[0]); \
            break; \
        } \
        case REG_ROW_PRIM: \
        { \
            val_ptr = &(m_prim_table->m_data[(idx)]); \
            break; \
        } \
        case REG_ROW_RREF: \
        { \
            size_t ridx = (size_t) rr_v_root[(idx)].m_d.vec->m_data[0].m_d.i; \
            val_ptr     = &(rr_v_root[(idx)].m_d.vec->m_data[1].m_d.vec->m_data[ridx]); break; \
            break; \
        } \
    } \
} while (0)
//---------------------------------------------------------------------------

#define E_SET_D(env_idx, idx, val) do { \
    switch ((env_idx)) { \
        case REG_ROW_FRAME: rr_v_frame[(idx)] = (val); break; \
        case REG_ROW_DATA:  rr_v_data[(idx)]  = (val); break; \
        case REG_ROW_ROOT:  rr_v_root[(idx)]  = (val); break; \
        case REG_ROW_UPV:   rr_v_frame[(idx)].m_d.vec->m_data[0] = (val); break; \
        case REG_ROW_RREF: \
        { \
            size_t ridx = (size_t) rr_v_root[(idx)].m_d.vec->m_data[0].m_d.i; \
            rr_v_root[(idx)].m_d.vec->m_data[1].m_d.vec->m_data[ridx] = (val); break; \
            break; \
        } \
    } \
} while (0)
//---------------------------------------------------------------------------

#define E_SET(reg, val)     E_SET_D(PE_##reg, P_##reg, (val))
#define E_GET(out_ptr, reg) E_SET_D_PTR(PE_##reg, P_##reg, out_ptr)

//---------------------------------------------------------------------------

#define RESTORE_FROM_CALL_FRAME(call_frame, ret_val)  \
    do {                                              \
        Atom *cv = (call_frame).m_d.vec->m_data;      \
        Atom &proc = cv[VM_CF_PROG];                  \
        if (   proc.m_type != T_UD                    \
            || proc.m_d.ud == nullptr)                \
        {                                             \
            ret = (ret_val);                          \
            m_pc =                                    \
                &(m_prog->m_instructions[             \
                    m_prog->m_instructions_len - 2]); \
            break;                                    \
        }                                             \
        Atom &frame  = cv[VM_CF_FRAME];               \
        Atom &r_env  = cv[VM_CF_ROOT];                \
        Atom &pc     = cv[VM_CF_PC];                  \
        int32_t oidx = (int32_t) cv[VM_CF_OUT].m_d.hpair.idx; \
        int8_t  eidx = (int8_t)  cv[VM_CF_OUT].m_d.hpair.key; \
                                                      \
        m_prog   = static_cast<PROG*>(proc.m_d.ud);   \
        m_pc     = (INST *) pc.m_d.ptr;               \
        SET_DATA_ROW(m_prog->m_data_vec);             \
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
//---------------------------------------------------------------------------

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
        data[VM_CF_OUT].set_hpair(PE_O, P_O);           \
    } while(0)
//---------------------------------------------------------------------------

#define JUMP_TO_CLEANUP(exec_cleanup, cleanup_frame, cond_val, tag, ret_val) \
    do {                                                                \
        AtomVec *clnup_frm = (cleanup_frame);                           \
        E_SET_CHECK_REALLOC_D(                                          \
            (clnup_frm)->m_data[VM_CLNUP_COND_E].m_d.i,                 \
            (clnup_frm)->m_data[VM_CLNUP_COND_I].m_d.i + 1);            \
        E_SET_D(                                                        \
            (clnup_frm)->m_data[VM_CLNUP_COND_E].m_d.i,                 \
            (clnup_frm)->m_data[VM_CLNUP_COND_I].m_d.i,                 \
            (cond_val));                                                \
        E_SET_D(                                                        \
            (clnup_frm)->m_data[VM_CLNUP_COND_E].m_d.i,                 \
            (clnup_frm)->m_data[VM_CLNUP_COND_I].m_d.i + 1,             \
            (tag));                                                     \
        E_SET_CHECK_REALLOC_D(                                          \
            (clnup_frm)->m_data[VM_CLNUP_VAL_E].m_d.i,                  \
            (clnup_frm)->m_data[VM_CLNUP_VAL_I].m_d.i);                 \
        E_SET_D(                                                        \
            (clnup_frm)->m_data[VM_CLNUP_VAL_E].m_d.i,                  \
            (clnup_frm)->m_data[VM_CLNUP_VAL_I].m_d.i,                  \
            (ret_val));                                                 \
        m_pc = (INST *) (clnup_frm)->m_data[VM_CLNUP_PC].m_d.ptr;       \
        exec_cleanup = true;                                            \
        cont_stack->pop();                                              \
    } while(0)
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

#if USE_MODULES
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
        throw BukaLISPException(
                "Bad __INIT__ in module initialization of '"
                + m->module_name(this).m_d.sym->m_str
                + "'");
    }
}
#endif
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

#define VM_CLNUP_FRAME_SIZE 5
#define VM_CLNUP_PC     0
#define VM_CLNUP_COND_I 1
#define VM_CLNUP_COND_E 2
#define VM_CLNUP_VAL_I  3
#define VM_CLNUP_VAL_E  4

#define VM_CLNUP_COND_UNDEF    0
#define VM_CLNUP_COND_CTRL_JMP 1
#define VM_CLNUP_COND_RETURN   2

#define VM_JUMP_FRAME_SIZE 4
#define VM_JMP_PC    0
#define VM_JMP_TAG   1
#define VM_JMP_OUT_I 2
#define VM_JMP_OUT_E 3

#define VM_CALL_FRAME_SIZE 6
#define VM_CF_CLOS  0
#define VM_CF_PROG  1
#define VM_CF_FRAME 2
#define VM_CF_ROOT  3
#define VM_CF_PC    4
#define VM_CF_OUT   5

//---------------------------------------------------------------------------

// TODO: For later
//Atom dump_routine_state(
//    GC &gc,
//    PROG *cur_prog,
//    INST *cur_pc,
//    AtomVec *cont_stack,
//    AtomVec *root_env,
//    AtomVec *rr_frame,
//    AtomVec *rr_data)
//{
//    Atom    *rr_v_frame = nullptr;
//    Atom    *rr_v_data  = nullptr;
//    Atom    *rr_v_root  = nullptr;
//
//    SET_ROOT_ENV(root_env);
//    SET_FRAME_ROW(rr_frame);
//    SET_DATA_ROW(rr_data);
//
//    AtomVec *out = gc.allocate_vector(10);
//
//
//}
//---------------------------------------------------------------------------

void walk_stack(PROG *cur_prog, INST *cur_pc, AtomVec *cont_stack,
                std::function<void(const std::string &place,
                                   const std::string &file_name,
                                   size_t line,
                                   const std::string &func_name)> walk_func)
{
    if (!(cur_pc && cur_prog))
        return;

//            if (!m_prog)
//                return Atom();
//            INST *start_pc = &(m_prog->m_instructions[0]);
//            Atom a(T_INT, m_pc - start_pc);
//            return m_prog->m_debug_info_map.at(a);

    Atom info;
    std::string func_info = cur_prog->func_info_at(cur_pc, info);

    walk_func(
        "prog@"
        + PROG::pc2str(cur_prog, cur_pc),
        info.at(0).to_display_str(),
        (size_t) info.at(1).to_int(),
        func_info);

    PROG *last_prog = cur_prog;

    for (size_t i = cont_stack->m_len; i > 0; i--)
    {
        Atom &frame = cont_stack->m_data[i - 1];
        if (frame.m_type != T_VEC)
        {
            walk_func("prog@?", "?", 0, frame.to_write_str());
            continue;
        }

        AtomVec *f = frame.m_d.vec;

        switch (f->m_len)
        {
            case VM_CLNUP_FRAME_SIZE:
            {
                std::stringstream ss_out_info;
                ss_out_info
                    << "(C: "
                    << INST::regidx2string(
                         (int32_t) f->m_data[VM_CLNUP_COND_I].to_int(),
                         (int8_t)  f->m_data[VM_CLNUP_COND_E].to_int())
                    << " V: "
                    << INST::regidx2string(
                         (int32_t) f->m_data[VM_CLNUP_VAL_I].to_int(),
                         (int8_t)  f->m_data[VM_CLNUP_VAL_E].to_int())
                    << ")";

                INST *pc = (INST *) f->m_data[VM_CLNUP_PC].m_d.ptr;

                Atom info;
                std::string func_info = last_prog->func_info_at(pc, info);

                walk_func(
                    "cleanup@"
                    + PROG::pc2str(last_prog, pc),
                    info.at(0).to_display_str(),
                    (size_t) info.at(1).to_int(),
                    "(" + func_info + " " + ss_out_info.str() + ")");

                break;
            }
            case VM_JUMP_FRAME_SIZE:
            {
                std::stringstream ss_out_info;
                ss_out_info
                    << "(O: "
                    << INST::regidx2string(
                         (int32_t) f->m_data[VM_JMP_OUT_I].to_int(),
                         (int8_t)  f->m_data[VM_JMP_OUT_E].to_int())
                    << " T: "
                    << f->m_data[VM_JMP_TAG].to_write_str()
                    << ")";

                INST *pc = (INST *) f->m_data[VM_JMP_PC].m_d.ptr;

                Atom info;
                std::string func_info = last_prog->func_info_at(pc, info);
                walk_func("jump@" + PROG::pc2str(last_prog, pc),
                          info.at(0).to_display_str(),
                          (size_t) info.at(1).to_int(),
                          "(" + func_info + " " + ss_out_info.str() + ")");
                break;
            }
            case VM_CALL_FRAME_SIZE:
            {
                Atom &proc = f->m_data[VM_CF_PROG];
                if (proc.m_type == T_UD && proc.m_d.ud != nullptr)
                {
                    PROG *prog = static_cast<PROG*>(proc.m_d.ud);
                    INST *pc   = (INST *) f->m_data[VM_CF_PC].m_d.ptr;

                    Atom info;
                    std::string func_info = prog->func_info_at(pc, info);

                    walk_func(
                        "prog@"
                        + PROG::pc2str(
                            proc,
                            (INST *) f->m_data[VM_CF_PC].m_d.ptr),
                        info.at(0).to_display_str(),
                        (size_t) info.at(1).to_int(),
                        func_info);

                    last_prog = prog;
                }
                else
                {
                    walk_func("init", "?", 0, "");
                }
                break;
            }
            default:
                walk_func("unknown", "?", f->m_len, "");
                break;
        }
    }
}
//---------------------------------------------------------------------------

Atom dump_stack_trace(GC &gc, AtomVec *cont_stack, PROG *cur_prog, INST *cur_pc)
{
    AtomVec *frms = gc.allocate_vector(10);

    walk_stack(cur_prog, cur_pc, cont_stack,
        [&](const std::string &place,
            const std::string &file_name,
            size_t line,
            const std::string &func_name)
        {
            AtomVec *frm = gc.allocate_vector(4);
            frm->push(Atom(T_STR, gc.new_symbol(place)));
            frm->push(Atom(T_STR, gc.new_symbol(file_name)));
            frm->push(Atom(T_INT, line));
            frm->push(Atom(T_STR, gc.new_symbol(func_name)));
            frms->push(Atom(T_VEC, frm));
        });

    return Atom(T_VEC, frms);
}
//---------------------------------------------------------------------------

Atom VM::eval(Atom callable, AtomVec *args)
{
    using namespace std::chrono;

    INST instant_operation;
    Atom instant_ctrl_jmp_obj;

    AtomVec *root_env   = nullptr;
    AtomVec *rr_frame   = nullptr;
    AtomVec *rr_data    = nullptr;
    Atom    *rr_v_frame = nullptr;
    Atom    *rr_v_data  = nullptr;
    Atom    *rr_v_root  = nullptr;
    GC_ROOT(m_rt->m_gc, reg_rows_ref) =
        Atom(T_UD, new RegRowsReference(&rr_frame, &rr_data, &root_env));

    PROG *prog = nullptr;
    INST *pc   = nullptr;

    if (!args) args = m_rt->m_gc.allocate_vector(0);

    GC_ROOT_VEC(m_rt->m_gc, args_root)     = args;
    GC_ROOT_VEC(m_rt->m_gc, cont_stack)    = nullptr;
    GC_ROOT_VEC(m_rt->m_gc, empty_upv_row) = m_rt->m_gc.allocate_vector(0);
    AtomVec *clos_upvalues = empty_upv_row;

    if (callable.m_type == T_UD && callable.m_d.ud->type() == "BKL-VM-PROG")
    {
        prog      = dynamic_cast<PROG*>(callable.m_d.ud);
        pc        = &(prog->m_instructions[0]);

        SET_DATA_ROW(prog->m_data_vec);
        SET_FRAME_ROW(args);
        SET_ROOT_ENV(prog->m_root_regs);
    }
    else if (callable.m_type == T_CLOS)
    {
        if (   callable.m_d.vec->m_len == VM_CLOS_SIZE
            && callable.m_d.vec->m_data[0].m_type == T_UD)
        {
            instant_operation.op = OP_CALL;
            instant_operation.a  = 1;
            instant_operation.ae = REG_ROW_FRAME;
            instant_operation.b  = 2;
            instant_operation.be = REG_ROW_FRAME;
            instant_operation.o  = 0;
            instant_operation.oe = REG_ROW_FRAME;

            AtomVec *new_frame = m_rt->m_gc.allocate_vector(3);
            new_frame->set(1, callable);
            new_frame->set(2, Atom(T_VEC, args));

            prog = nullptr;
            pc   = &instant_operation;
            SET_FRAME_ROW(new_frame);
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
//        error("Bad input type to run-vm-prog, expected BKL-VM-PROG.", A0);
        cout << "VM-ERROR: Bad input type to run-vm-prog, expected BKL-VM-PROG or closure, got: "
             << callable.to_write_str() << endl;
        return Atom();
    }

    cout << "vm start" << endl;

    VMProgStateGuard psg(m_prog, m_pc, prog, pc);

    // XXX: The root-call-frame actually keeps alive the whole code-tree, including
    //      any callable sub-m_prog objects. Thus, we don't have to explicitly
    //      keep alive the m-prog or it's m_atom_data
    //
    //      Also it keeps alive the dummy rr_frame that is used to
    //      call a T_CLOS that was passed to VM::eval().
    cont_stack = m_rt->m_gc.allocate_vector(7);
    {
        AtomVec *call_frame = m_rt->m_gc.allocate_vector(VM_CALL_FRAME_SIZE);
        call_frame->set(VM_CF_CLOS, callable);
        call_frame->set(VM_CF_FRAME, Atom(T_VEC, rr_frame));
        call_frame->set(VM_CALL_FRAME_SIZE - 1, Atom());
        cont_stack->push(Atom(T_VEC, call_frame));
    }

//    cout << "VM PROG: " << callable.to_write_str() << endl;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    Atom static_nil_atom;
    Atom ret;
    Atom *tmp = nullptr;

    bool alloc = false;
    VM_START:
    try
    {
        while (m_pc->op != OP_END)
        {
            if (m_trace)
            {
                cout << "VMTRC FRMS(" << cont_stack->m_len << "): ";
                Atom pc_a = m_pc->to_atom(m_rt->m_gc);
                cout << pc_a.to_write_str();
                cout << " ROOT: (" << ((void *) root_env) << ")";
                cout << " {ENV= ";
                for (size_t i = 0; i < rr_frame->m_len; i++)
                {
                    Atom &a = rr_v_frame[i];
                    if (a.m_type == T_UD && a.m_d.ud->type() == "BKL-VM-PROG")
                        cout << i << "[#<prog>] ";
                    else if (a.m_type == T_MAP && a.size() > 10)
                        cout << i << "[map:" << ((void *) a.m_d.map) << "] ";
                    else if (a.m_type == T_VEC && a.size() > 10)
                        cout << i << "[vec:" << ((void *) a.m_d.vec) << "] ";
                    else
                        cout << i << "[" << a.to_write_str() << "] ";
                }
                cout << "} " << (m_prog ? m_prog->m_function_info : std::string()) << " "
                     << get_current_debug_info().to_write_str() << endl;
            }

#           include "ops.cpp"

            if (alloc)
            {
                m_rt->m_gc.collect_maybe();
                alloc = false;
            }
        }
    }
    catch (VMRaise &)
    {
        throw;
    }
    catch (BukaLISPException &e)
    {
        if (!e.has_stack_trace())
        {
            Atom stk_trc =
                dump_stack_trace(m_rt->m_gc, cont_stack, m_prog, m_pc);
            e.push(stk_trc);
        }

        if (e.do_ctrl_jump())
        {
            instant_operation.op = OP_CTRL_JMP;
            instant_operation.o  = 0;
            instant_operation.oe = REG_ROW_SPECIAL;

            if (e.preserve_error_obj())
                instant_ctrl_jmp_obj = e.get_error_obj();
            else
                instant_ctrl_jmp_obj = e.create_error_object(m_rt->m_gc);

            m_pc = &instant_operation;
            goto VM_START;
        }

        throw e;
    }
    catch (std::exception &e)
    {
        BukaLISPException ex(std::string("Exception in VM: ") + e.what());
        throw add_stack_trace_error(ex);
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
