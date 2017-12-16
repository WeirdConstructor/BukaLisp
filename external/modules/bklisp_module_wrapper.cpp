#include "bklisp_module_wrapper.h"

using namespace bukalisp;
using namespace std;
using namespace VVal;

//---------------------------------------------------------------------------

class VVPointer : public bukalisp::UserData
{
    private:
        VVal::VV        m_vv;
    public:
        VVPointer(const VVal::VV &vv) : m_vv(vv) { }
        virtual ~VVPointer() { std::cout << "DEL VVPointer " << this << std::endl; }

        virtual std::string type() { return m_vv->type(); }

        virtual std::string as_string()
        {
            return std::string("#<userdata:vv:" + m_vv->type() + ">");
        }

        void *ptr() { return m_vv->p(m_vv->type()); }
};
//---------------------------------------------------------------------------

//VVGCRootRef::~VVGCRootRef() { m_root->unreg(m_idx); }
////---------------------------------------------------------------------------

Atom vv2atom(bukalisp::VM *vm, const VV &vv)
{
//    cout << "vv2atom: " << vv << endl;

    if (vv->is_keyword())
    {
        return Atom(T_KW, vm->m_rt->m_gc.new_symbol(vv->s()));
    }
    else if (vv->is_symbol())
    {
        return Atom(T_SYM, vm->m_rt->m_gc.new_symbol(vv->s()));
    }
    if (vv->is_string() || vv->is_bytes() || vv->is_datetime())
    {
        return Atom(T_STR, vm->m_rt->m_gc.new_symbol(vv->s()));
    }
    else if (vv->is_undef())
        return Atom();
    else if (vv->is_boolean())
    {
        Atom b(T_BOOL);
        b.m_d.b = vv->is_true();
        return b;
    }
    else if (vv->is_int())
    {
        return Atom(T_INT, vv->i());
    }
    else if (vv->is_double())
    {
        Atom d(T_DBL);
        d.m_d.d = vv->d();
        return d;
    }
    else if (vv->is_list())
    {
        Atom l(T_VEC, vm->m_rt->m_gc.allocate_vector(vv->size()));
        l.m_d.vec->m_len = 0;
        for (auto v : *vv)
            l.m_d.vec->push(vv2atom(vm, v));
        return l;
    }
    else if (vv->is_map())
    {
        Atom m(T_MAP, vm->m_rt->m_gc.allocate_map());
        for (auto pv : *vv)
        {
            m.m_d.map->set(
                Atom(T_KW, vm->m_rt->m_gc.new_symbol(pv->_s(0))),
                vv2atom(vm, pv->_(1)));
        }
        return m;
    }
    else if (vv->is_pointer())
    {
        if (vv->type().empty())
        {
            Atom p(T_C_PTR);
            p.m_d.ptr = vv->p("");
            return p;
        }
        else
        {
            Atom ud(T_UD);
            ud.m_d.ud = new VVPointer(vv);
            //d// std::cout << "VV2ATOM PTR: " << vv->p(vv->type()) << "::" << vv->type() << "::" << ud.m_d.ud << std::endl;
            return ud;
        }
    }
    else if (vv->is_closure())
    {
        // We do this, as I want to avoid memory management for PrimFunc.
        // They are (or should) currently be managed by the VM instance
        // and destroyed with it.
        throw VMException("Can't pass VV closure from module to BukaLISP");
    }
    else
    {
        throw VMException("Unknown VVal type in vv2atom");
    }
}
//---------------------------------------------------------------------------

//VV vm_closure_wrapper(VV &obj, VV &args)
//{
//    VV vv_vm = obj->_(0);
//    if (!vv_vm->is_pointer())
//        throw VMException("Can't call VM closure");
//    VM *vm = (VM *) vv_vm->p("bukalisp::VM");
//    if (!vm)
//        throw VMException("Can't call VM closure, no VM?!");
//
//    VV func = obj->_(1);
//    if (!func->is_pointer())
//        throw VMException("Can't call VM closure");
//
//    Atom a_args = vv2atom(vm, args);
//    if (a_args.m_type != T_VEC)
//    {
//        AtomVec *v = vm->m_rt->m_gc.allocate_vector(1);
//        v->m_data[0] = a_args;
//        a_args = Atom(T_VEC, v);
//    }
//
//    Atom res = vm->eval(a_args.at(0), a_args.m_d.vec);
//    return atom2vv(vm, res);
//}
////---------------------------------------------------------------------------

VV atom2vv(VM *vm, const Atom &a)
{
    switch (a.m_type)
    {
        case T_NIL:     return vv_undef();
        case T_INT:     return vv(a.m_d.i);
        case T_DBL:     return vv(a.m_d.d);
        case T_BOOL:    return vv_bool(a.m_d.b);
        case T_STR:     return vv(a.m_d.sym->m_str);
        case T_SYM:     return vv_sym(a.m_d.sym->m_str);
        case T_KW:      return vv_kw(a.m_d.sym->m_str);
        case T_SYNTAX:  return vv(a.to_write_str());
        case T_VEC:
        {
            AtomVec &v = *a.m_d.vec;
            VV l(vv_list());
            for (size_t i = 0; i < v.m_len; i++)
            {
                l << atom2vv(vm, v.m_data[i]);
            }
            return l;
        }
        case T_MAP:
        {
            VV vmap(vv_map());
            ATOM_MAP_FOR(p, a.m_d.map)
            {
                string key;
                if (   MAP_ITER_KEY(p).m_type == T_STR
                    || MAP_ITER_KEY(p).m_type == T_SYM
                    || MAP_ITER_KEY(p).m_type == T_KW)
                {
                    key = MAP_ITER_KEY(p).m_d.sym->m_str;
                }
                else
                    key = MAP_ITER_KEY(p).to_write_str();
                vmap << vv_kv(key, atom2vv(vm, MAP_ITER_VAL(p)));
            }
            return vmap;
        }
        case T_PRIM:
        case T_CLOS:
        {
            GC_ROOT_SHARED_PTR(vm->m_rt->m_gc, func_ref) = a;

            VVCLSF clos =
                [=](const VV &obj, const VV &args) -> VV
                {
                    GC_ROOT(vm->m_rt->m_gc, a_args) = vv2atom(vm, args);
                    if (a_args.m_type != T_VEC)
                    {
                        Atom t_a_args(T_VEC, vm->m_rt->m_gc.allocate_vector(1));
                        t_a_args.m_d.vec->m_data[0] = a_args;
                        a_args = t_a_args;
                    }

                    return atom2vv(vm,
                                   vm->eval(GC_ROOT_SHP_REF(func_ref),
                                            a_args.m_d.vec));
                };
            return vv_closure(clos, vv_undef());
        }
        case T_UD:
        {
            VVPointer *vvp = dynamic_cast<VVPointer *>(a.m_d.ud);
            //d// std::cout << "ATOM2VV PTR: " << vvp << "," << vvp->ptr() << std::endl;
            if (!vvp)
            {
                throw VMException("Can't convert non VVPointer-UserData "
                                  "in call to primitive!");
            }
            return vv_ptr(vvp->ptr(), vvp->type());
        }
        case T_C_PTR: return vv_ptr(a.m_d.ptr, "");
    }

    return vv_undef();
}
//---------------------------------------------------------------------------

VV atom2vv(VM *vm, AtomVec &a)
{
    return atom2vv(vm, Atom(T_VEC, &a));
}
//---------------------------------------------------------------------------

