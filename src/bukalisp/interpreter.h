#pragma once

#include "atom.h"
#include "runtime.h"
#include "atom_printer.h"
#include "buklivm.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

class InterpreterException : public std::exception
{
    private:
        std::string m_err;
    public:
        InterpreterException(const std::string &err) : m_err(err) { }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~InterpreterException() { }
};
//---------------------------------------------------------------------------

class Interpreter : public lilvm::ExternalGCRoot
{
    private:
        Runtime        *m_rt;
        VM             *m_vm;
        lilvm::AtomVec *m_env_stack;
        lilvm::AtomVec *m_root_stack;
        lilvm::AtomMap *m_debug_pos_map;
        std::string     m_debug_pos;
        std::vector<lilvm::Atom::PrimFunc *> m_primitives;

        bool            m_trace;

        void set_debug_pos(lilvm::Atom &a);

    public:
        Interpreter(Runtime *rt, VM *vm = nullptr)
            : lilvm::ExternalGCRoot(&(rt->m_gc)), m_rt(rt), m_env_stack(nullptr),
              m_trace(false), m_vm(vm), m_debug_pos_map(nullptr)
        {
            init();
        }

        virtual ~Interpreter()
        {
            for (auto p : m_primitives)
                delete p;
        }

        lilvm::AtomVec *root_stack() { return m_root_stack; }

        virtual size_t gc_root_count() { return 2; }
        virtual lilvm::AtomVec *gc_root_get(size_t i)
        {
            if (i == 0)      return m_root_stack;
            else if (i == 1) return m_env_stack;
            else             return nullptr;
        }

        void init();

        void set_trace(bool e) { m_trace = e; }

        lilvm::Atom lookup(lilvm::Sym *var, lilvm::AtomMap *&env)
        {
            env = nullptr;

            if (m_env_stack->m_len <= 0)
                return lilvm::Atom();

            size_t stk_idx1 = m_env_stack->m_len;

            while (stk_idx1 > 0)
            {
                lilvm::Atom *at_env = &(m_env_stack->m_data[--stk_idx1]);
                if (!at_env)
                    return lilvm::Atom();
                if (at_env->m_type != lilvm::T_MAP)
                    return lilvm::Atom();

                bool defined = false;
                lilvm::Atom ret = at_env->m_d.map->at(lilvm::Atom(lilvm::T_SYM, var), defined);
                if (defined)
                {
                    env = at_env->m_d.map;
                    return ret;
                }
            }

            return lilvm::Atom();
        }

        lilvm::Atom eval(const std::string &input_name, const std::string &input)
        {
            lilvm::Atom old_debug_info;
            if (m_debug_pos_map)
                old_debug_info = lilvm::Atom(lilvm::T_MAP, m_debug_pos_map);
            lilvm::AtomVecPush avpm(m_root_stack, old_debug_info);

            lilvm::Atom prog = m_rt->read(input_name, input, m_debug_pos_map);
            lilvm::AtomVecPush avpm2(
                m_root_stack, lilvm::Atom(lilvm::T_MAP, m_debug_pos_map));
            lilvm::AtomVecPush avp(m_root_stack, prog);
//            std::cerr << "EVAL(" << write_atom(prog) << std::endl;
            lilvm::Atom ret;
            try
            {
                if (prog.m_type == lilvm::T_VEC)
                    ret = eval_begin(prog, prog.m_d.vec, 0);
                else
                    ret = eval(prog);
            }
            catch (...)
            {
                m_debug_pos_map =
                    old_debug_info.m_type == lilvm::T_MAP
                    ? old_debug_info.m_d.map
                    : nullptr;
                throw;
            }

            m_debug_pos_map =
                old_debug_info.m_type == lilvm::T_MAP
                ? old_debug_info.m_d.map
                : nullptr;

            return ret;
        }

        void error(const std::string &msg)
        {
            throw InterpreterException("(@" + m_debug_pos + "): " + msg);
        }
        void error(const std::string &msg, lilvm::Atom &err_atom)
        {
            error(msg + ", atom: " + write_atom(err_atom));
        }

        lilvm::Atom call(lilvm::Atom func, lilvm::AtomVec *av, bool eval_args = false, size_t arg_offs = 0);
        lilvm::Atom eval(lilvm::Atom e);
        lilvm::Atom eval_begin(lilvm::Atom e, lilvm::AtomVec *av, size_t offs);
        lilvm::Atom eval_define(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_setM(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_let(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_lambda(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_if(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_when(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_unless(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_while(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_and(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_or(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_for(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_do_each(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_dot_call(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_meth_def(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_field_set(lilvm::Atom e, lilvm::AtomVec *av);
        lilvm::Atom eval_field_get(lilvm::Atom e, lilvm::AtomVec *av);
};
//---------------------------------------------------------------------------

}
