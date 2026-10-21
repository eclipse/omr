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

#include "codegen/RealRegister.hpp"

const OMR::X86::RealRegister::RegisterBinaryEncoding
    OMR::X86::I386::RealRegister::_fullRegisterBinaryEncodings[TR::RealRegister::NumRegisters]
    = {
          // clang-format off
          // ----------- id[3]
          // |    --------- vvvv[4] (1's complement)
          // |    |   -------- needsRXBV4[1]
          // |    |   |  ------- needsRexPlusRXB[1]
          // |    |   |  |  ------ needsRexForByte[1]
          // |    |   |  |  |  ----- needsDisp[1]
          // |    |   |  |  |  |  ---- needsSIB[1]
          // |    |   |  |  |  |  |
          // |    |   |  |  |  |  |
          // V    V   V  V  V  V  V
          { 0x0                     }, // NoReg
          { 0x0, 0x0, 0, 0, 0, 0, 0 }, // eax
          { 0x3, 0x0, 0, 0, 0       }, // ebx
          { 0x1, 0x0, 0, 0, 0       }, // ecx
          { 0x2, 0x0, 0, 0, 0       }, // edx
          { 0x7, 0x0, 0, 0, 1       }, // edi
          { 0x6, 0x0, 0, 0, 1       }, // esi
          { 0x5, 0x0, 0, 0, 1, 1, 0 }, // ebp
          { 0x4, 0x0, 0, 0, 1, 0, 1 }, // esp
          { 0x0                     }, // vfp
          { 0x0                     }, // st0Return
          { 0x0, 0x0, 0, 0, 0       }, // xmm0
          { 0x1, 0x0, 0, 0, 0       }, // xmm1
          { 0x2, 0x0, 0, 0, 0       }, // xmm2
          { 0x3, 0x0, 0, 0, 0       }, // xmm3
          { 0x4, 0x0, 0, 0, 0       }, // xmm4
          { 0x5, 0x0, 0, 0, 0       }, // xmm5
          { 0x6, 0x0, 0, 0, 0       }, // xmm6
          { 0x7, 0x0, 0, 0, 0       }, // xmm7
          { 0x0                     }, // k0
          { 0x1                     }, // k1
          { 0x2                     }, // k2
          { 0x3                     }, // k3
          { 0x4                     }, // k4
          { 0x5                     }, // k5
          { 0x6                     }, // k6
          { 0x7                     }, // k7
                 // clang-format on
};
