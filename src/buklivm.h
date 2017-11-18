// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include "atom.h"
#include "atom_userdata.h"
#include "runtime.h"
#include <sstream>

//---------------------------------------------------------------------------

class BukaLISPModule;

#define VM_CLOS_SIZE 5
#define VM_CLOS_PROG     0
#define VM_CLOS_UPV      1
#define VM_CLOS_IS_CORO  2
#define VM_CLOS_ARITY    3
#define VM_CLOS_ROOT_ENV 4

//---------------------------------------------------------------------------

namespace bukalisp
{
//---------------------------------------------------------------------------

class VMRaise : public std::exception
{
    private:
        GC_ROOT_MEMBER(m_raised_obj);
        std::string         m_err;
    public:
        VMRaise(GC &gc, const Atom &a)
          : GC_ROOT_MEMBER_INITALIZE(gc, m_raised_obj)
        {
            m_raised_obj = a;
            m_err = "Raised error obj: " + m_raised_obj.to_write_str();
        }

        Atom &get_error_obj() { return m_raised_obj; }

        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~VMRaise() { }
};
//---------------------------------------------------------------------------

class VMException : public std::exception
{
    private:
        std::string m_err;
    public:
        VMException(const std::string &err) : m_err(err) { std::cout << "FOO" << err << std::endl; }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~VMException() { }
};
//---------------------------------------------------------------------------

//    X(PUSH_RNG_ENV,   8)
//    X(PUSH_VEC_ENV,   9)

//    X(LOAD_STATIC,    4) 

#define OP_CODE_DEF(X) \
    X(NOP,            0) /* ()                                     */ \
    X(MOV,            1) /*                                        */ \
    X(MOV_FROM,       2) /*                                        */ \
    X(MOV_TO,         3) /*                                        */ \
    X(LOAD_PRIM,      5) /*                                        */ \
    X(LOAD_NIL,       6) /*                                        */ \
    X(NEW_VEC,        7) /*                                        */ \
    X(NEW_MAP,        8) /*                                        */ \
    X(CSET_VEC,       9) /*                                        */ \
    X(NEW_UPV,       10) /* (O: upvalue-adr)                       */ \
    X(EVAL,          11) /*                                        */ \
    X(DUMP_ENV_STACK,13) /*                                        */ \
    X(CALL,          15) /*                                        */ \
    X(NEW_CLOSURE,   16) /* (O: closure A: VM-PROG B: is_coro?)    */ \
    X(BRNIF,         17) /*                                        */ \
    X(BRIF,          18) /*                                        */ \
    X(BR,            19) /*                                        */ \
    X(FORINC,        20) /* (O: (cond+1=iter) A: cur   B: end)     */ \
    X(SET,           21) /*                                        */ \
    X(GET,           22) /*                                        */ \
    X(ITER,          23) /*                                        */ \
    X(NEXT,          24) /*                                        */ \
    X(IKEY,          25) /*                                        */ \
    X(YIELD,         26) /* (O: cont-val A: ret-val)               */ \
    X(NEW_ARG_VEC,   27) /*                                        */ \
    X(PUSH_JMP,      28) /*                                        */ \
    X(POP_JMP,       29) /*                                        */ \
    X(CTRL_JMP,      30) /*                                        */ \
    X(PUSH_CLNUP,    31) /*                                        */ \
    X(POP_CLNUP,     32) /*                                        */ \
    X(GET_CORO,      33) /*                                        */ \
    X(ADD,          100) /*                                        */ \
    X(SUB,          101) /*                                        */ \
    X(MUL,          102) /*                                        */ \
    X(DIV,          103) /*                                        */ \
    X(MOD,          104) /*                                        */ \
    X(LT,           105) /*                                        */ \
    X(GT,           106) /*                                        */ \
    X(LE,           107) /*                                        */ \
    X(GE,           108) /*                                        */ \
    X(EQ,           109) /*                                        */ \
    X(NEQ,          110) /*                                        */ \
    X(NOT,          111) /*                                        */ \
    X(EQV,          112) /*                                        */ \
    X(ISNIL,        113) /*                                        */ \
    X(RETURN,       250) /*                                        */ \
    X(END,          254) /*                                        */

enum OPCODE : uint8_t
{
#define X(name, code)   OP_##name = code,
OP_CODE_DEF(X)
#undef X
    END_OPCODE
};

//---------------------------------------------------------------------------

struct INST
{
    uint8_t op;
    int32_t  o;
    int32_t  a;
    int32_t  b;
    int32_t  c;
    int8_t  oe;
    int8_t  ae;
    int8_t  be;
    int8_t  ce;
//    union {
//        double  d;
//        int64_t i;
//    } va;

    void to_string(std::ostream &ss)
    {
        std::string op_name;
#       define X(name, code)    case code: op_name = #name; break;
        switch (op) { OP_CODE_DEF(X) }
#       undef X
        while (op_name.size() < 14)
           op_name = " " + op_name;
        ss << "#" << op_name << ": ("
           << o << "/" << (int32_t) oe
           << ", [" << a << "/" << (int32_t) ae
           << ":" << b << "/" << (int32_t) be
           << ":" << c << "/" << (int32_t) ce
           << "]";
    }

    INST() { clear(); }
    void clear()
    {
        op = 0;
        o  = 0;
        oe = 0;
        a  = 0;
        b  = 0;
        c  = 0;
        ae = 0;
        be = 0;
        ce = 0;
//        va.i = 0;
    }
};
//---------------------------------------------------------------------------

class PROG : public UserData
{
    public:
        Atom     m_debug_info_map;
        Atom     m_atom_data;
        INST    *m_instructions;
        size_t   m_instructions_len;
        std::string m_function_info;

    public:
        PROG()
            : m_instructions(nullptr)
        {
//            std::cout << "*NEW PROG" << ((void *) this) << std::endl;
        }
        PROG(GC &gc, size_t atom_data_len, size_t instr_len)
        {
            m_atom_data.set_vec(gc.allocate_vector(atom_data_len));
            m_instructions_len = instr_len;
            m_instructions     = new INST[instr_len + 1];
            m_instructions[instr_len].op = OP_END;
//            std::cout << "NEW PROG" << ((void *) this) << ";; " << atom_data_len << " ;;" << std::endl;
        }

        AtomVec *data_array() const { return m_atom_data.m_d.vec; }

        void set_data_from(AtomVec *av)
        {
            m_atom_data.m_d.vec->m_len = 0;
            for (size_t i = 0; i < av->m_len; i++)
                m_atom_data.m_d.vec->set(i, av->m_data[i]);
        }

        void set_debug_info(Atom &a)
        {
            m_debug_info_map = a;
        }

        void set(size_t idx, const INST &i)
        {
            if (idx >= m_instructions_len)
                return;
            m_instructions[idx] = i;
        }

        virtual std::string type() { return "VM-PROG"; }
        virtual std::string as_string();

        virtual void mark(GC *gc, uint8_t clr)
        {
            UserData::mark(gc, clr);
            gc->mark_atom(m_atom_data);
            gc->mark_atom(m_debug_info_map);
        }

        virtual ~PROG()
        {
            if (m_instructions) delete[] m_instructions;
//            std::cout << "DEL PROG" << ((void *) this) << std::endl;
        }
};
//---------------------------------------------------------------------------

Atom make_prog(GC &gc, Atom prog_info);
//---------------------------------------------------------------------------

class VMProgStateGuard
{
    private:
        PROG        *m_old_prog;
        INST        *m_old_pc;

        PROG       *&m_prog_ref;
        INST       *&m_pc_ref;
    public:
        VMProgStateGuard(PROG *&prog, INST *&pc, PROG *new_prog, INST *new_pc)
            : m_prog_ref(prog), m_pc_ref(pc)
        {
            m_old_prog = m_prog_ref;
            m_old_pc   = m_pc_ref;
            prog       = new_prog;
            pc         = new_pc;
        }

        ~VMProgStateGuard()
        {
            m_prog_ref = m_old_prog;
            m_pc_ref   = m_old_pc;
        }
};
//---------------------------------------------------------------------------

class VM
{
    private:
        INST      *m_pc;
        PROG      *m_prog;
        VM        *m_vm;
        GC_ROOT_MEMBER_VEC(m_prim_table);
        GC_ROOT_MEMBER_VEC(m_prim_sym_table);
        GC_ROOT_MEMBER_MAP(m_modules);
        GC_ROOT_MEMBER_MAP(m_documentation);
        bool       m_trace;
        std::function<Atom(Atom func, AtomVec *args)> m_interpreter_call;
        typedef std::function<Atom(Atom prog, AtomMap *root_env, const std::string &input_name, bool only_compile)> compiler_func;
        compiler_func m_compiler_call;

    public:
        Runtime   *m_rt;

        VM(Runtime *rt)
            : m_rt(rt),
              m_pc(nullptr),
              m_prog(nullptr),
              m_vm(this),
              m_trace(false),
              GC_ROOT_MEMBER_INITALIZE_VEC(rt->m_gc, m_prim_table),
              GC_ROOT_MEMBER_INITALIZE_VEC(rt->m_gc, m_prim_sym_table),
              GC_ROOT_MEMBER_INITALIZE_MAP(rt->m_gc, m_modules),
              GC_ROOT_MEMBER_INITALIZE_MAP(rt->m_gc, m_documentation)
        {
            m_prim_table     = rt->m_gc.allocate_vector(0);
            m_prim_sym_table = rt->m_gc.allocate_vector(0);
            m_modules        = rt->m_gc.allocate_map();
            m_documentation  = rt->m_gc.allocate_map();

            init_prims();
        }

        AtomVec *get_primitive_symbol_table()
        { return m_prim_sym_table; }

        void set_interpreter_call(
            const std::function<Atom(Atom func, AtomVec *args)> &func)
        {
            m_interpreter_call = func;
        }

        void set_compiler_call(const compiler_func &func)
        {
            m_compiler_call = func;
        }

        void set_documentation(
            const Atom &sym,
            Atom &doc_string)
        {
            m_documentation->set(sym, doc_string);
        }

        void set_documentation(
            const std::string &func_name,
            const std::string &doc_string)
        {
            set_documentation(
                Atom(T_SYM, m_rt->m_gc.new_symbol(func_name)),
                Atom(T_STR, m_rt->m_gc.new_symbol(doc_string)));
        }

        Atom get_documentation() { return Atom(T_MAP, m_documentation); }

        AtomMap *loaded_modules();

        void load_module(BukaLISPModule *m);

        void set_trace(bool t) { m_trace = t; }

        void report_arity_error(Atom &arity, size_t argc);

        BukaLISPException &add_stack_trace_error(BukaLISPException &e)
        {
            Atom info = get_current_debug_info();
            return
                e.push("vm",
                       info.at(0).to_display_str(),
                       (size_t) info.at(1).to_int(),
                       info.at(2).to_display_str());
        }

        Atom get_current_debug_info()
        {
            if (!m_prog)
                return Atom();
            INST *start_pc = &(m_prog->m_instructions[0]);
            Atom a(T_INT, m_pc - start_pc);
            return m_prog->m_debug_info_map.at(a);
        }

        void error(const std::string &msg)
        {
            throw add_stack_trace_error(BukaLISPException(msg));
        }

        void error(const std::string &msg, Atom &err_atom)
        {
            error(msg + ", atom: " + err_atom.to_write_str());
        }

        void init_prims();

        void run_module_destructors()
        {
            ATOM_MAP_FOR(p, m_modules)
            {
                Atom mod_funcs = MAP_ITER_VAL(p);
                if (mod_funcs.m_type != T_MAP)
                {
                    throw VMException(
                            "On destruction: In module '"
                            + MAP_ITER_KEY(p).m_d.sym->m_str
                            + "': bad function map");
                }
                Atom destr_func =
                    mod_funcs.m_d.map->at(
                        Atom(T_SYM, m_rt->m_gc.new_symbol("__DESTROY__")));
                destr_func = destr_func.at(2);
                if (destr_func.m_type != T_PRIM && destr_func.m_type != T_NIL)
                {
                    throw VMException(
                            "On destruction: In module '"
                            + MAP_ITER_KEY(p).m_d.sym->m_str
                            + "': bad destroy function");
                }
                else if (destr_func.m_type == T_PRIM)
                {
                    GC_ROOT_VEC(m_rt->m_gc, args) = m_rt->m_gc.allocate_vector(0);
                    Atom ret;
                    (*destr_func.m_d.func)(*args, ret);
                }

                // Attention: Destruction of the primitive pointers
                //            exported from the module happens in the
                //            ~VM() destructor, because the primitive functions
                //            are put into the m_prim_table!
            }
        }

        virtual ~VM()
        {
            run_module_destructors();

            for (size_t i = 0; i < m_prim_table->m_len; i++)
            {
                if (m_prim_table->at(i).m_type == T_PRIM)
                    delete m_prim_table->at(i).m_d.func;
            }
        }

        Atom eval(Atom at_ud, AtomMap *root_env, AtomVec *args = nullptr);
};
//---------------------------------------------------------------------------

};

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
