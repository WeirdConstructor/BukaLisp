// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "interpreter.h"
#include "buklivm.h"
#include "atom_cpp_serializer.h"
#include <modules/bklisp_module_wrapper.h>
#include <chrono>
#include "util.h"

using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

AtomMap *Interpreter::init_root_env()
{
    GC_ROOT_MAP(m_rt->m_gc, root_env) = m_rt->m_gc.allocate_map();

    Atom tmp;
    Atom key;

#define DEF_SYNTAX(name) \
    key = Atom(T_SYM, m_rt->m_gc.new_symbol(#name)); \
    tmp = Atom(T_SYNTAX); \
    tmp.m_d.sym = m_rt->m_gc.new_symbol(#name); \
    m_rt->m_gc.add_permanent(tmp.m_d.sym); \
    root_env->set(key, tmp);

    DEF_SYNTAX(begin);
    DEF_SYNTAX(define);
    DEF_SYNTAX(set!);
    DEF_SYNTAX(quote);
    DEF_SYNTAX(let);
    DEF_SYNTAX(lambda);
    DEF_SYNTAX(if);
    DEF_SYNTAX(when);
    DEF_SYNTAX(unless);
    DEF_SYNTAX(while);
    DEF_SYNTAX(and);
    DEF_SYNTAX(or);
    DEF_SYNTAX(for);
    DEF_SYNTAX(do-each);
    DEF_SYNTAX(case);
    DEF_SYNTAX(.);
    DEF_SYNTAX($define!);
    DEF_SYNTAX($!);
    DEF_SYNTAX($);
    DEF_SYNTAX(displayln-time);
    DEF_SYNTAX(include);

#define START_PRIM() \
    tmp = Atom(T_PRIM); \
    tmp.m_d.func = new Atom::PrimFunc; \
    m_primitives.push_back(tmp.m_d.func); \
    (*tmp.m_d.func) = [this](AtomVec &args, Atom &out) {

#define END_PRIM(name) \
    }; \
    m_prim_table->push(Atom(T_SYM, m_rt->m_gc.new_symbol(#name))); \
    root_env->set(Atom(T_SYM, m_rt->m_gc.new_symbol(#name)), tmp);

#define IN_INTERPRETER 1
    #include "primitives.cpp"
#undef IN_INTERPRETER

    return root_env;
}
//---------------------------------------------------------------------------

void Interpreter::init()
{
    m_env_stack  = m_rt->m_gc.allocate_vector(0);
    m_prim_table = m_rt->m_gc.allocate_vector(0);
    m_call_stack = m_rt->m_gc.allocate_vector(100);

    m_env_stack->push(Atom(T_MAP, init_root_env()));

    if (m_vm)
    {
        m_modules = m_vm->loaded_modules();

        m_vm->set_interpreter_call([=](Atom func, AtomVec *args)
        {
            GC_ROOT(m_rt->m_gc,     func_r) = func;
            GC_ROOT_VEC(m_rt->m_gc, args_r) = args;
            return this->call(func, args, false, 0);
        });

        m_vm->set_compiler_call([=](Atom prog,
                                    AtomVec *root_env,
                                    const std::string &input_name,
                                    bool only_compile)
        {
            GC_ROOT(m_rt->m_gc,     prog_r)           = prog;
            GC_ROOT_MAP(m_rt->m_gc, debug_info_map_r) = debug_info_map_r;
            GC_ROOT_VEC(m_rt->m_gc, root_env_r)       = root_env;
            return this->call_compiler(prog, root_env,
                                       input_name, only_compile);
        });
    }
}
//---------------------------------------------------------------------------

void Interpreter::print_primitive_table()
{
    AtomMap *m = m_rt->m_gc.allocate_map();
    for (size_t i = 0; i < m_prim_table->m_len; i++)
    {
        m->set(m_prim_table->m_data[i], Atom(T_INT, i));
    }
    cout << Atom(T_MAP, m).to_write_str() << endl;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_begin(Atom e, AtomVec *av, size_t offs)
{
    Atom l;
    for (size_t i = offs; i < av->m_len; i++)
        l = eval(av->m_data[i]);
    return l;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_define(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
        error("'define' does not contain enough arguments", e);

    Atom sym = av->m_data[1];
    if (sym.m_type != T_SYM)
        error(
            "first argument of 'define' needs to be a symbol",
            sym);

    AtomMap *am = m_env_stack->last()->m_d.map;
    e = eval(av->m_data[2]);
    am->set(sym, e);
    return e;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_setM(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
        error("'set!' does not contain enough arguments", e);

    Atom sym = av->m_data[1];
    if (sym.m_type != T_SYM)
        error(
            "first argument of 'set!' needs to be a symbol",
            sym);

    AtomMap *env = nullptr;
    lookup(sym.m_d.sym, env);
    if (!env)
        error("'set!' access to undefined variable", sym);

    e = eval(av->m_data[2]);
    env->set(sym, e);
    return e;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_let(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'let' does not contain enough arguments", e);

    if (av->m_data[1].m_type != T_VEC)
        error("First argument for 'let' needs to be a list", e);

    AtomVec *binds = av->m_data[1].m_d.vec;

    AtomMap *env_map = m_rt->m_gc.allocate_map();
    AtomVecPush avpe(m_env_stack, Atom(T_MAP, env_map));

    for (size_t i = 0; i < binds->m_len; i++)
    {
        if (binds->m_data[i].m_type != T_VEC)
            error("Binding specification in 'let' is not a list", binds->m_data[i]);

        AtomVec *bind_spec = binds->m_data[i].m_d.vec;

        if (bind_spec->m_len != 2)
            error("Binding specification does not contain 2 elements", binds->m_data[i]);

        if (bind_spec->m_data[0].m_type != T_SYM)
            error("First element in binding specification is not a symbol", binds->m_data[i]);

        env_map->set(
            bind_spec->m_data[0],
            eval(bind_spec->m_data[1]));
    }

    Atom ret;
    for (size_t i = 2; i < av->m_len; i++)
        ret = eval(av->m_data[i]);

    return ret;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_lambda(Atom e, AtomVec *av)
{
    // closure saves copy/clone of m_env_stack
    // code in lambda block is stored in the closure, evaluated when called
    if (av->m_len < 3)
        error("'lambda' does not contain enough elements", e);

    if (av->m_data[1].m_type != T_VEC)
        error("Argument binding is not a list in 'lambda'", e);

    AtomVec *clos_env = m_rt->m_gc.clone_vector(m_env_stack);
    AtomVec *closure  = m_rt->m_gc.allocate_vector(3);
    closure->push(Atom(T_VEC, clos_env));
    closure->push(Atom(T_VEC, av));
    return Atom(T_CLOS, closure);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_if(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
        error("'if' does not contain enough arguments", e);
    else if (av->m_len > 4)
        error("'if' does contain too many arguments", e);

    Atom cond_res = eval(av->m_data[1]);
    if (cond_res.is_false())
    {
        if (av->m_len == 4) return eval(av->m_data[3]);
        else                return Atom();
    }

    return eval(av->m_data[2]);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_when(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'when' does not contain enough arguments", e);

    Atom cond_res = eval(av->m_data[1]);
    if (cond_res.is_false())
        return Atom();
    return eval_begin(e, av, 2);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_unless(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'unless' does not contain enough arguments", e);

    Atom cond_res = eval(av->m_data[1]);
    if (!cond_res.is_false())
        return Atom();
    return eval_begin(e, av, 2);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_while(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'while' does not contain enough arguments", e);

    GC_ROOT(m_rt->m_gc, last) = Atom();

    Atom while_cond = eval(av->m_data[1]);
    bool run = !while_cond.is_false();
    while (run)
    {
        last = eval_begin(e, av, 2);
        while_cond = eval(av->m_data[1]);
        run = !while_cond.is_false();
    }

    return last;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_and(Atom e, AtomVec *av)
{
    if (av->m_len <= 0)
        return Atom();

    Atom last;
    for (size_t i = 0; i < av->m_len; i++)
    {
        last = eval(av->m_data[i]);
        if (last.is_false())
            return last;
    }
    return last;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_or(Atom e, AtomVec *av)
{
    if (av->m_len <= 1)
        return Atom();

    Atom last;
    for (size_t i = 1; i < av->m_len; i++)
    {
        last = eval(av->m_data[i]);
        if (!last.is_false())
            return last;
    }
    return last;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_for(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'for' does not contain enough arguments", e);

    if (av->m_data[1].m_type != T_VEC)
        error("'for' first argument needs to be a list", e);

    AtomVec *cnt_spec = av->m_data[1].m_d.vec;
    if (cnt_spec->m_len < 3)
        error("'for' count specification needs to contain at least a symbol, start and end value", e);
    else if (cnt_spec->m_len > 4)
        error("'for' count specification contains too many elements", e);
    if (cnt_spec->m_data[0].m_type != T_SYM)
        error("'for' first element of count specification needs to be a symbol", e);


    AtomMap *env_map = m_rt->m_gc.allocate_map();
    AtomVecPush avpe(m_env_stack, Atom(T_MAP, env_map));

    Atom at_i = eval(cnt_spec->m_data[1]);

    Atom at_end = eval(cnt_spec->m_data[2]);
    int64_t step = cnt_spec->m_len > 3 ? eval(cnt_spec->m_data[3]).to_int() : 1;
    int64_t end  = at_end.to_int();
    int64_t i    = at_i.to_int();

    at_i.m_type = T_INT;
    at_i.m_d.i  = i;
    env_map->set(cnt_spec->m_data[0], at_i);

    Atom last;
    if (end >= i)
    {
        while (i <= end)
        {
            last = eval_begin(e, av, 2);

            i += step;
            at_i.m_d.i = i;
            env_map->set(cnt_spec->m_data[0], at_i);
        }
    }
    else
    {
        while (i >= end)
        {
            last = eval_begin(e, av, 2);

            i += step;
            at_i.m_d.i = i;
            env_map->set(cnt_spec->m_data[0], at_i);
        }
    }

    return last;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_do_each(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'do-each' does not contain enough arguments", e);

    if (av->m_data[1].m_type != T_VEC)
        error("'do-each' first argument needs to be a list", e);

    AtomVec *bnd_spec = av->m_data[1].m_d.vec;
    if (bnd_spec->m_len < 2)
        error("'do-each' bind specification needs to contain at least a key symbol and an expression", e);
    else if (bnd_spec->m_len > 3)
        error("'do-each' bind specification contains too many elements", e);
    if (bnd_spec->m_data[0].m_type != T_SYM)
        error("'do-each' first element of bind specification needs to be a symbol", e);

    AtomMap *env_map = m_rt->m_gc.allocate_map();
    AtomVecPush avpe(m_env_stack, Atom(T_MAP, env_map));


    Atom last;
    if (bnd_spec->m_len == 3)
    {
        if (bnd_spec->m_data[1].m_type != T_SYM)
            error("'do-each' second element of bind specification needs to be a symbol", e);

        GC_ROOT(m_rt->m_gc, ds) = eval(bnd_spec->m_data[2]);

        if (ds.m_type == T_VEC)
        {
            for (size_t i = 0; i < ds.m_d.vec->m_len; i++)
            {
                Atom iat(T_INT);
                iat.m_d.i = i;
                env_map->set(bnd_spec->m_data[0], iat);
                env_map->set(bnd_spec->m_data[1], ds.m_d.vec->m_data[i]);
                last = eval_begin(e, av, 2);
            }
        }
        else if (ds.m_type == T_MAP)
        {
            for (auto &p : ds.m_d.map->m_map)
            {
                Atom key_val = p.first;
                env_map->set(bnd_spec->m_data[0], key_val);
                env_map->set(bnd_spec->m_data[1], p.second);
                last = eval_begin(e, av, 2);
            }
        }
        else
        {
            env_map->set(bnd_spec->m_data[0], Atom());
            env_map->set(bnd_spec->m_data[1], ds);
            last = eval_begin(e, av, 2);
        }
    }
    else
    {
        GC_ROOT(m_rt->m_gc, ds) = eval(bnd_spec->m_data[1]);

        if (ds.m_type == T_VEC)
        {
            for (size_t i = 0; i < ds.m_d.vec->m_len; i++)
            {
                env_map->set(bnd_spec->m_data[0], ds.m_d.vec->m_data[i]);
                last = eval_begin(e, av, 2);
            }
        }
        else if (ds.m_type == T_MAP)
        {
            for (auto &p : ds.m_d.map->m_map)
            {
                env_map->set(bnd_spec->m_data[0], p.second);
                last = eval_begin(e, av, 2);
            }
        }
        else
        {
            env_map->set(bnd_spec->m_data[0], ds);
            last = eval_begin(e, av, 2);
        }
    }

    return last;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_case(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
    {
        error("'case' needs at least one value expression and "
              "a comparsion branch", e);
    }

    GC_ROOT(m_rt->m_gc, val) = eval(av->m_data[1]);

    Atom ret;
    size_t case_idx = 2;

    while (case_idx < av->m_len)
    {
        if (av->m_data[case_idx].m_type != T_VEC)
            error("'case' test condition is not a list", av->m_data[case_idx]);

        AtomVec *cc = av->m_data[case_idx].m_d.vec;
        if (cc->m_len < 2)
            error("'case' test condition too short", Atom(T_VEC, cc));

        if (cc->m_data[0].m_type == T_SYM
            && cc->m_data[0].m_d.sym->m_str == "else")
        {
            return eval_begin(e, cc, 1);
        }

        if (cc->m_data[0].m_type != T_VEC)
            error("'case' test condition starts not with a list", Atom(T_VEC, cc));

        AtomVec *test = cc->m_data[0].m_d.vec;
        if (test->m_len < 1)
            error("'case' test condition too short", Atom(T_VEC, cc));

        for (size_t i = 0; i < test->m_len; i++)
        {
            if (test->m_data[i].eqv(val))
            {
                return eval_begin(e, cc, 1);
            }
        }

        case_idx++;
    }

    return ret;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_field_get(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
    {
        error("'$' map field needs at least 2 arguments: "
              "the key and the map", e);
    }

    GC_ROOT(m_rt->m_gc, key) = av->m_data[1];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);
    GC_ROOT(m_rt->m_gc, obj) = eval(av->m_data[2]);

    if (obj.m_type != T_MAP)
        error("Can't set key on non map", obj);

    return obj.at(key);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_field_set(Atom e, AtomVec *av)
{
    if (av->m_len < 4)
    {
        error("'$!' map field set needs at least 3 arguments: "
              "the key, the map and the value", e);
    }

    GC_ROOT(m_rt->m_gc, key) = av->m_data[1];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);
    GC_ROOT(m_rt->m_gc, obj) = eval(av->m_data[2]);

    if (obj.m_type != T_MAP)
        error("Can't set key on non map", obj);

    Atom val = eval(av->m_data[3]);
    obj.m_d.map->set(key, val);

    return val;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_meth_def(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
    {
        error("'$define!' method definition needs at least 2 arguments: "
              "the object and the argument binding definition with the "
              "method name as first argument", e);
    }

    GC_ROOT(m_rt->m_gc, obj) = eval(av->m_data[1]);

    if (obj.m_type != T_MAP)
        error("Can't define method on non map atom", obj);

    if (av->m_data[2].m_type != T_VEC)
        error("Argument binding definition is not a list", av->m_data[2]);

    AtomVec *arg_bind_def = av->m_data[2].m_d.vec;

    GC_ROOT(m_rt->m_gc, key) = arg_bind_def->m_data[0];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);

    AtomVec *arg_def_av = m_rt->m_gc.allocate_vector(arg_bind_def->m_len - 1);
    for (size_t i = 1; i < arg_bind_def->m_len; i++)
    {
        Atom bind_param = arg_bind_def->m_data[i];
        if (bind_param.m_type != T_SYM)
            error("Argument binding parameter name must be a symbol", bind_param);
        arg_def_av->set(i - 1, bind_param);
    }

    AtomVec *lambda_av = m_rt->m_gc.allocate_vector(2 + (av->m_len - 3));
    lambda_av->set(1, Atom(T_VEC, arg_def_av));
    for (size_t i = 3; i < av->m_len; i++)
        lambda_av->set(i - 1, av->m_data[i]);

    GC_ROOT(m_rt->m_gc, lambda) = Atom(T_VEC, lambda_av);

    lambda = eval_lambda(lambda, lambda_av);

    obj.m_d.map->set(key, lambda);
    return obj;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_dot_call(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
    {
        error("'.' call operator requires at least 2 arguments: "
              "the method name and the object", e);
    }

    GC_ROOT(m_rt->m_gc, key) = av->m_data[1];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);
    GC_ROOT(m_rt->m_gc, obj) = eval(av->m_data[2]);

    GC_ROOT(m_rt->m_gc, method) = obj.at(key);
    if (   method.m_type != T_PRIM
        && method.m_type != T_CLOS
        && method.m_type != T_UD)
    {
        error("'.' method call of '"
              + key.to_write_str()
              + "' resolves to non callable value", method);
    }

    return call(method, av, true, 2);
}
//---------------------------------------------------------------------------

Atom Interpreter::call(Atom func, AtomVec *av, bool eval_args, size_t arg_offs)
{
    Atom ret;

    if (eval_args)
    {
        GC_ROOT_VEC(m_rt->m_gc, ev_av) = m_rt->m_gc.allocate_vector(av->m_len - 1);

        for (size_t i = 1 + arg_offs; i < av->m_len; i++)
        {
            ev_av->set(i - (1 + arg_offs), eval(av->m_data[i]));
        }

        av = ev_av;
    }

    GC_ROOT(m_rt->m_gc, args_r) = Atom(T_VEC, av);
    GC_ROOT(m_rt->m_gc, func_r) = func;

    if (func.m_type == T_PRIM)
    {
        (*func.m_d.func)(*av, ret);
    }
    else if (func.m_type == T_CLOS)
    {
        // Call of closure:
        // store current m_env_stack, continuation is stored on the C stack.
        // restore m_env_stack by letting it point to the vector in the closure
        // push environment
        // evaluate the stored code and later return the returned value
        // upon exit: pop-environment for cleanup!
        // restore old m_env_stack
        Atom env         = func.m_d.vec->m_data[0];
        Atom lambda_form = func.m_d.vec->m_data[1];
        Atom debug_pos   = func.m_d.vec->m_data[2];

        GC_ROOT_VEC(m_rt->m_gc, old_env) = m_env_stack;

        m_env_stack = env.m_d.vec;

        {
            AtomMap *am_bind_env = m_rt->m_gc.allocate_map();
            AtomVecPush avp_env(m_env_stack, Atom(T_MAP, am_bind_env));

            AtomVec *binds = lambda_form.m_d.vec->m_data[1].m_d.vec;

            // TODO: Refactor for (apply ...)?
            for (size_t i = 0; i < binds->m_len; i++)
            {
                if (binds->m_data[i].m_type != T_SYM)
                    error("Atom in binding list is not a symbol",
                          lambda_form.m_d.vec->m_data[1]);

                if (i < av->m_len)
                    am_bind_env->set(
                        binds->m_data[i].m_d.sym,
                        av->m_data[i]);
                else
                    am_bind_env->set(
                        binds->m_data[i].m_d.sym,
                        Atom(T_NIL));
            }

            ret = eval_begin(lambda_form, lambda_form.m_d.vec, 2);
        }
        m_env_stack = old_env;
    }
    else if (m_vm && func == T_UD && func.m_d.ud->type() == "VM-PROG")
    {
        ret = m_vm->eval(func, av);
    }
    else
        error("Non callable function element in list", func);

    return ret;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_include(Atom e, AtomVec *av)
{
    if (av->m_len == 1)
        error("'include' needs exactly one argument", e);

    if (   av->m_data[1].m_type != T_STR
        && av->m_data[1].m_type != T_KW
        && av->m_data[1].m_type != T_SYM)
    {
        error("'include' first argument needs to be either a string, "
              "symbol or keyword.", e);
    }

    std::string libpath  = ".\\bukalisplib\\";
    std::string filepath = libpath + av->m_data[1].m_d.sym->m_str + ".bkl";

    std::string code = slurp_str(filepath);
    return eval(filepath, code);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval(Atom e, AtomMap *env)
{
    GC_ROOT_VEC(m_rt->m_gc, old_env) = m_env_stack;

    m_env_stack = m_rt->m_gc.allocate_vector(1);
    m_env_stack->push(Atom(T_MAP, env));

    Atom ret;
    try
    {
        ret = eval(e);
        m_env_stack = old_env;
    }
    catch (std::exception &)
    {
        m_env_stack = old_env;
        throw;
    }
    return ret;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval(Atom e)
{
    // TODO: Add eval flag
    if (m_trace)
        cout << ">> eval: " << write_atom(e) << endl;

    GC_ROOT(m_rt->m_gc, e_r) = e;

    GC_ROOT(m_rt->m_gc, ret) = Atom();

    switch (e.m_type)
    {
        case T_NIL:  ret = Atom(); break;
        case T_INT:  ret = e; break;
        case T_DBL:  ret = e; break;
        case T_BOOL: ret = e; break;
        case T_STR:  ret = e; break;
        case T_KW:   ret = e; break;
        case T_PRIM: ret = e; break;
        case T_CLOS: ret = e; break;

        case T_MAP:
        {
            AtomVecPush call_frame_r(m_call_stack, e);

            AtomMap *nm = m_rt->m_gc.allocate_map();
            ret = Atom(T_MAP, nm);

            GC_ROOT(m_rt->m_gc, key) = Atom();
            for (auto el : e.m_d.map->m_map)
            {
                key = eval(el.first);
                Atom val = eval(el.second);
                nm->set(key, val);
            }
            break;
        }

        case T_SYM:
        {
            AtomMap *env = nullptr;
            ret = lookup(e.m_d.sym, env);
            annotate_meta_func(ret, e);
            if (!env)
                error("Undefined variable binding", e);
            break;
        }

        case T_VEC:
        {
            AtomVecPush call_frame_r(m_call_stack, e);

            AtomVec *av = e.m_d.vec;

            if (av->m_len <= 0)
                error("Can't evaluate empty list of args", e);

            GC_ROOT(m_rt->m_gc, first) = eval(av->m_data[0]);

            if (first.m_type == T_SYNTAX)
            {
                annotate_meta_func(e, first);

                // TODO: optimize by caching symbol pointers:
                std::string s = first.m_d.sym->m_str;
                if      (s == "begin")    ret = eval_begin(e, av, 1);
                else if (s == "define")   ret = eval_define(e, av);
                else if (s == "set!")     ret = eval_setM(e, av);
                else if (s == "let")      ret = eval_let(e, av);
                else if (s == "quote")    ret = av->m_data[1];
                else if (s == "lambda")   ret = eval_lambda(e, av);
                else if (s == "if")       ret = eval_if(e, av);
                else if (s == "when")     ret = eval_when(e, av);
                else if (s == "unless")   ret = eval_unless(e, av);
                else if (s == "while")    ret = eval_while(e, av);
                else if (s == "and")      ret = eval_and(e, av);
                else if (s == "or")       ret = eval_or(e, av);
                else if (s == "for")      ret = eval_for(e, av);
                else if (s == "do-each")  ret = eval_do_each(e, av);
                else if (s == "case")     ret = eval_case(e, av);
                else if (s == ".")        ret = eval_dot_call(e, av);
                else if (s == "$")        ret = eval_field_get(e, av);
                else if (s == "$!")       ret = eval_field_set(e, av);
                else if (s == "$define!") ret = eval_meth_def(e, av);
                else if (s == "include")  ret = eval_include(e, av);
                else if (s == "displayln-time")
                {
                    if (av->m_len != 2)
                        error("'displayln-time' needs exactly 1 parameter", e);

                    using namespace std::chrono;

                    high_resolution_clock::time_point t1 = high_resolution_clock::now();
                    ret = eval(av->m_data[1]);
                    high_resolution_clock::time_point t2 = high_resolution_clock::now();
                    duration<double> time_span
                        = duration_cast<duration<double>>(t2 - t1);
                    std::cout << "eval run time: "
                              << (time_span.count() * 1000)
                              << " ms" << std::endl;
                              //, gc runs=" << gc_run_cnt << std::endl;
                }
                break;
            }
            else if (first.m_type == T_PRIM)
            {
                ret = call(first, av, true);
                break;
            }
            else if (first.m_type == T_CLOS)
            {
                ret = call(first, av, true);
                break;
            }
            else
                error("Non evaluatable first element in list", e);

            // Syntax handling or procedure call
            break;
        }
        default:
            error("Unknown atom type", e);
            break;
    }

    if (m_force_always_gc)
        m_rt->m_gc.collect();
    else
        m_rt->m_gc.collect_maybe();

    if (m_trace)
        cout << "<< eval: " << write_atom(e) << " => " << ret.to_write_str() << endl;
    return ret;
}
//---------------------------------------------------------------------------

Atom Interpreter::call_compiler(
    Atom prog,
    AtomVec *root_env,
    const std::string &input_name,
    bool only_compile)
{
    Atom compiler_func = get_compiler_func();

    try
    {
        AtomVec *args = m_rt->m_gc.allocate_vector(5);
        args->push(Atom(T_STR, m_rt->m_gc.new_symbol(input_name)));
        args->push(prog);
        args->push(Atom());
        args->push(Atom(T_BOOL, only_compile));
        args->push(Atom(T_VEC, root_env));

        return call(compiler_func, args, false);
    }
    catch (std::exception &e)
    {
        throw InterpreterException(
            "Compiler ERROR (" + input_name + "): " + e.what());
    }

    return Atom();
}
//---------------------------------------------------------------------------

Atom Interpreter::get_compiler_func()
{
    if (m_compiler_func.m_type != T_NIL)
        return m_compiler_func;

    const char *bukalisp_lib_path = std::getenv("BUKALISP_LIB");
    if (bukalisp_lib_path == NULL)
        bukalisp_lib_path = ".\\bukalisplib";

    std::string compiler_path =
        std::string(bukalisp_lib_path) + "\\" + "compiler.bkl";

    try
    {
        m_compiler_func = eval(compiler_path, slurp_str(compiler_path));
    }
    catch (BukaLISPException &e)
    {
        e.push("interpreter", compiler_path, 0, "get_compiler_func");
        throw e;
    }
    catch (std::exception &e)
    {
        throw BukaLISPException(
            "interpreter",
            compiler_path,
            0,
            "get_compiler_func",
            std::string(
                "Unknown error while compiling the compiler, Exception: ")
            + e.what());
    }

    if (m_compiler_func.m_type != T_CLOS)
        throw InterpreterException(
            "Compiler did not return a function! : "
            + m_compiler_func.to_write_str());

    return m_compiler_func;
}
//---------------------------------------------------------------------------

Atom Interpreter::call_compiler(
    const std::string &code_name,
    const std::string &code,
    AtomVec *root_env,
    bool only_compile)
{
    Atom compiler_func = get_compiler_func();

    try
    {
        Atom input_data = m_rt->read(code_name, code);
        return call_compiler(input_data, root_env, code_name, only_compile);
    }
    catch (std::exception &e)
    {
        throw InterpreterException(
            "Compiler ERROR (" + code_name + "): " + e.what());
    }

    return Atom();
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
