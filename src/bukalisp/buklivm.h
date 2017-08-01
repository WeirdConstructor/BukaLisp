#pragma once

#include "atom.h"
#include "atom_userdata.h"
#include "runtime.h"
#include <sstream>

namespace bukalisp
{
//---------------------------------------------------------------------------

class VMException : public std::exception
{
    private:
        std::string m_err;
    public:
        VMException(const std::string &err) : m_err(err) { }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~VMException() { }
};
//---------------------------------------------------------------------------

#define OP_CODE_DEF(X) \
    X(NOP,            0) \
    X(LOAD,           1) \
    X(LOAD_STATIC,    2) \
    X(PUSH_ENV,       3) \
    X(POP_ENV,        4) \
    X(DUMP_ENV_STACK, 5) \
    X(END,          254)

enum OPCODE : uint8_t
{
#define X(name, code)   OP_##name = code,
OP_CODE_DEF(X)
#undef X
    END_OPCODE
};

//---------------------------------------------------------------------------

struct INST
{
    uint8_t  op;
    uint8_t  m;
    uint16_t o;
    union {
        struct {
            uint16_t a;
            uint16_t b;
        } l;
        struct {
            uint32_t a;
        } x;
    } _;

    void to_string(std::ostream &ss)
    {
        std::string op_name;
#       define X(name, code)    case code: op_name = #name; break;
        switch (op) { OP_CODE_DEF(X) }
#       undef X
        ss << "#" << op_name << ": (" << o << ", [" << _.l.a << ":" << _.l.b << "](" << _.x.a << "))";
    }

    INST() { clear(); }
    void clear()
    {
        op    = 0;
        m     = 0;
        o     = 0;
        _.x.a = 0;
    }
};
//---------------------------------------------------------------------------

class PROG : public lilvm::UserData
{
    public:
        lilvm::Atom     m_debug_info_map;
        lilvm::Atom    *m_atom_data;
        size_t          m_atom_data_len;
        INST           *m_instructions;
        size_t          m_instructions_len;

    public:
        PROG(size_t atom_data_len, size_t instr_len)
        {
            m_atom_data_len    = atom_data_len;
            m_atom_data        = new lilvm::Atom[atom_data_len];
            m_instructions_len = instr_len;
            m_instructions     = new INST[instr_len + 1];
            m_instructions[instr_len].op = OP_END;
        }

        lilvm::Atom data_at(size_t idx)
        {
            if (idx >= m_atom_data_len)
                return lilvm::Atom();
            return m_atom_data[idx];
        }

        void set_data_from(lilvm::AtomVec *av)
        {
            for (size_t i = 0; i < av->m_len && i < m_atom_data_len; i++)
                m_atom_data[i] = av->m_data[i];
        }

        void set_debug_info(lilvm::Atom &a)
        {
            m_debug_info_map = a;
        }

        void set(size_t idx, const INST &i)
        {
            if (idx >= m_instructions_len)
                return;
            m_instructions[idx] = i;
        }

        virtual std::string type() { return "VM-PROG"; }
        virtual std::string as_string();

        virtual void mark(lilvm::GC *gc, uint8_t clr)
        {
            lilvm::UserData::mark(gc, clr);
            for (size_t i = 0; i < m_atom_data_len; i++)
                gc->mark_atom(m_atom_data[i]);
            gc->mark_atom(m_debug_info_map);
        }

        virtual ~PROG()
        {
            if (m_atom_data)    delete[] m_atom_data;
            if (m_instructions) delete[] m_instructions;
        }
};
//---------------------------------------------------------------------------

lilvm::Atom make_prog(lilvm::Atom prog_info);
//---------------------------------------------------------------------------

class VM : public lilvm::ExternalGCRoot
{
    private:
        Runtime          *m_rt;
        INST             *m_pc;
        lilvm::AtomVec   *m_root_stack;
        lilvm::AtomVec   *m_env_stack;
        lilvm::AtomVec   *m_cont_stack;

    public:
        VM(Runtime *rt)
            : lilvm::ExternalGCRoot(&(rt->m_gc)), m_rt(rt), m_pc(nullptr)
        {
            m_rt->m_gc.add_external_root(this);
            m_root_stack = rt->m_gc.allocate_vector(0);
            m_env_stack  = rt->m_gc.allocate_vector(0);
            m_cont_stack = rt->m_gc.allocate_vector(0);
        }

        virtual size_t gc_root_count() { return 3; }

        virtual lilvm::AtomVec *gc_root_get(size_t idx)
        {
            switch (idx)
            {
                case 0:  return m_root_stack;
                case 1:  return m_env_stack;
                case 2:  return m_cont_stack;
                default: return m_cont_stack;
            }
        }

        virtual ~VM()
        {
            m_rt->m_gc.remove_external_root(this);
        }

        lilvm::Atom eval(lilvm::Atom at_ud, lilvm::AtomVec *args = nullptr);
};
//---------------------------------------------------------------------------

};
