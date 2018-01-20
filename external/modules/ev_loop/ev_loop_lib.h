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

//---------------------------------------------------------------------------

#include <cstdio>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <modules/bklisp_module_wrapper.h>

//---------------------------------------------------------------------------
template<typename T>
struct SlotList
{
    std::vector<T *>        m_slots;
    std::vector<size_t>     m_free_slots;

    size_t put(T *t)
    {
        if (m_free_slots.empty())
        {
            m_slots.push_back(t);
            return m_slots.size() - 1;
        }
        else
        {
            size_t slot = m_free_slots.back();
            m_slots[slot] = t;
            m_free_slots.pop_back();
            return slot;
        }
    }

    T *remove(size_t slot)
    {
        if (slot >= m_slots.size()) return nullptr;

        T *t = m_slots[slot];
        if (t)
        {
            m_slots[slot] = nullptr;
            m_free_slots.push_back(slot);
        }

        return t;
    }

    void remove_delete(size_t slot)
    {
        if (slot >= m_slots.size()) return;

        T *t = m_slots[slot];
        if (t)
        {
            m_slots[slot] = nullptr;
            m_free_slots.push_back(slot);
        }

        delete t;
    }

    ~SlotList()
    {
        for (auto t : m_slots)
            if (t) delete t;
    }
};
//---------------------------------------------------------------------------

class EventLoop
{
    private:
        boost::asio::io_service                 m_service;
        bool                                    m_ran_once;
        bool                                    m_running;
        SlotList<boost::asio::deadline_timer>   m_timers;
        bool                                    m_self_destruct;

    public:
        EventLoop()
            : m_ran_once(false), m_self_destruct(false),
              m_running(false)
        {
        }

        boost::asio::io_service *get_io_service() const { return &m_service; }

        void start_timer(VVal::VV timeduration, VVal::VV cb)
        {
            using namespace boost::asio;

            io_service *s = &m_service;
            deadline_timer *dt =
                new deadline_timer(
                    m_service,
                    boost::posix_time::milliseconds(
                        timeduration->i()));

            size_t slot_idx = m_timers.put(dt);

            dt->async_wait([=](const boost::system::error_code &ec) {
                cb->call(VVal::vv_list() << VVal::vv_bool(ec != 0));

                s->post([=]() { m_timers.remove_delete(slot_idx); });
            });
        }

        void post(const std::function<void()> &fun)
        {
            m_service.post(fun);
        }

        void run()
        {
            if (m_ran_once)
                m_service.reset();

            m_ran_once = true;
            m_running = true;
            m_service.run();
            m_running = false;

                std::cout << "X" << std::endl;
            if (m_self_destruct)
            {
                std::cout << "X2" << std::endl;
                delete this;
                std::cout << "X3" << std::endl;
            }
        }

        void stop()
        {
            if (m_running)
                m_service.stop();
        }

        void kill()
        {
                std::cout << m_running << "STOPPPING" << std::endl;
            if (m_running)
            {
                m_self_destruct = true;
                std::cout << "STOPPPING" << std::endl;
                m_service.stop();
                std::cout << "STOPPPING" << std::endl;
            }
            else
            {
                delete this;
            }
        }

        ~EventLoop()
        {
            std::cout << "DETREOYYY" << std::endl;
        }
};
//---------------------------------------------------------------------------

BukaLISPModule init_ev_loop_lib();

//---------------------------------------------------------------------------

