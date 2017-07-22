#pragma once

#include "atom.h"
#include "runtime.h"
#include "atom_printer.h"

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
        lilvm::AtomVec *m_env_stack;
        lilvm::AtomVec *m_root_stack;
        std::string     m_debug_pos;
        std::vector<lilvm::Atom::PrimFunc *> m_primitives;

        bool            m_trace;

    public:
        Interpreter(Runtime *rt)
            : lilvm::ExternalGCRoot(&(rt->m_gc)), m_rt(rt), m_env_stack(nullptr),
              m_trace(false)
        {
            init();
        }

        virtual ~Interpreter()
        {
            for (auto p : m_primitives)
                delete p;
        }

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
            lilvm::Atom prog = m_rt->read(input_name, input);
            lilvm::AtomVecPush avp(m_root_stack, prog);
//            std::cerr << "EVAL(" << write_atom(prog) << std::endl;
            if (prog.m_type == lilvm::T_VEC)
                return eval_begin(prog, prog.m_d.vec, 0);
            else
                return eval(prog);
        }

        void error(const std::string &msg)
        {
            throw InterpreterException("(@" + m_debug_pos + "): " + msg);
        }
        void error(const std::string &msg, lilvm::Atom &err_atom)
        {
            error(msg + ", atom: " + write_atom(err_atom));
        }

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
};
//---------------------------------------------------------------------------

}
