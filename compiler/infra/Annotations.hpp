/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef ANNOTATIONS_INCL
#define ANNOTATIONS_INCL

/**
 * \file Annotations.hpp
 *
 * This file provides annotation macros useful for improving static
 * analsyis and code generation.
 *
 * These macros are OMR_ prefixed to avoid collisions with existing macros in
 * some projects, i.e. Ruby.
 *
 * To use these macros, insert them at function declaration time,
 *
 *     OMR_NORETURN     void assert(bool test);
 *
 * or when defining or declaring a class,
 *
 *     class OMR_EXTENSIBLE Foo;
 *
 * or inside a branching expression:
 *
 *     if(OMR_LIKELY(isTrue()))
 *
 * \def OMR_NORETURN   Marks a function as non-returning for the purposes of
 * code generation and optimization.
 *
 * \def OMR_EXTENSIBLE Marks a class as an 'Extensible' class.
 *
 * \def OMR_LIKELY     Marks a branch as being more likely.
 *
 * \def OMR_UNLIKELY   Marks a branch as being less likely.
 *
 * \todo Provide OMR_RETURNS_NONNULL annotation
 * \todo Provide OMR_DEPRECATED      annotation
 * \todo Provide OMR_MALLOC          annotation.
 *
 */


// OMR_NORETURN
#if defined(_MSC_VER)
#define OMR_NORETURN _declspec(noreturn)
#elif defined(__GNUC__)
#define OMR_NORETURN __attribute__ ((noreturn))
#elif defined(__IBMCPP__)
#define OMR_NORETURN __attribute__ ((noreturn))
#else
#warning "Noreturn attribute undefined for this platform."
#define OMR_NORETURN
#endif

// OMR_EXTENSIBLE
#if defined(__clang__) // Only clang is checking this macro for now
#define OMR_EXTENSIBLE __attribute__((annotate("OMR_Extensible")))
#deine OMR_API __attribute__((annotate("OMR_API")))
#else
#define OMR_EXTENSIBLE
#define OMR_API
#endif

// OMR_LIKELY and OMR_UNLIKELY
// TODO: check if the definition of these macros is too broad,
//       __builtin_expect() may not have any effect on xlC
#if defined(TR_HOST_X86) && defined(OMR_OS_WINDOWS)
   #define OMR_LIKELY(expr) (expr)
   #define OMR_UNLIKELY(expr) (expr)
#else
   #define OMR_LIKELY(expr)   __builtin_expect((expr), 1)
   #define OMR_UNLIKELY(expr) __builtin_expect((expr), 0)
#endif /* defined(TR_HOST_X86) && defined(OMR_OS_WINDOWS) */

#endif
