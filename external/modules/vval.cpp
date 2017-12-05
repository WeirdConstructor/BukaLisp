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

#include "vval.h"
#include <string>
#include <locale>
#include <algorithm>
#include <iomanip>
#include <sstream>

#ifdef BZVC
#    include "vval_util.h"
#endif

namespace VVal
{
//---------------------------------------------------------------------------

VV g_vv_undef = VV(new VariantValue);

//---------------------------------------------------------------------------

VV vv(int64_t v)        { return VV(new IntegerValue(v)); }
VV vv(int32_t v)        { return VV(new IntegerValue(v)); }
VV vv(int16_t v)        { return VV(new IntegerValue(v)); }
VV vv(double v)         { return VV(new DoubleValue(v)); }
VV vv(const std::string &v)        { return VV(new StringValue(v)); }
VV vv_kw(const std::string &v)     { return VV(new StringValue(v, 'k')); }
VV vv_sym(const std::string &v)    { return VV(new StringValue(v, 'y')); }
VV vv(const std::wstring &v)   { VV rv(new StringValue("")); rv->s_set(v); return rv; }
VV vv_dt(std::time_t v)  { return VV(new DateTimeValue(v)); }
VV vv_undef()            { return g_vv_undef; }
VV vv_list()             { return VV(new ListValue()); }
VV vv_map()              { return VV(new MapValue()); }
VV vv_bool(bool v)       { return VV(new BooleanValue(v)); }
VV vv_bytes(const std::string &v) { return VV(new BytesValue(v)); }
VV vv_closure(VVCLSF func, const VV &obj)
                        { return VV(new ClosureValue(func, obj)); }
VV vv_ptr(void *ptr, const std::string &type)
                        { return VV(new PointerValue(ptr, type)); }
VV vv_ptr(void *ptr, const std::string &type, std::function<void(void *)> freeer, bool same_thread)
                        { return VV(new PointerValue(ptr, type, freeer, same_thread)); }
//---------------------------------------------------------------------------

static int hex2int(char h)
{
    if (h >= 'A' && h <= 'F')      return 10 + (h - 'A');
    else if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
    else if (h >= '0' && h <= '9') return h - '0';
    else                           return -1;
}
//---------------------------------------------------------------------------

VV vv_bytes_from_hex(const std::string &v)
{
    char *buf = new char[(v.size() / 2) + 1];
    int out_size = 0;
    for (size_t i = 0; i < v.size(); i += 2)
    {
        int h1i = hex2int(v[i]);
        int h2i = hex2int(v[i + 1]);
        if (h1i < 0 || h2i < 0)
        {
            buf[out_size++] = '?';
            continue;
        }
        else
        {
            buf[out_size++] =   ((unsigned char) (h1i << 4))
                              | ((unsigned char) h2i);
        }
    }
    VV ret = vv_bytes(std::string(buf, out_size));
    delete buf;
    return ret;
}
//---------------------------------------------------------------------------

VV VariantValueIterator::operator*()
{
    VV v = vv_undef();
    if (m_is_single) v = m_single_val;
    if (m_is_list) v = *m_l_it;
    if (m_is_map)
    {
        VV pair(vv_list());
        pair->push(vv(m_m_it->first));
        pair->push(m_m_it->second);
        v = pair;
    }
    if (!v) v = vv_undef();
    return v;
}
//---------------------------------------------------------------------------

VVPair vv_kv(const std::string &sKey, const VV &v)
{ return VVPair(sKey, v); }
VVPair vv_kv(const std::string &sKey, const int &v)
{ return VVPair(sKey, vv(v)); }
VVPair vv_kv(const std::string &sKey, const int64_t &v)
{ return VVPair(sKey, vv(v)); }
VVPair vv_kv(const std::string &sKey, const double &v)
{ return VVPair(sKey, vv(v)); }
VVPair vv_kv(const std::string &sKey, const std::string &v)
{ return VVPair(sKey, vv(v)); }
VVPair vv_kv(const std::string &sKey, const std::wstring &v)
{ return VVPair(sKey, vv(v)); }
//---------------------------------------------------------------------------

const VV &operator<<(const VV &vvO, const VVPair &v)
{
    vvO->set(v.first, v.second);
    return vvO;
}
//---------------------------------------------------------------------------

const VV &operator<<(const VV &vvO, const int &v)
{
    VV dvV(vv(v));
    if (vvO->is_list()) vvO->push(dvV);
    else               vvO->i_set(v);
    return vvO;
}
//---------------------------------------------------------------------------

const VV &operator<<(const VV &vvO, const int64_t &v)
{
    VV dvV(vv(v));
    if (vvO->is_list()) vvO->push(dvV);
    else               vvO->i_set(v);
    return vvO;
}
//---------------------------------------------------------------------------

const VV &operator<<(const VV &dv, const VV &vvO)
{
    dv->push(vvO);
    return dv;
}
//---------------------------------------------------------------------------

const VV &operator<<(const VV &vvO, const double &v)
{
    VV dvV(vv(v));
    if (vvO->is_list()) vvO->push(dvV);
    else               vvO->d_set(v);
    return vvO;
}
//---------------------------------------------------------------------------

const VV &operator<<(const VV &vvO, const std::string &v)
{
    VV dvV(vv(v));
    if (vvO->is_list()) vvO->push(dvV);
    else               vvO->s_set(v);
    return vvO;
}
//---------------------------------------------------------------------------

static void dump_string_to_ostream(std::ostream &out, const std::string &s)
{
    std::stringstream ss;
    ss << "\"";
    for (auto i : s)
    {
        if (i == '"')        ss << "\\\"";
        else if (isgraph((unsigned char) i)) ss << i;
        else if (i == ' ')   ss << " ";
        else if (i == '\t')  ss << "\\t";
        else if (i == '\n')  ss << "\\n";
        else if (i == '\v')  ss << "\\v";
        else if (i == '\f')  ss << "\\f";
        else if (i == '\r')  ss << "\\r";
        else                 ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (int) (unsigned char) i << std::dec;
    }
    ss << "\"";
    out << ss.str();
}
//---------------------------------------------------------------------------

const char *scheme_symbol_initial =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "!$%&*/:<=>?^_~";
const char *scheme_symbol_subseq =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "!$%&*/:<=>?^_~"
    "0123456789+-.@";

bool is_scheme_symbol(const std::string &s)
{
    return    s.substr(0, 1).find_first_not_of(scheme_symbol_initial) == std::string::npos
           && s.find_first_not_of(scheme_symbol_subseq)               == std::string::npos;
}
//---------------------------------------------------------------------------

#ifdef BZVC
std::ostream &operator<<(std::ostream &out, const VV &vv)
{
    return out << as_json(vv, true);
}
#else
std::ostream &operator<<(std::ostream &out, const VV &vv)
{
    if (vv->is_list())
    {
        out << "[";
        bool first = true;
        for (auto i : *vv)
        {
            if (first) first = false;
            else       out << " ";
            out << i;
        }
        out << "]";
    }
    else if (vv->is_map())
    {
        out << "{";

        std::vector<std::string> keys;
        for (auto i : *vv)
            keys.push_back(i->_s(0));

        std::sort(keys.begin(), keys.end());

        bool first = true;
        for (auto i : keys)
        {
            if (first) first = false;
            else       out << " ";
            if (is_scheme_symbol(i)) out << i << ":";
            else                     dump_string_to_ostream(out, i);
            out << " " << vv->_(i);
        }

        out << "}";
    }
    else if (vv->is_keyword())
    {
        out << vv->s() << ":";
    }
    else if (vv->is_symbol())
    {
        out << vv->s();
    }
    else if (vv->is_string())
    {
        dump_string_to_ostream(out, vv->s());
    }
    else if (vv->is_bytes())
    {
        dump_string_to_ostream(out, vv->s());
    }
    else if (vv->is_boolean())
    {
        if (vv->is_true()) out << "true";
        else               out << "false";
    }
    else if (vv->is_closure())
    {
        out << "#<closure:"
            << vv->closure_func()
            << ":obj:"
            << vv->closure_obj() << ">";
    }
    else if (vv->is_undef())
    {
        out << "nil";
    }
    else
    {
        out << vv->s();
    }
    return out;
}
#endif
//---------------------------------------------------------------------------

std::string vv_dump(const VV &v)
{
    std::stringstream ss;
    ss << v;
    return ss.str();
}
//---------------------------------------------------------------------------

} // namespace VVal
