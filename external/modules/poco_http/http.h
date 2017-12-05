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

#pragma once

#include <Poco/Net/HTTPServer.h>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <modules/vval.h>

namespace http_srv
{
//---------------------------------------------------------------------------

class Exception : public std::exception
{
    private:
        std::string m_msg;
    public:
        Exception(const std::string &msg) : m_msg(msg) { }
        virtual const char *what() const noexcept { return m_msg.c_str(); }
};
//---------------------------------------------------------------------------

class Server
{
    private:
        std::function<int64_t(const VVal::VV &)>                m_request_emitter;
        std::unordered_map<int64_t, std::promise<VVal::VV> *>   m_outstanding_requests;
        std::mutex                                              m_mutex;
        Poco::Net::HTTPServer                                  *m_http_srv;
        int64_t                                                 m_next_token;

    public:
        Server() : m_http_srv(0), m_next_token(0) { }
        ~Server();

        void setup(const std::function<int64_t(const VVal::VV &)> &emit)
        {
            m_request_emitter = emit;
        }

        void submit_request(const VVal::VV &req,
                            std::promise<VVal::VV> *response_promise);

        void reply(int64_t token, const VVal::VV &reply);

        void start(unsigned int port = 19099);
};
//---------------------------------------------------------------------------

VVal::VV get(const std::string &url, const VVal::VV &opts = VVal::vv_undef());
void init_ssl();
void shutdown_ssl();

void init_error_handlers();

//---------------------------------------------------------------------------

} // namespace http_srv

