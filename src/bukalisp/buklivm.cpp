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
    {
        cout << "Too few elements in op desc (3 needed): "
             << desc.to_write_str() << endl;
        return;
    }

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
    else
    {
        cout << "unknown op name: " << op_name << endl;
    }
#undef X

    cout << "op code: " << op_code << endl;
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

lilvm::Atom VM::eval(Atom at_ud, AtomVec *args)
{
    Atom ret;

    if (at_ud.m_type != T_UD || at_ud.m_d.ud->type() != "VM-PROG")
    {
//        error("Bad input type to run-vm-prog, expected VM-PROG.", A0);
        cout << "VM-ERROR: Bad input type to run-vm-prog, expected VM-PROG."
             << at_ud.to_write_str() << endl;
        return Atom();
    }

    AtomVecPush avpvmprog(m_root_stack, at_ud);
    AtomVecPush avpvmargs(m_root_stack, Atom(T_VEC, args));

    PROG *p         = dynamic_cast<PROG*>(at_ud.m_d.ud);
    Atom *data      = p->data_array();
    size_t data_len = p->data_array_len();

    m_pc = &(p->m_instructions[0]);

    AtomVec *cur_env = args;

    while (m_pc->op != OP_END)
    {
        switch (m_pc->op)
        {
            case OP_DUMP_ENV_STACK:
            {
                for (size_t i = m_env_stack->m_len; i > 0; i--)
                {
                    cout << "ENV@" << (i - 1) << ": ";
                    AtomVec *env = m_env_stack->at(i - 1).m_d.vec;
                    for (size_t j = 0; j < env->m_len; j++)
                        cout << "[" << j << "=" << env->at(j).to_write_str() << "] ";
                    cout << endl;
                }
                break;
            }

            case OP_LOAD_STATIC:
            {
                size_t idx = m_pc->_.x.a;
                cur_env->set(m_pc->o, idx < data_len ? data[idx] : Atom());
                break;
            }

            case OP_PUSH_ENV:
            {
                m_env_stack->push(
                    Atom(T_VEC, cur_env = m_rt->m_gc.allocate_vector(m_pc->_.x.a)));
                break;
            }

            case OP_POP_ENV:
            {
                m_env_stack->pop();
                Atom *l = m_env_stack->last();
                if (l) cur_env = l->m_d.vec;
                else   cur_env = args;
                break;
            }

            case OP_SET_RETURN:
            {
                ret = cur_env->at(m_pc->_.x.a);
                break;
            }

            default:
                throw VMException("Unknown VM opcode: " + to_string(m_pc->op));
        }
        m_pc++;
    }

    return ret;
}
//---------------------------------------------------------------------------


}
