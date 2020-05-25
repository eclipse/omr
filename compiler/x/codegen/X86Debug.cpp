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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCRegisterMap.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "x/codegen/DataSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/X86Register.hpp"
#include "env/CompilerEnv.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "x/codegen/CallSnippet.hpp"
#endif

namespace TR { class Register; }

void
TR_Debug::printx(TR::FILE *pOutFile, TR::Instruction  * instr)
   {
   //print all the X86 instructions here.
   if (pOutFile == NULL)
      return;
   switch (instr->getKind())
      {
      case TR::Instruction::IsLabel:
         print(pOutFile, (TR::X86LabelInstruction  *)instr);
         break;
      case TR::Instruction::IsPadding:
         print(pOutFile, (TR::X86PaddingInstruction  *)instr);
         break;
      case TR::Instruction::IsAlignment:
         print(pOutFile, (TR::X86AlignmentInstruction  *)instr);
         break;
      case TR::Instruction::IsBoundaryAvoidance:
         print(pOutFile, (TR::X86BoundaryAvoidanceInstruction  *)instr);
         break;
      case TR::Instruction::IsPatchableCodeAlignment:
         print(pOutFile, (TR::X86PatchableCodeAlignmentInstruction  *)instr);
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::Instruction::IsVirtualGuardNOP:
         print(pOutFile, (TR::X86VirtualGuardNOPInstruction  *)instr);
         break;
#endif
      case TR::Instruction::IsFence:
         print(pOutFile, (TR::X86FenceInstruction  *)instr);
         break;
      case TR::Instruction::IsImm:
         print(pOutFile, (TR::X86ImmInstruction  *)instr);
         break;
      case TR::Instruction::IsImm64:
         print(pOutFile, (TR::AMD64Imm64Instruction *)instr);
         break;
      case TR::Instruction::IsImm64Sym:
         print(pOutFile, (TR::AMD64Imm64SymInstruction *)instr);
      case TR::Instruction::IsImmSnippet:
         print(pOutFile, (TR::X86ImmSnippetInstruction  *)instr);
         break;
      case TR::Instruction::IsImmSym:
         print(pOutFile, (TR::X86ImmSymInstruction  *)instr);
         break;
      case TR::Instruction::IsReg:
         print(pOutFile, (TR::X86RegInstruction  *)instr);
         break;
      case TR::Instruction::IsRegReg:
         print(pOutFile, (TR::X86RegRegInstruction  *)instr);
         break;
      case TR::Instruction::IsRegRegReg:
         print(pOutFile, (TR::X86RegRegRegInstruction  *)instr);
         break;
      case TR::Instruction::IsRegRegImm:
         print(pOutFile, (TR::X86RegRegImmInstruction  *)instr);
         break;
      case TR::Instruction::IsFPRegReg:
      case TR::Instruction::IsFPST0ST1RegReg:
      case TR::Instruction::IsFPST0STiRegReg:
      case TR::Instruction::IsFPSTiST0RegReg:
      case TR::Instruction::IsFPArithmeticRegReg:
      case TR::Instruction::IsFPCompareRegReg:
      case TR::Instruction::IsFPRemainderRegReg:
         print(pOutFile, (TR::X86FPRegRegInstruction  *)instr);
         break;
#ifdef TR_TARGET_64BIT
      case TR::Instruction::IsRegImm64:
      case TR::Instruction::IsRegImm64Sym:
         print(pOutFile, (TR::AMD64RegImm64Instruction *)instr);
         break;
#endif
      case TR::Instruction::IsRegImm:
      case TR::Instruction::IsRegImmSym:
         print(pOutFile, (TR::X86RegImmInstruction  *)instr);
         break;
      case TR::Instruction::IsRegMem:
         print(pOutFile, (TR::X86RegMemInstruction  *)instr);
         break;
      case TR::Instruction::IsRegMemImm:
         print(pOutFile, (TR::X86RegMemImmInstruction  *)instr);
         break;
      case TR::Instruction::IsRegRegMem:
         print(pOutFile, (TR::X86RegRegMemInstruction  *)instr);
         break;
      case TR::Instruction::IsFPRegMem:
         print(pOutFile, (TR::X86FPRegMemInstruction  *)instr);
         break;
      case TR::Instruction::IsFPReg:
         print(pOutFile, (TR::X86FPRegInstruction  *)instr);
         break;
      case TR::Instruction::IsMem:
      case TR::Instruction::IsMemTable:
      case TR::Instruction::IsCallMem:
         print(pOutFile, (TR::X86MemInstruction  *)instr);
         break;
      case TR::Instruction::IsMemImm:
      case TR::Instruction::IsMemImmSym:
      case TR::Instruction::IsMemImmSnippet:
         print(pOutFile, (TR::X86MemImmInstruction  *)instr);
         break;
      case TR::Instruction::IsMemReg:
         print(pOutFile, (TR::X86MemRegInstruction  *)instr);
         break;
      case TR::Instruction::IsMemRegImm:
         print(pOutFile, (TR::X86MemRegImmInstruction  *)instr);
         break;
      case TR::Instruction::IsFPMemReg:
         print(pOutFile, (TR::X86FPMemRegInstruction  *)instr);
         break;
      case TR::Instruction::IsVFPSave:
         print(pOutFile, (TR::X86VFPSaveInstruction  *)instr);
         break;
      case TR::Instruction::IsVFPRestore:
         print(pOutFile, (TR::X86VFPRestoreInstruction  *)instr);
         break;
      case TR::Instruction::IsVFPDedicate:
         print(pOutFile, (TR::X86VFPDedicateInstruction  *)instr);
         break;
      case TR::Instruction::IsVFPRelease:
         print(pOutFile, (TR::X86VFPReleaseInstruction  *)instr);
         break;
      case TR::Instruction::IsVFPCallCleanup:
         print(pOutFile, (TR::X86VFPCallCleanupInstruction  *)instr);
         break;
      default:
         TR_ASSERT( 0, "Unknown instruction kind");
         // fall thru
      case TR::Instruction::IsFPCompareEval:
      case TR::Instruction::IsNotExtended:
         {
         printPrefix(pOutFile, instr);
         trfprintf(pOutFile, "%-32s", getMnemonicName(&instr->getOpCode()));
         printInstructionComment(pOutFile, 0, instr);
         dumpDependencies(pOutFile, instr);
         trfflush(pOutFile);
         }
         break;
      }
   }



void
TR_Debug::printPrefix(TR::FILE *pOutFile, TR::Instruction  *instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr, instr->getBinaryEncoding(), instr->getBinaryLength());
   }

void
TR_Debug::printDependencyConditions(
   TR_X86RegisterDependencyGroup *conditions,
   uint8_t                         numConditions,
   char                           *prefix,
   TR::FILE *pOutFile)
   {
   char buf[32];
   char *cursor;
   int len,i;

   if (pOutFile == NULL)
      return;

   for (i=0; i<numConditions; i++)
      {
      cursor = buf;
      memset(buf, ' ', 23);
      len = sprintf(cursor, "    %s[%d]", prefix, i);
      *(cursor+len) = ' ';
      cursor += 12;

      *(cursor++) = '(';
      TR::RealRegister::RegNum r = (conditions->getRegisterDependency(i))->getRealRegister();
      if  (r == TR::RealRegister::AllFPRegisters)
         {
         len = sprintf(cursor, "AllFP");
         }
      else if (r == TR::RealRegister::NoReg)
         {
         len = sprintf(cursor, "NoReg");
         }
      else if (r == TR::RealRegister::ByteReg)
         {
         len = sprintf(cursor, "ByteReg");
         }
      else if (r == TR::RealRegister::BestFreeReg)
         {
         len = sprintf(cursor, "BestFreeReg");
         }
      else if (r == TR::RealRegister::SpilledReg)
         {
         len = sprintf(cursor, "SpilledReg");
         }
      else
         {
         len = sprintf(cursor, "%s", getName(_cg->machine()->getRealRegister(r)));
        }

      *(cursor+len)=')';
      *(cursor+9) = 0x00;

      trfprintf(pOutFile, "%s", buf);

      if (conditions->getRegisterDependency(i)->getRegister())
         {
         printFullRegInfo(pOutFile, conditions->getRegisterDependency(i)->getRegister());
         }
      else
         {
         trfprintf(pOutFile, "[ None        ]\n");
         }
      }
   }

void
TR_Debug::printFullRegisterDependencyInfo(TR::FILE *pOutFile, TR::RegisterDependencyConditions  * conditions)
   {
   if (pOutFile == NULL)
      return;

   if (conditions->getNumPreConditions() > 0)
      {
      printDependencyConditions(conditions->getPreConditions(), conditions->getNumPreConditions(), "Pre", pOutFile);
      }

   if (conditions->getNumPostConditions() > 0)
      {
      printDependencyConditions(conditions->getPostConditions(), conditions->getNumPostConditions(), "Post", pOutFile);
      }
   }

void
TR_Debug::dumpDependencyGroup(TR::FILE *                         pOutFile,
                              TR_X86RegisterDependencyGroup *group,
                              int32_t                         numConditions,
                              char                           *prefix,
                              bool                            omitNullDependencies)
   {
   TR::RealRegister::RegNum r;
   TR::Register *virtReg;
   int32_t      i;
   bool         foundDep = false;

   trfprintf(pOutFile, "\n\t%s:", prefix);
   for (i = 0; i < numConditions; ++i)
      {
      virtReg = group->getRegisterDependency(i)->getRegister();
      r = (group->getRegisterDependency(i))->getRealRegister();

      if (omitNullDependencies)
        if (!virtReg && r != TR::RealRegister::AllFPRegisters)
           continue;

      if  (r == TR::RealRegister::AllFPRegisters)
        {
        trfprintf(pOutFile, " [All FPRs]");
        }
      else
        {
        trfprintf(pOutFile, " [%s : ", getName(virtReg));
        if (r == TR::RealRegister::NoReg)
           trfprintf(pOutFile, "NoReg]");
        else if (r == TR::RealRegister::ByteReg)
           trfprintf(pOutFile, "ByteReg]");
        else if (r == TR::RealRegister::BestFreeReg)
           trfprintf(pOutFile, "BestFreeReg]");
        else if (r == TR::RealRegister::SpilledReg)
           trfprintf(pOutFile, "SpilledReg]");
        else
           trfprintf(pOutFile, "%s]", getName(_cg->machine()->getRealRegister(r)));
        }

      foundDep = true;
      }

   if (!foundDep)
      trfprintf(pOutFile, " None");
   }

void
TR_Debug::dumpDependencies(TR::FILE *pOutFile, TR::Instruction  * instr)
   {

   // If we are in instruction selection or register assignment and
   // dependency information is requested, dump it.
   //
   if (pOutFile == NULL || // no dump file
       (_cg->getStackAtlas() // not in instruction selection
        && !(_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS && // not in register assignment...
          _comp->getOption(TR_TraceRA)) // or dependencies are not traced
       ))
      return;

   TR::RegisterDependencyConditions  *deps = instr->getDependencyConditions();
   if (!deps)
      return;  // Nothing to dump

   if (deps->getNumPreConditions() > 0)
      dumpDependencyGroup(pOutFile, deps->getPreConditions(), deps->getNumPreConditions(), " PRE", true);

   if (deps->getNumPostConditions() > 0)
      dumpDependencyGroup(pOutFile, deps->getPostConditions(), deps->getNumPostConditions(), "POST", true);

   trfflush(pOutFile);
   }

void
TR_Debug::printRegisterInfoHeader(TR::FILE *pOutFile, TR::Instruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   trfprintf(pOutFile, "\n\n  FP stack height: %d", _cg->machine()->getFPTopOfStack() + 1);
   trfprintf(pOutFile,   "\n  Referenced Regs:        Register         State        Assigned      Total Future Flags\n");
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::Instruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   if (instr->getDependencyConditions())
      {
      printRegisterInfoHeader(pOutFile, instr);
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86PaddingInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   if (instr->getBinaryEncoding())
      trfprintf(pOutFile, "nop (%d byte%s)\t\t%s Padding (%d byte%s)",
         instr->getBinaryLength(), (instr->getBinaryLength() == 1)?"":"s",
         commentString(),
         instr->getLength(),       (instr->getLength()       == 1)?"":"s"
         );
   else
      trfprintf(pOutFile, "nop\t\t\t%s Padding (%d byte%s)",
         commentString(),
         instr->getLength(),       (instr->getLength()       == 1)?"":"s"
         );

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86AlignmentInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   uint8_t length = instr->getBinaryLength();
   uint8_t margin = instr->getMargin();

   printPrefix(pOutFile, instr);
   if (instr->getBinaryEncoding())
      trfprintf(pOutFile, "nop (%d byte%s)\t\t%s ",
                    instr->getBinaryLength(),
                    (instr->getBinaryLength() == 1)?"":"s",
                    commentString());
   else
      trfprintf(pOutFile, "nop\t\t\t%s ", commentString());

   if (margin)
      trfprintf(pOutFile, "Alignment (boundary=%d, margin=%d)", instr->getBoundary(), instr->getMargin());
   else
      trfprintf(pOutFile, "Alignment (boundary=%d)", instr->getBoundary());

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }


void
TR_Debug::printBoundaryAvoidanceInfo(TR::FILE *pOutFile, TR::X86BoundaryAvoidanceInstruction  * instr)
   {
   trfprintf(pOutFile, " @%d", instr->getBoundarySpacing());
   if (instr->getMaxPadding() < instr->getBoundarySpacing()-1)
      trfprintf(pOutFile, " max %d", instr->getMaxPadding());
   if (0) // We rarely care about the target instruction
      {
      if (instr->getTargetCode())
         {
         trfprintf(pOutFile, " [%s]", getName(instr->getTargetCode()));
         }
      else
         {
         trfprintf(pOutFile, " (anything)");
         }
      if (instr->getTargetCode() != instr->getNext())
         {
         trfprintf(pOutFile, " (actually %s)", getName(instr->getNext()));
         }
      }

   trfprintf(pOutFile, " [");

   const char *sep = "";
   for (const TR_AtomicRegion *region = instr->getAtomicRegions(); region->getLength(); region++)
      {
      trfprintf(pOutFile, "%s0x%x:%d", sep, region->getStart(), region->getLength());
      sep = ",";
      }
   trfprintf(pOutFile, "]");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86BoundaryAvoidanceInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   if (instr->getBinaryEncoding())
      trfprintf(pOutFile, "nop (%d byte%s)\t\t%s ",
                    instr->getBinaryLength(), (instr->getBinaryLength() == 1)?"":"s",
                    commentString());
   else
      trfprintf(pOutFile, "nop\t\t\t%s ", commentString());

   trfprintf(pOutFile, "Avoid boundary");
   printBoundaryAvoidanceInfo(pOutFile, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86PatchableCodeAlignmentInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   if (instr->getBinaryEncoding())
      trfprintf(pOutFile, "nop (%d byte%s)\t\t%s ",
                    instr->getBinaryLength(), (instr->getBinaryLength() == 1)?"":"s",
                    commentString());
   else
      trfprintf(pOutFile, "nop\t\t\t%s ", commentString());

   trfprintf(pOutFile, "Align patchable code");
   printBoundaryAvoidanceInfo(pOutFile, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86LabelInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   TR::LabelSymbol *label   = instr->getLabelSymbol();
   TR::Snippet *snippet = label ? label->getSnippet() : NULL;
   if (instr->getOpCodeValue() == LABEL)
      {
      print(pOutFile, label);

      trfprintf(pOutFile, ":");
      printInstructionComment(pOutFile, snippet ? 2 : 3, instr);

      if (label->isStartInternalControlFlow())
         trfprintf(pOutFile, "\t%s (Start of internal control flow)", commentString());
      else if (label->isEndInternalControlFlow())
         trfprintf(pOutFile, "\t%s (End of internal control flow)", commentString());
      }
   else
      {
      trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));

      if (label)
         {
         print(pOutFile, label);
         printInstructionComment(pOutFile, snippet ? 2 : 3, instr);
         }
      else
         {
         trfprintf(pOutFile, "Label L<null>");
         printInstructionComment(pOutFile, 2, instr);
         }

      if (snippet)
         trfprintf(pOutFile, "\t%s (%s)", commentString(), getName(snippet));
      }

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86FenceInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   // Omit fences from post-binary dumps unless they mark basic block boundaries.
   if (instr->getBinaryEncoding() &&
       instr->getNode()->getOpCodeValue() != TR::BBStart &&
       instr->getNode()->getOpCodeValue() != TR::BBEnd)
      return;

   TR::Node *node = instr->getNode();
   if (node && node->getOpCodeValue() == TR::BBStart)
      {
      TR::Block *block = node->getBlock();
      if (block->isExtensionOfPreviousBlock())
         {
         trfprintf(pOutFile, "\n........................................");
         }
      else
         {
         trfprintf(pOutFile, "\n========================================");
         }
      }

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s", getMnemonicName(&instr->getOpCode()));
   if (instr->getFenceNode()->getNumRelocations() > 0)
      {
      if (instr->getFenceNode()->getRelocationType() == TR_AbsoluteAddress)
         trfprintf(pOutFile, " Absolute [");
      else if (instr->getFenceNode()->getRelocationType() == TR_ExternalAbsoluteAddress)
         trfprintf(pOutFile, " External Absolute [");
      else
         trfprintf(pOutFile, " Relative [");

      if (!_comp->getOption(TR_MaskAddresses))
         {
         for (int32_t i = 0; i < instr->getFenceNode()->getNumRelocations(); ++i)
            trfprintf(pOutFile," " POINTER_PRINTF_FORMAT, instr->getFenceNode()->getRelocationDestination(i));
         }
      trfprintf(pOutFile, " ]");
      }

   printInstructionComment(pOutFile, (instr->getFenceNode()->getNumRelocations() > 0) ? 1 : 3, instr);

//   TR::Node *node = instr->getNode();
   printBlockInfo(pOutFile, node);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::print(TR::FILE *pOutFile, TR::X86VirtualGuardNOPInstruction  * instr)
   {
   // *this    swipeable for degubbing purposes
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s Site:" POINTER_PRINTF_FORMAT ", ", getMnemonicName(&instr->getOpCode()), instr->getSite());
   print(pOutFile, instr->getLabelSymbol());
   printInstructionComment(pOutFile, 1, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }
#endif

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86ImmInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));

   if (instr->getOpCode().isCallImmOp() && instr->getNode()->getSymbolReference())
      {
      TR::SymbolReference *symRef  = instr->getNode()->getSymbolReference();
      const char         *symName = getName(symRef);

      trfprintf(pOutFile, "%-24s", symName);
      printInstructionComment(pOutFile, 0, instr);
      if (symRef->isUnresolved())
         trfprintf(pOutFile, " (unresolved method)");
      else
         trfprintf(pOutFile, " (" POINTER_PRINTF_FORMAT ")",  instr->getSourceImmediate()); // TODO:AMD64: Target address gets truncated
      }
   else
      {
      printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
      printInstructionComment(pOutFile, 2, instr);
      }

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::AMD64Imm64Instruction * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));

   if (instr->getOpCode().isCallImmOp() && instr->getNode()->getSymbolReference())
      {
      TR::SymbolReference *symRef  = instr->getNode()->getSymbolReference();
      const char         *symName = getName(symRef);

      trfprintf(pOutFile, "%-24s", symName);
      printInstructionComment(pOutFile, 0, instr);
      if (symRef->isUnresolved())
         trfprintf(pOutFile, " (unresolved method)");
      else
         trfprintf(pOutFile, " (" POINTER_PRINTF_FORMAT ")",  instr->getSourceImmediate());
      }
   else
      {
      printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
      printInstructionComment(pOutFile, 2, instr);
      }

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::AMD64Imm64SymInstruction * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   TR::Symbol       *sym   = instr->getSymbolReference()->getSymbol();
   const char     *name  = getName(instr->getSymbolReference());

   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (sym->getMethodSymbol() && name)
      {
      trfprintf(pOutFile, "%-24s%s %s (" POINTER_PRINTF_FORMAT ")",
                    name,
                    commentString(),
                    getOpCodeName(&instr->getOpCode()),
                    instr->getSourceImmediate());
      }
   else if (sym->getLabelSymbol() && name)
      {
      if (sym->getLabelSymbol()->getSnippet())
         trfprintf(pOutFile, "%-24s%s %s (%s)",
                       name,
                       commentString(),
                       getOpCodeName(&instr->getOpCode()),
                       getName(sym->getLabelSymbol()->getSnippet()));
      else
         trfprintf(pOutFile, "%-24s%s %s (" POINTER_PRINTF_FORMAT ")",
                       name,
                       commentString(),
                       getOpCodeName(&instr->getOpCode()),
                       instr->getSourceImmediate());
      }
   else
      {
      printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
      printInstructionComment(pOutFile, 2, instr);
      }

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::AMD64RegImm64Instruction * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister(), TR_DoubleWordReg);
      trfprintf(pOutFile, ", ");
      }
   printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
   printInstructionComment(pOutFile, 1, instr);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void TR_Debug::print(TR::FILE *pOutFile, TR::X86VFPSaveInstruction  *instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "vfpSave", getMnemonicName(&instr->getOpCode()));

   printInstructionComment(pOutFile, 3, instr);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void TR_Debug::print(TR::FILE *pOutFile, TR::X86VFPRestoreInstruction  *instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "vfpRestore [%s]", getName(instr->getSaveInstruction()));

   printInstructionComment(pOutFile, 3, instr);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void TR_Debug::print(TR::FILE *pOutFile, TR::X86VFPDedicateInstruction  *instr)
   {
   if (pOutFile == NULL)
      return;

   print(pOutFile, (TR::X86RegMemInstruction*)instr);

   trfprintf(pOutFile, "%s vfpDedicate %s",
                 commentString(),
                 getName(instr->getTargetRegister()));
   trfflush(pOutFile);
   }

void TR_Debug::print(TR::FILE *pOutFile, TR::X86VFPReleaseInstruction  *instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "vfpRelease [%s]", getName(instr->getDedicateInstruction()));

   printInstructionComment(pOutFile, 3, instr);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void TR_Debug::print(TR::FILE *pOutFile, TR::X86VFPCallCleanupInstruction  *instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "vfpCallCleanup (%d bytes)", instr->getStackPointerAdjustment());

   printInstructionComment(pOutFile, 3, instr);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86ImmSnippetInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
   printInstructionComment(pOutFile, 2, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86ImmSymInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   TR::Symbol     *sym   = instr->getSymbolReference()->getSymbol();
   const char     *name  = getName(instr->getSymbolReference());

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   intptr_t targetAddress = 0;

   //  64 bit always gets the targetAddress from the symRef
   if(_comp->target().is64Bit())
      {
      // new code patching might have a call to a snippet label, which is not a method
      if(!sym->isLabel())
         {
         targetAddress = (intptr_t)instr->getSymbolReference()->getMethodAddress();
         }
      }
   else
      {
      targetAddress = instr->getSourceImmediate();
      }

   if (name)
      {
      trfprintf(pOutFile, "%-24s", name);
      }
   else
      {
      trfprintf(pOutFile, POINTER_PRINTF_FORMAT, targetAddress);
      }

   if (sym->getMethodSymbol() && name)
      {
      trfprintf(pOutFile, "%s %s (" POINTER_PRINTF_FORMAT ")",
                    commentString(),
                    getOpCodeName(&instr->getOpCode()),
                    targetAddress);
      }
   else if (sym->getLabelSymbol() && name)
     {
     if (sym->getLabelSymbol()->getSnippet())
        trfprintf(pOutFile, "%s %s (%s)",
                      commentString(),
                      getOpCodeName(&instr->getOpCode()),
                      getName(sym->getLabelSymbol()->getSnippet()));
     else
        trfprintf(pOutFile, "%s %s (" POINTER_PRINTF_FORMAT ")",
                      commentString(),
                      getOpCodeName(&instr->getOpCode()),
                      targetAddress);
     }
   else
     {
     trfprintf(pOutFile, " \t\t%s %s",
                   commentString(),
                   getOpCodeName(&instr->getOpCode()));
     }

   printInstructionComment(pOutFile, 0, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
   printInstructionComment(pOutFile, 3, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::X86RegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printRegisterInfoHeader(pOutFile, instr);
   trfprintf(pOutFile,"    Target            ");
   printFullRegInfo(pOutFile, instr->getTargetRegister());

   if (instr->getDependencyConditions())
      {
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0) && !(instr->getOpCode().sourceRegIsImplicit() !=0 ))
      trfprintf(pOutFile, ", ");
   if (!(instr->getOpCode().sourceRegIsImplicit() !=0 ))
      print(pOutFile, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
   printInstructionComment(pOutFile, 2, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::X86RegRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printRegisterInfoHeader(pOutFile, instr);
   trfprintf(pOutFile,"    Target            ");
   printFullRegInfo(pOutFile, instr->getTargetRegister());
   trfprintf(pOutFile,"    Source            ");
   printFullRegInfo(pOutFile, instr->getSourceRegister());

   if (instr->getDependencyConditions())
      {
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegImmInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
   printInstructionComment(pOutFile, 1, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegRegImmInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   if (!(instr->getOpCode().sourceRegIsImplicit() !=0 ))
      {
      print(pOutFile, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
   trfprintf(pOutFile, " \t%s %s",
                 commentString(),
                 getOpCodeName(&instr->getOpCode()));
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegRegRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   TR_RegisterSizes sourceSize = getSourceSizeFromInstruction(instr);
   if (!(instr->getOpCode().sourceRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getSource2ndRegister(), sourceSize);
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getSourceRegister(), sourceSize);
      }

   printInstructionComment(pOutFile, 2, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::X86RegRegRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printRegisterInfoHeader(pOutFile, instr);
   trfprintf(pOutFile,"    Source            ");
   printFullRegInfo(pOutFile, instr->getSourceRegister());
   trfprintf(pOutFile,"    2ndSource         ");
   printFullRegInfo(pOutFile, instr->getSource2ndRegister());
   trfprintf(pOutFile,"    Target            ");
   printFullRegInfo(pOutFile, instr->getTargetRegister());

   if (instr->getDependencyConditions())
      {
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }

   trfflush(pOutFile);
   }


int32_t
TR_Debug::printPrefixAndMnemonicWithoutBarrier(
   TR::FILE *pOutFile,
   TR::Instruction  *instr,
   int32_t             barrier)
   {
   int32_t barrierLength = ::estimateMemoryBarrierBinaryLength(barrier, _comp->cg());
   int32_t nonBarrierLength = instr->getBinaryLength()-barrierLength;

   printPrefix(pOutFile, instr, instr->getBinaryEncoding(), nonBarrierLength);
   trfprintf(pOutFile, "%s%s\t",  (barrier & LockPrefix) ? "lock " : "", getMnemonicName(&instr->getOpCode()));

   return nonBarrierLength;
   }


void
TR_Debug::printPrefixAndMemoryBarrier(
   TR::FILE *pOutFile,
   TR::Instruction  *instr,
   int32_t             barrier,
   int32_t             barrierOffset)
   {
   int32_t barrierLength = ::estimateMemoryBarrierBinaryLength(barrier, _comp->cg());
   uint8_t *barrierStart = instr->getBinaryEncoding() ? (instr->getBinaryEncoding()+barrierOffset) : NULL;

   printPrefix(pOutFile, instr, barrierStart, barrierLength);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86MemInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   print(pOutFile, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
   printInstructionComment(pOutFile, 2, instr);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::X86MemInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printRegisterInfoHeader(pOutFile, instr);
   printReferencedRegisterInfo(pOutFile, instr->getMemoryReference());

   if (instr->getDependencyConditions())
      {
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86MemImmInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   print(pOutFile, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
   trfprintf(pOutFile, ", ");
   printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
   printInstructionComment(pOutFile, 1, instr);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86MemRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   print(pOutFile, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
   if (!(instr->getOpCode().sourceRegIsImplicit() != 0))
      {
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
      }
   printInstructionComment(pOutFile, 2, instr);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::X86MemRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printRegisterInfoHeader(pOutFile, instr);
   trfprintf(pOutFile,"    Source            ");
   printFullRegInfo(pOutFile, instr->getSourceRegister());
   printReferencedRegisterInfo(pOutFile, instr->getMemoryReference());

   if (instr->getDependencyConditions())
      {
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86MemRegImmInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   print(pOutFile, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
   trfprintf(pOutFile, ", ");
   if (!(instr->getOpCode().sourceRegIsImplicit() !=0 ))
      {
      print(pOutFile, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
   printInstructionComment(pOutFile, 1, instr);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegMemInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   print(pOutFile, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
   printInstructionComment(pOutFile, 2, instr);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());
   TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference().getSymbol();
   if (symbol && symbol->isSpillTempAuto())
      {
      trfprintf(pOutFile, "%s, spilled for %s",
                    commentString(),
                    getName(instr->getNode()->getOpCode()));
      }

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::X86RegMemInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printRegisterInfoHeader(pOutFile, instr);
   trfprintf(pOutFile,"    Target            ");
   printFullRegInfo(pOutFile, instr->getTargetRegister());

   printReferencedRegisterInfo(pOutFile, instr->getMemoryReference());

   if (instr->getDependencyConditions())
      {
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegMemImmInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }

   print(pOutFile, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
   trfprintf(pOutFile, ", ");
   printIntConstant(pOutFile, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
   printInstructionComment(pOutFile, 1, instr);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86RegRegMemInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   if (!(instr->getOpCode().sourceRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getSource2ndRegister(), getSourceSizeFromInstruction(instr));
      trfprintf(pOutFile, ", ");
      }
   print(pOutFile, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
   printInstructionComment(pOutFile, 2, instr);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());
   TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference().getSymbol();
   if (symbol && symbol->isSpillTempAuto())
      {
      trfprintf(pOutFile, "%s, spilled for %s",
                    commentString(),
                    getName(instr->getNode()->getOpCode()));
      }

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::X86RegRegMemInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   printReferencedRegisterInfo(pOutFile, instr->getMemoryReference());

   printFullRegInfo(pOutFile, instr->getSourceRegister());
   trfprintf(pOutFile,"    2ndSource         ");
   printFullRegInfo(pOutFile, instr->getSource2ndRegister());
   trfprintf(pOutFile,"    Target            ");
   printFullRegInfo(pOutFile, instr->getTargetRegister());

   if (instr->getDependencyConditions())
      {
      printFullRegisterDependencyInfo(pOutFile, instr->getDependencyConditions());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86FPRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      print(pOutFile, instr->getTargetRegister());
   printInstructionComment(pOutFile, 3, instr);
   printFPRegisterComment(pOutFile, instr->getTargetRegister(), NULL);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86FPRegRegInstruction  * instr)
   {

   if (pOutFile == NULL)
      return;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      print(pOutFile, instr->getTargetRegister());
   if (!(instr->getOpCode().targetRegIsImplicit() != 0) && !(instr->getOpCode().sourceRegIsImplicit()!=0))
      trfprintf(pOutFile, ", ");
   if (!(instr->getOpCode().sourceRegIsImplicit()!=0))
      print(pOutFile, instr->getSourceRegister());
   printInstructionComment(pOutFile, 2, instr);
   printFPRegisterComment(pOutFile, instr->getTargetRegister(), instr->getSourceRegister());
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86FPMemRegInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", getMnemonicName(&instr->getOpCode()));
   print(pOutFile, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
   if (!(instr->getOpCode().sourceRegIsImplicit()!=0))
      {
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getSourceRegister());
      }
   printInstructionComment(pOutFile, 1, instr);
   printFPRegisterComment(pOutFile, NULL, instr->getSourceRegister());
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86FPRegMemInstruction  * instr)
   {
   if (pOutFile == NULL)
      return;

   int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
   int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(pOutFile, instr, barrier);

   if (!(instr->getOpCode().targetRegIsImplicit() != 0))
      {
      print(pOutFile, instr->getTargetRegister());
      trfprintf(pOutFile, ", ");
      }
   print(pOutFile, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
   printInstructionComment(pOutFile, 1, instr);
   printFPRegisterComment(pOutFile, instr->getTargetRegister(), NULL);
   printMemoryReferenceComment(pOutFile, instr->getMemoryReference());

   if (barrier & NeedsExplicitBarrier)
      printPrefixAndMemoryBarrier(pOutFile, instr, barrier, barrierOffset);

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::MemoryReference  * mr, TR_RegisterSizes operandSize)
   {
   if (pOutFile == NULL)
      return;

   const char *typeSpecifier[7] = {
      "byte",     // TR_ByteReg
      "word",     // TR_HalfWordReg
      "dword",    // TR_WordReg
      "qword",    // TR_DoubleWordReg
      "oword",    // TR_QuadWordReg
      "dword",    // TR_FloatReg
      "qword" };  // TR_DoubleReg

   TR_RegisterSizes addressSize = (_comp->target().cpu.isAMD64() ? TR_DoubleWordReg : TR_WordReg);
   bool hasTerm = false;
   bool hasPrecedingTerm = false;
   trfprintf(pOutFile, "%s ptr [", typeSpecifier[operandSize]);
   if (mr->getBaseRegister())
      {
      print(pOutFile, mr->getBaseRegister(), addressSize);
      hasPrecedingTerm = true;
      hasTerm = true;
      }

   if (mr->getIndexRegister())
      {
      if (hasPrecedingTerm)
         trfprintf(pOutFile, "+");
      else
         hasPrecedingTerm = true;
      trfprintf(pOutFile, "%d*", mr->getStrideMultiplier());
      print(pOutFile, mr->getIndexRegister(), addressSize);
      hasTerm = true;
      }

   TR::Symbol *sym = mr->getSymbolReference().getSymbol();

   if (sym != NULL || mr->getSymbolReference().getOffset() != 0)
      {
      intptr_t disp32 = mr->getDisplacement();

      if (!hasPrecedingTerm)
         {
         // Annotate the constant emitted to indicate it is an absolute address,
         // and further annotate to indicate RIP-relative addressability.
         //
#ifdef TR_TARGET_64BIT
         if (mr->getForceRIPRelative())
            {
            trfprintf(pOutFile, "rip $");
            }
         else
#endif
            {
            trfprintf(pOutFile, "$");
            }

         // Treat this as an absolute reference and display in base16.
         //
         printIntConstant(pOutFile, disp32, 16, addressSize, true);
         }
      else
         {
         // Treat this as a relative offset and display in base10, or in base16 if the displacement was
         // explicitly forced wider.
         //
         if ((disp32 != 0) || mr->isForceWideDisplacement())
            {
            if (disp32 > 0)
               {
               trfprintf(pOutFile, "+");
               }
            else
               {
               trfprintf(pOutFile, "-");
               disp32 = -disp32;
               }
            }

         if (mr->isForceWideDisplacement())
            printIntConstant(pOutFile, disp32, 16, TR_WordReg);
         else if (disp32 != 0)
            printIntConstant(pOutFile, disp32, 16);
         }

      hasTerm = true;
      }

   if (!hasTerm)
      {
      // This must be an absolute memory reference (a constant data snippet or a label)
      //
      TR::X86DataSnippet *cds = mr->getDataSnippet();
      TR::LabelSymbol *label = NULL;
      if (cds)
         label = cds->getSnippetLabel();
      else
         label = mr->getLabel();
      TR_ASSERT(label, "expecting a constant data snippet or a label");

      int64_t disp = (int64_t)(label->getCodeLocation());

      if (mr->getLabel())
         {
         print(pOutFile, label);
         if (disp)
            {
            trfprintf(pOutFile, " : ");
            printHexConstant(pOutFile, disp, _comp->target().is64Bit() ? 16 : 8, false);
            }
         }
      else if (disp)
         {
         printHexConstant(pOutFile,
                          _comp->target().is64Bit() ? disp : (uint32_t)disp,
                          _comp->target().is64Bit() ? 16 : 8,
                          true);
         }
      else if (cds)
         {
         trfprintf(pOutFile, "Data ");
         print(pOutFile, cds->getSnippetLabel());
         trfprintf(pOutFile, ": ");
         auto data = cds->getRawData();
         for (auto i = 0; i < cds->getDataSize(); i++)
            {
            trfprintf(pOutFile, "%02x ", 0xff & (unsigned int)(data[i]));
            }
         trfprintf(pOutFile, "| ");
         cds->printValue(pOutFile, this);
         }
      else
         {
         trfprintf(pOutFile, "UNKNOWN DATA");
         }
      }

   trfprintf(pOutFile, "]");
   }

int32_t
TR_Debug::printIntConstant(TR::FILE *pOutFile, int64_t value, int8_t radix, TR_RegisterSizes size, bool padWithZeros)
   {
   if (pOutFile == NULL)
      return 0;

   const int8_t registerSizeToWidth[7] = {
      2,     // TR_ByteReg
      4,     // TR_HalfWordReg
      8,     // TR_WordReg
      16,    // TR_DoubleWordReg
      32,    // TR_QuadWordReg
      8,     // TR_FloatReg
      16 };  // TR_DoubleReg

   int8_t width = registerSizeToWidth[size];

   switch (radix)
      {
      case 10:
         return printDecimalConstant(pOutFile, value, width, padWithZeros);

      case 16:
         return printHexConstant(pOutFile, value, width, padWithZeros);

      default:
         TR_ASSERT(0, "Can't print in unimplemented radix %d\n", radix);
         break;
      }

   return 0;
   }

int32_t
TR_Debug::printDecimalConstant(TR::FILE *pOutFile, int64_t value, int8_t width, bool padWithZeros)
   {
   trfprintf(pOutFile, "%lld", value);
   return 0;
   }

int32_t
TR_Debug::printHexConstant(TR::FILE *pOutFile, int64_t value, int8_t width, bool padWithZeros)
   {
   // we probably need to revisit generateMasmListingSyntax
   const char *prefix = _comp->target().isLinux() ? "0x" : (_cg->generateMasmListingSyntax() ? "0" : "0x");
   const char *suffix = _comp->target().isLinux() ? "" : (_cg->generateMasmListingSyntax() ? "h" : "");

   if (padWithZeros)
      trfprintf(pOutFile, "%s%0*llx%s", prefix, width, value, suffix);
   else
      trfprintf(pOutFile, "%s%llx%s", prefix, value, suffix);

   return 0;
   }

void
TR_Debug::printFPRegisterComment(TR::FILE *pOutFile, TR::Register *source, TR::Register *target)
   {
   trfprintf(pOutFile, " using ");
   if (target)
      print(pOutFile, target);
   if (source && target)
      trfprintf(pOutFile, " & ");
   if (source)
      print(pOutFile, source);
   }

void
TR_Debug::printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction  *instr)
   {
   while (tabStops-- > 0)
      trfprintf(pOutFile, "\t");

   trfprintf(pOutFile, "%s %s",
                 commentString(),
                 getOpCodeName(&instr->getOpCode()));
   dumpInstructionComments(pOutFile, instr);
   }

void
TR_Debug::printMemoryReferenceComment(TR::FILE *pOutFile, TR::MemoryReference  *mr)
   {
   if (pOutFile == NULL)
      return;

   TR::Symbol *symbol = mr->getSymbolReference().getSymbol();

   if (symbol == NULL && mr->getSymbolReference().getOffset() == 0)
      return;

   if (symbol && symbol->isSpillTempAuto())
      {
      const char *prefix = (symbol->getDataType() == TR::Float ||
                            symbol->getDataType() == TR::Double) ? "#FP" : "#";
      trfprintf(pOutFile, ", %sSPILL%d", prefix, symbol->getSize());
      }

   trfprintf(pOutFile, ", SymRef");

   print(pOutFile, &mr->getSymbolReference());
   }

TR_RegisterSizes
TR_Debug::getTargetSizeFromInstruction(TR::Instruction  *instr)
   {
   TR_RegisterSizes targetSize;

   if (instr->getOpCode().hasXMMTarget() != 0)
      targetSize = TR_QuadWordReg;
   else if (instr->getOpCode().hasIntTarget() != 0)
      targetSize = TR_WordReg;
   else if (instr->getOpCode().hasShortTarget() != 0)
      targetSize = TR_HalfWordReg;
   else if (instr->getOpCode().hasByteTarget() != 0)
      targetSize = TR_ByteReg;
   else if ((instr->getOpCode().hasLongTarget() != 0) || (instr->getOpCode().doubleFPOp() != 0))
      targetSize = TR_DoubleWordReg;
   else
      targetSize = TR_WordReg;

   return targetSize;
   }

TR_RegisterSizes
TR_Debug::getSourceSizeFromInstruction(TR::Instruction  *instr)
   {
   TR_RegisterSizes sourceSize;

   if (instr->getOpCode().hasXMMSource() != 0)
      sourceSize = TR_QuadWordReg;
   else if (instr->getOpCode().hasIntSource()!= 0)
      sourceSize = TR_WordReg;
   else if (instr->getOpCode().hasShortSource() != 0)
      sourceSize = TR_HalfWordReg;
   else if ((&instr->getOpCode())->hasByteSource())
      sourceSize = TR_ByteReg;
   else if ((instr->getOpCode().hasLongSource() != 0) || (instr->getOpCode().doubleFPOp() != 0))
      sourceSize = TR_DoubleWordReg;
   else
      sourceSize = TR_WordReg;

   return sourceSize;
   }

TR_RegisterSizes
TR_Debug::getImmediateSizeFromInstruction(TR::Instruction  *instr)
   {
   TR_RegisterSizes immedSize;

   if (instr->getOpCode().hasShortImmediate() != 0)
      immedSize = TR_HalfWordReg;
   else if (instr->getOpCode().hasByteImmediate() != 0)
      immedSize = TR_ByteReg;
   else if (instr->getOpCode().hasLongImmediate() != 0)
      immedSize = TR_DoubleWordReg;
   else
      // IntImmediate or SignExtendImmediate
      //
      immedSize = TR_WordReg;

   return immedSize;
   }

void
TR_Debug::printReferencedRegisterInfo(TR::FILE *pOutFile, TR::MemoryReference  * mr)
   {
   if (pOutFile == NULL)
      return;

   if (mr->getBaseRegister())
      {
      trfprintf(pOutFile,"    Base Reg          ");
      printFullRegInfo(pOutFile, mr->getBaseRegister());
      }

   if (mr->getIndexRegister())
      {
      trfprintf(pOutFile,"    Index Reg         ");
      printFullRegInfo(pOutFile, mr->getIndexRegister());
      }

   trfflush(pOutFile);
   }

void
TR_Debug::printX86GCRegisterMap(TR::FILE *pOutFile, TR::GCRegisterMap * map)
   {
   TR::Machine *machine = _cg->machine();

   trfprintf(pOutFile,"    slot pushes: %d", ((map->getMap() & _cg->getRegisterMapInfoBitsMask()) >> 16));

   trfprintf(pOutFile,"    registers: {");
   for (int i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; ++i)
      {
      if (map->getMap() & (1 << (i-1))) // TODO:AMD64: Use the proper mask value
         trfprintf(pOutFile,"%s ", getName(machine->getRealRegister((TR::RealRegister::RegNum)i)));
      }

   trfprintf(pOutFile,"}\n");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RealRegister * reg, TR_RegisterSizes size)
   {
   if (pOutFile == NULL)
      return;

   switch (size)
      {
      case TR_WordReg:
      case TR_FloatReg:
      case TR_DoubleReg:
         trfprintf(pOutFile, "%s", getName(reg));
         break;
      case TR_QuadWordReg:
      case TR_DoubleWordReg:
      case TR_HalfWordReg:
      case TR_ByteReg:
         trfprintf(pOutFile, "%s", getName(reg, size));
         break;
      default:
         break;
      }
   }

void
TR_Debug::printFullRegInfo(TR::FILE *pOutFile, TR::RealRegister * reg)
   {
   if (pOutFile == NULL)
      return;

   trfprintf(pOutFile, "[ ");

   trfprintf(pOutFile, "%-12s ][ ", getName(reg));

   static const char * stateNames[5] = { "Free", "Unlatched", "Assigned", "Blocked", "Locked" };

   trfprintf(pOutFile, "%-10s ][ ", stateNames[reg->getState()]);

   trfprintf(pOutFile, "%-12s ]\n", reg->getAssignedRegister() ? getName(reg->getAssignedRegister()) : " ");

   trfflush(pOutFile);
   }

static const char *
unknownRegisterName(const char kind=0)
   {
   // This function allows us to put a breakpoint to trap unknown registers
   switch(kind)
      {
      case 'f':
         return "fp?";
      case 's':
         return "st(?)";
      case 'm':
         return "mm?";
      case 'r':
         return "r?";
      case 'x':
         return "xmm?";
      case 'v':
         return "vfp?";
      default:
         return "???";
      }
   }


const char *
TR_Debug::getName(uint32_t realRegisterIndex, TR_RegisterSizes size)
   {
   switch (realRegisterIndex)
      {
      case TR::RealRegister::NoReg:
         return "noReg";
      case TR::RealRegister::ByteReg:
         return "byteReg";
      case TR::RealRegister::BestFreeReg:
         return "bestFreeReg";
      case TR::RealRegister::SpilledReg:
         return "spilledReg";
      case TR::RealRegister::eax:
         switch (size) { case 0: return "al";   case 1:  return "ax";  case 2: case -1: return "eax"; case 3: return "rax"; default: return "?a?"; }
      case TR::RealRegister::ebx:
         switch (size) { case 0: return "bl";   case 1:  return "bx";  case 2: case -1: return "ebx"; case 3: return "rbx"; default: return "?b?"; }
      case TR::RealRegister::ecx:
         switch (size) { case 0: return "cl";   case 1:  return "cx";  case 2: case -1: return "ecx"; case 3: return "rcx"; default: return "?c?"; }
      case TR::RealRegister::edx:
         switch (size) { case 0: return "dl";   case 1:  return "dx";  case 2: case -1: return "edx"; case 3: return "rdx"; default: return "?d?"; }
      case TR::RealRegister::edi:
         switch (size) { case 0: return "dil";  case 1: return "di";   case 2: case -1: return "edi"; case 3: return "rdi"; default: return "?di?"; }
      case TR::RealRegister::esi:
         switch (size) { case 0: return "sil";  case 1: return "si";   case 2: case -1: return "esi"; case 3: return "rsi"; default: return "?si?"; }
      case TR::RealRegister::ebp:
         switch (size) { case 0: return "bpl";  case 1: return "bp";   case 2: case -1: return "ebp"; case 3: return "rbp"; default: return "?bp?"; }
      case TR::RealRegister::esp:
         switch (size) { case 0: return "spl";  case 1: return "sp";   case 2: case -1: return "esp"; case 3: return "rsp"; default: return "?sp?"; }
#ifdef TR_TARGET_64BIT
      case TR::RealRegister::r8:
         switch (size) { case 0: return "r8b";  case 1: return "r8w";  case 2: return "r8d"; case 3: case -1: return "r8"; default: return "r8?"; }
      case TR::RealRegister::r9:
         switch (size) { case 0: return "r9b";  case 1: return "r9w";  case 2: return "r9d"; case 3: case -1: return "r9"; default: return "r9?"; }
      case TR::RealRegister::r10:
         switch (size) { case 0: return "r10b"; case 1: return "r10w"; case 2: return "r10d"; case 3: case -1: return "r10"; default: return "r10?"; }
      case TR::RealRegister::r11:
         switch (size) { case 0: return "r11b"; case 1: return "r11w"; case 2: return "r11d"; case 3: case -1: return "r11"; default: return "r11?"; }
      case TR::RealRegister::r12:
         switch (size) { case 0: return "r12b"; case 1: return "r12w"; case 2: return "r12d"; case 3: case -1: return "r12"; default: return "r12?"; }
      case TR::RealRegister::r13:
         switch (size) { case 0: return "r13b"; case 1: return "r13w"; case 2: return "r13d"; case 3: case -1: return "r13"; default: return "r13?"; }
      case TR::RealRegister::r14:
         switch (size) { case 0: return "r14b"; case 1: return "r14w"; case 2: return "r14d"; case 3: case -1: return "r14"; default: return "r14?"; }
      case TR::RealRegister::r15:
         switch (size) { case 0: return "r15b"; case 1: return "r15w"; case 2: return "r15d"; case 3: case -1: return "r15"; default: return "r15?"; }
#endif
      case TR::RealRegister::vfp:
         switch (size) { case 2: case 3: case -1: return "vfp";   default: return unknownRegisterName('v'); } // 3 is for AMD64
      case TR::RealRegister::st0:
         switch (size) { case 2: case -1: return "st(0)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::st1:
         switch (size) { case 2: case -1: return "st(1)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::st2:
         switch (size) { case 2: case -1: return "st(2)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::st3:
         switch (size) { case 2: case -1: return "st(3)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::st4:
         switch (size) { case 2: case -1: return "st(4)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::st5:
         switch (size) { case 2: case -1: return "st(5)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::st6:
         switch (size) { case 2: case -1: return "st(6)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::st7:
         switch (size) { case 2: case -1: return "st(7)";   default: return unknownRegisterName('s'); }
      case TR::RealRegister::mm0:
         switch (size) { case 3: case -1: return "mm0";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::mm1:
         switch (size) { case 3: case -1: return "mm1";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::mm2:
         switch (size) { case 3: case -1: return "mm2";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::mm3:
         switch (size) { case 3: case -1: return "mm3";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::mm4:
         switch (size) { case 3: case -1: return "mm4";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::mm5:
         switch (size) { case 3: case -1: return "mm5";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::mm6:
         switch (size) { case 3: case -1: return "mm6";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::mm7:
         switch (size) { case 3: case -1: return "mm7";   default: return unknownRegisterName('m'); }
      case TR::RealRegister::xmm0:
         switch (size) { case 4: case -1: return "xmm0";  default: return "?mm0"; }
      case TR::RealRegister::xmm1:
         switch (size) { case 4: case -1: return "xmm1";  default: return "?mm1"; }
      case TR::RealRegister::xmm2:
         switch (size) { case 4: case -1: return "xmm2";  default: return "?mm2"; }
      case TR::RealRegister::xmm3:
         switch (size) { case 4: case -1: return "xmm3";  default: return "?mm3"; }
      case TR::RealRegister::xmm4:
         switch (size) { case 4: case -1: return "xmm4";  default: return "?mm4"; }
      case TR::RealRegister::xmm5:
         switch (size) { case 4: case -1: return "xmm5";  default: return "?mm5"; }
      case TR::RealRegister::xmm6:
         switch (size) { case 4: case -1: return "xmm6";  default: return "?mm6"; }
      case TR::RealRegister::xmm7:
         switch (size) { case 4: case -1: return "xmm7";  default: return "?mm7"; }
#ifdef TR_TARGET_64BIT
      case TR::RealRegister::xmm8:
         switch (size) { case 4: case -1: return "xmm8";  default: return "?mm8"; }
      case TR::RealRegister::xmm9:
         switch (size) { case 4: case -1: return "xmm9";  default: return "?mm9"; }
      case TR::RealRegister::xmm10:
         switch (size) { case 4: case -1: return "xmm10"; default: return "?mm10"; }
      case TR::RealRegister::xmm11:
         switch (size) { case 4: case -1: return "xmm11"; default: return "?mm11"; }
      case TR::RealRegister::xmm12:
         switch (size) { case 4: case -1: return "xmm12"; default: return "?mm12"; }
      case TR::RealRegister::xmm13:
         switch (size) { case 4: case -1: return "xmm13"; default: return "?mm13"; }
      case TR::RealRegister::xmm14:
         switch (size) { case 4: case -1: return "xmm14"; default: return "?mm14"; }
      case TR::RealRegister::xmm15:
         switch (size) { case 4: case -1: return "xmm15"; default: return "?mm15"; }
#endif
      default: TR_ASSERT( 0, "unexpected register number"); return unknownRegisterName();
      }
   }

const char *
TR_Debug::getName(TR::RealRegister * reg, TR_RegisterSizes size)
   {
   if (reg->getKind() == TR_X87)
      {
      switch (reg->getRegisterNumber())
         {
         case TR::RealRegister::st0: return "st(0)";
         case TR::RealRegister::st1: return "st(1)";
         case TR::RealRegister::st2: return "st(2)";
         case TR::RealRegister::st3: return "st(3)";
         case TR::RealRegister::st4: return "st(4)";
         case TR::RealRegister::st5: return "st(5)";
         case TR::RealRegister::st6: return "st(6)";
         case TR::RealRegister::st7: return "st(7)";
         case TR::RealRegister::NoReg:
            switch (toX86FPStackRegister(reg)->getFPStackRegisterNumber())
               {
               case TR_X86FPStackRegister::fp0: return "fp0";
               case TR_X86FPStackRegister::fp1: return "fp1";
               case TR_X86FPStackRegister::fp2: return "fp2";
               case TR_X86FPStackRegister::fp3: return "fp3";
               case TR_X86FPStackRegister::fp4: return "fp4";
               case TR_X86FPStackRegister::fp5: return "fp5";
               case TR_X86FPStackRegister::fp6: return "fp6";
               case TR_X86FPStackRegister::fp7: return "fp7";
               default: TR_ASSERT( 0, "unexpected FPR number"); return unknownRegisterName('f');
               }
         default: TR_ASSERT( 0, "unexpected FPR number"); return unknownRegisterName('s');
         }
      }

   if (reg->getKind() == TR_FPR || reg->getKind() == TR_VRF)
      size = TR_QuadWordReg;

   return getName(reg->getRegisterNumber(), size);
   }

void TR_Debug::dumpInstructionWithVFPState(TR::Instruction *instr, const TR_VFPState *prevState)
   {
   if (_file != NULL)
      {
      const TR_VFPState &vfpState = _cg->vfpState();
      print(_file, instr);

      // Print the VFP state if something changed
      //
      // Note: IA32.cpp version 1.158 has code to compare the VFP state with
      // that calculated using the old VFP scheme to make sure it's correct.
      // TODO: Remove this note when we trust the new VFP scheme.
      //
      if (prevState && (vfpState != *prevState))
         {
         trfprintf(_file, "\n\t%s VFP=%s+%d",
                       commentString(),
                       getName(vfpState._register), vfpState._displacement);
         }
      trfflush(_file);
      }
   }

//
// IA32 Instructions
//

void
TR_Debug::printRegRegInstruction(TR::FILE *pOutFile, const char *opCode, TR::RealRegister *reg1, TR::RealRegister *reg2)
   {
   trfprintf(pOutFile, "%s\t", opCode);
   print(pOutFile, reg1);
   if (reg2)
      {
      trfprintf(pOutFile, ", ");
      print(pOutFile, reg2);
      }
   }

void
TR_Debug::printRegMemInstruction(TR::FILE *pOutFile, const char *opCode, TR::RealRegister *reg, TR::RealRegister *base, int32_t offset)
   {
   trfprintf(pOutFile, "%s\t", opCode);
   print(pOutFile, reg);
   if (base)
      {
      trfprintf(pOutFile, ", [");
      print(pOutFile, base);
      trfprintf(pOutFile, " +%d]", offset);
      }
   }

void
TR_Debug::printRegImmInstruction(TR::FILE *pOutFile, const char *opCode, TR::RealRegister *reg, int32_t imm)
   {
   trfprintf(pOutFile, "%s\t", opCode);
   print(pOutFile, reg);
   if (imm <= 1024)
      trfprintf(pOutFile, ", %d", imm);
   else
      trfprintf(pOutFile, ", " POINTER_PRINTF_FORMAT, imm);
   }

void
TR_Debug::printMemRegInstruction(TR::FILE *pOutFile, const char *opCode, TR::RealRegister *base, int32_t offset, TR::RealRegister *reg)
   {
   trfprintf(pOutFile, "%s\t", opCode);
   trfprintf(pOutFile, "[");
   print(pOutFile, base);
   trfprintf(pOutFile, " +%d]", offset);
   if (reg)
      {
      trfprintf(pOutFile, ", ");
      print(pOutFile, reg);
      }
   }

void
TR_Debug::printMemImmInstruction(TR::FILE *pOutFile, const char *opCode, TR::RealRegister *base, int32_t offset, int32_t imm)
   {
   trfprintf(pOutFile, "%s\t", opCode);
   trfprintf(pOutFile, "[");
   print(pOutFile, base);
   trfprintf(pOutFile, " +%d]", offset);
   if (imm <= 1024)
      trfprintf(pOutFile, ", %d", imm);
   else
      trfprintf(pOutFile, ", " POINTER_PRINTF_FORMAT, imm);
   }

void
TR_Debug::printLabelInstruction(TR::FILE *pOutFile, const char *opCode, TR::LabelSymbol *label)
   {
   trfprintf(pOutFile, "%s\t", opCode);
   print(pOutFile, label);
   }


const char callRegName64[][5] =
   {
      "rax","rsi","rdx","rcx"
   };

const char callRegName32[][5] =
   {
      "eax","esi","edx","ecx"
   };

static const char*
getInterpretedMethodNameHelper(TR::MethodSymbol *methodSymbol,
                               TR::DataType     type){
   if (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative())
      return "icallVMprJavaSendNativeStatic";

   switch(type)
      {
      case TR::NoType:
         return "interpreterVoidStaticGlue";
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         return "interpreterIntStaticGlue";
      case TR::Address:
         return "interpreterLongStaticGlue";
      case TR::Int64:
         return "interpreterLongStaticGlue";
      case TR::Float:
         return "interpreterFloatStaticGlue";
      case TR::Double:
         return "interpreterDoubleStaticGlue";
      default:
         TR_ASSERT(0, "Unsupported data type: %d", type);
         return "UNKNOWN interpreted method type";
      }
}


//
// IA32 Snippets
//

#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::print(TR::FILE *pOutFile, TR::X86CallSnippet  * snippet)
   {
   // *this   swipeable for debugger
   if (pOutFile == NULL)
      return;

   uint8_t            *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   TR::Node            *callNode = snippet->getNode();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   if (_comp->target().is64Bit())
      {
      int32_t   count;
      int32_t   size = 0;
      int32_t   offset = 8 * callNode->getNumChildren();

      for (count = 0; count < callNode->getNumChildren(); count++)
         {
            TR::Node       *child   = callNode->getChild(count);
            TR::DataType    type    = child->getType();


            switch (type)
               {
               case TR::Float:
               case TR::Int8:
               case TR::Int16:
               case TR::Int32:
                  printPrefix(pOutFile, NULL, bufferPos, 4);
                  trfprintf(pOutFile, "mov \tdword ptr[rsp+%d], %s\t\t#save registers for interpreter call snippet",
                                offset, callRegName32[count] );
                  offset-=8;
                  bufferPos+=4;
                  break;
               case TR::Double:
               case TR::Address:
               case TR::Int64:
                  printPrefix(pOutFile, NULL, bufferPos, 5);
                  trfprintf(pOutFile, "mov \tqword ptr[rsp+%d], %s\t\t#save registers for interpreter call snippet",
                                offset, callRegName64[count] );
                  offset-=8;
                  bufferPos+=5;
                  break;
               default:
                  TR_ASSERT(0, "unknown data type: %d", type);
                  break;
               }

          }
         intptr_t ramMethod = (intptr_t)methodSymbol->getMethodAddress();

         printPrefix(pOutFile, NULL, bufferPos, 10);
         trfprintf(pOutFile, "mov \trdi, 0x%x\t\t# MOV8RegImm64",ramMethod);
         bufferPos+=10;

         printPrefix(pOutFile, NULL, bufferPos, 5);
         trfprintf(pOutFile, "jmp \t%s\t\t# jump out of snippet code",
                       getInterpretedMethodNameHelper (methodSymbol, callNode->getDataType()));
         bufferPos+=5;
      }

   else
      {
         intptr_t ramMethod = (intptr_t)methodSymbol->getMethodAddress();

         printPrefix(pOutFile, NULL, bufferPos, 5);
         trfprintf(pOutFile, "mov \tedi, 0x%x\t\t# MOV8RegImm32",ramMethod);
         bufferPos+=5;

         printPrefix(pOutFile, NULL, bufferPos, 5);
         trfprintf(pOutFile, "jmp \t%s\t\t# jump out of snippet code",
                       getInterpretedMethodNameHelper (methodSymbol, callNode->getDataType()));
         bufferPos+=5;

      }

   return;

   }

#endif

void TR_Debug::printX86OOLSequences(TR::FILE *pOutFile)
   {

   auto oiIterator = _cg->getOutlinedInstructionsList().begin();
   while (oiIterator != _cg->getOutlinedInstructionsList().end())
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



//
// AMD64 Snippets
//

#ifdef TR_TARGET_64BIT
uint8_t *
TR_Debug::printArgumentFlush(TR::FILE *              pOutFile,
                             TR::Node             *callNode,
                             bool                 isFlushToStack, // flush to stack or flush to regs
                             uint8_t             *bufferPos)
   {
   TR::MethodSymbol *methodSymbol = callNode->getSymbol()->castToMethodSymbol();
   TR::Linkage *linkage = _cg->getLinkage();

   const TR::X86LinkageProperties &linkageProperties = linkage->getProperties();

   int32_t   numGPArgs = 0;
   int32_t   numFPArgs = 0;

   for (int i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++)
      {
      TR::Node *child = callNode->getChild(i);
      int8_t modrmOffset=0;
      const char *opCodeName=NULL, *regName=NULL;

      // Compute differences based on datatype
      //
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (numGPArgs < linkageProperties.getNumIntegerArgumentRegisters())
               {
               opCodeName = "mov";
               modrmOffset = 1;
               TR::RealRegister::RegNum reg = linkageProperties.getIntegerArgumentRegister(numGPArgs);
               regName = getName(_cg->machine()->getRealRegister(reg));
               }
            numGPArgs++;
            break;
         case TR::Address:
         case TR::Int64:
            if (numGPArgs < linkageProperties.getNumIntegerArgumentRegisters())
               {
               opCodeName = "mov";
               modrmOffset = 2;
               TR::RealRegister::RegNum reg = linkageProperties.getIntegerArgumentRegister(numGPArgs);
               regName = getName(_cg->machine()->getRealRegister(reg), TR_DoubleWordReg);
               }
            numGPArgs++;
            break;
         case TR::Float:
            if (numFPArgs < linkageProperties.getNumFloatArgumentRegisters())
               {
               opCodeName = "movss";
               modrmOffset = 3;
               TR::RealRegister::RegNum reg = linkageProperties.getFloatArgumentRegister(numFPArgs);
               regName = getName(_cg->machine()->getRealRegister(reg), TR_QuadWordReg);
               }
            numFPArgs++;
            break;
         case TR::Double:
            if (numFPArgs < linkageProperties.getNumFloatArgumentRegisters())
               {
               opCodeName = "movsd";
               modrmOffset = 3;
               TR::RealRegister::RegNum reg = linkageProperties.getFloatArgumentRegister(numFPArgs);
               regName = getName(_cg->machine()->getRealRegister(reg), TR_QuadWordReg);
               }
            numFPArgs++;
            break;
         default:
            break;
         }

      if (opCodeName)
         {
         // Compute differences based on addressing mode
         //
         uint32_t instrLength, displacement;
         switch (bufferPos[modrmOffset] & 0xc0)
            {
            case 0x40:
               displacement = *(uint8_t*)(bufferPos + modrmOffset + 2);
               instrLength = modrmOffset + 3;
               break;
            default:
               TR_ASSERT(0, "Unexpected Mod/RM value 0x%02x", bufferPos[modrmOffset]);
               // fall through
            case 0x80:
               displacement = *(uint32_t*)(bufferPos + modrmOffset + 2);
               instrLength = modrmOffset + 6;
               break;
            }

         // Print the instruction
         //
         printPrefix(pOutFile, NULL, bufferPos, instrLength);
         if (isFlushToStack)
            trfprintf(pOutFile, "%s\t[rsp +%d], %s", opCodeName, displacement, regName);
         else // Flush from stack to argument registers.
            trfprintf(pOutFile, "%s\t%s, [rsp +%d]", opCodeName, regName, displacement);
         bufferPos += instrLength;
         }
      else
         {
         // This argument is not in a register; skip it
         }
      }
   return bufferPos;
   }

#endif

static const char * opCodeToNameMap[] =
   {
#define INSTRUCTION(name, mnemonic, binary, property0, property1) #name
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
   };

static const char * opCodeToMnemonicMap[] =
   {
#define INSTRUCTION(name, mnemonic, binary, property0, property1) #mnemonic
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
   };

const char *
TR_Debug::getOpCodeName(TR_X86OpCode  * opCode)
   {
   return opCodeToNameMap[opCode->getOpCodeValue()];
   }

const char *
TR_Debug::getMnemonicName(TR_X86OpCode  * opCode)
   {
   if (_comp->target().isLinux())
      {
      int32_t o = opCode->getOpCodeValue();
      if (o == (int32_t) DQImm64) return dqString();
      if (o == (int32_t) DDImm4) return ddString();
      if (o == (int32_t) DWImm2) return dwString();
      if (o == (int32_t) DBImm1) return dbString();
      }
   return opCodeToMnemonicMap[opCode->getOpCodeValue()];
   }
