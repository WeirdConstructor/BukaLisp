#include "interpreter.h"

using namespace std;
using namespace lilvm;

namespace bukalisp
{
//---------------------------------------------------------------------------

void Interpreter::init()
{
    m_root_stack = m_rt->m_gc.allocate_vector(0);
    m_rt->m_gc.add_root(m_root_stack);

    m_env_stack = m_rt->m_gc.allocate_vector(0);
    m_rt->m_gc.add_root(m_env_stack);

//    std::cout << "ENVSTKROOT=" << m_env_stack << ", ROOTSTKROOT=" << m_root_stack << std::endl;

    AtomMap *root_env = m_rt->m_gc.allocate_map();
    m_env_stack->push(Atom(T_MAP, root_env));

    Atom tmp;

#define DEF_SYNTAX(name) \
    tmp = Atom(T_SYNTAX); \
    tmp.m_d.sym = m_rt->m_gc.new_symbol(#name); \
    m_rt->m_gc.add_root(tmp.m_d.sym); \
    root_env->set(#name, tmp);

    DEF_SYNTAX(begin);
    DEF_SYNTAX(define);
    DEF_SYNTAX(set!);
    DEF_SYNTAX(quote);
    DEF_SYNTAX(let);
    DEF_SYNTAX(lambda);
    DEF_SYNTAX(if);
    DEF_SYNTAX(when);
    DEF_SYNTAX(unless);

#define START_PRIM() \
    tmp = Atom(T_PRIM); \
    tmp.m_d.func = new Atom::PrimFunc; \
    m_primitives.push_back(tmp.m_d.func); \
    (*tmp.m_d.func) = [this](AtomVec &args, Atom &out) {

#define END_PRIM(name) }; root_env->set(#name, tmp);


#define BIN_OP_LOOPS(op) \
        if (out.m_type == T_DBL) \
            for (size_t i = 1; i < args.m_len; i++) \
                out.m_d.d = out.m_d.d op args.m_data[i].to_dbl(); \
        else \
            for (size_t i = 1; i < args.m_len; i++) \
                out.m_d.i = out.m_d.i op args.m_data[i].to_int();

#define NO_ARG_NIL if (args.m_len <= 0) { out = Atom(); return; }

    START_PRIM()
        NO_ARG_NIL;
        out = args.m_data[0];
        BIN_OP_LOOPS(+)
    END_PRIM(+);

    START_PRIM()
        NO_ARG_NIL;
        out = args.m_data[0];
        BIN_OP_LOOPS(*)
    END_PRIM(*);

    START_PRIM()
        NO_ARG_NIL;
        out = args.m_data[0];
        BIN_OP_LOOPS(/)
    END_PRIM(/);

    START_PRIM()
        NO_ARG_NIL;
        out = args.m_data[0];
        BIN_OP_LOOPS(-)
    END_PRIM(-);

#define REQ_GT_ARGC(prim, cnt)    if (args.m_len < cnt) error("Not enough arguments to " #prim ", expected " #cnt);
#define REQ_EQ_ARGC(prim, cnt)    if (args.m_len != cnt) error("Wrong number of arguments to " #prim ", expected " #cnt);
#define BIN_CMP_OP_NUM(op) \
        out = Atom(T_BOOL); \
        if (args.m_data[0].m_type == T_DBL) \
            out.m_d.b = args.m_data[0].m_d.d op args.m_data[1].m_d.d; \
        else \
            out.m_d.b = args.m_data[0].m_d.i op args.m_data[1].m_d.i;

    START_PRIM()
        REQ_EQ_ARGC(<, 2);
        BIN_CMP_OP_NUM(<);
    END_PRIM(<);
    START_PRIM()
        REQ_EQ_ARGC(>, 2);
        BIN_CMP_OP_NUM(>);
    END_PRIM(>);
    START_PRIM()
        REQ_EQ_ARGC(<=, 2);
        BIN_CMP_OP_NUM(<=);
    END_PRIM(<=);
    START_PRIM()
        REQ_EQ_ARGC(>=, 2);
        BIN_CMP_OP_NUM(>=);
    END_PRIM(>=);

#define A0  (args.m_data[0])
#define A1  (args.m_data[1])

    START_PRIM()
        REQ_EQ_ARGC(eq?, 2);
        out = Atom(T_BOOL);
        switch (A0.m_type)
        {
            case T_BOOL: out.m_d.b =    A1.m_type == T_BOOL
                                     && A0.m_d.b == A1.m_d.b; break;
            case T_KW:   out.m_d.b =    A1.m_type == T_KW
                                     && A0.m_d.sym == A1.m_d.sym; break;
            case T_SYM:  out.m_d.b =    A1.m_type == T_SYM
                                     && A0.m_d.sym == A1.m_d.sym; break;
            case T_STR:  out.m_d.b =    A1.m_type == T_STR
                                     && A0.m_d.sym == A1.m_d.sym; break;
            case T_SYNTAX:
                         out.m_d.b =    A1.m_type == T_SYNTAX
                                     && A0.m_d.sym == A1.m_d.sym; break;
            case T_INT:  out.m_d.b =    A1.m_type == T_INT
                                     && A0.m_d.i == A1.m_d.i; break;
            case T_DBL:  out.m_d.b =    A1.m_type == T_DBL
                                     && A0.m_d.d == A1.m_d.d; break;
            case T_VEC:  out.m_d.b =    A1.m_type == T_VEC
                                     && A0.m_d.vec == A1.m_d.vec; break;
            case T_MAP:  out.m_d.b =    A1.m_type == T_MAP
                                     && A0.m_d.map == A1.m_d.map; break;
            case T_PRIM: out.m_d.b =    A1.m_type == T_PRIM
                                     && A0.m_d.func == A1.m_d.func; break;
            case T_NIL:  out.m_d.b = A1.m_type == T_NIL; break;
            default:
                out.m_d.b = false;
                break;
        }
    END_PRIM(eqv?);

    START_PRIM()
        REQ_EQ_ARGC(not, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.is_false();
    END_PRIM(not);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_begin(Atom e, AtomVec *av, size_t offs)
{
    for (size_t i = offs; i < av->m_len; i++)
        e = eval(av->m_data[i]);
    return e;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_define(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
        error("'define' does not contain enough parameters", e);

    Atom sym = av->m_data[1];
    if (sym.m_type != T_SYM)
        error(
            "first parameter of 'define' needs to be a symbol",
            sym);

    AtomMap *am = m_env_stack->last()->m_d.map;
    e = eval(av->m_data[2]);
    am->set(sym.m_d.sym->m_str, e);
    return e;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_setM(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
        error("'set!' does not contain enough parameters", e);

    Atom sym = av->m_data[1];
    if (sym.m_type != T_SYM)
        error(
            "first parameter of 'set!' needs to be a symbol",
            sym);

    AtomMap *env = nullptr;
    lookup(sym.m_d.sym, env);
    if (!env)
        error("'set!' access to undefined variable", sym);

    std::string varname = sym.m_d.sym->m_str;
    e = eval(av->m_data[2]);
    env->set(varname, e);
    return e;
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_let(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'let' does not contain enough parameters", e);

    if (av->m_data[1].m_type != T_VEC)
        error("First parameter for 'let' needs to be a list", e);

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
            bind_spec->m_data[0].m_d.sym->m_str,
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
    AtomVec *closure  = m_rt->m_gc.allocate_vector(2);
    closure->m_data[0] = Atom(T_VEC, clos_env);
    closure->m_data[1] = Atom(T_VEC, av);
    return Atom(T_CLOS, closure);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_if(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
        error("'if' does not contain enough parameters", e);
    else if (av->m_len > 4)
        error("'if' does contain too many parameters", e);

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
        error("'when' does not contain enough parameters", e);

    Atom cond_res = eval(av->m_data[1]);
    if (cond_res.is_false())
        return Atom();
    return eval_begin(e, av, 2);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval_unless(Atom e, AtomVec *av)
{
    if (av->m_len < 2)
        error("'unless' does not contain enough parameters", e);

    Atom cond_res = eval(av->m_data[1]);
    if (!cond_res.is_false())
        return Atom();
    return eval_begin(e, av, 2);
}
//---------------------------------------------------------------------------

Atom Interpreter::eval(Atom e)
{
    // TODO: Add eval flag
//    cout << ">> eval: " << write_atom(e) << endl;

    AtomVecPush avpe(m_root_stack, e);

    Atom ret;

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
            AtomMap *nm = m_rt->m_gc.allocate_map();
            ret = Atom(T_MAP, nm);
            AtomVecPush avp(m_root_stack, ret);

            for (auto e : e.m_d.map->m_map)
                nm->set(e.first, eval(e.second));
            break;
        }

        case T_SYM:
        {
            AtomMap *env = nullptr;
            ret = lookup(e.m_d.sym, env);
            if (!env)
                error("Undefined variable binding", e);
            break;
        }

        case T_VEC:
        {
            AtomVec *av = e.m_d.vec;
            m_debug_pos = m_rt->debug_info(av);

            if (av->m_len <= 0)
                error("Can't evaluate empty list of args", e);

            Atom first = eval(av->m_data[0]);

            if (first.m_type == T_SYNTAX)
            {
                // TODO: optimize by caching symbol pointers:
                std::string s = first.m_d.sym->m_str;
                if      (s == "begin")  ret = eval_begin(e, av, 1);
                else if (s == "define") ret = eval_define(e, av);
                else if (s == "set!")   ret = eval_setM(e, av);
                else if (s == "let")    ret = eval_let(e, av);
                else if (s == "quote")  ret = av->m_data[1];
                else if (s == "lambda") ret = eval_lambda(e, av);
                else if (s == "if")     ret = eval_if(e, av);
                else if (s == "when")   ret = eval_when(e, av);
                else if (s == "unless") ret = eval_unless(e, av);
                break;
            }
            else if (first.m_type == T_PRIM)
            {
                AtomVec *ev_av = m_rt->m_gc.allocate_vector(av->m_len - 1);
                AtomVecPush avp(m_root_stack, Atom(T_VEC, ev_av));

                for (size_t i = 1; i < av->m_len; i++)
                    ev_av->m_data[i - 1] = eval(av->m_data[i]);

                (*first.m_d.func)(*ev_av, ret);
                break;
            }
            else if (first.m_type == T_CLOS)
            {
                AtomVec *ev_av = m_rt->m_gc.allocate_vector(av->m_len - 1);
                AtomVecPush avp(m_root_stack, Atom(T_VEC, ev_av));

                for (size_t i = 1; i < av->m_len; i++)
                    ev_av->m_data[i - 1] = eval(av->m_data[i]);

                // Call of closure:
                // store current m_env_stack, continuation is stored on the C stack.
                // restore m_env_stack by letting it point to the vector in the closure
                // push environment
                // evaluate the stored code and later return the returned value
                // upon exit: pop-environment for cleanup!
                // restore old m_env_stack
                Atom env         = first.m_d.vec->m_data[0];
                Atom lambda_form = first.m_d.vec->m_data[1];

                AtomVec *old_env = m_env_stack;
                m_env_stack = env.m_d.vec;

                {
                    AtomMap *am_bind_env = m_rt->m_gc.allocate_map();
                    AtomVecPush avp_env(m_env_stack, Atom(T_MAP, am_bind_env));

                    AtomVec *binds = lambda_form.m_d.vec->m_data[1].m_d.vec;

                    // TODO: map ev_av to binds in current top env!

                    ret = eval_begin(lambda_form, lambda_form.m_d.vec, 2);
                }
                m_env_stack = old_env;
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

    AtomVecPush avr(m_root_stack, ret);
    m_rt->m_gc.collect();

//    cout << "<< eval: " << write_atom(e) << endl;
    return ret;
}
//---------------------------------------------------------------------------

}
