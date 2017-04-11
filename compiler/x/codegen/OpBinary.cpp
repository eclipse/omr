/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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
