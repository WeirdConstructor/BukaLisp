// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include "config.h"
#include "atom.h"

//---------------------------------------------------------------------------

namespace bukalisp
{
//---------------------------------------------------------------------------

#define OP_CODE_DEF(X) \
    X(NOP,            0) /* ()                                     */ \
    X(MOV,            1) /*                                        */ \
    X(MOV_FROM,       2) /*                                        */ \
    X(MOV_TO,         3) /*                                        */ \
    X(LOAD_PRIM,      5) /*                                        */ \
    X(LOAD_NIL,       6) /*                                        */ \
    X(NEW_VEC,        7) /*                                        */ \
    X(NEW_MAP,        8) /*                                        */ \
    X(CSET_VEC,       9) /*                                        */ \
    X(NEW_UPV,       10) /* (O: upvalue-adr)                       */ \
    X(EVAL,          11) /*                                        */ \
    X(DUMP_ENV_STACK,13) /*                                        */ \
    X(CALL,          15) /*                                        */ \
    X(NEW_CLOSURE,   16) /* (O: closure A: VM-PROG B: is_coro?)    */ \
    X(BRNIF,         17) /*                                        */ \
    X(BRIF,          18) /*                                        */ \
    X(BR,            19) /*                                        */ \
    X(FORINC,        20) /* (O: (cond+1=iter) A: cur   B: end)     */ \
    X(SET,           21) /*                                        */ \
    X(GET,           22) /*                                        */ \
    X(ITER,          23) /*                                        */ \
    X(NEXT,          24) /*                                        */ \
    X(IKEY,          25) /*                                        */ \
    X(YIELD,         26) /* (O: cont-val A: ret-val)               */ \
    X(NEW_ARG_VEC,   27) /*                                        */ \
    X(PUSH_JMP,      28) /*                                        */ \
    X(POP_JMP,       29) /*                                        */ \
    X(CTRL_JMP,      30) /*                                        */ \
    X(PUSH_CLNUP,    31) /*                                        */ \
    X(POP_CLNUP,     32) /*                                        */ \
    X(GET_CORO,      33) /*                                        */ \
    X(STACK_TRC,     34) /*                                        */ \
    X(ADD,          100) /*                                        */ \
    X(SUB,          101) /*                                        */ \
    X(MUL,          102) /*                                        */ \
    X(DIV,          103) /*                                        */ \
    X(MOD,          104) /*                                        */ \
    X(LT,           105) /*                                        */ \
    X(GT,           106) /*                                        */ \
    X(LE,           107) /*                                        */ \
    X(GE,           108) /*                                        */ \
    X(EQ,           109) /*                                        */ \
    X(NEQ,          110) /*                                        */ \
    X(NOT,          111) /*                                        */ \
    X(EQV,          112) /*                                        */ \
    X(ISNIL,        113) /*                                        */ \
    X(RETURN,       250) /*                                        */ \
    X(END,          254) /*                                        */

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
    uint8_t op;
    int32_t  o;
    int32_t  a;
    int32_t  b;
    int32_t  c;
    int8_t  oe;
    int8_t  ae;
    int8_t  be;
    int8_t  ce;

    static std::string regidx2string(int32_t i, int8_t e);

    void from_atom(Atom at)
    {
        op = op_from_name(at.at(0).to_display_str());
        o  = (int32_t) at.at(1).to_int();
        oe = (int8_t)  at.at(2).to_int();
        a  = (int32_t) at.at(3).to_int();
        ae = (int8_t)  at.at(4).to_int();
        b  = (int32_t) at.at(5).to_int();
        be = (int8_t)  at.at(6).to_int();
        c  = (int32_t) at.at(7).to_int();
        ce = (int8_t)  at.at(8).to_int();
    }

    Atom to_atom(GC &gc) const
    {
        AtomVec *av = gc.allocate_vector(9);
        av->m_len = 9;
        av->m_data[0] = Atom(T_KW, gc.new_symbol(get_op_name()));
        av->m_data[1].set_int(o);
        av->m_data[2].set_int(oe);
        av->m_data[3].set_int(a);
        av->m_data[4].set_int(ae);
        av->m_data[5].set_int(b);
        av->m_data[6].set_int(be);
        av->m_data[7].set_int(c);
        av->m_data[8].set_int(ce);
        return Atom(T_VEC, av);
    }

    static uint8_t op_from_name(const std::string &opname)
    {
#       define X(name, code)    if ((opname) == #name) { return code; } else
        OP_CODE_DEF(X)
        {
            return -1;
        }
#       undef X
    }

    std::string get_op_name() const
    {
        std::string op_name;
#       define X(name, code)    case code: op_name = #name; break;
        switch (op) { OP_CODE_DEF(X) }
#       undef X
        return op_name;
    }

    INST() { clear(); }
    void clear()
    {
        op = 0;
        o  = 0;
        oe = 0;
        a  = 0;
        b  = 0;
        c  = 0;
        ae = 0;
        be = 0;
        ce = 0;
//        va.i = 0;
    }
};
//---------------------------------------------------------------------------

class PROG : public UserData
{
    public:
        AtomVec *m_data_vec;
        AtomVec *m_root_regs;

        Atom     m_debug_info_map;
        Atom     m_atom_data;
        INST    *m_instructions;
        size_t   m_instructions_len;
        std::string m_function_info;
        GC      *m_gc;

    public:
        static Atom create_prog_from_info(GC &gc, Atom prog_info, AtomMap *refmap = nullptr);
        static Atom repack_expanded_userdata(GC &gc, Atom a, AtomMap *refmap);

        PROG()
            : m_instructions(nullptr), m_gc(nullptr), m_instructions_len(0)
        {
//            std::cout << "*NEW PROG" << ((void *) this) << std::endl;
        }
        PROG(GC &gc, size_t atom_data_len, size_t instr_len)
            : m_gc(&gc)
        {
            m_atom_data.set_vec(gc.allocate_vector(atom_data_len));
            m_data_vec = m_atom_data.m_d.vec;
            m_instructions_len = instr_len;
            m_instructions     = new INST[instr_len + 1];
            m_instructions[instr_len].op = OP_END;
//            std::cout << "NEW PROG" << ((void *) this) << ";; " << atom_data_len << " ;;" << std::endl;
        }

        static std::string pc2str(Atom proc, INST *pc);
        static std::string pc2str(PROG *prog, INST *pc);

        std::string func_info_at(INST *pc, Atom &info);

        virtual void to_atom(Atom &a)
        {
            AtomVec *av = m_gc->allocate_vector(6);
            av->m_data[0] = Atom(T_SYM, m_gc->new_symbol("BKL-VM-PROG"));
            av->m_data[1] = Atom(T_STR, m_gc->new_symbol(m_function_info));
            av->m_data[2] = m_atom_data;

            AtomVec *instr = m_gc->allocate_vector(m_instructions_len);
            instr->m_len = m_instructions_len;
            for (size_t i = 0; i < m_instructions_len; i++)
                instr->m_data[i] = m_instructions[i].to_atom(*m_gc);
            av->m_data[3].set_vec(instr);

            av->m_data[4] = m_debug_info_map;
            av->m_data[5].set_vec(m_root_regs);

            av->m_len = 6;

            a.set_vec(av);
        }

        void set_root_env(AtomVec *root_regs)
        {
            m_root_regs = root_regs;
        }

        void set_debug_info(Atom &a)
        {
            m_debug_info_map = a;
        }

        void set(size_t idx, const INST &i)
        {
            if (idx >= m_instructions_len)
                return;
            m_instructions[idx] = i;
        }

        virtual std::string type() { return "BKL-VM-PROG"; }
        virtual std::string as_string(bool pretty = false);

        virtual void mark(GC *gc, uint8_t clr)
        {
            UserData::mark(gc, clr);
            gc->mark_atom(m_atom_data);
            gc->mark_atom(m_debug_info_map);
            gc->mark_atom(Atom(T_VEC, m_root_regs));
        }

        virtual ~PROG()
        {
            if (m_instructions)
                delete[] m_instructions;
//            std::cout << "DEL PROG" << ((void *) this) << std::endl;
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
