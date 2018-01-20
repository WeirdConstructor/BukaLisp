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

#include "ev_loop_lib.h"

using namespace VVal;
using namespace std;
//using namespace boost::asio;

//---------------------------------------------------------------------------

VV_CLOSURE_DOC(ev_loop_new,
"@ev-loop procedure (ev-loop-new)\n\n"
"Creates a new event loop instance.\n"
"See also `ev-loop-free`.\n"
)
{
    EventLoop *evl = new EventLoop;
    return vv_ptr((void *) evl, "ev-loop:instance");
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(ev_loop_run,
"@ev-loop procedure (ev-loop-run _ev-loop-handle_)\n\n"
"Runs the event loop until `ev-loop-stop` is called.\n"
)
{
    EventLoop *evl =
        vv_args->_P<EventLoop>(0, "ev-loop:instance");
    evl->run();
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(ev_loop_timeout,
"@ev-loop procedure (ev-loop-timeout _ev-loop-handle_ _timeout-in-ms_ _cb_)\n\n"
"Calls _cb_ after _timeout-in-ms_.\n"
"\n"
"    (ev-loop-timeout evl-handle 1000\n"
"      (lambda (error?) (displayln after-1-sec:)))\n"
)
{
    EventLoop *evl =
        vv_args->_P<EventLoop>(0, "ev-loop:instance");

    evl->start_timer(vv_args->_(1), vv_args->_(2));
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(ev_loop_free,
"@ev-loop procedure (ev-loop-free _ev-loop-handle_)\n\n"
"Destroys an previously with `ev-loop-new` allocated _ev-loop-handle_.\n"
)
{
    EventLoop *evl =
        vv_args->_P<EventLoop>(0, "ev-loop:instance");
    evl->kill();
    return vv_undef();
}
//---------------------------------------------------------------------------

BukaLISPModule init_ev_loop_lib()
{
    VV reg(vv_list() << vv("ev-loop"));
    VV obj(vv_map());
    VV mod(vv_list());
    reg << mod;

#define SET_FUNC(functionName, closureName) \
    mod << (vv_list() << vv(#functionName) << VVC_NEW_##closureName(obj))

//    SET_FUNC(__INIT__,    util_init);
//    SET_FUNC(__DESTROY__, util_destroy);

    SET_FUNC(new,       ev_loop_new);
    SET_FUNC(free,      ev_loop_free);
    SET_FUNC(run,       ev_loop_run);
    SET_FUNC(timeout,   ev_loop_timeout);

    return BukaLISPModule(reg);
}
//---------------------------------------------------------------------------

