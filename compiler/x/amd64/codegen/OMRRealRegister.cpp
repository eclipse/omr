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
    OMR::X86::AMD64::RealRegister::_fullRegisterBinaryEncodings[TR::RealRegister::NumRegisters]
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
          { 0x0, 0xf, 0, 0, 0, 0, 0 }, // rax
          { 0x3, 0xc, 0, 0, 0, 0, 0 }, // rbx
          { 0x1, 0xe, 0, 0, 0, 0, 0 }, // rcx
          { 0x2, 0xd, 0, 0, 0, 0, 0 }, // rdx
          { 0x7, 0x8, 0, 0, 1       }, // rdi
          { 0x6, 0x9, 0, 0, 1       }, // rsi
          { 0x5, 0xa, 0, 0, 1, 1, 0 }, // rbp
          { 0x4, 0xb, 0, 0, 1, 0, 1 }, // rsp
          { 0x0, 0x7, 0, 1, 1       }, // r8
          { 0x1, 0x6, 0, 1, 1       }, // r9
          { 0x2, 0x5, 0, 1, 1       }, // r10
          { 0x3, 0x4, 0, 1, 1       }, // r11
          { 0x4, 0x3, 0, 1, 1, 0, 1 }, // r12
          { 0x5, 0x2, 0, 1, 1, 1, 0 }, // r13
          { 0x6, 0x1, 0, 1, 1       }, // r14
          { 0x7, 0x0, 0, 1, 1       }, // r15
          { 0x0, 0xf, 1, 0, 1       }, // r16
          { 0x1, 0xe, 1, 0, 1       }, // r17
          { 0x2, 0xd, 1, 0, 1       }, // r18
          { 0x3, 0xc, 1, 0, 1       }, // r19
          { 0x4, 0xb, 1, 0, 1       }, // r20
          { 0x5, 0xa, 1, 0, 1       }, // r21
          { 0x6, 0x9, 1, 0, 1       }, // r22
          { 0x7, 0x8, 1, 0, 1       }, // r23
          { 0x0, 0x7, 1, 1, 1       }, // r24
          { 0x1, 0x6, 1, 1, 1       }, // r25
          { 0x2, 0x5, 1, 1, 1       }, // r26
          { 0x3, 0x4, 1, 1, 1       }, // r27
          { 0x4, 0x3, 1, 1, 1       }, // r28
          { 0x5, 0x2, 1, 1, 1       }, // r29
          { 0x6, 0x1, 1, 1, 1       }, // r30
          { 0x7, 0x0, 1, 1, 1       }, // r31
          { 0x0                     }, // vfp
          { 0x0                     }, // st0Return
          { 0x0, 0xf, 0, 0          }, // xmm0
          { 0x1, 0xe, 0, 0          }, // xmm1
          { 0x2, 0xd, 0, 0          }, // xmm2
          { 0x3, 0xc, 0, 0          }, // xmm3
          { 0x4, 0xb, 0, 0          }, // xmm4
          { 0x5, 0xa, 0, 0          }, // xmm5
          { 0x6, 0x9, 0, 0          }, // xmm6
          { 0x7, 0x8, 0, 0          }, // xmm7
          { 0x0, 0x7, 0, 1          }, // xmm8
          { 0x1, 0x6, 0, 1          }, // xmm9
          { 0x2, 0x5, 0, 1          }, // xmm10
          { 0x3, 0x4, 0, 1          }, // xmm11
          { 0x4, 0x3, 0, 1          }, // xmm12
          { 0x5, 0x2, 0, 1          }, // xmm13
          { 0x6, 0x1, 0, 1          }, // xmm14
          { 0x7, 0x0, 0, 1          }, // xmm15
          { 0x0, 0xf, 1, 0          }, // xmm16
          { 0x1, 0xe, 1, 0          }, // xmm17
          { 0x2, 0xd, 1, 0          }, // xmm18
          { 0x3, 0xc, 1, 0          }, // xmm19
          { 0x4, 0xb, 1, 0          }, // xmm20
          { 0x5, 0xa, 1, 0          }, // xmm21
          { 0x6, 0x9, 1, 0          }, // xmm22
          { 0x7, 0x8, 1, 0          }, // xmm23
          { 0x0, 0x7, 1, 1          }, // xmm24
          { 0x1, 0x6, 1, 1          }, // xmm25
          { 0x2, 0x5, 1, 1          }, // xmm26
          { 0x3, 0x4, 1, 1          }, // xmm27
          { 0x4, 0x3, 1, 1          }, // xmm28
          { 0x5, 0x2, 1, 1          }, // xmm29
          { 0x6, 0x1, 1, 1          }, // xmm30
          { 0x7, 0x0, 1, 1          }, // xmm31
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
