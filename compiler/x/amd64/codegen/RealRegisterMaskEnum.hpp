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

noRegMask = 0x00000000,

    // GPR
    //
    eaxMask = 0x00000001, ebxMask = 0x00000002, ecxMask = 0x00000004, edxMask = 0x00000008, ediMask = 0x00000010,
    esiMask = 0x00000020, ebpMask = 0x00000040, espMask = 0x00000080, r8Mask = 0x00000100, r9Mask = 0x00000200,
    r10Mask = 0x00000400, r11Mask = 0x00000800, r12Mask = 0x00001000, r13Mask = 0x00002000, r14Mask = 0x00004000,
    r15Mask = 0x00008000, r16Mask = 0x00010000, r17Mask = 0x00020000, r18Mask = 0x00040000, r19Mask = 0x00080000,
    r20Mask = 0x00100000, r21Mask = 0x00200000, r22Mask = 0x00400000, r23Mask = 0x00800000, r24Mask = 0x01000000,
    r25Mask = 0x02000000, r26Mask = 0x04000000, r27Mask = 0x08000000, r28Mask = 0x10000000, r29Mask = 0x20000000,
    r30Mask = 0x40000000, r31Mask = 0x80000000, AvailableGPRMask = 0xFFFFFFFF,

    // XMMR
    //
    xmm0Mask = 0x00000001, xmm1Mask = 0x00000002, xmm2Mask = 0x00000004, xmm3Mask = 0x00000008, xmm4Mask = 0x00000010,
    xmm5Mask = 0x00000020, xmm6Mask = 0x00000040, xmm7Mask = 0x00000080, xmm8Mask = 0x00000100, xmm9Mask = 0x00000200,
    xmm10Mask = 0x00000400, xmm11Mask = 0x00000800, xmm12Mask = 0x00001000, xmm13Mask = 0x00002000,
    xmm14Mask = 0x00004000, xmm15Mask = 0x00008000, xmm16Mask = 0x00010000, xmm17Mask = 0x00020000,
    xmm18Mask = 0x00040000, xmm19Mask = 0x00080000, xmm20Mask = 0x00100000, xmm21Mask = 0x00200000,
    xmm22Mask = 0x00400000, xmm23Mask = 0x00800000, xmm24Mask = 0x01000000, xmm25Mask = 0x02000000,
    xmm26Mask = 0x04000000, xmm27Mask = 0x08000000, xmm28Mask = 0x10000000, xmm29Mask = 0x20000000,
    xmm30Mask = 0x40000000, xmm31Mask = 0x80000000, AvailableXMMRMask = 0xFFFFFFFF,

    k0Mask = 0x00000001, k1Mask = 0x00000002, k2Mask = 0x00000004, k3Mask = 0x00000008, k4Mask = 0x00000010,
    k5Mask = 0x00000020, k6Mask = 0x00000040, k7Mask = 0x00000080, AvailableKMask = 0x000000FF,
