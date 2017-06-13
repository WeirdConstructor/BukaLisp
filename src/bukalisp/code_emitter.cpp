#include "code_emitter.h"
#include <iostream>
#include <sstream>

using namespace std;

namespace bukalisp
{

OutputPad::OpText::OpText(ASTNode *n, const std::string &text)
    : m_line(n->m_line), m_input_name(n->m_input_name),
      m_op_text(text)
{
}
//---------------------------------------------------------------------------

void OutputPad::add_str(const string &op, string arg1)
{
    std::stringstream ss;

    arg1 = str_replace(str_replace(arg1, "\\", "\\\\"), "\"", "\\\"");

    if (arg1.empty())
        ss << "[\"" << op << "\"],";
    else
        ss << "[\"" << op << "\", \"" << arg1 << "\"],";

    m_ops->push_back(OpText(m_node, ss.str()));
}
//---------------------------------------------------------------------------

void OutputPad::add(const string &op, string arg1, string arg2)
{
    std::stringstream ss;

    if (arg1.empty())
        ss << "[\"" << op << "\"],";
    else if (arg2.empty())
        ss << "[\"" << op << "\", " << arg1 << "],";
    else
        ss << "[\"" << op << "\", " << arg1 << ", " << arg2 << "],";

    m_ops->push_back(OpText(m_node, ss.str()));
}
//---------------------------------------------------------------------------

OutputPad ASTJSONCodeEmitter::emit_binary_op(ASTNode *n)
{
    OutputPad o(n);

    auto               op_node = n->m_childs[0];
    const string      &op      = op_node->m_text;
    int                nr_op   = n->m_childs.size() - 1; // -1 for the op itself

    if (nr_op == 1)
    {
        o.append(emit(n->m_childs[1]));

        // (+ 1) => (+ 0 1)
        if (   op == "+"
            || op == "i+"
            || op == "-"
            || op == "i-")
            o.add("PUSH_I", "0");
        else if (   op == "*"
                 || op == "i*"
                 || op == "/"
                 || op == "i/")
            o.add("PUSH_I", "1");
        o.append(emit(op_node));
    }
    else // nr_op >= 2
    {
        auto it = n->m_childs.begin();
        it++; // the operator

        o.append(emit(*it++));
        o.append(emit(*it++));
        o.append(emit(op_node));

        for (; it != n->m_childs.end(); it++)
        {
            o.append(emit(*it));
            o.append(emit(op_node));
        }
    }

    return o;
}
//---------------------------------------------------------------------------

OutputPad ASTJSONCodeEmitter::emit_let(ASTNode *n)
{
    OutputPad o(n);

    m_env = std::make_shared<Environment>(m_env);

    auto defs = n->m_childs[1];
    if (!defs)
    {
        cout << "ERROR: Let does not have enough arguments!" << endl;
        return o;
    }

    if (defs->m_type != A_LIST)
    {
        cout << "ERROR: Let does not have a list as first argument!" << endl;
        return o;
    }

    o.add("PUSH_ENV", to_string(defs->m_childs.size()));

    for (size_t idx = 0; idx < defs->m_childs.size(); idx++)
    {
        auto def_pair = defs->m_childs[idx];

        if (def_pair->m_type != A_LIST)
        {
            cout << "ERROR: Let definition at index "
                 << idx << " is not a list!" << endl;
            return o;
        }

        if (def_pair->m_childs.size() != 2)
        {
            cout << "ERROR: Bad let definition pair at index "
                 << idx << endl;
            return o;
        }

        if (def_pair->m_childs[0]->m_type != A_SYM)
        {
            cout << "ERROR: In let, can't bind to non symbol at index "
                 << idx << endl;
            return o;
        }

        auto sym = def_pair->m_childs[0];
        auto val = def_pair->m_childs[1];

        size_t var_pos = m_env->new_env_pos_for(m_symtbl->str2sym(sym->m_text));
        o.append(emit(val));
        o.add("SET_ENV", "0", to_string(var_pos));
    }

    o.append(emit_block(n, 2));

    o.add("POP_ENV");
    m_env = m_env->m_parent;

    return o;
}
//---------------------------------------------------------------------------

OutputPad ASTJSONCodeEmitter::emit_block(ASTNode *n, size_t offs)
{
    OutputPad o(n);

    for (size_t i = offs; i < n->m_childs.size(); i++)
    {
        o.append(emit(n->m_childs[i]));

        if ((i + 1) != n->m_childs.size())
            o.add("POP", "1");
    }

    return o;
}
//---------------------------------------------------------------------------

OutputPad ASTJSONCodeEmitter::emit_form(ASTNode *n)
{
    OutputPad o(n);

    if (n->m_childs.size() == 0)
    {
        cout << "ERROR: No empty lists allowed for now!" << endl;
        return o;
    }

    auto childs = n->m_childs;

    auto first_sym = n->_(0);
    if (first_sym->m_type != A_SYM)
    {
        cout << "ERROR: First element of any form must be a symbol!" << endl;
        return o;
    }

    const string op = first_sym->m_text;

    if (   op == "+"  || op == "-"  || op == "*"  || op == "/"
        || op == "i+" || op == "i-" || op == "i*" || op == "i/"
        || op == ">"  || op == "i>"
        || op == ">=" || op == "i>=" || op == "<" || op == "i<"
        || op == "<=" || op == "i<=" || op == "==" || op == "i==")
        o.append(emit_binary_op(n));

    else if (op == "." || op == "$")
    {
        if (n->csize() < 3)
        {
            cout << "ERROR: '" << op << "' requires at least 2 arguments "
                    "(object/function and field)" << endl;
            return o;
        }

        if (   n->_(2)->m_type != A_SYM
            && n->_(2)->m_type != A_KW
            && n->_(2)->m_type != A_INT)
        {
            cout << "ERROR: '" << op << "' requires a symbol, keyword or "
                    "integer as 2nd argument!"
                 << endl;
            return o;
        }

        size_t argc = 0;
        for (int i = n->m_childs.size() - 1; i >= 3; i--, argc++)
        {
            o.append(emit(n->m_childs[i]));
        }
        argc++;
        o.append(emit_atom(n->_(2)));
        o.append(emit(n->_(1)));

        if (op == "$")
            o.add("GET", std::to_string(argc));
        else
            o.add("CALL", std::to_string(argc));
    }

    else if (op == "quote")
        // TODO: Emit quoted list!
        o.append(emit_atom(n));

    else if (op == "let")
        o.append(emit_let(n));

    else if (op == "list")
        o.append(emit_list(n));

    else if (op == "car")
    {
        if (n->m_childs.size() != 2)
        {
            cout << "ERROR: 'car' needs exactly 1 argument!" << endl;
            return o;
        }

        o.append(emit(n->m_childs[1]));
        o.add("GET_REF");
    }
    else if (op == "tail")
    {
        if (n->m_childs.size() < 2)
        {
            cout << "ERROR: tail with too few arguments!" << endl;
            return o;
        }
        else if (n->m_childs.size() > 3)
        {
            cout << "ERROR: tail with too many arguments!" << endl;
            return o;
        }

        if (n->m_childs.size() == 3)
        {
            if (n->m_childs[1]->m_type != A_INT || n->m_childs[1]->m_type != A_DBL)
            {
                o.append(emit(n->m_childs[1]));
                o.append(emit(n->m_childs[2]));
                o.add("TAIL", "0");
            }
            else
            {
                o.append(emit(n->m_childs[2]));
                o.add("TAIL", n->m_childs[1]->num_as_string());
            }
        }
        else
        {
            o.append(emit(n->m_childs[1]));
            o.add("TAIL", "1");
        }
    }
//    else if (op == "set!")
//        emit_setM(n);
    else // a call
    {
        size_t argc = 0;
        if (n->m_childs.size() == 0)
        {
            cout << "ERROR: Invalid empty list." << endl;
            return o;
        }

        for (int i = n->m_childs.size() - 1; i >= 0; i--, argc++)
        {
            o.append(emit(n->m_childs[i]));
        }
        o.add("CALL", std::to_string(argc - 1));
        return o;
    }

    return o;
}
//---------------------------------------------------------------------------

OutputPad ASTJSONCodeEmitter::emit_list(ASTNode *n)
{
    OutputPad o(n);

    int len = 0;
    for (size_t i = 1; i < n->m_childs.size(); i++)
    {
        auto ch = n->m_childs[i];
        o.append(emit(ch));
        len++;
    }

    if (len == 0)
    {
        o.add("PUSH_NIL");
        len++;
    }

    o.add("LIST", to_string(len - 1));

    return o;
}
//---------------------------------------------------------------------------

OutputPad ASTJSONCodeEmitter::emit_atom(ASTNode *n)
{
    OutputPad o(n);

    switch (n->m_type)
    {
        case A_DBL:
            o.add("PUSH_D", to_string(n->m_num.d));
            break;

        case A_INT:
            o.add("PUSH_I", to_string(n->m_num.i));
            break;

        case A_KW:
            o.add_str("PUSH_SYM", n->m_text);
            break;

        case A_SYM:
            o.add_str("PUSH_SYM", n->m_text);
            break;

        default:
            cout << "ERROR: Invalid atom emit!" << endl;
    }

    return o;
}
//---------------------------------------------------------------------------

OutputPad ASTJSONCodeEmitter::emit(ASTNode *n)
{
    OutputPad o(n);

    switch (n->m_type)
    {
        case A_DBL: o.append(emit_atom(n)); break;
        case A_INT: o.append(emit_atom(n)); break;
        case A_KW:  o.append(emit_atom(n)); break;

        case A_SYM:
        {
            const string &op = n->m_text;
            if      (op == "+")   o.add("ADD_D");
            else if (op == "i+")  o.add("ADD_I");
            else if (op == "-")   o.add("SUB_D");
            else if (op == "i-")  o.add("SUB_I");
            else if (op == "*")   o.add("MUL_D");
            else if (op == "i*")  o.add("MUL_I");
            else if (op == "/")   o.add("DIV_D");
            else if (op == "i/")  o.add("DIV_I");
            else if (op == ">")   o.add("GT_D");
            else if (op == "i>")  o.add("GT_I");
            else if (op == ">=")  o.add("GE_D");
            else if (op == "i>=") o.add("GE_I");
            else if (op == "<")   o.add("LT_D");
            else if (op == "i<")  o.add("LT_I");
            else if (op == "<=")  o.add("LE_D");
            else if (op == "i<=") o.add("LE_I");
            else if (op == "==")  o.add("EQ_D");
            else if (op == "i==") o.add("EQ_I");

            // >, <, ==, != , <=, >=
            else
            {
                Environment::Slot s;
                size_t depth = 0;

                lilvm::Sym *sym = m_symtbl->str2sym(n->m_text);

                if (   !m_env
                    || !m_env->get_env_pos_for(sym, s, depth))
                {
                    if (!m_root->get_env_pos_for(sym, s, depth))
                    {
                        cout << "ERROR: Undefined identifier '"
                             << n->m_text << "'" << endl;
                    }
                    else
                    {
                        if (s.type != Environment::SLT_PRIM)
                        {
                            cout << "ERROR: Invalid entry '" << n->m_text
                                 << "' in root environment!" << endl;
                            return o;
                        }

                        o.add("PUSH_PRIM", std::to_string(s.idx));
                    }
                    return o;
                }
                o.add("GET_ENV", to_string(depth), to_string(s.idx));
            }
            break;
        }

        case A_LIST:
        {
            o.append(emit_form(n));
            break;
        }
        default:
            cout << "ERROR: Unknown ASTNode Type!" << endl;
    }

    return o;
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit_json(bukalisp::ASTNode *n)
{
    OutputPad o(n);

    o.append(emit(n));
    o.add("DBG_DUMP_STACK");

    *m_out_stream << "[" << endl;
    o.output(*m_out_stream);
    *m_out_stream << "[\"NOP\"]]" << endl;
}
//---------------------------------------------------------------------------
}
