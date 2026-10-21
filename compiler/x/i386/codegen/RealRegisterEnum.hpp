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
    eax = 1, FirstGPR = eax, ebx = 2, ecx = 3, edx = 4, Last8BitGPR = edx, edi = 5, esi = 6, ebp = 7,
    LastAssignableGPR = ebp, esp = 8, LastGPR = esp,

    NumGPRs = LastGPR - FirstGPR + 1,

    // Virtual frame pointer used as a placeholder for frame references before
    // their offsets are known for rsp mapping
    //
    vfp = 9,

    // st0Return is used for 32-bit linkages where floating point values are
    // returned in x87 register st0. It is a special register that cannot be
    // assigned.
    //
    st0Return = 10,

    xmm0 = 11, FirstXMMR = xmm0, xmm1 = 12, xmm2 = 13, xmm3 = 14, xmm4 = 15, xmm5 = 16, xmm6 = 17, xmm7 = 18,
    LastXMMR = xmm7,

    NumXMMRs = LastXMMR - FirstXMMR + 1,

    // Alias ymm registers with xmm registers as they architecturally overlap
    //
    ymm0 = xmm0, ymm1 = xmm1, ymm2 = xmm2, ymm3 = xmm3, ymm4 = xmm4, ymm5 = xmm5, ymm6 = xmm6, ymm7 = xmm7,

    // Alias zmm registers with xmm registers as they architecturally overlap
    //
    zmm0 = xmm0, zmm1 = xmm1, zmm2 = xmm2, zmm3 = xmm3, zmm4 = xmm4, zmm5 = xmm5, zmm6 = xmm6, zmm7 = xmm7,

    // AVX-512 write mask registers
    //
    k0 = 19, FirstVMR = k0, k1 = 20, k2 = 21, k3 = 22, k4 = 23, k5 = 24, k6 = 25, k7 = 26, LastVMR = k7,

    NumVMRs = LastVMR - FirstVMR + 1,

    // Pseudo register indicating any byte register
    //
    ByteReg = 27,

    // Pseudo register indicating a register in spill state
    //
    SpilledReg = 28,

    NumRegisters = 29,
