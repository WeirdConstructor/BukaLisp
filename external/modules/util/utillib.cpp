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

#include "utillib.h"
#include "csv.h"
#include <modules/vval_util.h>
#include <boost/locale/encoding.hpp>
#if USE_STD_REGEX
#   include <regex>
#   define RE_PREFIX std
#else
#   include <boost/regex.hpp>
#   define RE_PREFIX boost
#endif

using namespace VVal;
using namespace std;

VV_CLOSURE_DOC(util_from_csv,
"@util procedure (util-from-csv _csv-string_ _field-sep_ _row-sep_)\n"
"@util procedure (util-from-csv _csv-string_ _field-sep_)\n"
"@util procedure (util-from-csv _csv-string_)\n\n"
"Returns a table like data structure that contains the contents of\n"
"the CSV formatted _csv-string_.\n"
"_field-sep_ contains the field separator, usually something like `,` or `;` (default).\n"
"_row-sep_ contains the row/field set separator,\n"
"usually something like `\"\\r\\n\"` (default).\n"
"\n"
"    (util-from-csv \"a,b,c\\r\\nd,e,f\" \",\" \"\\r\\n\")\n"
"    ;=> [[\"a\",\"b\",\"c\"],[\"d\",\"e\",\"f\"]]\n"
)
{
    string sep     = vv_args->_s(1);
    string row_sep = vv_args->_s(2);
    if (sep.empty()) sep = ",";
    if (row_sep.empty()) row_sep = "\r\n";
    return VVal::csv::from_csv(vv_args->_s(0), sep[0], row_sep);
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(util_to_csv,
"@util procedure (util-to-csv _data_ _field-sep_ _row-sep_)\n"
"@util procedure (util-to-csv _data_ _field-sep_)\n"
"@util procedure (util-to-csv _data_)\n\n"
"Serializes the table data structure _data_ as CSV.\n"
"_field-sep_ contains the field separator, usually something like `,` or `;` (default).\n"
"_row-sep_ contains the row/field set separator,\n"
"usually something like `\"\\r\\n\"` (default).\n"
"\n"
"    (util-from-csv \"a,b,c\\r\\nd,e,f\" \",\" \"\\r\\n\")\n"
"    ;=> [[\"a\",\"b\",\"c\"],[\"d\",\"e\",\"f\"]]\n"
)
{
    string sep     = vv_args->_s(1);
    string row_sep = vv_args->_s(2);
    if (sep.empty()) sep = ",";
    if (row_sep.empty()) row_sep = "\r\n";
    return vv(VVal::csv::to_csv(vv_args->_(0), sep[0], row_sep));
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(util_to_json,
"@util procedure (util-to-json _data_ _do-indent-bool_)\n"
"@util procedure (util-to-json _data_)\n\n"
"Returns the _data_ structure as JSON and UTF-8 encoded string.\n"
"If the optional parameter _do-indent-bool_ is true, the output will be pretty printed.\n"
)
{
    return vv(VVal::as_json(vv_args->_(0), vv_args->_b(1)));
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(util_from_json,
"@util procedure (util-from-json _string_)\n\n"
"Interpretes _string_ as UTF-8 encoded JSON.\n"
)
{
    return VVal::from_json(vv_args->_s(0));
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(util_to_utf8,
"@util procedure (util-to-utf8 _string-or-bytes_ _source-encoding-name_)\n\n"
"Reencodes the character set of _string-or-bytes_ by interpreting it as\n"
"if it is encoded in _source-encoding-name_.\n"
"Returns an UTF8 encoded text string.\n"
"\n"
"Possible _encoding-names_:\n"
"    - Latin1\n"
"    - ISO-8859-8\n"
"    - UTF-16\n"
"    - UTF-16BE\n"
"    - UTF-16LE\n"
"    - UTF-32\n"
"    - UTF-32BE\n"
"    - UTF-32LE\n"
"    - CP1250 / WINDOWS-1250\n"
"    - 850 / CP850\n"
"And many others that are supported by boost::local (iconv/icu)\n"
)
{
    return vv(boost::locale::conv::to_utf<char>(vv_args->_s(0), vv_args->_s(1)));
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(util_from_utf8,
"@util:bklisp-util procedure (util-from-utf8 _string_ _dest-encoding-name_)\n\n"
"Decodes the UTF8 _string_ into the _dest-encoding-name_\n"
"Returns the bytes of the result.\n"
"See also `util-to-utf8`.\n"
)
{
    return vv_bytes(boost::locale::conv::from_utf<char>(vv_args->_s(0), vv_args->_s(1)));
}
//---------------------------------------------------------------------------

struct regex_matcher
{
    std::string m_str;
    RE_PREFIX::regex *m_r;
    bool        m_keep_first_submatch;
    bool        m_match_whole_string;
    bool        m_global_match;
    bool        m_with_token;
    std::string m_token;
    bool        m_split;
    regex_matcher()
        : m_r(nullptr),
          m_keep_first_submatch(false),
          m_match_whole_string(false),
          m_global_match(false),
          m_with_token(false),
          m_split(false) {}

    ~regex_matcher()
    {
        if (m_r) delete m_r;
    }

    void new_regex(const std::string &s, RE_PREFIX::regex::flag_type flags)
    {
        m_str = s;
        m_r = new RE_PREFIX::regex(m_str, flags);
    }


    bool apply_regex_replace(const VV &str, VV &results)
    {
        std::string in(str->_s(0));
        std::string repl(str->_s(1));
        VV s = vv(RE_PREFIX::regex_replace(in, *m_r, repl));

        if (m_with_token)
        {
            results = vv_list();
            if (m_with_token) results << vv(m_token);
            results << s;
        }
        else
            results = s;

        return true;
    }

    bool apply_regex_global(const VV &str, VV &results)
    {
        std::string in(str->s());
        RE_PREFIX::regex_iterator<std::string::iterator> rit (in.begin(), in.end(), *m_r);
        RE_PREFIX::regex_iterator<std::string::iterator> rend;
        if (rit == rend)
            return false;

        results = vv_list();
        if (m_with_token) results << vv(m_token);
        while (rit != rend)
        {
            int start_idx = (m_keep_first_submatch ? 0 : 1);

            if (flat_submatch_result())
                results << vv((*rit)[start_idx].str());
            else
            {
                VV sub(vv_list());
                for (size_t i = start_idx; i < rit->size(); i++)
                    sub << vv((*rit)[i].str());
                results << sub;
            }
            ++rit;
        }

        return true;
    }

    bool apply_regex_split(const VV &str, VV &results)
    {
        std::string in(str->s());
        RE_PREFIX::sregex_token_iterator rit(in.begin(), in.end(), *m_r, -1);
        RE_PREFIX::sregex_token_iterator rend;
        if (rit == rend)
        {
            results = vv_list();
            if (m_with_token) results << vv(m_token);
            results << vv(in);
            return true;
        }

        results = vv_list();
        if (m_with_token) results << vv(m_token);
        while (rit != rend)
        {
            results << vv(*rit);
            ++rit;
        }

        return true;
    }

    bool apply_regex_match(const VV &str, VV &results)
    {
        bool b = false;
        std::string in(str->s());
        RE_PREFIX::smatch m;
        if (m_match_whole_string)
            b = RE_PREFIX::regex_match(in, m, *m_r);
        else
            b = RE_PREFIX::regex_search(in, m, *m_r);

        if (b)
        {
            int start_idx = (m_keep_first_submatch ? 0 : 1);

            if (!m_with_token && flat_submatch_result())
            {
                results = vv(m[start_idx].str());
            }
            else
            {
                results = vv_list();
                if (m_with_token) results << vv(m_token);
                for (size_t i = start_idx; i < m.size(); i++)
                {
                    results << vv(m[i].str());
                }
            }
        }
        return b;
    }

    bool operator()(const VV &str, VV &results)
    {
        if (str->is_list())      return apply_regex_replace(str, results);
        else if (m_global_match) return apply_regex_global(str, results);
        else if (m_split)        return apply_regex_split(str, results);
        else                     return apply_regex_match(str, results);
    }

    bool flat_submatch_result() const
    {
        return
            (m_keep_first_submatch && m_r->mark_count() == 0)
            || (!m_keep_first_submatch && m_r->mark_count() == 1);
    }
};
//---------------------------------------------------------------------------

static regex_matcher *build_single_regex_matcher(bool with_token, const VV &desc)
{
    RE_PREFIX::regex::flag_type flags = RE_PREFIX::regex::ECMAScript;
    regex_matcher *o = new regex_matcher;

    if (desc->is_list() && desc->_(1)->is_string())
    {
        std::string f = desc->_s(1);
        if (f.find_first_of("0") != std::string::npos) flags |= RE_PREFIX::regex::ECMAScript;
        if (f.find_first_of("1") != std::string::npos) flags |= RE_PREFIX::regex::extended;
        if (f.find_first_of("2") != std::string::npos) flags |= RE_PREFIX::regex::awk;
        if (f.find_first_of("3") != std::string::npos) flags |= RE_PREFIX::regex::grep;
        if (f.find_first_of("4") != std::string::npos) flags |= RE_PREFIX::regex::egrep;
        if (f.find_first_of("5") != std::string::npos) flags |= RE_PREFIX::regex::basic;
#if !USE_STD_REGEX
        if (f.find_first_of("6") != std::string::npos) flags |= RE_PREFIX::regex::perl;
#endif
        if (f.find_first_of("i") != std::string::npos) flags |= RE_PREFIX::regex::icase;
        if (f.find_first_of("o") != std::string::npos) flags |= RE_PREFIX::regex::optimize;
        if (f.find_first_of("c") != std::string::npos) flags |= RE_PREFIX::regex::collate;
        if (f.find_first_of("k") != std::string::npos) o->m_keep_first_submatch = true;
        if (f.find_first_of("a") != std::string::npos) o->m_match_whole_string  = true;
        if (f.find_first_of("g") != std::string::npos) o->m_global_match        = true;
        if (f.find_first_of("s") != std::string::npos) o->m_split               = true;
    }
    else
    {
        flags = RE_PREFIX::regex::ECMAScript;
    }

//    L_TRACE << "MKRE[" << (desc->is_list() ? desc->_s(0) : desc->s()) << "]";
    o->new_regex(desc->is_list() ? desc->_s(0) : desc->s(), flags);

    o->m_with_token = with_token;
    o->m_token      = desc->_s(2);

    return o;
}
//---------------------------------------------------------------------------

static void build_regex_from_desc(const VV &desc, std::list<regex_matcher *> &regexes)
{
    if (!desc->_(0)->is_list())
    {
        regexes.push_back(
            build_single_regex_matcher(desc->_(2)->is_defined(), desc));
    }
    else
    {
        bool with_token = false;
        for (auto i : *desc)
        {
            if (i->is_list() && i->_(2)->is_defined())
            {
                with_token = true;
                break;
            }
        }

        for (auto i : *desc)
            regexes.push_back(build_single_regex_matcher(with_token, i));
    }
}

//---------------------------------------------------------------------------

VV_CLOSURE_DOC(util_re,
"@util procedure (util-re _strings_ _regexes_ _return-format-mode-str_)\n\n"
"Matches _strings_ using _regexes_. Can also be just a single string and\n"
"a single regex. Regex format is ECMAScript standard like C++11 uses it.\n"
"\n"
"_strings_ may look like in these examples:\n"
"   _strings_ = _string_\n"
"   _strings_ = [_string_, ...]\n"
"   _strings_ = [[_string_, _replacement_], ...]\n"
"_regexes_ may look like in these examples:\n"
"   _regexes_ = _regex_\n"
"   _regexes_ = [_regex_, _flags_, _match-token_]\n"
"   _regexes_ = [[_regex_, _flags_, _match-token_], ...]\n"
"Where _flags_ and _match-token_ are optional.\n"
"_match-token_ contains some value, that is used to identify the regex that was\n"
"matched.\n"
"_flags_ may be one or multiple of:\n"
"   _flags_:\n"
"       \"i\"   - case insensitive match\n"
"       \"o\"   - optimize for matching fast\n"
"       \"c\"   - collate according to current locale\n"
"       \"w\"   - widestring match (for UTF-8 input).\n"
"       \"k\"   - keep first submatch (the string itself) in the output.\n"
"       \"0\"   - Regex Grammar: ECMAScript (default)\n"
"       \"1\"   - Regex Grammar: Extended POSIX\n"
"       \"2\"   - Regex Grammar: Awk POSIX\n"
"       \"3\"   - Regex Grammar: Grep POSIX\n"
"       \"4\"   - Regex Grammar: Egrep POSIX\n"
"       \"5\"   - Regex Grammar: Basic POSIX\n"
"       \"6\"   - Regex Grammar: Perl Regex syntax (only with boost::regex)\n"
"       \"a\"   - will match the whole string, instead of just a part of it\n"
"                 against the regex.\n"
"       \"g\"   - globally match the pattern multiple times.\n"
"                 (does not work with replacements.)\n"
"       \"s\"   - split up the string using this regex.\n"
"                 (does not work with replacements.)\n"
//"       \"r\"   - required match (this makes matching this regex\n"
//"                 a requirement for matching any string.)\n"
//"       \"R\"   - must not match (no string matches, that matches\n"
//"                 this regex.)\n"
"_replacement_ may contain:\n"
"       $n      - backreference\n"
"       $&      - entire match\n"
"       $`      - prefix\n"
"       $´      - suffix\n"
"_return-format-mode-str_ may contain:\n"
"       \"n\"   - string index-numbers\n"
"\n"
"Examples:\n"
"\n"
"  \n; Searching for a match inside a string:\n"
"    (util-re \"foobar\" \"(o+)\")  ;=> \"oo\"\n"
"  \n; Matching a complete string:\n"
"    (util-re \"foobar\" [\"f(o+)bar\" \"a\"]) ;=> \"oo\"\n"
"    ; or:\n"
"    (util-re \"foobar\" \"^f(o+)bar$\") ;=> \"oo\"\n"
"  \n; Multiple sub-matches:\n"
"    (util-re \"foobar\" \"(o+).*(a+)\")  ;=> (\"oo\" \"a\")\n"
"  \n; Global match:\n"
"    (util-re \"foobarfoobfbffoooorer\" [\"f(o+)\" \"g\"])  ;=> (\"oo\" \"oo\" \"oooo\")\n"
"    (util-re \"foobarfoobfbffoooorer\" [\"(f(o+))\" \"g\"])\n"
"    ;=> ((\"foo\" \"oo\") (\"foo\" \"oo\") (\"foooo\" \"oooo\"))\n"
"  \n; Split string:\n"
"    (util-re \"foo, bar ,  baz,boo\" [\"\\s*,\\s*\" \"s\"])  ;=> (\"foo\" \"bar\" \"baz\" \"boo\")\n"
)
{
    std::list<regex_matcher *> matchers;

    std::string ret_mode = vv_args->_s(2);
    bool with_string_idxs = ret_mode.find_first_of("n") != std::string::npos;

    build_regex_from_desc(vv_args->_(1), matchers);

    VV ret = vv_list();
    int string_idx = 0;
    for (auto s : *(vv_args->_(0)))
    {
        for (auto m : matchers)
        {
            VV results;
            if ((*m)(s, results))
            {
                if (with_string_idxs)
                {
                    if (!results->is_list())
                        results = vv_list() << results;
                    results->unshift(vv(string_idx));
                }
                ret << results;
            }
        }
        string_idx++;
    }

    if (ret->size() == 0)
        ret = vv_undef();
    else if (matchers.size() == 1 && string_idx <= 1)
        ret = ret->_(0);

    for (auto m : matchers) delete m;

    return ret;
}
//---------------------------------------------------------------------------

VV_CLOSURE(util_init)
{
    cout << "INIT UTIL LIB" << endl;
    return vv_undef();
}
//---------------------------------------------------------------------------

VV_CLOSURE(util_destroy)
{
    cout << "DESTROYED UTIL LIB" << endl;
    return vv_undef();
}
//---------------------------------------------------------------------------

BukaLISPModule init_utillib()
{
    VV reg(vv_list() << vv("util"));
    VV obj(vv_map());
    VV mod(vv_list());
    reg << mod;

#define SET_FUNC(functionName, closureName) \
    mod << (vv_list() << vv(#functionName) << VVC_NEW_##closureName(obj))

    SET_FUNC(__INIT__,    util_init);
    SET_FUNC(__DESTROY__, util_destroy);

    SET_FUNC(fromCsv,     util_from_csv);
    SET_FUNC(toCsv,       util_to_csv);
    SET_FUNC(toUtf8,      util_to_utf8);
    SET_FUNC(fromUtf8,    util_from_utf8);
    SET_FUNC(toJson,      util_to_json);
    SET_FUNC(fromJson,    util_from_json);
    SET_FUNC(re      ,    util_re);

    return BukaLISPModule(reg);
}
//---------------------------------------------------------------------------
