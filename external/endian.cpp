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

#include "endian.h"
#include "compat_stdint.h"
#if defined(__BORLANDC__) && (__BORLANDC__ <= 0x0593) || defined(__MINGW32__) || defined(_MSC_VER)
extern "C"
{
#include <winsock2.h>
}
#else
extern "C"
{
#if not defined _MSC_VER
#include <arpa/inet.h>
#include <string.h>
#endif
}
#endif

namespace endian
{

bool isBigEndian() { uint32_t i = 1; return *(char *)&i == 0; }

void encode16Bit(char *x)
{ *((uint16_t *) x) = htons(*((uint16_t *) x)); }
void decode16Bit(char *x)
{ *((uint16_t *) x) = ntohs(*((uint16_t *) x)); }

void encode32Bit(char *x)
{ *((uint32_t *) x) = htonl(*((uint32_t *) x)); }
void decode32Bit(char *x)
{ *((uint32_t *) x) = ntohl(*((uint32_t *) x)); }

void encode64Bit(char *x)
{
    if (isBigEndian()) return;

    char out[8];

    encode32Bit(x);
    encode32Bit(x + 4);

    memcpy(out,     x + 4, 4);
    memcpy(out + 4, x,     4);
    memcpy(x, out, 8);
}
void decode64Bit(char *x)
{
    if (isBigEndian()) return;

    char out[8];

    decode32Bit(x);
    decode32Bit(x + 4);

    memcpy(out,     x + 4, 4);
    memcpy(out + 4, x,     4);
    memcpy(x, out, 8);
}

}
