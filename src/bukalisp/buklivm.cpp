#include "buklivm.h"
#include "atom_printer.h"

using namespace lilvm;
using namespace std;

namespace bukalisp
{
//---------------------------------------------------------------------------

std::string PROG::as_string()
{
    std::stringstream ss;
    ss << "#<prog:\n";
    for (size_t i = 0; i < m_atom_data_len; i++)
    {
        ss << " [" << i << "]: " << write_atom(m_atom_data[i]) << "\n";
    }

    for (size_t i = 0; i < m_instructions_len; i++)
    {
        ss << " {" << i << "}: ";
        m_instructions[i].to_string(ss);
        ss << "\n";
    }
    ss << ">";
    return ss.str();
}
//---------------------------------------------------------------------------

void parse_op_desc(INST &instr, Atom &desc)
{
    if (desc.m_type != T_VEC)
        return;
    AtomVec *av = desc.m_d.vec;
    if (av->m_len < 3)
        return;

    std::string op_name;
    Atom op_name_s = av->m_data[0];
    if (   op_name_s.m_type != T_SYM
        && op_name_s.m_type != T_KW)
        return;

    op_name = op_name_s.m_d.sym->m_str;

    uint8_t op_code = 0;

#define X(name, code) else if (op_name == #name) op_code = code;
    if (op_name == "***") op_code = 0;
    OP_CODE_DEF(X)
#undef X

    instr.op = op_code;
    instr.o  = (uint16_t) av->m_data[1].to_int();
    if (av->m_len == 4)
    {
        instr._.l.a = (uint16_t) av->m_data[2].to_int();
        instr._.l.b = (uint16_t) av->m_data[3].to_int();
    }
    else if (av->m_len == 3)
    {
        instr._.x.a = (uint32_t) av->m_data[2].to_int();
    }
}
//---------------------------------------------------------------------------

lilvm::Atom make_prog(lilvm::Atom prog_info)
{
    if (prog_info.m_type != T_VEC)
        return Atom();

    Atom data  = prog_info.m_d.vec->m_data[0];
    Atom prog  = prog_info.m_d.vec->m_data[1];
    Atom debug = prog_info.m_d.vec->m_data[2];

    if (   data.m_type  != T_VEC
        || prog.m_type  != T_VEC
        || debug.m_type != T_MAP)
        return Atom();

    PROG *new_prog = new PROG(data.m_d.vec->m_len, prog.m_d.vec->m_len);
    new_prog->set_debug_info(debug);
    new_prog->set_data_from(data.m_d.vec);

    for (size_t i = 0; i < prog.m_d.vec->m_len; i++)
    {
        INST instr;
        parse_op_desc(instr, prog.m_d.vec->m_data[i]);
        new_prog->set(i, instr);
    }

    Atom ud(T_UD);
    ud.m_d.ud = new_prog;
    Atom newud = ud;
    return ud;
}
//---------------------------------------------------------------------------

lilvm::Atom VM::eval(PROG &p, AtomVec *args)
{
    Atom ret;

    m_pc = &(p.m_instructions[0]);

    while (m_pc->op != OP_END)
    {
        switch (m_pc->op)
        {
            default:
                throw VMException("Unknown VM opcode: " + to_string(m_pc->op));
        }
        m_pc++;
    }

    return ret;
}
//---------------------------------------------------------------------------


}
