#include "buklivm.h"
#include "atom_generator.h"
#include <memory>

namespace bukalisp
{
//---------------------------------------------------------------------------

class Value
{
    private:
        GC_ROOT_MEMBER(m_value);

        Value(const Value &)        = delete;
        Value()                     = delete;
        operator=(const Value &)    = delete;
    public:
        Value(GC &gc, const Atom &value)
            : GC_ROOT_MEMBER_INITALIZE(gc, m_value)
        {
            m_value = value;
        }
};
//---------------------------------------------------------------------------

class ValueFactory
{
    private:
        GC          &m_gc;
        GC_ROOT_MEMBER_VEC(m_stack)
        GC_ROOT_MEMBER(m_current)
        bool         m_got_value;

    public:
        ValueFactory(GC &gc)
            : GC_ROOT_MEMBER_INITALIZE_VEC(gc, m_stack),
              GC_ROOT_MEMBER_INITALIZE(gc, m_current),
              m_got_value(false)
        {
            m_stack = m_gc.allocate_vector(0);
        }

        ValueFactory &open_list()
        {
            if (m_got_value)
                m_stack->push(m_current);
            return *this;
        }

        ValueFactory &close()
        {
            if (m_got_value)
                return *this;

            Atom new_cur = m_stack->last();
            m_stack->pop();

            if (m_current.m_type == T_VEC)
            {
                m_current.m_d.vec->push(new_cur);
            }
            else
            {
                m_current = new_cur;
            }
        }

        ValueFactory &operator()(int64_t i)
        { m_current.set_int(i); return *this; }
        ValueFactory &operator()(double i)
        { m_current.set_dbl(i); return *this; }
};
/*

    Instance bkl;

    auto fact = bkl.create_value_factory();

    auto func = bkl.get_global("handle-event");
    if (func.is_nil())
        throw ...;

    auto return =
        func(fact
                (10)
                (20)
                .read("(1 2 + 10 {a: 10 b: 20})")
                .open_list()
                    (1)
                    ("foo")
                    sym("bar")
                .close()
                .value());
*/
//---------------------------------------------------------------------------

typedef std::shared_ptr<Value>          ValuePtr;

//---------------------------------------------------------------------------

class Instance
{
    private:
        Runtime     m_rt;
        VM          m_vm;
        Interpreter m_i;

    public:
        Instance()
            : m_vb(&m_rt), m_i(&m_rt, &m_vm)
        {
        }

        ValueFactoryPtr create_value_factory()
        {
            return std::make_shared<ValueFactory>(m_rt.m_gc);
        }
};
//---------------------------------------------------------------------------

}
