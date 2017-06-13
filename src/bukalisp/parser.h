#pragma once
#include "tokenizer.h"
#include <iostream>

/*

Language:


    (let ((a 10) (b 20)) ...)
    (set! a 20)

    let{
        ((a 10) (b 20))
        set!{a 120}
        if { (> a b)
            begin{
                set!{a 23}
                (set! b
                 lambda{ (x) (+ a x) })
            }
        }
    }
*/

namespace bukalisp
{
//---------------------------------------------------------------------------

enum ASTNodeType
{
    A_INT,
    A_DBL,
    A_KW,
    A_SYM,
    A_LIST
};
//---------------------------------------------------------------------------

struct ASTNode;
struct ASTNode
{
    ASTNodeType m_type;
    std::string m_text;
    int         m_line;
    std::string m_input_name;
    union {
        double  d;
        int64_t i;
    } m_num;

    ASTNode(ASTNodeType t, Token &tok) : m_type(t)
    {
        m_text = tok.m_text;
        if (tok.m_token_id == T_INT)
            m_num.i  = tok.m_num.i;
        else if (tok.m_token_id == T_DBL)
            m_num.d  = tok.m_num.d;
        m_line       = tok.m_line;
        m_input_name = tok.m_input_name;
    }

    std::string num_as_string()
    {
        if (m_type == A_DBL)
            return std::to_string(m_num.d);
        else if (m_type == A_INT)
            return std::to_string(m_num.i);
        else if (m_type == A_KW)
            return m_text;
        else
            return std::to_string(0);
    }

    ASTNode *clone() { return new ASTNode(*this); }

    std::vector<ASTNode *> m_childs;


    void unshift(ASTNode *n) { m_childs.insert(m_childs.begin(), n); }
    void push(ASTNode *n) { m_childs.push_back(n); }
    size_t csize() { return m_childs.size(); }
    ASTNode *_(size_t idx)
    {
        if (idx >= m_childs.size()) return nullptr;
        else                        return m_childs[idx];
    }

    ~ASTNode()
    {
        for (auto an : m_childs)
            if (an) delete an;
    }
};
//---------------------------------------------------------------------------

class Parser
{
    private:
        Tokenizer   &m_tok;
        bool        m_eof;

    public:
        Parser(Tokenizer &tok)
            : m_tok(tok), m_eof(false)
        {
        }

        void log_error(const std::string &what, Token &t)
        {
            std::cout << "ERROR [" << t.m_line << "] :" << what << std::endl;
        }

        ASTNode *parse_sequence(ASTNodeType type, char end_delim)
        {
            m_tok.next();
            Token t = m_tok.peek();

            ASTNode *seq = new ASTNode(type, t);

            bool is_first = true;
            while (t.m_token_id != T_EOF)
            {
                if (t.m_token_id == T_CHR_TOK && t.nth(0) == end_delim)
                    break;

                ASTNode *f = parse();
                if (!f) { delete seq; return nullptr; }
                is_first = false;

                seq->push(f);

                t = m_tok.peek();
            }
            m_tok.next();

            return seq;
        }

        ASTNode *parse_atom()
        {
            Token t = m_tok.peek();

            switch (t.m_token_id)
            {
                case T_DBL:     m_tok.next(); return new ASTNode(A_DBL, t); break;
                case T_INT:     m_tok.next(); return new ASTNode(A_INT, t); break;
                case T_CHR_TOK: m_tok.next(); return new ASTNode(A_KW, t); break;
                case T_STR_BODY: //          return new ASTNode(A_STR, t); break;
                    std::cout << "STR[" << t.m_text << "]" << std::endl;
                    log_error("Can't handle string atoms yet!", t);
                    break;
                case T_BAD_NUM:
                    log_error("Expected atom", t);
                    break;
            }

            return nullptr;
        }

        ASTNode *parse()
        {
            using namespace std;

            Token t = m_tok.peek();

            switch (t.m_token_id)
            {
                case T_EOF: m_tok.next(); m_eof = true; return nullptr; break;
                case T_DBL: return parse_atom(); break;
                case T_INT: return parse_atom(); break;
                case T_CHR_TOK:
                {
                    switch (t.nth(0))
                    {
                        case '(': return parse_sequence(A_LIST, ')');
                        case '[':
                            {
                                ASTNode *seq = parse_sequence(A_LIST, ']');

                                Token t2(t);
                                t2.m_text = "list";
                                seq->unshift(new ASTNode(A_SYM, t2));
                                return seq;
                            }
                        case '{':
                        {
                            log_error("Unexpected '{'", t);
                            return nullptr;
                        }
                        default:
                        {
                            m_tok.next();

                            ASTNode *var = nullptr;
                            if (t.m_text[t.m_text.size() - 1] == ':')
                            {
                                Token t2(t);
                                t2.m_text = t2.m_text.substr(0, t2.m_text.size() - 1);
                                var = new ASTNode(A_KW, t2);
                            }
                            else
                                var = new ASTNode(A_SYM, t);

                            return var;
                        }
                        break;
                    }
                }
                case T_STR_BODY: //          return new ASTNode(A_STR, t); break;
                    std::cout << "STR[" << t.m_text << "]" << std::endl;
                    log_error("Can't handle strings yet!", t);
                    break;
                case T_BAD_NUM:
                    log_error("Bad number found", t);
                    break;
            }

            return nullptr;
        }
};
//---------------------------------------------------------------------------

}
