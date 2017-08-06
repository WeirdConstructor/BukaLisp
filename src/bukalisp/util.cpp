#include <cstdio>
#include <iostream>
#include <fstream>
#include "bukalisp/buklivm.h"
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
