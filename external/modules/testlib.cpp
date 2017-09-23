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

#include "testlib.h"
#include <modules/vval_util.h>

using namespace VVal;
using namespace std;

//---------------------------------------------------------------------------

VV_CLOSURE(test_init)
{
    vv_obj->set("ok", vv_bool(true));
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE(test_id)
{
    return vv_args->_(0);
}
//---------------------------------------------------------------------------

VV_CLOSURE(test_is_inited)
{
    return vv_obj->_("ok");
}
//---------------------------------------------------------------------------

VV_CLOSURE(test_get_c_ptr)
{
    return vv_ptr((void *) 0x12356789, "");
}
//---------------------------------------------------------------------------

VV_CLOSURE(test_get_vv_ptr)
{
    return vv_ptr((void *) 0x12356789, "CCC");
}
//---------------------------------------------------------------------------

VV_CLOSURE(test_destroy)
{
    return vv_undef();
}
//---------------------------------------------------------------------------

BukaLISPModule init_testlib()
{
    VV reg(vv_list() << vv("test"));
    VV obj(vv_map());
    VV mod(vv_list());
    reg << mod;

#define SET_FUNC(functionName, closureName) \
    mod << (vv_list() << vv(#functionName) << VVC_NEW_##closureName(obj))

    SET_FUNC(__INIT__,    test_init);
    SET_FUNC(__DESTROY__, test_destroy);

    SET_FUNC(isInited,    test_is_inited);
    SET_FUNC(identity,    test_id);
    SET_FUNC(getCPtr,     test_get_c_ptr);
    SET_FUNC(getVVPtr,    test_get_vv_ptr);

    return BukaLISPModule(reg);
}
//---------------------------------------------------------------------------
