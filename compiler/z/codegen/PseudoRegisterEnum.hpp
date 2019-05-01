/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

      EvenOddPair         = LastVRF + 1,      // Assign an even/odd pair to the reg pair
      LegalEvenOfPair     = LastVRF + 2,      // Assign an even reg that is followed by an unlocked odd register
      LegalOddOfPair      = LastVRF + 3,      // Assign an odd reg that is preceded by an unlocked even register
      FPPair              = LastVRF + 4,      // Assign an FP pair to the reg pair
      LegalFirstOfFPPair  = LastVRF + 5,      // Assign first FP reg of a FP reg Pair
      LegalSecondOfFPPair = LastVRF + 6,      // Assign second FP reg of a FP reg Pair
      AssignAny           = LastVRF + 7,      // Assign any register
      KillVolHighRegs     = LastVRF + 8,      // Kill all volatile access regs
      MayDefine           = LastVRF + 9,      // This instruction's result should be modelled as live before as this instruction only 'may defines' the register
      SpilledReg          = LastVRF + 10,     // OOL: Any Spilled register cross OOL sequences

      NumRegisters        = LastVRF + 1    // (include noReg)

