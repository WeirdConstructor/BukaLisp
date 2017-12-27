// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#include "bukalisp.h"
#include "util.h"

namespace bukalisp
{
//---------------------------------------------------------------------------

void Instance::load_bootstrapped_compiler_from_disk()
{
    m_i.cleanup_you_are_unused_now();

    std::string bootstrapped_compiler_filepath =
        m_rt.find_in_libdirs("compiler.bklc");

    BenchmarkTimer timer_comp_comp;

    Atom compiler_serialized_from_disk =
        m_rt.read(bootstrapped_compiler_filepath,
                  slurp_str(bootstrapped_compiler_filepath));
    compiler_serialized_from_disk =
        compiler_serialized_from_disk.at(0);

    AtomMap refmap;
    m_compiler =
        PROG::repack_expanded_userdata(
            m_rt.m_gc, compiler_serialized_from_disk, &refmap);

    m_compiler = m_vm.eval(m_compiler, nullptr);

    std::cout
        << "Compiler initializing from compiler.bklc done, took: "
        << timer_comp_comp.diff() << "ms" << std::endl;

    m_compile_func =
        [this](Atom prog,
            AtomMap *root_env,
            const std::string &input_name,
            bool only_compile)
        {
            GC_ROOT_VEC(m_rt.m_gc, args) = m_rt.m_gc.allocate_vector(4);
            args->m_len = 4;
            args->m_data[0] = Atom(T_STR, m_rt.m_gc.new_symbol(input_name));
            args->m_data[1] = prog;
            args->m_data[2].set_map(root_env);
            args->m_data[3].set_bool(only_compile);
            return m_vm.eval(m_compiler, args);
        };

    m_vm.set_compiler_call(m_compile_func);
}
//---------------------------------------------------------------------------

Atom Instance::execute_string(const std::string &line, AtomMap *root_env)
{
    if (!m_compile_func)
        throw BukaLISPException(
            "Can't use Instance::execute_string without calling "
            "load_bootstrapped_compiler_from_disk() first!");

    Atom line_code =
        m_rt.read("<repl>", line);

    Atom line_prog =
        m_compile_func(line_code, root_env, "<repl>", true);

    return m_vm.eval(line_prog, nullptr);
}
//---------------------------------------------------------------------------

Atom Instance::execute_file(const std::string &filepath)
{
    if (!m_compile_func)
        throw BukaLISPException(
            "Can't use Instance::execute_file without calling "
            "load_bootstrapped_compiler_from_disk() first!");

    GC_ROOT(m_rt.m_gc, root_env) = Atom(T_MAP, m_rt.m_gc.allocate_map());

    std::string code_str = slurp_str(filepath);
    Atom code            = m_rt.read(filepath, code_str);

    Atom vm_prog =
        m_compile_func(code, root_env.m_d.map, filepath, true);

    return m_vm.eval(vm_prog, nullptr);
}
//---------------------------------------------------------------------------

void Instance::load_module(BukaLISPModule *mod)
{
#if USE_MODULES
    m_vm.load_module(mod);
#endif
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
