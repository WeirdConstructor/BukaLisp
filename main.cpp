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


  public:
    LILASMParser(lilvm::VM &vm)
        : m_state(INIT),
          m_cur_op(nullptr),
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

        if (bIsFloat)
            m_cur_op->m_1.d = stod(csNumber);
        else
            m_cur_op->m_1.i = stoll(csNumber);
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
        HANDLE_OP_TYPE(PUSH_I)
        HANDLE_OP_TYPE(PUSH_D)
        HANDLE_OP_TYPE(DBG_DUMP_STACK)
        HANDLE_OP_TYPE(ADD_I)
        HANDLE_OP_TYPE(ADD_D)
        HANDLE_OP_TYPE(GOTO)
        HANDLE_OP_TYPE(BRANCH_IF)
        HANDLE_OP_TYPE(LT_D)
        HANDLE_OP_TYPE(LE_D)
        HANDLE_OP_TYPE(GT_D)
        HANDLE_OP_TYPE(GE_D)
        HANDLE_OP_TYPE(LT_I)
        HANDLE_OP_TYPE(LE_I)
        HANDLE_OP_TYPE(GT_I)
        HANDLE_OP_TYPE(GE_I)
        HANDLE_OP_TYPE(DEF_ARGS)
        HANDLE_OP_TYPE(CALL)
        HANDLE_OP_TYPE(RET)
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

        cout << "read(" << size << ")["
             << unneccesary_buffer_just_to_copy << "]" << endl;

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

int main(int argc, char *argv[])
{
    using namespace std;

    std::string input_file_path = "input.json";

    if (argc > 1)
        input_file_path = string(argv[1]);

    try
    {
        VM vm;

        LILASMParser theParser(vm);

        UTF8Buffer *u8b = slurp(input_file_path);
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

        vm.run();
        return 0;
    }
    catch (const std::exception &e)
    {
        cout << "Exception: " << e.what() << endl;
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
