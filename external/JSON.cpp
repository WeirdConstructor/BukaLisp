/******************************************************************************
* Copyright (C) 2007-2017 Weird Constructor
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

#include <iostream>
#include <cstdio>

#include "JSON.h"

namespace json
{

using namespace std;

// ### Parser ####

bool Parser::parse(UTF8Buffer *chb)
{
  m_u8Buf = chb;
  bool bRes = false;

  try
  {
      bRes = parseJSON();
  }
  catch (UTF8Buffer_exception &e)
  {
      (void) e;
      this->onError(m_u8Buf, "UTF-8 Encoding Error");
  }

  return bRes;
}

// ### Parser #### private

bool Parser::parseJSON()
{
    m_u8Buf->skip_json_ws();

    char next = m_u8Buf->first_ascii();

    if (next == '{')
        return parseJSONObject();
    else if (next == '[')
        return parseJSONArray();
    else
    {
        this->onError(m_u8Buf, "json: unexpected character. "
                               "expected JSON object or array");
        return false;
    }
}

bool Parser::parseJSONValue ()
{
    char tok = m_u8Buf->next_json_token();
    string sValue;

    switch (tok)
    {
        case '"':
            {
                if (!parseJSONString(sValue)) return false;
                this->onValueString(sValue);
                return true;
            }
        case '[': return parseJSONArray();
        case '{': return parseJSONObject();
        case 'f':
        case 't':
        case 'n':
            return parseJSONKeyword();
        default:
            if (!m_u8Buf->first_is_ascii())
                return false;
            return parseJSONNumber();
    }

    if (!m_u8Buf->first_is_ascii())
        return false;
    return parseJSONNumber();
}

bool Parser::parseJSONArray ()
{
    if (m_u8Buf->next_json_token(true) != '[')
    {
        this->onError(m_u8Buf, "array: expected '['");
        return false;
    }

    this->onArrayStart();

    if (m_u8Buf->next_json_token() != ']')
    {
        if (!parseJSONValue()) return false;
        this->onArrayValueDone();

        while (m_u8Buf->next_json_token() == ',')
        {
            m_u8Buf->skip_first();
            if (!parseJSONValue()) return false;
            this->onArrayValueDone();
        }
    }

    if (m_u8Buf->next_json_token(true) != ']')
    {
        this->onError(m_u8Buf, "array: expected ']'");
        return false;
    }

    this->onArrayEnd();

    return true;
}

bool Parser::parseJSONObject()
{
    if (m_u8Buf->next_json_token(true) != '{')
    {
        this->onError(m_u8Buf, "object: expected  '{'");
        return false;
    }

    bool bInObject = true;
    this->onObjectStart();

    if (m_u8Buf->next_json_token() != '}')
        while (bInObject)
        {
            string sKey;
            if (!parseJSONString(sKey)) return false;

            this->onObjectKey(sKey);

            if (m_u8Buf->next_json_token(true) != ':')
            {
                this->onError(m_u8Buf, "object: expected ':'");
                return false;
            }

            if (!parseJSONValue()) return false;
            this->onObjectValueDone();

            if (m_u8Buf->next_json_token() != ',')
                bInObject = false;
            else
                m_u8Buf->skip_first();
        }

    if (m_u8Buf->next_json_token(true) != '}')
    {
        this->onError(m_u8Buf, "object: expected '}'");
        return false;
    }

    this->onObjectEnd();

    return true;
}

bool Parser::parseJSONString(std::string &sOut)
{
    if (m_u8Buf->next_json_token(true) != '"')
    {
        this->onError(m_u8Buf, "string: expected '\"'");
        return "";
    }

    long lHexLitTmp = 0;
    bool bInString  = true;
    UTF8Buffer u8String;

    while (bInString)
    {
        if (m_u8Buf->first_is_ascii())
        {
            unsigned char chr = m_u8Buf->first_ascii();
            switch (chr)
            {
                case '"':
                    m_u8Buf->skip_first();
                    bInString = false;
                    continue;

                case '\\':
                    m_u8Buf->skip_first();
                    if (m_u8Buf->first_is_ascii())
                    {
                        chr = m_u8Buf->first_ascii();
                        switch (chr)
                        {
                            case '"':  chr = '"';    break;
                            case '\\': chr = '\\';   break;
                            case '/':  chr = '/';    break;
                            case 'b':  chr = '\x08'; break;
                            case 'f':  chr = '\x0C'; break;
                            case 'n':  chr = '\x0A'; break;
                            case 'r':  chr = '\x0D'; break;
                            case 't':  chr = '\x09'; break;
                            case 'u':
                                if (!parseHEXLiteral(lHexLitTmp)) return false;
                                u8String.append_unicode(lHexLitTmp);
                                continue;
                            default:
                                this->onError(m_u8Buf, "string: unallowed escaped in string");
                                return false;
                        }
                        u8String.append_ascii(chr);
                        m_u8Buf->skip_first();
                        continue;
                    }
                    break;

                default:
                    if (!((chr >= '\x20' && chr <= '\x21')
                          || (chr >= '\x23' && chr <= '\x5B')
                          || (chr >= '\x5D')))
                    {
                        this->onError(m_u8Buf, "string: illegal character found");
                        return false;
                    }
                    break;
            }
        }

        u8String.append_first_of(*m_u8Buf);
        m_u8Buf->skip_first();
    }

    sOut = u8String.as_string();

    return true;
}

bool Parser::parseHEXLiteral (long &lOut)
{
    if (m_u8Buf->first_ascii (true) != 'u')
    {
        this->onError(m_u8Buf, "hexliteral: expected 'u'");
        return false;
    }

    long l = 0;

    for (int i = 4; i > 0; i--)
    {
        if (!m_u8Buf->first_is_ascii ())
        {
            this->onError(m_u8Buf, "hexliteral: contained non-ascii-char");
            return false;
        }

        char h = m_u8Buf->first_ascii (true);
        int fact = 0;

        if (h >= '\x30' && h <= '\x39')
            fact = h - 0x30;
        else if (h >= '\x41' && h <= '\x46')
            fact = (h - 0x41) + 10;
        else if (h >= '\x61' && h <= '\x66')
            fact = (h - 0x61) + 10;
        else
        {
            this->onError(m_u8Buf, "hexliteral: invalid hex-character");
            return false;
        }

        //d// cout << i << ":" << (1 << (4 * (i - 1))) << " * "
        //d//      << fact << "(" << h << ")" << endl;

        l += (1 << (4 * (i - 1))) * fact;
    }

    if (l >= 0xD800 && l <= 0xDBFF)
    {
        if (m_u8Buf->first_ascii (true) != '\\')
        {
            this->onError(m_u8Buf, "hexliteral: surrogate-pair incomplete");
            return false;
        }

        long l2 = 0;
        if (!parseHEXLiteral(l2)) return false;

        if (!(l2 >= 0xDC00 && l2 <= 0xDFFF))
        {
            this->onError(m_u8Buf, "hexliteral: invalid surrogate-pair");
            return false;
        }

        // thanks to: http://www.unicode.org/faq/utf_bom.html#35
        // const long lead_offs        = 0xD800L - (0x10000L >> 10);
        const long surrogate_offset = 0x10000L - (0xD800L << 10) - 0xDC00L;
        l = (l << 10) + l2 + surrogate_offset;
    }

    lOut = l;

    return true;
}

bool Parser::parseJSONNumber()
{
    m_u8Buf->skip_json_ws();
    char numbuf[1024];
    int nb_pos = 0;

    // [minus]
    if (m_u8Buf->first_ascii() == '-')
    {
        numbuf[nb_pos++] = '-';
        m_u8Buf->skip_first();
    }

    // int
    char i = m_u8Buf->first_ascii(true);
    if (i == '0')
    {
        numbuf[nb_pos++] = '0';
    }
    else if (i >= '1' && i <= '9')
    {
        numbuf[nb_pos++] = i;

        i = m_u8Buf->first_ascii();
        while (i >= '0' && i <= '9')
        {
            numbuf[nb_pos++] = i;
            m_u8Buf->skip_first();
            i = m_u8Buf->first_ascii();
        }
    }
    else
    {
        onError(m_u8Buf, "number: parsing integer part");
        return false;
    }

    // [frac]
    bool has_frac = false;
    if (m_u8Buf->first_ascii() == '.')
    {
        has_frac = true;

        numbuf[nb_pos++] = '.';
        m_u8Buf->skip_first();

        i = m_u8Buf->first_ascii();
        if (!(i >= '0' && i <= '9'))
        {
            this->onError(m_u8Buf, "number: expected '0'-'9'");
            return false;
        }

        while (i >= '0' && i <= '9')
        {
            m_u8Buf->skip_first();
            numbuf[nb_pos++] = i;
            i = m_u8Buf->first_ascii();
        }
    }

    // [exp]
    bool has_exp = false;
    if (m_u8Buf->first_ascii() == 'e' || m_u8Buf->first_ascii() == 'E')
    {
        has_exp = true;

        m_u8Buf->skip_first();
        numbuf[nb_pos++] = 'e';

        i = m_u8Buf->first_ascii();
        if (i == '+' || i == '-')
        {
            m_u8Buf->skip_first();
            numbuf[nb_pos++] = i;
        }

        i = m_u8Buf->first_ascii();
        if (i >= '0' && i <= '9')
        {
            while (i >= '0' && i <= '9')
            {
                m_u8Buf->skip_first();
                numbuf[nb_pos++] = i;
                i = m_u8Buf->first_ascii();
            }
        }
        else
        {
            this->onError(m_u8Buf, "number: expected '0'-'9'");
            return false;
        }
    }

    numbuf[nb_pos] = '\0';

    if (!(has_frac || has_exp))
        this->onValueNumber(numbuf, false);
    else
        this->onValueNumber(numbuf, true);

    return true;
}

bool Parser::parseJSONKeyword()
{
    char        t       = m_u8Buf->next_json_token();
    const char *expect  = "";

    if (t == 't')
    {
        this->onValueBoolean(true);
        expect = "true";
    }
    else if (t == 'f')
    {
        this->onValueBoolean(false);
        expect = "false";
    }
    else if (t == 'n')
    {
        this->onValueNull();
        expect = "null";
    }
    else
    {
        this->onError(m_u8Buf, "keyword: unexpected keyword found");
        return false;
    }

    while (*expect)
    {
        if (m_u8Buf->next_json_token(true) != *expect)
        {
            this->onError(m_u8Buf, "keyword: unexpected keyword found");
            return false;
        }

        expect++;
    }

    return true;
}

} // namespace json
