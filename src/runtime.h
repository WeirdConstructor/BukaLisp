// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#pragma once

#include "atom.h"
#include "atom_generator.h"
#include "parser.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

struct Runtime
{
    GC             m_gc;
    AtomGenerator  m_ag;
    Tokenizer      m_tok;
    Parser         m_par;

    Runtime()
        : m_ag(&m_gc),
          m_par(m_tok, &m_ag)
    {
    }

    Atom read(const std::string &input_name, const std::string &input)
    {
        m_par.reset();
        m_tok.reset();
        m_tok.tokenize(input_name, input);
        // m_tok.dump_tokens();
        AtomVec *elems = m_gc.allocate_vector(0);
        while (!m_par.is_eof())
        {
            m_ag.start();
            if (!m_par.parse())
            {
                // TODO: Throw an exception!
                std::cerr << "ERROR while parsing " << input_name << std::endl;
                return Atom();
            }
            if (!m_par.is_eof())
            {
                elems->push(m_ag.root());
//                std::cout << "ELEMS PUSH " << Atom(T_VEC, elems).at(1).meta().to_write_str() << std::endl;
            }
        }
        return Atom(T_VEC, elems);
    }

    size_t pot_alive_vecs() { return m_gc.count_potentially_alive_vectors(); }
    size_t pot_alive_maps() { return m_gc.count_potentially_alive_maps(); }
    size_t pot_alive_syms() { return m_gc.count_potentially_alive_syms(); }

    void collect() { m_gc.collect(); }
};

//---------------------------------------------------------------------------

}

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
