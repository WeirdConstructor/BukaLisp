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

    Runtime()
        : m_ag(&m_gc),
          m_par(m_tok, &m_ag)
    {
    }

    lilvm::Atom read(const std::string &input_name, const std::string &input, lilvm::AtomMap *&debug_map_output)
    {
        m_ag.clear_debug_info();
        m_par.reset();
        m_tok.reset();
        m_tok.tokenize(input_name, input);
        // m_tok.dump_tokens();
        lilvm::AtomVec *elems = m_gc.allocate_vector(0);
        while (!m_par.is_eof())
        {
            m_ag.start();
            if (!m_par.parse())
            {
                // TODO: Throw an exception!
                std::cerr << "ERROR while parsing " << input_name << std::endl;
                return lilvm::Atom();
            }
            if (!m_par.is_eof())
                elems->push(m_ag.root());
        }
        debug_map_output = m_ag.debug_info();
        m_ag.clear_debug_info();
        return lilvm::Atom(lilvm::T_VEC, elems);
    }

    size_t pot_alive_vecs() { return m_gc.count_potentially_alive_vectors(); }
    size_t pot_alive_maps() { return m_gc.count_potentially_alive_maps(); }
    size_t pot_alive_syms() { return m_gc.count_potentially_alive_syms(); }

    void collect() { m_gc.collect(); }
};

//---------------------------------------------------------------------------

}
