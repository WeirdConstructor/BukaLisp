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

#ifndef JSON_H
#define JSON_H 1
#include <string>
#include <vector>
#include <stack>
#include <stdexcept>
#include "utf8buffer.h"
#ifdef __BCPLUSPLUS__
#include <vcl.h>
#endif

namespace json
{

using namespace std;

class Serializer
{
    private:
        struct state
        {
            bool m_bFirstInCollection;
            bool m_bFollowsKey;
            int  m_iIndent;
            UTF8Buffer *m_u8Buf;

            state() : m_bFirstInCollection(true), m_bFollowsKey(false), m_iIndent(0), m_u8Buf(0) { }
            state(UTF8Buffer *u8Buf) : m_bFirstInCollection(true), m_bFollowsKey(false), m_iIndent(0), m_u8Buf(u8Buf) { }

            void startValue()
            {
                if (!m_bFirstInCollection && !m_bFollowsKey)
                {
                    if (m_u8Buf) m_u8Buf->append_ascii(',');
                }

                indent();

                m_bFirstInCollection = false;
                m_bFollowsKey        = false;
            }

            void endCollection()
            {
                if (m_iIndent > 0)
                    m_iIndent--;
                if (!m_bFirstInCollection)
                    indent();
            }

            void indent()
            {
                if (!m_u8Buf)
                    return;
                if (m_iIndent < 0)
                    return;

                if (m_bFollowsKey)
                {
                    m_u8Buf->append_ascii(' ');
                    return;
                }

                if (m_iIndent > 0 || !m_bFirstInCollection)
                {
                    m_u8Buf->append_ascii('\n');
                }

                for (int i = 0; i < m_iIndent; i++)
                    m_u8Buf->append_bytes("  ", 2);
            }
        };
        bool m_bDoIndent;
        UTF8Buffer m_u8Buf;

        std::stack<state> m_States;

        void startNewListing()
        {
            int iIndent = m_States.empty() ? 0 : m_States.top().m_iIndent + 1;
            m_States.push(state(&m_u8Buf));
            m_States.top().m_iIndent = m_bDoIndent ? iIndent : -1;
        }

    public:
        Serializer()
            : m_bDoIndent(false)
        { startNewListing(); }
        Serializer(bool bDoIndent)
            : m_bDoIndent(bDoIndent)
        { startNewListing(); }

        std::string asString() { return m_u8Buf.as_string(); }

        void boolean(bool bValue)
        {
            m_States.top().startValue();
            if (bValue) m_u8Buf.append_bytes("true", 4);
            else        m_u8Buf.append_bytes("false", 5);
        }

        void number(double dValue)
        {
            char csBuf[1024];
            int  iLen = snprintf (csBuf, 1024, "%.10g", dValue);

            // FIXME: Hack!
            for (int i = 0; i < iLen; i++)
                if (csBuf[i] == ',') csBuf[i] = '.';

            m_States.top().startValue();
            m_u8Buf.append_bytes(csBuf, iLen);
        }

//        void number(int iValue) { this->number((long) iValue); }

#ifdef __BCPLUSPLUS__
        void number(__int64 lValue)
        {
            AnsiString aslValue = IntToStr((__int64) lValue);
            m_States.top().startValue();
            m_u8Buf.append_bytes (aslValue.c_str(), aslValue.Length());
        }
#else
        void number(int64_t lValue)
        {
            char csBuf[1024];
            int  iLen = snprintf (csBuf, 1024, "%" PRIi64, lValue);
            m_States.top().startValue();
            m_u8Buf.append_bytes(csBuf, iLen);
        }
#endif

//        void number(long lValue)
//        {
//            char csBuf[1024];
//            int iLen = snprintf (csBuf, 1024, "%ld", lValue);
//
//            m_States.top().startValue();
//            m_u8Buf.append_bytes (csBuf, iLen);
//        }

        void string(const std::string &sValue)
        {
            UTF8Buffer u8Str(sValue.data(), sValue.size());
            UTF8Buffer outStr;
            m_States.top().startValue();
            try
            {
                u8Str.dump_as_json_string(outStr);
                m_u8Buf.append_bytes(outStr);
            }
            catch (UTF8Buffer_exception &)
            {
                std::string hd = u8Str.hexdump();
                m_u8Buf.append_bytes(hd.c_str(), hd.length());
            }
        }

        void null()
        {
            m_States.top().startValue();
            m_u8Buf.append_bytes("null", 4);
        }

        void arrayStart()
        {
            m_States.top().startValue();
            m_u8Buf.append_ascii('[');
            startNewListing();
        }
        void arrayEnd()
        {
            m_States.top().endCollection();
            m_States.pop();
            m_u8Buf.append_ascii(']');
        }

        void objectStart()
        {
            m_States.top().startValue();
            m_u8Buf.append_ascii('{');
            startNewListing();
        }
        void objectKey(const std::string &sKey)
        {
            m_States.top().startValue();

            UTF8Buffer u8Str(sKey.data(), sKey.size());
            u8Str.dump_as_json_string(m_u8Buf);
            m_u8Buf.append_ascii(':');

            m_States.top().m_bFollowsKey = true;
        }
        void objectEnd()
        {
            m_States.top().endCollection();
            m_States.pop();
            m_u8Buf.append_ascii('}');
        }
};

class Parser
{
  private:
    UTF8Buffer *m_u8Buf;

    bool parseJSON();
    bool parseJSONValue();
    bool parseJSONArray();
    bool parseJSONObject();
    bool parseJSONString(std::string &sOut);
    bool parseHEXLiteral(long &lOut);
    bool parseJSONNumber();
    bool parseJSONKeyword();

  public:
    Parser() : m_u8Buf(0) {}
    virtual ~Parser() { }

    bool parse(UTF8Buffer *chb);

    inline void setBuffer(UTF8Buffer *b) { m_u8Buf = b; }

    virtual void onObjectStart()                      {}
    virtual void onObjectKey(const std::string &sOut) { (void) sOut; }
    virtual void onObjectValueDone()                  { }
    virtual void onObjectEnd()                        {}

    virtual void onArrayStart()                       {}
    virtual void onArrayValueDone()                   {}
    virtual void onArrayEnd()                         {}
    virtual void onError(UTF8Buffer *u8Buf, const char *csError) { (void) u8Buf; (void) csError; }

    virtual void onValueBoolean(bool bValue) { (void) bValue; }
    virtual void onValueNull()               {}
    virtual void onValueNumber(const char *csNumber, bool bIsFloat) { (void) csNumber; (void) bIsFloat; }
    virtual void onValueString(const std::string &sString)          { (void) sString; }
};

} // namespace json
#endif // JSON_H
