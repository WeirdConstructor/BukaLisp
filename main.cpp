#include <cstdio>
#include <fstream>
#include "lilvm.h"
#include "JSON.h"

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

      State     m_state;

      lilvm::Operation *m_cur_op;
      lilvm::VM        &m_vm;
      bool              m_second;


  public:
    LILASMParser(lilvm::VM &vm)
        : m_state(INIT),
          m_cur_op(nullptr),
          m_second(false),
          m_vm(vm)
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

#define HANDLE_OP_TYPE(type) \
        else if (sString == #type) m_cur_op->m_op = type;

        if (sString == "") m_cur_op->m_op = NOP;
        HANDLE_OP_TYPE(NOP)
        HANDLE_OP_TYPE(TRC)
        HANDLE_OP_TYPE(PUSH_I)
        HANDLE_OP_TYPE(PUSH_D)
        HANDLE_OP_TYPE(DBG_DUMP_STACK)
        HANDLE_OP_TYPE(ADD_I)
        HANDLE_OP_TYPE(ADD_D)
        HANDLE_OP_TYPE(JMP)
        HANDLE_OP_TYPE(BR_IF)
        HANDLE_OP_TYPE(LT_D)
        HANDLE_OP_TYPE(LE_D)
        HANDLE_OP_TYPE(GT_D)
        HANDLE_OP_TYPE(GE_D)
        HANDLE_OP_TYPE(LT_I)
        HANDLE_OP_TYPE(LE_I)
        HANDLE_OP_TYPE(GT_I)
        HANDLE_OP_TYPE(GE_I)
        HANDLE_OP_TYPE(PUSH_ENV)
        HANDLE_OP_TYPE(POP_ENV)
        HANDLE_OP_TYPE(SET_ENV)
        HANDLE_OP_TYPE(GET_ENV)
        else
            cout << "ERROR: Unknown OP type: " << sString << endl;
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
        size_t size = input_file.tellg();

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

bool run_test_prog(const std::string &filepath)
{
    try
    {
        VM vm;

        LILASMParser theParser(vm);

        UTF8Buffer *u8b = slurp(filepath);
        if (!u8b)
        {
            cout << "ERROR: No input?!" << endl;
            return 1;
        }

        try
        {
            theParser.parse(u8b);
            delete u8b;
        }
        catch (UTF8Buffer_exception &e)
        {
            cout << "UTF-8 error!" << endl;
            delete u8b;
        }

        Datum *d = vm.run();
        if (!d) return 1;
        return d->to_int() == 23 ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        cout << "Exception: " << e.what() << endl;
        return 1;
    }
}
//---------------------------------------------------------------------------

// TODO: Implement test suite
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

            if (run_test_prog(path) > 0)
            {
                cout << "*** FAIL: " << path << endl;
                break;
            }
            else
                cout << "OK: " << path << endl;
        }
    }
    else
        return run_test_prog(input_file_path);
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
