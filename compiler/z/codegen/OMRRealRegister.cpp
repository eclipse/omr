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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"TRZRealRegBase#C")
#pragma csect(STATIC,"TRZRealRegtBase#S")
#pragma csect(TEST,"TRZRealRegBase#T")

#include <stddef.h>
#include <stdint.h>
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "infra/Assert.hpp"
#include "z/codegen/EndianConversion.hpp"

namespace TR { class CodeGenerator; }

OMR::Z::RealRegister::RealRegister(TR::CodeGenerator *cg):
      OMR::RealRegister(cg, NoReg),
      _modified(0) {}

OMR::Z::RealRegister::RealRegister(TR_RegisterKinds rk, uint16_t w, RegState s,
                                          RegNum rn, RegMask m, TR::CodeGenerator* cg):
      OMR::RealRegister(rk, w, s, (uint16_t)rn, rn, m, cg),
      _modified(0) {}


void
OMR::Z::RealRegister::setBaseRegisterField(uint32_t *instruction)
   {
   TR::RealRegister::setBaseRegisterField(instruction,_registerNumber);
   }

void
OMR::Z::RealRegister::setIndexRegisterField(uint32_t *instruction)
   {
   TR::RealRegister::setIndexRegisterField(instruction,_registerNumber);
   }

void
OMR::Z::RealRegister::setRegisterField(uint32_t *instruction)
   {
   TR::RealRegister::setRegisterField(instruction,_registerNumber);
   }

// nibbleIndex-> rightmost nibble=0, leftmost nibble=7
void
OMR::Z::RealRegister::setRegisterField(uint32_t *instruction, int32_t nibbleIndex)
   {
   TR::RealRegister::setRegisterField(instruction,nibbleIndex,_registerNumber);
   }

void
OMR::Z::RealRegister::setRegister1Field(uint32_t *instruction)
   {
   self()->setRegisterField(instruction);
   }

void
OMR::Z::RealRegister::setRegister2Field(uint32_t *instruction)
   {
   self()->setIndexRegisterField(instruction);
   }

void
OMR::Z::RealRegister::setRegister3Field(uint32_t *instruction)
   {
   self()->setBaseRegisterField(instruction);
   }

void
OMR::Z::RealRegister::setRegister4Field(uint32_t *instruction)
   {
   TR::RealRegister::setRegister4Field(instruction, _registerNumber);
   }

bool
OMR::Z::RealRegister::setHasBeenAssignedInMethod(bool b)
   {
   return OMR::RealRegister::setHasBeenAssignedInMethod(b); //call base class
   }


/** \details
 *
 * Static method that sets the RXB field for vector register operands.
 *
 * Only VRF Register can set this RXB field. When a vector register acts as operands, its
 * register binary encoding's most significant bit is stored in 1 of 4 RXB bits.
 *
 * The RXB field is in the lower 4 bits of fifth byte (i.e. bit 36-39 of an instructions)
 *
 */
void
OMR::Z::RealRegister::setRegisterRXBField(uint32_t *instruction, RegNum reg, int index)
   {
   TR_ASSERT(TR::RealRegister::isVRF(reg), "Only VRF Register has RXB field.");
   TR_ASSERT(index >= 1 && index <= 4, "index out of range.");

   uint32_t RXB = boi( (_fullRegBinaryEncodings[reg] & 0x10) >> 4 << (4 - index) << 24 );
   *(instruction + 1) |= RXB;
   }

/** \details
 *
 * Static method set base register field.
 * The 'base register field' corresponds to bit 16-19 of an instruction, which is
 * where the 3rd field for register operands
 *
 * Also sets the 3rd bit of RXB in case the 3rd field refers to a VRF register
 *
*/
void
OMR::Z::RealRegister::setBaseRegisterField(uint32_t *instruction,RegNum reg)
   {
   *instruction &= boi(0xFFFF0FFF); // clear out bit 16-19 of the instruction memory
   *instruction |= boi( (_fullRegBinaryEncodings[reg] & 0x0f) << 12);
   if (TR::RealRegister::isVRF(reg))
      TR::RealRegister::setRegisterRXBField(instruction, reg, 3); // base reg is in the 3rd register field
   }

/** \details
 *
 * Static Set Index Register Field.
 * The index register field corresponds to bit 12-15 of an instruction, which is
 * where the 2nd field for register operands
 *
 * Slso sets the 2nd bit of RXB in case bit 12-15 refers to an VRF register
 *
*/
void
OMR::Z::RealRegister::setIndexRegisterField(uint32_t *instruction,RegNum reg)
   {
   *instruction &= boi(0xFFF0FFFF); // clear out bit 12-15 of the instruction memory
   *instruction |= boi( (_fullRegBinaryEncodings[reg] & 0x0f) << 16);
   if (TR::RealRegister::isVRF(reg))
      TR::RealRegister::setRegisterRXBField(instruction, reg, 2); // index reg is in the 2nd register field
   }

/** \details
 *
 * Static method that sets register field
 * The register field is bit 8-11 of an instruction, which is where the 1st field for register operands
 *
 * Also sets the 1nd bit of RXB in case the 1st field is a VRF register
*/
void
OMR::Z::RealRegister::setRegisterField(uint32_t *instruction,RegNum reg)
   {
   *instruction &= boi(0xFF0FFFFF); // clear out bit 8-11 of the instruction memory
   *instruction |= boi( (_fullRegBinaryEncodings[reg] & 0x0f) << 20);
   if (TR::RealRegister::isVRF(reg))
      TR::RealRegister::setRegisterRXBField(instruction, reg, 1); // 1st register field
   }

/** \details
 *
 * Set register field with nibble
*/
void
OMR::Z::RealRegister::setRegisterField(uint32_t *instruction, int32_t nibbleIndex,RegNum reg)
   {
   TR_ASSERT(nibbleIndex>=0 && nibbleIndex<=7,
     "OMR::Z::RealRegister::RealRegister: bad nibbleIndex\n");

   uint32_t maskInstruction = *instruction&(0xF<<nibbleIndex*4);
   *instruction ^= boi(maskInstruction); // clear out the memory of byte
   *instruction |= boi((_fullRegBinaryEncodings[reg] & 0x0f) << nibbleIndex*4);
   }

/** \details
 *
 * Static method that sets register1 field
*/
void
OMR::Z::RealRegister::setRegister1Field(uint32_t *instruction,RegNum reg)
   {
   TR::RealRegister::setRegisterField(instruction,reg);
   }


/** \details
 *
 * Static method that sets register2 field (also refered to as the index register)
*/
void
OMR::Z::RealRegister::setRegister2Field(uint32_t *instruction,RegNum reg)
   {
   TR::RealRegister::setIndexRegisterField(instruction,reg);
   }

/** \details
 *
 * Static method that sets register3 field, which is also refered to as the base register at bit 16-19
*/
void
OMR::Z::RealRegister::setRegister3Field(uint32_t *instruction,RegNum reg)
   {
   TR::RealRegister::setBaseRegisterField(instruction,reg);
   }

/** \details
 *
 * A static method that sets register4 field
 * The 4th reg field is in the 1st 4-bit of the 3rd halfword (bit 32-35) of the instruction
 *
*/
void
OMR::Z::RealRegister::setRegister4Field(uint32_t *instruction,RegNum reg)
   {
   TR_ASSERT(TR::RealRegister::isVRF(reg), "setRegister4Field is only for vector registers");
   TR::RealRegister::setRegisterField(instruction + 1, 7, reg); // the 4th reg field is in the 1st 4-bit of the 3rd halfword of the instuction
   TR::RealRegister::setRegisterRXBField(instruction, reg, 4);
   }

// static method
bool
OMR::Z::RealRegister::isPseudoRealReg(RegNum reg)
  {
  return (reg < FirstGPR || reg > LastVRF);
  }

// static method
bool
OMR::Z::RealRegister::isRealReg(RegNum reg)
   {
   return (reg >= FirstGPR && reg <= LastVRF);
   }

// static method
bool
OMR::Z::RealRegister::isGPR(RegNum reg)
   {
   if (reg >= FirstGPR && reg <= LastAssignableGPR)
       return true;
   else
      return false;
   }

// static method
bool
OMR::Z::RealRegister::isFPR(RegNum reg)
   {
   if(reg >= FirstFPR && reg <= LastAssignableFPR)
     return true;
   else
     return false;
   }

// static method
bool
OMR::Z::RealRegister::isVRF(RegNum reg)
   {
   if(reg >= FirstVRF && reg <= LastVRF)
      return true;
   else
      return false;
   }

const uint8_t OMR::Z::RealRegister::_fullRegBinaryEncodings[TR::RealRegister::NumRegisters] =
   {
   0x00,        // NoReg
   0x00,        // GPR0
   0x01,        // GPR1
   0x02,        // GPR2
   0x03,        // GPR3
   0x04,        // GPR4
   0x05,        // GPR5
   0x06,        // GPR6
   0x07,        // GPR7
   0x08,        // GPR8
   0x09,        // GPR9
   0x0A,        // GPR10
   0x0B,        // GPR11
   0x0C,        // GPR12
   0x0D,        // GPR13
   0x0E,        // GPR14
   0x0F,        // GPR15
   0x00,        // FPR0
   0x01,        // FPR1
   0x02,        // FPR2
   0x03,        // FPR3
   0x04,        // FPR4
   0x05,        // FPR5
   0x06,        // FPR6
   0x07,        // FPR7
   0x08,        // FPR8
   0x09,        // FPR9
   0x0A,        // FPR10
   0x0B,        // FPR11
   0x0C,        // FPR12
   0x0D,        // FPR13
   0x0E,        // FPR14
   0x0F,        // FPR15
/* VRF0-15 initialized same as FPR */
/*
   0x00,        // VRF0
   0x01,        // VRF1
   0x02,        // VRF2
   0x03,        // VRF3
   0x04,        // VRF4
   0x05,        // VRF5
   0x06,        // VRF6
   0x07,        // VRF7
   0x08,        // VRF8
   0x09,        // VRF9
   0x0A,        // VRF10
   0x0B,        // VRF11
   0x0C,        // VRF12
   0x0D,        // VRF13
   0x0E,        // VRF14
   0x0F,        // VRF15
*/
   0x10,        // VRF16
   0x11,        // VRF17
   0x12,        // VRF18
   0x13,        // VRF19
   0x14,        // VRF20
   0x15,        // VRF21
   0x16,        // VRF22
   0x17,        // VRF23
   0x18,        // VRF24
   0x19,        // VRF25
   0x1A,        // VRF26
   0x1B,        // VRF27
   0x1C,        // VRF28
   0x1D,        // VRF29
   0x1E,        // VRF30
   0x1F,        // VRF31
   };
