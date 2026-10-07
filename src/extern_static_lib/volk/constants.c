/* -*- c++ -*- */
/*
 * Copyright 2013, 2014 Free Software Foundation, Inc.
 *
 * This file is part of VOLK
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

/* #undef VOLK_STATIC_DISPATCH */

#ifndef VOLK_STATIC_DISPATCH
#include <stdlib.h>
#endif
#include <volk/constants.h>

const char*
volk_prefix()
{
#ifndef VOLK_STATIC_DISPATCH
  const char *prefix = getenv("VOLK_PREFIX");
  if (prefix != NULL) return prefix;
#endif
  return "";
}

const char*
volk_version()
{
  return "3.3.0";
}

const char*
volk_c_compiler()
{
  return "Microsoft Visual Studio 14.0";
}

const char*
volk_compiler_flags()
{
  return "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/14.32.31326/bin/Hostx64/x64/cl.exe::: /DWIN32 /D_WINDOWS -Wall \nC:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/14.32.31326/bin/Hostx64/x64/cl.exe::: /DWIN32 /D_WINDOWS /EHsc -Wall \ngeneric:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall  \nsse2:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall  \nsse3:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall /arch:AVX \nssse3:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall /arch:AVX /arch:AVX \nsse4_1:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall /arch:AVX /arch:AVX /arch:AVX \nsse4_2:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall /arch:AVX /arch:AVX /arch:AVX /arch:AVX /arch:AVX \navx:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall /arch:AVX /arch:AVX /arch:AVX /arch:AVX /arch:AVX /arch:AVX \navx2:::MSVC:::/O2 /Ob2 /DNDEBUG /DWIN32 /D_WINDOWS -Wall /arch:AVX /arch:AVX /arch:AVX /arch:AVX /arch:AVX /arch:AVX /arch:AVX2 /arch:AVX2";
}

const char*
volk_available_machines()
{
  return "generic;sse2;sse3;ssse3;sse4_1;sse4_2;avx;avx2";
}
