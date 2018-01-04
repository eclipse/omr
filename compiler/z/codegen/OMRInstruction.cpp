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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"OMRZInstBase#C")
#pragma csect(STATIC,"OMRZInstBase#S")
#pragma csect(TEST,"OMRZInstBase#T")

#include "codegen/Instruction.hpp"

#include <stdint.h>                                // for int32_t
#include <stdio.h>                                 // for sprintf
#include <string.h>                                // for NULL, strlen, etc
#include "codegen/CodeGenPhase.hpp"                // for CodeGenPhase, etc
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                    // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/Instruction.hpp"                 // for Instruction, etc
#include "codegen/Linkage.hpp"                     // for Linkage, etc
#include "codegen/Machine.hpp"                     // for Machine, etc
#include "codegen/MemoryReference.hpp"             // for MemoryReference
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                // for RegisterPair
#include "compile/Compilation.hpp"                 // for Compilation, comp
#include "compile/Method.hpp"                      // for TR_Method
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/arrayof.h"                           // for ArrayOf
#include "cs2/hashtab.h"                           // for HashTable
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"                        // for Allocator, etc
#include "env/jittypes.h"                          // for uintptrj_t
#include "il/Block.hpp"                            // for Block
#include "il/DataTypes.hpp"                        // for DataTypes::Int32
#include "il/ILOpCodes.hpp"                        // for opcodes
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                           // for Symbol
#include "il/SymbolReference.hpp"                  // for SymbolReference
#include "il/TreeTop.hpp"                          // for TreeTop
#include "il/TreeTop_inlines.hpp"                  // for TreeTop::getNode
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"              // for StaticSymbol
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/deque.hpp"
#include "ras/Debug.hpp"                           // for TR_DebugBase
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"
#include "env/TRMemory.hpp"

#define CTOR_INITIALIZER_LIST   _conditions(NULL), _operands(NULL),  _useRegs(NULL), _defRegs(NULL), \
	   _sourceUsedInMemoryReference(NULL), _flags(0), _longDispSpillReg1(NULL), _longDispSpillReg2(NULL), _binFreeRegs(0), \
   _targetRegSize(0), _sourceRegSize(0), _sourceMemSize(0), _targetMemSize(0), _sourceStart(-1), _targetStart(-1)

OMR::Z::Instruction::Instruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* node)
   : 
   OMR::Instruction(cg, op, node),
   CTOR_INITIALIZER_LIST
   {
   TR_ASSERT(cg->getS390ProcessorInfo()->supportsArch(_opcode.getMinimumALS()), "Processor detected does not support instruction %s\n", cg->getDebug()? cg->getDebug()->getOpCodeName(&_opcode) : "(unknown)");

   self()->initialize();
   }

OMR::Z::Instruction::Instruction(TR::CodeGenerator*cg, TR::Instruction* precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node* node)
   : 
   OMR::Instruction(cg, precedingInstruction, op, node),
   CTOR_INITIALIZER_LIST
   {
   TR_ASSERT(cg->getS390ProcessorInfo()->supportsArch(_opcode.getMinimumALS()), "Processor detected does not support instruction %s\n", cg->getDebug()? cg->getDebug()->getOpCodeName(&_opcode) : "(unknown)");

   self()->initialize(precedingInstruction, true);
   }

TR::RegisterDependencyConditions *
OMR::Z::Instruction::setDependencyConditionsNoBookKeeping(TR::RegisterDependencyConditions *cond)
  {
  // If register dependency conditions exist, merge with the argument conditions
  if( _conditions )
     {
  TR::RegisterDependencyConditions * n = new (self()->cg()->trHeapMemory()) TR::RegisterDependencyConditions( _conditions, cond, self()->cg() );
     _conditions = 0;
     cond = n;
     }
  return _conditions = cond;
  }


TR::RegisterDependencyConditions *
OMR::Z::Instruction::setDependencyConditions(TR::RegisterDependencyConditions *cond)
   {
   TR_ASSERT(cond, "NULL S390 register dependency conditions");
   int32_t oldPreAddCursor = 0;
   int32_t oldPostAddCursor = 0;

   // If register dependency conditions exist, merge with the argument conditions
   if (_conditions)
     {
     oldPreAddCursor  = _conditions->getAddCursorForPre();
     oldPostAddCursor = _conditions->getAddCursorForPost();

     TR::RegisterDependencyConditions * n = new (self()->cg()->trHeapMemory()) TR::RegisterDependencyConditions( _conditions, cond, self()->cg() );
     _conditions = 0;
     cond = n;
     }

   // Perform book keeping only on argument conditions
   cond->bookKeepingRegisterUses(self(), self()->cg(), oldPreAddCursor, oldPostAddCursor);
   cond->setIsUsed();
   if(cond->getPreConditions()) cond->getPreConditions()->incNumUses();
   if(cond->getPostConditions()) cond->getPostConditions()->incNumUses();

   return _conditions = cond;
   }


TR::RegisterDependencyConditions *
OMR::Z::Instruction::updateDependencyConditions(TR::RegisterDependencyConditions *cond)
   {
   int32_t currentPreCursor = 0;
   int32_t currentPostCursor = 0;
   if (_conditions)
      {
      currentPreCursor = _conditions->getAddCursorForPre();
      currentPostCursor = _conditions->getAddCursorForPost();
      }

   cond->bookKeepingRegisterUses(self(), self()->cg(), currentPreCursor, currentPostCursor);
   cond->setIsUsed();
   return _conditions = cond;
   }

void
OMR::Z::Instruction::initialize(TR::Instruction * precedingInstruction, bool instFlag, TR::RegisterDependencyConditions * cond, bool condFlag)
   {
   //call other initializer first
   self()->initialize(cond);

   TR::CodeGenerator * cg = OMR::Instruction::cg();
   self()->setBlockIndex(cg->getCurrentBlockIndex());

   if (cond)
      {
      // Don't want to increment total use counts for ASSOCREGS instructions
      // because their register references will confuse the code that tries
      // to determine when the first use of a register takes place
      if (condFlag || self()->getOpCodeValue() != TR::InstOpCode::ASSOCREGS)
         {
         cond->bookKeepingRegisterUses(self(), cg);
         if(cond->getPreConditions()) cond->getPreConditions()->incNumUses();
         if(cond->getPostConditions()) cond->getPostConditions()->incNumUses();
         }
      cond->setIsUsed();
      }

   if (cg->getCondCodeShouldBePreserved())
      TR_ASSERT( (_opcode.setsOverflowFlag() || _opcode.setsZeroFlag() || _opcode.setsSignFlag() || _opcode.setsCarryFlag()) == false,
            "We thought the instruction [0x%p] won't modify the CC, but it does.\n", self());

   self()->readCCInfo();  // Must come before setCCInfo - Instruction reads CC before setting CC.

   if (cg->comp()->getOption(TR_EnableEBBCCInfo))
      {
      ((self()->isLabel() || self()->isCall()) || (instFlag && precedingInstruction == NULL)) ? self()->clearCCInfo() : self()->setCCInfo();
      }
   else
      {
      (!self()->isBranchOp() && (!instFlag || precedingInstruction == NULL)) ? self()->setCCInfo() : self()->clearCCInfo();
      }
   }


// Realistic offset estimate for the instruction.
// Applicable to frontends which does not do binary encoding (i.e., not Java or OMR).
// Therefore, we will reuse the field for the binary buffer pointer to store the binary
// offset. This way we dont use an extra 4 byte field in TR::Instruction
uint32_t
OMR::Z::Instruction::getEstimatedBinaryOffset()
  {
  union { uint8_t *p; uint32_t i; } _u;
  _u.p = self()->getBinaryEncoding();
  return _u.i;
  }
void
OMR::Z::Instruction::setEstimatedBinaryOffset(uint32_t l)
  {
  union { uint8_t *p; uint32_t i; } _u;
  _u.i = l;
  self()->setBinaryEncoding(_u.p) ;
  }



void
OMR::Z::Instruction::setCCInfo()
   {
   if (self()->getNode() == NULL)
      {
      // play it safe... perhaps this should be changed to an assert that the node should never be NULL?
      self()->clearCCInfo();
      return;
      }

   if (_opcode.setsCC())
      {
      if (self()->cg()->ccInstruction() != NULL)
         {
         // Check previous CC setting instruction... we can mark its usage as
         // known, since the current instruction will override its CC value.
         self()->cg()->ccInstruction()->setCCuseKnown();
         }
      if (!self()->cg()->comp()->getOption(TR_EnableEBBCCInfo))
         {
         if (_opcode.setsOverflowFlag() || _opcode.setsZeroFlag() || _opcode.setsSignFlag() || _opcode.setsCarryFlag())
            {
            // we have detailed information on the CC setting
            self()->cg()->setHasCCInfo(true);
            self()->cg()->setHasCCOverflow(_opcode.setsOverflowFlag() && !self()->getNode()->cannotOverflow());
            self()->cg()->setHasCCZero(_opcode.setsZeroFlag());
            self()->cg()->setHasCCSigned(_opcode.setsSignFlag() || _opcode.setsCompareFlag());
            self()->cg()->setCCInstruction(self());
            self()->cg()->setHasCCCarry(_opcode.setsCarryFlag());
            }
         else
            {
            // the CC setting is generic or
            // there is no node associated with this instruction,
            // play it safe and record no information
            self()->cg()->setHasCCInfo(false);
            }
         self()->cg()->setCCInstruction(self());
         }
      else
         {
         if (_opcode.setsOverflowFlag() || _opcode.setsZeroFlag() || _opcode.setsSignFlag() || _opcode.setsCompareFlag() || _opcode.setsCarryFlag())
         // track Ops that setsCompareFlag for compare ops as well, so that we may remove redudent compare ops;
         // note this will not intruduce mismatching for compare immediate ops with CC from compare Ops,
         // because compare immed ops looks for setsSignedFlag or setsZeroFlags - compare ops does not sets these flags
            {
            // we have detailed information on the CC setting
            self()->cg()->setHasCCInfo(true);
            self()->cg()->setHasCCOverflow(_opcode.setsOverflowFlag() && !self()->getNode()->cannotOverflow());
            self()->cg()->setHasCCZero(_opcode.setsZeroFlag());
            self()->cg()->setHasCCSigned(_opcode.setsSignFlag());
            self()->cg()->setHasCCCompare(_opcode.setsCompareFlag());
            self()->cg()->setHasCCCarry(_opcode.setsCarryFlag());
            self()->cg()->setCCInstruction(self());
            }
         else
            {
            // the CC setting is generic, such as Load & Test Reg, Test Under Mask etc are not handled; or,
            // there is no node associated with this instruction,
            // play it safe and record no information
            self()->cg()->setHasCCInfo(false);
            }
         self()->cg()->setCCInstruction(self());
         } // if enableEBBCCInfo
      }
   }

void
OMR::Z::Instruction::readCCInfo()
   {
   if (_opcode.readsCC())
      {
      if (self()->cg()->ccInstruction() != NULL)
         {
         // Current instruction reads CC.  Mark the last instruction that
         // set the CC value as having its CC being used.
         self()->cg()->ccInstruction()->setCCuseKnown();
         self()->cg()->ccInstruction()->setCCused();
         }
      }
   }

void
OMR::Z::Instruction::clearCCInfo()
   {
   self()->cg()->setHasCCInfo(false);
   self()->cg()->setCCInstruction(NULL);
   }


bool
OMR::Z::Instruction::matchesTargetRegister(TR::Register* reg)
   {
   if (reg->getRealRegister())
      {
      if (self()->implicitlySetsGPR0() &&
          toRealRegister(reg)->getRegisterNumber() == TR::RealRegister::GPR0)
         return true;

      if (self()->implicitlySetsGPR1() &&
          toRealRegister(reg)->getRegisterNumber() == TR::RealRegister::GPR1)
         return true;

      if (self()->implicitlySetsGPR2() &&
          toRealRegister(reg)->getRegisterNumber() == TR::RealRegister::GPR2)
         return true;
      }
   return false;
   }

bool
OMR::Z::Instruction::matchesAnyRegister(TR::Register * reg, TR::Register * instReg)
   {
   if (!instReg)
      return false;
   TR::RegisterPair * regPair = instReg->getRegisterPair();

   TR::RealRegister * realReg = NULL;
   TR::RealRegister * realInstReg1 = NULL;
   TR::RealRegister * realInstReg2 = NULL;

   bool enableHighWordRA =
      self()->cg()->supportsHighWordFacility() &&
      !self()->cg()->comp()->getOption(TR_DisableHighWordRA) &&
      (reg->getKind()!=TR_FPR)                       &&
      (instReg->getKind()!=TR_FPR) &&
      (reg->getKind()!=TR_VRF) &&
      (instReg->getKind()!=TR_VRF);

   if (enableHighWordRA && reg->getRealRegister())
      {
      realReg = (TR::RealRegister *)reg;
      if (realReg->isHighWordRegister())
         {
         // Highword aliasing low word regs
         realReg = realReg->getLowWordRegister();
         }
      }

   if (regPair)
      {
      // if we are matching real regs
      if (enableHighWordRA && regPair->getHighOrder()->getRealRegister())
         {
         // reg pairs do not use HPRs
         realInstReg1 = (TR::RealRegister *)(regPair->getHighOrder());
         realInstReg2 = toRealRegister(regPair->getLowOrder());
         return realReg == realInstReg1 || realReg == realInstReg2;
         }
      // if we are matching virt regs
      if ((reg == regPair->getHighOrder()) || reg == regPair->getLowOrder())
         {
         return true;
         }
      }
   else
      {
      // if we are matching real regs
      if (enableHighWordRA && instReg->getRealRegister())
         {
         realInstReg1 = (TR::RealRegister *)instReg;
         return realReg == realInstReg1;
         }
      // if we are matching virt regs
      if (reg == instReg)
         {
         return true;
         }
      }
   return false;
   }

bool
OMR::Z::Instruction::matchesAnyRegister(TR::Register * reg, TR::Register * instReg1, TR::Register * instReg2)
   {
   return self()->matchesAnyRegister(reg, instReg1) || self()->matchesAnyRegister(reg, instReg2);
   }

TR::Register *
OMR::Z::Instruction::getRegForBinaryEncoding(TR::Register * reg)
   {
   return (reg->getRegisterPair()) ? reg->getRegisterPair()->getHighOrder() : reg;
   }

void
OMR::Z::Instruction::useRegister(TR::Register * reg, bool isDummy)
   {
   TR_ASSERT( reg != NULL, "NULL Register encountered");
   TR::RegisterPair * regPair = reg->getRegisterPair();
   if (regPair)
      {
      TR::Register * firstRegister = regPair->getHighOrder();
      TR::Register * lastRegister = regPair->getLowOrder();
      TR_ASSERT( firstRegister != NULL, "NULL First Register encountered");
      TR_ASSERT( lastRegister != NULL,"NULL Last Register encountered");

      if (firstRegister->getRealRegister()==NULL)
         {
         TR_ASSERT(firstRegister->getSiblingRegister(),
            "[%p] Register Pairs should be allocated using allocateConsecutivePair.\n", self());
         }
      if (lastRegister->getRealRegister()==NULL)
         {
         TR_ASSERT(lastRegister->getSiblingRegister(),
            "[%p] Register Pairs should be allocated using allocateConsecutivePair.\n", self());
         }

      if (_opcode.is64bit() || _opcode.is32to64bit())
         {
         firstRegister->setIsUpperBitsAreDirty(true);
         lastRegister->setIsUpperBitsAreDirty(true);
         }

      OMR::Instruction::useRegister(firstRegister);
      OMR::Instruction::useRegister(lastRegister);

      if (firstRegister->isArGprPair())
         {
        OMR::Instruction::useRegister(firstRegister->getARofArGprPair());
        OMR::Instruction::useRegister(firstRegister->getGPRofArGprPair());
         }

      if (lastRegister->isArGprPair())
         {
        OMR::Instruction::useRegister(lastRegister->getARofArGprPair());
        OMR::Instruction::useRegister(lastRegister->getGPRofArGprPair());
         }

      }
   else
      {
      if (_opcode.is64bit() || _opcode.is32to64bit())
         {
         reg->setIsUpperBitsAreDirty(true);
         }

      OMR::Instruction::useRegister(reg);

      // If an instruction uses a dummy register, that register should no longer be considered dummy.
      // S390RegisterDependency also calls useRegister, in this case we do not want to reset the dummy status of these regs
      //
      if (!isDummy && reg->isPlaceholderReg())
         {
         reg->resetPlaceholderReg();
         }
      }
   return;
   }

bool
OMR::Z::Instruction::refsRegister(TR::Register * reg)
   {
   return self()->getDependencyConditions() ? self()->getDependencyConditions()->refsRegister(reg) : false;
   }

bool
OMR::Z::Instruction::defsAnyRegister(TR::Register * reg)
   {
   TR::Register **_targetReg = self()->targetRegBase();

   if (_targetReg)
      {
      for (int32_t i = 0; i < _targetRegSize; ++i)
         {
         if (_targetReg[i]->usesAnyRegister(reg))
            return true;
         }
      }
   return false;
   }

bool
OMR::Z::Instruction::defsRegister(TR::Register * reg)
   {
   TR::Register **_targetReg = self()->targetRegBase();

   if (_targetReg)
      {
      for (int32_t i = 0; i < _targetRegSize; ++i)
         {
         if (_targetReg[i]->usesRegister(reg))
            return true;
         }
      }

   return false;
   }

// this may replace defsRegister
bool
OMR::Z::Instruction::isDefRegister(TR::Register * reg)
   {
   // Test if this is the implicitly targeted register
   if (self()->matchesTargetRegister(reg))
      return true;

   //Test explicitly targeted register by the instruction
   if (_defRegs)
      {
      for (int32_t i = 0; i < _defRegs->size(); ++i)
         {
         if ((* _defRegs) [i]->usesRegister(reg))
            return true;
         }
      }

   return false;
   }

bool OMR::Z::Instruction::usesOnlyRegister(TR::Register *reg)
  {
   int32_t i;
   TR::Register **_sourceReg = self()->sourceRegBase();
   TR::MemoryReference **_sourceMem = self()->sourceMemBase();
   TR::MemoryReference **_targetMem = self()->targetMemBase();

   for (i = 0; i < _sourceRegSize; ++i)
      {
      if (_sourceReg[i]->usesRegister(reg))
         return true;
      }

   for (i = 0; i < _sourceMemSize; ++i)
      {
      if (_sourceMem[i]->usesRegister(reg))
         return true;
      }

   for (i = 0; i < _targetMemSize; ++i)
      {
      if (_targetMem[i]->usesRegister(reg))
         return true;
      }

   // We might have an out of line EX instruction.  If so, we
   // need to check whether either the EX instruction OR the instruction
   // it refers to uses the specified register.
   TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
   if (outOfLineEXInstr && outOfLineEXInstr->usesOnlyRegister(reg))
     return true;

   return false;
  }

bool
OMR::Z::Instruction::usesRegister(TR::Register * reg)
   {
   int32_t i;

   if(self()->usesOnlyRegister(reg) || self()->defsRegister(reg))
     return true;

   // The only thing that is not covered by defsRegister is target registers found in EX
   // We might have an out of line EX instruction.  If so, we
   // need to check whether either the EX instruction OR the instruction
   // it refers to uses the specified register.
   TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
   if (outOfLineEXInstr)
      {
      TR::Register **_targetReg = outOfLineEXInstr->targetRegBase();
      if (_targetReg)
         {
         for (i = 0; i < outOfLineEXInstr->_targetRegSize; ++i)
            {
            if (_targetReg[i]->usesRegister(reg))
               return true;
            }
         }
      }

   return false;
   }


bool OMR::Z::Instruction::startOfLiveRange(TR::Register * reg)
  {
   int32_t i;
   TR::Register **_targetReg = self()->targetRegBase();
   bool result;

   // If reg is part of internal control flow region then it must stay alive
   if(self()->cg()->isInternalControlFlowReg(reg))
     return false;

   // Special case SRDA, SRDL, SLDA, SLDL when immediate is >= 32
   if((self()->getOpCodeValue() == TR::InstOpCode::SRDA || self()->getOpCodeValue() == TR::InstOpCode::SRDL) && toS390RSInstruction(self())->getSourceImmediate() >= 32)
     {
     // Clobbering the low order half by shifing completely over it so not really using it
     TR::RegisterPair *rp=_targetReg[0]->getRegisterPair();
     if(reg == rp->getLowOrder())
       return true;
     }
   if((self()->getOpCodeValue() == TR::InstOpCode::SLDA || self()->getOpCodeValue() == TR::InstOpCode::SLDL) && toS390RSInstruction(self())->getSourceImmediate() >= 32)
     {
     // Clobbering the high order half by shifing completely over it so not really using it
     TR::RegisterPair *rp=_targetReg[0]->getRegisterPair();
     if(reg == rp->getHighOrder())
       return true;
     }
   if(self()->getOpCodeValue() == TR::InstOpCode::RISBG || self()->getOpCodeValue() == TR::InstOpCode::RISBGN || self()->getOpCodeValue() == TR::InstOpCode::RISBHG || self()->getOpCodeValue() == TR::InstOpCode::RISBLG)
     {
      uint8_t endBit = ((TR::S390RIEInstruction* )self())->getSourceImmediate8Two();
      if(_targetReg[0] == reg && (endBit & 128) != 0)
        return true;
      }

   if (_targetReg && !(self()->getOpCode().usesTarget()))
      {
      for (i = 0; i < _targetRegSize; ++i)
        {
        if (_targetReg[i]->usesRegister(reg) && !self()->usesOnlyRegister(reg))
          {
          // Special case LTR GPR_X,GPR_X
          if(self()->getOpCodeValue() == TR::InstOpCode::LTR && self()->getRegisterOperand(1) == self()->getRegisterOperand(2))
            return false;
          if(_targetReg[i]->getKind() == TR_GPR64 && self()->getOpCode().is32bit() && TR::Compiler->target.is32Bit())
            return false;
          return true;
          }
        }
      }

   // We might have an out of line EX instruction.  If so, we
   // need to check whether either the EX instruction OR the instruction
   // it refers to uses the specified register.
   TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
   if (outOfLineEXInstr && outOfLineEXInstr->startOfLiveRange(reg))
      return true;

   return false;
  }

bool OMR::Z::Instruction::getRegisters(TR::list<TR::Register *> &regs)
  {
   TR::Compilation *comp = self()->cg()->comp();
  int32_t i,n;
  regs.clear();

  if(self()->getOpCodeValue() == TR::InstOpCode::ASSOCREGS)
    return false;

  TR::Register **_sourceReg = self()->sourceRegBase();
  TR::Register **_targetReg = self()->targetRegBase();
  TR::MemoryReference **_sourceMem = self()->sourceMemBase();
  TR::MemoryReference **_targetMem = self()->targetMemBase();
  TR::Register *baseReg, *indexReg;


  for (i = 0; i < _sourceRegSize; ++i)
    {
    TR::Register *r=_sourceReg[i];
    TR::RegisterPair *rp=r->getRegisterPair();
    if(rp)
      {
      regs.push_back(rp->getLowOrder());
      regs.push_back(rp->getHighOrder());
      }
    else
      regs.push_back(r);
    }

  for (i = 0; i < _targetRegSize; ++i)
    {
    TR::Register *r=_targetReg[i];
    TR::RegisterPair *rp=r->getRegisterPair();
    if(rp)
      {
      regs.push_back(rp->getLowOrder());
      regs.push_back(rp->getHighOrder());
      }
    else
      regs.push_back(r);
    }

  for (i = 0; i < _sourceMemSize; ++i)
    {
    baseReg=_sourceMem[i]->getBaseRegister();
    indexReg=_sourceMem[i]->getIndexRegister();
    if(baseReg) regs.push_back(baseReg);
    if(indexReg) regs.push_back(indexReg);
    }

  for (i = 0; i < _targetMemSize; ++i)
    {
    baseReg=_targetMem[i]->getBaseRegister();
    indexReg=_targetMem[i]->getIndexRegister();
    if(baseReg) regs.push_back(baseReg);
    if(indexReg) regs.push_back(indexReg);
    }

  // Also check the register dependency conditions.
  TR::RegisterDependencyConditions *conds=self()->getDependencyConditions();
  if(conds)
    {
    TR_S390RegisterDependencyGroup *preConds=conds->getPreConditions();
    n = conds->getNumPreConditions();
    for (i = 0; i < n; i++)
      {
      TR::RegisterDependency* dep=preConds->getRegisterDependency(i);
      TR::Register *r=dep->getRegister(self()->cg());
      if(r && !r->isPlaceholderReg())
        {
        TR::RegisterPair *rp=r->getRegisterPair();
        if(rp)
          {
          regs.push_back(rp->getLowOrder());
          regs.push_back(rp->getHighOrder());
          }
        else
          regs.push_back(r);
        }
      }

    TR_S390RegisterDependencyGroup *postConds=conds->getPostConditions();
    n = conds->getNumPostConditions();
    for (i = 0; i < n; i++)
      {
      TR::RegisterDependency* dep=postConds->getRegisterDependency(i);
      TR::Register *r=dep->getRegister(self()->cg());
      if(r && !r->isPlaceholderReg())
        {
        TR::RegisterPair *rp=r->getRegisterPair();
        if(rp)
          {
          regs.push_back(rp->getLowOrder());
          regs.push_back(rp->getHighOrder());
          }
        else
          regs.push_back(r);
        }
      }
    }

  // We might have an out of line EX instruction.  If so, we
  // need to check whether either the EX instruction OR the instruction
  // it refers to uses the specified register.
  TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
  if (outOfLineEXInstr)
    {
    TR::list<TR::Register *> exRegs(getTypedAllocator<TR::Register*>(comp->allocator()));
    if (outOfLineEXInstr->getRegisters(exRegs))
       {
       for(auto cursor = exRegs.begin(); cursor != exRegs.end(); ++cursor)
          {
          regs.push_back(*cursor);
          }
       }
    }

  return !regs.empty();
  }

bool OMR::Z::Instruction::getUsedRegisters(TR::list<TR::Register *> &usedRegs)
  {
  int32_t i,n;
  usedRegs.clear();
  TR::Compilation *comp = self()->cg()->comp();
  OMR::Z::Machine *machine=self()->cg()->machine();
  bool afterRA=self()->cg()->afterRA();

  if(self()->getOpCodeValue() == TR::InstOpCode::ASSOCREGS)
    return false;

  TR::Register **_sourceReg = self()->sourceRegBase();
  TR::Register **_targetReg = self()->targetRegBase();
  TR::MemoryReference **_sourceMem = self()->sourceMemBase();
  TR::MemoryReference **_targetMem = self()->targetMemBase();
  TR::Register *baseReg, *indexReg;

  if(self()->getOpCodeValue() == TR::InstOpCode::BCR && _sourceRegSize == 1 && _sourceReg[0]->getRealRegister() == machine->getS390RealRegister(TR::RealRegister::GPR0))
    return false;

  if((self()->getOpCodeValue() == TR::InstOpCode::XGR || self()->getOpCodeValue() == TR::InstOpCode::XR) && _sourceReg[0] == _targetReg[0])
    return false;

  for (i = 0; i < _sourceRegSize; ++i)
    {
    TR::Register *r=_sourceReg[i];
    TR::RegisterPair *rp=r->getRegisterPair();
    if(rp)
      {
      usedRegs.push_back(rp->getLowOrder());
      usedRegs.push_back(rp->getHighOrder());
      }
    else
      usedRegs.push_back(r);
    }

  if(self()->getOpCode().usesTarget())
    {
    for (i = 0; i < _targetRegSize; ++i)
      {
      TR::Register *r=_targetReg[i];
      TR::RegisterPair *rp=r->getRegisterPair();
      if(rp)
        {
        if((self()->getOpCodeValue() == TR::InstOpCode::SRDA || self()->getOpCodeValue() == TR::InstOpCode::SRDL) && toS390RSInstruction(self())->getSourceImmediate() >= 32)
          ; // Clobbering the low order half by shifing completely over it so not really using it
        else
          usedRegs.push_back(rp->getLowOrder());
        if((self()->getOpCodeValue() == TR::InstOpCode::SLDA || self()->getOpCodeValue() == TR::InstOpCode::SLDL) && toS390RSInstruction(self())->getSourceImmediate() >= 32)
          ; // Clobbering the high order half by shifing completely over it so not really using it
        else
          usedRegs.push_back(rp->getHighOrder());
        }
      else
        {
        if(i==0 && (self()->getOpCodeValue() == TR::InstOpCode::RISBG || self()->getOpCodeValue() == TR::InstOpCode::RISBGN || self()->getOpCodeValue() == TR::InstOpCode::RISBHG || self()->getOpCodeValue() == TR::InstOpCode::RISBLG))
          {
          uint8_t endBit = ((TR::S390RIEInstruction* )self())->getSourceImmediate8Two();
          if((endBit & 128) != 0)
            continue;       // Zeroing out unused bits therefore the first operand is not used
          else
            usedRegs.push_back(r);
          }
        else
          usedRegs.push_back(r);
        }
      }
    }
  else if(self()->getOpCode().is32bit() && TR::Compiler->target.is32Bit())
    {
    TR::Register **_targetReg = self()->targetRegBase();
    for (i = 0; i < _targetRegSize; ++i)
      {
      TR::Register *r=_targetReg[i];
      if(r->getKind() == TR_GPR64)
        {
        TR::RegisterPair *rp=r->getRegisterPair();
        if(rp)
          {
          if((self()->getOpCodeValue() == TR::InstOpCode::SRDA || self()->getOpCodeValue() == TR::InstOpCode::SRDL) && toS390RSInstruction(self())->getSourceImmediate() >= 32)
            ; // Clobbering the low order half by shifing completely over it so not really using it
          else
            usedRegs.push_back(rp->getLowOrder());
          if((self()->getOpCodeValue() == TR::InstOpCode::SLDA || self()->getOpCodeValue() == TR::InstOpCode::SLDL) && toS390RSInstruction(self())->getSourceImmediate() >= 32)
            ; // Clobbering the high order half by shifing completely over it so not really using it
          else
            usedRegs.push_back(rp->getHighOrder());
          }
        else
          usedRegs.push_back(r);
        }
      }
    }


  for (i = 0; i < _sourceMemSize; ++i)
    {
    baseReg=_sourceMem[i]->getBaseRegister();
    indexReg=_sourceMem[i]->getIndexRegister();
    if(baseReg) usedRegs.push_back(baseReg);
    if(indexReg) usedRegs.push_back(indexReg);
    }

  for (i = 0; i < _targetMemSize; ++i)
    {
    baseReg=_targetMem[i]->getBaseRegister();
    indexReg=_targetMem[i]->getIndexRegister();
    if(baseReg) usedRegs.push_back(baseReg);
    if(indexReg) usedRegs.push_back(indexReg);
    }

  // Also check the register dependency conditions. They record implied register uses
  TR::RegisterDependencyConditions *conds=self()->getDependencyConditions();
  if(conds)
    {
    TR_S390RegisterDependencyGroup *preConds=conds->getPreConditions();
    n = conds->getNumPreConditions();
    for (i = 0; i < n; i++)
      {
      if (preConds->getRegisterDependency(i)->getRefsRegister())
        {
        TR::RegisterDependency* dep=preConds->getRegisterDependency(i);
        TR::Register *r=dep->getRegister(self()->cg());
        if(r && !r->isPlaceholderReg())
          {
          TR::RegisterPair *rp=r->getRegisterPair();
          if(rp)
            {
            usedRegs.push_back(rp->getLowOrder());
            usedRegs.push_back(rp->getHighOrder());
            }
          else
            {
            if(!afterRA)
              usedRegs.push_back(r);
            else
              {
              TR::RealRegister::RegNum rr = dep->getRealRegister();
              if(rr>TR::RealRegister::NoReg && rr<=TR::RealRegister::MISC)
                {
                usedRegs.push_back(machine->getS390RealRegister(rr));
                }
              }
            }
          }
        }
      }

    TR_S390RegisterDependencyGroup *postConds=conds->getPostConditions();
    n = conds->getNumPostConditions();
    for (i = 0; i < n; i++)
      {
      if (postConds->getRegisterDependency(i)->getRefsRegister())
        {
        TR::RegisterDependency* dep=postConds->getRegisterDependency(i);
        TR::Register *r=dep->getRegister(self()->cg());
        if(r && !r->isPlaceholderReg())
          {
          TR::RegisterPair *rp=r->getRegisterPair();
          if(rp)
            {
            usedRegs.push_back(rp->getLowOrder());
            usedRegs.push_back(rp->getHighOrder());
            }
          else
            {
            if(!afterRA)
              usedRegs.push_back(r);
            else
              {
              TR::RealRegister::RegNum rr = dep->getRealRegister();
              if(rr>TR::RealRegister::NoReg && rr<=TR::RealRegister::MISC)
                {
                usedRegs.push_back(machine->getS390RealRegister(rr));
                }
              }
            }
          }
        }
      }
    }

  // We might have an out of line EX instruction.  If so, we
  // need to check whether either the EX instruction OR the instruction
  // it refers to uses the specified register.
  TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
  if (outOfLineEXInstr)
     {
     TR::list<TR::Register *> exRegs(getTypedAllocator<TR::Register*>(comp->allocator()));
     if(outOfLineEXInstr->getUsedRegisters(exRegs))
        {
        for(auto cursor = exRegs.begin(); cursor != exRegs.end(); ++cursor)
           {
           usedRegs.push_back(*cursor);
           }
        }
     }

  return !usedRegs.empty();
  }

bool OMR::Z::Instruction::getDefinedRegisters(TR::list<TR::Register *> &defedRegs)
  {
   TR::Compilation *comp = self()->cg()->comp();
   OMR::Z::Machine *machine=self()->cg()->machine();
   bool afterRA=self()->cg()->afterRA();

  int32_t i,n;

  defedRegs.clear();

  if(self()->getOpCodeValue() == TR::InstOpCode::ASSOCREGS)
    return false;

  if(!((self()->getOpCodeValue() == TR::InstOpCode::LTR || self()->getOpCodeValue() == TR::InstOpCode::LTGR) && self()->getRegisterOperand(1) == self()->getRegisterOperand(2)))
    {
    TR::Register **_targetReg = self()->targetRegBase();
    for (i = 0; i < _targetRegSize; ++i)
      {
      TR::Register *r=_targetReg[i];
      TR::RegisterPair *rp=r->getRegisterPair();
      if(rp)
        {
        defedRegs.push_back(rp->getLowOrder());
        defedRegs.push_back(rp->getHighOrder());
        }
      else
        defedRegs.push_back(r);
      }
    }

  // Also check the register dependency conditions. They record implied register uses
  TR::RegisterDependencyConditions *conds=self()->getDependencyConditions();
  if(conds)
    {
    // For function calls pre-conditions are uses and post-conditions are results
    TR_S390RegisterDependencyGroup *preConds=conds->getPreConditions();
    n = conds->getNumPreConditions();
    for (i = 0; i < n; i++)
      {
      if (preConds->getRegisterDependency(i)->getDefsRegister())
        {
        TR::RegisterDependency* dep=preConds->getRegisterDependency(i);
        TR::Register *r=dep->getRegister(self()->cg());
        if(r && !r->isPlaceholderReg())
          {
          TR::RegisterPair *rp=r->getRegisterPair();
          if(rp)
            {
            defedRegs.push_back(rp->getLowOrder());
            defedRegs.push_back(rp->getHighOrder());
            }
          else
            {
            if(!afterRA)
              defedRegs.push_back(r);
            else
              {
              TR::RealRegister::RegNum rr = dep->getRealRegister();
              if(rr>TR::RealRegister::NoReg && rr<=TR::RealRegister::MISC)
                {
                defedRegs.push_back(machine->getS390RealRegister(rr));
                }
              }
            }
          }
        }
      }

    TR_S390RegisterDependencyGroup *postConds=conds->getPostConditions();
    n = conds->getNumPostConditions();
    for (i = 0; i < n; i++)
      {
      if (postConds->getRegisterDependency(i)->getDefsRegister())
        {
        TR::RegisterDependency* dep=postConds->getRegisterDependency(i);
        TR::Register *r=dep->getRegister(self()->cg());
        if(r && !r->isPlaceholderReg())
          {
          TR::RegisterPair *rp=r->getRegisterPair();
          if(rp)
            {
            defedRegs.push_back(rp->getLowOrder());
            defedRegs.push_back(rp->getHighOrder());
            }
          else
            {
            if(!afterRA)
              defedRegs.push_back(r);
            else
              {
              TR::RealRegister::RegNum rr = dep->getRealRegister();
              if(rr>TR::RealRegister::NoReg && rr<=TR::RealRegister::MISC)
                {
                defedRegs.push_back(machine->getS390RealRegister(rr));
                }
              }
            }
          }
        }
      }
    }

  // We might have an out of line EX instruction.  If so, we
  // need to check whether either the EX instruction OR the instruction
  // it refers to uses the specified register.
  TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
  if (outOfLineEXInstr)
     {
     TR::list<TR::Register *> exRegs(getTypedAllocator<TR::Register*>(comp->allocator()));
     if(outOfLineEXInstr->getDefinedRegisters(exRegs))
        {
        for(auto cursor = exRegs.begin(); cursor != exRegs.end(); ++cursor)
           {
           defedRegs.push_back(*cursor);
           }
        }
     }

  return !defedRegs.empty();
  }

bool OMR::Z::Instruction::getKilledRegisters(TR::list<TR::Register *> &killedRegs)
  {
  int32_t i,n;
  TR::Compilation *comp = self()->cg()->comp();

  TR::Machine *machine = self()->cg()->machine();
  killedRegs.clear();

  if(self()->getOpCodeValue() == TR::InstOpCode::ASSOCREGS)
    return false;

  // Post dependency conditions will have placeholder regs which indicate which registers get killed
  TR::RegisterDependencyConditions *conds=self()->getDependencyConditions();
  if(conds)
    {
    TR_S390RegisterDependencyGroup *postConds=conds->getPostConditions();
    n = conds->getNumPostConditions();
    for (i = 0; i < n; i++)
      {
      if (postConds->getRegisterDependency(i)->getDefsRegister())
        {
        TR::RegisterDependency* dep=postConds->getRegisterDependency(i);
        TR::Register *r=dep->getRegister(self()->cg());
        if(r && r->isPlaceholderReg())
          killedRegs.push_back(machine->getS390RealRegister(dep->getRealRegister()));
        }
      }
    }

  // A function call via address in a register uses register r1 to hold the target address but this register is also killed.
  if(self()->getOpCodeValue() == TR::InstOpCode::BASR)
    killedRegs.push_back(machine->getS390RealRegister(TR::RealRegister::GPR1));

  // We might have an out of line EX instruction.  If so, we
  // need to check whether either the EX instruction OR the instruction
  // it refers to uses the specified register.
  TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
  if (outOfLineEXInstr)
     {
     TR::list<TR::Register *> exRegs(getTypedAllocator<TR::Register*>(comp->allocator()));
     if(outOfLineEXInstr->getKilledRegisters(exRegs))
        {
        for(auto cursor = exRegs.begin(); cursor != exRegs.end(); ++cursor)
           {
           killedRegs.push_back(*cursor);
           }
        }
     }

  return !killedRegs.empty();
  }


static bool isInternalControlFlowOneEntryOneExit(TR::Instruction *regionEnd, TR::Compilation *comp)
  {
  TR::deque<TR::LabelSymbol *> labels(comp->allocator());
  TR::Instruction *curr=NULL;
  TR::Instruction *regionStart=NULL;
  bool done=false;

  // Collect all the labels and start of region
  for(curr=regionEnd;!done;curr=curr->getPrev())
    {
    if(curr->isLabel())
      {
      TR::LabelSymbol *labelSym=toS390LabelInstruction(curr)->getLabelSymbol();
      labels.push_back(labelSym);
      labelSym->isStartInternalControlFlow();
      done=true;
      regionStart=curr->getPrev();
      }
    else if(curr->isStartInternalControlFlow())
      {
      done=true;
      regionStart=curr->getPrev();
      }
    }

  // Now check if all branches jump to lables inside region

  for(curr=regionEnd; curr != regionStart; curr=curr->getPrev())
    {
    if(curr->isBranchOp())
      {
      TR::S390LabeledInstruction *labeledInstr=(TR::S390LabeledInstruction *)curr;
      TR::LabelSymbol *targetLabel=labeledInstr->getLabelSymbol();
      bool found=false;
      for(auto it = labels.begin(); it != labels.end(); ++it)
        {
        if(*it == targetLabel)
          {
          found = true;
          break;
          }
        }
      if(!found)
        return false; // Found a branch to an outside label
      }
    }

  return true;
  }



TR::Instruction *
OMR::Z::Instruction::getOutOfLineEXInstr()
   {
   if (!self()->isOutOfLineEX())
      return NULL;
   switch(self()->getOpCodeValue())
      {
      case TR::InstOpCode::EX:
      {
         TR::MemoryReference * tempMR = ((TR::S390RXInstruction *)self())->getMemoryReference();
         TR::S390ConstantInstructionSnippet * cis = (TR::S390ConstantInstructionSnippet *) tempMR->getConstantDataSnippet();
         TR_ASSERT( cis != NULL, "Out of line EX instruction doesn't have a constantInstructionSnippet\n");
         return cis->getInstruction();
      }
      case TR::InstOpCode::EXRL:
      {
         TR::S390ConstantInstructionSnippet * cis = (TR::S390ConstantInstructionSnippet *)
            ((TR::S390RILInstruction *)self())->getTargetSnippet();
         TR_ASSERT( cis != NULL, "Out of line EXRL instruction doesn't have a constantInstructionSnippet\n");
         return cis->getInstruction();
      }
      default:
         return NULL;
      }
   }

// determine if this instruction will throw an implicit null pointer exception
// and set appropriate flags
void
OMR::Z::Instruction::setupThrowsImplicitNullPointerException(TR::Node *n, TR::MemoryReference *mr)
   {
   TR::Compilation *comp = self()->cg()->comp();
   if(self()->cg()->getSupportsImplicitNullChecks())
      {
      // this instruction throws an implicit null check if:
      // 1. The treetop node is a NULLCHK node
      // 2. The memory reference of this instruction can cause a null pointer exception
      // 3. The null check reference node must be a child of this node
      // 4. This memory reference uses the same register as the null check reference
      // 5. This is the first instruction in the evaluation of this null check node to have met all the conditions

      // Test conditions 1, 2, and 5
      if(n != NULL & mr != NULL &&
         mr->getCausesImplicitNullPointerException() &&
         self()->cg()->getCurrentEvaluationTreeTop()->getNode()->getOpCode().isNullCheck() &&
         self()->cg()->getImplicitExceptionPoint() == NULL)
         {
         // determine what the NULLcheck reference node is
         TR::Node * nullCheckReference;
         TR::Node * firstChild = self()->cg()->getCurrentEvaluationTreeTop()->getNode()->getFirstChild();
         if (comp->useCompressedPointers() &&
               firstChild->getOpCodeValue() == TR::l2a)
            {
            TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
            while (firstChild->getOpCodeValue() != loadOp)
               firstChild = firstChild->getFirstChild();
            nullCheckReference = firstChild->getFirstChild();
            }
         else
            nullCheckReference = self()->cg()->getCurrentEvaluationTreeTop()->getNode()->getNullCheckReference();


         // Test conditions 3 and 4
         if ((self()->getNode()->getOpCode().hasSymbolReference() &&
               self()->getNode()->getSymbolReference() ==  comp->getSymRefTab()->findVftSymbolRef()) ||
             (n->hasChild(nullCheckReference) && mr->usesRegister(nullCheckReference->getRegister())))
            {
            traceMsg(comp,"Instruction %p throws an implicit NPE, node: %p NPE node: %p\n", self(), n, nullCheckReference);
            self()->cg()->setImplicitExceptionPoint(self());
            }
         }
      }
   }

// The following safe virtual downcast method is only used in an assertion
// check within "toS390ImmInstruction"
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
TR::S390ImmInstruction *
OMR::Z::Instruction::getS390ImmInstruction()
   {
   return NULL;
   }
#endif

uint8_t *
OMR::Z::Instruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = self()->cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,self()->getEstimatedBinaryLength());

   self()->getOpCode().copyBinaryToBuffer(instructionStart);
   self()->setBinaryLength(self()->getOpCode().getInstructionLength());
   self()->setBinaryEncoding(instructionStart);
   self()->cg()->addAccumulatedInstructionLengthError(self()->getEstimatedBinaryLength() - self()->getBinaryLength());
   return cursor;
   }

int32_t
OMR::Z::Instruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   self()->setEstimatedBinaryLength(self()->getOpCode().getInstructionLength());
   return currentEstimate + self()->getEstimatedBinaryLength();
   }

bool
OMR::Z::Instruction::dependencyRefsRegister(TR::Register * reg)
   {
   return false;
   }

void
OMR::Z::Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Compilation *comp = self()->cg()->comp();

   if (self()->getOpCodeValue() != TR::InstOpCode::ASSOCREGS)
      {
      self()->assignRegistersAndDependencies(kindToBeAssigned);
      }
   else if (self()->cg()->enableRegisterAssociations())
      {
      //if (kindToBeAssigned == TR_GPR)
         {
         TR::Machine * machine = self()->cg()->machine();

         int32_t first = TR::RealRegister::FirstGPR;
         int32_t last  = TR::RealRegister::LastAssignableVRF;
      // Step 1 : First traverse the existing associations and remove them so that they don't interfere with the new ones
         for (int32_t i = first; i <= last; ++i)
            {
            TR::Register * virtReg = machine->getVirtualAssociatedWithReal((TR::RealRegister::RegNum) (i));
            if (virtReg)
               {
               virtReg->setAssociation(TR::RealRegister::NoReg);
               }
            }

         if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
            {
            for (int32_t i = TR::RealRegister::FirstHPR; i <= TR::RealRegister::LastHPR; i++)
               {
               TR::Register * virtReg = machine->getVirtualAssociatedWithReal((TR::RealRegister::RegNum) (i));
               if (virtReg)
                  {
                  virtReg->setAssociation(TR::RealRegister::NoReg);
                  }
               }
            }
      // Step 2 : loop through and set up the new associations (both on the machine and by associating the virtual
      // registers with their real dependencies)
         TR_S390RegisterDependencyGroup * depGroup = self()->getDependencyConditions()->getPostConditions();
         for (int32_t j = 0; j < last; ++j)
            {
            TR::Register * virtReg = depGroup->getRegisterDependency(j)->getRegister(self()->cg());
            machine->setVirtualAssociatedWithReal((TR::RealRegister::RegNum) (j + 1), virtReg);
            }

         if(self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
            {
            for (int32_t j = 0; j < TR::RealRegister::LastHPR-TR::RealRegister::FirstHPR; ++j)
               {
               TR::Register * virtReg = depGroup->getRegisterDependency(j+last)->getRegister(self()->cg());
               machine->setVirtualAssociatedWithReal((TR::RealRegister::RegNum) (j + TR::RealRegister::FirstHPR), virtReg);
               }
            }
         machine->setRegisterWeightsFromAssociations();
         }
      }

   // If we are register assigning an EX or EXRL instruction with the isOutOfLineEX flag set
   // we must register assign the instruction object hanging off of the memory reference's constantInstructionSnippet
   TR::Instruction *outOfLineEXInstr = self()->getOutOfLineEXInstr();
   if (outOfLineEXInstr)
      {
      TR::Instruction *savePrev = outOfLineEXInstr->getPrev();

      outOfLineEXInstr->setPrev(self()->getPrev()); // Temporarily set Prev() instruction of snippet to Prev() of EX just in case we insert LR_move on assignRegisters()
      self()->cg()->tracePreRAInstruction(outOfLineEXInstr);
      self()->cg()->setCurrentBlockIndex(outOfLineEXInstr->getBlockIndex());
      outOfLineEXInstr->assignRegisters(kindToBeAssigned);
      if (!self()->cg()->getRAPassAR())
         {
         TR::RegisterDependencyConditions *deps = outOfLineEXInstr->getDependencyConditions();
         if (deps) // merge the dependency into the EX deps
            {
            outOfLineEXInstr->resetDependencyConditions();
            TR::RegisterDependencyConditions * exDeps = (self())->getDependencyConditions();
            TR::RegisterDependencyConditions * newDeps = new (self()->cg()->trHeapMemory()) TR::RegisterDependencyConditions(deps, exDeps, self()->cg());
            (self())->setDependencyConditionsNoBookKeeping(newDeps);
            }
         }

      outOfLineEXInstr->setPrev(savePrev); // Restore Prev() of snippet
      self()->cg()->tracePostRAInstruction(outOfLineEXInstr);

      //Java and other languages probably need the value field to be set to the binary encoding of the instruction
      //This can be done any time after register assignment
      //If the value is set here, the snippet can be handled in an identical fashion to a normal constantdataSnippet
      }

   // trace assigned registers bit vector
   TR_Debug * debugObj = self()->cg()->getDebug();
   if (debugObj && comp->getOption(TR_TraceRegisterState))
      {
      // determine which GPRs are assigned and the total number of available GPRs
      uint32_t freeRegs = self()->getBinLocalFreeRegs();
      uint32_t numRegs  =  TR::RealRegister::LastAssignableGPR - TR::RealRegister::FirstGPR + 1;

      // print freeRegs as a binary number
      char * strFreeRegs = (char *)self()->cg()->trMemory()->allocateHeapMemory(numRegs+1);
      for(uint32_t i = 0; i < numRegs; i++)
         {
         strFreeRegs[numRegs - i - 1] = (freeRegs & 0x1) ? '1' : '0';
         freeRegs >>= 1;
         }
      strFreeRegs[numRegs] = '\0';

      // format the comment nicely and append it as a comment to the instruction
      char *commentTemplate = "RegState=[%s]";
      char *comment = (char *)self()->cg()->trMemory()->allocateHeapMemory(strlen(commentTemplate)+strlen(strFreeRegs)-1);
      if (comment != NULL)
         {
         sprintf(comment, commentTemplate, strFreeRegs);
         debugObj->addInstructionComment(self(),comment);
         }
      }


   // Modify TBEGIN/TBEGINC's General Register Save Mask (GRSM) to only include
   // live registers.
   if (self()->getOpCodeValue() == TR::InstOpCode::TBEGIN || self()->getOpCodeValue() == TR::InstOpCode::TBEGINC)
      {
      uint8_t linkageBasedSaveMask = 0;

      for (int32_t i = TR::RealRegister::GPR0; i != TR::RealRegister::GPR15 + 1; i++)
         {
         if (0 != self()->cg()->getS390Linkage()->getPreserved((TR::RealRegister::RegNum)i))
            {
            //linkageBasedSaveMask is 8 bit mask where each bit represents consecutive even/odd regpair to save.
            linkageBasedSaveMask |= (1 << (7 - ((i - 1) >> 1))); // bit 0 = GPR0/1, GPR0=1, GPR15=16. 'Or' with bit [(i-1)>>1]
            }
         }

      // General Register Save Mask (GRSM)
      uint8_t grsm = (self()->cg()->machine()->genBitVectOfLiveGPRPairs() | linkageBasedSaveMask);

      // GRSM occupies the top 8 bits of the immediate field.  Need to
      // preserve the lower 8 bits, which has controls for AR, Floating
      // Point and Program Interruption Filtering.
      uint16_t originalGRSM = ((TR::S390SILInstruction*)self())->getSourceImmediate();
      ((TR::S390SILInstruction*)self())->setSourceImmediate((grsm << 8) | (originalGRSM & 0xFF));
      }


   }



/////  registerSets utilities for generic register assignment

bool
OMR::Z::Instruction::isHPRUpgradable(uint16_t operandNumber)
   {
   if (_opcode.is64bit())
      return false;

   switch (self()->getOpCodeValue())
      {
      case TR::InstOpCode::AR:
      case TR::InstOpCode::ALR:
         //AHHHR, AHHLR,
         //ALHHHR, ALHHLR
         //Target Reg cannot be low word
         if (operandNumber == 0) return true;
         break;
      case TR::InstOpCode::AHI:
         //AIH
         return true;
         break;
      case TR::InstOpCode::BRCT:
         //BRCTH
         return true;
         break;
      case TR::InstOpCode::CR:
      case TR::InstOpCode::CLR:
         //CHHR, CHLR
         //CLHHR, CLHLR
         if (operandNumber == 0)
            return true;
         break;
      case TR::InstOpCode::C:
      case TR::InstOpCode::CY:
      case TR::InstOpCode::CL:
      case TR::InstOpCode::CLY:
         //CHF, CIH
         if (operandNumber == 0) return true;
         break;
      case TR::InstOpCode::CFI:
      case TR::InstOpCode::CLFI:
         //CLHF, CLIH
         return true;
         break;
      case TR::InstOpCode::LB:
      case TR::InstOpCode::LH:
      case TR::InstOpCode::LHY:
      case TR::InstOpCode::L:
      case TR::InstOpCode::LY:
      case TR::InstOpCode::LLC:
      case TR::InstOpCode::LLH:
         //LBH LHH LFH LLCH LLHH
         if (operandNumber == 0) return true;
         break;
      case TR::InstOpCode::LR:
      case TR::InstOpCode::LLCR:
      case TR::InstOpCode::LLHR:
         //LHHR, LHLR, LLHFR
         //LLCHHR, LLCHLR, LLCLHR
         //LLHHR, LLHHLR, LLHLHR
         return true;
         break;
      case TR::InstOpCode::LHI:
         //IIHF need to manually sign-extend
         return true;
         break;
         /* --- RNSBG etc are cracked, not worth it
      case TR::InstOpCode::NR:
      case TR::InstOpCode::OR:
      case TR::InstOpCode::XR:
         //NHHR, NHLR, NLHR
         //OHHR, OHLR, OLHR
         //XHHR, XHLR, XLHR
         return true;
         break;
         */
      case TR::InstOpCode::ST:
      case TR::InstOpCode::STY:
      case TR::InstOpCode::STC:
      case TR::InstOpCode::STCY:
      case TR::InstOpCode::STH:
      case TR::InstOpCode::STHY:
         //STFH, STCH, STHH
         if (operandNumber == 0) return true;
         break;
      case TR::InstOpCode::SR:
      case TR::InstOpCode::SLR:
         //SHHHR, SHHLR
         //SLHHHR, SLHHR
         if (operandNumber == 0) return true;
         break;
      case TR::InstOpCode::SLFI:
         //ALSIH
         return true;
         break;
      default:
         return false;
      }
   return false;
   }

uint32_t
OMR::Z::Instruction::useSourceRegister(TR::Register * reg)
   {
   TR::Compilation *comp = self()->cg()->comp();

   if(_sourceStart < 0)
     {
     if(_targetStart < 0)
       _sourceStart = 0;
     else
       _sourceStart = _targetRegSize;
     }

   self()->recordOperand((void*) reg, _sourceRegSize);

   self()->useRegister(reg);

   // mark used bit for HW/LW virtual regs
   if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      if (_opcode.is64bit())
         {
         if (self()->getOpCodeValue() == TR::InstOpCode::RISBG || self()->getOpCodeValue() == TR::InstOpCode::RISBGN)
            {
            uint8_t startBit = ((TR::S390RIEInstruction* )self())->getSourceImmediate8One();
            uint8_t endBit = ((TR::S390RIEInstruction* )self())->getSourceImmediate8Two();
            uint8_t rotateAmnt = ((TR::S390RIEInstruction* )self())->getSourceImmediate8();
            if ((startBit + rotateAmnt) < 32 &&
                (endBit - rotateAmnt) > 63)
               {
               // if the sourceReg bit range was more than the low word
               reg->setIs64BitReg(true);
               }
            }
         else
            {
            reg->setIs64BitReg(true);
            if (reg->getRegisterPair())
               {
               reg->getLowOrder()->setIs64BitReg(true);
               reg->getHighOrder()->setIs64BitReg(true);
               }
            }
         }
      if (!self()->isHPRUpgradable(_targetRegSize+_sourceRegSize-1))
         {
         reg->setIsNotHighWordUpgradable(true);
         if (self()->cg()->getDebug())
            {
            //traceMsg(comp,"%s not upgradable in instruction [%p]\n", cg()->getDebug()->getName(reg),self());
            }
         }

      if (reg->getRegisterPair())
         {
         reg->getLowOrder()->setIsNotHighWordUpgradable(true);
         reg->getHighOrder()->setIsNotHighWordUpgradable(true);
         //traceMsg(comp,"\n%s:%s not upgradable for instruction [%p]\n",
         //        cg()->getDebug()->getName(reg->getLowOrder()), cg()->getDebug()->getName(reg->getHighOrder()), self());
         }
      }

   return _sourceRegSize-1;   // index into the source register array
   }

const char *
OMR::Z::Instruction::getName(TR_Debug * debug)
   {
   if (debug)
         return debug->getName(self());
      else
         return "<unknown instruction>";
   }

void
OMR::Z::Instruction::recordOperand(void *operand, int8_t &operandCount)
   {
   TR::Compilation *comp = self()->cg()->comp();
   uint32_t n = self()->numOperands();

   void **p = (void **) comp->allocator().allocate(sizeof(void *) * (n+1));

   if(_operands==NULL)
      {
      ((uintptrj_t** )p)[0]=(uintptrj_t *)operand;
      _operands=p;
      }
   else
      {
      for (int i=0; i < n; i++) ((uintptrj_t**)p)[i] = ((uintptrj_t **) _operands)[i];
      ((uintptrj_t** )p)[n]= (uintptrj_t *) operand;

      comp->allocator().deallocate(_operands, sizeof(void *) * n);
      _operands = p;
      }

   operandCount++;
   }

/*
 * Tests if instruction contains given register. More comprehensive containment search
 * than matchAnyRegister.
 */
bool OMR::Z::Instruction::containsRegister(TR::Register *reg)
   {
   TR::Register **_sourceReg = self()->sourceRegBase();
   TR::Register **_targetReg = self()->targetRegBase();
   TR::MemoryReference **_sourceMem = self()->sourceMemBase();
   TR::MemoryReference **_targetMem = self()->targetMemBase();
   int i;
   for (i = 0; i < _sourceRegSize; i++)
      if (_sourceReg[i]->usesAnyRegister(reg))
         return true;

   for (i = 0; i < _targetRegSize; i++)
      if (_targetReg[i]->usesAnyRegister(reg))
         return true;

   for (i = 0; i < _sourceMemSize; i++)
      if (_sourceMem[i]->usesRegister(reg))
         return true;

   for (i = 0; i < _targetMemSize; i++)
      if (_targetMem[i]->usesRegister(reg))
         return true;

   if (self()->isOutOfLineEX())
      {
      if (self()->getOpCodeValue() == TR::InstOpCode::EX)
         {
         TR::MemoryReference * tempMR = ((TR::S390RXInstruction *)self())->getMemoryReference();
         TR::S390ConstantInstructionSnippet * cis = (TR::S390ConstantInstructionSnippet *) tempMR->getConstantDataSnippet();
         TR_ASSERT( cis != NULL, "Out of line EX instruction doesn't have a constantInstructionSnippet\n");
         if (cis->getInstruction()->containsRegister(reg))
            return true;
         }
      else if (self()->getOpCodeValue() == TR::InstOpCode::EXRL)
         {
         TR::S390ConstantInstructionSnippet * cis = (TR::S390ConstantInstructionSnippet *) ((TR::S390RILInstruction *)self())->getTargetSnippet();
         TR_ASSERT( cis != NULL, "Out of line EXRL instruction doesn't have a constantInstructionSnippet\n");
         if (cis->getInstruction()->containsRegister(reg))
            return true;
         }
      }
   return false;
   }

uint32_t
OMR::Z::Instruction::useTargetRegister(TR::Register* reg)
   {
   TR::Compilation *comp = self()->cg()->comp();
   if(_targetStart < 0)
     {
     if(_sourceStart < 0)
       _targetStart = 0;
     else
       _targetStart = _sourceRegSize;
     }

   self()->recordOperand((void*) reg, _targetRegSize);

   // Invalidate CC if target kills any of the usesOnlyRegisters of ccInst
   TR::Instruction* ccInst = self()->cg()->ccInstruction();
   if (self()->cg()->hasCCInfo() &&
       (self() != ccInst) &&
       ccInst->containsRegister(reg))
      {
      ccInst->setCCuseKnown();
      self()->clearCCInfo();
      }

   // Set Redefined flag for Trivial Remat
   bool redefined=(reg->getStartOfRange() != NULL);
   TR::RegisterPair *rp=reg->getRegisterPair();
   if(rp)
     {
     TR::Register *lowReg=rp->getLowOrder();
     TR::Register *highReg=rp->getHighOrder();
     redefined |= (lowReg->getStartOfRange() != NULL) || (highReg->getStartOfRange() != NULL);
     lowReg->setRedefined(redefined);
     highReg->setRedefined(redefined);
     }
   reg->setRedefined(redefined);
   self()->useRegister(reg);

   // mark used bit for HW/LW virtual regs
   if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      if (_opcode.is64bit() || _opcode.is32to64bit() ||
          (TR::Compiler->target.is64Bit() &&
           (self()->getOpCodeValue() == TR::InstOpCode::LA || self()->getOpCodeValue() == TR::InstOpCode::LAY || self()->getOpCodeValue() == TR::InstOpCode::LARL ||
            self()->getOpCodeValue() == TR::InstOpCode::BASR || self()->getOpCodeValue() == TR::InstOpCode::BRASL)))
         {
         // RISBG case is handled in S390RIEInstruction constructor
         if (self()->getOpCodeValue() != TR::InstOpCode::RISBG && self()->getOpCodeValue() != TR::InstOpCode::RISBGN)
            {
            reg->setIs64BitReg(true);
            if (reg->getRegisterPair())
               {
               reg->getLowOrder()->setIs64BitReg(true);
               reg->getHighOrder()->setIs64BitReg(true);
               }
            }
         }
      if (!self()->isHPRUpgradable(_targetRegSize+_sourceRegSize-1))
         {
         reg->setIsNotHighWordUpgradable(true);
         if (self()->cg()->getDebug())
            {
            //traceMsg(comp,"%s not upgradable in instruction [%p]\n", cg()->getDebug()->getName(*reg),self());
            }
         }

      if (reg->getRegisterPair())
         {
         reg->getLowOrder()->setIsNotHighWordUpgradable(true);
         reg->getHighOrder()->setIsNotHighWordUpgradable(true);
         //traceMsg(comp,"\n%s:%s not upgradable for instruction [%p]\n",
         //        cg()->getDebug()->getName(reg->getLowOrder()), cg()->getDebug()->getName(reg->getHighOrder()), self());
         }
      }

   return _targetRegSize-1;      // index into the target register array
   }


// check if a particular instruction has a long displacment version
bool
OMR::Z::Instruction::hasLongDisplacementSupport()
  {
  // Includes all instructions with long displacement support - RXE, RX, RXY, RS, RSY, SI, SIY
  return self()->getOpCode().hasLongDispSupport();
  }

TR::InstOpCode::Mnemonic OMR::Z::Instruction::opCodeCanBeAdjustedTo(TR::InstOpCode::Mnemonic op)
   {
   switch (op)
      {
      case TR::InstOpCode::A:
         return TR::InstOpCode::AY;
      case TR::InstOpCode::AL:
         return TR::InstOpCode::ALY;
      case TR::InstOpCode::AH:
         return TR::InstOpCode::AHY;
      case TR::InstOpCode::C:
         return TR::InstOpCode::CY;
      case TR::InstOpCode::CH:
         return TR::InstOpCode::CHY;
      case TR::InstOpCode::CL:
         return TR::InstOpCode::CLY;
      case TR::InstOpCode::IC:
         return TR::InstOpCode::ICY;
      case TR::InstOpCode::L:
         return TR::InstOpCode::LY;
      case TR::InstOpCode::LA:
         return TR::InstOpCode::LAY;
      case TR::InstOpCode::LAE:
         return TR::InstOpCode::LAEY;
      case TR::InstOpCode::LAM:
         return TR::InstOpCode::LAMY;
      case TR::InstOpCode::LRA:
         return TR::InstOpCode::LRAY;
      case TR::InstOpCode::LH:
         return TR::InstOpCode::LHY;
      case TR::InstOpCode::MS:
         return TR::InstOpCode::MSY;
      case TR::InstOpCode::M:
         return TR::InstOpCode::MFY;
      case TR::InstOpCode::MH:
         return TR::InstOpCode::MHY;
      case TR::InstOpCode::N:
         return TR::InstOpCode::NY;
      case TR::InstOpCode::NI:
         return TR::InstOpCode::NIY;
      case TR::InstOpCode::O:
         return TR::InstOpCode::OY;
       case TR::InstOpCode::OI:
         return TR::InstOpCode::OIY;
      case TR::InstOpCode::S:
         return TR::InstOpCode::SY;
      case TR::InstOpCode::SH:
         return TR::InstOpCode::SHY;
      case TR::InstOpCode::SL:
         return TR::InstOpCode::SLY;
      case TR::InstOpCode::ST:
         return TR::InstOpCode::STY;
       case TR::InstOpCode::STAM:
         return TR::InstOpCode::STAMY;
      case TR::InstOpCode::STC:
         return TR::InstOpCode::STCY;
      case TR::InstOpCode::STH:
         return TR::InstOpCode::STHY;
      case TR::InstOpCode::X:
         return TR::InstOpCode::XY;
      case TR::InstOpCode::XI:
         return TR::InstOpCode::XIY;
      case TR::InstOpCode::LE:
         return TR::InstOpCode::LEY;
      case TR::InstOpCode::LD:
         return TR::InstOpCode::LDY;
      case TR::InstOpCode::STE:
         return TR::InstOpCode::STEY;
      case TR::InstOpCode::STD:
         return TR::InstOpCode::STDY;
      case TR::InstOpCode::LM:
         return TR::InstOpCode::LMY;
      case TR::InstOpCode::STM:
         return TR::InstOpCode::STMY;
      case TR::InstOpCode::STCM:
         return TR::InstOpCode::STCMY;
      case TR::InstOpCode::ICM:
         return TR::InstOpCode::ICMY;
      case TR::InstOpCode::TM:
         return TR::InstOpCode::TMY;
      case TR::InstOpCode::MVI:
         return TR::InstOpCode::MVIY;
      case TR::InstOpCode::CLI:
         return TR::InstOpCode::CLIY;
      case TR::InstOpCode::CVB:
         return TR::InstOpCode::CVBY;
      case TR::InstOpCode::CVD:
         return TR::InstOpCode::CVDY;
      case TR::InstOpCode::CS:
         return TR::InstOpCode::CSY;
      case TR::InstOpCode::CDS:
         return TR::InstOpCode::CDSY;
      default:
         return TR::InstOpCode::BAD;
      }
   }

// the following converts RX and RXE instructions into their
// corresponding long displacement instructions
void
OMR::Z::Instruction::attemptOpAdjustmentForLongDisplacement()
   {
   TR::InstOpCode::Mnemonic n_op = self()->opCodeCanBeAdjustedTo(self()->getOpCodeValue());
   if (n_op != TR::InstOpCode::BAD)
      self()->setOpCodeValue(n_op);

   if (self()->getOpCode().getInstructionFormat(self()->getOpCodeValue()) == RXY_FORMAT)
      self()->setKind(IsRXY);
   else if (self()->getOpCode().getInstructionFormat(self()->getOpCodeValue()) == RSY_FORMAT)
      self()->setKind(IsRSY);
   else if (self()->getOpCode().getInstructionFormat(self()->getOpCodeValue()) == SIY_FORMAT)   // LL: Add SIY
      self()->setKind(IsSIY);
   else if (self()->cg()->getDebug())
      TR_ASSERT(0, "only RX, RS and SI instructions can be safely mapped to long displacement: opCode: %s", self()->cg()->getDebug()->getOpCodeName(&self()->getOpCode()));
   else
      TR_ASSERT(0, "only RX, RS and SI instructions can be safely mapped to long displacement");

   }


uint32_t
OMR::Z::Instruction::useSourceMemoryReference(TR::MemoryReference * memRef)
   {
   TR::Compilation *comp = self()->cg()->comp();
   TR_ASSERT( !memRef->getMemRefUsedBefore(), "Memory Reference Used Before");
   memRef->setMemRefUsedBefore();

   self()->recordOperand(memRef, _sourceMemSize);

   memRef->bookKeepingRegisterUses(self(), self()->cg());

   if(!self()->cg()->afterRA())
     {
    int32_t disp = memRef->getOffset();
    if (self()->hasLongDisplacementSupport() &&
        !memRef->hasTemporaryNegativeOffset() &&
        (disp < 0 || disp >= MAXDISP) && (disp >= MINLONGDISP || disp <= MAXLONGDISP))
      {
      self()->attemptOpAdjustmentForLongDisplacement();
      }

     }
   return _sourceMemSize-1; // index into source memref array
   }

uint32_t
OMR::Z::Instruction::useTargetMemoryReference(TR::MemoryReference * memRef, TR::MemoryReference * sourceMemRef)
   {
   TR::Compilation *comp = self()->cg()->comp();
   TR_ASSERT( !memRef->getMemRefUsedBefore(), "Memory Reference Used Before");
   memRef->setMemRefUsedBefore();

   self()->recordOperand(memRef, _targetMemSize);

   memRef->bookKeepingRegisterUses(self(), self()->cg());

   if(!self()->cg()->afterRA())
     {
     }

   return _targetMemSize-1;      // index into the target memref array
   }


//  Groups regpairs at the front of the array and returns the number of pair elements
int
OMR::Z::Instruction::gatherRegPairsAtFrontOfArray(TR::Register** regArr, int32_t regArrSize)
   {
   int32_t pairCnt = 0;

   for (int32_t i = 0; i < regArrSize; i++)
      {
      if ((regArr[i])->getRegisterPair() != NULL)
         {
         TR::Register *tempReg = regArr[i];
         regArr[i] = regArr[pairCnt];
         regArr[pairCnt] = tempReg;

         pairCnt++;
         }
      }
   return pairCnt;
   }



TR::Register *
OMR::Z::Instruction::assignRegisterNoDependencies(TR::Register * reg)
   {
   // don't assign gpr in AR pass (preserve futureUseCount)
   if (self()->cg()->getRAPassAR() && reg->getKind() != TR_AR)
      return reg;
   TR::Compilation *comp = self()->cg()->comp();
   bool enableHighWordRA = self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA);

   // preventing assigning Real regs to Real regs
   if (reg->getRealRegister() != NULL)
      {
      TR::Register *virtReg = reg->getAssignedRegister();
      TR::RealRegister * realReg = reg->getRealRegister();

      // BCR R15/R14, GPR0 have hardcoded GPR0 that we should ignore.
      // The register is treated as a NOP branch, and is neither used nor defined.
      bool skipBookkeeping = false;
      if (self()->getOpCodeValue() == TR::InstOpCode::BCR &&
          toRealRegister(reg)->getRegisterNumber() == TR::RealRegister::GPR0 &&
          (((TR::S390RegInstruction*)self())->getBranchCondition() == TR::InstOpCode::COND_MASK14 ||
           ((TR::S390RegInstruction*)self())->getBranchCondition() == TR::InstOpCode::COND_MASK15))
         skipBookkeeping = true;

      if (!skipBookkeeping && realReg->getState() != TR::RealRegister::Locked &&
           BOOKKEEPING && virtReg && virtReg->decFutureUseCount() == 0)
        {
         virtReg->setAssignedRegister(NULL);
         realReg->setAssignedRegister(NULL);
         realReg->setState(TR::RealRegister::Free);
         if (enableHighWordRA && virtReg->getKind() != TR_FPR && virtReg->is64BitReg() && virtReg->getKind() != TR_VRF)
            {
            toRealRegister(realReg)->getHighWordRegister()->setAssignedRegister(NULL);
            toRealRegister(realReg)->getHighWordRegister()->setState(TR::RealRegister::Free);
            self()->cg()->traceRegFreed(virtReg, toRealRegister(realReg)->getHighWordRegister());
            }
         self()->cg()->traceRegFreed(virtReg, realReg);
         }

      return reg;
      }

   if ((reg->getRegisterPair() &&
        (reg->getRegisterPair()->getHighOrder()->getRealRegister() != NULL ||
         reg->getRegisterPair()->getLowOrder()->getRealRegister() != NULL)))
      {
      TR::RealRegister * realRegHigh = reg->getRegisterPair()->getHighOrder()->getRealRegister();
      TR::RealRegister * realRegLow = reg->getRegisterPair()->getLowOrder()->getRealRegister();
      TR::Register *virtRegHigh = realRegHigh->getAssignedRegister();
      TR::Register *virtRegLow = realRegLow->getAssignedRegister();

      if (realRegHigh->getState() != TR::RealRegister::Locked &&
           BOOKKEEPING && virtRegHigh && virtRegHigh->decFutureUseCount() == 0)
         {
         virtRegHigh->setAssignedRegister(NULL);
         realRegHigh->setAssignedRegister(NULL);
         realRegHigh->setState(TR::RealRegister::Free);
         if (enableHighWordRA && virtRegHigh->getKind() != TR_FPR && virtRegHigh->is64BitReg() && virtRegHigh->getKind() != TR_VRF)
            {
            toRealRegister(realRegHigh)->getHighWordRegister()->setAssignedRegister(NULL);
            toRealRegister(realRegHigh)->getHighWordRegister()->setState(TR::RealRegister::Free);
            self()->cg()->traceRegFreed(virtRegHigh, toRealRegister(realRegHigh)->getHighWordRegister());
            }
         self()->cg()->traceRegFreed(virtRegHigh, realRegHigh);
         }

      if (realRegLow->getState() != TR::RealRegister::Locked &&
          BOOKKEEPING && virtRegLow && virtRegLow->decFutureUseCount() == 0)
         {
         virtRegLow->setAssignedRegister(NULL);
         realRegLow->setAssignedRegister(NULL);
         realRegLow->setState(TR::RealRegister::Free);
         if (enableHighWordRA && virtRegLow->getKind() != TR_FPR && virtRegLow->is64BitReg() && virtRegLow->getKind() != TR_VRF)
            {
            toRealRegister(realRegLow)->getHighWordRegister()->setAssignedRegister(NULL);
            toRealRegister(realRegLow)->getHighWordRegister()->setState(TR::RealRegister::Free);
            self()->cg()->traceRegFreed(virtRegLow, toRealRegister(realRegLow)->getHighWordRegister());
            }
         self()->cg()->traceRegFreed(virtRegLow, realRegLow);
         }

      return reg;
      }

   return self()->cg()->machine()->assignBestRegister(reg, self(), BOOKKEEPING);
   }

// Make sure reg life is really ended.
//
bool
OMR::Z::Instruction::anySpilledRegisters(TR::Register ** listReg, int32_t listRegSize)
   {
   int32_t i;

   if (listReg==NULL)
      {
      return false;
      }

   for (i = 0; i < listRegSize; ++i)
      {
      TR::Register* reg = listReg[i];

      if ( reg->getRegisterPair() )
         {
         TR::Register * regH = reg->getHighOrder();
         if (regH->getAssignedRealRegister()==NULL &&
             regH->getTotalUseCount() != regH->getFutureUseCount()
            )
            {
            return true;
            }

         TR::Register * regL = reg->getLowOrder();
         if (regL->getAssignedRealRegister()==NULL &&
             regL->getTotalUseCount() != regL->getFutureUseCount()
            )
            {
            return true;
            }
         }
      else if (reg->getAssignedRealRegister()==NULL &&
               reg->getTotalUseCount() != reg->getFutureUseCount()
              )
         {
         return true;
         }
      }

   return false;
   }

bool
OMR::Z::Instruction::anySpilledRegisters(TR::MemoryReference ** listMem, int32_t listMemSize)
   {
   int32_t i;

   if (listMem==NULL)
      {
      return false;
      }

   for (i = 0; i < listMemSize; ++i)
      {
      TR::MemoryReference * memRef = listMem[i];
      TR::Register * reg;
      if ((reg = memRef->getBaseRegister())    &&
          reg->getAssignedRealRegister()==NULL &&
          (reg->getTotalUseCount() != reg->getFutureUseCount())
         )
         {
         return true;
         }
      if ((reg = memRef->getIndexRegister())   &&
          reg->getAssignedRealRegister()==NULL &&
          (reg->getTotalUseCount() != reg->getFutureUseCount())
         )
         {
         return true;
         }
      // for ar register pair spilling
      if ((reg = memRef->getBaseRegister()) &&
         reg->getRegisterPair())
         {
         TR::Register * regH = reg->getHighOrder();
         if (regH->getAssignedRealRegister()==NULL &&
             regH->getTotalUseCount() != regH->getFutureUseCount())
            return true;

         TR::Register * regL = reg->getLowOrder();
         if (regL->getAssignedRealRegister()==NULL &&
             regL->getTotalUseCount() != regL->getFutureUseCount())
            return true;
         }
      if ((reg = memRef->getIndexRegister()) &&
         reg->getRegisterPair())
         {
         TR::Register * regH = reg->getHighOrder();
         if (regH->getAssignedRealRegister()==NULL &&
             regH->getTotalUseCount() != regH->getFutureUseCount())
            return true;

         TR::Register * regL = reg->getLowOrder();
         if (regL->getAssignedRealRegister()==NULL &&
             regL->getTotalUseCount() != regL->getFutureUseCount())
            return true;
         }

      }

   return false;
   }

void
OMR::Z::Instruction::assignOrderedRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   int32_t i;
   int32_t numTrgtPairs = 0;
   bool blockTarget = true;
   TR::Compilation *comp = self()->cg()->comp();

   TR::Register **_sourceReg = self()->sourceRegBase();
   TR::Register **_targetReg = self()->targetRegBase();
   TR::MemoryReference **_sourceMem = self()->sourceMemBase();
   TR::MemoryReference **_targetMem = self()->targetMemBase();

   TR::Register * srcRegister = NULL;
   TR::Register * trgtRegister = NULL;
   TR::Machine * machine = self()->cg()->machine();
   TR::RealRegister *postInstrFreeRealReg = NULL;
   TR::Register *postInstrFreeReg = NULL;

   // All register pairs are to be assigned before all single registers.
   //
   // This is performed by passing over the register arrays twice (assigning
   // register pairs on the first pass and single registers on the second pass) but care
   // must be taken to avoid calling assignRegisterNoDependencies on the same index twice.
   //
   // For example, a GPR:AR register pair seems to be replaced with one real register. If
   // using a two pass method that only checks for 'if reg[i]->getRegisterPair()' on the
   // second pass, the following bug can occur:
   //
   // 1st pass: GPR:AR assigned to S390 REAL
   // 2nd pass: S390 REAL assigned to S390 REAL (instead of being skipped because it was assigned on the first pass)
   //
   // Unneccesary futureUseCount dec'ing would occur on the second pass if reg[i] wasn't skipped.
   //
   // In the *assigned arrays, register pairs are marked with a 2 and single registers are marked with a 1.
   //
   TR_ASSERT( _sourceRegSize <= 5, "_sourceRegSize > 5!");
   TR_ASSERT( _targetRegSize <= 5, "_targetRegSize > 5!");
   int8_t srcAssigned[5] = { 0, 0, 0, 0, 0 };
   int8_t tgtAssigned[5] = { 0, 0, 0, 0, 0 };
   int32_t registerOperandNum;

   for (i = 0; i < _targetRegSize; i++)
      {
      if (_targetReg[i]->getRegisterPair())
         {
         numTrgtPairs += 1;
         }
      }

   if((self()->getOpCodeValue() == TR::InstOpCode::XGR || self()->getOpCodeValue() == TR::InstOpCode::XR) && _sourceReg[0] == _targetReg[0])
     {
     postInstrFreeReg = _targetReg[0];
     }

   // Keep track of the first and second non-pair source registers for later
   // when determining if "setIs64BitReg" and "setAssignToHPR" should be called.
   TR::Register *  firstNonPairSourceRegister = 0;
   TR::Register * secondNonPairSourceRegister = 0;

   for (i = 0; i < _sourceRegSize; i++)
      {
      if (_sourceReg[i]->getRegisterPair())
         continue;

      if (firstNonPairSourceRegister == 0)
         {
         firstNonPairSourceRegister = _sourceReg[i];
         }
      else if (secondNonPairSourceRegister == 0)
         {
         secondNonPairSourceRegister = _sourceReg[i];
         break;
         }
      }

   if (firstNonPairSourceRegister == secondNonPairSourceRegister)
      {
      secondNonPairSourceRegister = 0;
      }

   //  If there is only 1 target register we don't need to block
   //
   if (numTrgtPairs==0 && _targetReg &&
       _targetRegSize==1 && _targetMem==NULL &&
       !self()->getOpCode().usesTarget() && //ii: !getOpCode().isRegCopy() &&
       !self()->anySpilledRegisters(_sourceReg, _sourceRegSize)  && !self()->anySpilledRegisters(_sourceMem, _sourceMemSize)
      )
      {
      blockTarget = false;
      self()->block(_sourceReg, _sourceRegSize,  NULL, 0,  self()->sourceMemBase(), self()->targetMemBase());
      }
   else
      {
      blockTarget = true;
      self()->block(_sourceReg, _sourceRegSize, _targetReg, _targetRegSize, self()->sourceMemBase(), self()->targetMemBase());
      }

   // Do regPair assignment first. Start with source pairs, then do target pairs
   for (i = 0; i < _targetRegSize; ++i)
      {
      if (!_targetReg[i]->getRegisterPair())
         continue;

      if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
         _targetReg[i]->setAssignToHPR(false);
      TR::Register *virtReg=_targetReg[i];
      _targetReg[i] = self()->assignRegisterNoDependencies(virtReg);
      _targetReg[i]->block();

      tgtAssigned[i] = 2;

      if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
         {
         // don't need to block HPR here beacuse no Highword instruction uses register pair
         // but make sure we do not spill to the targetReg's HPR (even if it became free) while assigning sourceReg
         if (_targetReg[i]->getRegisterPair() &&
             _targetReg[i]->getRegisterPair()->getHighOrder()->getKind() != TR_FPR &&
             _targetReg[i]->getRegisterPair()->getHighOrder()->getKind() != TR_VRF)
            {
            uint32_t availHPRMask = self()->cg()->getAvailableHPRSpillMask();
            availHPRMask &= ~(toRealRegister(_targetReg[i]->getRegisterPair()->getHighOrder())->getHighWordRegister()->getRealRegisterMask());
            availHPRMask &= ~(toRealRegister(_targetReg[i]->getRegisterPair()->getLowOrder())->getHighWordRegister()->getRealRegisterMask());
            self()->cg()->setAvailableHPRSpillMask(availHPRMask);
            }
         }
      }
   for (i = 0; i < _sourceRegSize; ++i)
      {
      if (!_sourceReg[i]->getRegisterPair())
         continue;

      if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
         (_sourceReg[i])->setAssignToHPR(false);
      (_sourceReg[i]) = self()->assignRegisterNoDependencies(_sourceReg[i]);
      (_sourceReg[i])->block();

      srcAssigned[i] = 2;
      }
   bool blockTargetHighword = false;
   bool targetRegIs64Bit = false;
   // Assign all registers, blocking assignments as you go
   //
   if (_targetReg)
      {
      for (i = 0; i < _targetRegSize; ++i)
         {
         if (tgtAssigned[i] != 0)
            continue;

         registerOperandNum = (_targetReg < _sourceReg) ? i+1 : _sourceRegSize+i+1;

         /*
           When we are mixing 32 and 64 bit registers, we have to ensure that 64 bit instructions use 64 bit registers.
           If this assertion is triggered, it is probably because an evaluator called allocateRegister() when it should
           have called allocate64bitRegister().
          */
         if ((_opcode.is64bit() || _opcode.is32to64bit()) && self()->cg()->use64BitRegsOn32Bit() && TR::Compiler->target.is32Bit() &&
             !_targetReg[i]->getRealRegister()) // skip check if the target reg is already a real register.
            {
            TR_ASSERT(_targetReg[i]->getKind() != TR_GPR, "Using GPR for 64 bit instruction %s [%s] in 32 bit mode",
                      self()->cg()->getDebug()->getOpCodeName(&_opcode),
                      self()->cg()->getDebug()->getName(self()));
            }

         if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
            {
            if (_opcode.is64bit() || _opcode.is32to64bit() ||
                (TR::Compiler->target.is64Bit() &&
                 (self()->getOpCodeValue() == TR::InstOpCode::LA || self()->getOpCodeValue() == TR::InstOpCode::LAY || self()->getOpCodeValue() == TR::InstOpCode::LARL ||
                  self()->getOpCodeValue() == TR::InstOpCode::BASR || self()->getOpCodeValue() == TR::InstOpCode::BRASL)))
               {
               if (self()->getOpCodeValue() == TR::InstOpCode::RISBG || self()->getOpCodeValue() == TR::InstOpCode::RISBGN)
                  {
                  uint8_t endBit = ((TR::S390RIEInstruction *)self())->getSourceImmediate8Two();
                  if (endBit & 0x80) // if the zero bit is set, target reg will be 64bit
                     {
                     _targetReg[i]->setIs64BitReg(true);
                     }
                  }
               else
                  {
                  _targetReg[i]->setIs64BitReg(true);
                  }
               }
            if ((self()->getOpCodeValue() == TR::InstOpCode::RISBLG || self()->getOpCodeValue() == TR::InstOpCode::RISBHG) &&
                ((TR::S390RIEInstruction *)self())->getExtendedHighWordOpCode().getOpCodeValue() != TR::InstOpCode::BAD)
               {
               if (((TR::S390RIEInstruction *)self())->getExtendedHighWordOpCode().isOperandHW(registerOperandNum))
                  _targetReg[i]->setAssignToHPR(true);
               else
                  _targetReg[i]->setAssignToHPR(false);
               }
            else if (_opcode.isOperandHW(registerOperandNum))
               _targetReg[i]->setAssignToHPR(true);
            else
               _targetReg[i]->setAssignToHPR(false);
            }

         if (_targetReg[i]->is64BitReg() && _targetReg[i]->getRealRegister() == NULL &&
             _targetReg[i]->getKind() != TR_FPR && _targetReg[i]->getKind() != TR_VRF)
            {
            targetRegIs64Bit = true;
            }
         _targetReg[i] = self()->assignRegisterNoDependencies(_targetReg[i]);

         if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
             _targetReg[i]->getKind() != TR_FPR && _targetReg[i]->getKind() != TR_VRF)
            {
            if (toRealRegister(_targetReg[i])->getState() == TR::RealRegister::Free && targetRegIs64Bit)
               {
               blockTargetHighword = true;
               toRealRegister(_targetReg[i])->getHighWordRegister()->setState(TR::RealRegister::Blocked);
               TR_ASSERT(i==0, "more than 1 HPR target regs need to be blocked?");
               }

            // make sure we do not spill to the targetReg's HPR later (even if it's free)
            uint32_t availHPRMask = self()->cg()->getAvailableHPRSpillMask();
            availHPRMask &= ~(toRealRegister(_targetReg[i])->getHighWordRegister()->getRealRegisterMask());
            self()->cg()->setAvailableHPRSpillMask(availHPRMask);
            }

         if (_targetReg[i]->getAssignedRegister())
            {
            _targetReg[i]->getAssignedRegister()->block();
            }
         else if (blockTarget)
            {
            _targetReg[i]->block();
            }

         tgtAssigned[i] = 1;
         }
      }

   // block non-pair targetRegs again before assigning srcReg
   // "The solution is to unblock all _targetRegs before PreCond assignment and reblock them right after (required for srcReg assignment)."
   // "We only unblock those target registers that are not alive anymore. Block them again after pre conds are done."
   if (_targetReg && blockTarget)
      {
      for (i = 0; i < _targetRegSize; ++i)
         {
         // All register pairs will have been assigned already, so skip those.
         if (tgtAssigned[i] == 2)
            continue;

         if (!_targetReg[i]->getAssignedRegister())
            {
            _targetReg[i]->unblock();
            }
         }
      }

   if (self()->getDependencyConditions())
      {
      self()->getDependencyConditions()->assignPreConditionRegisters(self()->getPrev(), kindToBeAssigned, self()->cg());
      }

   // unblock blocked targetRegs
   if (_targetReg && blockTarget)
      {
      for (i = 0; i < _targetRegSize; ++i)
         {
         // All register pairs will have been assigned already, so skip those.
         if (tgtAssigned[i] == 2)
            continue;

         if (!_targetReg[i]->getAssignedRegister())
            {
            _targetReg[i]->block();
            }
         }
      }

   if (_sourceReg)
      {
      registerOperandNum = (_targetReg < _sourceReg) ? _targetRegSize+1 : 1;
      if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
         {
         if (firstNonPairSourceRegister)
            {
            if (_opcode.is64bit())
               {
               if (self()->getOpCodeValue() == TR::InstOpCode::RISBG || self()->getOpCodeValue() == TR::InstOpCode::RISBGN)
                  {
                  uint8_t startBit = ((TR::S390RIEInstruction* )self())->getSourceImmediate8One();
                  uint8_t endBit = ((TR::S390RIEInstruction* )self())->getSourceImmediate8Two();
                  uint8_t rotateAmnt = ((TR::S390RIEInstruction* )self())->getSourceImmediate8();
                  if ((startBit + rotateAmnt) < 32 &&
                      (endBit - rotateAmnt) > 63)
                     {
                     // if the sourceReg bit range was more than the low word
                     firstNonPairSourceRegister->setIs64BitReg(true);
                     }
                  }
               else
                  {
                  firstNonPairSourceRegister->setIs64BitReg(true);
                  }
               }

            if ((self()->getOpCodeValue() == TR::InstOpCode::RISBLG || self()->getOpCodeValue() == TR::InstOpCode::RISBHG) &&
                ((TR::S390RIEInstruction *)self())->getExtendedHighWordOpCode().getOpCodeValue() != TR::InstOpCode::BAD)
               {
               firstNonPairSourceRegister->setAssignToHPR(((TR::S390RIEInstruction *)self())->getExtendedHighWordOpCode().isOperandHW(registerOperandNum));
               }
            else
               firstNonPairSourceRegister->setAssignToHPR(_opcode.isOperandHW(registerOperandNum));
            }

         if (secondNonPairSourceRegister)
            {
            secondNonPairSourceRegister->setAssignToHPR(_opcode.isOperandHW(registerOperandNum+1));
            }
         }

      for (i = 0; i < _sourceRegSize; ++i)
         {
         if (srcAssigned[i] != 0)
            continue;

         TR::Register *r = self()->assignRegisterNoDependencies(_sourceReg[i]);
         _sourceReg[i] = r;
         (_sourceReg[i])->block();

         srcAssigned[i] = 1;
         }
      }

   if (_sourceMem)
      {
      for (i = 0; i < _sourceMemSize; ++i)
         {
         if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
            {
            if (_sourceMem[i]->getBaseRegister())
               {
               _sourceMem[i]->getBaseRegister()->setAssignToHPR(false);
               }
            if (_sourceMem[i]->getIndexRegister())
               {
               _sourceMem[i]->getIndexRegister()->setAssignToHPR(false);
               }
            }
         _sourceMem[i]->assignRegisters(self(), self()->cg());
         _sourceMem[i]->blockRegisters();
         }
      }
   if (_targetMem)
      {
      for (i = 0; i < _targetMemSize; ++i)
         {
         if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
            {
            if (_targetMem[i]->getBaseRegister())
               {
               _targetMem[i]->getBaseRegister()->setAssignToHPR(false);
               }
            if (_targetMem[i]->getIndexRegister())
               {
               _targetMem[i]->getIndexRegister()->setAssignToHPR(false);
               }
            }
         _targetMem[i]->assignRegisters(self(), self()->cg());
         _targetMem[i]->blockRegisters();
         }
      }


   if (blockTargetHighword)
      {
      toRealRegister(_targetReg[0])->getHighWordRegister()->setState(TR::RealRegister::Free);
      }
   // Unblock everything, as we are done assigning for this instr
   self()->unblock(_sourceReg, _sourceRegSize,  _targetReg, _targetRegSize, _sourceMem, _targetMem);
   /* Unblock registers that were Assigned and then Blocked in deeper stack frames of RA (src/target is HPR/GPR). */
   TR::RealRegister * reg = NULL;
   while(reg = self()->cg()->machine()->getNextRegFromUpgradedBlockedList())
      {
      self()->cg()->traceRegisterAssignment("Restoring blocked register %R (0x%p) to previous state", reg, reg);
      reg->unblock();
      }
   }

void
OMR::Z::Instruction::assignRegistersAndDependencies(TR_RegisterKinds kindToBeAssigned)
   {
   //

   TR::Register **_sourceReg = self()->sourceRegBase();
   TR::Register **_targetReg = self()->targetRegBase();
   TR::MemoryReference **_sourceMem = self()->sourceMemBase();
   TR::MemoryReference **_targetMem = self()->targetMemBase();

   // RIE for CompareAndBranch
   if (self()->getOpCode().isBranchOp() && self()->getKind() == IsRIE &&
       ((TR::S390RIEInstruction*)self())->getLabelSymbol()->isStartOfColdInstructionStream())
      {
      // Switch to the outlined instruction stream and assign registers.
      //
      TR_S390OutOfLineCodeSection *oi = self()->cg()->findS390OutOfLineCodeSectionFromLabel(((TR::S390RIEInstruction*)self())->getLabelSymbol());
      TR_ASSERT(oi, "Could not find S390OutOfLineCodeSection stream from label.  instr=%p, label=%p\n", self(), ((TR::S390RIEInstruction*)self())->getLabelSymbol());
      if (!oi->hasBeenRegisterAssigned())
         oi->assignRegisters(kindToBeAssigned);
      }
   // If there are any dependency conditions on this instruction, apply them.
   // Any register or memory references must be blocked before the condition
   // is applied, then they must be subsequently unblocked.
   //
   if (self()->getDependencyConditions())
      {
      self()->block(_sourceReg, _sourceRegSize, _targetReg, _targetRegSize, _sourceMem, _targetMem);
      self()->getDependencyConditions()->assignPostConditionRegisters(self(), kindToBeAssigned, self()->cg());

      // FPR and VRF register files overlap. Some of the FPRs are non-volatile but if they hold vector data
      // that part of the data would not be preserved. This code looks at the data stored in non-volatile FPRs
      // and if it is assigned a vector virtual then it needs to be killed.
      if(self()->cg()->getSupportsVectorRegisters() && self()->isCall())
        {
        TR::Machine *machine=self()->cg()->machine();
        TR::Register *dummy = self()->cg()->allocateRegister(TR_FPR);
        TR::Linkage *linkage = self()->cg()->getLinkage();
        dummy->setPlaceholderReg();
        for (int32_t i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
          {
          if (linkage->getPreserved(REGNUM(i)))
            {
            TR::RealRegister * targetRegister = machine->getS390RealRegister(REGNUM(i));
            TR::Register * assignedReg = targetRegister->getAssignedRegister();
            if(assignedReg && assignedReg->getKind() != TR_FPR)
              {
              machine->coerceRegisterAssignment(self(),dummy,REGNUM(i),DEPSREG);
              assignedReg->setAssignedRegister(NULL);
              targetRegister->setAssignedRegister(NULL);
              targetRegister->setState(TR::RealRegister::Unlatched);
              }
            }
          }
        self()->cg()->stopUsingRegister(dummy);
        }

      self()->unblock(_sourceReg, _sourceRegSize, _targetReg, _targetRegSize, _sourceMem, _targetMem);
      }

   self()->assignOrderedRegisters(kindToBeAssigned);

   // Compute bit vector of free regs
   //  if none found, find best spill register
   self()->assignFreeRegBitVector();
   }

int32_t
OMR::Z::Instruction::renameRegister(TR::Register *from, TR::Register *to)
  {
  int32_t j,n,i;
  int32_t numReplaced=0;

  // Do the register operands
  int32_t numRegOperands = self()->getNumRegisterOperands();
  for (j = 0; j < numRegOperands; j++)
    {
    if(self()->getRegisterOperand(j + 1) == from)
      {
      self()->setRegisterOperand(j + 1, to);
      numReplaced++;
      }
    }

  // Now do the Memory operands
  TR::MemoryReference *memRef1 = self()->getMemoryReference();
  TR::MemoryReference *memRef2 = self()->getMemoryReference2();
  if (memRef1)
    {
    if (memRef1->getBaseRegister() == from)
      {
      memRef1->setBaseRegister(to, self()->cg());
      numReplaced++;
      }
    if (memRef1->getIndexRegister() == from)
      {
      memRef1->setIndexRegister(to);
      numReplaced++;
      }
    }
  if (memRef2)
    {
    if (memRef2->getBaseRegister() == from)
      {
      memRef2->setBaseRegister(to, self()->cg());
      numReplaced++;
      }
    if (memRef2->getIndexRegister() == from)
      {
      memRef2->setIndexRegister(to);
      numReplaced++;
      }
    }

  TR::RegisterDependencyConditions *conds = self()->getDependencyConditions();
  if (conds && !conds->getConflictsResolved())
    {
    TR_S390RegisterDependencyGroup *preConds = conds->getPreConditions();
    TR_S390RegisterDependencyGroup *postConds = conds->getPostConditions();

    n = conds->getNumPreConditions();
    for (i = 0; i < n; i++)
      {
      TR::RegisterDependency* dep = preConds->getRegisterDependency(i);
      TR::Register *r = dep->getRegister(self()->cg());
      if (r == from)
        {
        dep->setRegister(to);
        numReplaced += preConds->getNumUses();
        }
      }
    n = conds->getNumPostConditions();
    for (i = 0; i < n; i++)
      {
      TR::RegisterDependency* dep = postConds->getRegisterDependency(i);
      TR::Register *r = dep->getRegister(self()->cg());
      if (r == from)
        {
        dep->setRegister(to);
        numReplaced += postConds->getNumUses();
        }
      }
    }

  return numReplaced;
  }


void
OMR::Z::Instruction::blockHPR(TR::Register * reg)
   {
   TR::Compilation *comp = self()->cg()->comp();
   if (reg->getKind() != TR_FPR && reg->getKind() != TR_VRF &&
       self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      if (reg->is64BitReg() && reg->getAssignedRegister() != NULL)
         {
         TR::RealRegister *assignedReg = reg->getAssignedRegister()->getRealRegister();
         if (assignedReg != NULL)
            {
            if (toRealRegister(assignedReg)->getHighWordRegister()->getState() == TR::RealRegister::Assigned)
               {
               toRealRegister(assignedReg)->getHighWordRegister()->setState(TR::RealRegister::Blocked);
               }
            }
         }
      }
   }

void
OMR::Z::Instruction::unblockHPR(TR::Register * reg)
   {
   TR::Compilation *comp = self()->cg()->comp();
   if (reg->getKind() != TR_FPR && reg->getKind() != TR_VRF &&
       self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      if (reg->is64BitReg() && reg->getAssignedRegister() != NULL)
         {
         TR::RealRegister *assignedReg = reg->getAssignedRegister()->getRealRegister();
         if (assignedReg != NULL)
            {
            if (toRealRegister(assignedReg)->getHighWordRegister()->getState() == TR::RealRegister::Blocked)
               {
               toRealRegister(assignedReg)->getHighWordRegister()->setState(TR::RealRegister::Assigned, reg->isPlaceholderReg());
               }
            }
         }
      }
   }

void
OMR::Z::Instruction::block(TR::Register **sourceReg, int sourceRegSize, TR::Register** targetReg, int targetRegSize,
   TR::MemoryReference ** sourceMem, TR::MemoryReference ** targetMem)
   {
   int32_t i;


   for (i = 0; i < targetRegSize; ++i)
      {
      (targetReg[i])->block();
      self()->blockHPR(targetReg[i]);
      }

   for (i = 0; i < sourceRegSize; ++i)
      {
      (sourceReg[i])->block();
      self()->blockHPR(sourceReg[i]);
      }

   for (i = 0; i < _targetMemSize; ++i)
      {
      targetMem[i]->blockRegisters();
      if (targetMem[i]->getBaseRegister())
         {
         self()->blockHPR(targetMem[i]->getBaseRegister());
         }
      if (targetMem[i]->getIndexRegister())
         {
         self()->blockHPR(targetMem[i]->getIndexRegister());
         }
      }

   for (i = 0; i < _sourceMemSize; ++i)
      {
      sourceMem[i]->blockRegisters();
      if (sourceMem[i]->getBaseRegister())
         {
         self()->blockHPR(sourceMem[i]->getBaseRegister());
         }
      if (sourceMem[i]->getIndexRegister())
         {
         self()->blockHPR(sourceMem[i]->getIndexRegister());
         }
      }
   }



void
OMR::Z::Instruction::unblock(TR::Register **sourceReg, int sourceRegSize, TR::Register** targetReg, int targetRegSize,
   TR::MemoryReference ** sourceMem, TR::MemoryReference ** targetMem)
   {
   int32_t i;


   for (i = 0; i < targetRegSize; ++i)
      {
      targetReg[i]->unblock();
      self()->unblockHPR(targetReg[i]);
      }

   for (i = 0; i < sourceRegSize; ++i)
      {
      (sourceReg[i])->unblock();
      self()->unblockHPR(sourceReg[i]);
      }

   for (i = 0; i < _targetMemSize; ++i)
      {
      targetMem[i]->unblockRegisters();
      if (targetMem[i]->getBaseRegister())
         {
         self()->unblockHPR(targetMem[i]->getBaseRegister());
         }
      if (targetMem[i]->getIndexRegister())
         {
         self()->unblockHPR(targetMem[i]->getIndexRegister());
         }
      }

   for (i = 0; i < _sourceMemSize; ++i)
      {
      sourceMem[i]->unblockRegisters();
      if (sourceMem[i]->getBaseRegister())
         {
         self()->unblockHPR(sourceMem[i]->getBaseRegister());
         }
      if (sourceMem[i]->getIndexRegister())
         {
         self()->unblockHPR(sourceMem[i]->getIndexRegister());
         }
      }
   }

// Logic to check if we must disallow assignment of GPR0 to a given register.
bool
OMR::Z::Instruction::checkRegForGPR0Disable(TR::InstOpCode::Mnemonic op, TR::Register * reg)
   {
   // If source reg is used by RR Inst and is branchOp, we need to make sure
   // GPR0 is not assigned.
   // However, in some cases, we may be passed in a real register
   //    i.e. BCR 15, R0 for serialization

   TR_ASSERT(reg!=NULL, "Reg cannot be null!");

   if (reg->getRealRegister() == NULL &&
         ((self()->getOpCode().isBranchOp() && self()->getOpCode().getInstructionFormat() == RR_FORMAT) || self()->getOpCodeValue() == TR::InstOpCode::EX || self()->getOpCodeValue() == TR::InstOpCode::EXRL))
      {
      reg->setIsUsedInMemRef();
      return true;
      }

   return false;
   }

void OMR::Z::Instruction::resetUseDefRegisters()
   {
   if (_useRegs != NULL)
      {
      _useRegs->MakeEmpty();
      }
   if (_defRegs != NULL)
      {
      _defRegs->MakeEmpty();
      }
   }


// This function collects all registers used by instruction
// in _useRegs array. It also collect registers that will be written to
// by an instruction in _defRegs.
// it grabs regs from _sourceReg,_targetReg,_sourceMem,_targetMem arrays
// note that for store-type instructions, for example ST Rx,...
// reg Rx is recorded in _targetReg even tough it's really a source reg
// in this case Rx will be recorded in _useRegs from _targetReg
// the same goes for compare instructions.
// all registers from both _sourceMem and _targetMem are recorded in _useRegs array.

void
OMR::Z::Instruction::setUseDefRegisters(bool updateDependencies)
   {
   TR::Compilation *comp = self()->cg()->comp();
   TR::Register **_sourceReg = self()->sourceRegBase();
   TR::Register **_targetReg = self()->targetRegBase();
   TR::MemoryReference **_sourceMem = self()->sourceMemBase();
   TR::MemoryReference **_targetMem = self()->targetMemBase();

   uint32_t indexSource=0, indexTarget=0,i;
   TR::RegisterPair * regPair;
   TR::RegisterDependencyConditions * dependencies = self()->getDependencyConditions();
   if (self()->cg()->getCodeGeneratorPhase() == TR::CodeGenPhase::PeepholePhase)
      {
      //In Peephole Phase, UseDefRegisters need to reset. The check on non Peephole Phase needs to be avoided here.
      if ((_useRegs != NULL && _useRegs->size() != 0) || (_defRegs != NULL && _defRegs->size() != 0))
         {}
      else
         _sourceUsedInMemoryReference = new (self()->cg()->comp()->allocator()) RegisterBitVector(self()->cg()->comp());
      //allocate arrays
      }
   else
      {
      TR_ASSERT(  ((_useRegs == NULL || _useRegs->size() == 0) || (_defRegs == NULL || _defRegs->size() == 0)), "source&target registers have already been set\n" );
      if ((_useRegs != NULL && _useRegs->size() != 0) || (_defRegs != NULL && _defRegs->size() != 0))
         return;
      _sourceUsedInMemoryReference = new (self()->cg()->comp()->allocator()) RegisterBitVector(self()->cg()->comp());
      //allocate arrays
      }

   // The check for isStore() || isCompare() || isTrap() should not hurt but should unnecessary as each instruction
   // will put used register operand 1 in source array
   if ((_sourceReg!=NULL) || (_sourceMem!=NULL) || (_targetMem!=NULL) ||
       (self()->implicitlyUsesGPR0() || self()->implicitlyUsesGPR1() || self()->implicitlyUsesGPR2()) ||
       ((_targetReg!=NULL) && (self()->isStore() || self()->isCompare() || self()->isTrap() || self()->getOpCode().usesTarget() )))
      {
      if (_useRegs == NULL)
         {
         _useRegs = new (self()->cg()->comp()->allocator()) RegisterArray<TR::Register *>(self()->cg()->comp());
         }
      }
   if ((self()->implicitlySetsGPR0() || self()->implicitlySetsGPR1() || self()->implicitlySetsGPR2()) ||
        ((_targetReg!=NULL) && ((!self()->isStore() && (!self()->isCompare()) && (!self()->isTrap()) && (self()->getOpCodeValue()!=TR::InstOpCode::PPA)) || self()->isLoad()) || (dependencies!=NULL)))
      {
      if (_defRegs == NULL)
         {
         _defRegs = new (self()->cg()->comp()->allocator()) RegisterArray<TR::Register *>(self()->cg()->comp());
         }
      }

   int32_t loadOrStoreMultiple = -1; // -1 unknown, 0 loadmultiple and 1 storemultiple
   TR::Register *firstReg = NULL;
   TR::Register *lastReg = NULL;
   if (_sourceReg)
      {
      for (i = 0; i < _sourceRegSize; ++i)
         {
         regPair = (_sourceReg[i])->getRegisterPair();
         if (regPair != NULL)
            {
            (*_useRegs)[indexSource++] = regPair->getHighOrder();
            (*_useRegs)[indexSource++] = regPair->getLowOrder();
            // register pair and both registers have been recorded
            // for load/store multiple
            }
         else
            {
            // TO-DO:  generalize the TR::InstOpCode::EDMK  support to something
            // more generic like "usesImpliedSource and setsImpliedTarget"
            // (rather than handing a specific op-code)
            if ((self()->getOpCodeValue() == TR::InstOpCode::CPSDR) && (i == 1))
               {
               (*_defRegs)[indexTarget++] = _sourceReg[i];
               }
            else if (self()->getOpCodeValue() == TR::InstOpCode::EDMK)
               {
               (*_useRegs)[indexSource++] = _sourceReg[i];
               }
            else if (self()->getOpCodeValue() != TR::InstOpCode::LM && self()->getOpCodeValue() != TR::InstOpCode::LMG && self()->getOpCodeValue() != TR::InstOpCode::LMH &&
                self()->getOpCodeValue() != TR::InstOpCode::LAM && self()->getOpCodeValue() != TR::InstOpCode::LMY)
               {
               (*_useRegs)[indexSource++] = _sourceReg[i];
               }
            else
               {
               (*_defRegs)[indexTarget++] = _sourceReg[i];
               }
            }
         }
      }
   if (_targetReg)
      {
      for (i = 0; i < _targetRegSize; ++i)
         {
         regPair = _targetReg[i]->getRegisterPair();
         if (regPair != NULL)
            {
            // if it's a store or compare instruction, put all regs in _useRegs
            if (self()->isStore() || self()->isCompare() || self()->isTrap() || self()->getOpCode().usesTarget())
               {
               (*_useRegs)[indexSource++] = regPair->getHighOrder();
               (*_useRegs)[indexSource++] = regPair->getLowOrder();
               }
            if ((!self()->isStore() && !self()->getOpCode().setsCompareFlag()) || self()->isLoad())
               {
               (*_defRegs)[indexTarget++] = regPair->getHighOrder();
               (*_defRegs)[indexTarget++] = regPair->getLowOrder();
               }
            }
         else
            {
            // if it's a store or compare instruction, put all regs in _useRegs
            if ((self()->isStore()  || self()->isCompare() || self()->isTrap() || self()->getOpCode().usesTarget() || (self()->getOpCodeValue() == TR::InstOpCode::CPSDR)) && (self()->getOpCodeValue() != TR::InstOpCode::EDMK) && (self()->getOpCodeValue() != TR::InstOpCode::TRTR))
               {
               (*_useRegs)[indexSource++] = _targetReg[i];
               }
            if ((!self()->isStore() && !self()->isCompare() && !self()->isTrap() && (self()->getOpCodeValue() != TR::InstOpCode::CPSDR) && (self()->getOpCodeValue() != TR::InstOpCode::PPA)) || self()->isLoad() || (self()->getOpCodeValue() == TR::InstOpCode::EDMK) || (self()->getOpCodeValue() == TR::InstOpCode::TRTR))
               {
               (*_defRegs)[indexTarget++] = _targetReg[i];
               }
            }
         }
      }

   if (self()->getOpCodeValue() == TR::InstOpCode::LM || self()->getOpCodeValue() == TR::InstOpCode::LMG || self()->getOpCodeValue() == TR::InstOpCode::LMH ||
       self()->getOpCodeValue() == TR::InstOpCode::LAM || self()->getOpCodeValue() == TR::InstOpCode::LMY)
     loadOrStoreMultiple = 0; //load
   else if (self()->getOpCodeValue() == TR::InstOpCode::STM || self()->getOpCodeValue() == TR::InstOpCode::STMG || self()->getOpCodeValue() == TR::InstOpCode::STMH ||
            self()->getOpCodeValue() == TR::InstOpCode::STAM || self()->getOpCodeValue() == TR::InstOpCode::STMY)
     loadOrStoreMultiple = 1; //store

   if (loadOrStoreMultiple >= 0)
     {
     firstReg = self()->getRegisterOperand(1);
     TR::RegisterPair *regPair=firstReg->getRegisterPair();
     if(regPair)
       {
       firstReg = regPair->getHighOrder();
       lastReg = regPair->getLowOrder();
       }
     else
       lastReg = self()->getRegisterOperand(2);

     if (lastReg->getRealRegister() && firstReg->getRealRegister())
      {
      TR::RealRegister *lReg = NULL;
      TR::RealRegister *fReg = NULL;
      if (lastReg->getRealRegister())
         {
         lReg = toRealRegister(lastReg);
         fReg = toRealRegister(firstReg);
         }
      else
         {
         lReg = toRealRegister(lastReg->getAssignedRealRegister());
         fReg = toRealRegister(firstReg->getAssignedRealRegister());
         }

      bool isAR = ((self()->getOpCodeValue() == TR::InstOpCode::LAM) || (self()->getOpCodeValue() == TR::InstOpCode::STAM));
      uint32_t lowRegNum = ANYREGINDEX(fReg->getRegisterNumber());
      uint32_t highRegNum = ANYREGINDEX(lReg->getRegisterNumber());

      int32_t numRegs = 0;
      if (highRegNum >= lowRegNum)
         numRegs = (highRegNum - lowRegNum + 1);
      else
         numRegs = (16 - lowRegNum) + highRegNum + 1;

      if (numRegs > 0)
         {
         lowRegNum = (lowRegNum == 15) ? 0 : lowRegNum + 1;
         for (uint32_t i = lowRegNum; i != highRegNum; i = ((i == 15) ? 0 : i + 1))  // wrap around to 0 at 15
            {
            TR::RealRegister *reg = self()->cg()->machine()->getS390RealRegister(i + (isAR ? TR::RealRegister::AR0 : TR::RealRegister::GPR0));
            if (loadOrStoreMultiple == 0) // load
               (*_defRegs)[indexTarget++] = reg;
            else if (loadOrStoreMultiple == 1) // store
               (*_useRegs)[indexSource++] = reg;
            }
         }
      }
   }

   //collect source registers from source memory references
   if (_sourceMem)
      {
      for (i = 0; i < _sourceMemSize; ++i)
         {
         if (_sourceMem[i]->getBaseRegister()!=NULL)
            {
            _sourceUsedInMemoryReference->set(indexSource);
            (*_useRegs)[indexSource++]=_sourceMem[i]->getBaseRegister();
            }
         if (_sourceMem[i]->getIndexRegister()!=NULL)
            {
            _sourceUsedInMemoryReference->set(indexSource);
            (*_useRegs)[indexSource++]=_sourceMem[i]->getIndexRegister();
            }
         }
      }
      //collect source registers from target memory references
   if (_targetMem)
      {
      for (i = 0; i < _targetMemSize; ++i)
         {
         if (_targetMem[i]->getBaseRegister()!=NULL)
            {
            _sourceUsedInMemoryReference->set(indexSource);
            (*_useRegs)[indexSource++]=_targetMem[i]->getBaseRegister();
            }
         if (_targetMem[i]->getIndexRegister()!=NULL)
            {
            _sourceUsedInMemoryReference->set(indexSource);
            (*_useRegs)[indexSource++]=_targetMem[i]->getIndexRegister();
            }
         }
      }

   if (updateDependencies)
      {
      // dont look at the dependencies for anything other than TR::InstOpCode::DEPEND
      // to avoid double entries due to register pair dependencies
      TR::Register *tempRegister;
      if ((dependencies!=NULL) && ((self()->getOpCode().getOpCodeValue() == TR::InstOpCode::DEPEND)))
         {
         TR_S390RegisterDependencyGroup *preConditions  = dependencies->getPreConditions();
         TR_S390RegisterDependencyGroup *postConditions = dependencies->getPostConditions();
         if (preConditions!=NULL)
            {
            for (i = 0; i < dependencies->getNumPreConditions(); i++)
               {
              tempRegister=preConditions->getRegisterDependency(i)->getRegister(self()->cg());
              if (tempRegister && tempRegister->getRegisterPair()==NULL)
                (*_defRegs)[indexTarget++]=tempRegister;
               }
            }
         if (postConditions!=NULL)
            {
            for (i = 0; i < dependencies->getNumPostConditions(); i++)
               {
             tempRegister=postConditions->getRegisterDependency(i)->getRegister(self()->cg());
              if (tempRegister && tempRegister->getRegisterPair()==NULL)
                (*_defRegs)[indexTarget++]=tempRegister;
               }
            }
         }
      }

   // set all HPRs to alias GPRs
   if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      if (_useRegs)
         {
         for (i=0;i<_useRegs->size();i++)
            {
            if (((*_useRegs)[i])->getRealRegister())
               {
               TR::RealRegister * realReg = toRealRegister((*_useRegs)[i]);
               if (realReg && realReg->isHighWordRegister())
                  (*_useRegs)[i] = realReg->getLowWordRegister();
               }
            }
         }
      if (_defRegs)
         {
         for (i=0;i<_defRegs->size();i++)
            {
            if (((*_defRegs)[i])->getRealRegister())
               {
               TR::RealRegister * realReg = toRealRegister((*_defRegs)[i]);
               if (realReg && realReg->isHighWordRegister())
                  (*_defRegs)[i] = realReg->getLowWordRegister();
               }
            }
         }
      }
   }

// Return i'th source reg or NULL
TR::Register *
OMR::Z::Instruction::getSourceRegister(uint32_t i)

   {
   if (_useRegs == NULL) return NULL;
   if (_useRegs->size()<=i)
      {
      return NULL;
      }
   else
      {
      return (*_useRegs)[i];
      }
   }

// Return i'th target reg or NULL
TR::Register *
OMR::Z::Instruction::getTargetRegister(uint32_t i)

   {
   if (_defRegs == NULL) return NULL;
   if (_defRegs->size()<=i)
      {
      return NULL;
      }
   else
      {
      return (*_defRegs)[i];
      }
   }

// Only RR instructions has implementation
// TR::Register *
// OMR::Z::Instruction::getSourceRegister()
//   {
//   return NULL;
//   }

bool
OMR::Z::Instruction::sourceRegUsedInMemoryReference(uint32_t i)
   {
   return _sourceUsedInMemoryReference->isSet(i);
   }

// Local-local allocation interface
//  Used to grab regs at genBin time.
//    Returns true if the desired number of regs are found.
//    Returns fale if not.
bool
OMR::Z::Instruction::getOneLocalLocalAllocFreeReg(TR::RealRegister ** reg)
   {
   int32_t i;
   int32_t cnt   = 1; // skip GPR0
   int32_t first = TR::RealRegister::FirstGPR+1; // skip GPR0
   int32_t last  = TR::RealRegister::LastAssignableGPR;
   *reg = NULL;

   for(i=first;i<=last;i++)
     {
     if ( (_binFreeRegs>>cnt)&0x1 )
        {
        *reg = self()->cg()->machine()->getS390RealRegister(cnt+1);
        return true;
        }
     cnt++;
     }

   return false;
   }

bool
OMR::Z::Instruction::getTwoLocalLocalAllocFreeReg(TR::RealRegister ** reg1, TR::RealRegister ** reg2)
   {
   int32_t i;
   int32_t cnt   = 1; // skip GPR0
   int32_t first = TR::RealRegister::FirstGPR+1; // skip GPR0
   int32_t last  = TR::RealRegister::LastAssignableGPR;
   *reg1 = NULL;
   *reg2 = NULL;
   TR_ASSERT( 0, "OMR::Z::Instruction::getTwoLocalLocalAllocFreeReg -- untested code.");

   for(i=first;i<=last;i++)
     {
     if ( (_binFreeRegs>>cnt)&0x1 )
        {
        if (*reg1 == NULL)
           {
           *reg1 = self()->cg()->machine()->getS390RealRegister(cnt+1);
           }
        else
           {
           *reg2 = self()->cg()->machine()->getS390RealRegister(cnt+1);
           return true;
           }
        }
     cnt++;
     }

   // Failed to find two, so reset.
   *reg1 = NULL;
   *reg2 = NULL;

   return false;
   }

// Populate the local-local reg alloc fields
//  Select the best spill register
TR::RealRegister *
OMR::Z::Instruction::assignBestSpillRegister()
   {
   // Possible improvement here would be to pick a register using the SPILL policy
   return _longDispSpillReg1 = self()->cg()->machine()->findRegNotUsedInInstruction(self()) ;
   }

// Populate the local-local reg alloc fields
//  Select the best spill register
TR::RealRegister *
OMR::Z::Instruction::assignBestSpillRegister2()
   {
   return _longDispSpillReg2 = self()->cg()->machine()->findRegNotUsedInInstruction(self()) ;
   }

// Setup the bit vect of free regs
//  Only free regs that have already been assigned
//  and volatiles (excluding GPR0) are included.
uint32_t
OMR::Z::Instruction::assignFreeRegBitVector()
   {
   return _binFreeRegs = self()->cg()->machine()->constructFreeRegBitVector(self()) ;
   }


bool
OMR::Z::Instruction::isCall()
   {
   if (self()->isLoad() || self()->isStore())
      if (self()->getMemoryReference() && self()->getMemoryReference()->getUnresolvedSnippet()!=NULL)
         return true;

   if (_opcode.isCall() > 0)
      return true;
   else
      return false;
   }



void
OMR::Z::Instruction::addARDependencyCondition(TR::Register * virtAR, TR::Register * assignedGPR)
   {
   bool assignPreCondition = false;
   bool newCondition = false;

   // AR register for stack pointer doesn't need to be set
   for (uint32_t x=1; x <= self()->cg()->getS390Linkage()->getNumStackPointerRegisters(); x++)
      {
      if (REGINDEX((toRealRegister(assignedGPR->getRealRegister()))->getRegisterNumber()) ==
          REGINDEX(self()->cg()->getS390Linkage()->getStackPointerRealRegister(x)->getRegisterNumber()))
         {
         virtAR->decTotalUseCount();
         virtAR->decFutureUseCount();
         return;
         }
      }

   if (self()->getDependencyConditions())
      {
      if (self()->getDependencyConditions()->searchPreConditionRegister(assignedGPR->getAssignedRegister()))
          {
          // add another pre condition to current list
          TR::RegisterDependencyConditions * tmp = new (self()->cg()->trHeapMemory()) TR::RegisterDependencyConditions(self()->getDependencyConditions(), 1, 0, self()->cg());
          self()->resetDependencyConditions(tmp);
          assignPreCondition = true;
          }
       else
          {
          // add another post condition to current list
          TR::RegisterDependencyConditions * tmp = new (self()->cg()->trHeapMemory()) TR::RegisterDependencyConditions(self()->getDependencyConditions(), 0, 1, self()->cg());
          self()->resetDependencyConditions(tmp);
         }
      }
   else
      {
      // add new post condition
      TR::RegisterDependencyConditions * tmp = new (self()->cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, self()->cg());
      self()->resetDependencyConditions(tmp);
      }
   if (assignPreCondition)
      {
       self()->cg()->traceRegisterAssignment("PRE dependancy for instruction %p on virtual reg %s -> %s to be added\n",
      self(),self()->cg()->getDebug()->getName(virtAR), self()->cg()->getDebug()->getName(assignedGPR));
      newCondition = self()->getDependencyConditions()->addPreConditionIfNotAlreadyInserted(virtAR, REGNUM(TR::RealRegister::FirstAR - TR::RealRegister::FirstGPR + toRealRegister(assignedGPR)->getRegisterNumber()));
      }
   else
      {
      self()->cg()->traceRegisterAssignment("POST dependancy for instruction %p on virtual reg %s -> %s to be added\n",
      self(),self()->cg()->getDebug()->getName(virtAR), self()->cg()->getDebug()->getName(assignedGPR));
      newCondition = self()->getDependencyConditions()->addPostConditionIfNotAlreadyInserted(virtAR, REGNUM(TR::RealRegister::FirstAR - TR::RealRegister::FirstGPR + toRealRegister(assignedGPR)->getRegisterNumber()));
      }

   if (newCondition)
      {
      self()->cg()->traceRegisterAssignment("dependancy for instruction %p on virtual reg %s -> %s added\n",
        self(),self()->cg()->getDebug()->getName(virtAR), self()->cg()->getDebug()->getName(assignedGPR));
      }
   else
        {

        self()->cg()->traceRegisterAssignment("duplicate dependancy for instruction %p on virtual reg %s ->  %s not added\n",
        self(),self()->cg()->getDebug()->getName(virtAR), self()->cg()->getDebug()->getName(assignedGPR));
      virtAR->decTotalUseCount();
      virtAR->decFutureUseCount();
      }
   //account for merged EX deps from the 1st RA pass
   if (self()->getOpCodeValue() == TR::InstOpCode::EX || self()->getOpCodeValue() == TR::InstOpCode::EXRL )
      {
      virtAR->decTotalUseCount();
      virtAR->decFutureUseCount();
      }
   self()->cg()->traceRegAssigned(virtAR, assignedGPR);
   }


void OMR::Z::Instruction::setMaskField(uint32_t *instruction, int8_t mask, int8_t nibbleIndex)
  {
  TR_ASSERT(nibbleIndex>=0 && nibbleIndex<=7,
        "OMR::Z::Instruction::setMaskField: bad nibbleIndex\n");
  uint32_t maskInstruction = *instruction&(0xF<<nibbleIndex*4);
  *instruction ^= maskInstruction; // clear out the memory of byte
  *instruction |= (mask << nibbleIndex*4);
  }



TR::Register *
OMR::Z::Instruction::tgtRegArrElem(int32_t i)
   {
   return (self()->targetRegBase() != NULL) ? (self()->targetRegBase())[i] : NULL;
   }

TR::Register *
OMR::Z::Instruction::srcRegArrElem(int32_t i)
   {
   return (self()->sourceRegBase() != NULL) ? (self()->sourceRegBase())[i] : NULL;
   }

void
OMR::Z::Instruction::setThrowsImplicitNullPointerException()
   {
   _flags.set(ThrowsImplicitNullPointerException); self()->setThrowsImplicitException();
   }


int32_t
OMR::Z::Instruction::getMachineOpCode()
   {
   return self()->getOpCodeValue();
   }

bool
OMR::Z::Instruction::is4ByteLoad()
   {
   return self()->getOpCodeValue() == TR::InstOpCode::L;
   }

bool
OMR::Z::Instruction::isAsmGen()
   {
   return self()->getOpCodeValue() == TR::InstOpCode::ASM;
   }

bool
OMR::Z::Instruction::isRet()
   {
   return self()->getOpCodeValue() == TR::InstOpCode::RET;
   }

bool
OMR::Z::Instruction::isTailCall()
   {
   return self()->getOpCodeValue() == TR::InstOpCode::TAILCALL;
   }

int8_t
OMR::Z::Instruction::lastOperandIndex(void)
   {
   return self()->numOperands() - 1;
   }
