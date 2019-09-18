/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include <stdint.h>
#include <stdio.h>
#include "compile/Method.hpp"
#include "env/jittypes.h"
#include "gtest/gtest.h"
#include "OpCodesTest.hpp"

namespace TestCompiler
{


#if defined(TR_TARGET_ARM64)
static int32_t int32BitwiseEncodableImmediates [] =
   {
   /* element size = 32 */
   static_cast<int32_t>(0x7fffffff), // ones count = 31, rotate count = 0
   static_cast<int32_t>(0xffff7fff), // ones count = 31, rotate count = 16
   static_cast<int32_t>(0xfffffffe), // ones count = 31, rotate count = 31
   static_cast<int32_t>(0x3fffffff), // ones count = 30, rotate count = 0
   static_cast<int32_t>(0xffff3fff), // ones count = 30, rotate count = 16
   static_cast<int32_t>(0xfffffffc), // ones count = 30, rotate count = 30
   static_cast<int32_t>(0x7ffffffe), // ones count = 30, rotate count = 31
   static_cast<int32_t>(0x0000ffff), // ones count = 16, rotate count = 0
   static_cast<int32_t>(0xff0000ff), // ones count = 16, rotate count = 8
   static_cast<int32_t>(0xffff0000), // ones count = 16, rotate count = 16
   static_cast<int32_t>(0x00ffff00), // ones count = 16, rotate count = 24
   static_cast<int32_t>(0x000000ff), // ones count = 8, rotate count = 0
   static_cast<int32_t>(0xf000000f), // ones count = 8, rotate count = 4
   static_cast<int32_t>(0xff000000), // ones count = 8, rotate count = 8
   static_cast<int32_t>(0x00ff0000), // ones count = 8, rotate count = 16
   static_cast<int32_t>(0x0000ff00), // ones count = 8, rotate count = 24
   static_cast<int32_t>(0x00000003), // ones count = 2, rotate count = 0
   static_cast<int32_t>(0x80000001), // ones count = 2, rotate count = 1
   static_cast<int32_t>(0xc0000000), // ones count = 2, rotate count = 2
   static_cast<int32_t>(0x00030000), // ones count = 2, rotate count = 16
   static_cast<int32_t>(0x00000001), // ones count = 1, rotate count = 0
   static_cast<int32_t>(0x80000000), // ones count = 1, rotate count = 1
   static_cast<int32_t>(0x00010000), // ones count = 1, rotate count = 16
   static_cast<int32_t>(0x00000002), // ones count = 1, rotate count = 31

   /* element size = 16 */
   static_cast<int32_t>(0x7fff7fff), // ones count = 15, rotate count = 0
   static_cast<int32_t>(0xff7fff7f), // ones count = 15, rotate count = 8
   static_cast<int32_t>(0xfffefffe), // ones count = 15, rotate count = 15
   static_cast<int32_t>(0x3fff3fff), // ones count = 14, rotate count = 0
   static_cast<int32_t>(0xff3fff3f), // ones count = 14, rotate count = 8
   static_cast<int32_t>(0xfffcfffc), // ones count = 14, rotate count = 14
   static_cast<int32_t>(0x7ffe7ffe), // ones count = 14, rotate count = 15
   static_cast<int32_t>(0x00ff00ff), // ones count = 8, rotate count = 0
   static_cast<int32_t>(0xf00ff00f), // ones count = 8, rotate count = 4
   static_cast<int32_t>(0xff00ff00), // ones count = 8, rotate count = 8
   static_cast<int32_t>(0x0ff00ff0), // ones count = 8, rotate count = 12
   static_cast<int32_t>(0x00030003), // ones count = 2, rotate count = 0
   static_cast<int32_t>(0x80018001), // ones count = 2, rotate count = 1
   static_cast<int32_t>(0xc000c000), // ones count = 2, rotate count = 2
   static_cast<int32_t>(0x03000300), // ones count = 2, rotate count = 8
   static_cast<int32_t>(0x00010001), // ones count = 1, rotate count = 0
   static_cast<int32_t>(0x80008000), // ones count = 1, rotate count = 1
   static_cast<int32_t>(0x01000100), // ones count = 1, rotate count = 8

   /* element size = 8 */
   static_cast<int32_t>(0x7f7f7f7f), // ones count = 7, rotate count = 0
   static_cast<int32_t>(0xf7f7f7f7), // ones count = 7, rotate count = 4
   static_cast<int32_t>(0xfefefefe), // ones count = 7, rotate count = 7
   static_cast<int32_t>(0x3f3f3f3f), // ones count = 6, rotate count = 0
   static_cast<int32_t>(0xf3f3f3f3), // ones count = 6, rotate count = 4
   static_cast<int32_t>(0xfcfcfcfc), // ones count = 6, rotate count = 6
   static_cast<int32_t>(0x7e7e7e7e), // ones count = 6, rotate count = 7
   static_cast<int32_t>(0x0f0f0f0f), // ones count = 4, rotate count = 0
   static_cast<int32_t>(0xc3c3c3c3), // ones count = 4, rotate count = 2
   static_cast<int32_t>(0xe1e1e1e1), // ones count = 4, rotate count = 3
   static_cast<int32_t>(0xf0f0f0f0), // ones count = 4, rotate count = 4
   static_cast<int32_t>(0x1e1e1e1e), // ones count = 4, rotate count = 7
   static_cast<int32_t>(0x03030303), // ones count = 2, rotate count = 0
   static_cast<int32_t>(0x81818181), // ones count = 2, rotate count = 1
   static_cast<int32_t>(0xc0c0c0c0), // ones count = 2, rotate count = 2
   static_cast<int32_t>(0x30303030), // ones count = 2, rotate count = 4
   static_cast<int32_t>(0x06060606), // ones count = 2, rotate count = 7
   static_cast<int32_t>(0x01010101), // ones count = 1, rotate count = 0
   static_cast<int32_t>(0x80808080), // ones count = 1, rotate count = 1
   static_cast<int32_t>(0x10101010), // ones count = 1, rotate count = 4

   /* element size = 4 */
   static_cast<int32_t>(0x77777777), // ones count = 3, rotate count = 0
   static_cast<int32_t>(0xbbbbbbbb), // ones count = 3, rotate count = 1
   static_cast<int32_t>(0xdddddddd), // ones count = 3, rotate count = 2
   static_cast<int32_t>(0xeeeeeeee), // ones count = 3, rotate count = 3
   static_cast<int32_t>(0x33333333), // ones count = 2, rotate count = 0
   static_cast<int32_t>(0x99999999), // ones count = 2, rotate count = 1
   static_cast<int32_t>(0xcccccccc), // ones count = 2, rotate count = 2
   static_cast<int32_t>(0x66666666), // ones count = 2, rotate count = 3
   static_cast<int32_t>(0x11111111), // ones count = 1, rotate count = 0
   static_cast<int32_t>(0x88888888), // ones count = 1, rotate count = 1
   static_cast<int32_t>(0x44444444), // ones count = 1, rotate count = 2
   static_cast<int32_t>(0x22222222), // ones count = 1, rotate count = 3

   /* element size = 2 */
   static_cast<int32_t>(0x55555555), // ones count = 1, rotate count = 0
   static_cast<int32_t>(0xaaaaaaaa), // ones count = 1, rotate count = 1

   };

static int32_t int32BitwiseUnencodableImmediates [] =
   {
   /* element size = 32 */
   static_cast<int32_t>(0x7ffeffff), // ones count = 30
   static_cast<int32_t>(0xffff7ffe), // ones count = 30
   static_cast<int32_t>(0x7fef7fff), // ones count = 29
   static_cast<int32_t>(0x7ffefffe), // ones count = 29
   static_cast<int32_t>(0x0000feff), // ones count = 15
   static_cast<int32_t>(0xfeff0000), // ones count = 15
   static_cast<int32_t>(0x00feff00), // ones count = 15
   static_cast<int32_t>(0x000000f7), // ones count = 7
   static_cast<int32_t>(0xf7000000), // ones count = 7
   static_cast<int32_t>(0x7000000f), // ones count = 7
   static_cast<int32_t>(0x000f7000), // ones count = 7
   static_cast<int32_t>(0x00020001), // ones count = 2
   static_cast<int32_t>(0x80004000), // ones count = 2
   static_cast<int32_t>(0x08000400), // ones count = 2
   /* element size = 16 */
   static_cast<int32_t>(0x7fef7fef), // ones count = 14
   static_cast<int32_t>(0xf7fef7fe), // ones count = 14
   static_cast<int32_t>(0x7eef7eef), // ones count = 13
   static_cast<int32_t>(0x00ef00ef), // ones count = 7
   static_cast<int32_t>(0xf00ef00e), // ones count = 7
   static_cast<int32_t>(0x0ef00ef0), // ones count = 7
   static_cast<int32_t>(0x02010201), // ones count = 2
   static_cast<int32_t>(0x80408040), // ones count = 2
   static_cast<int32_t>(0x08040804), // ones count = 2
   /* element size = 8 */
   static_cast<int32_t>(0x7b7b7b7b), // ones count = 6
   static_cast<int32_t>(0xf6f6f6f6), // ones count = 6
   static_cast<int32_t>(0x6e6e6e6e), // ones count = 5
   static_cast<int32_t>(0x21212121), // ones count = 2
   static_cast<int32_t>(0x84848484), // ones count = 2
   static_cast<int32_t>(0x42424242), // ones count = 2

   };

static int64_t int64BitwiseEncodableImmediates [] =
   {
   /* element size = 64 */
   static_cast<int64_t>(0x7fffffffffffffff), // ones count = 63, rotate count = 0
   static_cast<int64_t>(0xffffffff7fffffff), // ones count = 63, rotate count = 32
   static_cast<int64_t>(0xfffffffffffffffe), // ones count = 63, rotate count = 63
   static_cast<int64_t>(0x3fffffffffffffff), // ones count = 62, rotate count = 0
   static_cast<int64_t>(0xffffffffefffffff), // ones count = 62, rotate count = 32
   static_cast<int64_t>(0xfffffffffffffffc), // ones count = 62, rotate count = 62
   static_cast<int64_t>(0x7ffffffffffffffe), // ones count = 62, rotate count = 63
   static_cast<int64_t>(0x00000000ffffffff), // ones count = 32, rotate count = 0
   static_cast<int64_t>(0xffff00000000ffff), // ones count = 32, rotate count = 16
   static_cast<int64_t>(0xffffffff00000000), // ones count = 32, rotate count = 32
   static_cast<int64_t>(0x0000ffffffff0000), // ones count = 32, rotate count = 48
   static_cast<int64_t>(0x000000000000ffff), // ones count = 16, rotate count = 0
   static_cast<int64_t>(0xff000000000000ff), // ones count = 16, rotate count = 8
   static_cast<int64_t>(0xffff000000000000), // ones count = 16, rotate count = 16
   static_cast<int64_t>(0x0000ffff00000000), // ones count = 16, rotate count = 32
   static_cast<int64_t>(0x00000000000000ff), // ones count = 8, rotate count = 0
   static_cast<int64_t>(0xf00000000000000f), // ones count = 8, rotate count = 4
   static_cast<int64_t>(0xff00000000000000), // ones count = 8, rotate count = 8
   static_cast<int64_t>(0x00ff000000000000), // ones count = 8, rotate count = 16
   static_cast<int64_t>(0x000000ff00000000), // ones count = 8, rotate count = 32
   static_cast<int64_t>(0x0000000000000003), // ones count = 2, rotate count = 0
   static_cast<int64_t>(0x8000000000000001), // ones count = 2, rotate count = 1
   static_cast<int64_t>(0xc000000000000000), // ones count = 2, rotate count = 2
   static_cast<int64_t>(0x0000000300000000), // ones count = 2, rotate count = 32
   static_cast<int64_t>(0x0000000000000001), // ones count = 1, rotate count = 0
   static_cast<int64_t>(0x8000000000000000), // ones count = 1, rotate count = 1
   static_cast<int64_t>(0x0000000100000000), // ones count = 1, rotate count = 32
   static_cast<int64_t>(0x0000000000000002), // ones count = 1, rotate count = 63

   /* element size = 32 */
   static_cast<int64_t>(0x7fffffff7fffffff), // ones count = 31, rotate count = 0
   static_cast<int64_t>(0xffff7fffffff7fff), // ones count = 31, rotate count = 16
   static_cast<int64_t>(0xfffffffefffffffe), // ones count = 31, rotate count = 31
   static_cast<int64_t>(0x3fffffff3fffffff), // ones count = 30, rotate count = 0
   static_cast<int64_t>(0xffff3fffffff3fff), // ones count = 30, rotate count = 16
   static_cast<int64_t>(0xfffffffcfffffffc), // ones count = 30, rotate count = 30
   static_cast<int64_t>(0x7ffffffe7ffffffe), // ones count = 30, rotate count = 31
   static_cast<int64_t>(0x0000ffff0000ffff), // ones count = 16, rotate count = 0
   static_cast<int64_t>(0xff0000ffff0000ff), // ones count = 16, rotate count = 8
   static_cast<int64_t>(0xffff0000ffff0000), // ones count = 16, rotate count = 16
   static_cast<int64_t>(0x00ffff0000ffff00), // ones count = 16, rotate count = 24
   static_cast<int64_t>(0x000000ff000000ff), // ones count = 8, rotate count = 0
   static_cast<int64_t>(0xf000000ff000000f), // ones count = 8, rotate count = 4
   static_cast<int64_t>(0xff000000ff000000), // ones count = 8, rotate count = 8
   static_cast<int64_t>(0x00ff000000ff0000), // ones count = 8, rotate count = 16
   static_cast<int64_t>(0x0000ff000000ff00), // ones count = 8, rotate count = 24
   static_cast<int64_t>(0x0000000300000003), // ones count = 2, rotate count = 0
   static_cast<int64_t>(0x8000000180000001), // ones count = 2, rotate count = 1
   static_cast<int64_t>(0xc0000000c0000000), // ones count = 2, rotate count = 2
   static_cast<int64_t>(0x0003000000300000), // ones count = 2, rotate count = 16
   static_cast<int64_t>(0x0000000100000001), // ones count = 1, rotate count = 0
   static_cast<int64_t>(0x8000000080000000), // ones count = 1, rotate count = 1
   static_cast<int64_t>(0x0001000000010000), // ones count = 1, rotate count = 16
   static_cast<int64_t>(0x0000000200000002), // ones count = 1, rotate count = 31

   /* element size = 16 */
   static_cast<int64_t>(0x7fff7fff7fff7fff), // ones count = 15, rotate count = 0
   static_cast<int64_t>(0xff7fff7fff7fff7f), // ones count = 15, rotate count = 8
   static_cast<int64_t>(0xfffefffefffefffe), // ones count = 15, rotate count = 15
   static_cast<int64_t>(0x3fff3fff3fff3fff), // ones count = 14, rotate count = 0
   static_cast<int64_t>(0xff3fff3fff3fff3f), // ones count = 14, rotate count = 8
   static_cast<int64_t>(0xfffcfffcfffcfffc), // ones count = 14, rotate count = 14
   static_cast<int64_t>(0x7ffe7ffe7ffe7ffe), // ones count = 14, rotate count = 15
   static_cast<int64_t>(0x00ff00ff00ff00ff), // ones count = 8, rotate count = 0
   static_cast<int64_t>(0xf00ff00ff00ff00f), // ones count = 8, rotate count = 4
   static_cast<int64_t>(0xff00ff00ff00ff00), // ones count = 8, rotate count = 8
   static_cast<int64_t>(0x0ff00ff00ff00ff0), // ones count = 8, rotate count = 12
   static_cast<int64_t>(0x0003000300030003), // ones count = 2, rotate count = 0
   static_cast<int64_t>(0x8001800180018001), // ones count = 2, rotate count = 1
   static_cast<int64_t>(0xc000c000c000c000), // ones count = 2, rotate count = 2
   static_cast<int64_t>(0x0300030003000300), // ones count = 2, rotate count = 8
   static_cast<int64_t>(0x0001000100010001), // ones count = 1, rotate count = 0
   static_cast<int64_t>(0x8000800080008000), // ones count = 1, rotate count = 1
   static_cast<int64_t>(0x0100010001000100), // ones count = 1, rotate count = 8

   /* element size = 8 */
   static_cast<int64_t>(0x7f7f7f7f7f7f7f7f), // ones count = 7, rotate count = 0
   static_cast<int64_t>(0xf7f7f7f7f7f7f7f7), // ones count = 7, rotate count = 4
   static_cast<int64_t>(0xfefefefefefefefe), // ones count = 7, rotate count = 7
   static_cast<int64_t>(0x3f3f3f3f3f3f3f3f), // ones count = 6, rotate count = 0
   static_cast<int64_t>(0xf3f3f3f3f3f3f3f3), // ones count = 6, rotate count = 4
   static_cast<int64_t>(0xfcfcfcfcfcfcfcfc), // ones count = 6, rotate count = 6
   static_cast<int64_t>(0x7e7e7e7e7e7e7e7e), // ones count = 6, rotate count = 7
   static_cast<int64_t>(0x0f0f0f0f0f0f0f0f), // ones count = 4, rotate count = 0
   static_cast<int64_t>(0xc3c3c3c3c3c3c3c3), // ones count = 4, rotate count = 2
   static_cast<int64_t>(0xe1e1e1e1e1e1e1e1), // ones count = 4, rotate count = 3
   static_cast<int64_t>(0xf0f0f0f0f0f0f0f0), // ones count = 4, rotate count = 4
   static_cast<int64_t>(0x1e1e1e1e1e1e1e1e), // ones count = 4, rotate count = 7
   static_cast<int64_t>(0x0303030303030303), // ones count = 2, rotate count = 0
   static_cast<int64_t>(0x8181818181818181), // ones count = 2, rotate count = 1
   static_cast<int64_t>(0xc0c0c0c0c0c0c0c0), // ones count = 2, rotate count = 2
   static_cast<int64_t>(0x3030303030303030), // ones count = 2, rotate count = 4
   static_cast<int64_t>(0x0606060606060606), // ones count = 2, rotate count = 7
   static_cast<int64_t>(0x0101010101010101), // ones count = 1, rotate count = 0
   static_cast<int64_t>(0x8080808080808080), // ones count = 1, rotate count = 1
   static_cast<int64_t>(0x1010101010101010), // ones count = 1, rotate count = 4

   /* element size = 4 */
   static_cast<int64_t>(0x7777777777777777), // ones count = 3, rotate count = 0
   static_cast<int64_t>(0xbbbbbbbbbbbbbbbb), // ones count = 3, rotate count = 1
   static_cast<int64_t>(0xdddddddddddddddd), // ones count = 3, rotate count = 2
   static_cast<int64_t>(0xeeeeeeeeeeeeeeee), // ones count = 3, rotate count = 3
   static_cast<int64_t>(0x3333333333333333), // ones count = 2, rotate count = 0
   static_cast<int64_t>(0x9999999999999999), // ones count = 2, rotate count = 1
   static_cast<int64_t>(0xcccccccccccccccc), // ones count = 2, rotate count = 2
   static_cast<int64_t>(0x6666666666666666), // ones count = 2, rotate count = 3
   static_cast<int64_t>(0x1111111111111111), // ones count = 1, rotate count = 0
   static_cast<int64_t>(0x8888888888888888), // ones count = 1, rotate count = 1
   static_cast<int64_t>(0x4444444444444444), // ones count = 1, rotate count = 2
   static_cast<int64_t>(0x2222222222222222), // ones count = 1, rotate count = 3

   /* element size = 2 */
   static_cast<int64_t>(0x5555555555555555), // ones count = 1, rotate count = 0
   static_cast<int64_t>(0xaaaaaaaaaaaaaaaa), // ones count = 1, rotate count = 1

   };

static int64_t int64BitwiseUnencodableImmediates [] =
   {
   /* element size = 64 */
   static_cast<int64_t>(0x7ffeffffffffffff), // ones count = 62
   static_cast<int64_t>(0xffffffff7ffeffff), // ones count = 62
   static_cast<int64_t>(0x7ffeffff7fffffff), // ones count = 61
   static_cast<int64_t>(0x7ffefffffffffffe), // ones count = 61
   static_cast<int64_t>(0x00000000efffffff), // ones count = 31
   static_cast<int64_t>(0xffff00000000efff), // ones count = 31
   static_cast<int64_t>(0x00000000efff0000), // ones count = 31
   static_cast<int64_t>(0x000000000000feff), // ones count = 15
   static_cast<int64_t>(0xfeff000000000000), // ones count = 15
   static_cast<int64_t>(0x0000feff00000000), // ones count = 15
   static_cast<int64_t>(0x00000000000000f7), // ones count = 7
   static_cast<int64_t>(0xf700000000000000), // ones count = 7
   static_cast<int64_t>(0x700000000000000f), // ones count = 7
   static_cast<int64_t>(0x000000000f700000), // ones count = 7
   static_cast<int64_t>(0x0000002000000001), // ones count = 2
   static_cast<int64_t>(0x8000004000000000), // ones count = 2
   static_cast<int64_t>(0x0800000400000000), // ones count = 2

   /* element size = 32 */
   static_cast<int64_t>(0x7ffeffff7ffeffff), // ones count = 30
   static_cast<int64_t>(0xffff7ffeffff7ffe), // ones count = 30
   static_cast<int64_t>(0x7fef7fff7fef7fff), // ones count = 29
   static_cast<int64_t>(0x7ffefffe7ffefffe), // ones count = 29
   static_cast<int64_t>(0x0000feff0000feff), // ones count = 15
   static_cast<int64_t>(0xfeff0000feff0000), // ones count = 15
   static_cast<int64_t>(0x00feff0000feff00), // ones count = 15
   static_cast<int64_t>(0x000000f7000000f7), // ones count = 7
   static_cast<int64_t>(0xf7000000f7000000), // ones count = 7
   static_cast<int64_t>(0x7000000f7000000f), // ones count = 7
   static_cast<int64_t>(0x000f7000000f7000), // ones count = 7
   static_cast<int64_t>(0x0002000100020001), // ones count = 2
   static_cast<int64_t>(0x8000400080004000), // ones count = 2
   static_cast<int64_t>(0x0800040008000400), // ones count = 2
   /* element size = 16 */
   static_cast<int64_t>(0x7fef7fef7fef7fef), // ones count = 14
   static_cast<int64_t>(0xf7fef7fef7fef7fe), // ones count = 14
   static_cast<int64_t>(0x7eef7eef7eef7eef), // ones count = 13
   static_cast<int64_t>(0x00ef00ef00ef00ef), // ones count = 7
   static_cast<int64_t>(0xf00ef00ef00ef00e), // ones count = 7
   static_cast<int64_t>(0x0ef00ef00ef00ef0), // ones count = 7
   static_cast<int64_t>(0x0201020102010201), // ones count = 2
   static_cast<int64_t>(0x8040804080408040), // ones count = 2
   static_cast<int64_t>(0x0804080408040804), // ones count = 2
   /* element size = 8 */
   static_cast<int64_t>(0x7b7b7b7b7b7b7b7b), // ones count = 6
   static_cast<int64_t>(0xf6f6f6f6f6f6f6f6), // ones count = 6
   static_cast<int64_t>(0x6e6e6e6e6e6e6e6e), // ones count = 5
   static_cast<int64_t>(0x2121212121212121), // ones count = 2
   static_cast<int64_t>(0x8484848484848484), // ones count = 2
   static_cast<int64_t>(0x4242424242424242), // ones count = 2
   };

class ARM64Int32BitwiseOpCodesTest : public OpCodesTest, public ::testing::TestWithParam<int32_t>{};
class ARM64Int64BitwiseOpCodesTest : public OpCodesTest, public ::testing::TestWithParam<int64_t>{};

TEST_P(ARM64Int32BitwiseOpCodesTest, iAnd)
   {

   int32_t intOperand2 = GetParam();
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   int32_t intOperand1 = -1;

   signatureCharII_I_testMethodType * iBitwiseConst = 0;

   sprintf(resolvedMethodName, "iAndConst_TestCase");
   compileOpCodeMethod(iBitwiseConst,
         _numberOfBinaryArgs, TR::iand, resolvedMethodName, _argTypesBinaryInt,
             TR::Int32, rc, 2, 2, &(intOperand2));
   OMR_CT_EXPECT_EQ(iBitwiseConst, tand(intOperand1, intOperand2), iBitwiseConst(intOperand1, INT_PLACEHOLDER_2));
   }

TEST_P(ARM64Int32BitwiseOpCodesTest, iOr)
   {

   int32_t intOperand2 = GetParam();
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   int32_t intOperand1 = -1;

   signatureCharII_I_testMethodType * iBitwiseConst = 0;

   sprintf(resolvedMethodName, "iOrConst_TestCase");
   compileOpCodeMethod(iBitwiseConst,
         _numberOfBinaryArgs, TR::ior, resolvedMethodName, _argTypesBinaryInt,
            TR::Int32, rc, 2, 2, &(intOperand2));
   OMR_CT_EXPECT_EQ(iBitwiseConst, tor(intOperand1, intOperand2), iBitwiseConst(intOperand1, INT_PLACEHOLDER_2));
   }

TEST_P(ARM64Int32BitwiseOpCodesTest, iXor)
   {

   int32_t intOperand2 = GetParam();
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   int32_t intOperand1 = -1;

   signatureCharII_I_testMethodType * iBitwiseConst = 0;

   sprintf(resolvedMethodName, "iXorConst_TestCase");
   compileOpCodeMethod(iBitwiseConst,
         _numberOfBinaryArgs, TR::ixor, resolvedMethodName, _argTypesBinaryInt,
            TR::Int32, rc, 2, 2, &(intOperand2));
   OMR_CT_EXPECT_EQ(iBitwiseConst, txor(intOperand1, intOperand2), iBitwiseConst(intOperand1, INT_PLACEHOLDER_2));
   }

INSTANTIATE_TEST_CASE_P(ARM64Int32BitwiseEncodableImmediatesTest, ARM64Int32BitwiseOpCodesTest,
   ::testing::ValuesIn(int32BitwiseEncodableImmediates));

INSTANTIATE_TEST_CASE_P(ARM64Int32BitwiseUnencodableImmediatesTest, ARM64Int32BitwiseOpCodesTest,
   ::testing::ValuesIn(int32BitwiseUnencodableImmediates));


TEST_P(ARM64Int64BitwiseOpCodesTest, lAnd)
   {

   int64_t longOperand2 = GetParam();
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   int64_t longOperand1 = -1;

   signatureCharJJ_J_testMethodType * lBitwiseConst = 0;

   sprintf(resolvedMethodName, "lAndConst_TestCase");
   compileOpCodeMethod(lBitwiseConst,
         _numberOfBinaryArgs, TR::land, resolvedMethodName, _argTypesBinaryLong,
             TR::Int64, rc, 2, 2, &(longOperand2));
   OMR_CT_EXPECT_EQ(lBitwiseConst, tand(longOperand1, longOperand2), lBitwiseConst(longOperand1, INT_PLACEHOLDER_2));
   }

TEST_P(ARM64Int64BitwiseOpCodesTest, lOr)
   {

   int64_t longOperand2 = GetParam();
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   int64_t longOperand1 = -1;

   signatureCharJJ_J_testMethodType * lBitwiseConst = 0;

   sprintf(resolvedMethodName, "lOrConst_TestCase");
   compileOpCodeMethod(lBitwiseConst,
         _numberOfBinaryArgs, TR::lor, resolvedMethodName, _argTypesBinaryLong,
             TR::Int64, rc, 2, 2, &(longOperand2));
   OMR_CT_EXPECT_EQ(lBitwiseConst, tor(longOperand1, longOperand2), lBitwiseConst(longOperand1, INT_PLACEHOLDER_2));
   }

TEST_P(ARM64Int64BitwiseOpCodesTest, lXor)
   {

   int64_t longOperand2 = GetParam();
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   int64_t longOperand1 = -1;

   signatureCharJJ_J_testMethodType * lBitwiseConst = 0;

   sprintf(resolvedMethodName, "lXorConst_TestCase");
   compileOpCodeMethod(lBitwiseConst,
         _numberOfBinaryArgs, TR::lxor, resolvedMethodName, _argTypesBinaryLong,
             TR::Int64, rc, 2, 2, &(longOperand2));
   OMR_CT_EXPECT_EQ(lBitwiseConst, txor(longOperand1, longOperand2), lBitwiseConst(longOperand1, INT_PLACEHOLDER_2));
   }

INSTANTIATE_TEST_CASE_P(ARM64Int64BitwiseEncodableImmediatesTest, ARM64Int64BitwiseOpCodesTest,
   ::testing::ValuesIn(int64BitwiseEncodableImmediates));

INSTANTIATE_TEST_CASE_P(ARM64Int64BitwiseUnencodableImmediatesTest, ARM64Int64BitwiseOpCodesTest,
   ::testing::ValuesIn(int64BitwiseUnencodableImmediates));
#endif

}
