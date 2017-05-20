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

#ifndef UTF8BUFFER_H
#define UTF8BUFFER_H 1

#include <iostream>
#include <cstring>
#include "endian.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
extern "C"
{
#include <stdio.h>
//#include <stdlib.h>
//#include <mem.h>
}
#include "compat_stdint.h"
#include "compat_win.h"

//#define snprintf _snprintf

class UTF8Buffer_exception
{
    public:
        UTF8Buffer_exception () {}
};

/// @brief Ein dynamischer Zeichen-Puffer, welcher UTF-8 encodete Zeichen hält.
///
/// Er besitzt verschiedene Funktionen die hilfreich
/// zum Parsen von div. Text-Formaten sind.
class UTF8Buffer
{
    private:
        unsigned char *data;
        size_t len;
        size_t alloc;
        size_t offs;

    public:
        UTF8Buffer ()
            : data (0), len (0), alloc (0), offs (0)
        {
            alloc = 10;
            data = new unsigned char[alloc];
            len = 0;
            offs = 0;
        }

        UTF8Buffer (const char *str)
        {
            offs = 0;
            len = alloc = strlen (str);
            data = (unsigned char *) new char[alloc];
            memcpy (data, str, len);
        }

        UTF8Buffer (const char *in, size_t l)
        {
            offs = 0;
            len = alloc = l;
            data = (unsigned char *) new char[alloc];
            memcpy (data, in, len);
        }

        UTF8Buffer (const UTF8Buffer &chb)
        {
            offs = 0;
            len = alloc = chb.length ();
            data = (unsigned char *) new char[alloc];
            memcpy (data, chb.buffer (), chb.length ());
        }

        void reset()
        {
            alloc = 10;
            if (data) delete[] data;
            data  = new unsigned char[alloc];
            len   = 0;
            offs  = 0;
        }

        virtual ~UTF8Buffer ()
        {
            if (data)
                delete [] data;
        }

        UTF8Buffer& operator=(const UTF8Buffer &chb)
        {
            if (this == &chb)
                return *this;

            delete[] data;
            offs = 0;
            len = alloc = chb.length ();
            data = (unsigned char *) new char[alloc];
            memcpy (data, chb.buffer (), chb.length ());

            return *this;
        }

        bool operator==(const char *s) const
        {
            size_t strl = strlen (s);
            if (this->length () != strl)
                return false;

            for (size_t i = 0; i < this->length (); i++)
            {
                if (this->buffer()[i] != s[i])
                    return false;
            }

            return true;
        }
        bool operator==(const UTF8Buffer &s) const
        {
            if (this->length () != s.length ())
                return false;

            for (size_t i = 0; i < this->length (); i++)
            {
                if (this->buffer()[i] != s.buffer()[i])
                    return false;
            }

            return true;
        }

        size_t length () const { return len; }

        const char *buffer () const { return (char *) (data + offs); }

        size_t read_offset () const { return offs; }

        char *c_str () const
        {
            char *b = new char[this->length () + 1];
            memcpy (b, this->buffer (), this->length ());
            b[this->length ()] = '\0';
            return b;
        }

        void append_uint8(uint8_t i)
        { this->append_bytes((char *) &i, 1); }
        void append_uint16(uint16_t i)
        {
            endian::encode16Bit((char *) &i);
            this->append_bytes((char *) &i, 2);
        }
        void append_uint32(uint32_t i)
        {
            endian::encode32Bit((char *) &i);
            this->append_bytes((char *) &i, 4);
        }
        void append_uint64(uint64_t i)
        {
            endian::encode64Bit((char *) &i);
            this->append_bytes((char *) &i, 8);
        }
        void append_float(float f)
        {
            endian::encode32Bit((char *) &f);
            this->append_bytes((char *) &f, 4);
        }
        void append_double(double f)
        {
            endian::encode64Bit((char *) &f);
            this->append_bytes((char *) &f, 8);
        }

        bool read_uint8(uint8_t &iRes, bool bSkip = true)
        {
            if (this->length() < 1)
                return false;
            iRes = *((uint8_t *) this->buffer());
            if (bSkip)
                this->skip_bytes(1);
            return true;
        }

        bool read_uint16(uint16_t &iRes, bool bSkip = true)
        {
            if (this->length() < 2)
                return false;
            iRes = *((uint16_t *) this->buffer());
            endian::decode16Bit((char *) &iRes);
            if (bSkip)
                this->skip_bytes(2);
            return true;
        }

        bool read_uint32(uint32_t &iRes, bool bSkip = true)
        {
            if (this->length() < 4)
                return false;
            iRes = *((uint32_t *) this->buffer());
            endian::decode32Bit((char *) &iRes);
            if (bSkip)
                this->skip_bytes(4);
            return true;
        }

        bool read_uint64(uint64_t &iRes, bool bSkip = true)
        {
            if (this->length() < 8)
                return false;
            iRes = *((uint64_t *) this->buffer());
            endian::decode64Bit((char *) &iRes);
            if (bSkip)
                this->skip_bytes(8);
            return true;
        }

        bool read_float(float &fRes)
        {
            if (this->length() < 4)
                return false;
            fRes = *((float *) this->buffer());
            endian::decode32Bit((char *) &fRes);
            this->skip_bytes(4);
            return true;
        }

        bool read_double(double &fRes)
        {
            if (this->length() < 8)
                return false;
            fRes = *((double *) this->buffer());
            endian::decode64Bit((char *) &fRes);
            this->skip_bytes(8);
            return true;
        }

        void append_byte (const char c)
        { this->append_bytes(&c, 1); }

        void append_bytes (const UTF8Buffer &chb, size_t ilen = 0)
        {
            if (!ilen)
                ilen = chb.length ();
            this->append_bytes (chb.buffer (), ilen);
        }

        void append_bytes (const char *buffer, size_t ilen = 0)
        {
            if (alloc < (len + ilen))
            {
                if (len * 2 < (len + ilen))
                    alloc = (len + ilen) * 2;
                else
                    alloc = len * 2;

                unsigned char *buf = (unsigned char *) new char[alloc];
                if (data)
                {
                    memcpy (buf, data + offs, len);
                    delete[] data;
                }
                data = buf;
                offs = 0;
            }

            if (offs > 0)
            {
                memmove (data, data + offs, len);
                offs = 0;
            }

            memcpy (data + len, buffer, ilen);
            len += ilen;
        }

        bool first_is_ascii () const
        {
            if (len <= 0)
                return false;

            unsigned char *buf = data + offs;

            if (buf[0] <= 0x7F)
                return true;
            else
                return false;
        }

        // Char. number range  |        UTF-8 octet sequence
        //    (hexadecimal)    |              (binary)
        // --------------------+---------------------------------------------
        // 0000 0000-0000 007F | 0xxxxxxx
        // 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
        // 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
        // 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        //                     | 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        //                     | 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

        int length_of_first_char () const
        {
            if (len <= 0)
                return 0;

            unsigned char *buf = data + offs;

            // Attention: We assume, we have valid UTF-8 here.
            //            Sometimes this code will not give the correct
            //            length of invalid encoded UTF-8.
            if (len > 0 && buf[0] <= 0x7F)
                return 1;
            else if (len > 0 && (buf[0] & 0xE0) == 0xC0)
                return 2;
            else if (len > 2 && (buf[0] & 0xF0) == 0xE0)
                return 3;
            else if (len > 3 && (buf[0] & 0xF8) == 0xF0)
                return 4;
            else
                throw UTF8Buffer_exception ();
        }

        long first_char(bool bSkip = false)
        {
            if (len <= 0)
                return 0;

            unsigned char *buf = data + offs;

            long lUnicodepoint = 0;
            int iSkipLen = 1;

            if (buf[0] <= 0x7F)
            {
                iSkipLen = 1;
                lUnicodepoint = buf[0];
            }
            else if (len > 1
                     && (buf[0] & 0xE0) == 0xC0
                     && (buf[1] & 0xC0) == 0x80)
            {
                iSkipLen = 2;
                lUnicodepoint =
                        ((long) (buf[0] & 0x1F)) << 6
                      | ((long) (buf[1] & 0x3F));
            }
            else if (len > 2
                     && (buf[0] & 0xF0) == 0xE0
                     && (buf[1] & 0xC0) == 0x80
                     && (buf[2] & 0xC0) == 0x80)
            {
                iSkipLen = 3;
                lUnicodepoint =
                        ((long) (buf[0] & 0x0F)) << 12
                      | ((long) (buf[1] & 0x3F)) << 6
                      | ((long) (buf[2] & 0x3F));
            }
            else if (len > 3
                     && (buf[0] & 0xF8) == 0xF0
                     && (buf[1] & 0xC0) == 0x80
                     && (buf[2] & 0xC0) == 0x80
                     && (buf[3] & 0xC0) == 0x80)
            {
                iSkipLen = 4;
                lUnicodepoint =
                        ((long) (buf[0] & 0x07)) << 18
                      | ((long) (buf[1] & 0x3F)) << 12
                      | ((long) (buf[2] & 0x3F)) << 6
                      | ((long) (buf[3] & 0x3F));
            }

            if (bSkip)
                this->skip_bytes(iSkipLen);

            return lUnicodepoint;
        }

        void append_first_of (const UTF8Buffer &chb)
        {
            if (chb.length () > 0)
                this->append_bytes (chb, chb.length_of_first_char ());
        }

        void append_ascii (const char c)
        {
            this->append_bytes (&c, 1);
        }

        void append_ascii_as_hex (unsigned char c)
        {
            char buf[32];
            int l = snprintf (buf, 32, "%04x", c);
            this->append_bytes (buf, l);
        }

        void append_unicode (long uch)
        {
            int len = 0;
            char buf[4];
            if (uch <= 0x7F)
            {
                len = 1;
                buf[0] = (char) (uch & 0x7F);
            }
            else if (uch >=      0x80 && uch <=      0x7FF)
            {
                len = 2;
                buf[0] = (char) 0xC0;
                buf[1] = (char) 0x80;
                buf[0] |= (char) ((uch >> 6) & 0x1F);
                buf[1] |= (char) (uch & 0x3F);
            }
            else if (uch >=     0x800 && uch <=     0xFFFF)
            {
                len = 3;
                buf[0] = (char) 0xE0;
                buf[1] = (char) 0x80;
                buf[2] = (char) 0x80;
                buf[0] |= (char) ((uch >> 12) & 0x1F);
                buf[1] |= (char) ((uch >> 6)  & 0x3F);
                buf[2] |= (char) (uch         & 0x3F);
            }
            else if (uch >=   0x10000 && uch <=   0x1FFFFF)
            {
                len = 4;
                buf[0] = (char) 0xF0;
                buf[1] = (char) 0x80;
                buf[2] = (char) 0x80;
                buf[3] = (char) 0x80;
                buf[0] |= (char) ((uch >> 18) & 0x1F);
                buf[1] |= (char) ((uch >> 12) & 0x3F);
                buf[2] |= (char) ((uch >> 6)  & 0x3F);
                buf[3] |= (char) (uch         & 0x3F);
            }
            else
                throw UTF8Buffer_exception ();
            this->append_bytes (buf, len);
        }

        std::string hexdump ()
        {
            char *strbuf = new char[len * 3 + 1];
            unsigned char *buf = data + offs;
            unsigned int d = 0;
            for (size_t i = 0; i < len; i++)
                if (i == (len - 1))
                    d += snprintf (strbuf + d, 2, "%02X", buf[i]);
                else
                    d += snprintf (strbuf + d, 3, "%02X", buf[i]);

            std::string sOut(strbuf, d);
            delete[] strbuf;
            return sOut;
        }

        void skip_first ()
        {
            size_t l = this->length_of_first_char ();
            if (len > l)
            {
                offs += l;
                len  -= l;
            }
            else
            {
                len  = 0;
                offs = 0;
            }
        }

        char first_byte (bool skip = false)
        {
            if (this->length () <= 0)
                return '\0';
            char a = this->buffer ()[0];
            if (skip)
                this->skip_bytes (1);
            return a;
        }

        char first_ascii (bool skip = false)
        {
            if (this->length () <= 0 || !this->first_is_ascii ())
                return '\0';
            char a = this->buffer ()[0];
            if (skip)
                this->skip_first ();
            return a;
        }

        void skip_any_ascii (const char *str)
        {
            while (this->length () > 0 && this->first_is_ascii ())
            {
                bool match = false;
                char cur = this->first_ascii ();

                for (int i = 0; str[i]; i++)
                {
                    if (cur == str[i])
                    {
                        match = true;
                        break;
                    }
                }

                if (match)
                    this->skip_first ();
                else
                    break;
            }
        }

        void skip_json_ws ()
        {
            this->skip_any_ascii ("\x20\x9\xA\xD");
        }

        char next_json_token (bool skip = false)
        {
            this->skip_json_ws ();
            return this->first_ascii (skip);
        }

        void dump_as_json_string (UTF8Buffer &chb) const
        {
            UTF8Buffer buf(*this);

            chb.append_ascii ('"');

            while (buf.length () > 0)
            {
                if (buf.first_is_ascii ())
                {
                    unsigned char f = buf.first_ascii ();
                    if (!((f >= 0x20 && f <= 0x21)
                          || (f >= 0x23 && f <= 0x5B)
                          || (f >= 0x5D && f <= 0x7F)))
                    {
                        chb.append_ascii ('\\');
                        char esc;
                        switch (f)
                        {
                            case '"' :
                            case '\\':
                            case '/' :
                                esc = f; break;
                            case '\x08': esc = 'b'; break;
                            case '\x09': esc = 't'; break;
                            case '\x0C': esc = 'f'; break;
                            case '\x0A': esc = 'n'; break;
                            case '\x0D': esc = 'r'; break;
                            default:
                                         chb.append_ascii ('u');
                                         chb.append_ascii_as_hex (f);
                                         buf.skip_first ();
                                         continue;
                        }
                        chb.append_ascii (esc);
                        buf.skip_first ();
                        continue;
                    }
                }

                chb.append_first_of (buf);
                buf.skip_first ();
            }

            chb.append_ascii ('\x22');
        }

        size_t get_packet_length (size_t &pktoffs)
        {
            const char *b = this->buffer ();
            size_t i = 0;
            for (i = 0; i < this->length (); i++)
            {
                if (!(b[i] >= '0' && b[i] <= '9'))
                    break;
            }

            if (i == this->length ())
                return 0;

            if (!i)
                return 0;

            if (i > 255)
                return 0;

            char num[256];
            memcpy (num, b, i);
            num[i] = '\0';
            pktoffs = i + 1;
            return atoi (num);
        }

        void skip_bytes (size_t l)
        {
            if (len > l)
            {
                offs += l;
                len -= l;
            }
            else
            {
                offs = 0;
                len = 0;
            }
        }

        std::string as_string ()
        {
            return std::string (this->buffer (), this->length ());
        }
};

#endif // UTF8BUFFER_H
