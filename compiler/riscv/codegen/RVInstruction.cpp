/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "codegen/RVInstruction.hpp"

#include <stddef.h>                               // for NULL
#include <stdint.h>                               // for int32_t, uint32_t, etc
#include <riscv.h>
#include "codegen/CodeGenerator.hpp"              // for CodeGenerator, etc
#include "codegen/InstOpCode.hpp"                 // for InstOpCode, etc
#include "codegen/Instruction.hpp"                // for Instruction
#include "codegen/Machine.hpp"                    // for Machine, etc
#include "codegen/MemoryReference.hpp"            // for MemoryReference
#include "codegen/RealRegister.hpp"               // for RealRegister, etc
#include "codegen/Register.hpp"                   // for Register
#include "codegen/RegisterConstants.hpp"          // for TR_RegisterKinds, etc
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"   // for RegisterDependency
#include "codegen/Relocation.hpp"              // for TR::ExternalRelocation, etc
#include "compile/Compilation.hpp"             // for Compilation
#include "compile/ResolvedMethod.hpp"          // for TR_ResolvedMethod
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"                       // for CONSTANT64
#include "il/ILOps.hpp"                           // for ILOpCode
#include "il/Node.hpp"                            // for Node
#include "infra/Assert.hpp"                       // for TR_ASSERT
#include "codegen/RVOutOfLineCodeSection.hpp"
#include "codegen/GenerateInstructions.hpp"

#define TR_RISCV_RTYPE(insn, rd, rs1, rs2) \
  ((insn) | ((rd) << OP_SH_RD) | ((rs1) << OP_SH_RS1) | ((rs2) << OP_SH_RS2))
#define TR_RISCV_ITYPE(insn, rd, rs1, imm) \
  ((insn) | ((rd) << OP_SH_RD) | ((rs1) << OP_SH_RS1) | ENCODE_ITYPE_IMM(imm))
#define TR_RISCV_STYPE(insn, rs1, rs2, imm) \
  ((insn) | ((rs1) << OP_SH_RS1) | ((rs2) << OP_SH_RS2) | ENCODE_STYPE_IMM(imm))
#define TR_RISCV_SBTYPE(insn, rs1, rs2, target) \
  ((insn) | ((rs1) << OP_SH_RS1) | ((rs2) << OP_SH_RS2) | ENCODE_SBTYPE_IMM(target))
#define TR_RISCV_UTYPE(insn, rd, bigimm) \
  ((insn) | ((rd) << OP_SH_RD) | ENCODE_UTYPE_IMM(bigimm))
#define TR_RISCV_UJTYPE(insn, rd, target) \
  ((insn) | ((rd) << OP_SH_RD) | ENCODE_UJTYPE_IMM(target))

// TR::RtypeInstruction:: member functions

bool TR::RtypeInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister() ||
           reg == getSource1Register() ||
           reg == getSource2Register());
   }

bool TR::RtypeInstruction::usesRegister(TR::Register *reg)
   {
   return (reg == getSource1Register() || reg == getSource2Register());
   }

bool TR::RtypeInstruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister());
   }

bool TR::RtypeInstruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister()->getAssignedRegister());
   }

void TR::RtypeInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine = cg()->machine();
   TR::Register *target1Virtual = getTargetRegister();
   TR::Register *source1Virtual = getSource1Register();
   TR::Register *source2Virtual = getSource2Register();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   source1Virtual->block();
   target1Virtual->block();
   TR::RealRegister *assignedSource2Register = machine->assignOneRegister(this, source2Virtual);
   target1Virtual->unblock();
   source1Virtual->unblock();

   source2Virtual->block();
   target1Virtual->block();
   TR::RealRegister *assignedSource1Register = machine->assignOneRegister(this, source1Virtual);
   target1Virtual->unblock();
   source2Virtual->unblock();

   source2Virtual->block();
   source1Virtual->block();
   TR::RealRegister *assignedTarget1Register = machine->assignOneRegister(this, target1Virtual);
   source1Virtual->unblock();
   source2Virtual->unblock();

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   setTargetRegister(assignedTarget1Register);
   setSource1Register(assignedSource1Register);
   setSource2Register(assignedSource2Register);
   }

uint8_t *TR::RtypeInstruction::generateBinaryEncoding() {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;

   *iPtr = TR_RISCV_RTYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getTargetRegister())->binaryRegCode(),
                         toRealRegister(getSource1Register())->binaryRegCode(),
                         toRealRegister(getSource2Register())->binaryRegCode());

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
}

// TR::ItypeInstruction:: member functions

bool TR::ItypeInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister() ||
           reg == getSource1Register() );
   }

bool TR::ItypeInstruction::usesRegister(TR::Register *reg)
   {
   return (reg == getSource1Register());
   }

bool TR::ItypeInstruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister());
   }

bool TR::ItypeInstruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister()->getAssignedRegister());
   }

void TR::ItypeInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine = cg()->machine();
   TR::Register *target1Virtual = getTargetRegister();
   TR::Register *source1Virtual = getSource1Register();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   target1Virtual->block();
   TR::RealRegister *assignedSource1Register = machine->assignOneRegister(this, source1Virtual);
   target1Virtual->unblock();

   source1Virtual->block();
   TR::RealRegister *assignedTarget1Register = machine->assignOneRegister(this, target1Virtual);
   source1Virtual->unblock();

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   setTargetRegister(assignedTarget1Register);
   setSource1Register(assignedSource1Register);
   }

uint8_t *TR::ItypeInstruction::generateBinaryEncoding() {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;

   *iPtr = TR_RISCV_ITYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getTargetRegister())->binaryRegCode(),
                         toRealRegister(getSource1Register())->binaryRegCode(),
                         getSourceImmediate());

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
}

// TR::LoadInstruction:: member functions

bool TR::LoadInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister() || getMemoryReference()->refsRegister(reg));
   }

bool TR::LoadInstruction::usesRegister(TR::Register *reg)
   {
   return getMemoryReference()->refsRegister(reg);
   }

bool TR::LoadInstruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister());
   }

bool TR::LoadInstruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister()->getAssignedRegister());
   }

void TR::LoadInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine = cg()->machine();
   TR::MemoryReference *mref = getMemoryReference();
   TR::Register *targetVirtual = getTargetRegister();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   mref->blockRegisters();
   setTargetRegister(machine->assignOneRegister(this, targetVirtual));
   mref->unblockRegisters();

   targetVirtual->block();
   mref->assignRegisters(this, cg());
   targetVirtual->unblock();

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

uint8_t *TR::LoadInstruction::generateBinaryEncoding() {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;

   *iPtr = TR_RISCV_ITYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getTargetRegister())->binaryRegCode(),
                         toRealRegister(getMemoryBase())->binaryRegCode(),
                         getMemoryReference()->getOffset(true));

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
}


// TR::StypeInstruction:: member functions

bool TR::StypeInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getSource1Register() || reg == getSource2Register());
   }

bool TR::StypeInstruction::usesRegister(TR::Register *reg)
   {
   return (reg == getSource1Register() || reg == getSource2Register());
   }

bool TR::StypeInstruction::defsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::StypeInstruction::defsRealRegister(TR::Register *reg)
   {
   return false;
   }

void TR::StypeInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine = cg()->machine();
   TR::Register *source1Virtual = getSource1Register();
   TR::Register *source2Virtual = getSource2Register();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   source1Virtual->block();
   TR::RealRegister *assignedSource2Register = machine->assignOneRegister(this, source2Virtual);
   source1Virtual->unblock();

   source2Virtual->block();
   TR::RealRegister *assignedSource1Register = machine->assignOneRegister(this, source1Virtual);
   source2Virtual->unblock();

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   setSource1Register(assignedSource1Register);
   setSource2Register(assignedSource2Register);
   }

uint8_t *TR::StypeInstruction::generateBinaryEncoding() {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;

   *iPtr = TR_RISCV_STYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getSource1Register())->binaryRegCode(),
                         toRealRegister(getSource2Register())->binaryRegCode(),
                         getSourceImmediate());

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
}

// TR::StoreInstruction:: member functions

bool TR::StoreInstruction::refsRegister(TR::Register *reg)
   {
   return (getMemoryReference()->refsRegister(reg) || reg == getSource1Register());
   }

bool TR::StoreInstruction::usesRegister(TR::Register *reg)
   {
   return (getMemoryReference()->refsRegister(reg) || reg == getSource1Register());
   }

bool TR::StoreInstruction::defsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::StoreInstruction::defsRealRegister(TR::Register *reg)
   {
   return false;
   }

void TR::StoreInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine = cg()->machine();
   TR::MemoryReference *mref = getMemoryReference();
   TR::Register *source1Virtual = getSource1Register();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   source1Virtual->block();
   mref->assignRegisters(this, cg());
   source1Virtual->unblock();

   mref->blockRegisters();
   setSource1Register(machine->assignOneRegister(this, source1Virtual));
   mref->unblockRegisters();

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

uint8_t *TR::StoreInstruction::generateBinaryEncoding() {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;

   *iPtr = TR_RISCV_STYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getMemoryBase())->binaryRegCode(),
                         toRealRegister(getSource1Register())->binaryRegCode(),
                         getMemoryReference()->getOffset(true));

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
}

// TR::BtypeInstruction:: member functions

uint8_t *TR::BtypeInstruction::generateBinaryEncoding()
   {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;

   TR::LabelSymbol *label            = getLabelSymbol();

   *iPtr = TR_RISCV_SBTYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getSource1Register())->binaryRegCode(),
                         toRealRegister(getSource2Register())->binaryRegCode(),
                         0 /* to fix up in the future */ );

   if (label->getCodeLocation() != NULL) {
           // we already know the address
           int32_t delta = label->getCodeLocation() - cursor;
           *iPtr |= ENCODE_SBTYPE_IMM(delta);
   } else {
           cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(cursor, label));
   }

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }


int32_t TR::BtypeInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   // Conditional branches can be expanded into a conditional branch around an unconditional branch if the target label
   // is out of range for a simple b-type instruction. This is done by expandFarConditionalBranches, which runs after binary
   // length estimation but before binary encoding and will call BtypeInstruction::expandIntoFarBranch to
   // expand the branch into two instructions. For this reason, we conservatively assume that any conditional branch
   // could be expanded to ensure that the binary length estimates are correct.
   setEstimatedBinaryLength(RISCV_INSTRUCTION_LENGTH * 2);
   setEstimatedBinaryLocation(currentEstimate);
   return currentEstimate + self()->getEstimatedBinaryLength();
   }

TR::BtypeInstruction *TR::BtypeInstruction::getBtypeInstruction()
   {
   return this;
   }


void TR::BtypeInstruction::expandIntoFarBranch()
   {
   TR_ASSERT_FATAL(getLabelSymbol(), "Attempt to expand conditional branch %p without a label", self());

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "Expanding conditional branch instruction %p into a far branch\n", self());

   TR::RealRegister *zero = cg()->machine()->getRealRegister(TR::RealRegister::zero);

   TR::InstOpCode::Mnemonic newOpCode = TR::InstOpCode::reversedBranchOpCode(getOpCodeValue());

   setOpCodeValue(newOpCode);

   TR::LabelSymbol *skipBranchLabel = generateLabelSymbol(cg());
   skipBranchLabel->setEstimatedCodeLocation(getEstimatedBinaryLocation() + RISCV_INSTRUCTION_LENGTH);

   TR::Instruction *branchInstr = generateJTYPE(TR::InstOpCode::_jal, getNode(), zero, getLabelSymbol(), cg(), self());
   branchInstr->setEstimatedBinaryLength(RISCV_INSTRUCTION_LENGTH);

   TR::Instruction *labelInstr = generateLABEL(cg(), TR::InstOpCode::label, getNode(), skipBranchLabel, branchInstr);
   labelInstr->setEstimatedBinaryLength(0);

   setLabelSymbol(skipBranchLabel);
   setEstimatedBinaryLength(RISCV_INSTRUCTION_LENGTH);
   }

// TR::UtypeInstruction:: member functions

bool TR::UtypeInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister());
   }

bool TR::UtypeInstruction::usesRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::UtypeInstruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister());
   }

bool TR::UtypeInstruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister()->getAssignedRegister());
   }

void TR::UtypeInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   TR::Machine *machine = cg()->machine();
   TR::Register *targetVirtual = getTargetRegister();
   setTargetRegister(machine->assignOneRegister(this, targetVirtual));

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }


uint8_t *TR::UtypeInstruction::generateBinaryEncoding() {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;

   *iPtr = TR_RISCV_UTYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getTargetRegister())->binaryRegCode(),
                         _imm);

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
}

// TR::JtypeInstruction:: member functions

uint8_t *TR::JtypeInstruction::generateBinaryEncoding() {
   uint8_t        *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t        *cursor           = instructionStart;
   uint32_t *iPtr = (uint32_t*)instructionStart;
   intptr_t offset = 0;

   if (getSymbolReference() != nullptr)
      {
      TR_ASSERT(getLabelSymbol() == nullptr, "Both symbol reference and symbol set in J-type instruction");
      TR::ResolvedMethodSymbol *sym = getSymbolReference()->getSymbol()->getResolvedMethodSymbol();
      TR_ResolvedMethod *resolvedMethod = sym == NULL ? NULL : sym->getResolvedMethod();

      if (resolvedMethod != NULL && resolvedMethod->isSameMethod(cg()->comp()->getCurrentMethod()))
         {
         offset = cg()->getCodeStart() - cursor;
         }
      else
         {
         TR_ASSERT(0, "Non-recursive calls not (yet) supported");
         }
      }
   else
      {
      uintptr_t destination = (uintptr_t)(getLabelSymbol()->getCodeLocation());
      if (destination != 0)
         {
         offset = destination - (uintptr_t)cursor;
         }
      else
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getLabelSymbol()));
         }
      }

   TR_ASSERT(VALID_UJTYPE_IMM(offset), "Jump offset out of range");
   *iPtr = TR_RISCV_UJTYPE ((uint32_t)(getOpCode().getOpCodeBinaryEncoding()),
                         toRealRegister(getTargetRegister())->binaryRegCode(),
                         offset);

   cursor += RISCV_INSTRUCTION_LENGTH;
   setBinaryLength(RISCV_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
}

// TR::LabelInstruction:: member functions

uint8_t *TR::LabelInstruction::generateBinaryEncoding()
   {
   TR_ASSERT(getOpCodeValue() == OMR::InstOpCode::label, "Invalid opcode for label instruction, must be ::label");

   uint8_t *instructionStart = cg()->getBinaryBufferCursor();

   getLabelSymbol()->setCodeLocation(instructionStart);

   setBinaryLength(0);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return instructionStart;
   }

int32_t TR::LabelInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   TR_ASSERT(getOpCodeValue() == OMR::InstOpCode::label, "Invalid opcode for label instruction, must be ::label");

   setEstimatedBinaryLength(0);
   getLabelSymbol()->setEstimatedCodeLocation(currentEstimate);
   return currentEstimate;
   }

uint8_t *TR::AdminInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   TR::InstOpCode::Mnemonic op = getOpCodeValue();

   if (op != OMR::InstOpCode::proc && op != OMR::InstOpCode::fence && op != OMR::InstOpCode::retn)
      {
      TR_ASSERT(false, "Unsupported opcode in AdminInstruction.");
      }

   setBinaryLength(0);
   setBinaryEncoding(instructionStart);

   return instructionStart;
   }

int32_t TR::AdminInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(0);
   return currentEstimate;
   }
