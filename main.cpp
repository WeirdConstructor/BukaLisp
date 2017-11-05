// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include <cstdio>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include <cstdlib>
#include "utf8buffer.h"
#include "parser.h"
#include "atom_generator.h"
#include "interpreter.h"
#include "buklivm.h"
#include "atom.h"
#include "atom_printer.h"
#include "bukalisp.h"
#include "util.h"

//#include <modules/util/utillib.h>
//#include <modules/sys/syslib.h>
#include <modules/testlib.h>

//---------------------------------------------------------------------------

using namespace bukalisp;
using namespace std;

//---------------------------------------------------------------------------

#define TEST_TRUE(b, msg) \
    if (!(b)) throw bukalisp::BukLiVMException( \
        std::string(__FILE__ ":") + std::to_string(__LINE__) +  "| " + \
        std::string(msg) + " in: " #b);

#define TEST_EQ(a, b, msg) \
    if ((a) != (b)) \
        throw bukalisp::BukLiVMException( \
            std::string(__FILE__ ":") + std::to_string(__LINE__) +  "| " + \
            std::string(msg) + ", not eq: " \
            + std::to_string(a) + " != " + std::to_string(b));

#define TEST_EQSTR(a, b, msg) \
    if ((a) != (b)) \
        throw bukalisp::BukLiVMException( \
            std::string(__FILE__ ":") + std::to_string(__LINE__) +  "| " + \
            std::string(msg) + ", not eq: " \
            + (a) + " != " + (b));

void test_gc1()
{
    TEST_TRUE(AtomVec::s_alloc_count == 0, "none allocated at beginning");

    {
        GC gc;

        gc.allocate_vector(4);
        gc.allocate_vector(4);
        gc.allocate_vector(4);
        AtomVec *av =
            gc.allocate_vector(4);

        TEST_EQ(AtomVec::s_alloc_count, 2 + (2 + 1) * 2,
                "vec alloc count 1");
        TEST_EQ(av->m_alloc, GC_SMALL_VEC_LEN, "small vecs");

        gc.allocate_vector(50);
        gc.allocate_vector(50);
        gc.allocate_vector(50);
        av =
            gc.allocate_vector(50);

        TEST_EQ(AtomVec::s_alloc_count, 2 + (2 + 1) * 2 + 2 + (2 + 1) * 2,
                "vec alloc count 2");
        TEST_EQ(av->m_alloc, GC_MEDIUM_VEC_LEN, "medium vecs");

        gc.allocate_vector(1000);
        gc.allocate_vector(1000);
        av =
            gc.allocate_vector(1000);

        TEST_EQ(AtomVec::s_alloc_count, 2 + (2 + 1) * 2 + 2 + (2 + 1) * 2 + 3,
                "vec alloc count 3");
        TEST_EQ(av->m_alloc, 1000, "custom vecs");

        TEST_EQ(gc.count_potentially_alive_vectors(), 11, "pot alive");
        gc.collect();
        TEST_EQ(gc.count_potentially_alive_vectors(), 0, "pot alive after gc");

        TEST_EQ(AtomVec::s_alloc_count, 2 + (2 + 1) * 2 + 2 + (2 + 1) * 2,
                "vec alloc count after gc");
    }

    TEST_TRUE(AtomVec::s_alloc_count == 0, "none allocated at the end");
}
//---------------------------------------------------------------------------

class Reader
{
    private:
        GC                m_gc;
        AtomGenerator     m_ag;
        Tokenizer         m_tok;
        Parser            m_par;

        GC_ROOT_MEMBER_VEC(m_root_set);

    public:
        Reader()
            : m_ag(&m_gc),
              m_par(m_tok, &m_ag),
              GC_ROOT_MEMBER_INITALIZE_VEC(m_gc, m_root_set)
        {
            m_root_set = m_gc.allocate_vector(0);
        }

        std::string debug_info(Atom &a)
        {
            Atom meta = a.meta();
            std::string f = meta.at(0).at(0).to_display_str();
            int64_t l     = meta.at(0).at(1).to_int();
            return f + ":" + std::to_string(l);
        }

        size_t pot_alive_vecs() { return m_gc.count_potentially_alive_vectors(); }
        size_t pot_alive_maps() { return m_gc.count_potentially_alive_maps(); }
        size_t pot_alive_syms() { return m_gc.count_potentially_alive_syms(); }

        Atom a_sym(const std::string &s) { return Atom(T_SYM, m_gc.new_symbol(s)); }
        Atom a_kw(const std::string &s) { return Atom(T_KW, m_gc.new_symbol(s)); }
        Atom a_str(const std::string &s) { return Atom(T_STR, m_gc.new_symbol(s)); }

        void make_always_alive(Atom a) { m_root_set->push(a); }

        void collect()
        {
            m_ag.clear();
            m_gc.collect();
        }

        bool parse(const std::string &codename, const std::string &in)
        {
            m_tok.tokenize(codename, in);
            m_ag.start();
            bool r = m_par.parse();
            // m_tok.dump_tokens();
            return r;
        }

        Atom &root() { return m_ag.root(); }
};
//---------------------------------------------------------------------------

void test_gc2()
{
    Reader tc;

    TEST_EQ(tc.pot_alive_vecs(), 1, "potentially alive before");

    tc.parse("test_gc2", "(1 2 3 (4 5 6) (943 203))");

    TEST_EQ(tc.pot_alive_vecs(), 4, "potentially alive before");

    Atom r = tc.root();
    tc.make_always_alive(r);

    TEST_EQ(tc.pot_alive_vecs(), 4, "potentially alive before 2");

    tc.collect();
    TEST_EQ(AtomVec::s_alloc_count, 8, "after 1 vec count");
    TEST_EQ(tc.pot_alive_vecs(), 4, "potentially alive after");

    r.m_d.vec->m_data[0] = Atom();

    tc.collect();
    TEST_EQ(AtomVec::s_alloc_count, 8, "after 2 vec count");
    TEST_EQ(tc.pot_alive_vecs(), 4, "potentially alive after 2");

    TEST_EQ(r.m_d.vec->m_data[3].m_type, T_VEC, "3rd elem is vector");
    r.m_d.vec->m_data[3] = Atom();
    tc.collect();
    TEST_EQ(AtomVec::s_alloc_count, 8, "after 3 vec count");
    TEST_EQ(tc.pot_alive_vecs(), 3, "potentially alive after 3");

    r.m_d.vec->m_data[4] = Atom();
    tc.collect();
    TEST_EQ(AtomVec::s_alloc_count, 8, "after 4 vec count");
    TEST_EQ(tc.pot_alive_vecs(), 2, "potentially alive after 4");
}
//---------------------------------------------------------------------------

void test_atom_gen1()
{
    Reader tc;

    TEST_TRUE(tc.parse("test_atom_gen1", "3"), "test parse");

    Atom r = tc.root();

    TEST_EQ(r.m_type, T_INT, "basic atom type ok");
    TEST_EQ(r.m_d.i, 3, "basic atom int ok");
}
//---------------------------------------------------------------------------

void test_atom_gen2()
{
    Reader tc;

    TEST_TRUE(tc.parse("test_atom_gen1", "(1 2.94 (3 4 5 #t #f))"),
              "test parse");

    Atom r = tc.root();

    TEST_EQ(r.m_type,       T_VEC, "atom type");
    TEST_EQ(r.at(0).m_type, T_INT, "atom type 2");
    TEST_EQ(r.at(1).m_type, T_DBL, "atom type 3");
    TEST_EQ(r.at(2).m_type, T_VEC, "atom type 4");

    TEST_EQ(r.at(0).to_int(), 1, "atom 1");
    TEST_EQ(r.at(1).to_int(), 2, "atom 2");

    TEST_EQ(r.at(2).m_d.vec->m_len, 5,    "atom vec len");
    TEST_EQ(r.at(2).at(3).m_type, T_BOOL, "bool 1");
    TEST_EQ(r.at(2).at(4).m_type, T_BOOL, "bool 2");
    TEST_EQ(r.at(2).at(3).m_d.b, true,    "bool 1");
    TEST_EQ(r.at(2).at(4).m_d.b, false,   "bool 2");
}
//---------------------------------------------------------------------------

void test_maps()
{
    Reader tc;

    TEST_TRUE(tc.parse("test_maps", "(1 2.94 { a: 123 b: (1 2 { x: 3 }) } 4)"),
              "test parse");

    Atom r = tc.root();

    Atom m = r.at(2);

    TEST_EQSTR(r.meta().to_display_str(), "((\"test_maps\" 1))", "meta info");
    TEST_EQ(r.at(3).m_d.i, 4, "last atom");

    TEST_EQ(m.at(tc.a_kw("a")).m_type, T_INT, "map a key type");
    TEST_EQ(m.at(tc.a_kw("a")).m_d.i,  123,   "map a key");

    TEST_EQ(m.at(tc.a_kw("b")).at(2).m_type, T_MAP, "2nd map at right pos");

    tc.make_always_alive(m.at(tc.a_kw("b")));

    TEST_EQ(tc.pot_alive_maps(), 2, "alive map count");
    TEST_EQ(tc.pot_alive_vecs(), 14, "alive vec count");
    tc.collect();
    TEST_EQ(tc.pot_alive_maps(), 1, "alive map count after gc");
    TEST_EQ(tc.pot_alive_vecs(), 9, "alive vec count after gc");
}
//---------------------------------------------------------------------------

void test_symbols_and_keywords()
{
    Reader tc;

    TEST_EQ(tc.pot_alive_syms(), 0, "no syms alive at start");

    tc.parse("test_symbols_and_keywords",
             "(foo bar foo foo: baz: :baz :foo:)");

    Atom r = tc.root();

    TEST_EQ(tc.pot_alive_syms(), 4, "some syms alive after parse");

    TEST_EQ(r.at(0).m_type, T_SYM, "sym 1");
    TEST_EQ(r.at(1).m_type, T_SYM, "sym 2");
    TEST_EQ(r.at(2).m_type, T_SYM, "sym 3");
    TEST_EQ(r.at(3).m_type, T_KW,  "kw 1");
    TEST_EQ(r.at(4).m_type, T_KW,  "kw 2");
    TEST_EQ(r.at(5).m_type, T_KW,  "kw 3");
    TEST_EQ(r.at(6).m_type, T_KW,  "kw 4");

    TEST_TRUE(r.at(0).m_d.sym == r.at(2).m_d.sym, "sym 0 eq 2");
    TEST_TRUE(r.at(1).m_d.sym != r.at(2).m_d.sym, "sym 1 neq 2");
    TEST_TRUE(r.at(3).m_d.sym == r.at(6).m_d.sym, "sym 3 eq 6");
    TEST_TRUE(r.at(4).m_d.sym == r.at(5).m_d.sym, "sym 4 eq 5");

    tc.collect();

    TEST_EQ(tc.pot_alive_syms(), 0, "no syms except debug sym alive after collect");
}
//---------------------------------------------------------------------------

void test_atom_hash_table()
{
#if !WITH_STD_UNORDERED_MAP
    {
        AtomMap ht(true);
        ht.m_inhibit_grow = true;

        ht.set(Atom(T_INT, 1), Atom(T_INT, 2));
//        cout << ht.debug_dump();
        TEST_EQ(ht.at(Atom(T_INT, 1)).m_d.i, 2, "first set/get");

        ht.set(Atom(T_INT, 2), Atom(T_INT, 22));
        ht.set(Atom(T_INT, 3), Atom(T_INT, 23));
        ht.set(Atom(T_INT, 4), Atom(T_INT, 24));
        ht.set(Atom(T_INT, 5), Atom(T_INT, 25));
        ht.set(Atom(T_INT, 6), Atom(T_INT, 26));
        ht.set(Atom(T_INT, 7), Atom(T_INT, 27));
//        cout << ht.debug_dump();
        TEST_EQ(ht.at(Atom(T_INT, 2)).m_d.i, 22, "small ht get 1");
        TEST_EQ(ht.at(Atom(T_INT, 3)).m_d.i, 23, "small ht get 2");
        TEST_EQ(ht.at(Atom(T_INT, 4)).m_d.i, 24, "small ht get 3");
        TEST_EQ(ht.at(Atom(T_INT, 5)).m_d.i, 25, "small ht get 4");
        TEST_EQ(ht.at(Atom(T_INT, 6)).m_d.i, 26, "small ht get 5");
        TEST_EQ(ht.size(), 7, "correct size of small ht");

        ht.delete_key(Atom(T_INT, 1));
        TEST_EQ(ht.at(Atom(T_INT, 1)).m_type, T_NIL, "deleted key not present anymore");
//        cout << ht.debug_dump();
        ht.set(Atom(T_INT, 8), Atom(T_INT, 28));
//        cout << ht.debug_dump();
        TEST_EQ(ht.at(Atom(T_INT, 8)).m_d.i, 28, "overwrote deleted key successfully");

        ht.m_inhibit_grow = false;
        ht.set(Atom(T_INT, 9), Atom(T_INT, 29));
//        cout << ht.debug_dump();
        TEST_EQ(ht.at(Atom(T_INT, 8)).m_d.i, 28, "grew table successfully");
        TEST_EQ(ht.size(), 8, "correct size of bigger ht");

        TEST_EQ(ht.at(Atom(T_INT, 2)).m_d.i, 22, "bigger ht get 1");
        TEST_EQ(ht.at(Atom(T_INT, 3)).m_d.i, 23, "bigger ht get 2");
        TEST_EQ(ht.at(Atom(T_INT, 4)).m_d.i, 24, "bigger ht get 3");
        TEST_EQ(ht.at(Atom(T_INT, 5)).m_d.i, 25, "bigger ht get 4");
        TEST_EQ(ht.at(Atom(T_INT, 6)).m_d.i, 26, "bigger ht get 5");
        TEST_EQ(ht.at(Atom(T_INT, 7)).m_d.i, 27, "bigger ht get 6");
        TEST_EQ(ht.at(Atom(T_INT, 8)).m_d.i, 28, "bigger ht get 7");

        ht.m_inhibit_grow = true;
        ht.set(Atom(T_INT, 10), Atom(T_INT, 110));
        ht.set(Atom(T_INT, 11), Atom(T_INT, 111));
        ht.set(Atom(T_INT, 12), Atom(T_INT, 112));
        TEST_EQ(ht.size(), 11, "correct size 2 of bigger ht");

        std::string except;
        try
        { ht.set(Atom(T_INT, 13), Atom(T_INT, 113)); }
        catch (std::exception &e) { except = e.what(); }
        TEST_EQ(!!except.find("inserting"), true,
                "exception on overly full, corner case");


        // cout << ht.debug_dump();
        int64_t sum = 0;
        Atom *cur = nullptr;
        while (cur = ht.next(cur))
        {
            // cout << "ITER : " << cur[1].to_write_str() << endl;
            sum += cur[1].m_d.i;
        }

        TEST_EQ(sum, 111 + 23 + 112 + 27 + 22 + 29 + 25 + 110 + 113 + 26 + 28,
                "hash iteration works");

        // cout << "SUM: " << sum << endl;
    }

    {
        // Testing worst case here:

        AtomMap ht(true);
        ht.m_inhibit_grow = true;
        std::vector<unsigned int> idxes;
        for (unsigned int i = 0; ht.size() < 7 && i < 10000000000; i++)
        {
            if ((AtomHash()(Atom(T_INT, i)) % 7) == 6)
            {
                ht.set(Atom(T_INT, i), Atom(T_INT, i * 10));
                idxes.push_back(i);
            }
        }

        for (auto i : idxes)
        {
            TEST_EQ(ht.at(Atom(T_INT, i)).m_d.i, i * 10, "worst case test");
        }

        // cout << ht.debug_dump();
    }

    {
        for (size_t c = 0; c < 20000; c++)
        {
            AtomMap am;
            for (size_t i = 0; i < c; i++)
            {
                am.set(Atom(T_INT, i), Atom(T_INT, i * 10));
            }

            size_t sum1 = 0;
            size_t sum2 = 0;

            for (size_t i = 0; i < c; i++)
            {
                sum1 += (size_t) am.at(Atom(T_INT, i)).m_d.i;
            }

            Atom *cur = nullptr;
            while (cur = am.next(cur))
            {
                sum2 += (size_t) cur[1].m_d.i;
            }

            TEST_EQ(sum1, sum2, "summing up different table sizes");

            if (c > 10000)
                c += 53;
            else if (c > 5000)
                c += 11;
        }
    }
#endif
}
//---------------------------------------------------------------------------

void test_maps2()
{
    Reader tc;

    tc.parse("test_atom_printer", "({ 123 5 }{ #t 34 }{ #f 5 })");

    Atom r = tc.root();

    TEST_EQSTR(
        write_atom(r),
        "({123 5} {#true 34} {#false 5})",
        "atom print 1");
}
//---------------------------------------------------------------------------

void test_atom_printer()
{
    Reader tc;

    tc.parse("test_atom_printer", "(1 2.3 foo foo: \"foo\" { axx: #true } { bxx #f } { \"test\" 123 })");

    Atom r = tc.root();

    TEST_EQSTR(
        write_atom(r),
        "(1 2.3 foo foo: \"foo\" {axx: #true} {bxx #false} {\"test\" 123})",
        "atom print 1");
}
//---------------------------------------------------------------------------

void test_atom_debug_info()
{
    Reader tc;

    tc.parse("XXX", "\n(1 2 3\n\n(3 \n ( 4 )))");
    Atom r = tc.root();

    TEST_EQ(r.m_type, T_VEC, "is vec");
    TEST_EQSTR(tc.debug_info(r),             "XXX:2", "debug info 1")
    TEST_EQSTR(tc.debug_info(r.at(3)),       "XXX:4", "debug info 2")
    TEST_EQSTR(tc.debug_info(r.at(3).at(1)), "XXX:5", "debug info 3")
}
//---------------------------------------------------------------------------

#define TEST_EVAL(expr, b) \
    r = i.eval(std::string(__FILE__ ":") + std::to_string(__LINE__), expr); \
    if (bukalisp::write_atom(r) != (b)) \
        throw bukalisp::BukLiVMException( \
            std::string(__FILE__ ":") + std::to_string(__LINE__) +  "| " + \
            #expr " not eq: " \
            + bukalisp::write_atom(r) + " != " + (b));

//---------------------------------------------------------------------------

void test_ieval_atoms()
{
    Runtime rt;
    Interpreter i(&rt);

    Atom r = i.eval("testatoms", "123");
    TEST_EQ(r.m_type, T_INT, "is int");
    TEST_EQ(r.to_int(), 123, "correct int");

    TEST_EVAL("\"foo\"",        "\"foo\"");
    TEST_EVAL("\"f\\\\ oo\"",   "\"f\\\\ oo\"");
    TEST_EVAL("\"f\\\" oo\"",   "\"f\\\" oo\"");
    TEST_EVAL("\"f\\n oo\"",    "\"f\\n oo\"");
    TEST_EVAL("\"f\\r oo\"",    "\"f\\r oo\"");
    TEST_EVAL("\"f\\a oo\"",    "\"f\\a oo\"");
    TEST_EVAL("\"f\\b oo\"",    "\"f\\b oo\"");
    TEST_EVAL("\"f\\t oo\"",    "\"f\\t oo\"");
    TEST_EVAL("\"f\\v oo\"",    "\"f\\v oo\"");
    TEST_EVAL("\"f\\f oo\"",    "\"f\\f oo\"");
    TEST_EVAL("\"f\\xE4; oo\"", "\"f\\xe4; oo\"");
    TEST_EVAL("\"f\\uE4; oo\"", "\"f\\xc3;\\xa4; oo\"");
    TEST_EVAL("\"fä oo\"",      "\"f\\xc3;\\xa4; oo\"");
    TEST_EVAL("\"f\\\n oo\"",   "\"foo\"");
    TEST_EVAL("\"f\\ \t\n oo\"","\"foo\"");

    TEST_EVAL("#q'fooobar'",              "\"fooobar\"");
    TEST_EVAL("#q'foo\nobar'",            "\"foo\\nobar\"");
    TEST_EVAL("#q'fo\\'oobar'",           "\"fo'oobar\"");
    TEST_EVAL("#q'fo\\'\\\\oobar'",       "\"fo'\\\\oobar\"");
    TEST_EVAL("#q'fo\\'\\\\oobar\\\\'",   "\"fo'\\\\oobar\\\\\"");
    TEST_EVAL("#q'fo\\'\\\\oo\"bar\\\\'", "\"fo'\\\\oo\\\"bar\\\\\"");
    TEST_EVAL("#q'\\s\\x\\foobar+[]\\''", "\"\\\\s\\\\x\\\\foobar+[]'\"");

    TEST_EVAL("#q|foobar'xxx|", "\"foobar'xxx\"");
    TEST_EVAL("#q/f\\xoobar'xxx/", "\"f\\\\xoobar'xxx\"");

    TEST_EVAL("\"\\xFF;\\xFE;\"",         "\"\\xff;\\xfe;\"");
    TEST_EVAL("\"\xFF\xFE\"",             "\"\\xff;\\xfe;\"");
    TEST_EVAL("\"\r\n\\\"\a\t\b\"",       "\"\\r\\n\\\"\\a\\t\\b\"");
    TEST_EVAL("\"   fewfew \\\n    f ewufi wfew \\\n        \"",
              "\"   fewfew f ewufi wfew \"");
}
//---------------------------------------------------------------------------

void test_ieval_vars()
{
    Runtime rt;
    Interpreter i(&rt);

    Atom r =
        i.eval(
            "test vars",
            "(begin (define y 30) y)");
    TEST_EQ(r.to_int(), 30, "correct y");

    r =
        i.eval(
            "test vars",
            "(begin (define x 12) (define y 30) (set! y 10) y)");
    TEST_EQ(r.to_int(), 10, "correct y");
}
//---------------------------------------------------------------------------

void test_ieval_basic_stuff()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    // maps
    TEST_EVAL("{}", "{}");
    TEST_EVAL("{a: (* 2 4)}",       "{a: 8}");

    // quote
    TEST_EVAL("'a",                 "a");
    TEST_EVAL("'a:",                "a:");
    TEST_EVAL("'1",                 "1");
    TEST_EVAL("'(1 2 3)",           "(1 2 3)");
    TEST_EVAL("'((1 2 3) 2 3)",     "((1 2 3) 2 3)");
    TEST_EVAL("'[1 2 3]",           "(list 1 2 3)");
    TEST_EVAL("'(eq? 1 2)",         "(eq? 1 2)");

}
//---------------------------------------------------------------------------

void test_ieval_proc()
{
    Runtime rt;
    Interpreter i(&rt);
//    i.set_force_always_gc(true);
//    i.set_trace(true);
    Atom r;

    TEST_EVAL("(+ 1 2 3)",           "6");
    TEST_EVAL("(+ 1.2 2 3)",       "6.2");
    TEST_EVAL("(* 1.2 2 3)",       "7.2");
    TEST_EVAL("(/ 8 2 2)",           "2");
    TEST_EVAL("(/ 5 2)",             "2");
    TEST_EVAL("(- 10.5 1.3 2.2)",    "7");
    TEST_EVAL("(< 1 2)",            "#true");
    TEST_EVAL("(> 1 2)",            "#false");
    TEST_EVAL("(<= 1 2)",           "#true");
    TEST_EVAL("(<= 1 2)",           "#true");
    TEST_EVAL("(<= 2 2)",           "#true");
    TEST_EVAL("(<= 2.1 2)",         "#false");
    TEST_EVAL("(>= 2.1 2)",         "#true");

    TEST_EVAL("(eqv? #t #t)",                                       "#true");
    TEST_EVAL("(eqv? #f #f)",                                       "#true");
    TEST_EVAL("(eqv? #t #f)",                                       "#false");

    TEST_EVAL("(eqv? a: a:)",                                       "#true");
    TEST_EVAL("(eqv? a: b:)",                                       "#false");

    TEST_EVAL("(eqv? 'a 'a)",                                       "#true");
    TEST_EVAL("(eqv? 'a 'b)",                                       "#false");

    TEST_EVAL("(eqv? 2 (/ 4 2))",                                   "#true");
    TEST_EVAL("(eqv? 2 (/ 4.0 2.0))",                               "#false");
    TEST_EVAL("(eqv? 2.0 (/ 4.0 2.0))",                             "#true");

    TEST_EVAL("(eqv? [] [])",                                       "#false");
    TEST_EVAL("(eqv? {} {})",                                       "#false");
    TEST_EVAL("(eqv? { x: 11 } { x: 10 })",                         "#false");
    TEST_EVAL("(eqv? 2 (/ 5 2))",                                   "#true");
    TEST_EVAL("(eqv? 2 (/ 5.0 2))",                                 "#false");
    TEST_EVAL("(eqv? #f 0)",                                        "#false");
    TEST_EVAL("(eqv? #f [])",                                       "#false");
    TEST_EVAL("(eqv? 2.0 2)",                                       "#false");

    TEST_EVAL("(eqv? + (let ((y +)) y))",                           "#true");
    TEST_EVAL("(let ((m { x: 11 }) (l #f)) (set! l m) (eqv? m l))", "#true");

    TEST_EVAL("(not #t)",           "#false");
    TEST_EVAL("(not 1)",            "#false");
    TEST_EVAL("(not '())",          "#false");
    TEST_EVAL("(not #f)",           "#true");

    TEST_EVAL("(symbol? 'a)",       "#true");
    TEST_EVAL("(symbol? a:)",       "#false");
    TEST_EVAL("(symbol? 1)",        "#false");

    TEST_EVAL("(keyword? a:)",      "#true");
    TEST_EVAL("(keyword? 'a)",      "#false");
    TEST_EVAL("(keyword? 1)",       "#false");

    TEST_EVAL("(nil? 1)",           "#false");
    TEST_EVAL("(nil? 0)",           "#false");
    TEST_EVAL("(nil? [])",          "#false");
    TEST_EVAL("(nil? nil)",         "#true");

    TEST_EVAL("(exact? nil)",       "#false");
    TEST_EVAL("(exact? 123)",       "#true");
    TEST_EVAL("(exact? 1.0)",       "#false");

    TEST_EVAL("(inexact? nil)",     "#false");
    TEST_EVAL("(inexact? 123)",     "#false");
    TEST_EVAL("(inexact? 1.0)",     "#true");

    TEST_EVAL("(string? 1)",        "#false");
    TEST_EVAL("(string? a:)",       "#false");
    TEST_EVAL("(string? 'a)",       "#false");
    TEST_EVAL("(string? \"foo\")",  "#true");

    TEST_EVAL("(boolean? \"foo\")", "#false");
    TEST_EVAL("(boolean? #t)",      "#true");
    TEST_EVAL("(boolean? #f)",      "#true");
    TEST_EVAL("(boolean? nil)",     "#false");

    TEST_EVAL("(list? nil)",        "#false");
    TEST_EVAL("(list? [])",         "#true");
    TEST_EVAL("(list? '())",        "#true");
    TEST_EVAL("(list? string?)",    "#false");
    TEST_EVAL("(list? {})",         "#false");

    TEST_EVAL("(map? {})",          "#true");
    TEST_EVAL("(map? '())",         "#false");
    TEST_EVAL("(map? [])",          "#false");
    TEST_EVAL("(map? nil)",         "#false");

    TEST_EVAL("(procedure? string?)",       "#true");
    TEST_EVAL("(procedure? (lambda (x) x))","#true");
    TEST_EVAL("(procedure? if)",            "#false");
    TEST_EVAL("(procedure? '())",           "#false");

    TEST_EVAL("(type [])",              "list");
    TEST_EVAL("(type [1 2 3])",         "list");
    TEST_EVAL("(type '())",             "list");
    TEST_EVAL("(type '(1 2 3))",        "list");
    TEST_EVAL("(type {})",              "map");
    TEST_EVAL("(type map?)",            "procedure");
    TEST_EVAL("(type (lambda () nil))", "procedure");
    TEST_EVAL("(type 1)",               "exact");
    TEST_EVAL("(type 1.0)",             "inexact");
    TEST_EVAL("(type nil)",             "nil");
    TEST_EVAL("(type #t)",              "boolean");
    TEST_EVAL("(type #f)",              "boolean");
    TEST_EVAL("(type 'f)",              "symbol");
    TEST_EVAL("(type f:)",              "keyword");
    TEST_EVAL("(type \"f\")",           "string");

    TEST_EVAL("(length [1 2 3])",                   "3");
    TEST_EVAL("(length [])",                        "0");
    TEST_EVAL("(length '())",                       "0");
    TEST_EVAL("(length {})",                        "0");
    TEST_EVAL("(length {x: 12})",                   "1");
    TEST_EVAL("(length {y: 324 x: 12})",            "2");
    TEST_EVAL("(length (let ((l [])) (@!9 l l)))",  "10");

    TEST_EVAL("(let ((aid  (atom-id (type nil))) "
              "      (aid2 (atom-id (type nil))) "
              "      (aid3 (atom-id (type nil)))) "
              "  [(eqv? aid aid2) (eqv? aid aid3)])",
              "(#true #true)");

    TEST_EVAL("(let ((m { 'nil 1234 })) "
              "  (@(type nil) m))",
              "nil");

    TEST_EVAL("(let ((m { (type nil) 1234 })) "
              "  (@(type nil) m))",
              "1234");

    TEST_EVAL("(first '(a b c))",        "a");
    TEST_EVAL("(first '())",             "nil");
    TEST_EVAL("(first [3])",             "3");
    TEST_EVAL("(last [1 2 3])",          "3");
    TEST_EVAL("(last [3])",              "3");
    TEST_EVAL("(last [])",               "nil");
    TEST_EVAL("(take [x: y: 12 3 4] 2)", "(x: y:)");
    TEST_EVAL("(take [1] 2)",            "(1)");
    TEST_EVAL("(take [1 2] 2)",          "(1 2)");
    TEST_EVAL("(take [] 2)",             "()");
    TEST_EVAL("(drop [1 2 3] 2)",        "(3)");
    TEST_EVAL("(drop [1 2 3] 4)",        "()");
    TEST_EVAL("(drop [1 2 3] 3)",        "()");
    TEST_EVAL("(drop [] 3)",             "()");
    TEST_EVAL("(drop [1 2 3] 0)",        "(1 2 3)");
    TEST_EVAL("(drop [1 2 3] 1)",        "(2 3)");

    TEST_EVAL("(append [1 2 3] 1 a: { x: 12 } [5 5 5])",
              "(1 2 3 1 a: x: 12 5 5 5)");
    TEST_EVAL("(append [1 2 3])",
              "(1 2 3)");

//TEST_EVAL("(eq? \"foo\" (symbol->string 'foo))",               "#true");
//TEST_EVAL("(let ((p (lambda (x) x))) (eq? p p))",              "#true");
//TEST_EVAL("(eq? t: (string->symbol \"t\"))",                   "#false");
//TEST_EVAL("(eq? 't (string->symbol \"t\"))",                   "#true");
//TEST_EVAL("(eq? t: (string->keyword \"t\"))",                  "#true");
//TEST_EVAL("(eq? t:: (string->keyword \"t:\"))",                "#true");
}
//---------------------------------------------------------------------------

void test_ieval_let()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    TEST_EVAL("(begin (define x 20) (+ (let ((x 10)) x) x))",               "30");
    TEST_EVAL("(begin (define x 20) (+ (let () (define x 10) x) x))",       "30");
    TEST_EVAL("(begin (define y 20) (+ (let ((x 10)) (define y 10) x) y))", "30");
    TEST_EVAL("(begin (define y 20) (+ (let ((x 10)) (set! y 11) x) y))",   "21");

    TEST_EVAL("(let ((a 5) (b 20)) (set! a (* 2 a)) (+ a b))",                               "30")
    TEST_EVAL("(begin (define x 10) (define y 20) (+ x y 2))",                               "32")
    TEST_EVAL("(begin (begin (define x 10)) (define y 20) (+ x y 2))",                       "32")
    TEST_EVAL("(begin (begin (define x 10) (define y 20) 44) (begin (+ x y 2) (+ x y 2)))",  "32")
    TEST_EVAL("(begin (begin (define x 10) (define y (begin 44 20)) 44) (begin (+ x y 2)))", "32")
    TEST_EVAL("(begin (begin (define x 10) (define y 20) 44) (begin (+ x y 2)))",            "32")
}
//---------------------------------------------------------------------------

void test_ieval_cond()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    TEST_EVAL("(if #t 123 345)", "123");
    TEST_EVAL("(if #f 123 345)", "345");
    TEST_EVAL("(if #f 123)",     "nil");
    TEST_EVAL("(if #t 23)",      "23");
    TEST_EVAL(
        "(begin (define x 20) (when #t (set! x 10) (set! x (+ x 20))) x)",
        "30");
    TEST_EVAL(
        "(begin (define x 20) (when #f (set! x 10) (set! x (+ x 20))) x)",
        "20");
    TEST_EVAL(
        "(begin (define x 20) (unless #t (set! x 10) (set! x (+ x 20))) x)",
        "20");
    TEST_EVAL(
        "(begin (define x 20) (unless #f (set! x 10) (set! x (+ x 20))) x)",
        "30");
    TEST_EVAL("(if nil       #t #f)", "#false");
    TEST_EVAL("(if (not nil) #t #f)", "#true");

    TEST_EVAL("(let ((a 10) (l []) (i 0)) "
              "  (or "
              "      (begin (set! a #f) (@!(set! i (+ i 1)) l x:) a) "
              "      (begin (set! a #f) (@!(set! i (+ i 1)) l y:) a) "
              "      (begin (set! a #f) (@!(set! i (+ i 1)) l z:) a) "
              "      (set! a l)))",
              "(nil x: y: z:)");

    TEST_EVAL("(let ((a 10) (l []) (i 0)) "
              "  (and "
              "      (begin (set! a #t) (@!(set! i (+ i 1)) l x:) a) "
              "      (begin (set! a #t) (@!(set! i (+ i 1)) l y:) a) "
              "      (begin (set! a #t) (@!(set! i (+ i 1)) l z:) a) "
              "      (not (set! a l))) "
              "  l)",
              "(nil x: y: z:)");

    TEST_EVAL("(and 1 3 #f 99)", "#false");
    TEST_EVAL("(and 1 3 #t 99)", "99");
    TEST_EVAL("(and 1 3 3432 #t)", "#true");
    TEST_EVAL("(or #f 1 3 3432 #t)", "1");
    TEST_EVAL("(or #f #f ((lambda () #f)) #f)", "#false");

    TEST_EVAL("                         \
        (let ((x 10))                   \
            (or #f                      \
                (begin (set! x 22) #f)  \
                (+ x 71)                \
                (set! x 32)))           \
    ", "93")
    TEST_EVAL("                           \
        (let ((x 12)                      \
              (f (lambda () 10)))         \
            [(or #f                       \
                (begin (set! x 22) #f)    \
                (f)) x])                  \
    ", "(10 22)")
    TEST_EVAL("                        \
        (let ((x 44)                   \
              (f (lambda () 30)))      \
            (and #f                    \
                (begin (set! x 33) #f) \
                (f))                   \
            x)                         \
    ", "44")
//    assert_lal('33', [[
//        (let ((x 44)
//              (f (lambda () x)))
//            (and #t
//                (begin (set! x 33) #f)
//                (f))
//            x)
//    ]])
//    assert_lal('32', [[
//        (let ((x 44)
//              (f (lambda () x)))
//            (and #t
//                (begin (set! x 32) #t)
//                (f)))
//    ]])

    TEST_EVAL(" \
      (let ((x 0))                          \
        (case x:                            \
         ((x: y:) (set! x 9) (+ x 1))       \
         (else 12)))                        \
    ", "10")

    TEST_EVAL(" \
      (let ((x 0))                          \
        (case y:                            \
         ((x: y:) (set! x 9) (+ x 1))       \
         (else 12)))                        \
    ", "10")

    TEST_EVAL(" \
      (let ((x 0))                          \
        (case z:                            \
         ((x: y:) (set! x 9) (+ x 1))       \
         (else 12)))                        \
    ", "12")

    TEST_EVAL(" \
      (let ((x 0))                          \
        (case z:                            \
         (else 19)                          \
         ((x: y:) (set! x 9) (+ x 1))       \
         (else 12)))                        \
    ", "19")

    TEST_EVAL(" \
      (let ((x 0))                          \
        (case z:                            \
         ((x: y:) (set! x 9) (+ x 1))       \
         ))                                 \
    ", "nil")
}
//---------------------------------------------------------------------------

void test_ieval_lambda()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    TEST_EVAL("((lambda () 10))",                               "10");
    TEST_EVAL("((lambda (x) (+ x 10)) 20)",                     "30");
    TEST_EVAL("((let ((a 1.2)) (lambda (x) (+ a x 10))) 20)",   "31.2");
    TEST_EVAL("(begin "
              "  (define x (let ((a 1.2)) "
              "              (lambda (x) (+ a x 10)))) "
              "  (x 20))",   "31.2");
    TEST_EVAL("(begin "
              "  (define x "
              "   (lambda (x) (set! x 20) (+ x 10))) "
              "  (x 50))",   "30");
}
//---------------------------------------------------------------------------

void test_ieval_index_procs()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

//    TEST_EVAL("(@0 [1 2 3])",       "1");
//    TEST_EVAL("(@1 [1 2 3])",       "2");
//    TEST_EVAL("(@2 [1 2 3])",       "3");
//    TEST_EVAL("(@3 [1 2 3])",       "nil");

    TEST_EVAL("(xxx:      {a: 123 'b 444 \"xxx\" 3.4})",               "nil");
    TEST_EVAL("(@\"xxx\"  {a: 123 'b 444 \"xxx\" 3.4})",               "3.4");
    TEST_EVAL("(@'a       {a: 123 'b 444 \"xxx\" 3.4})",               "nil");
    TEST_EVAL("(@a:       {a: 123 'b 444 \"xxx\" 3.4})",               "123");
    TEST_EVAL("(@\"b\"    {a: 123 'b 444 \"xxx\" 3.4})",               "nil");
    TEST_EVAL("(@'b       {a: 123 'b 444 \"xxx\" 3.4})",               "444");
    TEST_EVAL("(let ((key xxx:)) (@((lambda () key)) {a: 123 'b 444 xxx: 3.4}))", "3.4");
    TEST_EVAL("(let ((key xxx:)) (@key               {a: 123 'b 444 xxx: 3.4}))", "3.4");

    TEST_EVAL("(let ((m {})) (@!'x m 123) m)",                              "{x 123}");
    TEST_EVAL("(let ((m {})) (@!x: m 123) (@!'x m 344) [(x: m) (@'x m) (@'x m)])", "(123 344 344)");

    TEST_EVAL("(let ((v (list))) (@!0 v 2) (@!10 v 99) v)",
              "(2 nil nil nil nil nil nil nil nil nil 99)");
}
//---------------------------------------------------------------------------

void test_ieval_loops()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    TEST_EVAL(
        "(let ((i 9) (o [])) "
        "  (while (>= i 0) "
        "    (@!i o i) "
        "    (set! i (- i 1)) "
        "    o))",
        "(0 1 2 3 4 5 6 7 8 9)");

    TEST_EVAL(
        "(let ((i 10) (o [])) "
        "  (while (>= (set! i (- i 1)) 0) "
        "    (@!i o i) "
        "    o))",
        "(0 1 2 3 4 5 6 7 8 9)");

    TEST_EVAL("(while #f 10)", "nil");

    TEST_EVAL("(let ((ol [])) (for (i 0 9) (@! i ol (* i 2))) ol)",
              "(0 2 4 6 8 10 12 14 16 18)");
    TEST_EVAL("(let ((ol [])) (for (i 9 0 -1) (@! (- 9 i) ol (* i 2))) ol)",
              "(18 16 14 12 10 8 6 4 2 0)");
    TEST_EVAL("(let ((ol []) (idx 0))             "
              "  (for (i 0 9 2)                   "
              "    (@! (- (set! idx (+ idx 1)) 1) "
              "        ol                         "
              "        (* i 2)))                  "
              "  ol)                              ",
              "(0 4 8 12 16)");

    TEST_EVAL("(for (i 1 1) (+ i 1))", "2");
    TEST_EVAL("(let ((x [])) (for (i -1 1) (push! x i)) x)", "(-1 0 1)");
    TEST_EVAL("(let ((x [])) (for (i -1 -1) (push! x i)) x)", "(-1)");

    TEST_EVAL("(let ((l [])) "
              "  (do-each (v [1 2 3]) "
              "    (push! l (* v 2))) "
              "  l)",
              "(2 4 6)");

    TEST_EVAL("(let ((l [])) "
              "  (do-each (k v [1 2 3]) "
              "    (push! l [k (* v 2)])) "
              "  l)",
              "((0 2) (1 4) (2 6))");

//    TEST_EVAL("(let ((l [])) "
//              "  (do-each (k v 123) "
//              "    (push! l [k (* v 2)])) "
//              "  l)",
//              "((nil 246))");

    TEST_EVAL("(let ((sum 0)) "
              "  (do-each (k v { a: 123 x: 999 'z 323 }) "
              "    (set! sum (+ sum v))) "
              "  sum)",
              "1445");

    TEST_EVAL("(let ((sum 0)) "
              "  (do-each (v { a: 123 x: 999 'z 323 }) "
              "    (set! sum (+ sum v))) "
              "  sum)",
              "1445");

    TEST_EVAL("(let ((a 0) (x 0) (z 0)) "
              "  (do-each (k v { a: 123 x: 999 'z 323 }) "
              "    (if (eqv? k a:) (set! a v) "
              "    (if (eqv? k x:) (set! x v) "
              "    (if (eqv? k 'z) (set! z v))))) "
              "  [a x z])",
              "(123 999 323)");
}
//---------------------------------------------------------------------------

void test_ieval_objs()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    TEST_EVAL("(let ((obj { x: (lambda (a) (+ a 10)) }))"
              "  (.x: obj 20))",
              "30");

    TEST_EVAL("(let ((obj { x: (lambda (a x) (set! x (* x 2)) (+ a 10 x)) }))"
              "  (.x: obj 20 12.5))",
              "55");

    TEST_EVAL("(let ((obj { 'x (lambda (a x) (set! x (* x 2)) (+ a 10 x)) }))"
              "  (.x obj 20 12.5))",
              "55");


    TEST_EVAL("(let ((obj {}))"
              "  ($define! obj (x: a x)"
              "    (set! x (* x 2))"
              "    (+ a 10 x))"
              "  (.x: obj 2.5 20))",
              "52.5");

    TEST_EVAL("(let ((obj {}))"
              "  ($define! obj (x a x)"
              "    (set! x (* x 2))"
              "    (+ a 10 x))"
              "  (.x obj 2.5 20))",
              "52.5");
}
//---------------------------------------------------------------------------

void test_ieval_comments()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    TEST_EVAL("(begin (define x 10) #;(set! x 20) x)",              "10");
    TEST_EVAL("#;()(begin #;[] (define x 10)#;{} #;(set! x 20) x)", "10");
    TEST_EVAL("(begin 13 #;12)",                    "13");
    TEST_EVAL("(begin 13 #;32.23)",                 "13");
    TEST_EVAL("(begin 13 #;[])",                    "13");
    TEST_EVAL("(begin 13 #;{})",                    "13");
    TEST_EVAL("(begin 13 #;#t)",                    "13");
    TEST_EVAL("(begin 13 #;nil)",                   "13");

    TEST_EVAL("(let ((x 10))\n;(set! x 20)\nx)",    "10");
    TEST_EVAL("(let ((x 10))\n#!(set! x 20)\nx)",   "10");

    TEST_EVAL("(begin 13 #|nil|#)",                 "13");
    TEST_EVAL("#|\n    test\n foo |#\n123",         "123");
    TEST_EVAL("(begin 13 #||#)",                    "13");
    TEST_EVAL("(begin (define |# 14) 13 #||#|#)",   "14");
    TEST_EVAL("(begin (define |# 14) 13 #|#||##|feuw weifew u #|fe wf wefwee w|#|#|#)",   "13");
}
//---------------------------------------------------------------------------

void test_ieval_eval()
{
    Runtime rt;
    Interpreter i(&rt);
    Atom r;

    TEST_EVAL("(eval 1)",                   "1");
    TEST_EVAL("(eval '(+ 1 2))",            "3");
    TEST_EVAL("(eval '(eval '(+ 1 2)))",    "3");
    TEST_EVAL("(let ((env (interaction-environment))) "
              "  (eval '(define x 12) env) "
              "  (eval 'x env))",
              "12");
}
//---------------------------------------------------------------------------

void test_bukalisp_instance()
{
    Instance i;

    auto factory = i.create_value_factory();

    auto val = (*factory).open_list()(1)(2)(3).close_list().value();

    TEST_EQSTR((*factory)(10).value()->to_write_str(),     "10",        "int");
    TEST_EQSTR((*factory)(10.2).value()->to_write_str(),   "10.2",      "dbl");
    TEST_EQSTR((*factory)(false).value()->to_write_str(),  "#false",    "bool");
    TEST_EQSTR((*factory)().value()->to_write_str(),       "nil",       "nil");
    TEST_EQSTR(
        ((*factory).open_list()(1)(2)(3).close_list().value()->to_write_str()),
        "(1 2 3)",
        "list 1");

    TEST_EQ(val->_i(0), 1, "get 1");
    TEST_EQ(val->_i(1), 2, "get 2");
    TEST_EQ(val->_i(2), 3, "get 3");
    TEST_EQ(val->_i(3), 0, "get 4");

    auto val2 = (*factory).open_list().read("foo:").close_list().value();
    TEST_EQSTR(val2->to_write_str(), "(foo:)", "read 1");
    val2 = (*factory).open_list().read("foo:").read("{x:(1 2 3)}").close_list().value();
    TEST_EQSTR(val2->to_write_str(), "(foo: {x: (1 2 3)})", "read 2");
    TEST_EQSTR(val2->_(1)->_("x")->to_write_str(),
               "(1 2 3)",
               "read 2b");
}
//---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    using namespace std::chrono;
    try
    {
        using namespace std;

        std::string input_file_path;
        std::string last_debug_sym;

        bool tests      = false;
        bool interpret  = false;
        bool i_trace    = false;
        bool i_force_gc = false;
        bool i_trace_vm = false;
        bool do_stat    = false;

        for (int i = 1; i < argc; i++)
        {
            std::string arg(argv[i]);
            if (arg == "tests")
                tests = true;
            else if (arg == "-i")
                interpret = true;
            else if (arg == "-g")
                i_force_gc = true;
            else if (arg == "-t")
                i_trace = true;
            else if (arg == "-T")
                i_trace_vm = true;
            else if (arg == "-S")
                do_stat = true;
            else if (arg[0] == '-')
            {
                std::cerr << "unknown option: " << argv[i] << std::endl;
                return -1;
            }
            else
                input_file_path = argv[i];
        }

        std::vector<BukaLISPModule *> bukalisp_modules;
//        bukalisp_modules.push_back(new BukaLISPModule(init_utillib()));
        bukalisp_modules.push_back(new BukaLISPModule(init_testlib()));
//        bukalisp_modules.push_back(new BukaLISPModule(init_syslib()));

        auto load_vm_modules = [&](VM &vm)
        {
            for (auto &mod : bukalisp_modules)
                vm.load_module(mod);
        };

        if (tests)
        {
            try
            {
#               define RUN_TEST(name)   test_##name(); std::cout << "OK - " << #name << std::endl;
////                RUN_TEST(gc1);
////                RUN_TEST(gc2);
//                RUN_TEST(atom_gen1);
//                RUN_TEST(atom_gen2);
//                RUN_TEST(symbols_and_keywords);
//                RUN_TEST(maps);
//                RUN_TEST(atom_printer);
//                RUN_TEST(maps2);
//                RUN_TEST(atom_hash_table);
//                RUN_TEST(atom_debug_info);
//                RUN_TEST(ieval_atoms);
//                RUN_TEST(ieval_vars);
//                RUN_TEST(ieval_basic_stuff);
//                RUN_TEST(ieval_proc);
//                RUN_TEST(ieval_let);
//                RUN_TEST(ieval_cond);
//                RUN_TEST(ieval_lambda);
//                RUN_TEST(ieval_index_procs);
//                RUN_TEST(ieval_loops);
//                RUN_TEST(ieval_objs);
//                RUN_TEST(ieval_comments);
//                RUN_TEST(ieval_eval);
                RUN_TEST(bukalisp_instance);

                cout << "TESTS OK" << endl;
            }
            catch (std::exception &e)
            {
                cerr << "TESTS FAIL, EXCEPTION: " << e.what() << endl;
            }
        }
        else if (do_stat)
        {
            cout << "Atom size:          " << sizeof(Atom)          << endl;
            cout << "AtomVec size:       " << sizeof(AtomVec)       << endl;
            cout << "AtomMap size:       " << sizeof(AtomMap)       << endl;

            std::vector<size_t> counts;
            counts.push_back(5);
            counts.push_back(10);
            counts.push_back(50);
            counts.push_back(200);
            counts.push_back(1000);
            counts.push_back(2000);
            counts.push_back(4000);
            counts.push_back(8000);
            counts.push_back(32000);
            for (auto c : counts)
            {
                size_t repeat = c < 1000 ? 100000 : 1000;

                {
                    BenchmarkTimer t;

                    int64_t sum = 0;

                    for (size_t j = 0; j < repeat; j++)
                    {
                        AtomMap am;
                        for (size_t i = 0; i < c; i++)
                        {
                            am.set(Atom(T_INT, i), Atom(T_INT, i * 10));
                        }

                        for (size_t j = 0; j < 10; j++)
                        {
                            for (size_t i = 0; i < c; i++)
                            {
                                sum += am.at(Atom(T_INT, i)).m_d.i;
                            }

                            ATOM_MAP_FOR(i, &am)
                            {
                                sum += MAP_ITER_VAL(i).m_d.i;
                            }
                        }
                    }

                    cout << "O[" << c << "](" << sum << "): " << t.diff() << endl;
                }
            }
        }
        else if (interpret && !input_file_path.empty())
        {
            try
            {
                Runtime rt;
                VM vm(&rt);
                load_vm_modules(vm);
                Interpreter i(&rt, &vm);
                vm.set_trace(i_trace_vm);
                i.set_trace(i_trace);
                i.set_force_always_gc(i_force_gc);
                Atom r = i.eval(input_file_path, slurp_str(input_file_path));
                std::string rs = write_atom(r);
                cout << rs << endl;
            }
            catch (std::exception &e)
            {
                cerr << "[" << input_file_path << "] Exception: " << e.what() << endl;
            }
        }
        else if (!input_file_path.empty())
        {
            Runtime rt;
            VM vm(&rt);
            load_vm_modules(vm);
            Interpreter i(&rt, &vm);
            vm.set_trace(i_trace_vm);
            i.set_trace(i_trace);
            i.set_force_always_gc(i_force_gc);

            GC_ROOT_MAP(rt.m_gc, root_env) = rt.m_gc.allocate_map();

            try
            {
                Atom r =
                    i.call_compiler(
                        input_file_path,
                        slurp_str(input_file_path),
                        root_env,
                        false);
                cout << r.to_write_str() << endl;
            }
            catch (std::exception &e)
            {
                cerr << "Exception: " << e.what() << endl;
            }
        }
        else if (interpret)
        {
            Runtime rt;
            VM vm(&rt);
            load_vm_modules(vm);
            Interpreter i(&rt, &vm);
            vm.set_trace(i_trace_vm);
            i.set_trace(i_trace);
            i.set_force_always_gc(i_force_gc);
            input_file_path = "<stdin>";

            std::string line;
            while (std::getline(std::cin, line))
            {
                try
                {
                    Atom r = i.eval(input_file_path, line);
                    cout << "> " << write_atom(r) << endl;
                }
                catch (std::exception &e)
                {
                    cerr << "Exception: " << e.what() << endl;
                }
            }
        }
        else
        {
            Runtime rt;
            VM vm(&rt);
            load_vm_modules(vm);
            Interpreter i(&rt, &vm);
            vm.set_trace(i_trace_vm);
            i.set_trace(i_trace);
            i.set_force_always_gc(i_force_gc);
            input_file_path = "<stdin>";

            GC_ROOT_MAP(rt.m_gc, root_env)
                = rt.m_gc.allocate_map();

            std::string line;
            while (std::getline(std::cin, line))
            {
                try
                {
                    if (line.size() > 0 && line[0] == '@')
                        line = slurp_str(line.substr(1, line.size()));

                    Atom r =
                        i.call_compiler(input_file_path, line, root_env, false);
                    cout << "> " << write_atom(r) << endl;
                }
                catch (std::exception &e)
                {
                    cerr << "Exception: " << e.what() << endl;
                }
            }
        }

        return 0;
    }
    catch (std::exception &e)
    {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    }
}
//---------------------------------------------------------------------------

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
