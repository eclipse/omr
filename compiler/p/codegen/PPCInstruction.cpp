/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "p/codegen/PPCInstruction.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "p/codegen/PPCOutOfLineCodeSection.hpp"

namespace TR { class SymbolReference; }


// TR::LabelInstruction function
void TR::PPCLabelInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Compilation *comp = cg()->comp();
   if( TR::Instruction::getDependencyConditions() )
      {
      TR::Instruction::getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());
      TR::Instruction::getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
      }

   TR::Machine *machine = cg()->machine();
   if (isLabel() && getLabelSymbol()->isEndOfColdInstructionStream())
      {
      // A label instruction coming back from OOL.
      //
      // This label is the end of the hot instruction stream (i.e., the fallthru path).
      //
      if (comp->getOption(TR_TraceRA))
         traceMsg (comp,"\nOOL: 1. Taking register state snap shot\n");
      cg()->setIsOutOfLineHotPath(true);
      machine->takeRegisterStateSnapShot();

      // Prevent spilled registers from reclaiming their backing store if they
      // become unspilled.  This will ensure a spilled register will receive the
      // same backing store if it is spilled on either path of the control flow.
      //
      cg()->lockFreeSpillList();
      }

   if (isBranchOp() && !usesCountRegister() && getLabelSymbol()->isEndOfColdInstructionStream())
      {
      // This is the branch at the end of the cold instruction stream to the end of
      // the hot instruction stream (or fallthru path).
      //
      // Start RA for OOL cold path, restore register state from snap shot
      //
      if (comp->getOption(TR_TraceRA))
         traceMsg (comp, "\nOOL: 1. Restoring Register state from snap shot\n");
      cg()->setIsOutOfLineHotPath(false);
      machine->restoreRegisterStateFromSnapShot();
      }

   if (((isBranchOp() && !usesCountRegister()) || isVirtualGuardNOPInstruction()) && getLabelSymbol()->isStartOfColdInstructionStream())
      {
      // This may be an unconditional branch to the OOL code.  This can occur when the
      // array is provably discontiguous but the BNDCHKwithSpineCHK node couldn't
      // be folded away.
      //

      // Switch to the outlined instruction stream and assign registers.
      //
      TR_PPCOutOfLineCodeSection *oi = cg()->findOutLinedInstructionsFromLabel(getLabelSymbol());
      TR_ASSERT(oi, "Could not find PPCOutOfLineCodeSection stream from label.  instr=%p, label=%p\n", this, getLabelSymbol());

      if (!oi->hasBeenRegisterAssigned())
         oi->assignRegisters(kindToBeAssigned);

      if (cg()->getDebug())
         cg()->traceRegisterAssignment("OOL: Finished register assignment in OOL section\n");

      // Unlock the free spill list.
      //
      cg()->unlockFreeSpillList();

      // Disassociate backing storage that was previously reserved for a spilled virtual if
      // virtual is no longer spilled. This occurs because the the free spill list was
      // locked.
      //
      machine->disassociateUnspilledBackingStorage();
      }
   }

// TR::PPCDepInstruction:: member functions

void TR::PPCDepInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if( getOpCodeValue() != TR::InstOpCode::assocreg )
      {
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
      }
   else
      {
      // Restore the register association using the ASSOCREGS instruction info
      int i;
      TR::RegisterDependencyConditions *assocreg = getDependencyConditions();
      TR::RegisterDependency *assoc;
      for( i=0 ; i < assocreg->getNumPostConditions() ; i++ )
         {
         assoc = assocreg->getPostConditions()->getRegisterDependency(i);
         assoc->getRegister()->setAssociation( assoc->getRealRegister() );
         }
      }
   }

bool TR::PPCDepInstruction::refsRegister(TR::Register *reg)
   {
   return _conditions->refsRegister(reg);
   }

bool TR::PPCDepInstruction::defsRegister(TR::Register *reg)
   {
   return _conditions->defsRegister(reg);
   }

bool TR::PPCDepInstruction::defsRealRegister(TR::Register *reg)
   {
   return _conditions->defsRealRegister(reg);
   }

bool TR::PPCDepInstruction::usesRegister(TR::Register *reg)
   {
   return _conditions->usesRegister(reg);
   }

// TR::PPCDepLabelInstruction:: member functions

void TR::PPCDepLabelInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   TR::PPCLabelInstruction::assignRegisters(kindToBeAssigned);

   getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

bool TR::PPCDepLabelInstruction::refsRegister(TR::Register *reg)
   {
   return _conditions->refsRegister(reg);
   }

bool TR::PPCDepLabelInstruction::defsRegister(TR::Register *reg)
   {
   return _conditions->defsRegister(reg);
   }

bool TR::PPCDepLabelInstruction::defsRealRegister(TR::Register *reg)
   {
   return _conditions->defsRealRegister(reg);
   }

bool TR::PPCDepLabelInstruction::usesRegister(TR::Register *reg)
   {
   return _conditions->usesRegister(reg);
   }

// TR::PPCConditionalBranchInstruction: member functions

TR::PPCConditionalBranchInstruction *
TR::PPCConditionalBranchInstruction::getPPCConditionalBranchInstruction()
   {
   return this;
   }

void TR::PPCConditionalBranchInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register           *virtualRegister = getConditionRegister();
   TR::RealRegister       *assignedRegister = virtualRegister->getAssignedRealRegister();
   TR::Machine *machine = cg()->machine();

   if (assignedRegister == NULL)
      {
      assignedRegister = machine->assignOneRegister(this, virtualRegister, false);
      }

   machine->decFutureUseCountAndUnlatch(virtualRegister);

   setConditionRegister(assignedRegister);

   if (getLabelSymbol()->isStartOfColdInstructionStream())
      {
      // Switch to the outlined instruction stream and assign registers.
      //
      TR_PPCOutOfLineCodeSection *oi = cg()->findOutLinedInstructionsFromLabel(getLabelSymbol());
      TR_ASSERT(oi, "Could not find PPCOutOfLineCodeSection stream from label.  instr=%p, label=%p\n", this, getLabelSymbol());
      if (!oi->hasBeenRegisterAssigned())
         oi->assignRegisters(kindToBeAssigned);
      if (cg()->getDebug())
            cg()->traceRegisterAssignment("OOL: Finished register assignment in OOL section\n");

      // Unlock the free spill list.
      //
      cg()->unlockFreeSpillList();

      // Disassociate backing storage that was previously reserved for a spilled virtual if
      // virtual is no longer spilled. This occurs because the the free spill list was
      // locked.
      //
      machine->disassociateUnspilledBackingStorage();
      }
   }

bool TR::PPCConditionalBranchInstruction::refsRegister(TR::Register *reg)
   {
   if (getConditionRegister() == reg)
      return(true);
   return(false);
   }

bool TR::PPCConditionalBranchInstruction::defsRegister(TR::Register *reg)
   {
   return(false);
   }

bool TR::PPCConditionalBranchInstruction::defsRealRegister(TR::Register *reg)
   {
   return(false);
   }

bool TR::PPCConditionalBranchInstruction::usesRegister(TR::Register *reg)
   {
   if (getConditionRegister() == reg)
      return(true);
   return(false);
   }

// TR::PPCDepConditionalBranchInstruction: member functions

void TR::PPCDepConditionalBranchInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register     *virtualRegister = getConditionRegister();

   virtualRegister->block();

   getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   virtualRegister->unblock();

   TR::PPCConditionalBranchInstruction::assignRegisters(kindToBeAssigned);

   virtualRegister->block();

   getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   virtualRegister->unblock();
   }

bool TR::PPCDepConditionalBranchInstruction::refsRegister(TR::Register *reg)
   {
   if (getConditionRegister()==reg || _conditions->refsRegister(reg))
      return(true);
   return(false);
   }

bool TR::PPCDepConditionalBranchInstruction::defsRegister(TR::Register *reg)
   {
   return _conditions->defsRegister(reg);
   }

bool TR::PPCDepConditionalBranchInstruction::defsRealRegister(TR::Register *reg)
   {
   return _conditions->defsRealRegister(reg);
   }

bool TR::PPCDepConditionalBranchInstruction::usesRegister(TR::Register *reg)
   {
   if (getConditionRegister()==reg || _conditions->usesRegister(reg))
      return(true);
   return(false);
   }

// TR::PPCImmInstruction:: member functions


// The following safe virtual downcast method is only used in an assertion
// check within "toPPCImmInstruction"
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
TR::PPCImmInstruction *TR::PPCImmInstruction::getPPCImmInstruction()
   {
   return this;
   }
#endif

// TR::PPCSrc1Instruction:: member functions

bool TR::PPCSrc1Instruction::refsRegister(TR::Register *reg)
   {
   if (reg == getSource1Register())
      {
      return true;
      }
   return false;
   }

void TR::PPCSrc1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register           *virtualRegister = getSource1Register();
   TR::RealRegister       *assignedRegister = virtualRegister->getAssignedRealRegister();
   TR::Machine *machine = cg()->machine();

   if (assignedRegister == NULL)
      {
      assignedRegister = machine->assignOneRegister(this, virtualRegister, false);
      }

   machine->decFutureUseCountAndUnlatch(virtualRegister);

   setSource1Register(assignedRegister);
   }


bool TR::PPCSrc1Instruction::defsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::PPCSrc1Instruction::defsRealRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::PPCSrc1Instruction::usesRegister(TR::Register *reg)
   {
   if (reg == getSource1Register())
      {
      return true;
      }
   return false;
   }

// TR::PPCTrg1Instruction:: member functions

bool TR::PPCTrg1Instruction::refsRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister() || TR::Instruction::refsRegister(reg) )
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Instruction::defsRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister() || TR::Instruction::defsRegister(reg) )
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Instruction::defsRealRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister()->getAssignedRegister() || TR::Instruction::defsRealRegister(reg) )
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Instruction::usesRegister(TR::Register *reg)
   {
   return TR::Instruction::usesRegister(reg);
   }

void TR::PPCTrg1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   assignRegisters(kindToBeAssigned, false);
   }

void TR::PPCTrg1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned, bool excludeGPR0)
   {
   TR::Register           *virtualRegister = getTargetRegister();
   TR::RealRegister       *assignedRegister = virtualRegister->getAssignedRealRegister();
   TR::Machine *machine = cg()->machine();
   TR_RegisterKinds       kindOfRegister = virtualRegister->getKind();

   TR::Instruction::assignRegisters( kindToBeAssigned );

   if (excludeGPR0 && (assignedRegister != NULL) &&
       (toRealRegister(assignedRegister) == machine->getRealRegister(TR::RealRegister::gr0)))
      {
      TR::RealRegister    *alternativeRegister;

      if ((alternativeRegister = machine->findBestFreeRegister(this, kindOfRegister, excludeGPR0, false, virtualRegister)) == NULL)
         {
         cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
         alternativeRegister = machine->freeBestRegister(this, virtualRegister, NULL, excludeGPR0);
         }
      machine->coerceRegisterAssignment(this, virtualRegister, toRealRegister(alternativeRegister)->getRegisterNumber());
      assignedRegister = alternativeRegister;
      }
   else
      {
      if (assignedRegister == NULL)
         {
         assignedRegister = machine->assignOneRegister(this, virtualRegister, excludeGPR0);
         }
      }

   machine->decFutureUseCountAndUnlatch(virtualRegister);

   setTargetRegister(assignedRegister);
   }

// TR::PPCTrg1Src1Instruction:: member functions

TR::PPCTrg1Src1Instruction::PPCTrg1Src1Instruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::Register *treg,
      TR::Register *sreg,
      TR::CodeGenerator *cg)
   : TR::PPCTrg1Instruction(op, n, treg, cg),
      _source1Register(sreg)
   {
   useRegister(sreg);
   if (op == TR::InstOpCode::addi || op == TR::InstOpCode::addis || op == TR::InstOpCode::addi2)
      {
      cg->addRealRegisterInterference(sreg, TR::RealRegister::gr0);
      }
   }

TR::PPCTrg1Src1Instruction::PPCTrg1Src1Instruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::Register *treg,
      TR::Register *sreg,
      TR::Instruction *precedingInstruction,
      TR::CodeGenerator *cg)
   : TR::PPCTrg1Instruction(op, n, treg, precedingInstruction, cg),
      _source1Register(sreg)
   {
   useRegister(sreg);
   if (op == TR::InstOpCode::addi || op == TR::InstOpCode::addis || op == TR::InstOpCode::addi2)
      {
      cg->addRealRegisterInterference(sreg, TR::RealRegister::gr0);
      }
   }

bool TR::PPCTrg1Src1Instruction::refsRegister(TR::Register *reg)
   {
   if (reg == getSource1Register() || TR::PPCTrg1Instruction::refsRegister(reg))
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Src1Instruction::defsRegister(TR::Register *reg)
   {
   return TR::PPCTrg1Instruction::defsRegister(reg);
   }

bool TR::PPCTrg1Src1Instruction::defsRealRegister(TR::Register *reg)
   {
   return TR::PPCTrg1Instruction::defsRealRegister(reg);
   }

bool TR::PPCTrg1Src1Instruction::usesRegister(TR::Register *reg)
   {
   if (reg == getSource1Register() || TR::PPCTrg1Instruction::usesRegister(reg) )
      {
      return true;
      }
   return false;
   }

void TR::PPCTrg1Src1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register      *sourceVirtual = getSource1Register();
   TR::Register      *targetVirtual = getTargetRegister();
   TR::Machine *machine = cg()->machine();
   TR::RealRegister  *assignedRegister;
   TR_RegisterKinds  kindOfRegister = sourceVirtual->getKind();
   bool              excludeGPR0 = false;

   if (getOpCodeValue()==TR::InstOpCode::addi || getOpCodeValue()==TR::InstOpCode::addis || getOpCodeValue()==TR::InstOpCode::addi2)
      excludeGPR0 = true;

   sourceVirtual->block();

   if ((sourceVirtual == targetVirtual) && excludeGPR0)
      TR::PPCTrg1Instruction::assignRegisters(kindToBeAssigned, true);
   else
      TR::PPCTrg1Instruction::assignRegisters(kindToBeAssigned, false);

   sourceVirtual->unblock();

   targetVirtual->block();
   assignedRegister = sourceVirtual->getAssignedRealRegister();
   if (excludeGPR0 && (assignedRegister != NULL) &&
       (toRealRegister(assignedRegister) == machine->getRealRegister(TR::RealRegister::gr0)))
      {
      TR::RealRegister    *alternativeRegister;

      if ((alternativeRegister = machine->findBestFreeRegister(this, kindOfRegister, excludeGPR0, false, sourceVirtual)) == NULL)
         {
         cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
         alternativeRegister = machine->freeBestRegister(this, sourceVirtual, NULL, excludeGPR0);
         }
      machine->coerceRegisterAssignment(this, sourceVirtual, toRealRegister(alternativeRegister)->getRegisterNumber());
      assignedRegister = alternativeRegister;
      }
   else
      {
      if (assignedRegister == NULL)
         {
         assignedRegister = machine->assignOneRegister(this, sourceVirtual, excludeGPR0);
         }
      }
   targetVirtual->unblock();

   machine->decFutureUseCountAndUnlatch(sourceVirtual);

   setSource1Register(assignedRegister);
   }

// TR::PPCTrg1Src1ImmInstruction:: member functions

// TR::PPCSrc2Instruction:: member functions

bool TR::PPCSrc2Instruction::refsRegister(TR::Register *reg)
   {
   if (reg == getSource1Register() || reg == getSource2Register())
      {
      return true;
      }
   return false;
   }

bool TR::PPCSrc2Instruction::defsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::PPCSrc2Instruction::defsRealRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::PPCSrc2Instruction::usesRegister(TR::Register *reg)
   {
   if ( reg == getSource1Register()  ||
        reg == getSource2Register())
      {
      return true;
      }
   return false;
   }

void TR::PPCSrc2Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register           *virtualRegister1 = getSource2Register();
   TR::Register           *virtualRegister2 = getSource1Register();
   TR::RealRegister       *assignedRegister;
   TR::Machine *machine = cg()->machine();

   virtualRegister2->block();
   assignedRegister = virtualRegister1->getAssignedRealRegister();
   if (assignedRegister == NULL)
      {
      assignedRegister = machine->assignOneRegister(this, virtualRegister1, false);
      }
   setSource2Register(assignedRegister);

   virtualRegister2->unblock();
   virtualRegister1->block();

   assignedRegister = virtualRegister2->getAssignedRealRegister();
   if (assignedRegister == NULL)
      {
      assignedRegister = machine->assignOneRegister(this, virtualRegister2, false);
      }
   setSource1Register(assignedRegister);

   virtualRegister1->unblock();

   machine->decFutureUseCountAndUnlatch(virtualRegister1);
   machine->decFutureUseCountAndUnlatch(virtualRegister2);
   }

// TR::PPCTrg1Src2Instruction:: member functions

bool TR::PPCTrg1Src2Instruction::refsRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister() || reg == getSource1Register() || reg == getSource2Register())
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Src2Instruction::defsRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister())
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Src2Instruction::defsRealRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister()->getAssignedRegister())
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Src2Instruction::usesRegister(TR::Register *reg)
   {
   if ( reg == getSource1Register()  ||
        reg == getSource2Register())
      {
      return true;
      }
   return false;
   }

void TR::PPCTrg1Src2Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register        *source2Virtual = getSource2Register();
   TR::Machine *machine = cg()->machine();
   TR::RealRegister    *assignedRegister;

   source2Virtual->block();
   TR::PPCTrg1Src1Instruction::assignRegisters(kindToBeAssigned);
   source2Virtual->unblock();

   getSource1Register()->block();
   getTargetRegister()->block();

   assignedRegister = source2Virtual->getAssignedRealRegister();
   if (assignedRegister == NULL)
      {
      assignedRegister = machine->assignOneRegister(this, source2Virtual, false);
      }

   getTargetRegister()->unblock();
   getSource1Register()->unblock();

   machine->decFutureUseCountAndUnlatch(source2Virtual);

   setSource2Register(assignedRegister);
   }


// TR::PPCTrg1Src3Instruction:: member functions

bool TR::PPCTrg1Src3Instruction::refsRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister() || reg == getSource1Register() || reg == getSource2Register() ||
       reg == getSource3Register())
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1Src3Instruction::usesRegister(TR::Register *reg)
   {
   if ( reg == getSource1Register()  ||
        reg == getSource2Register()  ||
        reg == getSource3Register())
      {
      return true;
      }
   return false;
   }

void TR::PPCTrg1Src3Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register        *source3Virtual = getSource3Register();
   TR::Machine *machine = cg()->machine();
   TR::RealRegister    *assignedRegister;

   source3Virtual->block();
   TR::PPCTrg1Src2Instruction::assignRegisters(kindToBeAssigned);
   source3Virtual->unblock();

   getSource1Register()->block();
   getSource2Register()->block();
   getTargetRegister()->block();

   assignedRegister = source3Virtual->getAssignedRealRegister();
   if (assignedRegister == NULL)
      {
      assignedRegister = machine->assignOneRegister(this, source3Virtual, false);
      }

   getTargetRegister()->unblock();
   getSource2Register()->unblock();
   getSource1Register()->unblock();

   machine->decFutureUseCountAndUnlatch(source3Virtual);

   setSource3Register(assignedRegister);
   }

TR::PPCMemInstruction::PPCMemInstruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::MemoryReference *mf,
      TR::CodeGenerator *cg)
   : TR::Instruction(op, n, cg),
      _memoryReference(mf)
   {
   if (getOpCode().offsetRequiresWordAlignment())
      {
      mf->setOffsetRequiresWordAlignment(n, cg, getPrev());
      }

   mf->bookKeepingRegisterUses(this, cg);
   TR::Register *baseReg = mf->getBaseRegister();
   if (baseReg != NULL)
      {
      cg->addRealRegisterInterference(baseReg, TR::RealRegister::gr0);
      }
   }

TR::PPCMemInstruction::PPCMemInstruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::MemoryReference *mf,
      TR::Instruction *precedingInstruction,
      TR::CodeGenerator *cg)
   : TR::Instruction(op, n, precedingInstruction, cg),
      _memoryReference(mf)
   {
   if (getOpCode().offsetRequiresWordAlignment())
      {
      mf->setOffsetRequiresWordAlignment(n, cg, getPrev());
      }
   mf->bookKeepingRegisterUses(this, cg);
   TR::Register *baseReg = mf->getBaseRegister();
   if (baseReg != NULL)
      {
      cg->addRealRegisterInterference(baseReg, TR::RealRegister::gr0);
      }
   }

bool TR::PPCMemInstruction::refsRegister(TR::Register *reg)
   {
   return getMemoryReference()->refsRegister(reg);
   }

bool TR::PPCMemInstruction::usesRegister(TR::Register *reg)
   {
   return getMemoryReference()->refsRegister(reg);
   }

void TR::PPCMemInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   getMemoryReference()->assignRegisters(this, cg());
   }

// TR::PPCMemSrc1Instruction:: member functions

TR::PPCMemSrc1Instruction::PPCMemSrc1Instruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::MemoryReference *mf,
      TR::Register *sreg,
      TR::CodeGenerator *cg)
   : TR::PPCMemInstruction(op, n, mf, cg),
      _sourceRegister(sreg)
   {
   useRegister(sreg);
   if (mf->isUsingStaticTOC() && mf->getUnresolvedSnippet()==NULL)
      {
      cg->addRealRegisterInterference(sreg, TR::RealRegister::gr0);
      }
   }

TR::PPCMemSrc1Instruction::PPCMemSrc1Instruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::MemoryReference *mf,
      TR::Register *sreg,
      TR::Instruction *precedingInstruction,
      TR::CodeGenerator *cg)
   : TR::PPCMemInstruction(op, n, mf, precedingInstruction, cg),
      _sourceRegister(sreg)
   {
   useRegister(sreg);
   if (mf->isUsingStaticTOC() && mf->getUnresolvedSnippet()==NULL)
      {
      cg->addRealRegisterInterference(sreg, TR::RealRegister::gr0);
      }
   }

TR::Register *TR::PPCMemSrc1Instruction::getMemoryDataRegister()
   {
   return getSourceRegister();
   }

bool TR::PPCMemSrc1Instruction::refsRegister(TR::Register *reg)
   {
   if (getMemoryReference()->refsRegister(reg) == true ||
       reg == getSourceRegister())
      {
      return true;
      }
   return false;
   }

bool TR::PPCMemSrc1Instruction::defsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::PPCMemSrc1Instruction::defsRealRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::PPCMemSrc1Instruction::usesRegister(TR::Register *reg)
   {
   if (getMemoryReference()->refsRegister(reg) == true ||
       reg == getSourceRegister())
      {
      return true;
      }
   return false;
   }

void TR::PPCMemSrc1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::MemoryReference *mref = getMemoryReference();
   TR::Register           *sourceVirtual = getSourceRegister();
   TR::Register           *mBaseVirtual = mref->getModBase();
   TR::Machine *machine = cg()->machine();
   TR::RealRegister       *assignedRegister, *mBaseReal;
   bool                   excludeGPR0 = mref->isUsingStaticTOC() &&
                                        (mref->getUnresolvedSnippet() == NULL) &&
                                        (mref->getTOCOffset()>UPPER_IMMED || mref->getTOCOffset()<LOWER_IMMED);


   sourceVirtual->block();
   TR::PPCMemInstruction::assignRegisters(kindToBeAssigned);
   sourceVirtual->unblock();
   mBaseReal = (mBaseVirtual==NULL)?NULL:toRealRegister(mref->getModBase());

   mref->blockRegisters();
   assignedRegister = sourceVirtual->getAssignedRealRegister();
   if (excludeGPR0 && (assignedRegister != NULL) &&
       (toRealRegister(assignedRegister) == machine->getRealRegister(TR::RealRegister::gr0)))
      {
      TR::RealRegister    *alternativeRegister;

      if ((alternativeRegister = machine->findBestFreeRegister(this, sourceVirtual->getKind(), excludeGPR0, false, sourceVirtual)) == NULL)
         {
         cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
         alternativeRegister = machine->freeBestRegister(this, sourceVirtual, NULL, excludeGPR0);
         }
      machine->coerceRegisterAssignment(this, sourceVirtual, toRealRegister(alternativeRegister)->getRegisterNumber());
      assignedRegister = alternativeRegister;
      }
   else
      {
      if (assignedRegister == NULL)
         {
         assignedRegister = machine->assignOneRegister(this, sourceVirtual, excludeGPR0);
         }
      }
   mref->unblockRegisters();

   machine->decFutureUseCountAndUnlatch(sourceVirtual);

   setSourceRegister(assignedRegister);

   if (mBaseVirtual!=NULL)
      {
      machine->decFutureUseCountAndUnlatch(mBaseVirtual);
      }
   }

TR::Register *TR::PPCMemSrc1Instruction::getSourceRegisterForStmw(uint32_t i)
   {
   // Index 0:   maps to base register of stmw
   // Index 1-N: map to the N stored registers of stmw
   // Index >N:  NULL
   if (i==0) return getMemoryReference()->getBaseRegister();

   TR::RealRegister::RegNum rrFirstSaved, rrWant;

   rrFirstSaved = toRealRegister(getSourceRegister())->getRegisterNumber();
   rrWant = (TR::RealRegister::RegNum)(rrFirstSaved+(i-1));
   if (rrWant <= TR::RealRegister::LastGPR)
       {
      TR::Machine *machine = cg()->machine();
       return machine->getRealRegister(rrWant);
       }
   return NULL;
   }

TR::PPCTrg1MemInstruction::PPCTrg1MemInstruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::Register *treg,
      TR::MemoryReference *mf,
      TR::CodeGenerator *cg,
      int32_t hint)
   : TR::PPCTrg1Instruction(op, n, treg, cg),
      _memoryReference(mf)
   {
   // If we set the hints, the instructions have to be lwarx or ldarx
   // Also control that the hints are set correctly for the type of instruction
   //
   if (encodeMutexHint())
      {
      _hint = hint;
      }
   else
      {
      _hint = PPCOpProp_NoHint;
      }

   TR_ASSERT(_hint == PPCOpProp_NoHint ||
          ( (op == TR::InstOpCode::lwarx || op == TR::InstOpCode::ldarx) &&
          (_hint == PPCOpProp_LoadReserveAtomicUpdate) || (_hint == PPCOpProp_LoadReserveExclusiveAccess)),"Hint set for the wrong instruction");

   if (getOpCode().offsetRequiresWordAlignment())
      {
      mf->setOffsetRequiresWordAlignment(n, cg, getPrev());
      }

   mf->bookKeepingRegisterUses(this, cg);
   TR::Register *baseReg = mf->getBaseRegister();
   if (baseReg != NULL)
      {
      cg->addRealRegisterInterference(baseReg, TR::RealRegister::gr0);
      }

   if (mf->isUsingStaticTOC() && mf->getUnresolvedSnippet()==NULL)
      {
      cg->addRealRegisterInterference(treg, TR::RealRegister::gr0);
      }
   }

TR::PPCTrg1MemInstruction::PPCTrg1MemInstruction(
      TR::InstOpCode::Mnemonic op,
      TR::Node *n,
      TR::Register *treg,
      TR::MemoryReference *mf,
      TR::Instruction *precedingInstruction,
      TR::CodeGenerator *cg,
      int32_t hint)
   : TR::PPCTrg1Instruction(op, n, treg, precedingInstruction, cg),
      _memoryReference(mf)
   {
   // If we set the hints, the instructions have to be lwarx or ldarx
   // Also control that the hints are set correctly for the type of instruction
   if (encodeMutexHint())
      {
      _hint = hint;
      }
   else
      {
      _hint = PPCOpProp_NoHint;
      }

   TR_ASSERT(_hint == PPCOpProp_NoHint ||
          ( (op == TR::InstOpCode::lwarx || op == TR::InstOpCode::ldarx) &&
          (_hint == PPCOpProp_LoadReserveAtomicUpdate) || (_hint == PPCOpProp_LoadReserveExclusiveAccess)),"Hint set for the wrong instruction");

   if (getOpCode().offsetRequiresWordAlignment())
      {
      mf->setOffsetRequiresWordAlignment(n, cg, getPrev());
      }
   mf->bookKeepingRegisterUses(this, cg);
   TR::Register *baseReg = mf->getBaseRegister();
   if (baseReg != NULL)
      {
      cg->addRealRegisterInterference(baseReg, TR::RealRegister::gr0);
      }

   if (mf->isUsingStaticTOC() && mf->getUnresolvedSnippet()==NULL)
      {
      cg->addRealRegisterInterference(treg, TR::RealRegister::gr0);
      }
   }


bool TR::PPCTrg1MemInstruction::encodeMutexHint()
   {
   return cg()->comp()->target().cpu.id() >= TR_PPCp6 && (getOpCodeValue() == TR::InstOpCode::lwarx || getOpCodeValue() == TR::InstOpCode::ldarx);
   }


TR::Register *TR::PPCTrg1MemInstruction::getTargetRegisterForLmw(uint32_t i)
   {
   // Index 0:   maps to base register of stmw
   // Index 1-N: map to the N stored registers of stmw
   // Index >N:  NULL
   if (i==0) return getMemoryReference()->getBaseRegister();

   TR::RealRegister::RegNum rrFirstSaved, rrWant;

   rrFirstSaved = toRealRegister(getTargetRegister())->getRegisterNumber();
   rrWant = (TR::RealRegister::RegNum)(rrFirstSaved+(i-1));
   if (rrWant <= TR::RealRegister::LastGPR)
       {
      TR::Machine *machine = cg()->machine();
       return machine->getRealRegister(rrWant);
       }
   return NULL;
   }


// TR::PPCTrg1MemInstruction:: member functions

TR::Register *TR::PPCTrg1MemInstruction::getMemoryDataRegister()
   {
   return getTargetRegister();
   }

bool TR::PPCTrg1MemInstruction::refsRegister(TR::Register *reg)
   {
   if (reg == getTargetRegister() ||
       getMemoryReference()->refsRegister(reg) == true)
      {
      return true;
      }
   return false;
   }

bool TR::PPCTrg1MemInstruction::usesRegister(TR::Register *reg)
   {
   if (getMemoryReference()->refsRegister(reg) == true)
      {
      return true;
      }
   return false;
   }

void TR::PPCTrg1MemInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::MemoryReference *mref = getMemoryReference();
   TR::Register           *targetVirtual = getTargetRegister();
   TR::Register           *mBaseVirtual = mref->getModBase();
   TR::RealRegister       *mBaseReal;
   bool                   excludeGPR0 = mref->isUsingStaticTOC() &&
                                        (mref->getUnresolvedSnippet() == NULL) &&
                                        (mref->getTOCOffset()>UPPER_IMMED || mref->getTOCOffset()<LOWER_IMMED);

   if (targetVirtual == mref->getBaseRegister())
      excludeGPR0 = true;

   mref->blockRegisters();
   TR::PPCTrg1Instruction::assignRegisters(kindToBeAssigned, excludeGPR0);
   mref->unblockRegisters();

   targetVirtual->block();
   mref->assignRegisters(this, cg());
   targetVirtual->unblock();

   mBaseReal = (mBaseVirtual==NULL)?NULL:toRealRegister(mref->getModBase());

   if (mBaseVirtual!=NULL)
      {
      cg()->machine()->decFutureUseCountAndUnlatch(mBaseVirtual);
      }
   }

// TR::PPCControlFlowInstruction:: member functions

bool TR::PPCControlFlowInstruction::refsRegister(TR::Register *reg)
   {
   int numTargets;
   int numSources;
   int i;
   numTargets = getNumTargets();
   numSources = getNumSources();
   for (i=0; i<numTargets; i++)
      {
      if (reg == getTargetRegister(i))
         {
         return true;
         }
      }
   for (i=0; i<numSources; i++)
      {
      if (!isSourceImmediate(i) && reg == getSourceRegister(i))
         {
         return true;
         }
      }
   return false;
   }

bool TR::PPCControlFlowInstruction::defsRegister(TR::Register *reg)
   {
   int numTargets;
   int i;
   numTargets = getNumTargets();
   for (i=0; i<numTargets; i++)
      {
      if (reg == getTargetRegister(i))
         {
         return true;
         }
      }
   return false;
   }

bool TR::PPCControlFlowInstruction::defsRealRegister(TR::Register *reg)
   {
   int numTargets;
   int i;
   numTargets = getNumTargets();
   for (i=0; i<numTargets; i++)
      {
      if (reg == getTargetRegister(i)->getAssignedRegister())
         {
         return true;
         }
      }
   return false;
   }

bool TR::PPCControlFlowInstruction::usesRegister(TR::Register *reg)
   {
   int numSources;
   int i;
   numSources = getNumSources();
   for (i=0; i<numSources; i++)
      {
      if (!isSourceImmediate(i) && reg == getSourceRegister(i))
         {
         return true;
         }
      }
   return false;
   }

void TR::PPCControlFlowInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine = cg()->machine();
   TR::Node          *currentNode = getNode();
   TR::RealRegister  *realSources[8], *realTargets[5];
   int numSources;
   int numTargets;
   int i;
   numSources = getNumSources();
   numTargets = getNumTargets();
   if (_conditions != NULL)
      _conditions->assignPostConditionRegisters(this, kindToBeAssigned, cg());
   for (i=0; i<numSources; i++)
      {
      if (!isSourceImmediate(i))
         getSourceRegister(i)->block();
      }
   for (i=0; i<numTargets; i++)
      {
      getTargetRegister(i)->block();
      }
   for (i=0; i<numTargets; i++)
      {
      TR::Register         *targetVirtual = getTargetRegister(i);
      TR::RealRegister  *assignedRegister = targetVirtual->getAssignedRealRegister();
      if (assignedRegister == NULL)
         {
         assignedRegister = machine->assignOneRegister(this, targetVirtual, false);
         }
      realTargets[i] = assignedRegister;
      targetVirtual->block();
      }
   for (i=0; i<numSources; i++)
      {
      if (!isSourceImmediate(i))
         {
         TR::Register      *sourceVirtual = getSourceRegister(i);
         TR::RealRegister  *assignedRegister = sourceVirtual->getAssignedRealRegister();
         if (assignedRegister == NULL)
            {
            assignedRegister = machine->assignOneRegister(this, sourceVirtual, false);
            }
         realSources[i] = assignedRegister;
         sourceVirtual->block();
         }
      }
   for (i=0; i<numTargets; i++)
      {
      TR::Register *targetVirtual = getTargetRegister(i);

      targetVirtual->unblock();
      machine->decFutureUseCountAndUnlatch(targetVirtual);
      setTargetRegister(i, realTargets[i]);
      }
   for (i=0; i<numSources; i++)
      {
      if (!isSourceImmediate(i))
         {
         TR::Register *sourceVirtual = getSourceRegister(i);

         sourceVirtual->unblock();
         machine->decFutureUseCountAndUnlatch(sourceVirtual);
         setSourceRegister(i, realSources[i]);
         }
      }
   // Now that registers have been allocated, expand the code
   TR::Instruction *cursor = this;
   TR::LabelSymbol *label2   = TR::LabelSymbol::create(cg()->trHeapMemory(),cg());
   TR::LabelSymbol *label1;
   TR::MemoryReference *tempMR = NULL;
   TR::SymbolReference    *temp;

   switch(getOpCode().getOpCodeValue())
      {
      case TR::InstOpCode::iflong:
         if (isSourceImmediate(2))
            {
            if (currentNode->getOpCode().isUnsigned())
               cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpli4, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceImmediate(2), cursor));
            else
               cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi4, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceImmediate(2), cursor));
            }
         else
            {
            if (currentNode->getOpCode().isUnsigned())
               cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::cmpl4, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(2), cursor));
            else
               cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::cmp4, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(2), cursor));
            }
         if (getOpCode2Value() == TR::InstOpCode::bne)
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, getLabelSymbol(), getTargetRegister(0), cursor));
         else
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, label2, getTargetRegister(0), cursor));
         if (isSourceImmediate(3))
            cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpli4, currentNode, getTargetRegister(0), getSourceRegister(1), getSourceImmediate(3), cursor));
         else
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::cmpl4, currentNode, getTargetRegister(0), getSourceRegister(1), getSourceRegister(3), cursor));
         if (getOpCode2Value() == TR::InstOpCode::beq)
            {
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), getOpCode2Value(), currentNode, getLabelSymbol(), getTargetRegister(0), cursor));
            cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
            }
         else
            {
            cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), getOpCode2Value(), currentNode, getLabelSymbol(), getTargetRegister(0), cursor));
            }
         break;
      case TR::InstOpCode::setblong:
         label1 = TR::LabelSymbol::create(cg()->trHeapMemory(),cg());
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), getOpCode2Value(), currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(2), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, label1, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::cmpl4, currentNode, getTargetRegister(0), getSourceRegister(1), getSourceRegister(3), cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), getOpCode3Value(), currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::setbool:
         if (cg()->comp()->target().cpu.id() >= TR_PPCp9 && getCmpOpValue() == TR::InstOpCode::fcmpu && (getOpCode2Value() == TR::InstOpCode::blt || getOpCode2Value() ==  TR::InstOpCode::bgt))
            {

            /* Used with: dcmpgt, dcmplt, fcmpgt and fcmplt*/
            /* Determine relationship between the two inputs, translate correct return value into GT bit of the Target Condition Register, clear the LT field of the target condition register and finally
            set the result. */

            int64_t imm = (TR::RealRegister::CRCC_LT <<  TR::RealRegister::pos_RT | TR::RealRegister::CRCC_LT <<  TR::RealRegister::pos_RA | TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB);

            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::fcmpu, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(1), cursor));

            if (getOpCode2Value() != TR::InstOpCode::bgt)
               cg()->traceRAInstruction(cursor = generateTrg1Src2ImmInstruction(cg(), TR::InstOpCode::cror, currentNode, getTargetRegister(0), getTargetRegister(0), getTargetRegister(0), getSourceImmediate(2), cursor));

            cg()->traceRAInstruction(cursor = generateTrg1Src2ImmInstruction(cg(), TR::InstOpCode::crxor, currentNode, getTargetRegister(0), getTargetRegister(0), getTargetRegister(0), imm, cursor));
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction(cg(), TR::InstOpCode::setb, currentNode, getTargetRegister(1), getTargetRegister(0), cursor));

            }
         else
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), getCmpOpValue(), currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(1), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 1, cursor));
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), getOpCode2Value(), currentNode, label2, getTargetRegister(0), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
            cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
            }
         break;
      case TR::InstOpCode::setbflt:
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), getCmpOpValue(), currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(1), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), getOpCode2Value(), currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), getOpCode3Value(), currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::lcmp:
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::cmp4, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(2), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bgt, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::blt, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::cmpl4, currentNode, getTargetRegister(0), getSourceRegister(1), getSourceRegister(3), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bgt, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::blt, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::flcmpl:
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::fcmpu, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(1), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bgt, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::flcmpg:
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::fcmpu, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(1), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::blt, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 1, cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::idiv:
         cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi4, currentNode, getTargetRegister(0), getSourceRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction(cg(), TR::InstOpCode::neg, currentNode, getTargetRegister(1), getSourceRegister(0), cursor));
         if (cg()->comp()->target().cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, currentNode, label2, getTargetRegister(0), cursor));
         else
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::divw, currentNode, getTargetRegister(1), getSourceRegister(0), getSourceRegister(1), cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::irem:
         cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi4, currentNode, getTargetRegister(0), getSourceRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
         if (cg()->comp()->target().cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, currentNode, label2, getTargetRegister(0), cursor));
         else
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, currentNode, label2, getTargetRegister(0), cursor));
         if (cg()->comp()->target().cpu.id() >= TR_PPCp9)
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::modsw, currentNode, getTargetRegister(1), getSourceRegister(0), getSourceRegister(1), cursor));
            }
         else
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::divw, currentNode, getTargetRegister(1), getSourceRegister(0), getSourceRegister(1), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::mullw, currentNode, getTargetRegister(1), getSourceRegister(1), getTargetRegister(1), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::subf, currentNode, getTargetRegister(1), getTargetRegister(1), getSourceRegister(0), cursor));
            }
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::ldiv:
         cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi8, currentNode, getTargetRegister(0), getSourceRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction(cg(), TR::InstOpCode::neg, currentNode, getTargetRegister(1), getSourceRegister(0), cursor));
         if (cg()->comp()->target().cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, currentNode, label2, getTargetRegister(0), cursor));
         else
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, currentNode, label2, getTargetRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::divd, currentNode, getTargetRegister(1), getSourceRegister(0), getSourceRegister(1), cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::lrem:
         cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi8, currentNode, getTargetRegister(0), getSourceRegister(1), -1, cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(1), 0, cursor));
         if (cg()->comp()->target().cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, currentNode, label2, getTargetRegister(0), cursor));
         else
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, currentNode, label2, getTargetRegister(0), cursor));
         if (cg()->comp()->target().cpu.id() >= TR_PPCp9)
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::modsd, currentNode, getTargetRegister(1), getSourceRegister(0), getSourceRegister(1), cursor));
            }
         else
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::divd, currentNode, getTargetRegister(1), getSourceRegister(0), getSourceRegister(1), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::mulld, currentNode, getTargetRegister(1), getSourceRegister(1), getTargetRegister(1), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::subf, currentNode, getTargetRegister(1), getTargetRegister(1), getSourceRegister(0), cursor));
            }
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));
         break;
      case TR::InstOpCode::d2i:
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::fcmpu, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction(cg(), TR::InstOpCode::fctiwz, currentNode, getTargetRegister(1), getSourceRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(2), 0, cursor));

         if (cg()->comp()->target().cpu.id() < TR_PPCp8)
            {
            tempMR = new (cg()->trHeapMemory()) TR::MemoryReference(cg()->getStackPointerRegister(), -8, 8, cg());
            cg()->traceRAInstruction(cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::stfd, currentNode, tempMR, getTargetRegister(1), cursor));
            }

         if (cg()->comp()->target().cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, currentNode, label2, getTargetRegister(0), cursor));
         else
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, label2, getTargetRegister(0), cursor));

         if (cg()->comp()->target().cpu.id() >= TR_PPCp8)
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction(cg(), TR::InstOpCode::mfvsrwz, currentNode, getTargetRegister(2), getTargetRegister(1), cursor));
         else
            cg()->traceRAInstruction(cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::lwz, currentNode, getTargetRegister(2), new (cg()->trHeapMemory()) TR::MemoryReference(currentNode, *tempMR, 4, 4, cg()), cursor));
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));

         break;
      case TR::InstOpCode::d2l:
         cg()->traceRAInstruction(cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::fcmpu, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(0), cursor));
         cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction(cg(), TR::InstOpCode::fctidz, currentNode, getTargetRegister(1), getSourceRegister(0), cursor));

         if (cg()->comp()->target().is64Bit())
            cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(2), 0, cursor));
         else
            {
            cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(2), 0, cursor));
            cg()->traceRAInstruction(cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, currentNode, getTargetRegister(3), 0, cursor));
            }
         if (cg()->comp()->target().cpu.id() < TR_PPCp8 || cg()->comp()->target().is32Bit())
            {
            tempMR = new (cg()->trHeapMemory()) TR::MemoryReference(cg()->getStackPointerRegister(), -8, 8, cg());
            cg()->traceRAInstruction(cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::stfd, currentNode, tempMR, getTargetRegister(1), cursor));
            }

         if (cg()->comp()->target().cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, currentNode, label2, getTargetRegister(0), cursor));
         else
            cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, label2, getTargetRegister(0), cursor));

         if (cg()->comp()->target().is64Bit())
            {
            if (cg()->comp()->target().cpu.id() >= TR_PPCp8 && cg()->comp()->target().is64Bit())
               cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction(cg(), TR::InstOpCode::mfvsrd, currentNode, getTargetRegister(2), getTargetRegister(1), cursor));
            else
               cg()->traceRAInstruction(cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ld, currentNode, getTargetRegister(2), tempMR, cursor));
            }
         else
            {
            cg()->traceRAInstruction(cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::lwz, currentNode, getTargetRegister(2), new (cg()->trHeapMemory()) TR::MemoryReference(currentNode, *tempMR, 0, 4, cg()), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::lwz, currentNode, getTargetRegister(3), new (cg()->trHeapMemory()) TR::MemoryReference(currentNode, *tempMR, 4, 4, cg()), cursor));
            }
         cg()->traceRAInstruction(cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, currentNode, label2, cursor));

         break;
      case TR::InstOpCode::iselect:
         {
         cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi4, currentNode, getTargetRegister(0), getSourceRegister(0), 0, cursor));

         if (useRegPairForResult())
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction   (cg(), getOpCode2Value(), currentNode, getTargetRegister(1), getSourceRegister(1), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction   (cg(), getOpCode2Value(), currentNode, getTargetRegister(2), getSourceRegister(2), cursor));
            }
         else
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction   (cg(), getOpCode2Value(), currentNode, getTargetRegister(1), getSourceRegister(1), cursor));
            }
         cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, label2, getTargetRegister(0), cursor));

         if (useRegPairForResult())
            {
            if (useRegPairForCond())
               {
               cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi4, currentNode, getTargetRegister(0), getSourceRegister(5), 0, cursor));
               cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, label2, getTargetRegister(0), cursor));
               }
            }
         else
            {
            if (useRegPairForCond())
               {
               cg()->traceRAInstruction(cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::cmpi4, currentNode, getTargetRegister(0), getSourceRegister(3), 0, cursor));
               cg()->traceRAInstruction(cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, currentNode, label2, getTargetRegister(0), cursor));
               }
            }

         if (useRegPairForResult())
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction   (cg(), getOpCode2Value(), currentNode, getTargetRegister(1), getSourceRegister(3), cursor));
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction   (cg(), getOpCode2Value(), currentNode, getTargetRegister(2), getSourceRegister(4), cursor));
            }
         else
            {
            cg()->traceRAInstruction(cursor = generateTrg1Src1Instruction   (cg(), getOpCode2Value(), currentNode, getTargetRegister(1), getSourceRegister(2), cursor));
            }
         cg()->traceRAInstruction(cursor = generateLabelInstruction      (cg(), TR::InstOpCode::label,currentNode, label2, cursor));
         }
         break;
      default:
         TR_ASSERT(false,"unknown control flow instruction ");
      }

   if (_conditions != NULL)
      _conditions->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }
