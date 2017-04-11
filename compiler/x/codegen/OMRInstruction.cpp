/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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

#include <stddef.h>                              // for NULL
#include <stdint.h>                              // for uint32_t, int32_t, etc
#include "codegen/CodeGenerator.hpp"             // for CodeGenerator, etc
#include "codegen/InstOpCode.hpp"                // for InstOpCode, etc
#include "codegen/Instruction.hpp"               // for Instruction, etc
#include "codegen/Machine.hpp"                   // for Machine
#include "codegen/MemoryReference.hpp"           // for MemoryReference
#include "codegen/RealRegister.hpp"              // for RealRegister, etc
#include "codegen/Register.hpp"                  // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"  // for RegisterDependency, etc
#include "compile/Compilation.hpp"               // for Compilation
#include "env/ObjectModel.hpp"                   // for ObjectModel
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"                      // for TR_ASSERT
#include "ras/Debug.hpp"                         // for TR_DebugBase
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                  // for TR_X86OpCodes, etc
#include "env/CompilerEnv.hpp"

namespace TR { class Node; }

////////////////////////////////////////////////////////////////////////////////
// OMR::X86::Instruction:: member functions
////////////////////////////////////////////////////////////////////////////////

OMR::X86::Instruction::Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, op, node),
      _conditions(0),
      _rexRepeatCount(0)
   {

   }

OMR::X86::Instruction::Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, precedingInstruction, op, node),
      _conditions(0),
      _rexRepeatCount(0)
   {

   }


void
OMR::X86::Instruction::initialize(TR::CodeGenerator *cg, TR::RegisterDependencyConditions *cond, TR_X86OpCodes op, bool flag)
   {
   self()->assumeValidInstruction();
   self()->clobberRegsForRematerialisation();

   if (cond && op != ASSOCREGS)
      {
      cond->useRegisters(self(), cg);

      if (flag && op != FPREGSPILL && cg->enableRegisterAssociations())
         {
         cond->createRegisterAssociationDirective(self(), cg);
         }
      }
   }


void OMR::X86::Instruction::assumeValidInstruction()
   {
   TR_ASSERT(!(TR::Compiler->target.is64Bit() && self()->getOpCode().isIA32Only()), "Cannot use invalid AMD64 instructions");
   }

bool OMR::X86::Instruction::isRegRegMove()
   {
   switch (self()->getOpCodeValue())
      {
      case FLDRegReg:
      case DLDRegReg:
      case MOVAPSRegReg:
      case MOVAPDRegReg:
      case MOVUPSRegReg:
      case MOVUPDRegReg:
      case MOVSSRegReg:
      case MOVSDRegReg:
      case MOV1RegReg:
      case MOV2RegReg:
      case MOV4RegReg:
      case MOV8RegReg:
      case MOVDQURegReg:
         return true;
      default:
         return false;
      }
   }

bool OMR::X86::Instruction::isPatchBarrier()
   {
   Kind kind = self()->getKind();
   return kind == TR::Instruction::IsPatchableCodeAlignment
          || kind == TR::Instruction::IsBoundaryAvoidance
          || kind == TR::Instruction::IsAlignment;
   }

void OMR::X86::Instruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
   {
   if (!self()->getDependencyConditions())
      {
      // Fast path when there are no dependency conditions.
      //
      return;
      }

   if (self()->getOpCodeValue() != ASSOCREGS)
      {
      self()->aboutToAssignRegDeps();

      if ((self()->cg()->getAssignmentDirection() == self()->cg()->Backward))
         {
         self()->getDependencyConditions()->assignPostConditionRegisters(self(), kindsToBeAssigned, self()->cg());
         self()->getDependencyConditions()->assignPreConditionRegisters(self(), kindsToBeAssigned, self()->cg());
         }
      else
         {
         self()->getDependencyConditions()->assignPreConditionRegisters(self()->getPrev(), kindsToBeAssigned, self()->cg());
         self()->getDependencyConditions()->assignPostConditionRegisters(self(), kindsToBeAssigned, self()->cg());
         }
      }
   else if ((self()->getOpCodeValue() == ASSOCREGS) && self()->cg()->enableRegisterAssociations())
      {
      if (kindsToBeAssigned & TR_GPR_Mask)
         {
         TR::Machine *machine = self()->cg()->machine();

         // First traverse the existing associations and remove them
         // so that they don't interfere with the new ones
         //
         for (int i = TR::RealRegister::FirstGPR;
              i <= TR::RealRegister::LastAssignableGPR;
              ++i)
            {
            // Skip non-assignable registers
            //
            if (machine->getX86RealRegister((TR::RealRegister::RegNum)i)->getState() == TR::RealRegister::Locked)
               continue;

            TR::Register *virtReg = machine->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)i);
            if (virtReg)
               {
               virtReg->setAssociation(TR::RealRegister::NoReg);
               }
            }

         // Next loop through and set up the new associations (both on the machine
         // and by associating the virtual registers with their real dependencies)
         //
         TR_X86RegisterDependencyGroup *depGroup = self()->getDependencyConditions()->getPostConditions();
         for (int j = 0; j < self()->getDependencyConditions()->getNumPostConditions(); ++j)
            {
            TR::RegisterDependency  *dep = depGroup->getRegisterDependency(j);
            machine->setVirtualAssociatedWithReal(dep->getRealRegister(), dep->getRegister());
            }

         machine->setGPRWeightsFromAssociations();
         }
      }
   }

bool OMR::X86::Instruction::refsRegister(TR::Register *reg)
   {
   return self()->getDependencyConditions() ? _conditions->refsRegister(reg) : false;
   }

bool OMR::X86::Instruction::defsRegister(TR::Register *reg)
   {
   return self()->getDependencyConditions() ? _conditions->defsRegister(reg) : false;
   }

bool OMR::X86::Instruction::usesRegister(TR::Register *reg)
   {
   return self()->getDependencyConditions() ? _conditions->usesRegister(reg) : false;
   }

bool OMR::X86::Instruction::dependencyRefsRegister(TR::Register *reg)
   {
   return self()->getDependencyConditions() ? _conditions->refsRegister(reg) : false;
   }

void OMR::X86::Instruction::setIsUpperHalfDead(TR::Register *reg, bool value, TR_UpperHalfRefConditions when)
   {
   if (self()->registerRefKindApplies(when))
      {
      reg->setIsUpperHalfDead(value);
      }
   }

bool OMR::X86::Instruction::registerRefKindApplies(TR_UpperHalfRefConditions when)
   {
   switch (when)
      {
      case TR_never:
         return false;
      case TR_always:
         return true;
      case TR_if64bitSource:
         return self()->getOpCode().hasLongSource();
      case TR_ifUses64bitTarget:
         return self()->getOpCode().usesTarget() && self()->getOpCode().hasLongTarget();
      case TR_ifUses64bitSourceOrTarget:
         return self()->registerRefKindApplies(TR_if64bitSource) || self()->registerRefKindApplies(TR_ifUses64bitTarget);
      case TR_ifModifies32or64bitTarget:
         return self()->getOpCode().modifiesTarget() && (self()->getOpCode().hasLongTarget() || self()->getOpCode().hasIntTarget());
      case TR_ifModifies32or64bitSource:
         return self()->getOpCode().modifiesSource() && (self()->getOpCode().hasLongSource() || self()->getOpCode().hasIntSource());
      default:
         TR_ASSERT(0, "Unknown register reference kind");
         return false;
      }
   }

void OMR::X86::Instruction::aboutToAssignDefdRegister(TR::Register *reg, TR_UpperHalfRefConditions defsUpperHalf)
   {
   // Only setIsUpperHalfDead() when you're sure this instruction doesn't need the upper half

   if (TR::Compiler->target.is32Bit() || reg->getKind() != TR_GPR || self()->cg()->getAssignmentDirection() != self()->cg()->Backward)
      return;

   if (self()->cg()->internalControlFlowNestingDepth() == 0)
      self()->setIsUpperHalfDead(reg, true, defsUpperHalf);
   }

void OMR::X86::Instruction::aboutToAssignUsedRegister(TR::Register *reg, TR_UpperHalfRefConditions usesUpperHalf)
   {
   // Must setIsUpperHalfDead(false) there's any chance this instruction needs the upper half

   if (TR::Compiler->target.is32Bit() || reg->getKind() != TR_GPR || self()->cg()->getAssignmentDirection() != self()->cg()->Backward)
      return;

   self()->setIsUpperHalfDead(reg, false, usesUpperHalf);
   }

void OMR::X86::Instruction::aboutToAssignMemRef(TR::MemoryReference *memref)
   {
   if (TR::Compiler->target.is64Bit())
      {
      if (memref->getBaseRegister())
         self()->aboutToAssignUsedRegister(memref->getBaseRegister(), TR_always);

      if (memref->getIndexRegister())
         self()->aboutToAssignUsedRegister(memref->getIndexRegister(), TR_always);
      }
   }

void OMR::X86::Instruction::aboutToAssignRegDeps(TR_UpperHalfRefConditions usesUpperHalf, TR_UpperHalfRefConditions defsUpperHalf)
   {
   if (!self()->getDependencyConditions())
      return;

   for (uint32_t i = 0; i < self()->getDependencyConditions()->getNumPreConditions(); i++)
      {
      TR::RegisterDependency *dep = self()->getDependencyConditions()->getPreConditions()->getRegisterDependency(i);
      self()->aboutToAssignRegister(dep->getRegister(), usesUpperHalf, defsUpperHalf);
      }
   for (uint32_t i = 0; i < self()->getDependencyConditions()->getNumPostConditions(); i++)
      {
      TR::RegisterDependency *dep = self()->getDependencyConditions()->getPostConditions()->getRegisterDependency(i);
      self()->aboutToAssignRegister(dep->getRegister(), usesUpperHalf, defsUpperHalf);
      }
   }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)

uint32_t OMR::X86::Instruction::totalReferencedGPRegisters(TR::CodeGenerator * cg)
   {
   if (self()->getDependencyConditions())
      {
      return self()->getNumOperandReferencedGPRegisters() + self()->getDependencyConditions()->numReferencedGPRegisters(cg);
      }

   return self()->getNumOperandReferencedGPRegisters();
   }

uint32_t OMR::X86::Instruction::totalReferencedFPRegisters(TR::CodeGenerator * cg)
   {
   if (self()->getDependencyConditions())
      {
      return self()->getNumOperandReferencedFPRegisters() + self()->getDependencyConditions()->numReferencedFPRegisters(cg);
      }

   return self()->getNumOperandReferencedFPRegisters();
   }
#endif

void OMR::X86::Instruction::adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg)
   {
   TR::Compilation* comp = cg->comp();
   if (state->_register == TR::RealRegister::esp)
      {
      if (self()->getOpCode().isPushOp())
         state->_displacement += TR::Compiler->om.sizeofReferenceAddress();
      else if (self()->getOpCode().isPopOp())
         state->_displacement -= TR::Compiler->om.sizeofReferenceAddress();
      else if (self()->getOpCodeValue() == RET || self()->getOpCodeValue() == RETImm2 || self()->getOpCodeValue() == ReturnMarker)
         *state = cg->vfpResetInstruction()->getSavedState();
      }
   }

void OMR::X86::Instruction::adjustVFPStateForCall(TR_VFPState *state, int32_t vfpAdjustmentForCall, TR::CodeGenerator *cg)
   {
   if (state->_register == TR::RealRegister::esp && self()->getOpCode().isCallOp())
      {
      state->_displacement += vfpAdjustmentForCall;
      }
   else
      {
      OMR::X86::Instruction::adjustVFPState(state, cg);
      }
   }

void OMR::X86::Instruction::clobberRegsForRematerialisation()
   {
   // We assume most instructions modify all registers that appear in their
   // postconditions, with a few exceptions.
   //
   if (  self()->cg()->enableRematerialisation()
      && self()->getDependencyConditions()
      && (self()->getOpCodeValue() != ASSOCREGS)  // reg associations aren't really instructions, so they don't modify anything
      && (self()->getOpCodeValue() != LABEL)      // labels must already be handled properly for a variety of reasons
      && (!self()->getOpCode().isShiftOp())
      && (!self()->getOpCode().isRotateOp())      // shifts and rotates often have a postcondition on ecx but don't clobber it
      ){
      // Check the live discardable register list to see if this is the first
      // instruction that kills the rematerialisable range of a register.
      //
      TR::ClobberingInstruction *clob = NULL;
      TR_X86RegisterDependencyGroup *post = self()->getDependencyConditions()->getPostConditions();
      for (uint32_t i = 0; i < self()->getDependencyConditions()->getNumPostConditions(); i++)
         {
         TR::Register *reg = post->getRegisterDependency(i)->getRegister();
         if (reg->isDiscardable())
            {
            if (!clob)
               {
               clob = new (self()->cg()->trHeapMemory()) TR::ClobberingInstruction(self(), self()->cg()->trMemory());
               self()->cg()->addClobberingInstruction(clob);
               }
            clob->addClobberedRegister(reg);
            self()->cg()->removeLiveDiscardableRegister(reg);
            self()->cg()->clobberLiveDependentDiscardableRegisters(clob, reg);

            if (debug("dumpRemat"))
               diagnostic("---> Clobbering %s discardable postcondition register %s at instruction %s\n",
                  self()->cg()->getDebug()->toString(reg->getRematerializationInfo()),
                  self()->cg()->getDebug()->getName(reg),
                  self()->cg()->getDebug()->getName(self()));

            }
         }
      }
   }

#if defined(TR_TARGET_64BIT)
uint8_t OMR::X86::Instruction::operandSizeRexBits() // Rex bits indicating 32/64-bit operand sizes
   {
   if (self()->getOpCode().info().rex_w)
      return TR::RealRegister::REX | TR::RealRegister::REX_W;
   else
      return 0;
   }

uint8_t
OMR::X86::Instruction::rexBits()
   {
   return self()->operandSizeRexBits();
   }
#endif

bool
OMR::X86::Instruction::isLabel()
   {
   return self()->getOpCodeValue() == LABEL;
   }

void
OMR::X86::Instruction::aboutToAssignRegister(TR::Register *reg, TR_UpperHalfRefConditions usesUpperHalf, TR_UpperHalfRefConditions defsUpperHalf)
   {
   // It's important to call these in this order
   self()->aboutToAssignDefdRegister(reg, defsUpperHalf);
   self()->aboutToAssignUsedRegister(reg, usesUpperHalf);
   }

uint8_t *
OMR::X86::Instruction::generateRepeatedRexPrefix(uint8_t *cursor)
   {
   uint8_t rex = self()->rexBits();
   uint8_t numBytes = self()->rexRepeatCount();
   if (numBytes)
      {
      // If we're forced to add rex prefixes we didn't otherwise need,
      // just use the benign 0x40
      //
      if (!rex)
         rex = 0x40;
      }

   for (uint8_t i = 0; i < numBytes; ++i)
      *cursor++ = rex;

   return cursor;
   }

uint8_t
OMR::X86::Instruction::rexRepeatCount()
   {
   return _rexRepeatCount;
   }

/* -----------------------------------------------------------------------------
 * The following code is here only temporarily during the transition to OMR.
 * -----------------------------------------------------------------------------
 */

void TR_X86OpCode::trackUpperBitsOnReg(TR::Register *reg, TR::CodeGenerator *cg)
   {
   if (TR::Compiler->target.is64Bit())
      {
      if (clearsUpperBits())
         {
         reg->setUpperBitsAreZero(true);
         }
      else if (setsUpperBits())
         {
         reg->setUpperBitsAreZero(false);
         }
      }
   }
