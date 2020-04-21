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

#include "il/TreeTop.hpp"
#include "compiler/il/OMRTreeTop_inlines.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/ARMInstruction.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/TreeEvaluator.hpp"
#include "infra/Bit.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "il/Block.hpp"
#include "codegen/ARMOperand2.hpp"


// the operandEncodingTable contains 2 bits of info:
//  - the "family" of operand
//  - the bit definition for that specific operand
//  mask with OP2_TYPE_MASK to get the family,
//  OP2_ENCODING_MASK to get the encoding

#define OP2_TYPE_MASK 0x000F0000

typedef enum {
   Op2Illegal = 1 << 16,
   Op2Immed = 2 << 16,
   Op2Reg = 3 << 16,
   Op2RegImmed = 4 << 16,
   Op2RegReg = 5 << 16
} TR_ARMOperand2EncodingType;

#define OP2_ENCODING_MASK 0x00000FFF

typedef enum {
   Op2ShiftByReg = 0x1 << 4,
   Op2LSL = 0x0 << 5,
   Op2LSR = 0x1 << 5,
   Op2ASR = 0x2 << 5,
   Op2ROR = 0x3 << 5,
} TR_ARMOperand2ShiftEncoding;

static uint32_t operandEncodingTable[] =
   {
   0x000 | Op2Illegal,                    // ARMOp2Illegal,
   0x000 | Op2Immed,                      // ARMOp2Immed8r,
   Op2LSL | Op2RegImmed,                  // ARMOp2RegLSLImmed,
   Op2LSR | Op2RegImmed,                  // ARMOp2RegLSRImmed,
   Op2ASR | Op2RegImmed,                  // ARMOp2RegASRImmed,
   Op2ROR | Op2RegImmed,                  // ARMOp2RegRORImmed,
   0x000 | Op2Reg,                        // ARMOp2Reg,
   Op2ROR | Op2Reg,                       // ARMOp2RegRRX,
   Op2LSL | Op2ShiftByReg | Op2RegReg,    // ARMOp2RegLSLReg,
   Op2LSR | Op2ShiftByReg | Op2RegReg,    // ARMOp2RegLSRReg,
   Op2ASR | Op2ShiftByReg | Op2RegReg,    // ARMOp2RegASRReg,
   Op2ROR | Op2ShiftByReg | Op2RegReg     // ARMOp2RegRORReg
   };

void TR_ARMOperand2::incTotalUseCount()
   {
   switch(_opType)
      {
      case ARMOp2RegLSLReg:
      case ARMOp2RegLSRReg:
      case ARMOp2RegASRReg:
      case ARMOp2RegRORReg:
         _shiftRegister->incTotalUseCount();
               // fall through to get base register case.

      case ARMOp2RegLSLImmed:
      case ARMOp2RegLSRImmed:
      case ARMOp2RegASRImmed:
      case ARMOp2RegRORImmed:
      case ARMOp2Reg:
      case ARMOp2RegRRX:
         _baseRegister->incTotalUseCount();
         break;
      }
   }

void TR_ARMOperand2::setBinaryEncoding(uint32_t *instruction)
   {
   if (ARMOp2Immed8r == _opType)
      {
      // the shift is actually encoded as rotate right by n bits
      *instruction |= (_immedShift  ? 16 - (_immedShift >> 1) : 0) << 8;
      *instruction |= (_shiftOrImmed & 0xFF); // encode lowest 8 bits only
      *instruction |= 1 << 25; // set the I bit.
      }
   else
      {
      // set Rm and shift type for all following.
      TR::RealRegister *realBaseReg = toRealRegister(_baseRegister);
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      if(isSinglePrecision(realBaseReg->getRegisterNumber()))
         {
#if DEBUG_FPN
    	  printf("RealBaseReg num=%d\n", realBaseReg->getRegisterNumber());
          TR_ASSERT(false, "Should not occur.");
#endif
         realBaseReg->setRegisterFieldSM(instruction);
         }
      else
         {
         realBaseReg->setRegisterFieldRM(instruction);
         }
#else
      realBaseReg->setRegisterFieldRM(instruction);
#endif

      *instruction |= operandEncodingTable[(uint32_t) _opType] & OP2_ENCODING_MASK;

      uint32_t opType = operandEncodingTable[(uint32_t) _opType] & OP2_TYPE_MASK;

      switch(opType)
         {
         case Op2RegImmed:
            *instruction |= _shiftOrImmed << 7;
            break;
         case Op2RegReg:
            {
            TR::RealRegister *realShiftReg = toRealRegister(_shiftRegister);
            realShiftReg->setRegisterFieldRS(instruction);
            break;
            }
         default:
            break;
         }
      }
   }

bool TR_ARMOperand2::refsRegister(TR::Register *reg)
   {
   switch(_opType)
      {
      case ARMOp2RegLSLReg:
      case ARMOp2RegLSRReg:
      case ARMOp2RegASRReg:
      case ARMOp2RegRORReg:
         if(_shiftRegister == reg)
            {
            return true;
            }
               // fall through to get base register case.

      case ARMOp2RegLSLImmed:
      case ARMOp2RegLSRImmed:
      case ARMOp2RegASRImmed:
      case ARMOp2RegRORImmed:
      case ARMOp2Reg:
      case ARMOp2RegRRX:
         if(_baseRegister == reg)
            {
            return true;
            }
      default:
         return false;
      }
   }

void TR_ARMOperand2::block()
{
   switch(_opType)
   {
      case ARMOp2RegLSLReg:
      case ARMOp2RegLSRReg:
      case ARMOp2RegASRReg:
      case ARMOp2RegRORReg:
         _shiftRegister->block();
               // fall through to get base register case.

      case ARMOp2RegLSLImmed:
      case ARMOp2RegLSRImmed:
      case ARMOp2RegASRImmed:
      case ARMOp2RegRORImmed:
      case ARMOp2Reg:
      case ARMOp2RegRRX:
         _baseRegister->block();
   }
}

void TR_ARMOperand2::unblock()
{
   switch(_opType)
   {
      case ARMOp2RegLSLReg:
      case ARMOp2RegLSRReg:
      case ARMOp2RegASRReg:
      case ARMOp2RegRORReg:
         _shiftRegister->unblock();
               // fall through to get base register case.

      case ARMOp2RegLSLImmed:
      case ARMOp2RegLSRImmed:
      case ARMOp2RegASRImmed:
      case ARMOp2RegRORImmed:
      case ARMOp2Reg:
      case ARMOp2RegRRX:
         _baseRegister->unblock();
   }
}

void TR_ARMOperand2::decFutureUseCountAndAdjustRegState()
{
   switch(_opType)
   {
      case ARMOp2RegLSLReg:
      case ARMOp2RegLSRReg:
      case ARMOp2RegASRReg:
      case ARMOp2RegRORReg:
         if (_shiftRegister->decFutureUseCount() == 0)
         {
            _shiftRegister->getAssignedRealRegister()->setState(TR::RealRegister::Unlatched);
            _shiftRegister->setAssignedRegister(NULL);
         }
               // fall through to get base register case.

      case ARMOp2RegLSLImmed:
      case ARMOp2RegLSRImmed:
      case ARMOp2RegASRImmed:
      case ARMOp2RegRORImmed:
      case ARMOp2Reg:
      case ARMOp2RegRRX:
         if (_baseRegister->decFutureUseCount() == 0)
         {
            _baseRegister->getAssignedRealRegister()->setState(TR::RealRegister::Unlatched);
            _baseRegister->setAssignedRegister(NULL);
         }
   }
}

TR_ARMOperand2 *TR_ARMOperand2::assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
{
   switch(_opType)
   {
      case ARMOp2RegLSLReg:
      case ARMOp2RegLSRReg:
      case ARMOp2RegASRReg:
      case ARMOp2RegRORReg:
         _baseRegister->block();
         _shiftRegister = cg->machine()->assignSingleRegister(_shiftRegister, currentInstruction);
         _baseRegister->unblock();

         _shiftRegister->block();
         _baseRegister = cg->machine()->assignSingleRegister(_baseRegister, currentInstruction);
         _shiftRegister->unblock();
         break;

      case ARMOp2RegLSLImmed:
      case ARMOp2RegLSRImmed:
      case ARMOp2RegASRImmed:
      case ARMOp2RegRORImmed:
      case ARMOp2Reg:
      case ARMOp2RegRRX:
         _baseRegister = cg->machine()->assignSingleRegister(_baseRegister, currentInstruction);
         break;
   }
   return this;
}
