BukaLisp - An embeddable Lisp based scripting language
======================================================

BukaLisp is a dynamically typed programming language for scripting that
targets it's own in C++ written VM runtime. One of the main goals is, to have
a small language that is easily embeddable into existing C++ projects.

Aside from embedding it can be extended using compile-time modules that
provide bindings to C++ libraries like Qt, POCO or Boost. The main project
provides modules to extend the standalone BukaLisp interpreter with the goal
of creating a more or less general purpose programming environment.

Relation to Lisp and Scheme
---------------------------

The core primitives and syntax is strongly inspired by Scheme (R7RS small).
In my opinion it's a very solid set of primitives and syntax constructs to
expand upon. However, BukaLisp does not use cons pairs as it's most basic
block for data structures anymore. It internally implements lists as vectors
and not as a linked data structure made of pairs. The main reason is that
I am more comfortable with O(1) access to lists and that it benefits
a bit from the cache architecture of modern Systems.
Another core data structure that is built right into the core of BukaLisp
are (unordered) maps, which provide (mostly) O(1) access by arbitrary keys.

I called it a Lisp and not a Scheme, because BukaLisp is deviating from
the R7RS specification in some more or less important aspects.
It does not support full call-with-current-continuation. It has only support
for coroutines, which is a limited kind of call/cc. There is no strong
reason why, mostly because I haven't engineered it enough to provide them
without overhead for the common case. While call/cc is a very clever construct,
I find it more useful to provide actual control flow primitives like (return ...).

On the performance side BukaLisp performs well on par with Python. I would be
glad to compete on the same performance level as Lua, but for that much more
engineering has to be done at the VM. The garbage collector BukaLisp provides
is also still a very simple mark & sweep garbage collector, and not a three
color GC like Lua uses.

Bootstrapping
-------------

BukaLisp can bootstrap itself by using a small and limited interpreter
for the language. This interpreter is used to execute the compiler, which
compiles itself into a program data structure that can be excuted using
the VM. That data structure then is serialized into a compiled C++ file,
that is integrated into the BukaLisp binary at compile time. This is done
for speeding up the load time of the compiler.

The BukaLisp compiler is a multi-stage compiler, that can be changed to
target other languages like JavaScript or Lua (which is a long term goal).

Project Home
------------

You find the project home at GitHub: https://github.com/WeirdConstructor/BukaLisp

If you find bugs, you may post an Issue on GitHub.

Please bear in mind, that this project is currently only developed by
a single author and issues might get fixed a lot later if not
provided with proper steps for reproduction and/or a patch.

Author
------

The author is "Weird Constructor" a guy you may meet online on his
Twitch channel: http://twitch.tv/weirdconstructor

For any questions about BukaLisp contact the author via
E-Mail at weirdconstructor@gmail.com

License
-------

This source code is published under the MIT License. See lal.lua or LICENSE
file for the complete copyright notice.
