/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "x/codegen/X86Ops.hpp"

 // Heuristics for X87 second byte opcode
 // It could be eliminated if GCC/MSVC fully support initializer list
#define X87_________________(x) ((uint8_t)((x & 0xE0) >> 5)), ((uint8_t)((x & 0x18) >> 3)), (uint8_t)(x & 0x07)
#define BINARY(...) {__VA_ARGS__}
#define PROPERTY0(...) __VA_ARGS__
#define PROPERTY1(...) __VA_ARGS__

// see compiler/x/codegen/OMRInstruction.hpp for structural information.
const TR_X86OpCode::OpCode_t TR_X86OpCode::_binaries[] =
   {
#define INSTRUCTION(name, mnemonic, binary, property0, property1) binary
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
   };

const uint32_t TR_X86OpCode::_properties[] =
   {
#define INSTRUCTION(name, mnemonic, binary, property0, property1) property0
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
   };

const uint32_t TR_X86OpCode::_properties1[] =
   {
#define INSTRUCTION(name, mnemonic, binary, property0, property1) property1
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
   };
