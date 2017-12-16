// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include <string>
#include <chrono>

std::string slurp_str(const std::string &filepath);
bool write_str(const std::string &filepath, const std::string &data);

std::string from_wstring(const std::wstring &str);
std::wstring to_wstring(const std::string &str);
std::string application_dir_path();
bool file_exists(const std::string &filename);

class BenchmarkTimer
{
        std::chrono::high_resolution_clock::time_point m_t1;

    public:
        BenchmarkTimer()
            : m_t1(std::chrono::high_resolution_clock::now())
        {
        }

        double diff()
        {
            std::chrono::high_resolution_clock::time_point t2 =
                std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_span
                = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - m_t1);
            return time_span.count() * 1000.0;
        }
};

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
