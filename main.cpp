#include <cstdio>
#include "JSON.h"

class LILASMParser : public json::Parser
{
  private:

  public:
    LILASMParser() {}
    virtual ~LILASMParser() { }

    virtual void onObjectStart()                      {}
    virtual void onObjectKey(const std::string &sOut) { (void) sOut; }
    virtual void onObjectValueDone()                  { }
    virtual void onObjectEnd()                        {}

    virtual void onArrayStart()                       {}
    virtual void onArrayValueDone()                   {}
    virtual void onArrayEnd()                         {}
    virtual void onError(UTF8Buffer *u8Buf, const char *csError) { (void) u8Buf; (void) csError; }

    virtual void onValueBoolean(bool bValue) { (void) bValue; }
    virtual void onValueNull()               {}
    virtual void onValueNumber(const char *csNumber, bool bIsFloat) { (void) csNumber; (void) bIsFloat; }
    virtual void onValueString(const std::string &sString)          { (void) sString; }
};

int test();

int main(int argc, char *argv[])
{
    printf("%d\n", test());
    return 0;
}
