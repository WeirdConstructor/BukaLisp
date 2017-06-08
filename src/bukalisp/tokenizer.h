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

#ifndef BUKALISP_TOKENIZER_H
#define BUKALISP_TOKENIZER_H 1
#include "utf8buffer.h"
#include <string>
#include <vector>
#include "parse_util.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

enum TokenType
{
    T_EOF,
    T_CHR_TOK,
    T_STR_BODY,
    T_DBL,
    T_INT,
    T_BAD_NUM
};
//---------------------------------------------------------------------------

struct Token
{
    TokenType       m_token_id;
    std::string     m_text;
    int             m_line;
    union {
        double  d;
        int64_t i;
    } m_num;


    Token() : m_line(0), m_token_id(T_EOF), m_text("") { }

    Token(double d)  : m_line(0), m_token_id(T_DBL) { m_num.d = d; }
    Token(int64_t i) : m_line(0), m_token_id(T_INT) { m_num.i = i; }

    Token(char c) : m_line(0), m_token_id(T_CHR_TOK)
    {
        m_text = std::string(&c, 1);
    }
    Token(const char *csStr) : m_line(0), m_token_id(T_CHR_TOK)
    {
        m_text = std::string(csStr);
    }
    Token(UTF8Buffer &u8Tmp) : m_line(0), m_token_id(T_CHR_TOK)
    {
        m_text = u8Tmp.as_string();
    }
    Token(const std::string &sStrBody)
        : m_line(0), m_token_id(T_STR_BODY)
    {
        m_text = sStrBody;
    }

    char nth(unsigned int i)
    {
        if (m_text.size() <= i) return '\0';
        return m_text[i];
    }

    std::string dump()
    {
        std::string s;

        if (m_token_id == T_DBL)
            s = "T_DBL=" + std::to_string(m_num.d);
        else if (m_token_id == T_INT)
            s = "T_INT=" + std::to_string(m_num.i);
        else if (m_token_id == T_EOF)
            s = "T_EOF";
        else if (m_token_id == T_CHR_TOK)
            s = "T_CHR_TOK[" + m_text + "]";
        else if (m_token_id == T_STR_BODY)
            s = "T_STR_BODY\"" + m_text + "\"";
        else
            s = "T_BAD_NUM";

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

    protected:
        void push(Token t)
        {
            t.m_line = m_cur_line;
            m_tokens.push_back(t);
        }

    public:
        Tokenizer()
        {
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

        void tokenize(const std::string &sCode)
        {
            m_tokens.clear();
            m_token_pos = 0;
            m_cur_line  = 1;

            m_u8buf.reset();
            m_u8buf.append_bytes(sCode.data(), sCode.size());

            while (m_u8buf.length() > 0)
            {
                char c = m_u8buf.first_byte(true);

                if (charClass(c, " ,\t\r\n\v\f"))
                {
                    if (c == '\n') m_cur_line++;
                }
                else if (c == '~' && m_u8buf.first_byte() == '@')
                {
                    m_u8buf.skip_bytes(1);
                    push(Token("~@"));
                }
                else if (charClass(c, "[]{}()'`~^@"))
                {
                    push(Token(c));
                }
                else if (c == '"')
                {
                    push(Token(c));
                    m_u8tmp.reset();

                    while (m_u8buf.length() > 0)
                    {
                        c = m_u8buf.first_byte(true);
                        if (c == '\\')
                        {
                            if (checkEOF()) return;
                            if (charClass(m_u8buf.first_byte(), "\\\""))
                                m_u8tmp.append_byte(m_u8buf.first_byte(true));
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
                            push(Token(c));
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
                    t.m_token_id = T_BAD_NUM;
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
                push(Token(u8));
        }

        ~Tokenizer()
        {
        }
};

} // namespace bukalisp

#endif // BUKALISP_TOKENIZER_H
