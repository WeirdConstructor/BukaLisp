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
        VMException(const std::string &err) : m_err(err) { std::cout << "FOO" << err << std::endl; }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~VMException() { }
};
//---------------------------------------------------------------------------

//    X(PUSH_RNG_ENV,   8)
//    X(PUSH_VEC_ENV,   9)

#define OP_CODE_DEF(X) \
    X(NOP,            0) \
    X(MOV,            1) \
    X(MOV_FROM,       2) \
    X(MOV_TO,         3) \
    X(LOAD_STATIC,    4) \
    X(LOAD_PRIM,      5) \
    X(NEW_VEC,        6) \
    X(NEW_MAP,        7) \
    X(CSET_VEC,       8) \
    X(PUSH_ENV,      10) \
    X(POP_ENV,       11) \
    X(DUMP_ENV_STACK,12) \
    X(SET_RETURN,    13) \
    X(CALL,          14) \
    X(NEW_CLOSURE,   15) \
    X(ADD,          100) \
    X(SUB,          101) \
    X(MUL,          102) \
    X(DIV,          103) \
    X(LT,           104) \
    X(GT,           105) \
    X(LE,           106) \
    X(GE,           107) \
    X(EQ,           108) \
    X(NOT,          109) \
    X(RETURN,       250) \
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
        PROG()
            : m_atom_data_len(0), m_instructions(nullptr), m_atom_data(nullptr)
        {
//            std::cout << "*NEW PROG" << ((void *) this) << std::endl;
        }
        PROG(size_t atom_data_len, size_t instr_len)
        {
            m_atom_data_len    = atom_data_len;
            m_atom_data        = new lilvm::Atom[atom_data_len + 1];
            m_instructions_len = instr_len;
            m_instructions     = new INST[instr_len + 1];
            m_instructions[instr_len].op = OP_END;
//            std::cout << "NEW PROG" << ((void *) this) << ";; " << atom_data_len << " ;;" << std::endl;
        }

        size_t data_array_len()   { return m_atom_data_len; }
        lilvm::Atom *data_array() { return m_atom_data; }

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
//            std::cout << "DEL PROG" << ((void *) this) << std::endl;
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
        PROG             *m_prog;
        VM               *m_vm;
        lilvm::AtomVec   *m_root_stack;
        lilvm::AtomVec   *m_env_stack;
        lilvm::AtomVec   *m_cont_stack;
        lilvm::AtomVec   *m_prim_table;

    public:
        VM(Runtime *rt)
            : lilvm::ExternalGCRoot(&(rt->m_gc)), m_rt(rt), m_pc(nullptr),
              m_vm(this)
        {
            m_rt->m_gc.add_external_root(this);
            m_root_stack = rt->m_gc.allocate_vector(10);
            m_env_stack  = rt->m_gc.allocate_vector(100);
            m_cont_stack = rt->m_gc.allocate_vector(100);
            m_prim_table = rt->m_gc.allocate_vector(0);

            m_root_stack->m_len = 0;
            m_cont_stack->m_len = 0;
            m_env_stack->m_len  = 0;

            init_prims();
        }

        void error(const std::string &msg)
        {
            INST *start_pc = &(m_prog->m_instructions[0]);
            lilvm::Atom a(lilvm::T_INT, m_pc - start_pc);
            lilvm::Atom info = m_prog->m_debug_info_map.at(a);
            throw VMException("(@" + info.to_display_str() + "): " + msg);
        }

        void error(const std::string &msg, lilvm::Atom &err_atom)
        {
            error(msg + ", atom: " + err_atom.to_write_str());
        }

        void init_prims();

        virtual size_t gc_root_count() { return 4; }

        virtual lilvm::AtomVec *gc_root_get(size_t idx)
        {
            switch (idx)
            {
                case 0:  return m_root_stack;
                case 1:  return m_env_stack;
                case 2:  return m_cont_stack;
                case 3:  return m_prim_table;
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
