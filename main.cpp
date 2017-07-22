#include <cstdio>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include <cstdlib>
#include "utf8buffer.h"
#include "bukalisp/parser.h"
#include "bukalisp/atom_generator.h"
#include "bukalisp/interpreter.h"
#include "bukalisp/buklivm.h"
#include "atom.h"
#include "atom_printer.h"

//---------------------------------------------------------------------------

using namespace lilvm;
using namespace std;

UTF8Buffer *slurp(const std::string &filepath)
{
    ifstream input_file(filepath.c_str(),
                        ios::in | ios::binary | ios::ate);

    if (!input_file.is_open())
        throw bukalisp::BukLiVMException("Couldn't open '" + filepath + "'");

    size_t size = (size_t) input_file.tellg();

    // FIXME (maybe, but not yet)
    char *unneccesary_buffer_just_to_copy
        = new char[size];

    input_file.seekg(0, ios::beg);
    input_file.read(unneccesary_buffer_just_to_copy, size);
    input_file.close();

//        cout << "read(" << size << ")["
//             << unneccesary_buffer_just_to_copy << "]" << endl;

    UTF8Buffer *u8b =
        new UTF8Buffer(unneccesary_buffer_just_to_copy, size);
    delete[] unneccesary_buffer_just_to_copy;

    return u8b;
}
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
            + a + " != " + b);

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

class Reader : public lilvm::ExternalGCRoot
{
    private:
        GC                          m_gc;
        bukalisp::AtomGenerator     m_ag;
        bukalisp::Tokenizer         m_tok;
        bukalisp::Parser            m_par;
        bukalisp::AtomDebugInfo     m_deb_info;

        AtomVec                    *m_root_set;

    public:
        Reader()
            : lilvm::ExternalGCRoot(&m_gc),
              m_ag(&m_gc, &m_deb_info),
              m_par(m_tok, &m_ag)
        {
            m_root_set = m_gc.allocate_vector(0);

            lilvm::ExternalGCRoot::init();
        }

        virtual size_t gc_root_count() { return 1; }
        virtual AtomVec *gc_root_get(size_t i) { return m_root_set; }

        std::string debug_info(AtomVec *v) { return m_deb_info.pos((void *) v); }
        std::string debug_info(AtomMap *m) { return m_deb_info.pos((void *) m); }
        std::string debug_info(Atom &a)
        {
            switch (a.m_type)
            {
                case T_VEC: return m_deb_info.pos((void *) a.m_d.vec); break;
                case T_MAP: return m_deb_info.pos((void *) a.m_d.map); break;
                default: return "?:?";
            }
        }

        size_t pot_alive_vecs() { return m_gc.count_potentially_alive_vectors(); }
        size_t pot_alive_maps() { return m_gc.count_potentially_alive_maps(); }
        size_t pot_alive_syms() { return m_gc.count_potentially_alive_syms(); }

        Atom a_sym(const std::string &s) { return Atom(T_SYM, m_gc.new_symbol(s)); }
        Atom a_kw(const std::string &s) { return Atom(T_KW, m_gc.new_symbol(s)); }
        Atom a_str(const std::string &s) { return Atom(T_STR, m_gc.new_symbol(s)); }

        void make_always_alive(Atom a) { m_root_set->push(a); }

        void collect() { m_gc.collect(); }

        bool parse(const std::string &codename, const std::string &in)
        {
            m_tok.tokenize(codename, in);
            m_ag.start();
            // m_tok.dump_tokens();
            return m_par.parse();
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

    TEST_EQ(r.at(3).m_d.i, 4, "last atom");

    TEST_EQ(m.at(tc.a_kw("a")).m_type, T_INT, "map a key type");
    TEST_EQ(m.at(tc.a_kw("a")).m_d.i,  123,   "map a key");

    TEST_EQ(m.at(tc.a_kw("b")).at(2).m_type, T_MAP, "2nd map at right pos");

    tc.make_always_alive(m.at(tc.a_kw("b")));

    TEST_EQ(tc.pot_alive_maps(), 2, "alive map count");
    TEST_EQ(tc.pot_alive_vecs(), 3, "alive vec count");
    tc.collect();
    TEST_EQ(tc.pot_alive_maps(), 1, "alive map count after gc");
    TEST_EQ(tc.pot_alive_vecs(), 2, "alive vec count after gc");
}
//---------------------------------------------------------------------------

void test_symbols_and_keywords()
{
    Reader tc;

    TEST_EQ(tc.pot_alive_syms(), 0, "no syms alive at start");

    tc.parse("test_symbols_and_keywords",
             "(foo bar foo foo: baz: baz: foo:)");

    Atom r = tc.root();

    TEST_EQ(tc.pot_alive_syms(), 3, "some syms alive after parse");

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

    TEST_EQ(tc.pot_alive_syms(), 0, "no syms alive after collect");
}
//---------------------------------------------------------------------------

void test_maps2()
{
    Reader tc;

    tc.parse("test_atom_printer", "({ 123 5 }{ #t 34 }{ #f 5 }");

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

void test_ieval_atoms()
{
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);

    Atom r = i.eval("testatoms", "123");
    TEST_EQ(r.m_type, T_INT, "is int");
    TEST_EQ(r.to_int(), 123, "correct int");
}
//---------------------------------------------------------------------------

void test_ieval_vars()
{
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);

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

#define TEST_EVAL(expr, b) \
    r = i.eval(std::string(__FILE__ ":") + std::to_string(__LINE__), expr); \
    if (lilvm::write_atom(r) != (b)) \
        throw bukalisp::BukLiVMException( \
            std::string(__FILE__ ":") + std::to_string(__LINE__) +  "| " + \
            #expr " not eq: " \
            + lilvm::write_atom(r) + " != " + (b));

//---------------------------------------------------------------------------

void test_ieval_basic_stuff()
{
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);
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
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);
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
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);
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
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);
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
}
//---------------------------------------------------------------------------

void test_ieval_lambda()
{
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);
    Atom r;

    TEST_EVAL("((lambda () 10))",                               "10");
    TEST_EVAL("((lambda (x) (+ x 10)) 20)",                     "30");
    TEST_EVAL("((let ((a 1.2)) (lambda (x) (+ a x 10))) 20)",   "31.2");
    TEST_EVAL("(begin "
              "  (define x (let ((a 1.2)) "
              "              (lambda (x) (+ a x 10)))) "
              "  (x 20))",   "31.2");
}
//---------------------------------------------------------------------------

void test_ieval_index_procs()
{
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);
    Atom r;

    TEST_EVAL("(@0 [1 2 3])",       "1");
    TEST_EVAL("(@1 [1 2 3])",       "2");
    TEST_EVAL("(@2 [1 2 3])",       "3");
    TEST_EVAL("(@3 [1 2 3])",       "nil");

    TEST_EVAL("($xxx:     {a: 123 'b 444 \"xxx\" 3.4})",               "nil");
    TEST_EVAL("($\"xxx\"  {a: 123 'b 444 \"xxx\" 3.4})",               "3.4");
    TEST_EVAL("($'a       {a: 123 'b 444 \"xxx\" 3.4})",               "nil");
    TEST_EVAL("($a:       {a: 123 'b 444 \"xxx\" 3.4})",               "123");
    TEST_EVAL("($\"b\"    {a: 123 'b 444 \"xxx\" 3.4})",               "nil");
    TEST_EVAL("($'b       {a: 123 'b 444 \"xxx\" 3.4})",               "444");
    TEST_EVAL("(let ((key xxx:)) ($key {a: 123 'b 444 xxx: 3.4}))", "3.4");

    TEST_EVAL("(let ((m {})) ($!'x m 123) m)",                              "{x 123}");
    TEST_EVAL("(let ((m {})) ($!x: m 123) ($!'x m 344) [($x: m) ($'x m)])", "(123 344)");

    TEST_EVAL("(let ((v (list))) (@!0 v 2) (@!10 v 99) v)",
              "(2 nil nil nil nil nil nil nil nil nil 99)");
}
//---------------------------------------------------------------------------

void test_ieval_loops()
{
    bukalisp::Runtime rt;
    bukalisp::Interpreter i(&rt);
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

    TEST_EVAL("(let ((l [])) "
              "  (do-each (v [1 2 3]) "
              "    (push l (* v 2))) "
              "  l)",
              "(2 4 6)");

    TEST_EVAL("(let ((l [])) "
              "  (do-each (k v [1 2 3]) "
              "    (push l [k (* v 2)])) "
              "  l)",
              "((0 2) (1 4) (2 6))");

    TEST_EVAL("(let ((l [])) "
              "  (do-each (k v 123) "
              "    (push l [k (* v 2)])) "
              "  l)",
              "((nil 246))");

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

int main(int argc, char *argv[])
{
    using namespace std::chrono;
    try
    {
        using namespace std;

        std::string input_file_path = "input.json";
        std::string last_debug_sym;

        if (argc > 1)
            input_file_path = string(argv[1]);

        cout << "SIL: " << sizeof(bukalisp::INST) << endl;

        if (input_file_path == "tests")
        {
            try
            {
#               define RUN_TEST(name)   test_##name(); std::cout << "OK - " << #name << std::endl;
                RUN_TEST(gc1);
                RUN_TEST(gc2);
                RUN_TEST(atom_gen1);
                RUN_TEST(atom_gen2);
                RUN_TEST(symbols_and_keywords);
                RUN_TEST(maps);
                RUN_TEST(atom_printer);
                RUN_TEST(maps2);
                RUN_TEST(atom_debug_info);
                RUN_TEST(ieval_atoms);
                RUN_TEST(ieval_vars);
                RUN_TEST(ieval_basic_stuff);
                RUN_TEST(ieval_proc);
                RUN_TEST(ieval_let);
                RUN_TEST(ieval_cond);
                RUN_TEST(ieval_lambda);
                RUN_TEST(ieval_index_procs);
                RUN_TEST(ieval_loops);

                cout << "TESTS OK" << endl;
            }
            catch (std::exception &e)
            {
                cerr << "TESTS FAIL, EXCEPTION: " << e.what() << endl;
            }
        }
        else if (!input_file_path.empty())
        {
            try
            {
                bukalisp::Runtime rt;
                bukalisp::VM vm(&rt);
                bukalisp::Interpreter i(&rt, &vm);
                UTF8Buffer *u8b = slurp(input_file_path);
                Atom r = i.eval(input_file_path, u8b->as_string());
                std::string rs = write_atom(r);
                delete u8b;
                cout << rs << endl;
            }
            catch (std::exception &e)
            {
                cerr << "[" << input_file_path << "] Exception: " << e.what() << endl;
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
