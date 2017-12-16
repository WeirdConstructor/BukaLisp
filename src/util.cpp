// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include <codecvt>
#include "buklivm.h"
#include "utf8buffer.h"
#include "util.h"

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#else
extern "C"
{
#  include <unistd.h>
#  include <libgen.h>
}
#endif

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

bool write_str(const std::string &filepath, const std::string &data)
{
    std::string tmpoutpath = filepath + ".bkltmp.~";
    ofstream output_file(tmpoutpath.c_str(),
                         ios::out | ios::binary | ios::trunc);

    if (!output_file.is_open())
        throw bukalisp::BukLiVMException("Couldn't open '" + tmpoutpath + "'");

    output_file.write(data.data(), data.size());
    output_file.close();

    std::string bakoutpath = filepath + ".bklbak.~";

    remove(bakoutpath.c_str());

    if (file_exists(filepath))
    {
        if (rename(filepath.c_str(), bakoutpath.c_str()) != 0)
            return false;
    }

    if (rename(tmpoutpath.c_str(), filepath.c_str()) != 0)
    {
        rename(bakoutpath.c_str(), filepath.c_str());
        return false;
    }

    remove(bakoutpath.c_str());

    return true;
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

std::string from_wstring(const std::wstring &wstr)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16_converter;
    return utf8_utf16_converter.to_bytes(wstr);
}
//---------------------------------------------------------------------------

std::wstring to_wstring(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_utf16_converter;
    return utf8_utf16_converter.from_bytes(str);
}
//---------------------------------------------------------------------------

std::string application_dir_path()
{
#if defined(WIN32) || defined(_WIN32)
    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);

    wchar_t csDrive[_MAX_DRIVE];
    wchar_t csDir  [_MAX_DIR];
    wchar_t csName [_MAX_FNAME];
    wchar_t csExt  [_MAX_EXT];
    _wsplitpath(path,
                (wchar_t *) &csDrive,
                (wchar_t *) &csDir,
                (wchar_t *) &csName,
                (wchar_t *) &csExt);

    return from_wstring(std::wstring(csDrive) + std::wstring(csDir));
#else
    char result[PATH_MAX];
    auto s = readlink("/proc/self/exe", result, PATH_MAX);
    if (s <= 0) return "";
    char *dir = dirname(result);
    std::string file_path(dir, strlen(dir));
    return file_path;
#endif
}
//---------------------------------------------------------------------------

bool file_exists(const std::string &filename)
{
    std::ifstream infile(filename);
    return infile.good();
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
