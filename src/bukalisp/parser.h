#pragma once
#include "tokenizer.h"
#include <iostream>

namespace bukalisp
{
//---------------------------------------------------------------------------

class SEX_Builder
{
    protected:
        std::string         m_dbg_input_name;
        size_t              m_dbg_line;

    public:
        virtual ~SEX_Builder() { }

        virtual void set_debug_info(const std::string &inp_name, size_t line)
        {
            m_dbg_input_name = inp_name;
            m_dbg_line       = line;
        }

        virtual void start_list()                            = 0;
        virtual void end_list()                              = 0;

        virtual void start_map()                             = 0;
        virtual void start_kv_pair()                         = 0;
        virtual void end_kv_key()                            = 0;
        virtual void end_kv_pair()                           = 0;
        virtual void end_map()                               = 0;

        virtual void atom_string(const std::string &str)     = 0;
        virtual void atom_symbol(const std::string &symstr)  = 0;
        virtual void atom_keyword(const std::string &symstr) = 0;
        virtual void atom_int(int64_t i)                     = 0;
        virtual void atom_dbl(double d)                      = 0;
        virtual void atom_nil()                              = 0;
        virtual void atom_bool(bool b)                       = 0;
};
//---------------------------------------------------------------------------

class Parser
{
    private:
        Tokenizer   &m_tok;
        SEX_Builder *m_builder;
        bool        m_eof;

    public:
        Parser(Tokenizer &tok, SEX_Builder *builder)
            : m_tok(tok), m_eof(false), m_builder(builder)
        {
        }

        void reset() { m_eof = false; }

        bool is_eof() { return m_eof; }

        void log_error(const std::string &what, Token &t)
        {
            std::cout << "ERROR [" << t.m_line << "] :" << what << std::endl;
        }

        void debug_token(Token &t)
        {
            m_builder->set_debug_info(t.m_input_name, t.m_line);
        }

        bool parse_map()
        {
            m_tok.next();
            Token t = m_tok.peek();

            debug_token(t);
            m_builder->start_map();

            while (t.m_token_id != TOK_EOF)
            {
                if (t.m_token_id == TOK_CHR && t.nth(0) == '}')
                    break;

                m_builder->start_kv_pair();

                if (!parse())
                    return false;

                m_builder->end_kv_key();

                if (!parse())
                    return false;

                m_builder->end_kv_pair();


                t = m_tok.peek();
            }

            debug_token(t);
            m_builder->end_map();

            m_tok.next();

            return true;
        }

        bool parse_sequence(bool quoted, char end_delim)
        {
            m_tok.next();
            Token t = m_tok.peek();

            debug_token(t);

            m_builder->start_list();
            if (quoted)
                m_builder->atom_symbol("list");

            while (t.m_token_id != TOK_EOF)
            {
                if (t.m_token_id == TOK_CHR && t.nth(0) == end_delim)
                    break;

                if (!parse())
                    return false;

                t = m_tok.peek();
            }

            debug_token(t);
            m_builder->end_list();

            m_tok.next();

            return true;
        }

        bool parse_atom()
        {
            Token t = m_tok.peek();
            debug_token(t);

            switch (t.m_token_id)
            {
                case TOK_DBL:      m_tok.next(); m_builder->atom_dbl(t.m_num.d); break;
                case TOK_INT:      m_tok.next(); m_builder->atom_int(t.m_num.i); break;
                case TOK_CHR:
                    {
                        m_tok.next();

                        if (t.m_text[t.m_text.size() - 1] == ':')
                            m_builder->atom_keyword(
                                t.m_text.substr(0, t.m_text.size() - 1));
                        else if (t.m_text == "#t" || t.m_text == "#true")
                            m_builder->atom_bool(true);
                        else if (t.m_text == "#f" || t.m_text == "#false")
                            m_builder->atom_bool(false);
                        else if (t.m_text == "nil")
                            m_builder->atom_nil();
                        else if (t.m_text == "\"")
                        {
                            t = m_tok.peek();
                            if (t.m_token_id != TOK_STR)
                            {
                                log_error("Expected string body", t);
                                return false;
                            }
                            m_builder->atom_string(t.m_text);
                            m_tok.next();
                            m_tok.next(); // skip end '"'
                        }
                        else
                            m_builder->atom_symbol(t.m_text);
                        break;
                    }
                case TOK_BAD_NUM:
                    log_error("Expected atom", t);
                    return false;
            }
            return true;
        }

        bool parse()
        {
            using namespace std;

            Token t = m_tok.peek();
            debug_token(t);

            switch (t.m_token_id)
            {
                case TOK_EOF: m_tok.next(); m_eof = true; break;
                case TOK_DBL: return parse_atom();        break;
                case TOK_INT: return parse_atom();        break;
                case TOK_STR: return parse_atom();        break;

                case TOK_CHR:
                {
                    switch (t.nth(0))
                    {
                        case '(': return parse_sequence(false, ')'); break;
                        case '[': return parse_sequence(true, ']');  break;
                        case '{': return parse_map(); break;
                        case ')': log_error("unexpected ')'", t); return false;
                        case ']': log_error("unexpected ']'", t); return false;
                        case '}': log_error("unexpected '}'", t); return false;
                        case '\'':
                        {
                            m_tok.next();
                            debug_token(t);
                            m_builder->start_list();
                            m_builder->atom_symbol("quote");
                            bool b = parse();
                            debug_token(t);
                            m_builder->end_list();
                            return b;
                        }
                        default: return parse_atom(); break;
                    }
                }
                case TOK_BAD_NUM:
                    log_error("Bad number found", t);
                    return false;
            }

            return true;
        }
};
//---------------------------------------------------------------------------

}
