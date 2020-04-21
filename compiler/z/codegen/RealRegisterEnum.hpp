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

/*
 * This file will be included within an enum.  Only comments and enumerator
 * definitions are permitted.
 */


      /* Note: For a given register X, enum value is offset + X + 1 */
      NoReg             = 0,
      GPRBase           = NoReg,

      GPR0              = GPRBase + 1,
      GPR1              = GPRBase + 2,
      GPR2              = GPRBase + 3,
      GPR3              = GPRBase + 4,
      GPR4              = GPRBase + 5,
      GPR5              = GPRBase + 6,
      GPR6              = GPRBase + 7,
      GPR7              = GPRBase + 8,
      GPR8              = GPRBase + 9,
      GPR9              = GPRBase + 10,
      GPR10             = GPRBase + 11,
      GPR11             = GPRBase + 12,
      GPR12             = GPRBase + 13,
      GPR13             = GPRBase + 14,
      GPR14             = GPRBase + 15,
      GPR15             = GPRBase + 16,

      FirstGPR          = GPR0,
      LastGPR           = GPR15,
      LastAssignableGPR = GPR15,
      FPRBase           = LastGPR,

      FPR0              = FPRBase + 1,
      FPR1              = FPRBase + 2,
      FPR2              = FPRBase + 3,
      FPR3              = FPRBase + 4,
      FPR4              = FPRBase + 5,
      FPR5              = FPRBase + 6,
      FPR6              = FPRBase + 7,
      FPR7              = FPRBase + 8,
      FPR8              = FPRBase + 9,
      FPR9              = FPRBase + 10,
      FPR10             = FPRBase + 11,
      FPR11             = FPRBase + 12,
      FPR12             = FPRBase + 13,
      FPR13             = FPRBase + 14,
      FPR14             = FPRBase + 15,
      FPR15             = FPRBase + 16,

      FirstFPR          = FPR0,
      LastFPR           = FPR15,
      LastAssignableFPR = FPR15,

      VRFBase           = FPRBase, /* The base is same because VRF0-15 overlap with FPR0-15 */

      VRF0              = VRFBase + 1,
      VRF1              = VRFBase + 2,
      VRF2              = VRFBase + 3,
      VRF3              = VRFBase + 4,
      VRF4              = VRFBase + 5,
      VRF5              = VRFBase + 6,
      VRF6              = VRFBase + 7,
      VRF7              = VRFBase + 8,
      VRF8              = VRFBase + 9,
      VRF9              = VRFBase + 10,
      VRF10             = VRFBase + 11,
      VRF11             = VRFBase + 12,
      VRF12             = VRFBase + 13,
      VRF13             = VRFBase + 14,
      VRF14             = VRFBase + 15,
      VRF15             = VRFBase + 16,
      VRF16             = VRFBase + 17,
      VRF17             = VRFBase + 18,
      VRF18             = VRFBase + 19,
      VRF19             = VRFBase + 20,
      VRF20             = VRFBase + 21,
      VRF21             = VRFBase + 22,
      VRF22             = VRFBase + 23,
      VRF23             = VRFBase + 24,
      VRF24             = VRFBase + 25,
      VRF25             = VRFBase + 26,
      VRF26             = VRFBase + 27,
      VRF27             = VRFBase + 28,
      VRF28             = VRFBase + 29,
      VRF29             = VRFBase + 30,
      VRF30             = VRFBase + 31,
      VRF31             = VRFBase + 32,

      FirstVRF          = VRF0,
      LastVRF           = VRF31,
      FirstAssignableVRF= VRF0, /* Need this until we complete overlapping FPR/VRF implementation */
      LastAssignableVRF = VRF31,
      FirstOverlapVRF   = VRF0,
      LastOverlapVRF    = VRF15
