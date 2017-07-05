#include "lilvm.h"
#include <chrono>

using namespace std;

int test() { return 11; }

namespace lilvm
{
//---------------------------------------------------------------------------

const char *OPCODE_NAMES[] = {
#define X(a, b, c) b,
    OPCODE_DEF(X)
#undef X
    ""
};
//---------------------------------------------------------------------------

void VM::log(const string &error)
{
    cout << error << endl;
}
//---------------------------------------------------------------------------

void VM::append(Operation *op)
{
    m_ops.push_back(op);
}
//---------------------------------------------------------------------------


void VM::dump_envstack(const std::string &msg, Operation &op)
{
    int offs = 0;

    cout << "### BEGIN ENV STACK DUMP {" << msg << "}@["
         << op.m_debug_sym->m_str << "]" << endl;
    for (int i = m_env_stack.size() - 1; i >= 0; i--)
    {
        cout << "    [" << ((m_env_stack.size() - 1) - i) << "] ";
        SimpleVec &sv = m_env_stack[i]->m_d.vec;
        for (size_t d = 0; d < sv.len; d++)
        {
            if (sv.elems[d])
                cout << "(" << d << ": " << sv.elems[d]->to_string() << ")";
            else
                cout << "(" << d << "  " << "null" << ")";
        }
        cout << endl;
    }
    cout << "### END ENV STACK DUMP {" << msg << "}" << endl;
}
//---------------------------------------------------------------------------

void DatumPool::grow()
{
    size_t new_base_size = m_allocated.size();
    if (new_base_size < m_min_pool_size)
        new_base_size = m_min_pool_size;
    size_t new_chunk_size = new_base_size * 2;
    auto dt = new Datum[new_chunk_size];
    m_chunks.push_back(std::move(std::unique_ptr<Datum[]>(dt)));

    Datum *dt_ptr = &dt[0];
    for (size_t i = 0; i < new_chunk_size; i++)
    {
        dt_ptr->m_marks = DT_MARK_FREE;
        m_allocated.push_back(dt_ptr);
        dt_ptr->m_next = m_free_list;
        m_free_list = dt_ptr;

        dt_ptr++;
    }

    cout << "Datum pool grew to " << m_allocated.size() << endl;
}
//---------------------------------------------------------------------------

void DatumPool::reclaim(uint8_t mark)
{
    unsigned int in_use_before = m_datums_in_use;

    for (auto &dt : m_allocated)
    {
        auto dt_mark = dt->get_mark();
//        cout << "DT ALLOC: " << dt << ": " << dt->to_string() << ": " << std::to_string(dt_mark) << endl;

        if (dt_mark == DT_MARK_FREE) continue;
        if (dt_mark != mark)
            put_on_free_list(dt);
    }

//    cout << "Datums in use before gc: " << in_use_before
//        << ", after: " << m_datums_in_use << endl;

    for (auto gcm : m_markables)
        gcm->reclaim(mark);

    m_next_collect_countdown =
        (m_allocated.size() - m_datums_in_use) / 2;
//    cout<< "NEXT COL COUNTDOWN: " << m_next_collect_countdown << endl;
}
//---------------------------------------------------------------------------
//
//Datum *VM::new_dt_int(int64_t i)
//{
//    Datum *dt = m_datum_pool.new_datum(T_INT);
//    dt->m_d.i = i;
//    return dt;
//}
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_dbl(double d)
//{
//    Datum *dt = m_datum_pool.new_datum(T_DBL);
//    dt->m_d.d = d;
//    return dt;
//}
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_sym(Sym *sym)
//{
//    Datum *dt = m_datum_pool.new_datum(T_SYM);
//    dt->m_d.sym = sym;
//    return dt;
//}
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_external(Sym *ext_type, Datum *args)
//{
//    if (!m_ext_factory)
//        throw DatumException("No external factory for type: "
//                             + ext_type->m_str);
//
//    External *ext = (*m_ext_factory)(ext_type->m_str, args);
//    if (!ext)
//        throw DatumException("Unknown external type: " + ext_type->m_str);
//
//    Datum *dt = m_datum_pool.new_datum(T_EXT);
//    dt->m_d.ext = ext;
//    return dt;
//}
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_prim(Datum::PrimFunc *func) {
//    Datum *dt = m_datum_pool.new_datum(T_PRIM);
//    dt->m_d.func = func;
//    return dt;
//}
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_clos(int64_t op_idx)
//{
//    Datum *dt =
//        m_datum_pool.new_vector(
//            T_CLOS, 1 + m_env_stack.size());
//
//    SimpleVec &sv = dt->m_d.vec;
//    sv.elems[0] = new_dt_int(op_idx);
//    size_t i = 1;
//    for (auto &dtp : m_env_stack)
//        sv.elems[i++] = dtp;
//
//    return dt;
//}
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_map(size_t reserve)
//{
//    Datum *dt = m_datum_pool.new_datum(T_MAP);
//    dt->m_d.map = new SymDatumMap;
//    return dt;
//}
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_nil() { return m_datum_pool.new_datum(T_NIL); }
////---------------------------------------------------------------------------
//
//Datum *VM::new_dt_ref(Datum *dt)
//{
//    Datum *dt_ref = m_datum_pool.new_datum(T_REF);
//    dt_ref->m_d.ref = dt;
//    return dt_ref;
//}
////---------------------------------------------------------------------------

//void VM::init_core_primitives()
//{
//    // PRIM *get-datums-in-use-count*
//    register_primitive(0, [this](VM *vm, SimpleVec &sv) {
//        m_datum_pool.collect();
//        return new_dt_int(m_datum_pool.get_datums_in_use_count());
//    });
//
//    // PRIM *value*
//    register_primitive(1, [this](VM *vm, SimpleVec &sv) {
//        if (sv.len <= 0) return new_dt_nil();
//        else             return sv.elems[sv.len - 1];
//    });
//
////    register_primitive(1, [](VM *vm, std::vector<Datum *> &s, size_t ac){
////        vm->pop(ac);
////        POP_STACK_ARGS(vm, s, ac);
////        m_datum_pool.collect();
////        vm->push(new_dt_int(0));
////    });
//}
//---------------------------------------------------------------------------

Datum *VM::run()
{
    int op_max_ip = m_ops.size() - 1;

    size_t local_stack_size = GET_STK_SIZE;

    using namespace std::chrono;

    m_datum_pool.grow();

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    double gc_run_time = 0;

    size_t gc_run_cnt = 0;

    Datum *return_register = nullptr;

    while (m_ip <= op_max_ip)
    {
        Operation &op = *(m_ops[m_ip]);
        try
        {
            if (m_enable_trace_log)
                cout << "TRC (ds=" << GET_STK_SIZE
                     << ",es=" << m_env_stack.size() << ") IP="
                     << m_ip << ": " << op.name()
                     << endl;

            switch (op.m_op)
            {
#               include "ops.cpp"
            }

        }
        catch (DatumException &e)
        {
            throw VMException(e.what(), op);
        }
        catch (std::exception &)
        {
            throw;
        }

        m_ip++;

//        high_resolution_clock::time_point gct1 = high_resolution_clock::now();
        if (m_datum_pool.collect_if_needed())
        {
            gc_run_cnt++;
//            high_resolution_clock::time_point gct2 = high_resolution_clock::now();
//            duration<double> time_span = duration_cast<duration<double>>(gct2 - gct1);
//            gc_run_time += time_span.count() * 1000;
//            std::cout << "gc run time: " << (time_span.count() * 1000) << " ms" << std::endl;
        }
    }
//    if (GET_STK_SIZE <= 0)
//        return nullptr;
//    local_stack_size = GET_STK_SIZE;

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
    std::cout << "vm run time: " << (time_span.count() * 1000)
              << " ms, gc runs=" << gc_run_cnt << std::endl;
//    std::cout << "gc run time: " << gc_run_time
//              << " ms" << std::endl;

    return return_register;
}
//---------------------------------------------------------------------------

}
