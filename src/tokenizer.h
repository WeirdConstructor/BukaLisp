// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#ifndef BUKALISP_TOKENIZER_H
#define BUKALISP_TOKENIZER_H 1
#include "utf8buffer.h"
#include <iostream>
#include <string>
#include <vector>
#include "parse_util.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

enum TokenType
{
    TOK_EOF,
    TOK_CHR,
    TOK_STR,
    TOK_STR_DELIM,
    TOK_DBL,
    TOK_INT,
    TOK_BAD_NUM
};
//---------------------------------------------------------------------------

struct Token
{
    TokenType       m_token_id;
    std::string     m_text;
    int             m_line;
    std::string     m_input_name;
    union {
        double  d;
        int64_t i;
    } m_num;

    Token() : m_line(0), m_token_id(TOK_EOF), m_text("") { }

    Token(double d)  : m_line(0), m_token_id(TOK_DBL) { m_num.d = d; }
    Token(int64_t i) : m_line(0), m_token_id(TOK_INT) { m_num.i = i; }

    Token(char c) : m_line(0), m_token_id(TOK_CHR)
    {
        m_text = std::string(&c, 1);
    }
    Token(const char *csStr) : m_line(0), m_token_id(TOK_CHR)
    {
        m_text = std::string(csStr);
    }
    Token(UTF8Buffer &u8Tmp) : m_line(0), m_token_id(TOK_CHR)
    {
        m_text = u8Tmp.as_string();
    }
    Token(const std::string &sStrBody)
        : m_line(0), m_token_id(TOK_STR)
    {
        m_text = sStrBody;
    }
    Token(TokenType tt, char c)
        : m_line(0), m_token_id(tt), m_text(std::string(&c, 1))
    { }

    char nth(unsigned int i)
    {
        if (m_text.size() <= i) return '\0';
        return m_text[i];
    }

    std::string dump()
    {
        std::string s;

        if (m_token_id == TOK_DBL)
            s = "TOK_DBL=" + std::to_string(m_num.d);
        else if (m_token_id == TOK_INT)
            s = "TOK_INT=" + std::to_string(m_num.i);
        else if (m_token_id == TOK_EOF)
            s = "TOK_EOF";
        else if (m_token_id == TOK_CHR)
            s = "TOK_CHR[" + m_text + "]";
        else if (m_token_id == TOK_STR)
            s = "TOK_STR\"" + m_text + "\"";
        else if (m_token_id == TOK_STR_DELIM)
            s = "TOK_STR_DELIM(" + m_text + ")";
        else
            s = "TOK_BAD_NUM";

        s += "@" + std::to_string(m_line);

        return s;
    }
};
//---------------------------------------------------------------------------

class Tokenizer
{
    private:
        UTF8Buffer           m_u8buf;
        UTF8Buffer           m_u8tmp;
        std::vector<Token>   m_tokens;
        unsigned int         m_token_pos;
        int                  m_cur_line;
        std::string          m_input_name;

    protected:
        void push(Token t)
        {
            t.m_line       = m_cur_line;
            t.m_input_name = m_input_name;
            m_tokens.push_back(t);
        }

    public:
        Tokenizer()
        {
        }

        void reset()
        {
            m_tokens.clear();
        }

        bool checkEOF()
        {
            if (m_u8buf.length() <= 0)
            {
                push(Token());
                return true;
            }
            return false;
        }

        void dump_tokens()
        {
            std::cout << "TOKENS: " << std::endl;
            for (auto t : m_tokens)
            {
                std::cout << "    " << t.dump() << std::endl;
            }
        }

        void tokenize(const std::string &input_name, const std::string &sCode)
        {
            m_tokens.clear();
            m_token_pos  = 0;
            m_cur_line   = 1;
            m_input_name = input_name;

            m_u8buf.reset();
            m_u8buf.append_bytes(sCode.data(), sCode.size());

            while (m_u8buf.length() > 0)
            {
                char c = m_u8buf.first_byte(true);

                if (charClass(c, " ,\t\r\n\v\f"))
                {
                    if (c == '\n') m_cur_line++;
                }
                else if (c == '$' && m_u8buf.match_prefix("define!"))
                {
                    m_u8buf.skip_bytes(7);
                    push(Token("$define!"));
                }
                else if (c == '$' && m_u8buf.match_prefix("^!"))
                {
                    m_u8buf.skip_bytes(2);
                    push(Token("$^!"));
                }
                else if (c == '@' && m_u8buf.match_prefix("^!"))
                {
                    m_u8buf.skip_bytes(2);
                    push(Token("@^!"));
                }
                else if (c == '$' && m_u8buf.first_byte() == '!')
                {
                    m_u8buf.skip_bytes(1);
                    push(Token("$!"));
                }
                else if (c == '@' && m_u8buf.first_byte() == '!')
                {
                    m_u8buf.skip_bytes(1);
                    push(Token("@!"));
                }
                else if (c == '~' && m_u8buf.first_byte() == '@')
                {
                    m_u8buf.skip_bytes(1);
                    push(Token("~@"));
                }
                else if (c == '#' && m_u8buf.first_byte() == ';')
                {
                    m_u8buf.skip_bytes(1);
                    push(Token("#;"));
                }
                else if (c == '#' && m_u8buf.first_byte() == 'q')
                {
                    m_u8buf.skip_bytes(1);
                    c = m_u8buf.first_byte();
                    if (c == '\'')
                    {
                        m_u8buf.skip_bytes(1);
                        push(Token(TOK_STR_DELIM, c));
                        m_u8tmp.reset();

                        while (m_u8buf.length() > 0)
                        {
                            c = m_u8buf.first_byte(true);
                            if (c == '\\')
                            {
                                char peek_c = m_u8buf.first_byte();

                                if (checkEOF()) return;
                                if (peek_c == '\'')
                                    m_u8tmp.append_byte(m_u8buf.first_byte(true));
                                else if (peek_c == '\\')
                                    m_u8tmp.append_byte(m_u8buf.first_byte(true));
                                else
                                {
                                    if (c      == '\n') m_cur_line++;
                                    if (peek_c == '\n') m_cur_line++;

                                    m_u8tmp.append_byte(c);
                                    m_u8tmp.append_byte(m_u8buf.first_byte(true));
                                }
                            }
                            else if (c == '\'')
                            {
                                push(Token(m_u8tmp.as_string()));
                                push(Token(TOK_STR_DELIM, c));
                                break;
                            }
                            else
                            {
                                m_u8tmp.append_byte(c);
                            }
                        }

                        if (checkEOF()) return;
                    }
                    else
                    {
                        push(Token("#q"));
                    }
                }
                else if (c == '#' && m_u8buf.first_byte() == '|')
                {
                    m_u8buf.skip_bytes(1);

                    c = m_u8buf.first_byte();
                    if (c == '\n') m_cur_line++;

                    int nest = 1;
                    while (nest > 0 && m_u8buf.length() > 0)
                    {
                        while (m_u8buf.length() > 0 && c != '|' && c != '#')
                        {
                            m_u8buf.skip_bytes(1);
                            c = m_u8buf.first_byte();
                            if (c == '\n') m_cur_line++;
                        }

                        if (c == '|')
                        {
                            m_u8buf.skip_bytes(1);
                            c = m_u8buf.first_byte();

                            if (c == '#')
                            {
                                m_u8buf.skip_bytes(1);
                                nest--;
                                continue;
                            }

                        }
                        else if (c == '#')
                        {
                            m_u8buf.skip_bytes(1);
                            c = m_u8buf.first_byte();

                            if (c == '|')
                            {
                                m_u8buf.skip_bytes(1);
                                nest++;
                                continue;
                            }
                        }

                        if (c == '\n') m_cur_line++;
                    }
                }
                else if (charClass(c, "[]{}()'`~^@$."))
                {
                    push(Token(c));
                }
                else if (c == '"')
                {
                    push(Token(TOK_STR_DELIM, c));
                    m_u8tmp.reset();

                    while (m_u8buf.length() > 0)
                    {
                        c = m_u8buf.first_byte(true);
                        if (c == '\\')
                        {
                            char peek_c = m_u8buf.first_byte();

                            if (checkEOF()) return;
                            if (charClass(peek_c, "\\\""))
                                m_u8tmp.append_byte(m_u8buf.first_byte(true));
                            else if (peek_c == 'r')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('\r'); }
                            else if (peek_c == 'n')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('\n'); }
                            else if (peek_c == 'v')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('\v'); }
                            else if (peek_c == 'f')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('\f'); }
                            else if (peek_c == 'a')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('\a'); }
                            else if (peek_c == 'b')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('\b'); }
                            else if (peek_c == 't')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('\t'); }
                            else if (peek_c == '|')
                            { m_u8buf.skip_bytes(1); m_u8tmp.append_byte('|'); }
                            else if (peek_c == ' ' || peek_c == '\t' || peek_c == '\n' || peek_c == '\r')
                            {
                                while (peek_c == ' ' || peek_c == '\t' || peek_c == '\n' || peek_c == '\r')
                                {
                                    m_u8buf.skip_bytes(1);
                                    peek_c = m_u8buf.first_byte();
                                    if (peek_c == '\n')
                                    {
                                        m_u8buf.skip_bytes(1);
                                        m_cur_line++;
                                    }
                                    else if (peek_c == '\r')
                                    {
                                        m_u8buf.skip_bytes(1);
                                        peek_c = m_u8buf.first_byte();
                                        if (peek_c == '\n')
                                        {
                                            m_u8buf.skip_bytes(1);
                                            peek_c = m_u8buf.first_byte();
                                        }
                                        m_cur_line++;
                                    }
                                }
                            }
                            else if (peek_c == 'x' || peek_c == 'u')
                            {
                                bool is_unicode = peek_c == 'u';
                                m_u8buf.skip_bytes(1);
                                peek_c = m_u8buf.first_byte();

                                std::string hex_chr = "0x";

                                while (peek_c != ';'
                                       && charClass(peek_c, "0123456789ABCDEFabcdef"))
                                {
                                    m_u8buf.skip_bytes(1);
                                    hex_chr += peek_c;
                                    peek_c = m_u8buf.first_byte();
                                    if (checkEOF()) return;
                                }
                                m_u8buf.skip_bytes(1);

                                try
                                {
                                    if (is_unicode)
                                        m_u8tmp.append_unicode(
                                            std::stol(hex_chr, 0, 16));
                                    else
                                        m_u8tmp.append_byte(
                                            std::stoi(hex_chr, 0, 16));
                                }
                                catch (std::exception &)
                                {
                                    m_u8tmp.append_byte('?');
                                }
                            }
                            else
                            {
                                m_u8tmp.append_byte(c);
                                c = m_u8buf.first_byte(true);
                                if (c == '\n') m_cur_line++;
                                m_u8tmp.append_byte(c);
                            }
                        }
                        else if (c == '"')
                        {
                            push(Token(m_u8tmp.as_string()));
                            push(Token(TOK_STR_DELIM, c));
                            break;
                        }
                        else
                        {
                            if (c == '\n') m_cur_line++;
                            m_u8tmp.append_byte(c);
                        }
                    }

                    if (checkEOF()) return;
                }
                else if (c == ';')
                {
                    while (m_u8buf.length() > 0
                           && m_u8buf.first_byte(true) != '\n')
                        ;

                    if (checkEOF()) return;

                    m_cur_line++;
                }
                else
                {
                    m_u8tmp.reset();
                    m_u8tmp.append_byte(c);

                    while (m_u8buf.length() > 0)
                    {
                        char peekC = m_u8buf.first_byte();
                        if (charClass(peekC, " \t\r\n\v\f[]{}()'\"`,;"))
                            break;

                        m_u8tmp.append_byte(m_u8buf.first_byte(true));
                    }

                    if (m_u8tmp.length() > 0)
                        pushAtom(m_u8tmp);

                    if (checkEOF()) return;
                }
            }

            checkEOF();
        }

        Token peek()
        {
            if (m_tokens.size() <= m_token_pos) return Token();
            return m_tokens[m_token_pos];
        }
        Token next()
        {
            if (m_tokens.size() <= m_token_pos) return Token();
            return m_tokens[m_token_pos++];
        }

        void pushAtom(UTF8Buffer &u8)
        {
            std::string s(u8.as_string());
            if (s == "->")
            {
                push(u8);
                return;
            }

            UTF8Buffer u8P(u8);
            if (u8P.length() == 1 && charClass(u8P.first_byte(), "+-"))
            {
                push(Token(u8));
                return;
            }

            bool    bIsDouble   = false;
            bool    bBadNumber  = false;
            double  dVal        = 0;
            int64_t iVal        = 0;
            if (u8BufParseNumber(u8P, dVal, iVal, bIsDouble, bBadNumber))
            {
                if (bBadNumber)
                {
                    Token t;
                    t.m_token_id = TOK_BAD_NUM;
                    push(t);
                }
                else
                {
                    if (bIsDouble)
                        push(Token(dVal));
                    else
                        push(Token(iVal));
                }
            }
            else
            {
                std::string s(u8.as_string());
                size_t pos = s.find("->");
                if (pos != std::string::npos)
                {
                    Token t("->");
                    t.m_token_id = TOK_CHR;
                    push(t);
                    push(Token(s.substr(0, pos).c_str()));
                    push(Token(s.substr(pos + 2, s.size() - (pos + 2)).c_str()));
                }
                else
                {
                    push(Token(u8));
                }
            }
        }

        ~Tokenizer()
        {
        }
};

} // namespace bukalisp

#endif // BUKALISP_TOKENIZER_H

/******************************************************************************
* Copyright (C) 2016-2017 Weird Constructor
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

