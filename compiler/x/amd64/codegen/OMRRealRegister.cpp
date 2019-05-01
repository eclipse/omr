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

#include "codegen/RealRegister.hpp"

const OMR::X86::RealRegister::TR_RegisterBinaryEncoding OMR::X86::AMD64::RealRegister::_fullRegisterBinaryEncodings[TR::RealRegister::NumRegisters] =
   {
   // -----------id
   // |   --------needsRexPlusRXB
   // |   |  ------needsRexForByte
   // |   |  |  ----needsDisp
   // |   |  |  |  --needsSib
   // |   |  |  |  |
   // V   V  V  V  V
   {0x00, 0, 0},        // NoReg
   {0x00, 0, 0},        // eax
   {0x03, 0, 0},        // ebx
   {0x01, 0, 0},        // ecx
   {0x02, 0, 0},        // edx
   {0x07, 0, 1},        // edi
   {0x06, 0, 1},        // esi
   {0x05, 0, 1, 1, 0},  // ebp
   {0x04, 0, 1, 0, 1},  // esp
   {0x00, 1, 1},        // r8
   {0x01, 1, 1},        // r9
   {0x02, 1, 1},        // r10
   {0x03, 1, 1},        // r11
   {0x04, 1, 1, 0, 1},  // r12
   {0x05, 1, 1, 1, 0},  // r13
   {0x06, 1, 1},        // r14
   {0x07, 1, 1},        // r15
   {0x00},              // vfp
   {0x00},              // st0
   {0x01},              // st1
   {0x02},              // st2
   {0x03},              // st3
   {0x04},              // st4
   {0x05},              // st6
   {0x06},              // st6
   {0x07},              // st7
   {0x00},              // mm0
   {0x01},              // mm1
   {0x02},              // mm2
   {0x03},              // mm3
   {0x04},              // mm4
   {0x05},              // mm6
   {0x06},              // mm6
   {0x07},              // mm7
   {0x00, 0, 0},        // xmm0
   {0x01, 0, 0},        // xmm1
   {0x02, 0, 0},        // xmm2
   {0x03, 0, 0},        // xmm3
   {0x04, 0, 0},        // xmm4
   {0x05, 0, 0},        // xmm6
   {0x06, 0, 0},        // xmm6
   {0x07, 0, 0},        // xmm7
   {0x00, 1, 1},        // xmm8
   {0x01, 1, 1},        // xmm9
   {0x02, 1, 1},        // xmm10
   {0x03, 1, 1},        // xmm11
   {0x04, 1, 1},        // xmm12
   {0x05, 1, 1},        // xmm14
   {0x06, 1, 1},        // xmm14
   {0x07, 1, 1},        // xmm15
   };
