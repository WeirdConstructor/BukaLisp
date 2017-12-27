// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

//---------------------------------------------------------------------------

// Without this, all Atom arrays of AtomVec and AtomMap
// will be allocated directly in the heap in some unspecified order.
// If WITH_MEM_POOL is 1, Atom arrays will be allocated from a
// more compact pool that reduces memory fragmentation a bit.
// If WITH_MEM_POOL is 0 less memory is preallocated and for small
// programs this might mean less memory usage.
#define WITH_MEM_POOL           1

#define TEST_DISABLE_MEM_POOL_INTERNAL 0

//---------------------------------------------------------------------------

// If you enable WITH_STD_UNORDERED_MAP AtomMap will use std::unordered_map.
// This has slight performance implications, as the AtomMap that BukaLisp
// provides does not use linked lists, in the hope of optimizing for better
// cache performance. Benchmarks have shown, that BukaLisp AtomMaps are
// twice as fast as std::unordered_map based implementations. For small
// maps (around 10 key/value pairs) the performance is even better (almost
// 3 times faster).
#define WITH_STD_UNORDERED_MAP  0

//---------------------------------------------------------------------------

// If you enable this flag, the GC will do some more checking and try
// to find bugs in missed rooting of values.
#define GC_DEBUG_MODE 0
//---------------------------------------------------------------------------

// Disables usage of modules:
#define USE_MODULES 1

//---------------------------------------------------------------------------

#if defined(WIN32) || defined(_WIN32)
#   define BKL_PATH_SEP    "\\"
#else
#   define BKL_PATH_SEP    "/"
#endif


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
