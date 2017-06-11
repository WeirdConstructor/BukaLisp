#include <cstdio>
#include <fstream>
#include "lilvm.h"
#include "JSON.h"
#include "symtbl.h"
#include "bukalisp/code_emitter.h"

//---------------------------------------------------------------------------

using namespace lilvm;
using namespace std;

class BukLiVMException : public std::exception
{
    private:
        std::string m_err;
    public:
        BukLiVMException(const std::string &err) : m_err(err) { }
        virtual const char *what() const noexcept { return m_err.c_str(); }
        virtual ~BukLiVMException() { }
};
//---------------------------------------------------------------------------

class LILASMParser : public json::Parser
{
  private:
      enum State
      {
          INIT,
          READ_PROG,
          READ_OP
      };

      SymTable *m_symtbl;
      State     m_state;
      Sym      *m_cur_debug_sym;

      lilvm::Operation *m_cur_op;
      lilvm::VM        &m_vm;
      bool              m_second;
      bool              m_sym;
      bool              m_debug_sym;


  public:
    LILASMParser(lilvm::VM &vm)
        : m_state(INIT),
          m_cur_op(nullptr),
          m_second(false),
          m_vm(vm),
          m_symtbl(new SymTable(1000)),
          m_sym(false),
          m_debug_sym(false)
    {
        m_cur_debug_sym = m_symtbl->str2sym("");
    }

    Sym *last_debug_sym() { return m_cur_debug_sym; }

    virtual ~LILASMParser() { }

    virtual void onObjectStart()                      {}
    virtual void onObjectKey(const std::string &sOut) { (void) sOut; }
    virtual void onObjectValueDone()                  { }
    virtual void onObjectEnd()                        {}

    virtual void onArrayStart()
    {
        switch (m_state)
        {
            case INIT: // opening "[" in the file
                m_state = READ_PROG;
                break;

            case READ_PROG:
                m_state     = READ_OP;
                m_second    = false;
                m_sym       = false;
                m_debug_sym = false;
                m_cur_op    = new Operation(NOP);
                break;

            default:
                // Ignore it boldly
                break;
        }
    }

    virtual void onArrayValueDone()
    {
    }

    virtual void onArrayEnd()
    {
        switch (m_state)
        {
            case READ_OP: // opening "[" in the file
                if (m_cur_op)
                {
                    m_cur_op->m_min_arg_cnt =
                        Operation::min_arg_cnt_for_op(
                            m_cur_op->m_op,
                            *m_cur_op);
                    m_vm.append(m_cur_op);
                    m_cur_op = nullptr;
                }

                m_state = READ_PROG;
                break;

            case READ_PROG:
                // TODO: do something useful?
                m_state = INIT;
                break;

            default:
                // Ignore it boldly
                break;
        }
    }
    virtual void onError(UTF8Buffer *u8Buf, const char *csError)
    {
        throw BukLiVMException("Parsing the JSON: " + string(csError));
    }

    virtual void onValueBoolean(bool bValue) { (void) bValue; }
    virtual void onValueNull()               {}
    virtual void onValueNumber(const char *csNumber, bool bIsFloat)
    {
        if (!m_cur_op)
        {
            throw BukLiVMException("Bad OP sequence in JSON, bad OP type?");
        }

        if (m_second)
        {
            if (bIsFloat)
                m_cur_op->m_2.d = stod(csNumber);
            else
                m_cur_op->m_2.i = stoll(csNumber);
        }
        else
        {
            if (bIsFloat)
                m_cur_op->m_1.d = stod(csNumber);
            else
                m_cur_op->m_1.i = stoll(csNumber);
        }

        m_second = true;
    }

    virtual void onValueString(const std::string &sString)
    {
        if (!m_cur_op)
        {
            throw BukLiVMException("Can't define OP type at this point!");
        }

        if (m_debug_sym)
        {
            m_cur_debug_sym = m_symtbl->str2sym(sString);
        }
        else if (m_sym)
        {
            Sym *sym = m_symtbl->str2sym(sString);
            if (m_second)
                m_cur_op->m_2.sym = sym;
            else
                m_cur_op->m_1.sym = sym;
        }
        else
        {
            m_sym = true;

            if (sString == "#DEBUG_SYM")
            {
                m_debug_sym = true;
                return;
            }

            if (sString == "") m_cur_op->m_op = NOP;

            bool found = false;
            for (int i = 0; OPCODE_NAMES[i][0] != '\0'; i++)
            {
                if (sString == OPCODE_NAMES[i])
                {
                    m_cur_op->m_op        = (OPCODE) i;
                    m_cur_op->m_debug_sym = m_cur_debug_sym;
                    found = true;
                }
            }

            if (!found)
                throw BukLiVMException("Unknown OP type: " + sString);
        }
    }
};
//---------------------------------------------------------------------------

int test();
//---------------------------------------------------------------------------

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

bool run_test_prog(UTF8Buffer *input, Sym *&last_debug_sym)
{
    try
    {
        int retcode = -11;
        {
            VM vm;

            LILASMParser theParser(vm);

            try
            {
                theParser.parse(input);
                last_debug_sym = theParser.last_debug_sym();
            }
            catch (std::exception &e)
            {
                cerr << "ERROR: " << e.what() << endl;
                last_debug_sym = theParser.last_debug_sym();
                return false;
            }
            catch (UTF8Buffer_exception &)
            {
                cerr << "UTF-8 error!" << endl;
                last_debug_sym = theParser.last_debug_sym();
                return false;
            }

            Datum *d = vm.run();
            if (!d) return false;

            retcode = (int) d->to_int();
        }

        if (Datum::s_instance_counter > 1)
        {
            std::stringstream ss;
            ss << (Datum::s_instance_counter - 1) << " friggin leaked Datums!";
            throw BukLiVMException(ss.str());
        }

        return retcode == 23 ? true : false;
    }
    catch (const std::exception &e)
    {
        cerr << "ERROR: " << e.what() << endl;
        return false;
    }

    return false;
}
//---------------------------------------------------------------------------

bool run_test_prog_ok(const std::string &filepath, Sym *&last_debug_sym)
{
    UTF8Buffer *u8b = slurp(filepath);
    bool ret = run_test_prog(u8b, last_debug_sym);
    delete u8b;
    return ret;
}
//---------------------------------------------------------------------------

void ast_debug_walker(bukalisp::ASTNode *n, int indent_level = 0)
{
    using namespace bukalisp;

    for (int i = 0; i < 4 * indent_level; i++)
        cout << " ";

    switch (n->m_type)
    {
        case A_DBL:
            cout << "* DBL (" << n->m_num.d << ")" << endl; break;

        case A_INT:
            cout << "* INT (" << n->m_num.i << ")" << endl; break;

        case A_VAR:
            cout << "* VAR (" << n->m_text << ")" << endl; break;

        case A_LIST:
        {
            cout << "* LIST" << endl;
            for (auto child : n->m_childs)
                ast_debug_walker(child, indent_level + 1);
            break;
        }

        case A_LLIST:
        {
            cout << "* LLIST" << endl;
            for (auto child : n->m_childs)
                ast_debug_walker(child, indent_level + 1);
            break;
        }

        default:
            throw BukLiVMException("Unknown ASTNode Type: "
                                   + to_string(n->m_type));
    }
}
//---------------------------------------------------------------------------

// TODO: Improve test suite handling of compilation!?
void test_parser(const std::string &input)
{
    using namespace bukalisp;
    Tokenizer tok;
    Parser p(tok);

    tok.tokenize("stdin", input);
    ASTNode *anode = p.parse();
    if (anode)
    {
        ast_debug_walker(anode);

        bukalisp::ASTJSONCodeEmitter code_emit;
        std::fstream out(input.c_str(), std::ios::out);
        code_emit.set_output(&out);
        code_emit.emit_json(anode);
    }
    else
        throw BukLiVMException("BukaLISP Compile Error!");
}
//---------------------------------------------------------------------------

void test_case(const std::string &inp_file)
{
    using namespace bukalisp;
    Tokenizer tok;
    Parser p(tok);

    auto u8b = slurp(inp_file);
    tok.tokenize(inp_file, u8b->as_string());
    delete u8b;

    ASTNode *anode = p.parse();
    if (anode)
    {
        ast_debug_walker(anode);

        bukalisp::ASTJSONCodeEmitter code_emit;
        std::stringstream os;
        code_emit.set_output(&os);
        code_emit.emit_json(anode);
        std::string js = os.str();

        cout << "EXEC<<" << js << ">>" << endl;
        UTF8Buffer u8b(js.data(), js.size());

        Sym *last_debug_sym = nullptr;
        if (run_test_prog(&u8b, last_debug_sym))
        {
            cout << "OK: " << inp_file
                 << " [" << (last_debug_sym ? last_debug_sym->m_str : "") << "]"
                 << endl;
        }
        else
        {
            cout << "*** FAIL: " << inp_file
                 << " [" << (last_debug_sym ? last_debug_sym->m_str : "") << "]"
                 << endl;
        }
    }
    else
        throw BukLiVMException("BukaLISP Compile Error!");
}
//---------------------------------------------------------------------------

// TODO: Implement basic comparsion and jump operations
// TODO: Implement argv stack
// TODO: Implement function type (pointer, arity and argv-stack are members)

int main(int argc, char *argv[])
{
    try
    {
        using namespace std;

        std::string input_file_path = "input.json";
        Sym *last_debug_sym = nullptr;

        if (argc > 1)
            input_file_path = string(argv[1]);

        if (input_file_path == "tests")
        {
            for (int i = 0; i < 100; i++)
            {
                std::string path = "tests\\test" + to_string(i) + ".json";
                ifstream input_file(path.c_str(), ios::in);
                if (!input_file.is_open())
                    break;
                input_file.close();


                if (run_test_prog_ok(path, last_debug_sym))
                {
                    cout << "OK: " << path
                         << " [" << (last_debug_sym ? last_debug_sym->m_str : "") << "]"
                         << endl;
                }
                else
                {
                    cout << "*** FAIL: " << path
                         << " [" << (last_debug_sym ? last_debug_sym->m_str : "") << "]"
                         << endl;
                    break;
                }
            }
        }
        else if (input_file_path == "parser_test")
        {
            if (argc > 2) test_parser(argv[2]);
            else          test_parser("");
        }
        else if (input_file_path.substr(input_file_path.size() - 4, 4) == "bkli")
        {
            test_case(input_file_path);
        }
        else
        {
            if (run_test_prog_ok(input_file_path, last_debug_sym))
            {
                cout << "OK: " << input_file_path
                               << " [" << (last_debug_sym ? last_debug_sym->m_str : "") << "]"
                               << endl;
                return 0;
            }
            else
            {
                cout << "*** FAIL: " << input_file_path
                     << " [" << (last_debug_sym ? last_debug_sym->m_str : "") << "]"
                     << endl;
                return 1;
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

//        Operation *o = nullptr;
//
//        o = new Operation(NOP);
//        vm.append(o);
//
//        o = new Operation(PUSH_I);
//        o->m_1.i = 123;
//        vm.append(o);
//
//        o = new Operation(PUSH_D);
//        o->m_1.d = 432.12;
//        vm.append(o);
//
//        o = new Operation(ADD_D);
//        vm.append(o);
//
//        o = new Operation(PUSH_I);
//        o->m_1.i = 123;
//        vm.append(o);
//
//        o = new Operation(PUSH_D);
//        o->m_1.d = 432.12;
//        vm.append(o);
//
//        o = new Operation(ADD_I);
//        vm.append(o);
//
//        o = new Operation(DBG_DUMP_STACK);
//        vm.append(o);

//---------------------------------------------------------------------------
