// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#define REQ_PORT_ARG(procname, arg) \
    if ((arg).m_type != T_UD || (arg).m_d.ud->type() != "Port") \
        error("'" #procname "' requires a port object as argument", (arg)); \
    Port *p = static_cast<Port *>((arg).m_d.ud);


START_PRIM()
    REQ_EQ_ARGC(open-input-file, 1);
    Port *p = new Port(m_rt);
    try
    {
        p->open_input_file(A0.to_display_str(), true);
    }
    catch (...)
    {
        delete p;
        throw;
    }
    out.set_ud(p);
END_PRIM_DOC(open-input-file,
"@ports procedure (open-input-file _string_)\n\n"
"Opens the filename denoted by _string_ for textual output\n"
"and returns an port object. If the file could not be opened,\n"
"an exception is raised.\n"
"\n"
"    (let ((file-port (open-input-file \"test.bkl\")))\n"
"      (with-cleanup\n"
"        (close-port file-port)\n"
"        (read-all file-port))) ; reads a list of Bukalisp atoms from file-port\n")

START_PRIM()
    if (args.m_len == 0)
    {
        std::unique_ptr<Port> p = std::make_unique<Port>(m_rt);
        p->open_stdin();
        out = p->read();
        p->close();
    }
    else
    {
        REQ_EQ_ARGC(read, 1);
        REQ_PORT_ARG(read, A0);
        out = p->read();
    }
END_PRIM_DOC(read-all,
"@ports procedure (read-all)\n"
"@ports procedure (read-all _port_)\n\n"
"Reads a list of BukaLisp atoms from _port_ (or the `current-input-port`).\n");

START_PRIM()
    REQ_EQ_ARGC(close-port, 1);
    REQ_PORT_ARG(close-port, A0);
    p->close();
END_PRIM_DOC(close-port,
"@ports procedure (close-port _port_)\n\n"
"Closes input/output streams of _port_.\n");

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
