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

#pragma once

#include <ctime>
#include <locale>
#include <iostream>
#include <sstream>
#include <cstring>
#include <iomanip>

//---------------------------------------------------------------------------

inline std::string format_datetime(std::time_t t, const std::string &format="")
{
    char buf[128];
    std::tm tm;
    std::memset(&tm, 0, sizeof(std::tm));
#ifdef __MINGW32__
    gmtime_s(&tm, &t);
#else
# ifdef _MSC_VER
    gmtime_s(&tm, &t);
# else
    gmtime_r(&t, &tm);
# endif
#endif

    size_t bufsize =
        std::strftime(buf, sizeof(buf), format.c_str(), &tm);
    return std::string(buf, bufsize);
}
//---------------------------------------------------------------------------

// By Eric S. Raymond
// See http://www.catb.org/esr/time-programming/
// struct tm to seconds since Unix epoch, ignoring timezone stuff
inline time_t my_timegm(std::tm *t)
{
    if (t->tm_year <= 0)
        return 0;

#   define MONTHSPERYEAR   12      /* months per calendar year */
    long year;
    std::time_t result = 0;
    static const int cumdays[MONTHSPERYEAR] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    year = 1900 + t->tm_year + t->tm_mon / MONTHSPERYEAR;
    result = (year - 1970) * 365 + cumdays[t->tm_mon % MONTHSPERYEAR];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    if ((year % 4) == 0
        && ((year % 100) != 0 || (year % 400) == 0)
        && (t->tm_mon % MONTHSPERYEAR) < 2)
        result--;
    result += t->tm_mday - 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    if (t->tm_isdst == 1)
        result -= 3600;
    return (result);
}
//---------------------------------------------------------------------------

inline std::time_t parse_datetime(const std::string &datetime, const std::string &format="")
{
    std::istringstream ss(datetime);
    ss.imbue(std::locale()); // I assume, classic "C" locale
    std::tm tm;
    std::memset(&tm, 0, sizeof(std::tm));
    tm.tm_isdst = 0;
    ss >> std::get_time(&tm, format.c_str());

    if (ss.fail())
        return std::time_t(0);
    return my_timegm(&tm);
}
//---------------------------------------------------------------------------

