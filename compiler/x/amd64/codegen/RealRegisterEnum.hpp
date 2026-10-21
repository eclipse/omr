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

/*
 * This file will be included within an enum.  Only comments and enumerator
 * definitions are permitted.
 */

// Pseudo register indicating any register
//
NoReg = 0,

    // Some consumers of OMR may rely on this register ordering (e.g., for
    // register masks for live references for garbage collection), so be
    // mindful of altering it.
    //
    eax = 1, FirstGPR = eax, ebx = 2, ecx = 3, edx = 4, edi = 5, esi = 6, ebp = 7, esp = 8, r8 = 9, r9 = 10, r10 = 11,
    r11 = 12, r12 = 13, r13 = 14, r14 = 15, r15 = 16,

    // Extended GPRs (APX)
    //
    r16 = 17, r17 = 18, r18 = 19, r19 = 20, r20 = 21, r21 = 22, r22 = 23, r23 = 24, r24 = 25, r25 = 26, r26 = 27,
    r27 = 28, r28 = 29, r29 = 30, r30 = 31, r31 = 32, LastGPR = r31,

    NumGPRs = LastGPR - FirstGPR + 1,

    // Virtual frame pointer used as a placeholder for frame references before
    // their offsets are known for rsp mapping
    //
    vfp = 33,

    // st0Return is used for 32-bit linkages where floating point values are
    // returned in x87 register st0. It is a special register that cannot be
    // assigned.
    //
    st0Return = 34,

    xmm0 = 35, FirstXMMR = xmm0, xmm1 = 36, xmm2 = 37, xmm3 = 38, xmm4 = 39, xmm5 = 40, xmm6 = 41, xmm7 = 42, xmm8 = 43,
    xmm9 = 44, xmm10 = 45, xmm11 = 46, xmm12 = 47, xmm13 = 48, xmm14 = 49, xmm15 = 50,

    // Extended XMM registers (AVX-512F)
    //
    xmm16 = 51, xmm17 = 52, xmm18 = 53, xmm19 = 54, xmm20 = 55, xmm21 = 56, xmm22 = 57, xmm23 = 58, xmm24 = 59,
    xmm25 = 60, xmm26 = 61, xmm27 = 62, xmm28 = 63, xmm29 = 64, xmm30 = 65, xmm31 = 66, LastXMMR = xmm31,

    NumXMMRs = LastXMMR - FirstXMMR + 1,

    // Alias ymm registers with xmm registers as they architecturally overlap
    //
    ymm0 = xmm0, ymm1 = xmm1, ymm2 = xmm2, ymm3 = xmm3, ymm4 = xmm4, ymm5 = xmm5, ymm6 = xmm6, ymm7 = xmm7, ymm8 = xmm8,
    ymm9 = xmm9, ymm10 = xmm10, ymm11 = xmm11, ymm12 = xmm12, ymm13 = xmm13, ymm14 = xmm14, ymm15 = xmm15,
    ymm16 = xmm16, ymm17 = xmm17, ymm18 = xmm18, ymm19 = xmm19, ymm20 = xmm20, ymm21 = xmm21, ymm22 = xmm22,
    ymm23 = xmm23, ymm24 = xmm24, ymm25 = xmm25, ymm26 = xmm26, ymm27 = xmm27, ymm28 = xmm28, ymm29 = xmm29,
    ymm30 = xmm30, ymm31 = xmm31,

    // Alias zmm registers with xmm registers as they architecturally overlap
    //
    zmm0 = xmm0, zmm1 = xmm1, zmm2 = xmm2, zmm3 = xmm3, zmm4 = xmm4, zmm5 = xmm5, zmm6 = xmm6, zmm7 = xmm7, zmm8 = xmm8,
    zmm9 = xmm9, zmm10 = xmm10, zmm11 = xmm11, zmm12 = xmm12, zmm13 = xmm13, zmm14 = xmm14, zmm15 = xmm15,
    zmm16 = xmm16, zmm17 = xmm17, zmm18 = xmm18, zmm19 = xmm19, zmm20 = xmm20, zmm21 = xmm21, zmm22 = xmm22,
    zmm23 = xmm23, zmm24 = xmm24, zmm25 = xmm25, zmm26 = xmm26, zmm27 = xmm27, zmm28 = xmm28, zmm29 = xmm29,
    zmm30 = xmm30, zmm31 = xmm31,

    // AVX-512 write mask registers
    //
    k0 = 67, FirstVMR = k0, k1 = 68, k2 = 69, k3 = 70, k4 = 71, k5 = 72, k6 = 73, k7 = 74, LastVMR = k7,

    NumVMRs = LastVMR - FirstVMR + 1,

    // Pseudo register indicating any byte register
    //
    ByteReg = 75,

    // Pseudo register indicating a register in spill state
    //
    SpilledReg = 76,

    NumRegisters = 77,
