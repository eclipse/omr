/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/*
 * This file will be included within an enum.  Only comments and enumerator
 * definitions are permitted.
 */

      noRegMask = 0x00000000,

#if 0
	/* from PPC version */

      // TR_GPR
      //
      gr0Mask   = 0x00000001,
      gr1Mask   = 0x00000002,
      gr2Mask   = 0x00000004,
      gr3Mask   = 0x00000008,
      gr4Mask   = 0x00000010,
      gr5Mask   = 0x00000020,
      gr6Mask   = 0x00000040,
      gr7Mask   = 0x00000080,
      gr8Mask   = 0x00000100,
      gr9Mask   = 0x00000200,
      gr10Mask  = 0x00000400,
      gr11Mask  = 0x00000800,
      gr12Mask  = 0x00001000,
      gr13Mask  = 0x00002000,
      gr14Mask  = 0x00004000,
      gr15Mask  = 0x00008000,
      gr16Mask  = 0x00010000,
      gr17Mask  = 0x00020000,
      gr18Mask  = 0x00040000,
      gr19Mask  = 0x00080000,
      gr20Mask  = 0x00100000,
      gr21Mask  = 0x00200000,
      gr22Mask  = 0x00400000,
      gr23Mask  = 0x00800000,
      gr24Mask  = 0x01000000,
      gr25Mask  = 0x02000000,
      gr26Mask  = 0x04000000,
      gr27Mask  = 0x08000000,
      gr28Mask  = 0x10000000,
      gr29Mask  = 0x20000000,
      gr30Mask  = 0x40000000,
      gr31Mask  = 0x80000000,
      AvailableGPRMask = 0xffffffff,

      // TR_FPR
      //
      fp0Mask   = 0x00000001,
      fp1Mask   = 0x00000002,
      fp2Mask   = 0x00000004,
      fp3Mask   = 0x00000008,
      fp4Mask   = 0x00000010,
      fp5Mask   = 0x00000020,
      fp6Mask   = 0x00000040,
      fp7Mask   = 0x00000080,
      fp8Mask   = 0x00000100,
      fp9Mask   = 0x00000200,
      fp10Mask  = 0x00000400,
      fp11Mask  = 0x00000800,
      fp12Mask  = 0x00001000,
      fp13Mask  = 0x00002000,
      fp14Mask  = 0x00004000,
      fp15Mask  = 0x00008000,
      fp16Mask  = 0x00010000,
      fp17Mask  = 0x00020000,
      fp18Mask  = 0x00040000,
      fp19Mask  = 0x00080000,
      fp20Mask  = 0x00100000,
      fp21Mask  = 0x00200000,
      fp22Mask  = 0x00400000,
      fp23Mask  = 0x00800000,
      fp24Mask  = 0x01000000,
      fp25Mask  = 0x02000000,
      fp26Mask  = 0x04000000,
      fp27Mask  = 0x08000000,
      fp28Mask  = 0x10000000,
      fp29Mask  = 0x20000000,
      fp30Mask  = 0x40000000,
      fp31Mask  = 0x80000000,
      AvailableFPRMask = 0xffffffff,

      // TR_CCR
      //
      cr0Mask   = 0x00000001,
      cr1Mask   = 0x00000002,
      cr2Mask   = 0x00000004,
      cr3Mask   = 0x00000008,
      cr4Mask   = 0x00000010,
      cr5Mask   = 0x00000020,
      cr6Mask   = 0x00000040,
      cr7Mask   = 0x00000080,
      AvailableCCRMask = 0x000000ff,

      // TR_VRF
      //
      vr0Mask   = 0x00000001,
      vr1Mask   = 0x00000002,
      vr2Mask   = 0x00000004,
      vr3Mask   = 0x00000008,
      vr4Mask   = 0x00000010,
      vr5Mask   = 0x00000020,
      vr6Mask   = 0x00000040,
      vr7Mask   = 0x00000080,
      vr8Mask   = 0x00000100,
      vr9Mask   = 0x00000200,
      vr10Mask  = 0x00000400,
      vr11Mask  = 0x00000800,
      vr12Mask  = 0x00001000,
      vr13Mask  = 0x00002000,
      vr14Mask  = 0x00004000,
      vr15Mask  = 0x00008000,
      vr16Mask  = 0x00010000,
      vr17Mask  = 0x00020000,
      vr18Mask  = 0x00040000,
      vr19Mask  = 0x00080000,
      vr20Mask  = 0x00100000,
      vr21Mask  = 0x00200000,
      vr22Mask  = 0x00400000,
      vr23Mask  = 0x00800000,
      vr24Mask  = 0x01000000,
      vr25Mask  = 0x02000000,
      vr26Mask  = 0x04000000,
      vr27Mask  = 0x08000000,
      vr28Mask  = 0x10000000,
      vr29Mask  = 0x20000000,
      vr30Mask  = 0x40000000,
      vr31Mask  = 0x80000000,
      AvailableVRFMask = 0xffffffff
#endif
