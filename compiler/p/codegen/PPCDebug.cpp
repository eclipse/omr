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

#ifndef TR_TARGET_POWER
int jitDebugPPC;
#else

#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/GCRegisterMap.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "p/codegen/PPCOutOfLineCodeSection.hpp"
#include "codegen/Snippet.hpp"
#include "ras/Debug.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "p/codegen/CallSnippet.hpp"
#include "p/codegen/InterfaceCastSnippet.hpp"
#include "p/codegen/StackCheckFailureSnippet.hpp"
#endif

namespace TR { class PPCForceRecompilationSnippet; }
namespace TR { class PPCRecompilationSnippet; }

const char *
TR_Debug::getOpCodeName(TR::InstOpCode * opCode)
   {
   return TR::InstOpCode::metadata[opCode->getOpCodeValue()].name;
   }

void
TR_Debug::printMemoryReferenceComment(TR::FILE *pOutFile, TR::MemoryReference  *mr)
   {
   if (pOutFile == NULL)
      return;

   TR::Symbol *symbol = mr->getSymbolReference()->getSymbol();

   if (symbol == NULL && mr->getSymbolReference()->getOffset() == 0)
      return;

   if (symbol && symbol->isSpillTempAuto())
      {
      const char *prefix = (symbol->getDataType() == TR::Float ||
                            symbol->getDataType() == TR::Double) ? "#FP" : "#";
      trfprintf(pOutFile, " # %sSPILL%d", prefix, symbol->getSize());
      }

   trfprintf(pOutFile, "\t\t# SymRef");
   print(pOutFile, mr->getSymbolReference());

   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::Instruction * instr)
   {
   if (pOutFile == NULL)
      return;

   switch (instr->getKind())
      {
      case OMR::Instruction::IsAlignmentNop:
         print(pOutFile, (TR::PPCAlignmentNopInstruction *)instr);
         break;
      case OMR::Instruction::IsImm:
         print(pOutFile, (TR::PPCImmInstruction *)instr);
         break;
      case OMR::Instruction::IsImm2:
         print(pOutFile, (TR::PPCImm2Instruction *)instr);
         break;
      case OMR::Instruction::IsSrc1:
         print(pOutFile, (TR::PPCSrc1Instruction *)instr);
         break;
      case OMR::Instruction::IsDep:
         print(pOutFile, (TR::PPCDepInstruction *)instr);
         break;
      case OMR::Instruction::IsDepImmSym:
         print(pOutFile, (TR::PPCDepImmSymInstruction *)instr);
         break;
      case OMR::Instruction::IsLabel:
         print(pOutFile, (TR::PPCLabelInstruction *)instr);
         break;
      case OMR::Instruction::IsDepLabel:
         print(pOutFile, (TR::PPCDepLabelInstruction *)instr);
         break;
#ifdef J9_PROJECT_SPECIFIC
      case OMR::Instruction::IsVirtualGuardNOP:
         print(pOutFile, (TR::PPCVirtualGuardNOPInstruction *)instr);
         break;
#endif
      case OMR::Instruction::IsConditionalBranch:
         print(pOutFile, (TR::PPCConditionalBranchInstruction *)instr);
         break;
      case OMR::Instruction::IsDepConditionalBranch:
         print(pOutFile, (TR::PPCDepConditionalBranchInstruction *)instr);
         break;
      case OMR::Instruction::IsAdmin:
         print(pOutFile, (TR::PPCAdminInstruction *)instr);
         break;
      case OMR::Instruction::IsTrg1:
         print(pOutFile, (TR::PPCTrg1Instruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Imm:
         print(pOutFile, (TR::PPCTrg1ImmInstruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Src1:
         print(pOutFile, (TR::PPCTrg1Src1Instruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Src1Imm:
         print(pOutFile, (TR::PPCTrg1Src1ImmInstruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Src1Imm2:
         print(pOutFile, (TR::PPCTrg1Src1Imm2Instruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Src2:
         print(pOutFile, (TR::PPCTrg1Src2Instruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Src2Imm:
         print(pOutFile, (TR::PPCTrg1Src2ImmInstruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Src3:
         print(pOutFile, (TR::PPCTrg1Src3Instruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Mem:
         print(pOutFile, (TR::PPCTrg1MemInstruction *)instr);
         break;
      case OMR::Instruction::IsSrc2:
         print(pOutFile, (TR::PPCSrc2Instruction *)instr);
         break;
      case OMR::Instruction::IsMem:
         print(pOutFile, (TR::PPCMemInstruction *)instr);
         break;
      case OMR::Instruction::IsMemSrc1:
         print(pOutFile, (TR::PPCMemSrc1Instruction *)instr);
         break;
      case OMR::Instruction::IsControlFlow:
         print(pOutFile, (TR::PPCControlFlowInstruction *)instr);
         break;
      default:
         TR_ASSERT( 0, "unexpected instruction kind");
            // fall thru
      case OMR::Instruction::IsNotExtended:
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
TR_Debug::printPrefix(TR::FILE *pOutFile, TR::Instruction * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr, instr->getBinaryEncoding(), instr->getBinaryLength());
   if (instr->getNode())
      {
      trfprintf(pOutFile, "%d \t", instr->getNode()->getByteCodeIndex());
      }
   else
      trfprintf(pOutFile, "0 \t");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCAlignmentNopInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t; Align to %u bytes", getOpCodeName(&instr->getOpCode()), instr->getAlignment());
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCDepInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s", getOpCodeName(&instr->getOpCode()));
   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());
   trfflush(pOutFile);

   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCLabelInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   TR::LabelSymbol *label   = instr->getLabelSymbol();
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
         TR::SymbolReference *callSym = NULL;
         switch(snippet->getKind())
            {
#ifdef J9_PROJECT_SPECIFIC
            case TR::Snippet::IsCall:
            case TR::Snippet::IsUnresolvedCall:
               callSym = ((TR::PPCCallSnippet *)snippet)->getNode()->getSymbolReference();
               break;
            case TR::Snippet::IsVirtual:
            case TR::Snippet::IsVirtualUnresolved:
            case TR::Snippet::IsInterfaceCall:
               callSym = ((TR::PPCCallSnippet *)snippet)->getNode()->getSymbolReference();
               break;
#endif
            case TR::Snippet::IsHelperCall:
            case TR::Snippet::IsMonitorEnter:
            case TR::Snippet::IsMonitorExit:
            case TR::Snippet::IsReadMonitor:
            case TR::Snippet::IsLockReservationEnter:
            case TR::Snippet::IsLockReservationExit:
            case TR::Snippet::IsArrayCopyCall:
               callSym = ((TR::PPCHelperCallSnippet *)snippet)->getDestination();
               break;
            }
         if (callSym)
            trfprintf(pOutFile, "\t; Call \"%s\"", getName(callSym));
         }
      }
   printInstructionComment(pOutFile, 1, instr);
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCDepLabelInstruction * instr)
   {
   print(pOutFile, (TR::PPCLabelInstruction *) instr);
   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCConditionalBranchInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getConditionRegister(), TR_WordReg);
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getLabelSymbol());
   if ( instr -> haveHint() )
      trfprintf(pOutFile, " # with prediction hints: %s", instr->getLikeliness() ? "Likely" : "Unlikely" );
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCDepConditionalBranchInstruction * instr)
   {
   print(pOutFile, (TR::PPCConditionalBranchInstruction *)instr);
   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCAdminInstruction * instr)
   {
   int i;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s ", getOpCodeName(&instr->getOpCode()));
   if (instr->isDebugFence())
      trfprintf(pOutFile, "DBG");
   if (instr->getOpCodeValue()==TR::InstOpCode::fence && instr->getFenceNode()!=NULL)
      {
      trfprintf(pOutFile, "\t%s[", (instr->getFenceNode()->getRelocationType()==TR_AbsoluteAddress)?"Absolute":"Relative");
      for (i=0; i<instr->getFenceNode()->getNumRelocations(); i++)
         {
         trfprintf(pOutFile, " %08x", instr->getFenceNode()->getRelocationDestination(i));
         }
      trfprintf(pOutFile, " ]");
      }

   TR::Node *node = instr->getNode();
   if (node && node->getOpCodeValue() == TR::BBStart)
      {
      TR::Block *block = node->getBlock();
      trfprintf(pOutFile, " (BBStart (block_%d))", block->getNumber());
      }
   else if (node && node->getOpCodeValue() == TR::BBEnd)
      {
      trfprintf(pOutFile, " (BBEnd (block_%d))", node->getBlock()->getNumber());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCImmInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t0x%08x", getOpCodeName(&instr->getOpCode()), instr->getSourceImmediate());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCSrc1Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   if (!(instr->getOpCodeValue() == TR::InstOpCode::mtlr || instr->getOpCodeValue() == TR::InstOpCode::mtctr))
      trfprintf(pOutFile, ", " POINTER_PRINTF_FORMAT, (intptr_t)(int32_t)instr->getSourceImmediate());

   trfflush(_comp->getOutFile());
   }

// Returns true if the given call symbol reference refers to a method address
// that is outside of the range of legal branch offsets. Sets distance to the
// branch distance (either to the method directly or to the trampoline) as a
// side effect.
bool
TR_Debug::isBranchToTrampoline(TR::SymbolReference *symRef, uint8_t *cursor, int32_t &distance)
   {
   intptr_t methodAddress = (intptr_t)(symRef->getMethodAddress());
   bool requiresTrampoline = false;

   if (_cg->directCallRequiresTrampoline(methodAddress, (intptr_t)cursor))
      {
      methodAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(symRef->getReferenceNumber(), (void *)cursor);
      requiresTrampoline = true;
      }

   distance = (int32_t)(methodAddress - (intptr_t)cursor);
   return requiresTrampoline;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCDepImmSymInstruction * instr)
   {
   intptr_t targetAddress = instr->getAddrImmediate();
   uint8_t *cursor = instr->getBinaryEncoding();
   TR::Symbol *target = instr->getSymbolReference()->getSymbol();
   TR::LabelSymbol *label = target->getLabelSymbol();

   printPrefix(pOutFile, instr);

   if (cursor)
      {
      if (label)
         {
         targetAddress = (intptr_t)label->getCodeLocation();
         }
      else if (targetAddress == 0)
         {
         uint8_t *jitTojitStart = _cg->getCodeStart();

         jitTojitStart += ((*(int32_t *)(jitTojitStart - 4)) >> 16) & 0x0000ffff;
         targetAddress = (intptr_t)jitTojitStart;
         }
      else if (_cg->directCallRequiresTrampoline(targetAddress, (intptr_t)cursor))
         {
         int32_t refNum = instr->getSymbolReference()->getReferenceNumber();
         if (refNum < TR_PPCnumRuntimeHelpers)
            {
            targetAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(refNum, (void *)cursor);
            }
         else
            {
            targetAddress = _comp->fe()->methodTrampolineLookup(_comp, instr->getSymbolReference(), (void *)cursor);
            }
         }
      }

   intptr_t distance = targetAddress - (intptr_t)cursor;

   const char *name = target ? getName(instr->getSymbolReference()) : 0;
   if (name)
      trfprintf(pOutFile, "%s \t" POINTER_PRINTF_FORMAT "\t\t; Direct Call \"%s\"", getOpCodeName(&instr->getOpCode()), (intptr_t)cursor + distance, name);
   else
      trfprintf(pOutFile, "%s \t" POINTER_PRINTF_FORMAT, getOpCodeName(&instr->getOpCode()), (intptr_t)cursor + distance);

   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1Src1Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));

   // Use the VSR name instead of the FPR/VR
   bool isVSX = instr->getOpCode().isVSX();
   if (instr->getTargetRegister()->getRealRegister())
      toRealRegister(instr->getTargetRegister())->setUseVSR(isVSX);
   if (instr->getSource1Register()->getRealRegister())
      toRealRegister(instr->getSource1Register())->setUseVSR(isVSX);

   print(pOutFile, instr->getTargetRegister(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   if (strcmp("ori", getOpCodeName(&instr->getOpCode())) == 0)
      trfprintf(pOutFile, ", 0x0");
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1ImmInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));

   print(pOutFile, instr->getTargetRegister(), TR_WordReg);
   trfprintf(pOutFile, ", " POINTER_PRINTF_FORMAT, (intptr_t)(int32_t)instr->getSourceImmediate());

   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1Src1ImmInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg);
   TR::InstOpCode::Mnemonic op =  instr->getOpCodeValue();

   trfprintf(pOutFile, ", " "%d", (intptr_t)(int32_t)instr->getSourceImmediate());

   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());

   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1Src1Imm2Instruction * instr)
   {
   uint64_t lmask;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg);

   lmask = instr->getLongMask();
   if (instr->cg()->comp()->target().is64Bit())
      trfprintf(pOutFile, ", " POINTER_PRINTF_FORMAT ", " POINTER_PRINTF_FORMAT, instr->getSourceImmediate(), lmask);
   else
      trfprintf(pOutFile, ", " POINTER_PRINTF_FORMAT ", 0x%x", instr->getSourceImmediate(), (uint32_t)lmask);

   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCSrc2Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getSource1Register(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource2Register(), TR_WordReg);
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1Src2Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));

   // Use the VSR name instead of the FPR/VR
   bool isVSX = instr->getOpCode().isVSX();
   if (instr->getTargetRegister()->getRealRegister())
      toRealRegister(instr->getTargetRegister())->setUseVSR(isVSX);
   if (instr->getSource1Register()->getRealRegister())
      toRealRegister(instr->getSource1Register())->setUseVSR(isVSX);
   if (instr->getSource2Register()->getRealRegister())
      toRealRegister(instr->getSource2Register())->setUseVSR(isVSX);

   print(pOutFile, instr->getTargetRegister(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource2Register(), TR_WordReg);
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1Src2ImmInstruction * instr)
   {
   uint64_t lmask;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource2Register(), TR_WordReg);
   lmask = instr->getLongMask();
   if (instr->cg()->comp()->target().is64Bit())
      trfprintf(pOutFile, ", " POINTER_PRINTF_FORMAT, lmask);
   else
      trfprintf(pOutFile, ", 0x%x", (uint32_t)lmask);
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1Src3Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getTargetRegister(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource1Register(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource2Register(), TR_WordReg); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSource3Register(), TR_WordReg);
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCMemSrc1Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));

   // Use the VSR name instead of the FPR/VR
   if (instr->getSourceRegister()->getRealRegister())
      toRealRegister(instr->getSourceRegister())->setUseVSR(instr->getOpCode().isVSX());

   print(pOutFile, instr->getMemoryReference()); trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSourceRegister(), TR_WordReg);

   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCMemInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getMemoryReference());
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCTrg1MemInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));

   // Use the VSR name instead of the FPR/VR
   if (instr->getTargetRegister()->getRealRegister())
      toRealRegister(instr->getTargetRegister())->setUseVSR(instr->getOpCode().isVSX());

   print(pOutFile, instr->getTargetRegister(), TR_WordReg); trfprintf(pOutFile, ", ");

   print(pOutFile, instr->getMemoryReference(), strncmp("addi", getOpCodeName(&instr->getOpCode()), 4) != 0);
   TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference()->getSymbol();
   if (symbol && symbol->isSpillTempAuto())
      {
      trfprintf(pOutFile, "\t\t; spilled for %s", getName(instr->getNode()->getOpCode()));
      }
   if (instr->getSnippetForGC() != NULL)
      {
      trfprintf(pOutFile, "\t\t; Backpatched branch to Unresolved Data %s", getName(instr->getSnippetForGC()->getSnippetLabel()));
      }

   if ( instr->haveHint() )
      {
      int32_t hint = instr->getHint();
      if (hint == PPCOpProp_LoadReserveAtomicUpdate)
         trfprintf(pOutFile, " # with hint: Reserve Atomic Update (Default)");
      if (hint == PPCOpProp_LoadReserveExclusiveAccess)
         trfprintf(pOutFile, " # with hint: Exclusive Access");
      }

   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCControlFlowInstruction * instr)
   {
   int i;
   printPrefix(pOutFile, instr);

   trfprintf(pOutFile, "%s \t", getOpCodeName(&instr->getOpCode()));
   int numTargets = instr->getNumTargets();
   int numSources = instr->getNumSources();
   for (i = 0; i < numTargets; i++)
      {
      print(pOutFile, instr->getTargetRegister(i), TR_WordReg);
      if (i != numTargets + numSources - 1)
         trfprintf(pOutFile, ", ");
      }
   for (i = 0; i < numSources; i++)
      {
      if (instr->isSourceImmediate(i))
         trfprintf(pOutFile, "0x%08x", instr->getSourceImmediate(i));
      else
         print(pOutFile, instr->getSourceRegister(i), TR_WordReg);
      if (i != numSources - 1)
         trfprintf(pOutFile, ", ");
      }
   if (instr->getOpCode2Value() != TR::InstOpCode::bad)
      {
      trfprintf(pOutFile, ", %s", getOpCodeName(&instr->getOpCode2()));
      }
   if (instr->getLabelSymbol() != NULL)
      {
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getLabelSymbol());
      }
   trfflush(_comp->getOutFile());
   }

#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCVirtualGuardNOPInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s Site:" POINTER_PRINTF_FORMAT ", ", getOpCodeName(&instr->getOpCode()), instr->getSite());
   print(pOutFile, instr->getLabelSymbol());
   if (instr->getDependencyConditions())
      print(pOutFile, instr->getDependencyConditions());
   trfflush(_comp->getOutFile());
   }
#endif

void
TR_Debug::print(TR::FILE *pOutFile, TR::RegisterDependency * dep)
   {
   TR::Machine *machine = _cg->machine();
   trfprintf(pOutFile,"[");
   print(pOutFile, dep->getRegister(), TR_WordReg);
   trfprintf(pOutFile," : ");
   trfprintf(pOutFile,"%s] ",getPPCRegisterName(dep->getRealRegister()));
   trfflush(_comp->getOutFile());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RegisterDependencyConditions * conditions)
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
TR_Debug::print(TR::FILE *pOutFile, TR::MemoryReference * mr, bool d_form)
   {
   trfprintf(pOutFile, "[");

   if (mr->getBaseRegister() != NULL)
      {
      print(pOutFile, mr->getBaseRegister());
      trfprintf(pOutFile, ", ");
      }

   if (mr->getIndexRegister() != NULL)
      print(pOutFile, mr->getIndexRegister());
   else
      trfprintf(pOutFile, "%d", mr->getOffset(*_comp));

   trfprintf(pOutFile, "]");
   }

void
TR_Debug::printPPCGCRegisterMap(TR::FILE *pOutFile, TR::GCRegisterMap * map)
   {
   TR::Machine *machine = _cg->machine();

   trfprintf(pOutFile,"    registers: {");
   for (int i = 31; i>=0; i--)
      {
      if (map->getMap() & (1 << i))
         trfprintf(pOutFile, "%s ", getName(machine->getRealRegister((TR::RealRegister::RegNum)(31 - i + TR::RealRegister::FirstGPR))));
      }

   trfprintf(pOutFile,"}\n");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RealRegister * reg, TR_RegisterSizes size)
   {
   trfprintf(pOutFile, "%s", getName(reg, size));
   }

static const char *
getRegisterName(TR::RealRegister::RegNum num, bool isVSR = false)
   {
   switch (num)
      {
      case TR::RealRegister::gr0: return "gr0";
      case TR::RealRegister::gr1: return "gr1";
      case TR::RealRegister::gr2: return "gr2";
      case TR::RealRegister::gr3: return "gr3";
      case TR::RealRegister::gr4: return "gr4";
      case TR::RealRegister::gr5: return "gr5";
      case TR::RealRegister::gr6: return "gr6";
      case TR::RealRegister::gr7: return "gr7";
      case TR::RealRegister::gr8: return "gr8";
      case TR::RealRegister::gr9: return "gr9";
      case TR::RealRegister::gr10: return "gr10";
      case TR::RealRegister::gr11: return "gr11";
      case TR::RealRegister::gr12: return "gr12";
      case TR::RealRegister::gr13: return "gr13";
      case TR::RealRegister::gr14: return "gr14";
      case TR::RealRegister::gr15: return "gr15";
      case TR::RealRegister::gr16: return "gr16";
      case TR::RealRegister::gr17: return "gr17";
      case TR::RealRegister::gr18: return "gr18";
      case TR::RealRegister::gr19: return "gr19";
      case TR::RealRegister::gr20: return "gr20";
      case TR::RealRegister::gr21: return "gr21";
      case TR::RealRegister::gr22: return "gr22";
      case TR::RealRegister::gr23: return "gr23";
      case TR::RealRegister::gr24: return "gr24";
      case TR::RealRegister::gr25: return "gr25";
      case TR::RealRegister::gr26: return "gr26";
      case TR::RealRegister::gr27: return "gr27";
      case TR::RealRegister::gr28: return "gr28";
      case TR::RealRegister::gr29: return "gr29";
      case TR::RealRegister::gr30: return "gr30";
      case TR::RealRegister::gr31: return "gr31";

      case TR::RealRegister::fp0: return (isVSR? "vsr0":"fp0");
      case TR::RealRegister::fp1: return (isVSR? "vsr1":"fp1");
      case TR::RealRegister::fp2: return (isVSR? "vsr2":"fp2");
      case TR::RealRegister::fp3: return (isVSR? "vsr3":"fp3");
      case TR::RealRegister::fp4: return (isVSR? "vsr4":"fp4");
      case TR::RealRegister::fp5: return (isVSR? "vsr5":"fp5");
      case TR::RealRegister::fp6: return (isVSR? "vsr6":"fp6");
      case TR::RealRegister::fp7: return (isVSR? "vsr7":"fp7");
      case TR::RealRegister::fp8: return (isVSR? "vsr8":"fp8");
      case TR::RealRegister::fp9: return (isVSR? "vsr9":"fp9");
      case TR::RealRegister::fp10: return (isVSR? "vsr10":"fp10");
      case TR::RealRegister::fp11: return (isVSR? "vsr11":"fp11");
      case TR::RealRegister::fp12: return (isVSR? "vsr12":"fp12");
      case TR::RealRegister::fp13: return (isVSR? "vsr13":"fp13");
      case TR::RealRegister::fp14: return (isVSR? "vsr14":"fp14");
      case TR::RealRegister::fp15: return (isVSR? "vsr15":"fp15");
      case TR::RealRegister::fp16: return (isVSR? "vsr16":"fp16");
      case TR::RealRegister::fp17: return (isVSR? "vsr17":"fp17");
      case TR::RealRegister::fp18: return (isVSR? "vsr18":"fp18");
      case TR::RealRegister::fp19: return (isVSR? "vsr19":"fp19");
      case TR::RealRegister::fp20: return (isVSR? "vsr20":"fp20");
      case TR::RealRegister::fp21: return (isVSR? "vsr21":"fp21");
      case TR::RealRegister::fp22: return (isVSR? "vsr22":"fp22");
      case TR::RealRegister::fp23: return (isVSR? "vsr23":"fp23");
      case TR::RealRegister::fp24: return (isVSR? "vsr24":"fp24");
      case TR::RealRegister::fp25: return (isVSR? "vsr25":"fp25");
      case TR::RealRegister::fp26: return (isVSR? "vsr26":"fp26");
      case TR::RealRegister::fp27: return (isVSR? "vsr27":"fp27");
      case TR::RealRegister::fp28: return (isVSR? "vsr28":"fp28");
      case TR::RealRegister::fp29: return (isVSR? "vsr29":"fp29");
      case TR::RealRegister::fp30: return (isVSR? "vsr30":"fp30");
      case TR::RealRegister::fp31: return (isVSR? "vsr31":"fp31");

      case TR::RealRegister::cr0: return "cr0";
      case TR::RealRegister::cr1: return "cr1";
      case TR::RealRegister::cr2: return "cr2";
      case TR::RealRegister::cr3: return "cr3";
      case TR::RealRegister::cr4: return "cr4";
      case TR::RealRegister::cr5: return "cr5";
      case TR::RealRegister::cr6: return "cr6";
      case TR::RealRegister::cr7: return "cr7";

      case TR::RealRegister::lr: return "lr";

      case TR::RealRegister::vr0: return (isVSR? "vsr32":"vr0");
      case TR::RealRegister::vr1: return (isVSR? "vsr33":"vr1");
      case TR::RealRegister::vr2: return (isVSR? "vsr34":"vr2");
      case TR::RealRegister::vr3: return (isVSR? "vsr35":"vr3");
      case TR::RealRegister::vr4: return (isVSR? "vsr36":"vr4");
      case TR::RealRegister::vr5: return (isVSR? "vsr37":"vr5");
      case TR::RealRegister::vr6: return (isVSR? "vsr38":"vr6");
      case TR::RealRegister::vr7: return (isVSR? "vsr39":"vr7");
      case TR::RealRegister::vr8: return (isVSR? "vsr40":"vr8");
      case TR::RealRegister::vr9: return (isVSR? "vsr41":"vr9");
      case TR::RealRegister::vr10: return (isVSR? "vsr42":"vr10");
      case TR::RealRegister::vr11: return (isVSR? "vsr43":"vr11");
      case TR::RealRegister::vr12: return (isVSR? "vsr44":"vr12");
      case TR::RealRegister::vr13: return (isVSR? "vsr45":"vr13");
      case TR::RealRegister::vr14: return (isVSR? "vsr46":"vr14");
      case TR::RealRegister::vr15: return (isVSR? "vsr47":"vr15");
      case TR::RealRegister::vr16: return (isVSR? "vsr48":"vr16");
      case TR::RealRegister::vr17: return (isVSR? "vsr49":"vr17");
      case TR::RealRegister::vr18: return (isVSR? "vsr50":"vr18");
      case TR::RealRegister::vr19: return (isVSR? "vsr51":"vr19");
      case TR::RealRegister::vr20: return (isVSR? "vsr52":"vr20");
      case TR::RealRegister::vr21: return (isVSR? "vsr53":"vr21");
      case TR::RealRegister::vr22: return (isVSR? "vsr54":"vr22");
      case TR::RealRegister::vr23: return (isVSR? "vsr55":"vr23");
      case TR::RealRegister::vr24: return (isVSR? "vsr56":"vr24");
      case TR::RealRegister::vr25: return (isVSR? "vsr57":"vr25");
      case TR::RealRegister::vr26: return (isVSR? "vsr58":"vr26");
      case TR::RealRegister::vr27: return (isVSR? "vsr59":"vr27");
      case TR::RealRegister::vr28: return (isVSR? "vsr60":"vr28");
      case TR::RealRegister::vr29: return (isVSR? "vsr61":"vr29");
      case TR::RealRegister::vr30: return (isVSR? "vsr62":"vr30");
      case TR::RealRegister::vr31: return (isVSR? "vsr63":"vr31");

      default:                       return "???";
      }
   }

const char *
TR_Debug::getName(TR::RealRegister * reg, TR_RegisterSizes size)
   {
   return getRegisterName(reg->getRegisterNumber(), reg->getUseVSR());
   }

const char *
TR_Debug::getPPCRegisterName(uint32_t regNum, bool useVSR)
   {
   return getRegisterName((TR::RealRegister::RegNum) regNum, useVSR);
   }

TR::Instruction* TR_Debug::getOutlinedTargetIfAny(TR::Instruction *instr)
   {
   switch (instr->getKind())
      {
      case TR::Instruction::IsLabel:
      case TR::Instruction::IsDepLabel:
      case TR::Instruction::IsVirtualGuardNOP:
      case TR::Instruction::IsConditionalBranch:
      case TR::Instruction::IsDepConditionalBranch:
         break;
      default:
         return NULL;
      }

   TR::PPCLabelInstruction *labelInstr = (TR::PPCLabelInstruction *)instr;
   TR::LabelSymbol          *label = labelInstr->getLabelSymbol();

   if (!label || !label->isStartOfColdInstructionStream())
      return NULL;

   return label->getInstruction();
   }

void TR_Debug::printPPCOOLSequences(TR::FILE *pOutFile)
   {
   auto oiIterator = _cg->getPPCOutOfLineCodeSectionList().begin();
   while (oiIterator != _cg->getPPCOutOfLineCodeSectionList().end())
      {
      trfprintf(pOutFile, "\n------------ start out-of-line instructions\n");
      TR::Instruction *instr = (*oiIterator)->getFirstInstruction();

      do {
         print(pOutFile, instr);
         instr = instr->getNext();
      } while (instr != (*oiIterator)->getAppendInstruction());

      if ((*oiIterator)->getAppendInstruction())
         {
         print(pOutFile, (*oiIterator)->getAppendInstruction());
         }
      trfprintf(pOutFile, "\n------------ end out-of-line instructions\n");

      ++oiIterator;
      }
   }

void
TR_Debug::printp(TR::FILE *pOutFile, TR::Snippet * snippet)
   {
   if (pOutFile == NULL)
      return;
   switch (snippet->getKind())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::Snippet::IsCall:
         print(pOutFile, (TR::PPCCallSnippet *)snippet);
         break;
      case TR::Snippet::IsUnresolvedCall:
         print(pOutFile, (TR::PPCUnresolvedCallSnippet *)snippet);
         break;
      case TR::Snippet::IsVirtual:
         print(pOutFile, (TR::PPCVirtualSnippet *)snippet);
         break;
      case TR::Snippet::IsVirtualUnresolved:
         print(pOutFile, (TR::PPCVirtualUnresolvedSnippet *)snippet);
         break;
      case TR::Snippet::IsInterfaceCall:
         print(pOutFile, (TR::PPCInterfaceCallSnippet *)snippet);
         break;
      case TR::Snippet::IsInterfaceCastSnippet:
         print(pOutFile, (TR::PPCInterfaceCastSnippet *)snippet);
         break;
      case TR::Snippet::IsStackCheckFailure:
         print(pOutFile, (TR::PPCStackCheckFailureSnippet *)snippet);
         break;
      case TR::Snippet::IsForceRecompilation:
         print(pOutFile, (TR::PPCForceRecompilationSnippet *)snippet);
         break;
      case TR::Snippet::IsRecompilation:
         print(pOutFile, (TR::PPCRecompilationSnippet *)snippet);
         break;
#endif
      case TR::Snippet::IsArrayCopyCall:
      case TR::Snippet::IsHelperCall:
         print(pOutFile, (TR::PPCHelperCallSnippet *)snippet);
         break;
      case TR::Snippet::IsUnresolvedData:
         print(pOutFile, (TR::UnresolvedDataSnippet *)snippet);
         break;


      case TR::Snippet::IsHeapAlloc:
      case TR::Snippet::IsMonitorExit:
      case TR::Snippet::IsMonitorEnter:
      case TR::Snippet::IsReadMonitor:
      case TR::Snippet::IsLockReservationEnter:
      case TR::Snippet::IsLockReservationExit:
      case TR::Snippet::IsAllocPrefetch:
      case TR::Snippet::IsNonZeroAllocPrefetch:
         snippet->print(pOutFile, this);
         break;
      default:
         TR_ASSERT( 0, "unexpected snippet kind");
      }
   }

#endif
