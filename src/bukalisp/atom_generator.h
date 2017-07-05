#pragma once

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

class AtomDebugInfo
{
    private:
        std::unordered_map<void *, std::pair<std::string, size_t>> m_debug_map;
    public:
        void add(void *v, const std::string &name, size_t line)
        {
            m_debug_map[v] = std::pair<std::string, size_t>(name, line);
        }

        std::string pos(void *v)
        {
            auto it = m_debug_map.find(v);
            if (it == m_debug_map.end())
                return "?:?";
            return it->second.first + ":" + std::to_string(it->second.second);
        }
};
//---------------------------------------------------------------------------

class AtomGenerator : public bukalisp::SEX_Builder
{
    private:
        GC                              *m_gc;
        AtomDebugInfo                   *m_deb_info;

        Atom                             m_root;

        typedef std::function<void(Atom &a)>  AddFunc;
        std::vector<std::pair<Atom, AddFunc>> m_add_stack;

    public:
        AtomGenerator(GC *gc, AtomDebugInfo *atd)
            : m_gc(gc), m_deb_info(atd)
        {
            m_add_stack.push_back(
                std::pair<Atom, AddFunc>(m_root, [this](Atom &a) { m_root = a; }));
        }
        virtual ~AtomGenerator() { }

        Atom &root() { return m_root; }

        virtual void start_list(bool quoted = false)
        {
            AtomVec *new_vec = m_gc->allocate_vector(0);
            m_deb_info->add((void *) new_vec, m_dbg_input_name, m_dbg_line);
            Atom a(T_VEC);
            a.m_d.vec = new_vec;

            m_add_stack.push_back(
                std::pair<Atom, AddFunc>(
                    a,
                    [new_vec](Atom &a) { new_vec->push(a); }));
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
            m_deb_info->add((void *) new_map, m_dbg_input_name, m_dbg_line);
            Atom a(T_MAP);
            a.m_d.map = new_map;

            std::shared_ptr<std::pair<bool, std::string>> key_data
                = std::make_shared<std::pair<bool, std::string>>(true, "");

            m_add_stack.push_back(
                std::pair<Atom, AddFunc>(
                    a,
                    [=](Atom &a)
                    {
                        if (key_data->first)
                        {
                            if (   a.m_type != T_SYM
                                && a.m_type != T_KW
                                && a.m_type != T_STR)
                                key_data->second = write_atom(a);
                            else
                                key_data->second = a.m_d.sym->m_str;

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
