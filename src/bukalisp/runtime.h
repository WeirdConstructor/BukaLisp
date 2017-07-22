#pragma once

#include "atom.h"
#include "atom_generator.h"
#include "parser.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

struct Runtime
{
    lilvm::GC                m_gc;
    bukalisp::AtomGenerator  m_ag;
    bukalisp::Tokenizer      m_tok;
    bukalisp::Parser         m_par;
    bukalisp::AtomDebugInfo  m_deb_info;

    Runtime()
        : m_ag(&m_gc, &m_deb_info),
          m_par(m_tok, &m_ag)
    {
    }

    lilvm::Atom read(const std::string &input_name, const std::string &input)
    {
        m_par.reset();
        m_tok.reset();
        m_tok.tokenize(input_name, input);
        lilvm::AtomVec *elems = m_gc.allocate_vector(0);
        while (!m_par.is_eof())
        {
            m_ag.start();
            if (!m_par.parse())
            {
                std::cerr << "ERROR while parsing " << input_name << std::endl;
                return lilvm::Atom();
            }
            if (!m_par.is_eof())
                elems->push(m_ag.root());
        }
        return lilvm::Atom(lilvm::T_VEC, elems);
    }

    std::string debug_info(lilvm::AtomVec *v) { return m_deb_info.pos((void *) v); }
    std::string debug_info(lilvm::AtomMap *m) { return m_deb_info.pos((void *) m); }
    std::string debug_info(lilvm::Atom &a)
    {
        switch (a.m_type)
        {
            case lilvm::T_VEC: return m_deb_info.pos((void *) a.m_d.vec); break;
            case lilvm::T_MAP: return m_deb_info.pos((void *) a.m_d.map); break;
            default: return "?:?";
        }
    }

    size_t pot_alive_vecs() { return m_gc.count_potentially_alive_vectors(); }
    size_t pot_alive_maps() { return m_gc.count_potentially_alive_maps(); }
    size_t pot_alive_syms() { return m_gc.count_potentially_alive_syms(); }

    void collect() { m_gc.collect(); }
};

//---------------------------------------------------------------------------

}
