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

#include "vval_util.h"
#include "JSON.h"
#include <iostream>
#include <algorithm>
#include <string>

using namespace std;

//---------------------------------------------------------------------------

namespace VVal
{
//---------------------------------------------------------------------------

std::string VariantValue::s_hex() const
{
    std::string str = this->s();
    std::stringstream ss;
    for (int i = 0; i < str.size(); i++)
    {
        char buf[10];
        snprintf(buf, 10, "%02X", (unsigned char) str[i]);
        ss << buf;
    }
    return ss.str();
}
//---------------------------------------------------------------------------

void as_json(const VV &value, json::Serializer &ser)
{
    if      (value->is_undef())     ser.null();
    else if (value->is_boolean())   ser.boolean(value->is_true());
    else if (value->is_int())       ser.number(value->i());
    else if (value->is_double())    ser.number(value->d());
    else if (value->is_string())
    {
        try
        { ser.string(value->s()); }
        catch (UTF8Buffer_exception &)
        { ser.string(value->s_hex()); }
    }
    else if (value->is_bytes())
    {
        ser.string(value->s_hex());
    }
    else if (value->is_list())
    {
        ser.arrayStart();
        for (auto i : *value)
            as_json(i, ser);
        ser.arrayEnd();
    }
    else if (value->is_map())
    {
        ser.objectStart();
        for (auto i : *value)
        {
            ser.objectKey(i->_s(0));
            as_json(i->_(1), ser);
        }
        ser.objectEnd();
    }
    else
    {
        ser.string(value->s());
    }
}
//---------------------------------------------------------------------------

string as_json(const VV &value, bool bIndent)
{
    json::Serializer ser(bIndent);
    as_json(value, ser);
    return ser.asString();
}
//---------------------------------------------------------------------------

class VVJSONParser : public json::Parser
{
    private:
        UTF8Buffer      *m_u8b;
        VV               m_stack;
        VV               m_obj_stack;
        VV               m_obj_key_stack;
        VV               m_cur_obj;
        string           m_cur_key;

    public:
        string      m_error;

        VVJSONParser(const string &json)
        {
            m_stack         = vv_list();
            m_obj_stack     = vv_list();
            m_obj_key_stack = vv_list();
            m_cur_obj       = vv_undef();
            m_u8b           = new UTF8Buffer(json.data(), json.size());
        }

        VV parseVV()
        {
            if (this->parse(m_u8b))
            {
                if (m_error != "") return vv_undef();
                return m_stack->pop();
            }
            else return vv_undef();
        }

        virtual ~VVJSONParser()
        {
            delete m_u8b;
        }

        virtual void onObjectStart()
        {
            m_obj_stack->push(m_cur_obj);
            m_obj_key_stack->push(vv(m_cur_key));
            m_cur_obj = vv_map();
        }
        virtual void onObjectKey(const string &sOut) { m_cur_key = sOut; }
        virtual void onObjectValueDone()
        {
            m_cur_obj << vv_kv(m_cur_key, m_stack->pop());
        }
        virtual void onObjectEnd()
        {
            m_stack->push(m_cur_obj);
            m_cur_obj = m_obj_stack->pop();
            m_cur_key = m_obj_key_stack->pop()->s();
        }

        virtual void onArrayStart()
        {
            m_obj_stack->push(m_cur_obj);
            m_cur_obj = vv_list();
        }
        virtual void onArrayValueDone()
        {
            m_cur_obj->push(m_stack->pop());
        }
        virtual void onArrayEnd()
        {
            m_stack->push(m_cur_obj);
            m_cur_obj = m_obj_stack->pop();
        }
        virtual void onError(UTF8Buffer *u8Buf, const char *csError)
        { m_error = string(csError); }

        virtual void onValueBoolean(bool bValue) { m_stack->push(vv_bool(bValue)); }
        virtual void onValueNull() { m_stack->push(vv_undef()); }
        virtual void onValueNumber(const char *csNumber, bool bIsFloat)
        {
            VV tmp(vv(string(csNumber)));
            if (bIsFloat) m_stack->push(vv(tmp->d()));
            else          m_stack->push(vv(tmp->i()));

        }
        virtual void onValueString(const string &sString) { m_stack->push(vv(sString)); }
};
//---------------------------------------------------------------------------

VV from_json(const string &json)
{
    VVJSONParser p(json);
    return p.parseVV();
}
//---------------------------------------------------------------------------

std::string to_lower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}
//---------------------------------------------------------------------------

};
