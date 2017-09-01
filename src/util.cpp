// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include <cstdio>
#include <iostream>
#include <fstream>
#include "buklivm.h"
#include "utf8buffer.h"
#include "util.h"

using namespace std;
using namespace bukalisp;

//---------------------------------------------------------------------------

UTF8Buffer *slurp(const std::string &filepath)
{
    ifstream input_file(filepath.c_str(),
                        ios::in | ios::binary | ios::ate);

    if (!input_file.is_open())
        throw bukalisp::BukLiVMException("Couldn't open '" + filepath + "'");

    size_t size = (size_t) input_file.tellg();

    // FIXME (maybe, but not yet)
    char *unneccesary_buffer_just_to_copy
        = new char[size];

    input_file.seekg(0, ios::beg);
    input_file.read(unneccesary_buffer_just_to_copy, size);
    input_file.close();

//        cout << "read(" << size << ")["
//             << unneccesary_buffer_just_to_copy << "]" << endl;

    UTF8Buffer *u8b =
        new UTF8Buffer(unneccesary_buffer_just_to_copy, size);
    delete[] unneccesary_buffer_just_to_copy;

    return u8b;
}
//---------------------------------------------------------------------------

std::string slurp_str(const std::string &filepath)
{
    ifstream input_file(filepath.c_str(),
                        ios::in | ios::binary | ios::ate);

    if (!input_file.is_open())
        throw bukalisp::BukLiVMException("Couldn't open '" + filepath + "'");

    size_t size = (size_t) input_file.tellg();

    // FIXME (maybe, but not yet)
    char *unneccesary_buffer_just_to_copy
        = new char[size];

    input_file.seekg(0, ios::beg);
    input_file.read(unneccesary_buffer_just_to_copy, size);
    input_file.close();

//        cout << "read(" << size << ")["
//             << unneccesary_buffer_just_to_copy << "]" << endl;

    std::string data(unneccesary_buffer_just_to_copy, size);
    delete[] unneccesary_buffer_just_to_copy;
    return data;
}
//---------------------------------------------------------------------------

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
