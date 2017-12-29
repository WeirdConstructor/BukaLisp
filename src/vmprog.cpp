// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "vmprog.h"
#include "config.h"
#include "buklivm.h"
//#include "atom_printer.h"
//#include "atom_cpp_serializer.h"
#include "util.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

std::string PROG::pc2str(PROG *prog, INST *pc)
{
    size_t pc_idx = pc - &(prog->m_instructions[0]);
    return "(" + std::to_string(pc_idx)
           + " "
           + std::to_string(prog->m_instructions_len - 1)
           + ")";
}
//---------------------------------------------------------------------------

std::string PROG::pc2str(Atom proc, INST *pc)
{
    if (proc.m_type != T_UD || proc.m_d.ud->type() != "BKL-VM-PROG")
        return "(? ?)";

    PROG *prog = static_cast<PROG*>(proc.m_d.ud);
    return PROG::pc2str(prog, pc);
}
//---------------------------------------------------------------------------

std::string PROG::func_info_at(INST *pc, Atom &info)
{
    size_t pc_idx = pc - &(m_instructions[0]);
    Atom a(T_INT, pc_idx);
    info = m_debug_info_map.at(a);

    std::string func_info = m_function_info;
    if (info.at(2).m_type != T_NIL
        && !info.at(2).to_display_str().empty())
    {
        func_info =
            "(" + func_info
            + " "
            + info.at(2).to_display_str() + ")";
    }

    return func_info;
}
//---------------------------------------------------------------------------

std::string INST::regidx2string(int32_t i, int8_t e)
{
    std::string pref;
    switch (e)
    {
        case REG_ROW_FRAME:   pref = "FRM"; break;
        case REG_ROW_DATA:    pref = "DAT"; break;
        case REG_ROW_PRIM:    pref = "PRM"; break;
        case REG_ROW_UPV:     pref = "UPV"; break;
        case REG_ROW_ROOT:    pref = "ROT"; break;
        case REG_ROW_RREF:    pref = "REF"; break;
        case REG_ROW_SPECIAL: pref = "SPC"; break;
    }

    return "(" + pref + " " + std::to_string(i) + ")";
}
//---------------------------------------------------------------------------

std::string PROG::as_string(bool pretty)
{
    Atom a;
    to_atom(a);
    return a.to_write_str(pretty);
}
//---------------------------------------------------------------------------

Atom PROG::repack_expanded_userdata(GC &gc, Atom a, AtomMap *refmap)
{
    if (refmap->at(a).m_type != T_NIL)
        return refmap->at(a);

    if (a.m_type == T_VEC)
    {
        if (a.at(0).m_type == T_SYM)
        {
            auto first = a.at(0).m_d.sym->m_str;

            if (first == "BKL-VM-PROG")
                return PROG::create_prog_from_info(gc, a, refmap);
            else if (first == "BKL-META")
            {
                Atom a_meta = a.at(1);
                Atom a_data = a.at(2);

                if (a_meta.m_type == T_VEC)
                {
                    if (a_data.m_type == T_VEC)
                    {
                        Atom out(T_VEC, gc.allocate_vector(a_data.m_d.vec->m_len));
                        refmap->set(a, out);
                        Atom meta = repack_expanded_userdata(gc, a_meta, refmap);
                        out.m_d.vec->m_meta = meta.m_d.vec;
                        Atom data = repack_expanded_userdata(gc, a_data, refmap);

                        for (size_t i = 0; i < data.m_d.vec->m_len; i++)
                            out.m_d.vec->set(i, data.m_d.vec->m_data[i]);

                        return out;
                    }
                    else if (a_data.m_type = T_MAP)
                    {
                        Atom out(T_MAP, gc.allocate_map());
                        refmap->set(a, out);
                        Atom meta = repack_expanded_userdata(gc, a_meta, refmap);
                        out.m_d.map->m_meta = meta.m_d.vec;
                        Atom data = repack_expanded_userdata(gc, a_data, refmap);

                        ATOM_MAP_FOR(i, data.m_d.map)
                        {
                            out.m_d.map->set(MAP_ITER_KEY(i), MAP_ITER_VAL(i));
                        }

                        return out;
                    }
                    else
                        throw BukaLISPException(
                            "'bkl-make-vm-prog' bad BKL-META, "
                            "did not assign meta to a vector or map!");
                }
                else
                {
                    throw BukaLISPException(
                        "'bkl-make-vm-prog' bad BKL-META, "
                        "did not assign a vector!");
                }
            }
            else if (first == "BKL-CLOSURE")
            {
                Atom out(T_CLOS, gc.clone_vector(a.m_d.vec));
                refmap->set(a, out);

                for (size_t i = 0; i < out.m_d.vec->m_len; i++)
                {
                    out.m_d.vec->m_data[i] =
                        repack_expanded_userdata(gc, a.m_d.vec->m_data[i], refmap);
                }

                out.m_d.vec->shift(); // remove BKL-CLOSURE

                return out;
            }

            // vvv--- fall through to loop ---vvv
        }

        Atom out(T_VEC, gc.clone_vector(a.m_d.vec));
        refmap->set(a, out);

        for (size_t i = 0; i < out.m_d.vec->m_len; i++)
        {
            out.m_d.vec->m_data[i] =
                repack_expanded_userdata(gc, a.m_d.vec->m_data[i], refmap);
        }

        return out;
    }
    else if (a.m_type == T_MAP)
    {
        Atom out(T_MAP, gc.allocate_map());
        refmap->set(a, out);

        ATOM_MAP_FOR(i, a.m_d.map)
        {
            Atom key = repack_expanded_userdata(gc, MAP_ITER_KEY(i), refmap);
            Atom val = repack_expanded_userdata(gc, MAP_ITER_VAL(i), refmap);
            out.m_d.map->set(key, val);
        }

        return out;
    }

    return a;
}
//---------------------------------------------------------------------------

Atom PROG::create_prog_from_info(GC &gc, Atom prog_info, AtomMap *refmap)
{
    if (prog_info.m_type != T_VEC)
        throw BukaLISPException(
            "'bkl-make-vm-prog' can only operate on a list");

    if (prog_info.m_d.vec->m_len < 5)
        throw BukaLISPException(
            "'bkl-make-vm-prog' needs a list with at least 5 elements");

    Atom tag = prog_info.m_d.vec->m_data[0];
    if (   tag.m_type == T_VEC
        && tag.at(0).m_type == T_SYM
        && tag.at(0).m_d.sym->m_str == "BKL-VM-PROG")
        throw BukaLISPException(
            "'bkl-make-vm-prog' expected 'BKL-VM-PROG' tag as first element");

    Atom func_info = prog_info.m_d.vec->m_data[1];
    Atom data      = prog_info.m_d.vec->m_data[2];
    Atom prog      = prog_info.m_d.vec->m_data[3];
    Atom debug     = prog_info.m_d.vec->m_data[4];
    Atom root_regs = prog_info.m_d.vec->m_data[5];

//    std::cout << "PROG START " << func_info.to_display_str() << " RooT-ID:" << root_regs.id() << std::endl;

    if (data.m_type != T_VEC)
        throw BukaLISPException("'bkl-make-vm-prog' bad data element @2");

    if (prog.m_type != T_VEC)
        throw BukaLISPException("'bkl-make-vm-prog' bad code element @3");

    PROG *new_prog = new PROG(gc, data.m_d.vec->m_len, prog.m_d.vec->m_len);
    gc.reg_userdata(new_prog);

    Atom ret(T_UD);
    ret.m_d.ud = new_prog;
    if (refmap)
        refmap->set(prog_info, ret);

    if (refmap)
    {
        debug     = repack_expanded_userdata(gc, debug, refmap);
        root_regs = repack_expanded_userdata(gc, root_regs, refmap);
    }

    if (debug.m_type != T_MAP)
        throw BukaLISPException("'bkl-make-vm-prog' bad debug element @4");

    if (root_regs.m_type != T_VEC)
        throw BukaLISPException("'bkl-make-vm-prog' bad root-regs element @5");

    if (!root_regs.m_d.vec->m_meta)
        throw BukaLISPException("'bkl-make-vm-prog' root regs dont have meta");

    new_prog->set_root_env(root_regs.m_d.vec);
    new_prog->set_debug_info(debug);
    new_prog->m_function_info = func_info.to_display_str();

    if (refmap)
    {
        for (size_t i = 0; i < data.m_d.vec->m_len; i++)
        {
            Atom d =
                PROG::repack_expanded_userdata(
                    gc, data.m_d.vec->m_data[i], refmap);
            new_prog->m_atom_data.m_d.vec->set(i, d);
        }
    }
    else
    {
        for (size_t i = 0; i < data.m_d.vec->m_len; i++)
        {
            new_prog->m_atom_data.m_d.vec->set(i, data.m_d.vec->m_data[i]);
        }
    }

    for (size_t i = 0; i < prog.m_d.vec->m_len; i++)
    {
        INST instr;
        instr.from_atom(prog.m_d.vec->m_data[i]);
        new_prog->set(i, instr);
    }

    return ret;
}
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
