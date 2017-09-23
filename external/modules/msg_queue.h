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

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "optional.h"

template<typename MSGTYPE>
class MsgQueue
{
    private:
        std::mutex              m_mutex;
        std::queue<MSGTYPE>     m_queue;
        std::condition_variable m_cv;
        std::function<void()>   m_notifier;

    public:
        MsgQueue() { }
        MsgQueue(std::function<void()> f) : m_notifier(f) { }
        virtual ~MsgQueue() { }

        void push(const MSGTYPE &msg)
        {
            {
                std::lock_guard<std::mutex> lg(m_mutex);
                m_queue.push(msg);
            }

            if (m_notifier)
                m_notifier();
            m_cv.notify_one();
        }

        Optional<MSGTYPE> pop_now()
        {
            std::lock_guard<std::mutex> lg(m_mutex);
            if (m_queue.empty())
                return Optional<MSGTYPE>();

            MSGTYPE msg = m_queue.front();
            m_queue.pop();
            return Optional<MSGTYPE>(msg);
        }

        MSGTYPE pop_blocking()
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this]{ return !m_queue.empty(); });
            MSGTYPE msg = m_queue.front();
            m_queue.pop();
            return msg;
        }

        Optional<MSGTYPE> pop_waiting(uint64_t wait_ms)
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            if (!m_cv.wait_for(lk, std::chrono::milliseconds(wait_ms),
                              [this]{ return !m_queue.empty(); }))
                return Optional<MSGTYPE>();

            MSGTYPE msg = m_queue.front();
            m_queue.pop();
            return Optional<MSGTYPE>(msg);
        }

        bool empty()
        {
            std::lock_guard<std::mutex> lg(m_mutex);
            return m_queue.empty();
        }

        void clear()
        {
            std::lock_guard<std::mutex> lg(m_mutex);
            while (!m_queue.empty())
                m_queue.pop();
        }
};
