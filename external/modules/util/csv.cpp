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

#include "csv.h"
#include <regex>
#include <algorithm>

using namespace VVal;
using namespace std;

namespace VVal
{
namespace csv
{
//---------------------------------------------------------------------------

class CSVParser
{
    private:
        char    m_delim;
        string  m_row_sep;
        string  m_data;
        int     m_pos;

        int rest_len() { return (int) m_data.size() - m_pos; }
        void skip_char(int cnt = 1) { m_pos += cnt; }

        char next_char(bool do_skip = false)
        {
            if (rest_len() <= 0) return '\0';
            char c = m_data[m_pos];
            if (do_skip) skip_char();
            return c;
        }

        bool check_char(char c)
        {
            if (rest_len() < 1) return false;
            return next_char() == c;
        }

        bool check_substr(const string &chrs)
        {
            if (((size_t) rest_len()) < chrs.size()) return false;
            size_t i = 0;
            for (auto c : chrs)
                if (m_data[m_pos + i++] != c) return false;
            return true;
        }

        bool check_oneof(const string &chrs)
        {
            if (((size_t) rest_len()) < 1) return false;
            for (auto c : chrs)
                if (m_data[m_pos] == c) return true;
            return false;
        }

        bool parse_escaped_field()
        {
            string field_data;

            bool end_found = false;
            while (!end_found && rest_len() > 0)
            {
                if (check_char('"'))
                {
                    skip_char();

                    if (check_char('"'))
                    {
                        skip_char();
                        field_data += '"';
                    }
                    else
                        end_found = true;
                }
                else if (check_char('\x0d'))
                {
                    skip_char();
                    if (check_char('\x0a')) skip_char();
                    field_data += "\x0a";
                }
                else if (check_char('\x0a'))
                {
                    skip_char();
                    field_data += "\x0a";
                }
                else
                    field_data += next_char(true);
            }

            if (rest_len() <= 0) end_found = true;

            if (end_found) this->on_field(field_data);

            if (rest_len() <= 0 || check_substr(m_row_sep))
            {
                skip_char((int) m_row_sep.size());
                this->on_row_end();
            }
            else if (check_char(m_delim))
                skip_char();
            else
                return false; // kein korrekter feld-abschluss!


            return end_found;
        }

        bool parse_delimited_field()
        {
            string field_data;
            bool end_found = false;
            bool row_end   = false;

            while (!end_found && rest_len() > 0)
            {
                if (check_substr(m_row_sep))
                {
                    skip_char((int) m_row_sep.size());
                    end_found = true;
                    row_end   = true;
                }
                else if (check_char(m_delim))
                {
                    skip_char();
                    end_found = true;
                }
                else
                    field_data += next_char(true);
            }

            if (rest_len() <= 0)
            {
                end_found = true;
                row_end   = true;
            }

            if (end_found) this->on_field(field_data);
            if (row_end)   this->on_row_end();

            return end_found;
        }

        bool parse_field()
        {
            if (rest_len() < 1) return false;
            if (next_char() == '"')
            {
                skip_char();
                return parse_escaped_field();
            }
            else
            {
                return parse_delimited_field();
            }
        }

    public:
        CSVParser(char cDelim, const string &row_sep)
            : m_delim(cDelim), m_row_sep(row_sep)
        {
        }

        virtual ~CSVParser() { }

        virtual void on_field(const string &sData) = 0;
        virtual void on_row_end() = 0;

        void parse(const string &sData)
        {
            m_data = sData;
            m_pos  = 0;

            while (rest_len() > 0)
            {
                if (!parse_field())
                {
                    return;
                }
            }
        }
};
//---------------------------------------------------------------------------

class VVCSVParser : public CSVParser
{
    private:
        VV m_table;
        VV m_row;

    public:
        VVCSVParser(char delim, const string &row_sep)
            : CSVParser(delim, row_sep), m_table(vv_list()), m_row(vv_list())

        {
        }

        virtual ~VVCSVParser() {}
        virtual void on_field(const string &data)
        {
            m_row << data;
        }
        virtual void on_row_end()
        {
            m_table << m_row;
            m_row = vv_list();
        }

        VV table() { return m_table; }
};
//---------------------------------------------------------------------------

VVal::VV from_csv(const string &csv, char sep, const string &row_sep)
{
    VVCSVParser csvp(sep, row_sep);
    csvp.parse(csv);
    return csvp.table();
}
//---------------------------------------------------------------------------

string to_csv(const VVal::VV &table, char sep, const string &row_sep)
{
    regex re("[\t\r\n " + string(&sep, 1) + "]");

    stringstream ss;

    for (auto row : *table)
    {
        bool first_field = true;
        for (auto cell : *row)
        {
            if (!first_field) ss << sep;
            else first_field = false;

            string field = cell->s();

            smatch match;
            if (regex_search(field, match, re))
            {
                stringstream field_out;
                field_out << "\"";
                for (auto c : field)
                {
                    if (c == '"') field_out << "\"\"";
                    else          field_out << c;
                }
                field_out << "\"";
                field = field_out.str();

            }

            ss << field;
        }
        ss << row_sep;
    }

    return ss.str();
}
//---------------------------------------------------------------------------

} // namespace VVal::csv
} // namespace VVal
