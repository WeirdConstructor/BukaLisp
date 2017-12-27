// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "atom_userdata.h"
#include "atom.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

Atom expand_userdata_to_atoms(GC *gc, Atom in, AtomMap *refmap)
{
    if (refmap->at(in).m_type != T_NIL)
        return refmap->at(in);

    switch (in.m_type)
    {
        case T_CLOS:
        {
            AtomVec *out = gc->allocate_vector(in.m_d.vec->m_len + 1);
            refmap->set(in, Atom(T_VEC, out));

            out->set(0, Atom(T_SYM, gc->new_symbol("BKL-CLOSURE")));

            for (size_t i = 0; i < in.m_d.vec->m_len; i++)
            {
                out->set(i + 1,
                    expand_userdata_to_atoms(gc, in.m_d.vec->m_data[i], refmap));
            }
            return Atom(T_VEC, out);
        }
        case T_VEC:
        {
            Atom out;
            AtomVec *out_vec = nullptr;

            if (in.m_d.vec->m_meta)
            {
                AtomVec *meta_vec = gc->allocate_vector(0);
                refmap->set(in, Atom(T_VEC, meta_vec));
                meta_vec->push(Atom(T_SYM, gc->new_symbol("BKL-META")));
                meta_vec->push(
                    expand_userdata_to_atoms(
                        gc, Atom(T_VEC, in.m_d.vec->m_meta), refmap));

                out_vec = gc->allocate_vector(in.m_d.vec->m_len);
                meta_vec->push(Atom(T_VEC, out_vec));
                out = Atom(T_VEC, meta_vec);
            }
            else
            {
                out_vec = gc->allocate_vector(in.m_d.vec->m_len);
                out = Atom(T_VEC, out_vec);
                refmap->set(in, out);
            }


            for (size_t i = 0; i < in.m_d.vec->m_len; i++)
            {
                out_vec->set(i,
                    expand_userdata_to_atoms(gc, in.m_d.vec->m_data[i], refmap));
            }
            return out;
        }
        case T_MAP:
        {
            Atom out;
            AtomMap *out_map = nullptr;

            if (in.m_d.map->m_meta)
            {
                AtomVec *meta_vec = gc->allocate_vector(0);
                refmap->set(in, Atom(T_VEC, meta_vec));
                meta_vec->push(Atom(T_SYM, gc->new_symbol("BKL-META")));
                meta_vec->push(
                    expand_userdata_to_atoms(
                        gc, Atom(T_VEC, in.m_d.map->m_meta), refmap));

                out_map = gc->allocate_map();
                meta_vec->push(Atom(T_MAP, out_map));
                out = Atom(T_VEC, meta_vec);
            }
            else
            {
                out_map = gc->allocate_map();
                out = Atom(T_MAP, out_map);
                refmap->set(in, out);
            }

            ATOM_MAP_FOR(i, in.m_d.map)
            {
                Atom key = expand_userdata_to_atoms(gc, MAP_ITER_KEY(i), refmap);
                Atom val = expand_userdata_to_atoms(gc, MAP_ITER_VAL(i), refmap);
                out_map->set(key, val);
            }
            return out;
        }
        case T_UD:
        {
//            AtomVec *out_ud = gc->allocate_vector(2);
//            refmap->set(in, Atom(T_VEC, out_ud));

            Atom a;
            in.m_d.ud->to_atom(a);
            refmap->set(in, a);
            return expand_userdata_to_atoms(gc, a, refmap);
        }
        default:
            return in;
    }
}

Atom expand_userdata_to_atoms(GC *gc, Atom in)
{
    AtomMap refmap;
    Atom a = expand_userdata_to_atoms(gc, in, &refmap);
    return a;
}
//---------------------------------------------------------------------------

}
