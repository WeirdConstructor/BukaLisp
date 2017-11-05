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
        Value &operator=(const Value &)    = delete;
    public:
        Value(GC &gc, const Atom &value)
            : GC_ROOT_MEMBER_INITALIZE(gc, m_value)
        {
            m_value = value;
        }

        std::string to_write_str() const { return m_value.to_write_str(); }
};
//---------------------------------------------------------------------------

typedef std::shared_ptr<Value>          ValuePtr;
class ValueFactory;
typedef std::shared_ptr<ValueFactory>   ValueFactoryPtr;

//---------------------------------------------------------------------------

class ValueFactory
{
    private:
        GC           &m_gc;
        AtomGenerator m_ag;

    public:
        ValueFactory(GC &gc)
            : m_gc(gc), m_ag(&gc)
        {
            m_ag.start();
        }

        ValueFactory &open_list()
        {
            m_ag.start_list();
            return *this;
        }

        ValueFactory &close_list()
        {
            m_ag.end_list();
            return *this;
        }

        ValueFactory &operator()(int i)
        {
            m_ag.atom_int(i);
            return *this;
        }
        ValueFactory &operator()(int64_t i)
        {
            m_ag.atom_int(i);
            return *this;
        }
        ValueFactory &operator()(double i)
        {
            m_ag.atom_dbl(i);
            return *this;
        }
        ValueFactory &operator()()
        {
            m_ag.atom_nil();
            return *this;
        }
        ValueFactory &operator()(bool b)
        {
            m_ag.atom_bool(b);
            return *this;
        }

        ValuePtr value()
        {
            auto val = std::make_shared<Value>(m_gc, m_ag.root());
            m_ag.start();
            return val;
        }
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


class Instance
{
    private:
        Runtime     m_rt;
        VM          m_vm;
        Interpreter m_i;

    public:
        Instance()
            : m_vm(&m_rt), m_i(&m_rt, &m_vm)
        {
        }

        ValueFactoryPtr create_value_factory()
        {
            return std::make_shared<ValueFactory>(m_rt.m_gc);
        }
};
//---------------------------------------------------------------------------

}
