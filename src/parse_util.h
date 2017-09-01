// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#ifndef BUKALISP_UTIL_H
#define BUKALISP_UTIL_H 1
#include "utf8buffer.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

inline bool charClass(char c, const char *cs, bool bInvert = false)
{
    if (bInvert)
    {
        for (unsigned int i = 0; i < strlen(cs); i++)
            if (cs[i] != c) return true;
    }
    else
    {
        for (unsigned int i = 0; i < strlen(cs); i++)
            if (cs[i] == c) return true;
    }
    return false;
}
//---------------------------------------------------------------------------

inline bool u8BufParseNumber(UTF8Buffer &u8P, double &dVal, int64_t &iVal, bool &bIsDouble, bool &bBadNumber)
{
    try
    {
        if (!charClass(u8P.first_byte(), "+-0123456789"))
            return false;

        int iBase = 10;
        bool bNeg = u8P.first_byte() == '-';
        if (charClass(u8P.first_byte(), "+-"))
            u8P.skip_bytes(1);

        bIsDouble = false;

        UTF8Buffer u8Num;
        while (u8P.length() > 0 && charClass(u8P.first_byte(), "0123456789"))
            u8Num.append_byte(u8P.first_byte(true));

        if (u8P.length() > 0 && u8P.first_byte() == 'r')
        {
            u8P.skip_bytes(1);

            iBase = u8Num.length() > 0 ? stoi(u8Num.as_string()) : 10;
            u8Num.reset();

            while (u8P.length() > 0
                   && charClass(u8P.first_byte(),
                                "0123456789abcdefghijklmnopqrstuvwxyz"
                                "ABCDEFGHIJKLMNOPQRSTUVW"))
                u8Num.append_byte(u8P.first_byte(true));
        }

        while (u8P.length() > 0)
        {
            if (!charClass(u8P.first_byte(), "0123456789"))
                bIsDouble = true;

            u8Num.append_byte(u8P.first_byte(true));
        }

        if (bIsDouble)
        {
            if (iBase != 10) return bBadNumber = true;
            dVal = (bNeg ? -1LL : 1LL) * stod(u8Num.as_string());
        }
        else
        {
            if (iBase < 2 || iBase > 36) return bBadNumber = true;
            iVal = (bNeg ? -1LL : 1LL) * stoll(u8Num.as_string(), 0, iBase);
        }
    }
    catch (const std::exception &)
    {
        return bBadNumber = true;
    }

    return true;
}
//---------------------------------------------------------------------------

inline std::string printString(const std::string &sValue)
{
    std::string os;

    int iSize = sValue.size();
    os.reserve(iSize + 2);

    os.append(1, '"');
    for (int i = 0; i < iSize; i++)
    {
        if (sValue[i] == '"' || sValue[i] == '\\')
            os.append(1, '\\');
        os.append(1, sValue[i]);
    }
    os.append(1, '"');

    return os;
}
//---------------------------------------------------------------------------

} // namespace bukalisp

#endif // BUKALISP_UTIL_H

/******************************************************************************
* Copyright (C) 2016-2017 Weird Constructor
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

