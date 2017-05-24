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
    A_VAR,
    A_LIST
};
//---------------------------------------------------------------------------

struct ASTNode;
struct ASTNode
{
    ASTNodeType m_type;
    std::string m_text;
    int         m_line;
    union {
        double  d;
        int64_t i;
    } m_num;

    ASTNode(ASTNodeType t, Token &tok) : m_type(t)
    {
        m_text = tok.m_text;
        if (tok.m_token_id == T_INT)
            m_num.i  = tok.m_num.i;
        else if (tok.m_token_id == T_INT)
            m_num.d  = tok.m_num.d;
    }

    ASTNode *clone() { return new ASTNode(*this); }

    std::vector<ASTNode *> m_childs;

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

        ASTNode *parse_sequence(char end_delim, ASTNode *seq = nullptr)
        {
            Token t = m_tok.peek();
            m_tok.next();

            if (!seq)
                seq = new ASTNode(A_LIST, t);

            while (t.m_token_id != T_EOF)
            {
                if (t.m_token_id == T_CHR_TOK && t.nth(0) == end_delim)
                    break;

                ASTNode *f = parse();
                if (!f) { delete seq; return nullptr; }

                seq->m_childs.push_back(f);

                t = m_tok.peek();
            }
            m_tok.next();

            t = m_tok.peek();

            if (t.m_token_id == T_CHR_TOK && t.nth(0) == '{')
                return parse_sequence('}', seq);

            return seq;
        }

        ASTNode *parse()
        {
            Token t = m_tok.peek();

            switch (t.m_token_id)
            {
                case T_EOF: m_tok.next(); m_eof = true; return nullptr; break;
                case T_DBL: m_tok.next(); return new ASTNode(A_DBL, t); break;
                case T_INT: m_tok.next(); return new ASTNode(A_INT, t); break;
                case T_CHR_TOK:
                {
                    switch (t.nth(0))
                    {
                        case '(': return parse_sequence(')');
                        case '{':
                        {
                            log_error("Unexpected '{'", t);
                            return nullptr;
                        }
                        default:
                        {
                            m_tok.next();
                            ASTNode *var = new ASTNode(A_VAR, t);

                            t = m_tok.peek();
                            if (t.m_token_id == T_CHR_TOK && t.nth(0) == '{')
                            {
                                ASTNode *seq = new ASTNode(A_LIST, t);
                                seq->m_childs.push_back(var);
                                return parse_sequence('}', seq);
                            }

                            return var;
                        }
                        break;
                    }
                }
                case T_STR_BODY: //          return new ASTNode(A_STR, t); break;
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
