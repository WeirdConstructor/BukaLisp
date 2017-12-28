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

std::string PROG::as_string(bool pretty)
{
    Atom a;
    to_atom(a);
    return a.to_write_str(pretty);
}
//---------------------------------------------------------------------------

Atom PROG::repack_expanded_userdata(GC &gc, Atom a, AtomMap *refmap)
{
    if (refmap->at(a).m_type != T_NIL)
        return refmap->at(a);

    if (a.m_type == T_VEC)
    {
        if (a.at(0).m_type == T_SYM)
        {
            auto first = a.at(0).m_d.sym->m_str;

            if (first == "BKL-VM-PROG")
                return PROG::create_prog_from_info(gc, a, refmap);
            else if (first == "BKL-META")
            {
                Atom a_meta = a.at(1);
                Atom a_data = a.at(2);

                if (a_meta.m_type == T_VEC)
                {
                    if (a_data.m_type == T_VEC)
                    {
                        Atom out(T_VEC, gc.allocate_vector(a_data.m_d.vec->m_len));
                        refmap->set(a, out);
                        Atom meta = repack_expanded_userdata(gc, a_meta, refmap);
                        out.m_d.vec->m_meta = meta.m_d.vec;
                        Atom data = repack_expanded_userdata(gc, a_data, refmap);

                        for (size_t i = 0; i < data.m_d.vec->m_len; i++)
                            out.m_d.vec->set(i, data.m_d.vec->m_data[i]);

                        return out;
                    }
                    else if (a_data.m_type = T_MAP)
                    {
                        Atom out(T_MAP, gc.allocate_map());
                        refmap->set(a, out);
                        Atom meta = repack_expanded_userdata(gc, a_meta, refmap);
                        out.m_d.map->m_meta = meta.m_d.vec;
                        Atom data = repack_expanded_userdata(gc, a_data, refmap);

                        ATOM_MAP_FOR(i, data.m_d.map)
                        {
                            out.m_d.map->set(MAP_ITER_KEY(i), MAP_ITER_VAL(i));
                        }

                        return out;
                    }
                    else
                        throw BukaLISPException(
                            "'bkl-make-vm-prog' bad BKL-META, "
                            "did not assign meta to a vector or map!");
                }
                else
                {
                    throw BukaLISPException(
                        "'bkl-make-vm-prog' bad BKL-META, "
                        "did not assign a vector!");
                }
            }
            else if (first == "BKL-CLOSURE")
            {
                Atom out(T_CLOS, gc.clone_vector(a.m_d.vec));
                refmap->set(a, out);

                for (size_t i = 0; i < out.m_d.vec->m_len; i++)
                {
                    out.m_d.vec->m_data[i] =
                        repack_expanded_userdata(gc, a.m_d.vec->m_data[i], refmap);
                }

                out.m_d.vec->shift(); // remove BKL-CLOSURE

                return out;
            }

            // vvv--- fall through to loop ---vvv
        }

        Atom out(T_VEC, gc.clone_vector(a.m_d.vec));
        refmap->set(a, out);

        for (size_t i = 0; i < out.m_d.vec->m_len; i++)
        {
            out.m_d.vec->m_data[i] =
                repack_expanded_userdata(gc, a.m_d.vec->m_data[i], refmap);
        }

        return out;
    }
    else if (a.m_type == T_MAP)
    {
        Atom out(T_MAP, gc.allocate_map());
        refmap->set(a, out);

        ATOM_MAP_FOR(i, a.m_d.map)
        {
            Atom key = repack_expanded_userdata(gc, MAP_ITER_KEY(i), refmap);
            Atom val = repack_expanded_userdata(gc, MAP_ITER_VAL(i), refmap);
            out.m_d.map->set(key, val);
        }

        return out;
    }

    return a;
}
//---------------------------------------------------------------------------

Atom PROG::create_prog_from_info(GC &gc, Atom prog_info, AtomMap *refmap)
{
    if (prog_info.m_type != T_VEC)
        throw BukaLISPException(
            "'bkl-make-vm-prog' can only operate on a list");

    if (prog_info.m_d.vec->m_len < 5)
        throw BukaLISPException(
            "'bkl-make-vm-prog' needs a list with at least 5 elements");

    Atom tag = prog_info.m_d.vec->m_data[0];
    if (   tag.m_type == T_VEC
        && tag.at(0).m_type == T_SYM
        && tag.at(0).m_d.sym->m_str == "BKL-VM-PROG")
        throw BukaLISPException(
            "'bkl-make-vm-prog' expected 'BKL-VM-PROG' tag as first element");

    Atom func_info = prog_info.m_d.vec->m_data[1];
    Atom data      = prog_info.m_d.vec->m_data[2];
    Atom prog      = prog_info.m_d.vec->m_data[3];
    Atom debug     = prog_info.m_d.vec->m_data[4];
    Atom root_regs = prog_info.m_d.vec->m_data[5];

//    std::cout << "PROG START " << func_info.to_display_str() << " RooT-ID:" << root_regs.id() << std::endl;

    if (data.m_type != T_VEC)
        throw BukaLISPException("'bkl-make-vm-prog' bad data element @2");

    if (prog.m_type != T_VEC)
        throw BukaLISPException("'bkl-make-vm-prog' bad code element @3");

    PROG *new_prog = new PROG(gc, data.m_d.vec->m_len, prog.m_d.vec->m_len);
    gc.reg_userdata(new_prog);

    Atom ret(T_UD);
    ret.m_d.ud = new_prog;
    if (refmap)
        refmap->set(prog_info, ret);

    if (refmap)
    {
        debug     = repack_expanded_userdata(gc, debug, refmap);
        root_regs = repack_expanded_userdata(gc, root_regs, refmap);
    }

    if (debug.m_type != T_MAP)
        throw BukaLISPException("'bkl-make-vm-prog' bad debug element @4");

    if (root_regs.m_type != T_VEC)
        throw BukaLISPException("'bkl-make-vm-prog' bad root-regs element @5");

    if (!root_regs.m_d.vec->m_meta)
        throw BukaLISPException("'bkl-make-vm-prog' root regs dont have meta");

    new_prog->set_root_env(root_regs.m_d.vec);
    new_prog->set_debug_info(debug);
    new_prog->m_function_info = func_info.to_display_str();

    if (refmap)
    {
        for (size_t i = 0; i < data.m_d.vec->m_len; i++)
        {
            Atom d =
                PROG::repack_expanded_userdata(
                    gc, data.m_d.vec->m_data[i], refmap);
            new_prog->m_atom_data.m_d.vec->set(i, d);
        }
    }
    else
    {
        for (size_t i = 0; i < data.m_d.vec->m_len; i++)
        {
            new_prog->m_atom_data.m_d.vec->set(i, data.m_d.vec->m_data[i]);
        }
    }

    for (size_t i = 0; i < prog.m_d.vec->m_len; i++)
    {
        INST instr;
        instr.from_atom(prog.m_d.vec->m_data[i]);
        new_prog->set(i, instr);
    }

    return ret;
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

static void dump_stack_trace(const std::string &title, AtomVec *cont_stack, PROG *cur_prog, INST *cur_pc)
{
    std::cout << "STACK TRACE(" << title << ")" << std::endl;

    if (cur_pc && cur_prog)
    {
        size_t pc_idx = cur_pc - &(cur_prog->m_instructions[0]);
        Atom a(T_INT, pc_idx);
        Atom info = cur_prog->m_debug_info_map.at(a);
        std::cout << "    [*] @" << pc_idx
                  << "/" << (cur_prog->m_instructions_len - 1)
                  << " " << cur_prog->m_function_info
                  << "{" << info.to_write_str() << "}" << std::endl;
    }

    for (size_t i = cont_stack->m_len; i > 0; i--)
    {
        std::cout << "    [" << i << "] ";

        Atom &frame = cont_stack->m_data[i - 1];
        if (frame.m_type != T_VEC)
        {
            std::cout << "? weird frame: " << frame.m_type << std::endl;
            continue;
        }

        AtomVec *f = frame.m_d.vec;

        switch (f->m_len)
        {
            case VM_CLNUP_FRAME_SIZE:
                std::cout << "CLEANUP " << std::endl;
                break;
            case VM_JUMP_FRAME_SIZE:
                std::cout << "JUMP FRAME " << f->m_data[VM_JMP_TAG].to_write_str() << std::endl;
                break;
            case VM_CALL_FRAME_SIZE:
            {
                Atom &proc = f->m_data[VM_CF_PROG];
                if (proc.m_type == T_UD && proc.m_d.ud != nullptr)
                {
                    PROG *prog = static_cast<PROG*>(proc.m_d.ud);
                    INST *pc   = (INST *) f->m_data[VM_CF_PC].m_d.ptr;
                    size_t pc_idx = pc - &(prog->m_instructions[0]);

                    Atom a(T_INT, pc_idx);
                    Atom info = prog->m_debug_info_map.at(a);

                    std::cout << "CALL FRAME @" << pc_idx
                              << "/" << (prog->m_instructions_len - 1)
                              << " "
                              << prog->m_function_info << "{" << info.to_write_str() << "}"
                              << std::endl;
                }
                else
                {
                    std::cout << "INITIAL FRAME" << std::endl;
                }
                break;
            }
            default:
                std::cout << "weird frame len=" << f->m_len << std::endl;
                break;
        }
    }
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

#define E_SET(reg, val)     E_SET_D(PE_##reg, P_##reg, (val))
#define E_GET(out_ptr, reg) E_SET_D_PTR(PE_##reg, P_##reg, out_ptr)

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

            switch (m_pc->op)
            {
                case OP_STACK_TRC:
                {
                    E_GET(tmp, A);
                    dump_stack_trace(tmp->to_display_str(), cont_stack, m_prog, m_pc);
                    break;
                }
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

                    if (P_A > 0)
                    {
                        Atom *first;
                        E_GET(first, B);
                        out_av->check_size(P_A - 1);
                        out_av->m_data[0] = *first;
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
                        E_SET_D_PTR(key_eidx->m_d.i, key_idx->m_d.i, key);
                        E_SET_D_PTR(val_eidx->m_d.i, val_idx->m_d.i, val);
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
                    if (prog.m_type != T_UD || prog.m_d.ud->type() != "BKL-VM-PROG")
                        error("VM can't make closure from non BKL-VM-PROG", prog);

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
                    cl.m_d.vec->m_data[VM_CLOS_PROG].set_ud(prog.m_d.ud);
                    cl.m_d.vec->m_data[VM_CLOS_UPV].set_vec(upvalues);
                    cl.m_d.vec->m_data[VM_CLOS_IS_CORO].set_bool(is_coroutine);
                    cl.m_d.vec->m_data[VM_CLOS_ARITY] = tmp->at(1);

                    E_SET(O, cl);
                    break;
                }

                case OP_NEW_UPV:
                {
                    E_SET_CHECK_REALLOC(A, A);
                    E_SET_CHECK_REALLOC(O, O);

                    E_GET(tmp, A);
                    Atom val = *tmp;
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
                    Atom *argv_tmp;
                    E_GET(func, A);
                    E_GET(argv_tmp, B);
                    if (argv_tmp->m_type != T_VEC)
                        error("Bad argument index vector!", *argv_tmp);
                    AtomVec *av    = argv_tmp->m_d.vec;
                    AtomVec *frame = nullptr;

                    if (PE_B == REG_ROW_DATA)
                    {
                        size_t len = av->m_len;
                        frame = m_rt->m_gc.allocate_vector(len);
                        frame->m_len = len >> 1;
                        for (size_t i = 0, j = 0; i < av->m_len; i += 2, j++)
                        {
                            E_SET_D_PTR(
                                av->m_data[i + 1].m_d.i,
                                av->m_data[i].m_d.i,
                                tmp);
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
                            try
                            {
                                (*func->m_d.func)(*(frame), *ot);
                                if (m_trace) cout << "CALL=> " << ot->to_write_str() << endl;
                            }
                            catch (VMRaise &r)
                            {
                                std::cout << "EXII" << std::endl;
                                // TODO: If we have an error object, we need to
                                //       attach current stack trace!
                                BukaLISPException e("raised obj");
                                e.set_error_obj(m_rt->m_gc, r.get_error_obj());
                                e.set_do_ctrl_jmp_preserve_obj();
                                throw e;
                            }
                            catch (BukaLISPException &e)
                            {
                                std::cout << "EX0" << e.what() << std::endl;
                                if (m_trace) cout << "CALL=>Exception!" << endl;
                                e.set_do_ctrl_jmp();
                                throw add_stack_trace_error(e);
                            }
                            break;
                        }
                        case T_CLOS:
                        {
                            if (func->m_d.vec->m_len < VM_CLOS_SIZE)
                                error("Bad closure found (size)", Atom(T_VEC, func->m_d.vec));
                            if (func->m_d.vec->m_data[VM_CLOS_PROG].m_type != T_UD)
                                error("Bad closure found (no PROG)", *func);

                            alloc = true;

                            Atom &arity = func->m_d.vec->m_data[VM_CLOS_ARITY];

                            // save the current execution context:
                            RECORD_CALL_FRAME(*func, call_frame);
                            cont_stack->push(call_frame);

                            if (func->m_d.vec->m_data[VM_CLOS_IS_CORO].m_type == T_VEC)
                            {
                                // XXX: Check if T_CLOS is a coroutine (!func->at(2).is_false())
                                //      Then instead of a new entry point in PROG, we restore
                                //      the former execution context by restoring the
                                //      call frames and the execution context
                                //      (m_prog, m_pc, frm_stack, output-register)
                                //      We also need to set the output-register to the
                                //      value of frame.at(0).

                                if (frame->m_len > 1)
                                    error("Wrong number of arguments to a "
                                          "yielded coroutine, expected 0 or 1, got",
                                          Atom(T_INT, frame->m_len));

                                Atom ret_val = frame->at(0);

                                AtomVec *coro_cont_stack =
                                    func->m_d.vec->m_data[VM_CLOS_IS_CORO].m_d.vec;
                                func->m_d.vec->m_data[VM_CLOS_IS_CORO].set_bool(true);

                                Atom restore_frame = *(coro_cont_stack->last());
                                coro_cont_stack->pop();
                                for (size_t i = coro_cont_stack->m_len; i > 0; i--)
                                    cont_stack->push(coro_cont_stack->m_data[i - 1]);

                                RESTORE_FROM_CALL_FRAME(restore_frame, ret_val);
                            }
                            else
                            {
                                if (CHECK_ARITY(arity, frame->m_len) != 0)
                                    report_arity_error(arity, frame->m_len);

    //                            std::cout << "ARITY" << func->m_d.vec->m_data[VM_CLOS_ARITY].to_write_str() << std::endl;
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

                                m_prog   = dynamic_cast<PROG*>(func->m_d.vec->m_data[VM_CLOS_PROG].m_d.ud);
                                m_pc     = &(m_prog->m_instructions[0]);
                                m_pc--;

                                SET_DATA_ROW(m_prog->m_data_vec);
                                AtomVec *upvs = func->m_d.vec->m_data[VM_CLOS_UPV].m_d.vec;
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
                                SET_ROOT_ENV(m_prog->m_root_regs);
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

                    AtomMap *eval_env = nullptr;
                    if (env->m_type != T_NIL)
                    {
                        if (env->m_type != T_MAP)
                            error("Bad environment (not a map) to 'eval'", *env);
                        eval_env = env->m_d.map;
                    }
                    else
                    {
                        // OP_EVAL is wrapped into a (lambda (code . env) ...)
                        // to inherit the root_env of the caller in case env is nil,
                        // we walk the control stack downwards until we find the
                        // original caller frame that was stored in OP_CALL.
                        //
                        // The (lambda (code . env) ...) is generated by the compiler
                        // using a special root environment, which is completely
                        // uninteresting to the actual user of (eval ...).
                        AtomVec *caller_env = nullptr;
                        for (size_t i = cont_stack->m_len; i > 0; i--)
                        {
                            AtomVec *frame = cont_stack->m_data[i - 1].m_d.vec;
                            if (frame->m_len == VM_CALL_FRAME_SIZE)
                            {
                                caller_env = frame->m_data[VM_CF_ROOT].m_d.vec;
                                break;
                            }
                        }

                        // Fallback is the current root env of the compiled PROG:
                        if (!caller_env)
                            caller_env = m_prog->m_root_regs;

                        if (!caller_env->m_meta
                            || caller_env->m_meta->at(1).m_type != T_MAP)
                        {
                            error("Bad environment passed in caller, root_env has no meta env attached",
                                  Atom(T_VEC, caller_env));
                        }

                        eval_env = caller_env->m_meta->at(1).m_d.map;
                    }

                    //d// std::cout << "EVAL ENV=" << ((void *) eval_env) << "(" << code->to_write_str() << ")" << std::endl;

                    GC_ROOT(m_rt->m_gc, prog) =
                        m_compiler_call(*code, eval_env, "<vm-op-eval>", true);
                    if (prog.m_type != T_UD || prog.m_d.ud->type() != "BKL-VM-PROG")
                        error("In 'eval' compiler did not return proper BKL-VM-PROG",
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

                    SET_DATA_ROW(m_prog->m_data_vec);
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
                    bool exec_cleanup = false;
                    Atom *c = cont_stack->last();
                    while (   c
                           && c->m_type == T_VEC
                           && c->m_d.vec->m_len != VM_CALL_FRAME_SIZE)
                    {
                        if (c->m_d.vec->m_len == VM_CLNUP_FRAME_SIZE)
                        {
                            JUMP_TO_CLEANUP(
                                exec_cleanup,
                                c->m_d.vec,
                                Atom(T_INT, VM_CLNUP_COND_RETURN),
                                Atom(),
                                ret_val);
                            break;
                        }

                        cont_stack->pop();
                        c = cont_stack->last();
                    }

                    if (exec_cleanup)
                        break;

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

                case OP_GET_CORO:
                {
                    Atom func;
                    size_t ret_cont_stack_idx = 0;
                    for (size_t i = cont_stack->m_len; i > 0; i--)
                    {
                        Atom &frm = cont_stack->m_data[i - 1];
                        if (   frm.m_type == T_VEC
                            && frm.m_d.vec->m_len == VM_CALL_FRAME_SIZE
                            && !frm.at(VM_CF_CLOS).at(VM_CLOS_IS_CORO).is_false())
                        {
                            func = frm.at(VM_CF_CLOS);
                            break;
                        }
                    }

                    E_SET_CHECK_REALLOC(O, O);
                    E_SET(O, func);
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
                        c = nullptr;
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
                        if (!frm.at(VM_CF_CLOS).at(VM_CLOS_IS_CORO).is_false())
                        {
                            //d// cout << "FOUND COROUTINE CALL AT DEPTH="
                            //d//      << i << " of " << cont_stack->m_len << endl;
                            ret_call_frame = frm;
                            func           = frm.at(VM_CF_CLOS);
                            ret_cont_stack_idx = i - 1;
                            break;
                        }

                        cont_copy->set((cont_stack->m_len - i), frm);
                    }

                    if (func.m_type == T_CLOS)
                    {
                        if (func.m_d.vec->at(VM_CLOS_IS_CORO).m_type != T_BOOL)
                            error("Can't yield from an already yielded "
                                  "coroutine. Recusive calling of a coroutine "
                                  "is not supported!", Atom());
                        cont_copy->push(coro_cont_frame);
                        func.m_d.vec->set(VM_CLOS_IS_CORO, Atom(T_VEC, cont_copy));
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
                    if (P_B < 0)
                        jmp_frame->m_data[VM_JMP_TAG].clear();
                    else
                    {
                        E_GET(tmp, B);
                        jmp_frame->m_data[VM_JMP_TAG] = *tmp;
                    }
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
                        error("Bad ctrl jmp frame on stack",
                              c ? *c : Atom());

                    cont_stack->pop();
                    break;
                }

                case OP_PUSH_CLNUP:
                {
                    AtomVec *clnup_frame =
                        m_rt->m_gc.allocate_vector(
                            VM_CLNUP_FRAME_SIZE);
                    clnup_frame->m_len = VM_CLNUP_FRAME_SIZE;
                    clnup_frame->m_data[VM_CLNUP_PC].set_ptr(m_pc + P_A);
                    clnup_frame->m_data[VM_CLNUP_COND_I].set_int(P_O);
                    clnup_frame->m_data[VM_CLNUP_COND_E].set_int(PE_O);
                    clnup_frame->m_data[VM_CLNUP_VAL_I].set_int(P_B);
                    clnup_frame->m_data[VM_CLNUP_VAL_E].set_int(PE_B);
                    cont_stack->push(Atom(T_VEC, clnup_frame));
                    break;
                }

                case OP_POP_CLNUP:
                {
                    Atom *c = cont_stack->last();
                    if (   !c
                        || c->m_type != T_VEC
                        || c->m_d.vec->m_len != VM_CLNUP_FRAME_SIZE)
                        error("Bad cleanup frame on stack",
                              c ? *c : Atom());

                    cont_stack->pop();
                    break;
                }

                case OP_CTRL_JMP:
                {
                    Atom raised_val;
                    if (PE_O == REG_ROW_SPECIAL)
                    {
                        raised_val = instant_ctrl_jmp_obj;
                    }
                    else
                    {
                        E_GET(tmp, O);
                        raised_val = *tmp;
                    }

                    Atom jmp_tag;
                    if (P_A >= 0)
                    {
                        E_GET(tmp, A);
                        jmp_tag = *tmp;
                    }

                    //d// std::cout << "****** CTRL JMP TAG@ " << jmp_tag.to_write_str() << std::endl;

                    bool exec_cleanup = false;
                    Atom *c = cont_stack->last();
                    while (c && (   c->m_type != T_VEC
                                 || c->m_d.vec->m_len != VM_JUMP_FRAME_SIZE
                                 || (   c->m_d.vec->m_len == VM_JUMP_FRAME_SIZE
                                     && !(c->m_d.vec->m_data[VM_JMP_TAG] == jmp_tag))))
                    {
                        if (c->m_d.vec->m_len == VM_CALL_FRAME_SIZE)
                        {
                            // Restore earlier call frames (return from routines)
                            Atom call_frame = *c;
                            Atom nil_val;
                            RESTORE_FROM_CALL_FRAME(call_frame, nil_val);
                        }
                        else if (c->m_d.vec->m_len == VM_CLNUP_FRAME_SIZE)
                        {
                            JUMP_TO_CLEANUP(
                                exec_cleanup,
                                c->m_d.vec,
                                Atom(T_INT, VM_CLNUP_COND_CTRL_JMP),
                                jmp_tag,
                                raised_val);
                            break;
                        }

                        cont_stack->pop();
                        c = cont_stack->last();
                    }

                    if (exec_cleanup)
                        break;

                    // Next frame is something else? No idea, this shouldn't
                    // happen actually. Either we end up in a cleanup
                    // frame or the exception is caught here in a proper
                    // jmp_frame!
                    if (   !c
                        || c->m_type == T_NIL
                        || (   c->m_type != T_VEC
                            && c->m_d.vec->m_len != VM_JUMP_FRAME_SIZE))
                        throw VMRaise(m_rt->m_gc, raised_val);

                    AtomVec *jmp_frame = c->m_d.vec;
                    if (   jmp_frame->at(VM_JMP_PC).m_type    != T_C_PTR
                        || !(jmp_frame->at(VM_JMP_TAG)        == jmp_tag)
                        || jmp_frame->at(VM_JMP_OUT_I).m_type != T_INT
                        || jmp_frame->at(VM_JMP_OUT_E).m_type != T_INT)
                        error("Bad exception handler/jump block frame on call stack", *c);

                    cont_stack->pop();

                    E_SET_CHECK_REALLOC_D(
                        jmp_frame->at(VM_JMP_OUT_E).m_d.i,
                        jmp_frame->at(VM_JMP_OUT_I).m_d.i);
                    E_SET_D(
                        jmp_frame->at(VM_JMP_OUT_E).m_d.i,
                        jmp_frame->at(VM_JMP_OUT_I).m_d.i,
                        raised_val);

                    m_pc = (INST *) jmp_frame->at(VM_JMP_PC).m_d.ptr;
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

                case OP_ISNIL:
                {
                    E_SET_CHECK_REALLOC(O, O);

                    E_GET(tmp, A);
                    Atom o(T_BOOL);
                    o.m_d.b = tmp->m_type == T_NIL;
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
                    iter.m_d.vec->m_len = 2;
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
                    if (iter.m_type != T_VEC || iter.m_d.vec->m_len < 2)
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
                    throw BukaLISPException("Unknown VM opcode: " + to_string(m_pc->op));
            }
            m_pc++;

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
            add_stack_trace_error(e);

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
