/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

      NoReg             = 0,
      x0                = 1,
      FirstGPR          = x0,
      x1                = 2,
      x2                = 3,
      x3                = 4,
      x4                = 5,
      x5                = 6,
      x6                = 7,
      x7                = 8,
      x8                = 9,
      x9                = 10,
      x10               = 11,
      x11               = 12,
      x12               = 13,
      x13               = 14,
      x14               = 15,
      x15               = 16,
      x16               = 17,
      x17               = 18,
      x18               = 19,
      x19               = 20,
      x20               = 21,
      x21               = 22,
      x22               = 23,
      x23               = 24,
      x24               = 25,
      x25               = 26,
      x26               = 27,
      x27               = 28,
      x28               = 29,
      x29               = 30,
      x30               = 31,
      lr                = x30,
      LastGPR           = x30,
      LastAssignableGPR = x29,
      sp                = 32,
      xzr               = 33,
      v0                = 34,
      FirstFPR          = v0,
      v1                = 35,
      v2                = 36,
      v3                = 37,
      v4                = 38,
      v5                = 39,
      v6                = 40,
      v7                = 41,
      v8                = 42,
      v9                = 43,
      v10               = 44,
      v11               = 45,
      v12               = 46,
      v13               = 47,
      v14               = 48,
      v15               = 49,
      v16               = 50,
      v17               = 51,
      v18               = 52,
      v19               = 53,
      v20               = 54,
      v21               = 55,
      v22               = 56,
      v23               = 57,
      v24               = 58,
      v25               = 59,
      v26               = 60,
      v27               = 61,
      v28               = 62,
      v29               = 63,
      v30               = 64,
      v31               = 65,
      LastFPR           = v31,
      LastAssignableFPR = v31,
      SpilledReg        = 66,
      NumRegisters      = SpilledReg + 1
