// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include <memory>
#include <vector>
#include "parser.h"
#include "atom.h"
#include "atom_printer.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

class BukLiVMException : public std::exception
{
    private:
        std::string m_err;
    public:
        BukLiVMException(const std::string &err) : m_err(err) { }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~BukLiVMException() { }
};
//---------------------------------------------------------------------------

class AtomGenerator : public bukalisp::SEX_Builder
{
    private:
        GC                       *m_gc;
        Atom                      m_root;

        typedef std::function<void(Atom &a)>  AddFunc;
        std::vector<std::pair<Atom, AddFunc>> m_add_stack;

    public:
        AtomGenerator(GC *gc)
            : m_gc(gc)
        {
        }
        virtual ~AtomGenerator() { }

        void start()
        {
            m_add_stack.push_back(
                std::pair<Atom, AddFunc>(
                    m_root, [this](Atom &a) { m_root = a; }));
        }

        Atom &root() { return m_root; }

        virtual void error(const std::string &what,
                           const std::string &inp_name,
                           size_t line,
                           const std::string &tok)
        {
            throw BukaLISPException("reader", inp_name, line, tok, what);
        }

        virtual void start_list()
        {
            AtomVec *new_vec = m_gc->allocate_vector(0);
            Atom a(T_VEC, new_vec);

            AtomVec *meta_info = m_gc->allocate_vector(2);
            meta_info->push(Atom(T_STR, m_gc->new_symbol(m_dbg_input_name)));
            meta_info->push(Atom(T_INT, m_dbg_line));
            m_gc->set_meta_register(a, 0, Atom(T_VEC, meta_info));

            m_add_stack.push_back(
                std::pair<Atom, AddFunc>(
                    a,
                    [=](Atom &a) { new_vec->push(a); }));
        }

        virtual void end_list()
        {
            auto add_pair = m_add_stack.back();
            m_add_stack.pop_back();
            add(add_pair.first);
        }

        virtual void start_map()
        {
            AtomMap *new_map = m_gc->allocate_map();
            Atom a(T_MAP, new_map);

            AtomVec *meta_info = m_gc->allocate_vector(2);
            meta_info->push(Atom(T_STR, m_gc->new_symbol(m_dbg_input_name)));
            meta_info->push(Atom(T_INT, m_dbg_line));
            m_gc->set_meta_register(a, 0, Atom(T_VEC, meta_info));

            std::shared_ptr<std::pair<bool, Atom>> key_data
                = std::make_shared<std::pair<bool, Atom>>(true, Atom());

            m_add_stack.push_back(
                std::pair<Atom, AddFunc>(
                    a,
                    [=](Atom &a)
                    {
                        if (key_data->first)
                        {
                            key_data->second = a;
                            key_data->first = false;
                        }
                        else
                        {
                            new_map->set(key_data->second, a);
                            key_data->first = true;
                        }
                    }));
        }

        virtual void start_kv_pair() { }
        virtual void end_kv_key() { }
        virtual void end_kv_pair() { }

        virtual void end_map()
        {
            auto add_pair = m_add_stack.back();
            m_add_stack.pop_back();
            add(add_pair.first);
        }

        void add(Atom &a)
        {
            m_add_stack.back().second(a);
        }

        virtual void atom_string(const std::string &str)
        {
            Atom a(T_STR);
            a.m_d.sym = m_gc->new_symbol(str);
            add(a);
        }

        virtual void atom_symbol(const std::string &symstr)
        {
            Atom a(T_SYM);
            a.m_d.sym = m_gc->new_symbol(symstr);
            add(a);
        }

        virtual void atom_keyword(const std::string &symstr)
        {
            Atom a(T_KW);
            a.m_d.sym = m_gc->new_symbol(symstr);
            add(a);
        }

        virtual void atom_int(int64_t i)
        {
            Atom a(T_INT);
            a.m_d.i = i;
            add(a);
        }

        virtual void atom_dbl(double d)
        {
            Atom a(T_DBL);
            a.m_d.d = d;
            add(a);
        }

        virtual void atom_nil()
        {
            Atom a(T_NIL);
            add(a);
        }

        virtual void atom_bool(bool b)
        {
            Atom a(T_BOOL);
            a.m_d.b = b;
            add(a);
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
