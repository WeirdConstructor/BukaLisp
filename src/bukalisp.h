// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include "buklivm.h"
#include "interpreter.h"
#include "atom_generator.h"
#include <memory>

namespace bukalisp
{
//---------------------------------------------------------------------------

class Value;
typedef std::shared_ptr<Value> ValuePtr;

class Value
{
    private:
        GC &m_gc;
        GC_ROOT_MEMBER(m_value);

        Value(const Value &)        = delete;
        Value()                     = delete;
        Value &operator=(const Value &)    = delete;
    public:
        Value(GC &gc, const Atom &value)
            : m_gc(gc),
              GC_ROOT_MEMBER_INITALIZE(gc, m_value)
        {
            m_value = value;
        }

        bool is_nil() const { return m_value.m_type == T_NIL; }
        int64_t i() const { return m_value.to_int(); }
        double d() const { return m_value.to_dbl(); }
        bool b() const { return !m_value.is_false(); }
        std::string s() const
        {
            switch (m_value.m_type)
            {
                case T_STR:
                case T_SYM:
                case T_KW:
                    return m_value.m_d.sym->m_str;
                default:
                    return m_value.to_display_str();
            }
        }

        int64_t _i(size_t i) const { return m_value.at(i).to_int(); }
        double _d(size_t i) const { return m_value.at(i).to_dbl(); }
        bool _b(size_t i) const { return !(m_value.at(i).is_false()); }
        std::string _s(size_t i) const
        {
            Atom v = m_value.at(i);
            switch (v.m_type)
            {
                case T_STR:
                case T_SYM:
                case T_KW:
                    return v.m_d.sym->m_str;
                default:
                    return v.to_display_str();
            }
        }
        bool _is_nil(size_t i) const
        {
            return m_value.at(i).m_type == T_NIL;
        }

        int64_t _i(const std::string &key) const {
            return m_value.at(Atom(T_KW, m_gc.new_symbol(key))).to_int(); }
        double _d(const std::string &key) const {
            return m_value.at(Atom(T_KW, m_gc.new_symbol(key))).to_dbl(); }
        bool _b(const std::string &key) const {
            return !(m_value.at(Atom(T_KW, m_gc.new_symbol(key))).is_false()); }
        std::string _s(const std::string &key) const
        {
            Atom v = m_value.at(Atom(T_KW, m_gc.new_symbol(key)));
            switch (v.m_type)
            {
                case T_STR:
                case T_SYM:
                case T_KW:
                    return v.m_d.sym->m_str;
                default:
                    return v.to_display_str();
            }
        }
        bool _is_nil(const std::string &key)
        {
            return m_value.at(Atom(T_KW, m_gc.new_symbol(key))).m_type == T_NIL;
        }

        ValuePtr _(size_t i)
        {
            if (m_value.m_type != T_VEC)
                return std::make_shared<Value>(m_gc, Atom());
            return std::make_shared<Value>(m_gc, m_value.at(i));
        }
        ValuePtr _(const std::string &key)
        {
            if (m_value.m_type != T_MAP)
                return std::make_shared<Value>(m_gc, Atom());
            return
                std::make_shared<Value>(
                    m_gc,
                    m_value.at(Atom(T_KW, m_gc.new_symbol(key))));
        }
        ValuePtr _(const ValuePtr &ptr)
        {
            if (m_value.m_type != T_MAP)
                return std::make_shared<Value>(m_gc, Atom());
            return
                std::make_shared<Value>(
                    m_gc,
                    m_value.at(ptr->m_value));
        }

        std::string to_write_str(bool pp = false) const { return m_value.to_write_str(pp); }
        std::string to_display_str() const { return m_value.to_display_str(); }
};
//---------------------------------------------------------------------------

class ValueFactory;
typedef std::shared_ptr<ValueFactory>   ValueFactoryPtr;

//---------------------------------------------------------------------------

class ValueFactory
{
    private:
        GC           &m_gc;
        AtomGenerator m_ag;

    public:
        ValueFactory(GC &gc)
            : m_gc(gc), m_ag(&gc)
        {
            m_ag.start();
        }

        ValueFactory &read(const std::string &input, const std::string &name = "ValueFactory::read")
        {
            Tokenizer      tok;
            Parser         m_par(tok, &m_ag);
            m_par.reset();
            tok.reset();
            tok.tokenize(name, input);
            if (!m_par.parse())
            {
                throw BukaLISPException(
                    "ERROR while parsing in ValueFactory::read");
            }
            return *this;
        }

        ValueFactory &open_list()
        {
            m_ag.start_list();
            return *this;
        }

        ValueFactory &close_list()
        {
            m_ag.end_list();
            return *this;
        }

        ValueFactory &operator()(int i)
        {
            m_ag.atom_int(i);
            return *this;
        }
        ValueFactory &operator()(int64_t i)
        {
            m_ag.atom_int(i);
            return *this;
        }
        ValueFactory &operator()(double i)
        {
            m_ag.atom_dbl(i);
            return *this;
        }
        ValueFactory &operator()()
        {
            m_ag.atom_nil();
            return *this;
        }
        ValueFactory &operator()(bool b)
        {
            m_ag.atom_bool(b);
            return *this;
        }

        ValuePtr value()
        {
            auto val = std::make_shared<Value>(m_gc, m_ag.root());
            m_ag.start();
            return val;
        }
};
/*

    Instance bkl;

    auto fact = bkl.create_value_factory();

    auto func = bkl.get_global("handle-event");
    if (func.is_nil())
        throw ...;

    auto return =
        func(fact
                (10)
                (20)
                .read("(1 2 + 10 {a: 10 b: 20})")
                .open_list()
                    (1)
                    ("foo")
                    sym("bar")
                .close()
                .value());
*/
//---------------------------------------------------------------------------


class Instance
{
    private:
        Runtime     m_rt;
        VM          m_vm;
        Interpreter m_i;
        bool        m_trace_vm;

        GC_ROOT_MEMBER(m_compiler);
        std::function<Atom(Atom prog, AtomMap *root_env, const std::string &name, bool only_compile)>
                    m_compile_func;

    public:
        Instance()
            : m_vm(&m_rt),
              m_i(&m_rt, &m_vm),
              GC_ROOT_MEMBER_INITALIZE(m_rt.m_gc, m_compiler),
              m_trace_vm(false)
        {
        }

        void set_trace(bool trc) { m_trace_vm = trc; }

        Runtime &get_runtime() { return m_rt; }

        void load_bootstrapped_compiler_from_disk();
        Atom execute_string(const std::string &line, AtomMap *root_env);
        Atom execute_file(const std::string &filepath);
        void load_module(BukaLISPModule *mod);

        ValueFactoryPtr create_value_factory()
        {
            return std::make_shared<ValueFactory>(m_rt.m_gc);
        }
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
