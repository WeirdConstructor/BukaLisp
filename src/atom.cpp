#include "atom.h"

namespace lilvm
{
//---------------------------------------------------------------------------

size_t AtomVec::s_alloc_count = 0;

//---------------------------------------------------------------------------

void AtomVec::alloc(size_t len)
{
    m_alloc = len;
    m_len   = 0;
    m_data  = new Atom[len];
}
//---------------------------------------------------------------------------

void AtomVec::init(uint8_t current_gc_color, size_t len)
{
    m_len      = len;
    m_gc_color = current_gc_color;

    for (size_t i = 0; i < len; i++)
        m_data[i].clear();
}
//---------------------------------------------------------------------------

Atom AtomVec::at(size_t idx)
{
    if (idx > m_len)
        return Atom();
    return m_data[idx];
}
//---------------------------------------------------------------------------

Atom *AtomVec::first()
{
    if (m_len <= 0) return nullptr;
    return m_data;
}
//---------------------------------------------------------------------------

Atom *AtomVec::last()
{
    if (m_len <= 0) return nullptr;
    return &(m_data[m_len - 1]);
}
//---------------------------------------------------------------------------

void AtomVec::pop()
{
    if (m_len > 0) m_len--;
}
//---------------------------------------------------------------------------

void AtomVec::push(const Atom &a)
{
    size_t new_idx = m_len;
    check_size(new_idx);
    m_data[new_idx] = a;
}
//---------------------------------------------------------------------------

void AtomVec::check_size(size_t idx)
{
    if (idx >= m_alloc)
    {
        Atom *old_data = m_data;
        m_alloc = idx * 2;
        m_data = new Atom[m_alloc];
        for (size_t i = 0; i < m_len; i++)
            m_data[i] = old_data[i];
        delete[] old_data;
    }

    m_len = idx + 1;
}
//---------------------------------------------------------------------------

AtomVec::~AtomVec()
{
    s_alloc_count--;

    if (m_data)
        delete[] m_data;
}
//---------------------------------------------------------------------------

void AtomMap::set(Sym *s, Atom &a) { m_map[s->m_str] = a; }

void AtomMap::set(const std::string &str, Atom &a) { m_map[str] = a; }

//---------------------------------------------------------------------------

Atom AtomMap::at(Sym *s) { return at(s->m_str); }

Atom AtomMap::at(const std::string &str)
{
    auto it = m_map.find(str);
    if (it == m_map.end()) return Atom();
    return it->second;
}
//---------------------------------------------------------------------------

};
