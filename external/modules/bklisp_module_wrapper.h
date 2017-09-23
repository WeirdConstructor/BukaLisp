#pragma once

#include <modules/vval.h>
#include "runtime.h"
#include "buklivm.h"

//---------------------------------------------------------------------------

//class VVGCRoot;
//
//struct VVGCRootRef
//{
//    VVGCRoot *m_root;
//    size_t m_idx;
//
//    VVGCRootRef(VVGCRoot *r, size_t idx) : m_root(r), m_idx(idx) { }
//    ~VVGCRootRef();
//};
//
//typedef std::shared_ptr<VVGCRootRef> VVGCRootRefPtr;
////---------------------------------------------------------------------------
//
//class VVGCRoot : public bukalisp::ExternalGCRoot
//{
//    private:
//        AtomVec               *m_root_set;
//        std::vector<size_t>    m_free;
//
//    public:
//        VVGCRoot(GC *gc)
//            : ExternalGCRoot(gc)
//        {
//            m_root_set = gc->allocate_vector(0);
//        };
//        virtual ~VVGCRoot() { };
//
//        virtual size_t gc_root_count() { return 1; }
//        virtual AtomVec *gc_root_get(size_t idx) { return m_root_set; }
//
//        void unreg(size_t idx)
//        {
//            m_root_set->set(idx, Atom());
//            m_free.push_back(idx);
//        }
//
//        VVGCRootRefPtr reg(Atom a)
//        {
//            if (m_free.size() > 0)
//            {
//                size_t idx = m_free.back();
//                m_free.pop_back();
//                m_root_set->set(idx, a);
//                return
//                    std::make_shared<VVGCRootRef>(this, idx);
//            }
//            else
//            {
//                m_root_set->push(a);
//                return
//                    std::make_shared<VVGCRootRef>(this, m_root_set->m_len - 1);
//            }
//        }
//};
////---------------------------------------------------------------------------


bukalisp::Atom vv2atom(bukalisp::VM *vm, const VVal::VV &vv);
VVal::VV atom2vv(bukalisp::VM *vm, bukalisp::Atom &a);
VVal::VV atom2vv(bukalisp::VM *vm, bukalisp::AtomVec &a);

class BukaLISPModule
{
    private:
        VVal::VV m_module_closures;

    public:
        BukaLISPModule(const VVal::VV &mod_clos)
            : m_module_closures(mod_clos)
        { }

        bukalisp::Atom module_name(bukalisp::VM *vm)
        {
            return
                bukalisp::Atom(bukalisp::T_SYM,
                    vm->m_rt->m_gc.new_symbol(m_module_closures->_s(0)));
        }

        size_t function_count()
        {
            return m_module_closures->_(1)->size();
        }

        bukalisp::Atom get_func_name(bukalisp::VM *vm, size_t idx)
        {
            if (idx >= (size_t) m_module_closures->_(1)->size())
                return bukalisp::Atom();

            return
                bukalisp::Atom(bukalisp::T_SYM,
                    vm->m_rt->m_gc.new_symbol(m_module_closures->_(1)->_(idx)->_s(0)));
        }

        bukalisp::Atom get_func(bukalisp::VM *vm, size_t idx)
        {
            if (idx >= (size_t) m_module_closures->_(1)->size())
                return bukalisp::Atom();

            VVal::VV closure = m_module_closures->_(1)->_(idx)->_(1);

            auto func = [=](bukalisp::AtomVec &args, bukalisp::Atom &out)
            {
                out = vv2atom(vm, closure->call(atom2vv(vm, args)));
            };

            auto func_ptr = new bukalisp::Atom::PrimFunc;
            (*func_ptr) = func;

            return bukalisp::Atom(bukalisp::T_PRIM, func_ptr);
        }
};
