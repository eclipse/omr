/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#ifndef OMRFORMATCONSTANTS_H_
#define OMRFORMATCONSTANTS_H_

#include "omrcfg.h"

#if !defined(_MSC_VER)
#include <inttypes.h>
#else
#include <stdint.h>
#endif

/* OMR_PRId8: int8_t */
#if defined(PRId8)
#define OMR_PRId8 PRId8
#elif defined(__GNUC__) || defined(__clang__) || defined(__xlC__) || defined(__open_xl__)
#define OMR_PRId8 "hhd"
#else
#define OMR_PRId8 "d" /* promote to int */
#endif

/* OMR_PRIu8: uint8_t */
#if defined(PRIu8)
#define OMR_PRIu8 PRIu8
#elif defined(__GNUC__) || defined(__clang__) || defined(__xlC__) || defined(__open_xl__)
#define OMR_PRIu8 "hhu"
#else
#define OMR_PRIu8 "u" /* promote to unsigned int */
#endif

/* OMR_PRIx8: uint8_t (hex) */
#if defined(PRIx8)
#define OMR_PRIx8 PRIx8
#elif defined(__GNUC__) || defined(__clang__) || defined(__xlC__) || defined(__open_xl__)
#define OMR_PRIx8 "hhx"
#else
#define OMR_PRIx8 "x" /* promote to unsigned int */
#endif

/* OMR_PRIX8: uint8_t (uppercase hex) */
#if defined(PRIX8)
#define OMR_PRIX8 PRIX8
#elif defined(__GNUC__) || defined(__clang__) || defined(__xlC__) || defined(__open_xl__) 
#define OMR_PRIX8 "hhX"
#else
#define OMR_PRIX8 "X" /* promote to unsigned int */
#endif

/* OMR_PRId16: int16_t */
#if defined(PRId16)
#define OMR_PRId16 PRId16
#else
#define OMR_PRId16 "hd"
#endif

/* OMR_PRIu16: uint16_t */
#if defined(PRIu16)
#define OMR_PRIu16 PRIu16
#else
#define OMR_PRIu16 "hu"
#endif

/* OMR_PRIx16: uint16_t (hex) */
#if defined(PRIx16)
#define OMR_PRIx16 PRIx16
#else
#define OMR_PRIx16 "hx"
#endif

/* OMR_PRIX16: uint16_t (uppercase hex) */
#if defined(PRIX16)
#define OMR_PRIX16 PRIX16
#else
#define OMR_PRIX16 "hX"
#endif

/* OMR_PRId32: int32_t */
#if defined(_MSC_VER) && (1900 <= _MSC_VER)
/* msvc has a badly defined format macro for int32 */
#define OMR_PRId32 "I32d"
#elif defined(PRId32)
#define OMR_PRId32 PRId32
#else
#define OMR_PRId32 "d"
#endif

/* OMR_PRIu32: uint32_t */
#if defined(_MSC_VER) && (1900 <= _MSC_VER)
/* msvc has a badly defined format macro for int32 */
#define OMR_PRIu32 "I32u"
#elif defined(PRIu32)
#define OMR_PRIu32 PRIu32
#else
#define OMR_PRIu32 "u"
#endif

/* OMR_PRIx32: uint32_t (hex) */
#if defined(_MSC_VER) && (1900 <= _MSC_VER)
/* msvc has a badly defined format macro for int32 */
#define OMR_PRIx32 "I32x"
#elif defined(PRIx32)
#define OMR_PRIx32 PRIx32
#else
#define OMR_PRIx32 "x"
#endif

/* OMR_PRIX32: uint32_t (uppercase hex) */
#if defined(_MSC_VER) && (1900 <= _MSC_VER)
/* msvc has a badly defined format macro for int32 */
#define OMR_PRIX32 "I32X"
#elif defined(PRIX32)
#define OMR_PRIX32 PRIX32
#else
#define OMR_PRIX32 "X"
#endif

/* OMR_PRId64: int64_t */
#if defined(PRId64)
#define OMR_PRId64 PRId64
#elif defined(_MSC_VER)
#define OMR_PRId64 "I64d"
#else
#define OMR_PRId64 "lld"
#endif

/* OMR_PRIu64: uint64_t */
#if defined(PRIu64)
#define OMR_PRIu64 PRIu64
#elif defined(_MSC_VER)
#define OMR_PRIu64 "I64u"
#else
#define OMR_PRIu64 "llu"
#endif

/* OMR_PRIx64: uint64_t (hex) */
#if defined(PRIx64)
#define OMR_PRIx64 PRIx64
#elif defined(_MSC_VER)
#define OMR_PRIx64 "I64x"
#else
#define OMR_PRIx64 "llx"
#endif

/* OMR_PRIX64: uint64_t (uppercase hex) */
#if defined(PRIX64)
#define OMR_PRIX64 PRIX64
#elif defined(_MSC_VER)
#define OMR_PRIX64 "I64X"
#else
#define OMR_PRIX64 "llX"
#endif

/* OMR_PRIdSIZE: signed size_t */
#if defined(_MSC_VER)
#define OMR_PRIdSIZE "Iu"
#else
#define OMR_PRIdSIZE "zu"
#endif

/* OMR_PRIuSIZE: size_t */
#if defined(_MSC_VER)
#define OMR_PRIuSIZE "Iu"
#else
#define OMR_PRIuSIZE "zu"
#endif

/* OMR_PRIxSIZE: size_t (hex) */
#if defined(_MSC_VER)
#define OMR_PRIxSIZE "Ix"
#else
#define OMR_PRIxSIZE "zx"
#endif

/* OMR_PRIXSIZE: size_t (uppercase hex) */
#if defined(_MSC_VER)
#define OMR_PRIXSIZE "IX"
#else
#define OMR_PRIXSIZE "zX"
#endif

/* PRIdPTR: intptr_t */
#if defined(PRIdPTR)
#define OMR_PRIdPTR PRIdPTR
#elif defined(_MSC_VER)
#define OMR_PRIdPTR "Id"
#else
#define OMR_PRIdPTR "zd"
#endif

/* PRIuPTR: uintptr_t */
#if defined(PRIuPTR)
#define OMR_PRIuPTR PRIuPTR
#elif defined(_MSC_VER)
#define OMR_PRIuPTR "Iu"
#else
#define OMR_PRIuPTR "zu"
#endif

/* PRIxPTR: uintptr_t (hex) */
#if defined(PRIxPTR)
#define OMR_PRIxPTR PRIxPTR
#elif defined(_MSC_VER)
#define OMR_PRIxPTR "Ix"
#else
#define OMR_PRIxPTR "zx"
#endif

/* PRIXPTR: uintptr_t (uppercase hex) */
#if defined(PRIXPTR)
#define OMR_PRIXPTR PRIXPTR
#elif defined(_MSC_VER)
#define OMR_PRIXPTR "IX"
#else
#define OMR_PRIXPTR "zX"
#endif

/* OMR_PRIdPTRDIFF: ptrdiff_t */
#if defined(_MSC_VER)
#define OMR_PRIdPTRDIFF "Id"
#else
#define OMR_PRIdPTRDIFF "td"
#endif

/* OMR_PRIuPTRDIFF: ptrdiff_t */
#if defined(_MSC_VER)
#define OMR_PRIuPTRDIFF "Iu"
#else
#define OMR_PRIuPTRDIFF "tu"
#endif

/* OMR_PRIxPTRDIFF: unsigned ptrdiff_t (hex) */
#if defined(_MSC_VER)
#define OMR_PRIxPTRDIFF "Ix"
#else
#define OMR_PRIxPTRDIFF "tx"
#endif

/* OMR_PRIXPTRDIFF: unsigned ptrdiff_t (uppercase hex) */
#if defined(_MSC_VER)
#define OMR_PRIXPTRDIFF "IX"
#else
#define OMR_PRIXPTRDIFF "tX"
#endif

#endif // OMRFORMATCONSTANTS_H_
