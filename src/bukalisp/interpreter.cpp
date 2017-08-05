#include "interpreter.h"
#include "buklivm.h"
#include <chrono>

using namespace std;
using namespace lilvm;

namespace bukalisp
{
//---------------------------------------------------------------------------

void Interpreter::init()
{
    ExternalGCRoot::init();

    m_root_stack = m_rt->m_gc.allocate_vector(0);
    m_env_stack  = m_rt->m_gc.allocate_vector(0);

//    std::cout << "ENVSTKROOT=" << m_env_stack << ", ROOTSTKROOT=" << m_root_stack << std::endl;

    AtomMap *root_env = m_rt->m_gc.allocate_map();
    m_env_stack->push(Atom(T_MAP, root_env));

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
    DEF_SYNTAX(.);
    DEF_SYNTAX($define!);
    DEF_SYNTAX($!);
    DEF_SYNTAX($);
    DEF_SYNTAX(displayln-time);

#define START_PRIM() \
    tmp = Atom(T_PRIM); \
    tmp.m_d.func = new Atom::PrimFunc; \
    m_primitives.push_back(tmp.m_d.func); \
    (*tmp.m_d.func) = [this](AtomVec &args, Atom &out) {

#define END_PRIM(name) }; root_env->set(Atom(T_SYM, m_rt->m_gc.new_symbol(#name)), tmp);


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
#define A2  (args.m_data[2])

    START_PRIM()
        out = Atom(T_VEC);
        AtomVec *l = m_rt->m_gc.allocate_vector(args.m_len);
        out.m_d.vec = l;
        for (size_t i = 0; i < args.m_len; i++)
            l->m_data[i] = args.m_data[i];
    END_PRIM(list);

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
            case T_UD:   out.m_d.b =    A1.m_type == T_UD 
                                     && A0.m_d.ud == A1.m_d.ud; break;
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

    START_PRIM()
        REQ_EQ_ARGC(symbol?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_SYM;
    END_PRIM(symbol?);

    START_PRIM()
        REQ_EQ_ARGC(keyword?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_KW;
    END_PRIM(keyword?);

    START_PRIM()
        REQ_EQ_ARGC(nil?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_NIL;
    END_PRIM(nil?);

    START_PRIM()
        REQ_EQ_ARGC(exact?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_INT;
    END_PRIM(exact?);

    START_PRIM()
        REQ_EQ_ARGC(inexact?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_DBL;
    END_PRIM(inexact?);

    START_PRIM()
        REQ_EQ_ARGC(string?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_STR;
    END_PRIM(string?);

    START_PRIM()
        REQ_EQ_ARGC(boolean?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_BOOL;
    END_PRIM(boolean?);

    START_PRIM()
        REQ_EQ_ARGC(list?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_VEC;
    END_PRIM(list?);

    START_PRIM()
        REQ_EQ_ARGC(map?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_MAP;
    END_PRIM(map?);

    START_PRIM()
        REQ_EQ_ARGC(procedure?, 1);
        out = Atom(T_BOOL);
        out.m_d.b = A0.m_type == T_PRIM || A0.m_type == T_CLOS;
    END_PRIM(procedure?);

    START_PRIM()
        REQ_EQ_ARGC(@, 2);
        if (A1.m_type == T_VEC)
        {
            int64_t idx = A0.to_int();
            if (idx < 0) out = Atom();
            else         out = A1.at((size_t) idx);
        }
        else if (A1.m_type == T_MAP)
        {
            out = A1.at(A0);
        }
        else
            error("Can apply '@' only to lists or maps", A1);
    END_PRIM(@);

    START_PRIM()
        REQ_EQ_ARGC(@!, 3);
        if (A1.m_type == T_VEC)
        {
            int64_t i = A0.to_int();
            if (i >= 0)
                A1.m_d.vec->set((size_t) A0.to_int(), A2);
            out = A2;
        }
        else if (A1.m_type == T_MAP)
        {
            A1.m_d.map->set(A0, A2);
            out = A2;
        }
        else
            error("Can apply '@!' only to lists or maps", A1);
    END_PRIM(@!);

    START_PRIM()
        REQ_EQ_ARGC(type, 1);
        out = Atom(T_SYM);
        switch (A0.m_type)
        {
            // TODO: use X macro!
            case T_NIL:    out.m_d.sym = m_rt->m_gc.new_symbol("nil");       break;
            case T_INT:    out.m_d.sym = m_rt->m_gc.new_symbol("exact");     break;
            case T_DBL:    out.m_d.sym = m_rt->m_gc.new_symbol("inexact");   break;
            case T_BOOL:   out.m_d.sym = m_rt->m_gc.new_symbol("boolean");   break;
            case T_STR:    out.m_d.sym = m_rt->m_gc.new_symbol("string");    break;
            case T_SYM:    out.m_d.sym = m_rt->m_gc.new_symbol("symbol");    break;
            case T_KW:     out.m_d.sym = m_rt->m_gc.new_symbol("keyword");   break;
            case T_VEC:    out.m_d.sym = m_rt->m_gc.new_symbol("list");      break;
            case T_MAP:    out.m_d.sym = m_rt->m_gc.new_symbol("map");       break;
            case T_PRIM:   out.m_d.sym = m_rt->m_gc.new_symbol("procedure"); break;
            case T_SYNTAX: out.m_d.sym = m_rt->m_gc.new_symbol("syntax");    break;
            case T_CLOS:   out.m_d.sym = m_rt->m_gc.new_symbol("procedure"); break;
            case T_UD:     out.m_d.sym = m_rt->m_gc.new_symbol("userdata");  break;
            default:       out.m_d.sym = m_rt->m_gc.new_symbol("<unknown>"); break;
        }
    END_PRIM(type)

    START_PRIM()
        REQ_EQ_ARGC(length, 1);
        out = Atom(T_INT);
        if (A0.m_type == T_VEC)         out.m_d.i = A0.m_d.vec->m_len;
        else if (A1.m_type == T_MAP)    out.m_d.i = A0.m_d.map->m_map.size();
        else                            out.m_d.i = 0;
    END_PRIM(length)

    START_PRIM()
        REQ_EQ_ARGC(push!, 2);

        if (A0.m_type != T_VEC)
            error("Can't push onto something that is not a list", A0);

        A0.m_d.vec->push(A1);
        out = A1;
    END_PRIM(push!)

    START_PRIM()
        REQ_EQ_ARGC(pop!, 1);

        if (A0.m_type != T_VEC)
            error("Can't pop from something that is not a list", A0);

        Atom *a = A0.m_d.vec->last();
        if (a) out = *a;
        A0.m_d.vec->pop();
    END_PRIM(pop!)

    START_PRIM()
        REQ_EQ_ARGC(make-vm-prog, 1);
        out = bukalisp::make_prog(A0);
        if (out.m_type == T_UD)
            m_rt->m_gc.reg_userdata(out.m_d.ud);
    END_PRIM(make-vm-prog)

    START_PRIM()
        REQ_EQ_ARGC(run-vm-prog, 1);

        if (A0.m_type != T_UD || A0.m_d.ud->type() != "VM-PROG")
            error("Bad input type to run-vm-prog, expected VM-PROG.", A0);

        if (!m_vm)
            error("Can't execute VM progs, no VM instance loaded into interpreter", A0);

        out = m_vm->eval(A0, &args);
    END_PRIM(run-vm-prog)

    START_PRIM()
        REQ_GT_ARGC(error, 1);

        Atom a(T_VEC);
        a.m_d.vec = m_rt->m_gc.allocate_vector(args.m_len - 1);
        for (size_t i = 1; i < args.m_len; i++)
            a.m_d.vec->m_data[i - 1] = args.m_data[i];
        error(A0.to_display_str(), a);
    END_PRIM(error)

    START_PRIM()
        REQ_GT_ARGC(display, 1);

        for (size_t i = 0; i < args.m_len; i++)
        {
            std::cout << args.m_data[i].to_display_str();
            if (i < (args.m_len - 1))
                std::cout << " ";
        }

        out = args.m_data[args.m_len - 1];
    END_PRIM(display)

    START_PRIM()
        REQ_EQ_ARGC(newline, 0);
        std::cout << std::endl;
    END_PRIM(newline)

    START_PRIM()
        REQ_GT_ARGC(displayln, 1);

        for (size_t i = 0; i < args.m_len; i++)
        {
            std::cout << args.m_data[i].to_display_str();
            if (i < (args.m_len - 1))
                std::cout << " ";
        }
        std::cout << std::endl;

        out = args.m_data[args.m_len - 1];
    END_PRIM(displayln)

    START_PRIM()
        REQ_EQ_ARGC(atom-id, 1);
        out = Atom(T_INT, A0.id());
    END_PRIM(atom-id)

    START_PRIM()
        REQ_GT_ARGC(str, 0);
        std::string out_str;
        for (size_t i = 0; i < args.m_len; i++)
            out_str += args.m_data[i].to_display_str();
        out = Atom(T_STR, m_rt->m_gc.new_symbol(out_str));
    END_PRIM(str)

    START_PRIM()
        REQ_GT_ARGC(str-join, 1);
        std::string sep = A0.to_display_str();
        std::string out_str;
        for (size_t i = 1; i < args.m_len; i++)
        {
            if (i > 1)
                out_str += sep;
            out_str += args.m_data[i].to_display_str();
        }
        out = Atom(T_STR, m_rt->m_gc.new_symbol(out_str));
    END_PRIM(str-join)

    START_PRIM()
        REQ_EQ_ARGC(last, 1);
        if (A0.m_type != T_VEC)
            error("Can't 'last' on a non-list", A0);
        AtomVec *avl = A0.m_d.vec;
        if (avl->m_len <= 0)
            return;
        out = avl->m_data[avl->m_len - 1];
    END_PRIM(last)

    START_PRIM()
        REQ_EQ_ARGC(last, 1);
        if (A0.m_type != T_VEC)
            error("Can't 'first' on a non-list", A0);
        out = A0.at(0);
    END_PRIM(first)
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
    closure->m_data[0] = Atom(T_VEC, clos_env);
    closure->m_data[1] = Atom(T_VEC, av);
    closure->m_data[2] =
        m_debug_pos_map ? Atom(T_MAP, m_debug_pos_map) : Atom();
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

    Atom last;
    Atom while_cond = eval(av->m_data[1]);
    bool run = !while_cond.is_false();
    while (run)
    {
        last = eval_begin(e, av, 2);
        AtomVecPush avpw(m_root_stack, last);
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
    if (end > i)
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

        Atom ds = eval(bnd_spec->m_data[2]);
        AtomVecPush avpr(m_root_stack, ds);

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
        Atom ds = eval(bnd_spec->m_data[1]);
        AtomVecPush avpr(m_root_stack, ds);

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

Atom Interpreter::eval_field_get(Atom e, AtomVec *av)
{
    if (av->m_len < 3)
    {
        error("'$' map field needs at least 2 arguments: "
              "the key and the map", e);
    }

    Atom key = av->m_data[1];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);
    AtomVecPush avpk(m_root_stack, key);
    Atom obj = eval(av->m_data[2]);
    AtomVecPush avpo(m_root_stack, obj);

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

    Atom key = av->m_data[1];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);
    AtomVecPush avpk(m_root_stack, key);
    Atom obj = eval(av->m_data[2]);
    AtomVecPush avpo(m_root_stack, obj);

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

    Atom obj = eval(av->m_data[1]);
    AtomVecPush avpo(m_root_stack, obj);

    if (obj.m_type != T_MAP)
        error("Can't define method on non map atom", obj);

    if (av->m_data[2].m_type != T_VEC)
        error("Argument binding definition is not a list", av->m_data[2]);

    AtomVec *arg_bind_def = av->m_data[2].m_d.vec;

    Atom key = arg_bind_def->m_data[0];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);
    AtomVecPush avpk(m_root_stack, key);

    AtomVec *arg_def_av = m_rt->m_gc.allocate_vector(arg_bind_def->m_len - 1);
    for (size_t i = 1; i < arg_bind_def->m_len; i++)
    {
        Atom bind_param = arg_bind_def->m_data[i];
        if (bind_param.m_type != T_SYM)
            error("Argument binding parameter name must be a symbol", bind_param);
        arg_def_av->m_data[i - 1] = bind_param;
    }

    AtomVec *lambda_av = m_rt->m_gc.allocate_vector(2 + (av->m_len - 3));
    lambda_av->m_data[1] = Atom(T_VEC, arg_def_av);
    for (size_t i = 3; i < av->m_len; i++)
        lambda_av->m_data[i - 1] = av->m_data[i];

    Atom lambda(T_VEC, lambda_av);
    AtomVecPush avls(m_root_stack, lambda);

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

    Atom key = av->m_data[1];
    if (   key.m_type != T_SYM
        && key.m_type != T_STR
        && key.m_type != T_KW)
        key = eval(key);
    AtomVecPush avpk(m_root_stack, key);
    Atom obj = eval(av->m_data[2]);
    AtomVecPush avpo(m_root_stack, obj);

    Atom method = obj.at(key);
    AtomVecPush avpm(m_root_stack, method);
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
        AtomVec *ev_av = m_rt->m_gc.allocate_vector(av->m_len - 1);
        AtomVecPush avp(m_root_stack, Atom(T_VEC, ev_av));

        for (size_t i = 1 + arg_offs; i < av->m_len; i++)
            ev_av->m_data[i - (1 + arg_offs)] = eval(av->m_data[i]);

        av = ev_av;
    }

    AtomVecPush avp(m_root_stack, Atom(T_VEC, av));

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

        AtomVec *old_env = m_env_stack;
        AtomVecPush avp_old_env(m_root_stack, Atom(T_VEC, old_env));

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

            // XXX: Old m_debug_pos_map and the closure keep alive
            //      the AtomMaps.
            AtomMap *old_debug_pos_map = m_debug_pos_map;
            m_debug_pos_map =
                debug_pos.m_type == T_MAP ? debug_pos.m_d.map : nullptr;
            try
            {
                ret = eval_begin(lambda_form, lambda_form.m_d.vec, 2);
            }
            catch (...)
            {
                m_debug_pos_map = old_debug_pos_map;
                throw;
            }
            m_debug_pos_map = old_debug_pos_map;
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

void Interpreter::set_debug_pos(Atom &a)
{
    if (!m_debug_pos_map)
        m_debug_pos = "";
    Atom deb_info = m_debug_pos_map->at(Atom(T_INT, a.id()));
    m_debug_pos = deb_info.to_display_str();
}
//---------------------------------------------------------------------------

Atom Interpreter::eval(Atom e)
{
    // TODO: Add eval flag
    if (m_trace)
        cout << ">> eval: " << write_atom(e) << endl;

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
            set_debug_pos(e);

            AtomMap *nm = m_rt->m_gc.allocate_map();
            ret = Atom(T_MAP, nm);
            AtomVecPush avp(m_root_stack, ret);

            std::cout << "INTMAP: " << e.to_write_str() << std::endl;

            for (auto el : e.m_d.map->m_map)
            {
                Atom key = eval(el.first);
                AtomVecPush avpk(m_root_stack, key);
                Atom val = eval(el.second);
                nm->set(key, val);
            }
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
            set_debug_pos(e);

            AtomVec *av = e.m_d.vec;

            if (av->m_len <= 0)
                error("Can't evaluate empty list of args", e);

            Atom first = eval(av->m_data[0]);
            AtomVecPush avpf(m_root_stack, first);

            if (first.m_type == T_SYNTAX)
            {
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
                else if (s == ".")        ret = eval_dot_call(e, av);
                else if (s == "$")        ret = eval_field_get(e, av);
                else if (s == "$!")       ret = eval_field_set(e, av);
                else if (s == "$define!") ret = eval_meth_def(e, av);
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

    AtomVecPush avr(m_root_stack, ret);
    if (m_force_always_gc)
        m_rt->m_gc.collect();
    else
        m_rt->m_gc.collect_maybe();

    if (m_trace)
        cout << "<< eval: " << write_atom(e) << " => " << ret.to_write_str() << endl;
    return ret;
}
//---------------------------------------------------------------------------

}
