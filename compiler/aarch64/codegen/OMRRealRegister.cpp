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

#include "codegen/RealRegister.hpp"

const uint8_t OMR::ARM64::RealRegister::fullRegBinaryEncodings[TR::RealRegister::NumRegisters] =
   {
      0x00, // NoReg
      0x00, // x0
      0x01, // x1
      0x02, // x2
      0x03, // x3
      0x04, // x4
      0x05, // x5
      0x06, // x6
      0x07, // x7
      0x08, // x8
      0x09, // x9
      0x0a, // x10
      0x0b, // x11
      0x0c, // x12
      0x0d, // x13
      0x0e, // x14
      0x0f, // x15
      0x10, // x16
      0x11, // x17
      0x12, // x18
      0x13, // x19
      0x14, // x20
      0x15, // x21
      0x16, // x22
      0x17, // x23
      0x18, // x24
      0x19, // x25
      0x1a, // x26
      0x1b, // x27
      0x1c, // x28
      0x1d, // x29
      0x1e, // x30
      0x1f, // sp
      0x1f, // xzr
      0x00, // v0
      0x01, // v1
      0x02, // v2
      0x03, // v3
      0x04, // v4
      0x05, // v5
      0x06, // v6
      0x07, // v7
      0x08, // v8
      0x09, // v9
      0x0a, // v10
      0x0b, // v11
      0x0c, // v12
      0x0d, // v13
      0x0e, // v14
      0x0f, // v15
      0x10, // v16
      0x11, // v17
      0x12, // v18
      0x13, // v19
      0x14, // v20
      0x15, // v21
      0x16, // v22
      0x17, // v23
      0x18, // v24
      0x19, // v25
      0x1a, // v26
      0x1b, // v27
      0x1c, // v28
      0x1d, // v29
      0x1e, // v30
      0x1f, // v31
      0x00  // SpilledReg
   };
