// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "runtime.h"
#include "util.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

void Runtime::init_lib_dir_paths()
{
    m_library_dir_paths.push_back("." BKL_PATH_SEP "bukalisplib");

    const char *bukalisp_lib_path_env = std::getenv("BUKALISP_LIB");
    if (bukalisp_lib_path_env)
        m_library_dir_paths.push_back(bukalisp_lib_path_env);

    std::string exepath = application_dir_path();
    if (!exepath.empty())
        m_library_dir_paths.push_back(exepath);

    if (!exepath.empty())
        m_library_dir_paths.push_back(exepath + BKL_PATH_SEP + "bukalisplib");
}
//---------------------------------------------------------------------------

std::string Runtime::find_in_libdirs(const std::string &fileSubPath)
{
    for (auto fp : m_library_dir_paths)
    {
        std::string filePath = fp + BKL_PATH_SEP + fileSubPath;
        if (file_exists(filePath))
        {
            return filePath;
        }
    }

    return "";
}
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
