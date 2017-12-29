// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include "atom.h"
#include "atom_userdata.h"
#include "runtime.h"
#include <sstream>
#include "vmprog.h"

//---------------------------------------------------------------------------

class BukaLISPModule;

#define VM_CLOS_SIZE     4
#define VM_CLOS_PROG     0
#define VM_CLOS_UPV      1
#define VM_CLOS_IS_CORO  2
#define VM_CLOS_ARITY    3

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
            const Atom &doc_string)
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

#       if USE_MODULES
            void load_module(BukaLISPModule *m);
#       endif

        void set_trace(bool t) { m_trace = t; }

        void report_arity_error(Atom &arity, size_t argc);

        // XXX FIXME TODO: This method needs to be as clever as the stack trace printer!
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
            BukaLISPException e(msg);
            throw add_stack_trace_error(e);
        }

        void error(const std::string &msg, const Atom &err_atom)
        {
            BukaLISPException e(msg);
            e.set_error_obj(m_rt->m_gc, err_atom);
            throw e;
        }

        void init_prims();

        void run_module_destructors()
        {
            ATOM_MAP_FOR(p, m_modules)
            {
                Atom mod_funcs = MAP_ITER_VAL(p);
                if (mod_funcs.m_type != T_MAP)
                {
                    throw BukaLISPException(
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
                    throw BukaLISPException(
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

        Atom eval(Atom at_ud, AtomVec *args = nullptr);
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
