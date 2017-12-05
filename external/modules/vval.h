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

#ifndef VVAL_H
#define VVAL_H 1
#include <iostream>
#include <memory>
#include <thread>
#include <locale>
#include <codecvt>
#include <cstring>
#include <list>
#include <unordered_map>
#include <functional>
#include "utf8buffer.h"
#include <boost/format.hpp>
#include "datetime.h"

using boost::format;

#define UNUSED(x) ((void)(x))

namespace VVal
{


class VariantValue;
class VariantValueIterator;
typedef std::shared_ptr<VariantValue>         VV;
typedef std::shared_ptr<UTF8Buffer>           VBuf;
typedef std::pair<std::string, VV>            VVPair;
//typedef VV (*VVCLSF)(VV vv_obj, VV vv_args);
typedef std::function<VV(const VV &, const VV &)>  VVCLSF;

class VariantValueException : public std::exception
{
    private:
        VV          m_value;
        std::string m_msg;

    public:
        VariantValueException(const VV &v, const std::string &s)
        {
            std::stringstream ss;
            ss << s << ", Value: " << v;
            m_msg = ss.str();
        }
        virtual ~VariantValueException() noexcept {}

        virtual const char *what() const noexcept { return m_msg.c_str(); }
};

std::string vv_dump(const VV &v);

VV vv(int64_t v);
VV vv(int32_t v);
VV vv(int16_t v);
VV vv(double v);
VV vv(const std::string &v);
VV vv_kw(const std::string &v);
VV vv_sym(const std::string &v);
VV vv(const std::wstring &v);
VV vv_list();
VV vv_map();
VV vv_undef();
VV vv_bool(bool v);
VV vv_bytes(const std::string &v);
VV vv_bytes_from_hex(const std::string &v);
VV vv_closure(VVCLSF func, const VV &obj);
VV vv_ptr(void *ptr, const std::string &type);
VV vv_ptr(void *ptr, const std::string &type, std::function<void(void *)> freeer, bool same_thread);
VV vv_dt(std::time_t);

#define VV_CLOSURE_DECL(name) \
    extern const char *VVC_DOC_##name; \
    VVal::VV VVC_CLS_##name(const VVal::VV &vv_obj, const VVal::VV &vv_args); \
    VVal::VV VVC_NEW_##name(const VVal::VV &vv_obj); \
    VVal::VV VVC_NEW_##name();

#define VV_CLOSURE(name) \
    const char *VVC_DOC_##name = "(undocumented)"; \
    VVal::VV VVC_CLS_##name(const VVal::VV &vv_obj, const VVal::VV &vv_args); \
    VVal::VV VVC_NEW_##name(const VVal::VV &vv_obj) { return VVal::vv_closure(&VVC_CLS_##name, vv_obj); } \
    VVal::VV VVC_NEW_##name()                       { return VVC_NEW_##name(VVal::g_vv_undef); } \
    VVal::VV VVC_CLS_##name(const VVal::VV &vv_obj, const VVal::VV &vv_args)

#define VV_CLOSURE_DOC(name,doc) \
    const char *VVC_DOC_##name = doc; \
    VVal::VV VVC_CLS_##name(const VVal::VV &vv_obj, const VVal::VV &vv_args); \
    VVal::VV VVC_NEW_##name(const VVal::VV &vv_obj) { return VVal::vv_closure(&VVC_CLS_##name, vv_obj); } \
    VVal::VV VVC_NEW_##name()                       { return VVC_NEW_##name(VVal::g_vv_undef); } \
    VVal::VV VVC_CLS_##name(const VVal::VV &vv_obj, const VVal::VV &vv_args)

#define vvc_new(name) VVC_NEW_##name

VVPair vv_kv(const std::string &sKey, const VV &v);
VVPair vv_kv(const std::string &sKey, const int &v);
VVPair vv_kv(const std::string &sKey, const int64_t &v);
VVPair vv_kv(const std::string &sKey, const double &v);
VVPair vv_kv(const std::string &sKey, const std::string &v);
VVPair vv_kv(const std::string &sKey, const std::wstring &v);

const VV &operator<<(const VV &vv, const VVPair &v);
const VV &operator<<(const VV &vv, const int &v);
const VV &operator<<(const VV &vv, const int64_t &v);
const VV &operator<<(const VV &vv, const std::string &v);
const VV &operator<<(const VV &vv, const double &v);
const VV &operator<<(const VV &vv, const VV &vvO);

std::ostream &operator<<(std::ostream &out, const VV &vv);

//---------------------------------------------------------------------------

typedef std::shared_ptr<std::vector<VV>>                      VV_LIST;
typedef std::shared_ptr<std::unordered_map<std::string, VV>>  VV_MAP;

class VariantValueIterator
{
    private:
        bool                                                   m_is_single;
        bool                                                   m_is_list;
        bool                                                   m_is_map;
        VV_LIST                                                m_p_list;
        VV_MAP                                                 m_p_map;
        std::vector<VV>::const_iterator                        m_l_it;
        std::unordered_map<std::string, VV>::const_iterator    m_m_it;
        VV                                                     m_single_val;

    public:
        VariantValueIterator()
            : m_is_list(false), m_is_map(false), m_is_single(false), m_p_list(0), m_p_map(0)
        {
        }
        VariantValueIterator(const VV &v)
            : m_is_list(false), m_is_map(false), m_is_single(true), m_single_val(v), m_p_list(0), m_p_map(0)
        {
        }
        VariantValueIterator(const VV_MAP &v, bool is_begin = false)
            : m_is_list(false), m_is_map(true), m_is_single(false), m_p_list(0), m_p_map(v)
        {
            if (is_begin) m_m_it = v->begin();
            else          m_m_it = v->end();
        }
        VariantValueIterator(const VV_LIST &v, bool is_begin = false)
            : m_is_list(true), m_is_map(false), m_is_single(false), m_p_list(v), m_p_map(0)
        {
            if (is_begin) m_l_it = v->begin();
            else          m_l_it = v->end();
        }

        VV operator*();

        bool operator!=(const VariantValueIterator &o) const
        {
            if (m_is_list)        return m_l_it != o.m_l_it;
            else if (m_is_map)    return m_m_it != o.m_m_it;
            else if (m_is_single) return m_is_single != o.m_is_single;
            else                  return false;
        }
        VariantValueIterator &operator++()
        {
            if (m_is_list) ++m_l_it;
            if (m_is_map)  ++m_m_it;
            if (m_is_single) m_is_single = false;
            return *this;
        }
};
//---------------------------------------------------------------------------

extern VV g_vv_undef;

//---------------------------------------------------------------------------

class VariantValue
{
    public:
        VariantValue()              { }
        virtual ~VariantValue()     { }

        virtual void i_set(int64_t v) { UNUSED(v); }
        virtual void d_set(double  v) { UNUSED(v); }
        virtual void p_set(void *v, const std::string &t) { UNUSED(v); UNUSED(t); }
        virtual void s_set(const std::string &v)     { UNUSED(v); }

        virtual int64_t i() const { return 0;   }
        virtual double  d() const { return 0.0; }
        virtual void   *p(const std::string &t) const { UNUSED(t); return (void *) 0; }
        virtual char *s_buffer(size_t &len)
        {
            std::string s = this->s();
            char *buf = new char[s.size()];
            len = s.size();
            std::memcpy(buf, s.data(), s.size());
            return buf;
        }
        virtual std::string  s() const { return std::string(); }
        virtual std::string  s_hex() const;
        virtual std::time_t  dt() const
        {
            return parse_datetime(this->s(), "%Y-%m-%d %H:%M:%S");
        }

        virtual bool is_undef()    const { return true; }
        virtual bool is_defined()  const { return !this->is_undef(); }
        virtual bool is_true()     const { return this->b(); }
        virtual bool is_false()    const { return !this->b(); }
        virtual bool is_boolean()  const { return false; }
        virtual bool is_string()   const { return false; }
        virtual bool is_keyword()  const { return false; }
        virtual bool is_symbol()   const { return false; }
        virtual bool is_bytes()    const { return false; }
        virtual bool is_int()      const { return false; }
        virtual bool is_double()   const { return false; }
        virtual bool is_pointer()  const { return false; }
        virtual bool is_map()      const { return false; }
        virtual bool is_list()     const { return false; }
        virtual bool is_datetime() const { return false; }
        virtual bool is_closure()  const { return false; }

        virtual std::string type() const { return ""; }

        virtual void dt_set(std::time_t t)
        {
            this->s_set(format_datetime(t, "%Y-%m-%d %H:%M:%S"));
        }
        virtual void b_set(VBuf vb)                  { this->s_set(vb->as_string()); }
        virtual void b_set(const std::string &v)     { this->b_set(VBuf(new UTF8Buffer(v.data(), v.size()))); }
        virtual void b_set(const UTF8Buffer *u8b)    { this->b_set(VBuf(new UTF8Buffer(*u8b))); }
        virtual void s_set(const std::wstring &v)
        {
            // TODO: check if codecvt_utf8 is enough for windows
#ifdef __gnu_linux__
            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_utf16_converter;
#else
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16_converter;
#endif
            this->s_set(utf8_utf16_converter.to_bytes(v));
        }
        virtual std::wstring w() const
        {
#ifdef __gnu_linux__
            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_utf16_converter;
#else
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16_converter;
#endif
            return utf8_utf16_converter.from_bytes(this->s());
        }
        virtual bool         b() const
        {
            return this->i() != 0;
        }
        virtual VBuf         u() const
        {
            std::string s = this->s();
            return VBuf(new UTF8Buffer(s.data(), s.size()));
        }

        template<typename T>
        T *P(const std::string &sType) { return (T *) this->p(sType); }

        virtual int32_t size() const { return 0; }
        virtual VV clone() const { return g_vv_undef; }
        virtual VV _(int32_t i)        const { (void) i; return g_vv_undef; }
        virtual VV _(const std::string &i) const { (void) i; return g_vv_undef; }

        virtual int64_t _i(const std::string &i) const { return this->_(i)->i(); }
        virtual int64_t _i(int32_t i)            const { return this->_(i)->i(); }
        virtual double _d(const std::string &i) const  { return this->_(i)->d(); }
        virtual double _d(int32_t i)            const  { return this->_(i)->d(); }
        virtual bool _b(const std::string &i) const    { return this->_(i)->b(); }
        virtual bool _b(int32_t i)            const    { return this->_(i)->b(); }
        virtual VBuf _u(const std::string &i) const    { return this->_(i)->u(); }
        virtual VBuf _u(int32_t i)            const    { return this->_(i)->u(); }
        virtual std::string _s(const std::string &i) const    { return this->_(i)->s(); }
        virtual std::string _s(int32_t i)            const    { return this->_(i)->s(); }
        virtual std::wstring _w(const std::string &i) const   { return this->_(i)->w(); }
        virtual std::wstring _w(int32_t i)            const   { return this->_(i)->w(); }
        virtual std::time_t _dt(const std::string &i) const    { return this->_(i)->dt(); }
        virtual std::time_t _dt(int32_t i)            const    { return this->_(i)->dt(); }

        virtual VV closure_obj()     const { return g_vv_undef; }
        virtual void *closure_func()       { return 0; }
#undef _P
        template<typename T>
        T *_P(const std::string &sIdx, const std::string &sType)
        { return (T *) this->_(sIdx)->p(sType); }

        template<typename T>
        T *_P(int32_t lIdx, const std::string &sType)
        { return (T *) this->_(lIdx)->p(sType); }

        virtual VV call(const VV &vvArg) const
        {
            (void) vvArg;
            return this->clone();
        }

        virtual VV call() const
        {
            VV vvNull;
            return this->call(vvNull);
        }

        virtual void set(int32_t i, const VV &v)            { UNUSED(i); UNUSED(v); }
        virtual void set(const std::string &i, const VV &v) { UNUSED(i); UNUSED(v); }

        virtual VV pop()
        {
            if (this->size() <= 0) return g_vv_undef;
            VV vvRet = this->_(this->size() - 1);
            this->set(-3, VV());
            return vvRet;
        }

        virtual VV shift()
        {
            if (this->size() <= 0) return g_vv_undef;
            VV vvRet = this->_(0);
            this->set(-4, VV());
            return vvRet;
        }

        virtual void unshift(const VV &v) { this->set(-2, v); }
        virtual void push(const VV &v)    { this->set(-1, v); }

        virtual void remove(const std::string &i) { this->set(-5, vv(i)); }

        virtual VariantValueIterator begin() const { return VariantValueIterator(this->clone()); }
        virtual VariantValueIterator end()   const { return VariantValueIterator(); }
};
//---------------------------------------------------------------------------

class BooleanValue : public VariantValue
{
    private:
        bool    m_b;
    public:
        BooleanValue(bool v) : m_b(v) { }
        virtual ~BooleanValue() { }

        virtual void i_set(int64_t v) { m_b = v != 0; }
        virtual void d_set(double  v) { m_b = v != 0.0; }
        virtual void p_set(void *v, const std::string &t) { m_b = v != 0; UNUSED(t); }
        virtual void s_set(const std::string &v)     { m_b = v.size() > 0; }
        virtual void s_set(const std::wstring &v)    { m_b = v.size() > 0; }
        virtual void b_set(VBuf vb)                  { m_b = vb->length() > 0; }

        virtual int64_t i() const { return m_b ? 1 : 0;   }
        virtual double  d() const { return m_b ? 1.0 : 0.0; }
        virtual std::string  s() const { return m_b ? "1" : ""; }
        virtual bool         b() const { return m_b; }

        virtual bool is_undef()   const { return false; }
        virtual bool is_true()    const { return m_b; }
        virtual bool is_boolean() const { return true; }

        virtual VV clone() const { return VV(new BooleanValue(m_b)); }
};
//---------------------------------------------------------------------------

class IntegerValue : public VariantValue
{
    private:
        int64_t m_int;
    public:
        IntegerValue(int64_t v) : m_int(v) { }
        virtual ~IntegerValue() { }

        virtual void dt_set(std::time_t t) { m_int = (int64_t) t; }
        virtual void i_set(int64_t v) { m_int = v; }
        virtual void d_set(double  v) { m_int = (int64_t) v; }
        virtual void p_set(void *v, const std::string &t) { m_int = (int64_t) v; UNUSED(t); }
        virtual void s_set(const std::string &v)
        {
            // TODO: TEST EXCEPTIONS!
            try { m_int = std::stoll(v); } catch (const std::exception &) { m_int = 0; }
        }

        virtual std::time_t dt() const    { return (std::time_t) m_int; }
        virtual int64_t i() const       { return m_int;   }
        virtual double  d() const       { return (double) m_int; }
        virtual std::string  s() const  { return std::to_string(m_int); }
        virtual std::wstring  w() const { return std::to_wstring(m_int); }
        virtual bool b() const          { return m_int != 0; }

        virtual bool is_undef()   const { return false; }
        virtual bool is_int()     const { return true; }

        virtual VV clone() const { return VV(new IntegerValue(m_int)); }
};
//---------------------------------------------------------------------------

class DateTimeValue : public VariantValue
{
    private:
        std::time_t m_time;

    public:
        DateTimeValue(std::time_t v) : m_time(v) { }
        virtual ~DateTimeValue() { }

        virtual void dt_set(std::time_t t) { m_time = t; }
        virtual void i_set(int64_t v) { m_time = v; }
        virtual void d_set(double  v) { m_time = (std::time_t) (int64_t) v; }
        virtual void p_set(void *v, const std::string &t) { m_time = (int64_t) v; UNUSED(t); }
        virtual void s_set(const std::string &v)
        {
            m_time = parse_datetime(v, "%Y-%m-%d %H:%M:%S");
        }

        virtual std::time_t dt() const    { return m_time; }
        virtual int64_t i() const       { return (int64_t) m_time;   }
        virtual double  d() const       { return (double) m_time; }
        virtual std::string  s() const  { return format_datetime(m_time, "%Y-%m-%d %H:%M:%S"); }
        virtual bool b() const          { return m_time != 0; }

        virtual bool is_undef()    const { return false; }
        virtual bool is_datetime() const { return true; }

        virtual VV clone() const { return VV(new DateTimeValue(m_time)); }
};
//---------------------------------------------------------------------------

class DoubleValue : public VariantValue
{
    private:
        double m_dbl;
    public:
        DoubleValue(double v) : m_dbl(v) { }
        virtual ~DoubleValue() { }

        virtual void dt_set(std::time_t t) { m_dbl = (double) (int64_t) t; }
        virtual void i_set(int64_t v) { m_dbl = (double) v; }
        virtual void d_set(double  v) { m_dbl = v; }
        virtual void p_set(void *v, const std::string &t) { m_dbl = (double) (int64_t) v; UNUSED(t); }
        virtual void s_set(const std::string &v)
        {
            try { m_dbl = std::stod(v); } catch (const std::exception &) { m_dbl = 0.0; }
        }
        virtual void s_set(const std::wstring &v)
        {
            try { m_dbl = std::stod(v); } catch (const std::exception &) { m_dbl = 0.0; }
        }

        virtual std::time_t dt() const    { return (std::time_t) m_dbl; }
        virtual int64_t i() const { return (int64_t) m_dbl; }
        virtual double  d() const { return m_dbl; }
        virtual std::string  s() const { return std::to_string(m_dbl); }
        virtual bool b() const { return m_dbl != 0.0; }

        virtual bool is_undef()   const { return false; }
        virtual bool is_double()  const { return true; }

        virtual VV clone() const { return VV(new DoubleValue(m_dbl)); }
};
//---------------------------------------------------------------------------

class StringValue : public VariantValue
{
    protected:
        std::string m_str;
        char        m_flag;

    public:
        StringValue(const std::string &s) : m_str(s), m_flag('\0') { }
        StringValue(const std::string &s, char f) : m_str(s), m_flag(f) { }
        virtual ~StringValue()
        {
//            std::cout << "DESTRSTR[" << m_str << "]" << std::endl;
        }

        virtual void i_set(int64_t v)                     { m_str = std::to_string(v); }
        virtual void d_set(double v)                      { m_str = std::to_string(v); }
        virtual void p_set(void *v, const std::string &t) { m_str = "#<" + t + ":" + std::to_string((uint64_t) v) + ">"; }
        virtual void s_set(const std::string &v)          { m_str = v; }

        virtual bool is_undef()   const { return false; }
        virtual bool is_string()  const { return true; }
        virtual bool is_keyword() const { return m_flag == 'k'; }
        virtual bool is_symbol()  const { return m_flag == 'y'; }

        virtual char *s_buffer(size_t &len)
        {
            char *buf = new char[m_str.size()];
            len = m_str.size();
            std::memcpy(buf, m_str.data(), m_str.size());
            return buf;
        }

        virtual int64_t i() const
        {
            int64_t iv;
            try { iv = std::stoll(m_str); } catch (const std::exception &) { iv = 0; }
            return iv;
        }
        virtual double d() const
        {
            double dv;
            try { dv = std::stod(m_str); } catch (const std::exception &) { dv = 0.0; }
            return dv;
        }
        virtual std::string s() const { return m_str; }

        virtual VV clone() const { return VV(new StringValue(m_str)); }
};
//---------------------------------------------------------------------------

class BytesValue : public StringValue
{
    public:
        BytesValue(const std::string &s) : StringValue(s) { }
        virtual ~BytesValue() { }

        virtual bool is_undef()   const { return false; }
        virtual bool is_string()  const { return false; }
        virtual bool is_bytes()   const { return true; }

        virtual VV clone() const { return VV(new BytesValue(m_str)); }
};
//---------------------------------------------------------------------------

class ClosureValue : public VariantValue
{
    private:
        VV          m_vvObj;
        VVCLSF      m_vClsF;
    public:
        ClosureValue(VVCLSF vClsF, VV vv_obj)
            : m_vClsF(vClsF), m_vvObj(vv_obj)
        {
            if (!m_vvObj) m_vvObj = g_vv_undef;
        }

        virtual ~ClosureValue() { }

        virtual void i_set(int64_t v) { this->call(vv(v)); }
        virtual void d_set(double  v) { this->call(vv(v)); }
        virtual void p_set(void *v, const std::string &t) { this->call(vv_ptr(v, t)); }
        virtual void s_set(const std::string &v)     { this->call(vv(v)); }

        virtual int64_t i() const { return this->call()->i(); }
        virtual double  d() const { return this->call()->d(); }
        virtual void   *p(const std::string &t) const { return this->call()->p(t); }
        virtual std::string  s() const { return this->call()->s(); }
        virtual bool         b() const { return this->call()->b(); }

        virtual bool is_undef()   const { return false; }
        virtual bool is_closure() const { return true; }

        virtual VV closure_obj()     const { return m_vvObj; }
        virtual void *closure_func()
        {
            return (void *) m_vClsF.target<VV(const VV &, const VV &)>();
        }

        virtual VV call(const VV &vvArg) const
        {
            VV vvRet;

            if (vvArg) vvRet = m_vClsF(m_vvObj, vvArg);
            else       vvRet = m_vClsF(m_vvObj, g_vv_undef);

            if (!vvRet) vvRet = g_vv_undef;
            return vvRet;
        }

        virtual VV call() const
        {
            VV vvNull;
            return this->call(vvNull);
        }

        virtual void set(int32_t i, const VV &v)            { UNUSED(i); UNUSED(v); }
        virtual void set(const std::string &i, const VV &v) { UNUSED(i); UNUSED(v); }

        virtual VV clone() const { return VV(new ClosureValue(m_vClsF, m_vvObj->clone())); }
};
//---------------------------------------------------------------------------

class ListValue : public VariantValue
{
    friend class VariantValueIterator;
    protected:
        VV_LIST             m_v;

    public:
        ListValue() : m_v(new std::vector<VV>) { }
        virtual ~ListValue() { }

        virtual std::string s() const { return std::string("#<list: ") + std::to_string((uint64_t) this) + ">"; }
        virtual bool is_undef() const { return false; }
        virtual bool is_list()  const { return true; }
        virtual int32_t size() const { return (int32_t) m_v->size(); }
        virtual VV clone() const
        {
            ListValue *lv = new ListValue;
            VV v(lv);
            auto &vec = *(lv->m_v);
            vec.reserve(vec.size());
            for (auto i : *m_v)
                vec.push_back(i->clone());
            return v;
        }
        virtual void set(const std::string &i, const VV &v)
        {
            int32_t idx = 0;
            try { idx = std::stoi(i); } catch (const std::exception &) { idx = 0; }
            set(idx, v);
        }
        virtual void set(int32_t i, const VV &v)
        {
            auto &vec = *m_v;
            if (i == -1)        vec.push_back(v);
            else if (i == -2)   vec.insert(vec.begin(), v);
            else if (i == -3)   vec.pop_back();
            else if (i == -4)   vec.erase(vec.begin());
            else if (i == -5) // remove
            {
                // TODO
            }
            else
            {
                if (vec.size() <= (size_t) i)
                    vec.resize(i + 1);
                vec[i] = v;
            }
        }

        virtual VV _(int32_t i) const
        {
            auto &vec = *m_v;
            if (((size_t) i) >= vec.size())
                return vv_undef();
            if (!vec[i])
                return vv_undef();
            return vec[i];
        }

        virtual VV _(const std::string &i) const
        {
            auto &vec = *m_v;
            int32_t idx = 0;
            try { idx = std::stoi(i); } catch (const std::exception &) { idx = 0; }
            if (((size_t) idx) >= vec.size())
                return vv_undef();
            if (!vec[idx])
                return vv_undef();
            return vec[idx];
        }

        virtual VariantValueIterator begin() const { return VariantValueIterator(m_v, true); }
        virtual VariantValueIterator end()   const { return VariantValueIterator(m_v); }
};
//---------------------------------------------------------------------------

class MapValue : public VariantValue
{
    friend class VariantValueIterator;
    protected:
        VV_MAP m_m;

    public:
        MapValue() : m_m(new std::unordered_map<std::string, VV>) { }
        virtual ~MapValue() { }

        virtual std::string s() const { return std::string("#<map: ") + std::to_string((uint64_t) this) + ">"; }
        virtual bool is_undef() const { return false; }
        virtual bool is_map()  const { return true; }
        virtual int32_t size() const { return (int32_t) m_m->size(); }
        virtual VV clone() const
        {
            MapValue *mv = new MapValue;
            VV v(mv);
            auto &map = *(mv->m_m);
            for (auto i : *m_m)
                map[i.first] = i.second->clone();
            return v;
        }
        virtual void set(const std::string &i, const VV &v) { (*m_m)[i] = v; }
        virtual void set(int32_t i, const VV &v) { set(std::to_string(i), v); }
        virtual VV _(int32_t i) const { return _(std::to_string(i)); }
        virtual VV _(const std::string &i) const
        {
            auto it = m_m->find(i);
            if (it != m_m->end())
                return it->second;
            return vv_undef();
        }

        virtual VariantValueIterator begin() const { return VariantValueIterator(m_m, true); }
        virtual VariantValueIterator end()   const { return VariantValueIterator(m_m); }
};
//---------------------------------------------------------------------------

class PointerValue : public VariantValue
{
    private:
        void                       *m_pointer;
        std::string                 m_type;
        std::function<void(void *)> m_free;
        std::thread::id             m_create_thread;
        bool                        m_check_same_thread;

        std::string ptr2str() const
        {
            char buf[128];
            int iLen = snprintf(buf, 128, "%p", (void *) m_pointer);
            return std::string(buf, iLen);
        }
        void check_thread() const
        {
            if (!m_check_same_thread)
                return;
            if (std::this_thread::get_id() != m_create_thread)
            {
                std::cerr << "FATAL ERROR: PointerValue VariantValue used outside creation thread! (vv="
                        << this << ")" << std::endl;
                throw VariantValueException(
                        vv(this->s()),
                        "FATAL ERROR: PointerValue VariantValue used outside creation thread!");
            }
        }
    public:
        PointerValue(void *ptr, const std::string &s)
            : m_pointer(ptr), m_type(s), m_check_same_thread(false)
        {
            m_create_thread = std::this_thread::get_id();
        }
        PointerValue(void *ptr, const std::string &s, std::function<void(void *)> freefunc, bool sameth = false)
            : m_pointer(ptr), m_type(s), m_free(freefunc), m_check_same_thread(sameth)
        {
            m_create_thread = std::this_thread::get_id();
        }
        virtual ~PointerValue()
        {
            if (m_free)
            {
                if (m_check_same_thread && std::this_thread::get_id() != m_create_thread)
                    std::cerr << "FATAL ERROR IN VVal::VV (vv="
                              << this << "): FREE PointerValue from wrong thread!"
                              << m_create_thread << ", freed from "
                              << std::this_thread::get_id() << std::endl;
                else
                    m_free((void *) m_pointer);
            }
        }

        virtual bool is_undef()   const { return false; }
        virtual bool is_true()    const { return m_pointer != 0; }
        virtual bool is_pointer() const { return true; }

        virtual void i_set(int64_t v) { m_pointer = (void *) v; }
        virtual void d_set(double  v) { m_pointer = (void *) (int64_t) v; }
        virtual void p_set(void *v, const std::string &t) { m_pointer = v; m_type = t; }
        virtual void s_set(const std::string &v)
        {
            try { m_pointer = (void *) std::stoll(v); }
            catch (const std::exception &) { m_pointer = 0; }
        }

        virtual int64_t i() const { return (int64_t) m_pointer;   }
        virtual double  d() const { return (double) (int64_t) m_pointer; }
        virtual void   *p(const std::string &t) const
        {
            check_thread();
            if (m_type == t)            return m_pointer;
            else if (m_type.empty())    return m_pointer;
            else
            {
                throw
                    VariantValueException(
                        this->clone(),
                        (format("invalid pointer access, type '%1%' expected, got %2%.")
                        % t % m_type).str());
            }
        }
        virtual std::string  s() const
        {
            if (m_free)
                return (format("#<pointer-freeable:%2%:%1%>") % this->ptr2str() % m_type).str();
            else
                return (format("#<pointer:%2%:%1%>") % this->ptr2str() % m_type).str();
        }
        virtual std::string type() const
        {
            return m_type;
        }
        virtual VV clone() const
        {
            check_thread();
            if (m_free)
                return VV(new PointerValue(nullptr, m_type));
            else
                return VV(new PointerValue(m_pointer, m_type));
        }

};
// TODO: weitere typen aus vval.h!

} // namespace vval

#endif // VVAL_H
