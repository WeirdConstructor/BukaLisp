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

#include "http.h"
//#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPCookie.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/Context.h>
#include <Poco/URI.h>
#include <Poco/ErrorHandler.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <regex>
#include <modules/vval_util.h>

using namespace std;
using namespace VVal;

namespace http_srv
{
//---------------------------------------------------------------------------

class BklErrHandler : public Poco::ErrorHandler
{
    public:
        virtual ~BklErrHandler() { }
        virtual void exception(const Poco::Exception &exc)
        {
            std::cout << "POCO Exception: "
                        << exc.what() << " / " << exc.displayText()
                        << std::endl;
        }

        virtual void exception(const std::exception &exc)
        {
            std::cout << "POCO Exception: " << exc.what() << std::endl;
        }

        virtual void exception()
        {
            std::cout << "POCO Unknown Exception in Poco!";
        }
};
//---------------------------------------------------------------------------

BklErrHandler g_ErrHandler;
void init_error_handlers()
{
    Poco::ErrorHandler::set(&g_ErrHandler);
}
//---------------------------------------------------------------------------

static std::string mimetype_from_path(const std::string &path)
{
    std::string mimetype = "text/plain";

    std::smatch string_match_capts;
    if (regex_match(path, string_match_capts, std::regex(".*\\.([^/\\.]+)$")))
    {
        std::string file_ext = string_match_capts[1];
        std::transform(
            file_ext.begin(),
            file_ext.end(),
            file_ext.begin(),
            ::tolower);

        if      (file_ext == "html") mimetype = "text/html; charset=UTF-8";
        else if (file_ext == "htm")  mimetype = "text/html; charset=UTF-8";
        else if (file_ext == "js")   mimetype = "text/javascript; charset=UTF-8";
        else if (file_ext == "json") mimetype = "application/json; charset=UTF-8";
        else if (file_ext == "css")  mimetype = "text/css";
        else if (file_ext == "txt")  mimetype = "text/plain; charset=UTF-8";
        else if (file_ext == "jpg")  mimetype = "image/jpeg";
        else if (file_ext == "jpeg") mimetype = "image/jpeg";
        else if (file_ext == "png")  mimetype = "image/png";
        else if (file_ext == "gif")  mimetype = "image/gif";
    }

    return mimetype;
}

//---------------------------------------------------------------------------

void Server::submit_request(const VV &req, promise<VV> *response_promise)
{
    lock_guard<mutex> lg(m_mutex);

    int64_t reqtoken = m_next_token++;
    req->set("token", vv(reqtoken));
    m_outstanding_requests[reqtoken] = response_promise;
    m_request_emitter(req);
}
//---------------------------------------------------------------------------

void Server::reply(int64_t token, const VV &reply)
{
    lock_guard<mutex> lg(m_mutex);

    if (m_outstanding_requests.find(token) == m_outstanding_requests.end())
    {
//        L_FATAL << "HTTP: Double reply to " << token << "(" << reply << ")";
        throw Exception("HTTP No such token! Replied two times to same request.");
    }

    // TODO: optimize map access!
    promise<VV> *p = m_outstanding_requests[token];
    m_outstanding_requests.erase(token);
    p->set_value(reply);
}
//---------------------------------------------------------------------------

class HTTP_SRV_Handler : public Poco::Net::HTTPRequestHandler
{
    private:
        Server  *m_srv;
    public:
        HTTP_SRV_Handler(Server *s) : m_srv(s) { }
        virtual ~HTTP_SRV_Handler() { }

        virtual void handleRequest(Poco::Net::HTTPServerRequest &request,
                                   Poco::Net::HTTPServerResponse &response)
        {
            std::promise<VV> response_promise;
            auto f = response_promise.get_future();

            std::ostringstream os;
            os << request.stream().rdbuf();

            VV params(vv_list());

            Poco::URI u(request.getURI());
            for (auto p : u.getQueryParameters())
                params << (vv_list() << vv(p.first) << vv(p.second));

            VV req(
                vv_map()
                << vv_kv("host",            request.getHost())
                << vv_kv("method",          request.getMethod())
                << vv_kv("content_type",    request.getContentType())
                << vv_kv("url",             request.getURI())
                << vv_kv("params",          params)
                << vv_kv("body",            os.str()));

//            L_DEBUG << "HTTP Request: " << req;
            m_srv->submit_request(req, &response_promise);

            f.wait();

            VV resp = f.get();
//            L_DEBUG << "HTTP Response: " << resp;

            if (resp->_s("action") == "file")
            {
                response.setStatusAndReason(
                    (Poco::Net::HTTPResponse::HTTPStatus) 200);
                string contenttype;
                if (resp->_("contenttype")->is_undef())
                    contenttype = mimetype_from_path(resp->_s("path"));
                else
                    contenttype = resp->_s("contenttype");

                response.sendFile(resp->_s("path"), contenttype);
            }
            else if (resp->_s("action") == "json")
            {
                response.setStatusAndReason(
                    (Poco::Net::HTTPResponse::HTTPStatus) 200);
                response.setContentType("application/json");
                string body = as_json(resp->_("data"));
                response.sendBuffer(body.c_str(), body.length());
            }
            else if (resp->_s("action") == "error")
            {
                if (resp->_("reason")->is_undef())
                    response.setStatusAndReason(
                        (Poco::Net::HTTPResponse::HTTPStatus) resp->_i("status"));
                else
                    response.setStatusAndReason(
                        (Poco::Net::HTTPResponse::HTTPStatus) resp->_i("status"),
                        resp->_s("reason"));

                response.setContentType(resp->_s("contenttype"));
                string body = resp->_s("data");
                response.sendBuffer(body.c_str(), body.length());
            }
            else
            {
                response.setStatusAndReason(
                    (Poco::Net::HTTPResponse::HTTPStatus) 200);
                response.setContentType(resp->_s("contenttype"));
                string body = resp->_s("data");
                response.sendBuffer(body.c_str(), body.length());
            }
        }
};
//---------------------------------------------------------------------------

class HTTP_SRV_Handler_Factory : public Poco::Net::HTTPRequestHandlerFactory
{
    private:
        Server  *m_srv;
    public:
        HTTP_SRV_Handler_Factory(Server *s) : m_srv(s) { }
        virtual ~HTTP_SRV_Handler_Factory() { }

        virtual Poco::Net::HTTPRequestHandler *createRequestHandler(
            const Poco::Net::HTTPServerRequest &request
        )
        {
            return new HTTP_SRV_Handler(m_srv);
        }
};
//---------------------------------------------------------------------------

void Server::start(unsigned int port)
{
    Poco::Net::HTTPRequestHandlerFactory::Ptr p(new HTTP_SRV_Handler_Factory(this));
    Poco::Net::HTTPServer *s = new Poco::Net::HTTPServer(p, port);
    s->start();

    m_http_srv = s;

//    L_DEBUG << "HTTP Server start on port: " << port;

    // TODO: Handle stopping?!
}
//---------------------------------------------------------------------------

Server::~Server()
{
    if (m_http_srv)
	{
//        L_DEBUG << "HTTP Server stop.";
        m_http_srv->stopAll(true);
        delete m_http_srv;
//        L_DEBUG << "HTTP Server deleted.";
	}
}
//---------------------------------------------------------------------------

Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> m_cert;
Poco::Net::Context::Ptr                               m_context;
std::mutex                                            m_ssl_mtx;

//---------------------------------------------------------------------------

void init_ssl()
{
    using namespace Poco;
    using namespace Poco::Net;

    if (m_cert)
        return;

    lock_guard<mutex> lg(m_ssl_mtx);
    m_cert = new AcceptCertificateHandler(false);
    m_context
        = new Context(Context::CLIENT_USE, "", "", "", Context::VERIFY_RELAXED,
                      9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    SSLManager::instance().initializeClient(0, m_cert, m_context);
}
//---------------------------------------------------------------------------

void shutdown_ssl()
{
    lock_guard<mutex> lg(m_ssl_mtx);
    m_cert    = Poco::SharedPtr<Poco::Net::InvalidCertificateHandler>();
    m_context = Poco::Net::Context::Ptr();
}
//---------------------------------------------------------------------------

VV get(const std::string &url, const VVal::VV &opts)
{
    using namespace Poco;
    using namespace Poco::Net;

    URI uri(url);
    string path(uri.getPathAndQuery());
    if (path.empty()) path = "/";

    bool need_mtx_unlock = false;

    HTTPClientSession *s = nullptr;
    if (m_cert && uri.getScheme() == "https")
    {
        m_ssl_mtx.lock();
        need_mtx_unlock = true;
        s = new HTTPSClientSession(uri.getHost(), uri.getPort(), m_context);
    }
    else
        s = new HTTPClientSession(uri.getHost(), uri.getPort());

    VV resp(vv_undef());
    try
    {
        HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);


        if (opts->_("cookies")->is_defined())
        {
            NameValueCollection nvc;
            for (auto cookie : *(opts->_("cookies")))
                nvc.add(cookie->_s(0), HTTPCookie::escape(cookie->_s(1)));
            request.setCookies(nvc);
        }

        if (opts->_("headers")->is_defined())
        {
            for (auto header : *(opts->_("headers")))
                request.set(header->_s(0), header->_s(1));
        }

        s->sendRequest(request);
        HTTPResponse response;

        std::istream &rs = s->receiveResponse(response);

        std::ostringstream os;
        os << rs.rdbuf();

        resp = vv_map()
            << vv_kv("content_type",    response.getContentType())
            << vv_kv("status",          (int64_t) response.getStatus())
            << vv_kv("reason",          response.getReason())
            << vv_kv("body",            os.str());

        if (opts->_("cookies")->is_defined())
        {
            std::vector<HTTPCookie> cookies;
            response.getCookies(cookies);
            VV cookieMap = vv_map();
            for (auto cookie : cookies)
            {
                cookieMap->set(
                    cookie.getName(),
                    vv(HTTPCookie::unescape(cookie.getValue())));
            }

            resp->set("cookies", cookieMap);
        }

        if (opts->_("headers")->is_defined())
        {
            VV headerMap = vv_map();
            for (auto header : response)
                headerMap->set(header.first, vv(header.second));
            resp->set("headers", headerMap);
        }

        if (response.getContentType() == "application/json")
            resp->set("data", from_json(resp->_s("body")));

        if (need_mtx_unlock) m_ssl_mtx.unlock();
        delete s;
    }
    catch (const std::exception &)
    {
        if (need_mtx_unlock) m_ssl_mtx.unlock();
        delete s;
//        L_ERROR << "HTTP-Get: Exception: " << e.what();
        return vv_undef();
    }

    return resp;
}
//---------------------------------------------------------------------------

} // namespace http_srv
