#include <cstdio>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include "utf8buffer.h"
#include "bukalisp/parser.h"
#include "bukalisp/atom_generator.h"
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
        throw BukLiVMException("Couldn't open '" + filepath + "'");

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
    if (!(b)) throw BukLiVMException( \
        std::string(__FILE__ ":") + std::to_string(__LINE__) +  "| " + \
        std::string(msg) + " in: " #b);

#define TEST_EQ(a, b, msg) \
    if ((a) != (b)) \
        throw BukLiVMException( \
            std::string(__FILE__ ":") + std::to_string(__LINE__) +  "| " + \
            std::string(msg) + ", not eq: " \
            + std::to_string(a) + " != " + std::to_string(b));

#define TEST_EQSTR(a, b, msg) \
    if ((a) != (b)) \
        throw BukLiVMException( \
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

class Reader
{
    private:
        GC                  m_gc;
        AtomGenerator       m_ag;
        bukalisp::Tokenizer m_tok;
        bukalisp::Parser    m_par;
        AtomDebugInfo       m_deb_info;

    public:
        Reader()
            : m_ag(&m_gc, &m_deb_info),
              m_par(m_tok, &m_ag)
        {
        }

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

        void make_always_alive(Atom a)
        {
            AtomVec *av = m_gc.allocate_vector(1);
            av->m_data[0] = a;
            m_gc.add_root(av);
        }

        void collect() { m_gc.collect(); }

        bool parse(const std::string &codename, const std::string &in)
        {
            m_tok.tokenize(codename, in);
            // m_tok.dump_tokens();
            return m_par.parse();
        }

        Atom &root() { return m_ag.root(); }
};
//---------------------------------------------------------------------------

void test_gc2()
{
    Reader tc;

    tc.parse("test_gc2", "(1 2 3 (4 5 6) (943 203))");

    TEST_EQ(tc.pot_alive_vecs(), 3, "potentially alive before");

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

    TEST_EQ(m.at("a").m_type, T_INT, "map a key type");
    TEST_EQ(m.at("a").m_d.i,  123,   "map a key");

    TEST_EQ(m.at("b").at(2).m_type, T_MAP, "2nd map at right pos");

    tc.make_always_alive(m.at("b"));

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
        "({\"123\" 5} {\"#t\" 34} {\"#f\" 5})",
        "atom print 1");
}
//---------------------------------------------------------------------------

void test_atom_printer()
{
    Reader tc;

    tc.parse("test_atom_printer", "(1 2.3 foo foo: \"foo\" { axx: #t } { bxx #f } { \"test\" 123 })");

    Atom r = tc.root();

    TEST_EQSTR(
        write_atom(r),
        "(1 2.3 foo foo: \"foo\" {\"axx\" #t} {\"bxx\" #f} {\"test\" 123})",
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

        if (input_file_path == "tests")
        {
            try
            {
                test_gc1();
                test_gc2();
                test_atom_gen1();
                test_atom_gen2();
                test_symbols_and_keywords();
                test_maps();
                test_atom_printer();
                test_maps2();
                test_atom_debug_info();
                cout << "TESTS OK" << endl;
            }
            catch (std::exception &e)
            {
                cerr << "TESTS FAIL, EXCEPTION: " << e.what() << endl;
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
