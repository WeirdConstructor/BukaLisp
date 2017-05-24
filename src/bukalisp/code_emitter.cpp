#include "code_emitter.h"
#include <iostream>

using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::open_output_file(const std::string &path)
{
    m_out_file_stream = new std::fstream(path.c_str(), std::ios::out);
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit_line(const std::string &line)
{
    if (m_out_file_stream)
        *m_out_file_stream << line << endl;
    cout << line << endl;
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit_json_op(const string &op,
                                      const string &operands)
{

    if (m_out_file_stream)
    {
        if (operands.empty())
            *m_out_file_stream << "[\"" << op << "\"]," << endl;
        else
            *m_out_file_stream << "[\"" << op << "\", " << operands << "]," << endl;
    }

    if (operands.empty())
        cout << "[\"" << op << "\"]," << endl;
    else
        cout << "[\"" << op << "\", " << operands << "]," << endl;
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit_binary_op(ASTNode *n)
{
    auto               op_node = n->m_childs[0];
    const string      &op      = op_node->m_text;
    int                nr_op   = n->m_childs.size() - 1; // -1 for the op itself

    if (nr_op == 1)
    {
        emit(n->m_childs[1]);
        // (+ 1) => (+ 0 1)
        if (   op == "+"
            || op == "i+"
            || op == "-"
            || op == "i-")
            emit_json_op("PUSH_I", "0");
        else if (   op == "*" 
                 || op == "i*"
                 || op == "/"
                 || op == "i/")
            emit_json_op("PUSH_I", "1");
        else
            emit_json_op("NOP");
    }
    else // nr_op >= 2
    {
        auto it = n->m_childs.begin();
        it++; // the operator

        emit(*it++);
        emit(*it++);
        emit(op_node);

        for (; it != n->m_childs.end(); it++)
        {
            emit(*it);
            emit(op_node);
        }
    }
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit_let(ASTNode *n
{
    // TODO: Rename CodeEmitter => Compiler & CodeEmitter
    // TODO: Environment
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit_form(ASTNode *n)
{
    if (n->m_childs.size() == 0)
    {
        cout << "ERROR: No empty lists allowed for now!" << endl;
        return;
    }

    auto childs = n->m_childs;

    auto first_sym = n->m_childs[0];
    if (first_sym->m_type != A_VAR)
    {
        cout << "ERROR: First element of any form must be a symbol!" << endl;
        return;
    }

    const string op = first_sym->m_text;

    if (
           op == "+"  || op == "-"  || op == "*"  || op == "/"
        || op == "i+" || op == "i-" || op == "i*" || op == "i/")
        emit_binary_op(n);
    else if (op == "let")
        emit_let(n);
//    else if (op == "set!")
//        emit_setM(n);
    else
    {
        cout << "ERROR: Unknown operation!" << endl;
        return;
    }
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit(ASTNode *n)
{
    switch (n->m_type)
    {
        case A_DBL:
            emit_json_op("PUSH_D", to_string(n->m_num.d));
            break;

        case A_INT:
            emit_json_op("PUSH_I", to_string(n->m_num.i));
            break;

        case A_VAR:
        {
            const string &op = n->m_text;
            if      (op == "+")   emit_json_op("ADD_D");
            else if (op == "i+")  emit_json_op("ADD_I");
            else if (op == "-")   emit_json_op("SUB_D");
            else if (op == "i-")  emit_json_op("SUB_I");
            else if (op == "*")   emit_json_op("MUL_D");
            else if (op == "i*")  emit_json_op("MUL_I");
            else if (op == "/")   emit_json_op("DIV_D");
            else if (op == "i/")  emit_json_op("DIV_I");

            // >, <, ==, != , <=, >=
            else                 emit_json_op("NOP");
            break;
        }

        case A_LIST:
        {
            emit_form(n);
            break;
        }
        default:
            cout << "ERROR: Unknown ASTNode Type!" << endl;
    }
}
//---------------------------------------------------------------------------

void ASTJSONCodeEmitter::emit_json(bukalisp::ASTNode *n)
{
    emit_line("[");
    emit(n);
    emit_line("[\"DBG_DUMP_STACK\"],");
    emit_line("[\"NOP\"]");
    emit_line("]");

    if (m_out_file_stream)
        delete m_out_file_stream;
}
//---------------------------------------------------------------------------
}
