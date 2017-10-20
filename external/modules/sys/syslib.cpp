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

#include "syslib.h"
#include <modules/vval_util.h>
#include <boost/filesystem.hpp>
#include <Poco/Process.h>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <regex>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

using namespace boost::filesystem;
using namespace VVal;
using namespace Poco;

static std::string g_path(const VVal::VV &v)
{
    // TODO: Wait for mingw-w64 bugfix : https://sourceforge.net/p/mingw-w64/bugs/538/
    // std::string s = vv(boost::filesystem::path(v->w()).string())->s();
    std::string s = v->s(); // vv(boost::filesystem::path(v->w()).string())->s();
//    L_TRACE << "PATH" << v << "|" << v->w() << "=>" << s;
    return s;
}

//---------------------------------------------------------------------------

VV_CLOSURE_DOC(file_exists_Q,
"@sys procedure (sys-file-exists? _path-string_)\n\n"
"Returns `#true` if _path-string_ points to an existing file/directory.\n"
"\n"
"    (sys-file-exists? \"C:/Windows\") ;=> #true\n"
"    (sys-file-exists? \"C:/FoobarNotExists.txt\") ;=> #false\n"
)
{
    return vv_bool(exists(vv_args->_w(0)));
}
//---------------------------------------------------------------------------

static VVal::VV extract_directory_entry(const std::string &flags, const directory_entry &e)
{
    VVal::VV ent(vv_map());

    path p = e.path();

    boost::system::error_code ec;

    if (flags.find_first_of("a") != std::string::npos)
        ent->set("abs_path", vv(absolute(p).generic_string()));
    if (flags.find_first_of("c") != std::string::npos)
        ent->set("canonical_path", vv(canonical(p).generic_string()));
    if (flags.find_first_of("r") != std::string::npos)
        ent->set("relative_path", vv(relative(p).generic_string()));
    ent->set("path", vv(p.generic_string()));

    switch (e.status(ec).type())
    {
        case boost::filesystem::file_type::status_error:
            ent->set("type", vv("error")); break;
        case boost::filesystem::file_type::file_not_found:
            ent->set("type", vv("not_found")); break;
        case boost::filesystem::file_type::regular_file:
            ent->set("type", vv("regular")); break;
        case boost::filesystem::file_type::directory_file:
            ent->set("type", vv("directory")); break;
        case boost::filesystem::file_type::symlink_file:
            ent->set("type", vv("symlink")); break;
        case boost::filesystem::file_type::block_file:
            ent->set("type", vv("block")); break;
        case boost::filesystem::file_type::character_file:
            ent->set("type", vv("char")); break;
        case boost::filesystem::file_type::fifo_file:
            ent->set("type", vv("fifo")); break;
        case boost::filesystem::file_type::socket_file:
            ent->set("type", vv("socket")); break;
        case boost::filesystem::file_type::type_unknown:
        default:
            ent->set("type", vv("unknown")); break;
    }

    ent->set("size",  vv((int64_t) file_size(p, ec)));
    ent->set("perms", vv(e.status(ec).permissions()));
    ent->set("mtime", vv_dt(last_write_time(e.path(), ec)));

    return ent;
}
//---------------------------------------------------------------------------

static void process_entry(const std::list<std::regex> &regexes,
                          const std::string &flags,
                          const directory_entry &e,
                          const VV &list)
{
    path p = e.path();

    if (flags.find_first_of("d") != std::string::npos)
        if (!is_directory(p)) return;
    if (flags.find_first_of("D") != std::string::npos)
        if (is_directory(p)) return;
    if (flags.find_first_of("f") != std::string::npos)
        if (!is_regular_file(p)) return;
    if (flags.find_first_of("F") != std::string::npos)
        if (is_regular_file(p)) return;

    std::string ps;
    if (flags.find_first_of("a") != std::string::npos)
        ps = absolute(p).generic_string();
    else if (flags.find_first_of("c") != std::string::npos)
        ps = canonical(p).generic_string();
    else if (flags.find_first_of("r") != std::string::npos)
        ps = relative(p).generic_string();
    else // "g"
        ps = p.generic_string();

    bool match = regexes.empty();
    for (auto r : regexes)
    {
        if (std::regex_match(ps, r))
        {
            match = true;
            break;
        }
    }

    if (match)
    {
        if (flags.find_first_of("Y") != std::string::npos)
        {
            list << extract_directory_entry(flags, e);
        }
        else
            list << vv(ps);
    }
}
//---------------------------------------------------------------------------

static void make_regex_list_from_vv(const std::string &flags,
                                    VV relist,
                                    std::list<std::regex> &rl)
{
    bool case_insens = flags.find_first_of("i") != std::string::npos;

    if (relist->is_undef())
        return;

    for (auto r : *relist)
    {
        if (case_insens)
            rl.push_back(std::regex(r->s(), std::regex_constants::icase));
        else
            rl.push_back(std::regex(r->s()));
    }
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(find,
"@sys:rt-sys procedure (sys-find _path-string_ _flags-string_ _regex-or-regex-list_)\n"
"\n"
"This procedure searches (optionally recursively) through the directory at\n"
"_path-string_ and returns a list of filenames.\n"
"_flags-string_ may contain one of the following mode chars:\n"
"   \"R\"       - Search recursively.\n"
"   \"i\"       - Regex matching is done case insensitive.\n"
"   \"d\"       - Only directories.\n"
"   \"D\"       - Only non-directories.\n"
"   \"f\"       - Only regular files.\n"
"   \"F\"       - Anything but regular files.\n"
"\n"
"   \"a\"       - Returns absolute paths.\n"
"   \"c\"       - Returns canonical paths.\n"
"   \"r\"       - Returns relative paths.\n"
"   \"g\"       - Returns generic path.\n"
"   \"Y\"       - Returns a structure containing status information about the file.\n"
"                 (Can be combined with a, c, r)\n"
"   \"y\"       - The info structure that is returned with 'Y' contains also the symlink_status.\n"
"\n"
"_regex-or-regex-list_ may contain one or more regexes that the files may match.\n"
"Matching one is enough\n"
"\n"
"   (sys-find \".\" \"iR\" \"(.*)\\.exe\")\n"
)
{
    std::string flags = vv_args->_s(1);
    path p(g_path(vv_args->_(0)));

    if (exists(p))
    {
        if (!is_directory(p))
            return vv_undef();

        std::list<std::regex> rl;
        make_regex_list_from_vv(flags, vv_args->_(2), rl);

        VV list(vv_list());
        if (flags.find_first_of("R") != std::string::npos)
        {
            for (directory_entry &x : recursive_directory_iterator(p))
                process_entry(rl, flags, x, list);
        }
        else
        {
            for (directory_entry &x : directory_iterator(p))
                process_entry(rl, flags, x, list);
        }

        return list;
    }
    else
        return vv_undef();
}
//---------------------------------------------------------------------------

std::string read_all(std::istream &in)
{
    std::string all;
    char buf[4096];

    while (in.read(buf, sizeof(buf)))
        all.append(buf, sizeof(buf));

    all.append(buf, in.gcount());

    return all;
}
//---------------------------------------------------------------------------

VV_CLOSURE_DOC(exec_M,
"@sys procedure (sys-exec! _mode-string-or-keyword_ _prog-path_ _arg-list_)\n\n"
"Starts the executable _prog-path_ with the arguments _arg-list_.\n"
"The _mode_ decides, what is returned. Currently only the `simple:` mode\n"
"is implemented. And it returns a data structure, that contains the exit code,\n"
"and the complete output of the stderr and stdout streams.\n"
"If an error occured, it will be logged and `exit-status` will be `nil`.\n"
"\n"
"    (sys-exec! simple: \"./foo.exe\" [])\n"
"    ;=> { exit-status: 0 stderr: \"...\" stdout: \"...\" }\n"
)
{
    if (vv_args->_s(0) == "simple")
    {
        try
        {
            std::vector<std::string> args;
            for (auto a : *(vv_args->_(2)))
                args.push_back(a->s());

            Pipe stdout_pipe;
            Pipe stderr_pipe;

            ProcessHandle ph =
                Poco::Process::launch(
                    g_path(vv_args->_(1)), args, 0, &stdout_pipe, &stderr_pipe);

            PipeInputStream io_str(stdout_pipe);
            PipeInputStream ie_str(stderr_pipe);

            std::string out = read_all(io_str);
            std::string err = read_all(ie_str);

            int rc = ph.wait();

            return vv_map()
                << vv_kv("exit-code",   rc)
                << vv_kv("stderr",      err)
                << vv_kv("stdout",      out);
        }
        catch (const std::exception &e)
        {
//            L_ERROR << "sys-exit! (" << vv_args << ") Exception: " << e.what();
            return vv_map() << vv_kv("error", e.what());
        }
    }
    else
        return vv_undef();
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

BukaLISPModule init_syslib()
{
    VV reg(vv_list() << vv("sys"));
    VV obj(vv_map());
    VV mod(vv_list());
    reg << mod;

#define SET_FUNC(functionName, closureName) \
    mod << (vv_list() << vv(#functionName) << VVC_NEW_##closureName(obj))

    // Prepare boost::filesystem to return utf-8 encoded strings:
    std::locale old_locale = std::locale();
    std::locale new_locale(
        old_locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(new_locale);

    SET_FUNC(fileExists?, file_exists_Q);
    SET_FUNC(exec!,       exec_M);
    SET_FUNC(find,        find);

    return BukaLISPModule(reg);
}
//---------------------------------------------------------------------------
