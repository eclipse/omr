/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_DEFINES_H
#define OMR_DEFINES_H

/*
   This is the first file that the OMR code should be including.
   The idea is the define a standard set of C macros based on the
   system configuration. This is the only file (hopefully) needs to
   use system specific macros (e.g. __linux__).

   In addition, if there are system-configuration specific feature
   test macros that you want to define, this is probably the file you
   want to define them in. You should also provide documentation of
   what your macro does. As an example see SUPPORTS_THREAD_LOCAL

   Note that this file is included is asm code as well. Apart from C
   pre-processor macros, it shouldn't contain anything else.
*/

/* Operating Systems */
#ifndef OMR_LINUX
#define OMR_LINUX 201
#endif
#ifndef OMR_WINDOWS
#define OMR_WINDOWS 202
#endif
#ifndef OMR_AIX
#define OMR_AIX 203
#endif
#ifndef OMR_ZOS
#define OMR_ZOS 204
#endif
#ifndef OMR_OSX
#define OMR_OSX 205
#endif
#ifndef OMR_BSD
#define OMR_BSD 206
#endif

/* Architectures */
#ifndef ARCH_X86
#define ARCH_X86 101
#endif
#ifndef ARCH_POWER
#define ARCH_POWER 102
#endif
#ifndef ARCH_ZARCH
#define ARCH_ZARCH 103
#endif
#ifndef ARCH_ARM
#define ARCH_ARM 104
#endif
#ifndef ARCH_ARM64
#define ARCH_ARM64 105
#endif
#ifndef ARCH_RISCV
#define ARCH_RISCV 106
#endif

/* Compilers */
#ifndef COMPILER_GCC
#define COMPILER_GCC 301
#endif
#ifndef COMPILER_XLC
#define COMPILER_XLC 302
#endif
#ifndef COMPILER_MSVC
#define COMPILER_MSVC 303
#endif
#ifndef COMPILER_CLANG
#define COMPILER_CLANG 304
#endif

/* @ddr_namespace: map_to_type=TRBuildFlags */

/*
  Standardize the OS macros Use HOST_OS instead of various different
  ways of checking for a particular host os.  As a reference, see
  http://sourceforge.net/p/predef/wiki/OperatingSystems/
*/
#if defined(__linux__)
#define HOST_OS OMR_LINUX
#elif defined(_TPF_SOURCE)
#define HOST_OS OMR_LINUX
#elif defined(_AIX)
#define HOST_OS OMR_AIX
#elif defined(OMR_OS_WINDOWS)
#define HOST_OS OMR_WINDOWS
#elif defined(__MVS__)
#define HOST_OS OMR_ZOS
#elif defined(__APPLE__) && defined(__MACH__)
#define HOST_OS OMR_OSX
#elif defined(OMR_OS_BSD)
#define HOST_OS OMR_BSD
#else
#error "defines.h: unknown OS"
#endif

/* Architecture macros */
/* As a reference see http://sourceforge.net/p/predef/wiki/Architectures */
/* FIXME: rename TR_HOST_32BIT and TR_HOST_64BIT */
#if defined(__amd64__) || defined(_M_AMD64)
#define HOST_ARCH ARCH_X86
#define TR_HOST_64BIT 1
#elif defined(__i386__) || defined(_M_IX86)
#define HOST_ARCH ARCH_X86
#define TR_HOST_32BIT 1
#elif defined(__PPC__) || defined(_ARCH_PPC)
#define HOST_ARCH ARCH_POWER
#if defined(__PPC64__) || defined(__64BIT__)
#define TR_HOST_64BIT 1
#else
#define TR_HOST_32BIT 1
#endif
#elif defined(__s390x__) || (defined(__370__) && defined(__64BIT__))
#define HOST_ARCH ARCH_ZARCH
#define TR_HOST_64BIT 1
#elif defined(__s390__) || (defined(__370__) && !defined(__64BIT__))
#define HOST_ARCH ARCH_ZARCH
#define TR_HOST_32BIT 1
#elif defined(__arm__)
#define HOST_ARCH ARCH_ARM
#define TR_HOST_32BIT 1
#elif defined(__aarch64__)
#define HOST_ARCH ARCH_ARM64
#define TR_HOST_64BIT 1
#elif defined(__riscv)
#define HOST_ARCH ARCH_RISCV
#if __riscv_xlen == 32
#define TR_HOST_32BIT 1
#elif __riscv_xlen == 64
#define TR_HOST_64BIT 1
#else
#error "defines.h: unknown word size"
#endif
#else
#error "defines.h: unknown architecture"
#endif

/* Unless the target arch was specified, we are building the same target as the host. */
#ifndef TARGET_ARCH
#define TARGET_ARCH HOST_ARCH
#endif

/* Compiler macros */
#if defined(__GNUC__)
#define HOST_COMPILER COMPILER_GCC
#elif defined(__IBMC__) || defined(__IBMCPP__) || defined(__open_xl__)
#define HOST_COMPILER COMPILER_XLC
#elif defined(_MSC_VER)
#define HOST_COMPILER COMPILER_MSVC
#elif defined(__clang__)
#define HOST_COMPILER COMPILER_CLANG
#else
#error "defines.h: unknown compiler"
#endif

/* FIXME: we need to deprecate TR_HOST_* macros and replace them with
   code of the form: #if (HOST_ARCH == ARCH_X86) */
#if (HOST_ARCH == ARCH_X86)
#define TR_HOST_X86 1
#endif

#if (HOST_ARCH == ARCH_ZARCH)
#define TR_HOST_S390 1
#endif

#if (HOST_ARCH == ARCH_POWER)
#define TR_HOST_POWER 1
#endif

#if (HOST_ARCH == ARCH_ARM)
#define TR_HOST_ARM 1
#endif

#if (HOST_ARCH == ARCH_ARM64)
#define TR_HOST_ARM64 1
#endif

#if (HOST_ARCH == ARCH_RISCV)
#define TR_HOST_RISCV 1
#endif

/* @ddr_namespace: default */

/* FIXME: we need to document what these ifdefs do
   If these are permanently enabled, the ifdefs should be removed
   and the guarded code permanently enabled */
#define BITVECTOR_BIT_NUMBERING_MSB 1

// If TR_ALLOW_NON_CONST_KNOWN_OBJECTS is defined, then it's possible to
// disable const refs (using the disableConstRefs option), in which case known
// object indices will be used in the trees in the same way they were used
// prior to const refs. That is, static and shadow symrefs will be "improved"
// with known object indices, and (to a more limited extent) individual nodes
// will have known object indices set on them.
//
// Remove this definition to statically disallow "improved" symrefs and
// per-node known object indices at build time. This is useful to ensure that
// const refs has an alternative for every usage of those techniques. It should
// also eventually be useful for identifying code that's no longer necessary
// once const refs are sufficiently well established that the old way can be
// removed.
//
//#define TR_ALLOW_NON_CONST_KNOWN_OBJECTS

#endif /* OMR_DEFINES_H */
