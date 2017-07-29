#pragma once

#include <memory>
#include <vector>
#include "bukalisp/parser.h"
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
        lilvm::GC                       *m_gc;
        lilvm::AtomMap                  *m_debug_map;

        lilvm::Atom                      m_root;

        typedef std::function<void(lilvm::Atom &a)>  AddFunc;
        std::vector<std::pair<lilvm::Atom, AddFunc>> m_add_stack;

    public:
        AtomGenerator(lilvm::GC *gc)
            : m_gc(gc)
        {
        }
        virtual ~AtomGenerator() { }

        void start()
        {
            m_add_stack.push_back(
                std::pair<lilvm::Atom, AddFunc>(
                    m_root, [this](lilvm::Atom &a) { m_root = a; }));
        }

        lilvm::Atom &root() { return m_root; }

        void clear_debug_info() { m_debug_map = nullptr; }
        lilvm::AtomMap *debug_info() { return m_debug_map; }

        void add_debug_info(int64_t id, const std::string &input_name, size_t line)
        {
            if (!m_debug_map)
                m_debug_map = m_gc->allocate_map();
            std::string info = input_name + ":" + std::to_string(line);
            m_debug_map->set(
                lilvm::Atom(lilvm::T_INT, id),
                lilvm::Atom(lilvm::T_STR, m_gc->new_symbol(info)));
        }

        virtual void start_list()
        {
            lilvm::AtomVec *new_vec = m_gc->allocate_vector(0);
            lilvm::Atom a(lilvm::T_VEC);
            a.m_d.vec = new_vec;
            add_debug_info(a.id(), m_dbg_input_name, m_dbg_line);

            m_add_stack.push_back(
                std::pair<lilvm::Atom, AddFunc>(
                    a,
                    [new_vec](lilvm::Atom &a) { new_vec->push(a); }));
        }

        virtual void end_list()
        {
            auto add_pair = m_add_stack.back();
            m_add_stack.pop_back();
            add(add_pair.first);
        }

        virtual void start_map()
        {
            lilvm::AtomMap *new_map = m_gc->allocate_map();
            lilvm::Atom a(lilvm::T_MAP);
            a.m_d.map = new_map;
            add_debug_info(a.id(), m_dbg_input_name, m_dbg_line);

            std::shared_ptr<std::pair<bool, lilvm::Atom>> key_data
                = std::make_shared<std::pair<bool, lilvm::Atom>>(true, lilvm::Atom());

            m_add_stack.push_back(
                std::pair<lilvm::Atom, AddFunc>(
                    a,
                    [=](lilvm::Atom &a)
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

        void add(lilvm::Atom &a)
        {
            m_add_stack.back().second(a);
        }

        virtual void atom_string(const std::string &str)
        {
            lilvm::Atom a(lilvm::T_STR);
            a.m_d.sym = m_gc->new_symbol(str);
            add(a);
        }

        virtual void atom_symbol(const std::string &symstr)
        {
            lilvm::Atom a(lilvm::T_SYM);
            a.m_d.sym = m_gc->new_symbol(symstr);
            add(a);
        }

        virtual void atom_keyword(const std::string &symstr)
        {
            lilvm::Atom a(lilvm::T_KW);
            a.m_d.sym = m_gc->new_symbol(symstr);
            add(a);
        }

        virtual void atom_int(int64_t i)
        {
            lilvm::Atom a(lilvm::T_INT);
            a.m_d.i = i;
            add(a);
        }

        virtual void atom_dbl(double d)
        {
            lilvm::Atom a(lilvm::T_DBL);
            a.m_d.d = d;
            add(a);
        }

        virtual void atom_nil()
        {
            lilvm::Atom a(lilvm::T_NIL);
            add(a);
        }

        virtual void atom_bool(bool b)
        {
            lilvm::Atom a(lilvm::T_BOOL);
            a.m_d.b = b;
            add(a);
        }
};
//---------------------------------------------------------------------------

}
