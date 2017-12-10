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

#include "httplib.h"
#include <modules/vval_util.h>
#include <modules/ev_loop/ev_loop_lib.h>
#include <Poco/URI.h>
#include "http.h"

using namespace VVal;
using namespace std;

//---------------------------------------------------------------------------

/* event loop module:

(define evl-handle (ev-loop-new))

(ev-loop-run evl-handle)
(ev-loop-poll evl-handle)
(ev-loop-wait evl-handle 100)
(ev-loop-quit evl-handle)

(ev-loop-on-next-idle evl-handle (lambda () nil))
(ev-loop-on-timeout evl-handle 1000 (lambda () nil))
(ev-loop-on-interval evl-handle 1000 (lambda () nil))

(ev-loop-free evl-handle)

*/

VV_CLOSURE_DOC(http_bind,
"@http procedure (http-bind _ev-loop-handle_ _port-number_ _request-cb_)\n\n"
"Binds a HTTP server to the TCP _port-number_.\n"
"The _request-cb_ is called once a request is received.\n"
"If there is an error, an exception will be thrown.\n"
"This procedure returns a handle for the newly created HTTP server.\n"
"This handle must be destroyed using `http-free`.\n"
"\n"
"_request-cb_ gets passed a request map, replying is handled by `http-response`:\n"
"\n"
"    (import (module ev-loop))\n"
"    (import (module http))\n"
"    (define event-loop (ev-loop-new))\n"
"    (define http-server nil)\n"
"    (with-cleanup\n"
"      (set! http-server\n"
"        (http-bind event-loop 18088\n"
"          (lambda (request)\n"
"            (http-response http-server (token: request)}\n"
"              {action: \"error\"\n"
"               status: 501\n"
"               reason: all-putt!:\n"
"               contenttype: \"text/plain\"\n"
"               data: \"Kaput!\"}))))\n"
"      (http-free http-server)\n"
"      (event-loop-run event-loop))\n"
)
{
    VVal::VV callback = vv_args->_(2);
    http_srv::Server *s = new http_srv::Server;
    EventLoop *evl =
        vv_args->_P<EventLoop>(0, "ev-loop:instance");
    s->setup(
        [=](const VVal::VV &req)
        {
            evl->post([=]()
            {
                callback->call(vv_list() << req);
            });
            return (int64_t) 1;
        });
    s->start((unsigned int) vv_args->_i(1));
    return vv_ptr((void *) s, "poco_http:server");
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
"\n"
"You can send headers that tell the browser not to cache:\n"
"\n"
"   {no-cache-flag: true}\n"
"\n"
"You can pass additional headers with this:\n"
"\n"
"    {cookies:  _map-of-cookie-names-and-values_}\n"
"\n\nFor an example see `http-bind`\n"
)
{
    http_srv::Server *s =
        vv_args->_P<http_srv::Server>(0, "poco_http:server");
    s->reply(vv_args->_i(1), vv_args->_(2));
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(http_free,
"@http procedure (http-free _server-handle_)\n"
"Frees the HTTP-Server handle.\n"
)
{
    http_srv::Server *s =
        vv_args->_P<http_srv::Server>(0, "poco_http:server");
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
"Please note, that this HTTP call is blocking and will\n"
"block the whole process.\n"
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

VV_CLOSURE_DOC(http_url_split,
"@http procedure (http-url-split _url_ [_resolve-relative-url_])\n"
"\n"
"Splits an URL up into it's parts.\n"
"If _resolve-relative-url_ is specified, it is resolved against\n"
"the base URL _url_. This procedure will return a map containing:\n"
"\n"
"   {\n"
"       path            \"...\"\n"
"       path-normalized \"...\"\n"
"       port            80\n"
"       host            \"...\"\n"
"       file-name       \"...\"\n"
"       segments            [...]\n"
"       segments-normalized [...]\n"
"       fragment        \"...\"\n"
"       password        \"...\"\n"
"       query           \"...\"\n"
"       query-decoded   \"...\"\n"
"       scheme          \"...\"\n"
"       userInfo        \"...\"\n"
"       userName        \"...\"\n"
"       params          [...]\n"
"   }\n"
)
{
    VV vv_url(vv_map());

    Poco::URI uri(vv_args->_s(0));

    if (vv_args->_(1)->is_defined())
        uri.resolve(vv_args->_s(1));

    VV vv_segs(vv_list());
    std::vector<std::string> segments;
    uri.getPathSegments(segments);
    for (auto &s : segments)
        vv_segs << vv(s);

    Poco::URI::QueryParameters qpm = uri.getQueryParameters();
    VV vv_params(vv_list());
    for (auto param : qpm)
        vv_params << (vv_list() << vv(param.first) << vv(param.second));

    vv_url
        << vv_kv("path",            uri.getPath())
        << vv_kv("port",            uri.getPort())
        << vv_kv("host",            uri.getHost())
        << vv_kv("fragment",        uri.getFragment())
        << vv_kv("file-name",       segments.size() > 0 ? segments.back() : "")
        << vv_kv("segments",        vv_segs)
        << vv_kv("user-info",       uri.getUserInfo())
        << vv_kv("scheme",          uri.getScheme())
        << vv_kv("query",           uri.getRawQuery())
        << vv_kv("query-decoded",   uri.getQuery())
        << vv_kv("params-decoded",  vv_params);

    uri.normalize();
    segments.clear();
    uri.getPathSegments(segments);
    vv_segs = vv_list();
    for (auto &s : segments)
        vv_segs << vv(s);

    vv_url
        << vv_kv("path-normalized",        uri.getPath())
        << vv_kv("segments-normalized",    vv_segs);

    return vv_url;
}
//---------------------------------------------------------------------------

BukaLISPModule init_httplib()
{
    VV reg(vv_list() << vv("http"));
    VV obj(vv_map());
    VV mod(vv_list());
    reg << mod;

#define SET_FUNC(functionName, closureName) \
    mod << (vv_list() << vv(#functionName) << VVC_NEW_##closureName(obj))

    http_srv::init_error_handlers();

    SET_FUNC(bind,          http_bind);
    SET_FUNC(free,          http_free);
    SET_FUNC(response,      http_response);
    SET_FUNC(get,           http_get);
    SET_FUNC(url-split,     http_url_split);

    return reg;
}
//---------------------------------------------------------------------------
