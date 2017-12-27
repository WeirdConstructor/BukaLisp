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
        bool                      m_include_debug_info;

        GC_ROOT_MEMBER(m_last);
        GC_ROOT_MEMBER(m_last_key);
        GC_ROOT_MEMBER_VEC(m_stack);
        GC_ROOT_MEMBER_MAP(m_refmap);

        int64_t                   m_next_label;

    public:
        AtomGenerator(GC *gc)
            : m_gc(gc), m_include_debug_info(true),
              GC_ROOT_MEMBER_INITALIZE_VEC(*gc, m_stack),
              GC_ROOT_MEMBER_INITALIZE(*gc, m_last),
              GC_ROOT_MEMBER_INITALIZE(*gc, m_last_key),
              GC_ROOT_MEMBER_INITALIZE_MAP(*gc, m_refmap),
              m_next_label(-1)
        {
            m_stack = gc->allocate_vector(10);
            m_refmap = gc->allocate_map();
        }
        virtual ~AtomGenerator() { }

        void disable_debug_info()
        {
            m_include_debug_info = false;
        }

        void clear()
        {
            m_last.clear();
            m_last_key.clear();
            m_stack->clear();
            m_refmap->clear();
        }

        void start()
        {
            clear();
        }

        Atom &root() { return m_last; }

        virtual void error(const std::string &what,
                           const std::string &inp_name,
                           size_t line,
                           const std::string &tok)
        {
            throw BukaLISPException("reader", inp_name, line, tok, what);
        }

        virtual void label(int64_t lbl_id)
        {
            Atom a = m_refmap->at(Atom(T_INT, lbl_id));
            add(a);
        }

        virtual void next_label(int64_t lbl_id)
        {
            m_next_label = lbl_id;
        }

#define ON_NXT_LBL_SET_REFMAP(atom) \
    if (m_next_label >= 0) \
    { \
        m_refmap->set(Atom(T_INT, m_next_label), (atom)); \
        m_next_label = -1; \
    }

        virtual void start_list()
        {
            AtomVec *new_vec = m_gc->allocate_vector(0);
            Atom new_vec_atom(T_VEC, new_vec);

            ON_NXT_LBL_SET_REFMAP(new_vec_atom);

            if (m_include_debug_info)
            {
                AtomVec *meta_info = m_gc->allocate_vector(2);
                meta_info->push(Atom(T_STR, m_gc->new_symbol(m_dbg_input_name)));
                meta_info->push(Atom(T_INT, m_dbg_line));
                m_gc->set_meta_register(new_vec_atom, 0, Atom(T_VEC, meta_info));
            }

            m_stack->push(new_vec_atom);
        }

        virtual void end_list()
        {
            m_last = *(m_stack->last());
            m_stack->pop();
            add(m_last);
        }

        virtual void start_map()
        {
            AtomMap *new_map = m_gc->allocate_map();
            Atom new_map_atom(T_MAP, new_map);

            ON_NXT_LBL_SET_REFMAP(new_map_atom);

            if (m_include_debug_info)
            {
                AtomVec *meta_info = m_gc->allocate_vector(2);
                meta_info->push(Atom(T_STR, m_gc->new_symbol(m_dbg_input_name)));
                meta_info->push(Atom(T_INT, m_dbg_line));
                m_gc->set_meta_register(new_map_atom, 0, Atom(T_VEC, meta_info));
            }

            m_stack->push(new_map_atom);
        }

        virtual void start_kv_pair() { }
        virtual void end_kv_key()
        {
            m_stack->push(m_last);
            m_stack->push(Atom(T_INT, -1));
        }
        virtual void end_kv_pair()
        {
        }

        virtual void end_map()
        {
            m_last = *(m_stack->last());
            m_stack->pop();
            add(m_last);
        }

        void add(Atom &a)
        {
            // std::cout << "ADD: " << a.to_write_str() << " | " << Atom(T_VEC, m_stack).to_write_str() << std::endl;
            if (m_stack->m_len > 0)
            {
                Atom *elem = m_stack->last();
                if (elem->m_type == T_VEC)
                    elem->m_d.vec->push(a);
                else if (elem->m_type == T_INT)
                {
                    m_stack->pop();
                    if (elem->m_d.i == -1)
                    {
                        Atom key = *(m_stack->last());
                        m_stack->pop();
                        m_stack->last()->m_d.map->set(key, a);
                        m_last = *(m_stack->last());
                    }
                }
                else
                    m_last = a;
            }
            else
                m_last = a;
        }

        virtual void atom_string(const std::string &str)
        {
            Atom a(T_STR);
            a.m_d.sym = m_gc->new_symbol(str);
            ON_NXT_LBL_SET_REFMAP(a);
            add(a);
        }

        virtual void atom_symbol(const std::string &symstr)
        {
            Atom a(T_SYM);
            a.m_d.sym = m_gc->new_symbol(symstr);
            ON_NXT_LBL_SET_REFMAP(a);
            add(a);
        }

        virtual void atom_keyword(const std::string &symstr)
        {
            Atom a(T_KW);
            a.m_d.sym = m_gc->new_symbol(symstr);
            ON_NXT_LBL_SET_REFMAP(a);
            add(a);
        }

        virtual void atom_int(int64_t i)
        {
            Atom a(T_INT);
            a.m_d.i = i;
            ON_NXT_LBL_SET_REFMAP(a);
            add(a);
        }

        virtual void atom_dbl(double d)
        {
            Atom a(T_DBL);
            a.m_d.d = d;
            ON_NXT_LBL_SET_REFMAP(a);
            add(a);
        }

        virtual void atom_nil()
        {
            Atom a(T_NIL);
            ON_NXT_LBL_SET_REFMAP(a);
            add(a);
        }

        virtual void atom_bool(bool b)
        {
            Atom a(T_BOOL);
            a.m_d.b = b;
            ON_NXT_LBL_SET_REFMAP(a);
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
