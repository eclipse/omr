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

#include <stdint.h>
#include <string.h>

#include "ras/Debug.hpp"

#include "codegen/RVInstruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "env/IO.hpp"
#include "il/Block.hpp"

static const char *opCodeToNameMap[] =
   {
   "bad",
/*
 * RISC-V instructions
 */
#define DECLARE_INSN(mnemonic, match, mask) #mnemonic,
#include <riscv-opc.h>
#undef DECLARE_INSN
   "proc",
   "fence",
   "return",
   "dd",
   "label"
   };

const char *
TR_Debug::getOpCodeName(TR::InstOpCode *opCode)
   {
   return opCodeToNameMap[opCode->getMnemonic()];
   }

void
TR_Debug::printMemoryReferenceComment(TR::FILE *pOutFile, TR::MemoryReference *mr)
   {
   if (pOutFile == NULL)
      return;

   TR::Symbol *symbol = mr->getSymbolReference()->getSymbol();
   if (symbol == NULL && mr->getSymbolReference()->getOffset() == 0)
      return;

   trfprintf(pOutFile, "\t\t# SymRef");
   print(pOutFile, mr->getSymbolReference());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RtypeInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource2Register(), TR_WordReg);
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ItypeInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   trfprintf(pOutFile, ", 0x%04x (%d)", instr->getSourceImmediate(), instr->getSourceImmediate());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::StypeInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource2Register(), TR_WordReg);
   trfprintf(pOutFile, ", 0x%04x (%d)", instr->getSourceImmediate(), instr->getSourceImmediate());
   trfflush(_comp->getOutFile());
   }
void

TR_Debug::print(TR::FILE *pOutFile, TR::BtypeInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource2Register(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getLabelSymbol());
   trfflush(_comp->getOutFile());
   }
void

TR_Debug::print(TR::FILE *pOutFile, TR::UtypeInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfprintf(pOutFile, ", 0x%08x (%d)", instr->getSourceImmediate(), instr->getSourceImmediate());
   trfflush(_comp->getOutFile());
   }
void

TR_Debug::print(TR::FILE *pOutFile, TR::JtypeInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   if (instr->getLabelSymbol())
      {
      print(pOutFile, instr->getLabelSymbol());
      }
   else if (instr->getSymbolReference())
      {
      print(pOutFile, instr->getSymbolReference());
      }
   else
      {
      trfprintf(pOutFile, "0x%08x (%d)", instr->getSourceImmediate(), instr->getSourceImmediate());
      }
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::LoadInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfprintf(pOutFile, " <- ");
   print(pOutFile, instr->getMemoryReference());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::StoreInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   trfprintf(pOutFile, " -> ");
   print(pOutFile, instr->getMemoryReference());
   trfflush(_comp->getOutFile());
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::Instruction *instr)
   {
   if (pOutFile == NULL)
      return;

   switch (instr->getKind())
      {
      case OMR::Instruction::IsLabel:
         print(pOutFile, (TR::LabelInstruction *)instr);
         break;
      case OMR::Instruction::IsAdmin:
         print(pOutFile, (TR::AdminInstruction *)instr);
         break;
      case OMR::Instruction::IsRTYPE:
         print(pOutFile, (TR::RtypeInstruction *)instr);
         break;
      case OMR::Instruction::IsITYPE:
         print(pOutFile, (TR::ItypeInstruction *)instr);
         break;
      case OMR::Instruction::IsSTYPE:
         print(pOutFile, (TR::StypeInstruction *)instr);
         break;
      case OMR::Instruction::IsBTYPE:
         print(pOutFile, (TR::BtypeInstruction *)instr);
         break;
      case OMR::Instruction::IsUTYPE:
         print(pOutFile, (TR::UtypeInstruction *)instr);
         break;
      case OMR::Instruction::IsJTYPE:
         print(pOutFile, (TR::JtypeInstruction *)instr);
         break;
      case OMR::Instruction::IsLOAD:
         print(pOutFile, (TR::LoadInstruction *)instr);
         break;
      case OMR::Instruction::IsSTORE:
         print(pOutFile, (TR::StoreInstruction *)instr);
         break;
      default:
         {
         printPrefix(pOutFile, instr);
         trfprintf(pOutFile, "%s", getOpCodeName(&instr->getOpCode()));
         trfflush(_comp->getOutFile());
         }
      }
   }

void
TR_Debug::printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction *instr)
   {
   while (tabStops-- > 0)
      trfprintf(pOutFile, "\t");

   dumpInstructionComments(pOutFile, instr);
   }

void
TR_Debug::printPrefix(TR::FILE *pOutFile, TR::Instruction *instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr, instr->getBinaryEncoding(), instr->getBinaryLength());
   TR::Node *node = instr->getNode();
   trfprintf(pOutFile, "%d \t", node ? node->getByteCodeIndex() : 0);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::LabelInstruction *instr)
   {
   printPrefix(pOutFile, instr);

   TR::LabelSymbol *label = instr->getLabelSymbol();
   TR::Snippet *snippet = label ? label->getSnippet() : NULL;
   if (instr->getOpCodeValue() == TR::InstOpCode::label)
      {
      print(pOutFile, label);
      trfprintf(pOutFile, ":");
      if (label->isStartInternalControlFlow())
         trfprintf(pOutFile, "\t; (Start of internal control flow)");
      else if (label->isEndInternalControlFlow())
         trfprintf(pOutFile, "\t; (End of internal control flow)");
      }
   else
      {
      trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
      print(pOutFile, label);
      if (snippet)
         {
         TR_UNIMPLEMENTED();
         }
      }
   printInstructionComment(pOutFile, 1, instr);
   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::AdminInstruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s ", getOpCodeName(&instr->getOpCode()));

   TR::Node *node = instr->getNode();
   if (node)
      {
      if (node->getOpCodeValue() == TR::BBStart)
         {
         trfprintf(pOutFile, " (BBStart (block_%d))", node->getBlock()->getNumber());
         }
      else if (node->getOpCodeValue() == TR::BBEnd)
         {
         trfprintf(pOutFile, " (BBEnd (block_%d))", node->getBlock()->getNumber());
         }
      }

   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());

   trfflush(pOutFile);
   }

/*
void
TR_Debug::print(TR::FILE *pOutFile, TR::RVTrg1Instruction *instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfflush(_comp->getOutFile());
   }
*/

void
TR_Debug::print(TR::FILE *pOutFile, TR::RegisterDependency *dep)
   {
   trfprintf(pOutFile,"[");
   print(pOutFile, dep->getRegister(), TR_WordReg);
   trfprintf(pOutFile," : ");
   trfprintf(pOutFile,"%s] ", getRVRegisterName(dep->getRealRegister()));
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RegisterDependencyConditions *conditions)
   {
    if (conditions)
      {
      int i;
      trfprintf(pOutFile,"\n PRE: ");
      for (i=0; i<conditions->getAddCursorForPre(); i++)
         {
         print(pOutFile, conditions->getPreConditions()->getRegisterDependency(i));
         }
      trfprintf(pOutFile,"\nPOST: ");
      for (i=0; i<conditions->getAddCursorForPost(); i++)
         {
         print(pOutFile, conditions->getPostConditions()->getRegisterDependency(i));
         }
      trfflush(_comp->getOutFile());
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::MemoryReference *mr)
   {
   trfprintf(pOutFile, "[");

   if (mr->getBaseRegister() != NULL)
      {
      print(pOutFile, mr->getBaseRegister());
      trfprintf(pOutFile, ", ");
      }

   trfprintf(pOutFile, "%d", mr->getOffset(true));

   trfprintf(pOutFile, "]");
   }

void
TR_Debug::printRVGCRegisterMap(TR::FILE *pOutFile, TR::GCRegisterMap *map)
   {
   TR_UNIMPLEMENTED();
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RealRegister *reg, TR_RegisterSizes size)
   {
   trfprintf(pOutFile, "%s", getName(reg, size));
   }

static const char *
getRegisterName(TR::RealRegister::RegNum num, bool is64bit)
   {
   switch (num)
      {
#define DECLARE_GPR(regname, abiname, encoding) \
     case TR::RealRegister:: regname: return #abiname;
#define DECLARE_FPR(regname, abiname, encoding) \
     case TR::RealRegister:: regname: return #abiname;
#include "codegen/riscv-regs.h"
#undef DECLARE_GPR
#undef DECLARE_FPR
      default: return "???";
      }
   }

const char *
TR_Debug::getName(TR::RealRegister *reg, TR_RegisterSizes size)
   {
   return getRegisterName(reg->getRegisterNumber(), (size == TR_DoubleWordReg || size == TR_DoubleReg));
   }

const char *
TR_Debug::getRVRegisterName(uint32_t regNum, bool is64bit)
   {
   return getRegisterName((TR::RealRegister::RegNum)regNum, is64bit);
   }

void TR_Debug::printRVOOLSequences(TR::FILE *pOutFile)
   {
   TR_UNIMPLEMENTED();
   }
