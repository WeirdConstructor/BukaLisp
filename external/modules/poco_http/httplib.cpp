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

#include "base/http.h"
#include "rt/httplib.h"
#include "rt/lua_thread.h"
#include "rt/lua_thread_helper.h"

using namespace VVal;
using namespace std;

namespace lal_rt
{
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(http_bind,
"@http procedure (http-bind _port-number_)\n\n"
"Binds a HTTP server to the TCP _port-number_.\n"
"Returns a token, that can be used for waiting on new requests.\n"
"If there is an error, an exception will be thrown.\n"
"\n"
"    (let ((f (http-bind 18099)))\n"
"      (do ((req (mp-wait f) (mp-wait f)))\n"
"          (#t #t)\n"
"        (let ((handle (@1 f)))\n"
"          (http-response\n"
"           handle   ; srv-handle\n"
"           (@1 req) ; req-token\n"
"           { :action :json :data { :x 1 :y 2 } }))))\n"
)
{
    auto t = LT;
    int64_t srv_token = t->m_port.new_token();
    http_srv::Server *s = new http_srv::Server;
    s->setup(
        [srv_token, t](const VVal::VV &req)
        {
            return
                t->m_port.emit_message(
                    vv_list() << vv(srv_token) << req, t->m_port.pid());
        });
    s->start((unsigned int) vv_args->_i(0));
    LT->register_resource(s);

    return vv_list() << srv_token << vv_ptr((void *) s, "http_srv::Server");
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(http_response,
"@http:rt-http procedure (http-response _server-handle_ _request-token_ _response-data_)\n"
"Sends the _response-data_ back to the _server-handler_.\n"
"_response-data_ should be a map, that should provide the `:action` key\n"
"to set the kind of response.\n"
"Throws an exception if an error occured (for example, when replying to a\n"
"request twice).\n"
"This is a possible map:\n"
"\n"
"    { :action :json :data some-data-structure }\n"
"    { :action :file :path \"webdata/index.html\" } ; content-type is automatically generated\n"
"    { :action :file :path \"webdata/index.html\" :contenttype \"text/html; charset=utf-8\" }\n"
"    { :action :data :data \"foobar\" :contenttype \"text/plain; charset=utf-8\" }\n"
"\n\nFor an example see `http-bind`\n"
)
{
    LTRES(s, 0, http_srv::Server);
    s->reply(vv_args->_i(1), vv_args->_(2));
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(http_free,
"@http procedure (http-free _server-handle_)\n"
"Frees the HTTP-Server handle.\n"
)
{
    LTRES(s, 0, http_srv::Server);
    LT->delete_resource(s);
    delete s;
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(http_get,
"@http procedure (http-get _url_ _options_)\n"
"A very simple HTTP GET implementation that returns a data structure\n"
"containing the response. And conveniently decodes JSON based on the\n"
"Content-Type of the response.\n"
"Throws an exception if an error occured.\n"
"_options_ is a map that can contain following items:\n"
"\n"
"    {cookies:  _map-of-cookie-names-and-values_}\n"
"    {headers:  _map-of-header-names-and-values_}\n"
"\n"
"Also notice, that you only get headers and cookies in the response if the\n"
"option for it has at least a defined value (eg. empty map)\n"
"\n"
"    (let ((resp (http-get \"http://localhost/test.json\")))\n"
"      (display (str \"Status/Reason:\" ($:status resp) ($:reason resp)))\n"
"      (display (str \"Got JSON:\" (write-str ($:data resp))))\n"
"      (display (str \"Body:\" ($:body resp))))\n"
)
{
    return http_srv::get(vv_args->_s(0), vv_args->_(1));
}
//---------------------------------------------------------------------------

void init_httplib(LuaThread *t, Lua::Instance &lua)
{
    VV obj(vv_list() << vv_ptr(t, "LuaThread"));

    LUA_REG(lua, "http", "bind",     obj, http_bind);
    LUA_REG(lua, "http", "free",     obj, http_free);
    LUA_REG(lua, "http", "response", obj, http_response);
    LUA_REG(lua, "http", "get",      obj, http_get);
}
//---------------------------------------------------------------------------

} // namespace lal_rt
