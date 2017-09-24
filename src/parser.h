// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

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

        virtual void error(const std::string &what,
                           const std::string &inp_name,
                           size_t line,
                           const std::string &tok) = 0;

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
        bool        m_builder_enabled;
#       define      M_BUILDER(X)   do { if (m_builder_enabled) { m_builder->X; } } while(0)

    public:
        Parser(Tokenizer &tok, SEX_Builder *builder)
            : m_tok(tok), m_eof(false), m_builder(builder),
              m_builder_enabled(true)
        {
        }

        void reset() { m_eof = false; m_builder_enabled = true; }

        bool is_eof() { return m_eof; }

        void log_error(const std::string &what, Token &t)
        {
            M_BUILDER(error(what, t.m_input_name, t.m_line, t.dump()));
        }

        void debug_token(Token &t)
        {
            M_BUILDER(set_debug_info(t.m_input_name, t.m_line));
        }

        bool parse_map()
        {
            Token start_t = m_tok.peek();
            m_tok.next();
            if (!skip_comments())
                return false;
            Token t = m_tok.peek();

            debug_token(t);
            M_BUILDER(start_map());

            while (t.m_token_id != TOK_EOF)
            {
                if (t.m_token_id == TOK_CHR && t.nth(0) == '}')
                    break;

                debug_token(t);
                M_BUILDER(start_kv_pair());

                if (!parse())
                    return false;

                debug_token(t);
                M_BUILDER(end_kv_key());

                if (!parse())
                    return false;

                debug_token(t);
                M_BUILDER(end_kv_pair());

                if (!skip_comments())
                    return false;
                t = m_tok.peek();
            }

            if (t.m_token_id == TOK_EOF)
            {
                log_error(
                    "Couldn't find matching delimiter: '}'",
                    start_t);
            }

            debug_token(t);
            M_BUILDER(end_map());

            m_tok.next();
            if (!skip_comments())
                return false;

            return true;
        }

        bool parse_sequence(bool quoted, char end_delim)
        {
            Token start_t = m_tok.peek();
            m_tok.next();
            if (!skip_comments())
                return false;
            Token t = m_tok.peek();

            debug_token(t);

            M_BUILDER(start_list());
            if (quoted)
                M_BUILDER(atom_symbol("list"));

            while (t.m_token_id != TOK_EOF)
            {
                if (t.m_token_id == TOK_CHR && t.nth(0) == end_delim)
                    break;

                debug_token(t);
                if (!parse())
                    return false;

                if (!skip_comments())
                    return false;
                t = m_tok.peek();
            }

            if (t.m_token_id == TOK_EOF)
            {
                log_error(
                    std::string("Couldn't find matching delimiter: '")
                    + end_delim + "'",
                    start_t);
            }

            debug_token(t);
            M_BUILDER(end_list());

            m_tok.next();
            if (!skip_comments())
                return false;

            return true;
        }

        bool parse_atom()
        {
            Token t = m_tok.peek();
            debug_token(t);

            switch (t.m_token_id)
            {
                case TOK_DBL:      m_tok.next(); M_BUILDER(atom_dbl(t.m_num.d)); break;
                case TOK_INT:      m_tok.next(); M_BUILDER(atom_int(t.m_num.i)); break;
                case TOK_CHR:
                    {
                        m_tok.next();

                        if (t.m_text[t.m_text.size() - 1] == ':')
                            m_builder->atom_keyword(
                                t.m_text.substr(0, t.m_text.size() - 1));
                        else if (t.m_text == "#t" || t.m_text == "#true")
                            M_BUILDER(atom_bool(true));
                        else if (t.m_text == "#f" || t.m_text == "#false")
                            M_BUILDER(atom_bool(false));
                        else if (t.m_text == "nil")
                            M_BUILDER(atom_nil());
                        else
                            M_BUILDER(atom_symbol(t.m_text));
                        break;
                    }
                case TOK_STR_DELIM:
                    {
                        m_tok.next();
                        t = m_tok.peek();
                        if (t.m_token_id != TOK_STR)
                        {
                            log_error("Expected string body", t);
                            return false;
                        }
                        M_BUILDER(atom_string(t.m_text));
                        m_tok.next();
                        m_tok.next(); // skip end TOK_STR_DELIM
                        break;
                    }
                case TOK_BAD_NUM:
                    log_error("Expected atom", t);
                    return false;
            }
            return true;
        }

        bool skip_comments()
        {
            Token t = m_tok.peek();

            if (t.m_token_id == TOK_CHR && t.m_text == "#;")
            {
                m_tok.next();
                m_builder_enabled = false;
                parse();
                m_builder_enabled = true;
                return true;
            }

            return true;
        }

        bool parse()
        {
            using namespace std;

            if (!skip_comments())
                return false;

            Token t = m_tok.peek();
            debug_token(t);

            switch (t.m_token_id)
            {
                case TOK_EOF: m_tok.next(); m_eof = true; break;
                case TOK_DBL:       return parse_atom();  break;
                case TOK_INT:       return parse_atom();  break;
                case TOK_STR:       return parse_atom();  break;
                case TOK_STR_DELIM: return parse_atom();  break;

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
                            M_BUILDER(start_list());
                            M_BUILDER(atom_symbol("quote"));
                            bool b = parse();
                            debug_token(t);
                            M_BUILDER(end_list());
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
#undef M_BUILDER
};
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
