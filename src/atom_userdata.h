#pragma once
#include <string>

namespace lilvm
{
//---------------------------------------------------------------------------
class GC;

class UserData
{
    public:
        uint8_t   m_gc_color;
        UserData *m_gc_next;

        UserData()
            : m_gc_color(), m_gc_next(nullptr)
        {
        }

        virtual std::string type() { return "unknown"; }

        virtual std::string as_string()
        {
            return std::string("#<userdata:unknown>");
        }
        virtual void mark(GC *gc, uint8_t clr) { m_gc_color = clr; }

        virtual ~UserData()
        {
        }
};
//---------------------------------------------------------------------------

}

