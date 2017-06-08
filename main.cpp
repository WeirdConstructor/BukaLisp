#include <cstdio>
#include <fstream>
#include "lilvm.h"
#include "JSON.h"
#include "symtbl.h"
#include "bukalisp/code_emitter.h"

//---------------------------------------------------------------------------

using namespace lilvm;
using namespace std;

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

      lilvm::Operation *m_cur_op;
      lilvm::VM        &m_vm;
      bool              m_second;
      bool              m_sym;


  public:
    LILASMParser(lilvm::VM &vm)
        : m_state(INIT),
          m_cur_op(nullptr),
          m_second(false),
          m_vm(vm),
          m_symtbl(new SymTable(1000)),
          m_sym(false)
    {}
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
                m_state = READ_OP;
                m_second = false;
                m_sym    = false;
                m_cur_op = new Operation(NOP);
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
                m_vm.append(m_cur_op);
                m_cur_op = nullptr;

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
        cout << "ERROR: Parsing the JSON: " << csError << endl;
    }

    virtual void onValueBoolean(bool bValue) { (void) bValue; }
    virtual void onValueNull()               {}
    virtual void onValueNumber(const char *csNumber, bool bIsFloat)
    {
        if (!m_cur_op)
        {
            cout << "ERROR: Bad OP sequence in JSON, bad OP type?" << endl;
            return;
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
            cout << "ERROR: Can't define OP type at this point!" << endl;
            return;
        }

        if (m_sym)
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

            if (sString == "") m_cur_op->m_op = NOP;

            bool found = false;
            for (int i = 0; OPCODE_NAMES[i][0] != '\0'; i++)
            {
                if (sString == OPCODE_NAMES[i])
                {
                    m_cur_op->m_op = (OPCODE) i;
                    found = true;
                }
            }

            if (!found)
                cout << "ERROR: Unknown OP type: " << sString << endl;
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

    if (input_file.is_open())
    {
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
    else
    {
        cout << "ERROR: Couldn't open: '" << filepath << endl;
        return nullptr;
    }
}
//---------------------------------------------------------------------------

bool run_test_prog_ok(const std::string &filepath)
{
    try
    {
        int retcode = -11;
        {
            VM vm;

            LILASMParser theParser(vm);

            UTF8Buffer *u8b = slurp(filepath);
            if (!u8b)
            {
                cout << "ERROR: No input?!" << endl;
                return false;
            }

            try
            {
                theParser.parse(u8b);
                delete u8b;
            }
            catch (UTF8Buffer_exception &)
            {
                cout << "UTF-8 error!" << endl;
                delete u8b;
            }

            Datum *d = vm.run();
            if (!d) return false;

            retcode = (int) d->to_int();
        }

        if (Datum::s_instance_counter > 0)
        {
            cout << "ERROR: "
                 << Datum::s_instance_counter << " friggin leaked Datums!"
                 << endl;
            return false;
        }

        return retcode == 23 ? true : false;
    }
    catch (const std::exception &e)
    {
        cout << "Exception: " << e.what() << endl;
        return false;
    }

    return false;
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
        default:
            cout << "ERROR: Unknown ASTNode Type!" << endl;
    }
}
//---------------------------------------------------------------------------

// TODO: Improve test suite handling of compilation!?
void test_parser(const std::string &input)
{
    using namespace bukalisp;
    Tokenizer tok;
    Parser p(tok);

    tok.tokenize(input);
    ASTNode *anode = p.parse();
    if (anode)
    {
        ast_debug_walker(anode);

        bukalisp::ASTJSONCodeEmitter code_emit;
        code_emit.open_output_file("code_emit_test.json");
        code_emit.emit_json(anode);
    }
    else
        cout << "ERROR: Compile Error!" << endl;
}
//---------------------------------------------------------------------------

// TODO: Implement basic comparsion and jump operations
// TODO: Implement argv stack
// TODO: Implement function type (pointer, arity and argv-stack are members)

int main(int argc, char *argv[])
{
    using namespace std;

    std::string input_file_path = "input.json";

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

            if (run_test_prog_ok(path))
            {
                cout << "OK: " << path << endl;
            }
            else
            {
                cout << "*** FAIL: " << path << endl;
                break;
            }
        }
    }
    else if (input_file_path == "parser_test")
    {
        if (argc > 2) test_parser(argv[2]);
        else          test_parser("");
    }
    else
    {
        if (run_test_prog_ok(input_file_path))
        {
            cout << "OK: " << input_file_path << endl;
            return 0;
        }
        else
        {
            cout << "*** FAIL: " << input_file_path << endl;
            return 1;
        }
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
