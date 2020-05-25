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

#if !defined(TR_TARGET_ARM)
int jitDebugARM;
#else

#include "arm/codegen/ARMInstruction.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "arm/codegen/ARMRecompilationSnippet.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCRegisterMap.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Recompilation.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "runtime/CodeCacheManager.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "arm/codegen/ARMHelperCallSnippet.hpp"
#endif

// change the following lines to not get the instruction addr during
// prints (useful for diff-ing output)
#define INSTR_OR_NULL instr

static const char *ARMConditionNames[] =
   {
   "eq",
   "ne",
   "cs",
   "cc",
   "mi",
   "pl",
   "vs",
   "vc",
   "hi",
   "ls",
   "ge",
   "lt",
   "gt",
   "le",
   "al",
   "nv"
   };

#if defined(__ARM_ARCH_7A__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
static const char * opCodeToVFPMap[] =
   {
   ".f64",      // fabsd (vabs<c>.f64)
   ".f32",      // fabss (vabs<c>.f32)
   ".f64",      // faddd (vadd<c>.f64)
   ".f32",      // fadds (vadd<c>.f32)
   ".f64",      // fcmpd (vcmp<c>.f64)
   ".f32",      // fcmps (vcmp<c>.f32)
   ".f64",      // fcmpzd (vcmp<c>.f64)
   ".f32",      // fcmpzs (vcmp<c>.f32)
   ".f64",      // fcpyd (vmov<c>.f64)
   ".f32",      // fcpys (vmov<c>.f32)
   ".f64.f32",  // fcvtds (vcvt<c>.f64.f32)
   ".f32.f64",  // fcvtsd (vcvt<c>.f32.f64)
   ".f64",      // fdivd (vdiv<c>.f64)
   ".f32",      // fdivs (vdiv<c>.f32)
   "",          // fldd  (vldr)
   "",          // fldmd (vldm)
   "",          // fldms (vldm)
   "",          // flds (vldr)
   ".f64",      // fmacd (vmla.f64)
   ".f32",      // fmacs (vmla.f32)
   "",          // fmdrr (vmov)
   "",          // fmrrd (vmov)
   "",          // fmrrs (vmov)
   "",          // fmrs (vmov)
   "",          // fmrx (vmrs)
   ".f64",      // fmscd (vmls.f64)
   ".f32",      // fmscs (vmls.f32)
   "",          // fmsr (vmov)
   "",          // fmsrr (vmov)
   "",          // fmstat (vmrs APSR_nzcv, FPSCR)
   ".f64",      // fmuld (vmul.f64)
   ".f32",      // fmuls (vmul.f32)
   "",          // fmxr (vmsr)
   ".f64",      // fnegd (vneg.f64)
   ".f32",      // fnegs (vneg.f32)
   ".f64",      // fnmacd (vnmla.f64)
   ".f32",      // fnmacs (vnmla.f32)
   ".f64",      // fnmscd (vnmls.f64)
   ".f32",      // fnmscs (vnmls.f32)
   ".f64",      // fnmuld (vnmul.f64)
   ".f32",      // fnmuls (vnmul.f32)
   ".f64.s32",  // fsitod (vcvt.f64.s32)
   ".f32.s32",  // fsitos (vcvt.f32.s32)
   ".f64",      // fsqrtd (vsqrt.f64)
   ".f32",      // fsqrts (vsqrt.f32)
   "",          // fstd (vstr)
   "",          // fstmd (vstm)
   "",          // fstms (vstm)
   "",          // fsts (vstr)
   ".f64",      // fsubd (vsub.f64)
   ".f32",      // fsubs (vsub.f32)
   ".s32.f64",  // ftosizd (vcvt.s32.f64)
   ".s32.f32",  // ftosizs (vcvt.s32.f32)
   ".u32.f64",  // ftouizd (vcvt.u32.f64)
   ".u32.f32",  // ftouizs (vcvt.u32.f32)
   ".f64,u32",  // fuitod (vcvt.f64,u32)
   ".f32.u32"   // fuitos (vcvt.f32.u32)
   };
#endif

static const char *
getExtraVFPInstrSpecifiers(TR_ARMOpCode *opCode)
   {
#define FIRST_VFP_INSTR		ARMOp_fabsd

#if defined(__ARM_ARCH_7A__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
   uint32_t index = (uint32_t)opCode->getOpCodeValue() - (uint32_t)(FIRST_VFP_INSTR);
   return opCodeToVFPMap[index];
#else
   return "";
#endif
#undef FIRST_VFP_INSTR
   }

char *
TR_Debug::fullOpCodeName(TR::Instruction * instr)
   {
   static char nameBuf[64];
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
   sprintf(nameBuf,
           "%s%s%s",
           getOpCodeName(&instr->getOpCode()),
           (instr->getConditionCode() != ARMConditionCodeAL) ? ARMConditionNames[instr->getConditionCode()] : "",
            instr->getOpCode().isVFPOp() ? getExtraVFPInstrSpecifiers(&instr->getOpCode()) : "");
#else
   sprintf(nameBuf,
           "%s%s",
           getOpCodeName(&instr->getOpCode()),
           (instr->getConditionCode() != ARMConditionCodeAL) ? ARMConditionNames[instr->getConditionCode()] : "");
#endif
   return nameBuf;
   }

void
TR_Debug::printPrefix(TR::FILE *pOutFile, TR::Instruction * instr)
   {
   printPrefix(pOutFile, instr, instr->getBinaryEncoding(), instr->getBinaryLength());
   }

void
TR_Debug::printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction *instr)
   {
   while (tabStops-- > 0)
      trfprintf(pOutFile, "\t");

   dumpInstructionComments(pOutFile, instr);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::Instruction * instr)
   {
   if (pOutFile == NULL)
      return;

   switch (instr->getKind())
      {
      case OMR::Instruction::IsImm:
         print(pOutFile, (TR::ARMImmInstruction *)instr);
         break;
      case OMR::Instruction::IsImmSym:
         print(pOutFile, (TR::ARMImmSymInstruction *)instr);
         break;
      case OMR::Instruction::IsLabel:
         print(pOutFile, (TR::ARMLabelInstruction *)instr);
         break;
      case OMR::Instruction::IsConditionalBranch:
         print(pOutFile, (TR::ARMConditionalBranchInstruction *)instr);
         break;
#ifdef J9_PROJECT_SPECIFIC
      case OMR::Instruction::IsVirtualGuardNOP:
         print(pOutFile, (TR::ARMVirtualGuardNOPInstruction *)instr);
         break;
#endif
      case OMR::Instruction::IsAdmin:
         print(pOutFile, (TR::ARMAdminInstruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Src2:
      case OMR::Instruction::IsSrc2:
      case OMR::Instruction::IsTrg1Src1:
         print(pOutFile, (TR::ARMTrg1Src2Instruction *)instr);
         break;
      case OMR::Instruction::IsTrg1:
         print(pOutFile, (TR::ARMTrg1Instruction *)instr);
         break;
      case OMR::Instruction::IsMem:
         print(pOutFile, (TR::ARMMemInstruction *)instr);
         break;
      case OMR::Instruction::IsMemSrc1:
         print(pOutFile, (TR::ARMMemSrc1Instruction *)instr);
         break;
      case OMR::Instruction::IsTrg1Mem:
         print(pOutFile, (TR::ARMTrg1MemInstruction *)instr);
         break;
      case OMR::Instruction::IsTrg1MemSrc1:
         print(pOutFile, (TR::ARMTrg1MemSrc1Instruction *)instr);
         break;
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
      case OMR::Instruction::IsTrg2Src1:
         print(pOutFile, (TR::ARMTrg2Src1Instruction *)instr);
         break;
#endif
      case OMR::Instruction::IsMul:
         print(pOutFile, (TR::ARMMulInstruction *)instr);
         break;
      case OMR::Instruction::IsControlFlow:
         print(pOutFile, (TR::ARMControlFlowInstruction *)instr);
         break;
      case OMR::Instruction::IsMultipleMove:
         print(pOutFile, (TR::ARMMultipleMoveInstruction *)instr);
         break;
      default:
         TR_ASSERT( 0, "unexpected instruction kind");
            // fall thru
      case OMR::Instruction::IsNotExtended:
         {
         printPrefix(pOutFile, instr);
         trfprintf(pOutFile, "%-32s", fullOpCodeName(instr));
         trfflush(pOutFile);
         }
      }
   }

void
TR_Debug::dumpDependencyGroup(TR::FILE *                        pOutFile,
                              TR_ARMRegisterDependencyGroup *group,
                              int32_t                        numConditions,
                              char                          *prefix,
                              bool                           omitNullDependencies)
   {
   int32_t i;
   bool    foundDep = false;

   trfprintf(pOutFile, "\n\t%s: ", prefix);
   for (i = 0; i < numConditions; ++i)
      {
      TR::Register *virtReg = group->getRegisterDependency(i)->getRegister();

      if (omitNullDependencies && !virtReg)
            continue;

      TR::RealRegister::RegNum r = group->getRegisterDependency(i)->getRealRegister();

      trfprintf(pOutFile, " [%s : ", getName(virtReg));
      if (r == TR::RealRegister::NoReg)
         trfprintf(pOutFile, "NoReg]");
      else
         trfprintf(pOutFile, "%s]", getName(_cg->machine()->getRealRegister(r)));

      foundDep = true;
      }

   if (!foundDep)
      trfprintf(pOutFile, " None");
   }

void
TR_Debug::dumpDependencies(TR::FILE *pOutFile, TR::Instruction *instr)
   {
   // If we are in instruction selection and dependency
   // information is requested, dump it.
   if (pOutFile == NULL || _cg->getStackAtlas())
      return;

   TR::RegisterDependencyConditions *deps = instr->getDependencyConditions();
   if (!deps)
      return;

   if (deps->getNumPreConditions() > 0)
      dumpDependencyGroup(pOutFile, deps->getPreConditions(), deps->getNumPreConditions(), " PRE", true);

   if (deps->getNumPostConditions() > 0)
      dumpDependencyGroup(pOutFile, deps->getPostConditions(), deps->getNumPostConditions(), "POST", true);

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMLabelInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   TR::LabelSymbol *label   = instr->getLabelSymbol();
   TR::Snippet *snippet = label ? label->getSnippet() : NULL;
   if (instr->getOpCodeValue() == ARMOp_label)
      {
      print(pOutFile, label);
      trfprintf(pOutFile, ":");
      if (label->isStartInternalControlFlow())
         trfprintf(pOutFile, " (Start of internal control flow)");
      else if (label->isEndInternalControlFlow())
         trfprintf(pOutFile, " (End of internal control flow)");
      }
   else if (instr->getOpCodeValue() == ARMOp_b || instr->getOpCodeValue() == ARMOp_bl)
      {
      trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
      print(pOutFile, label);
      if (snippet)
         trfprintf(pOutFile, " (%s)", getName(snippet));
      }
   else
      {
      trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
      print(pOutFile, instr->getTarget1Register(), TR_WordReg);
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getSource1Register(), TR_WordReg);
      trfprintf(pOutFile, ", ");
      uint8_t *instrPos   = instr->getBinaryEncoding();
      trfprintf(pOutFile, "#(");
      print(pOutFile, label);
      trfprintf(pOutFile, " - gr15)");
      }
   printInstructionComment(pOutFile, 1, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMVirtualGuardNOPInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s Site:" POINTER_PRINTF_FORMAT ", ", getOpCodeName(&instr->getOpCode()), instr->getSite());
   print(pOutFile, instr->getLabelSymbol());
   printInstructionComment(pOutFile, 1, instr);
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }
#endif

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMAdminInstruction * instr)
   {
   // Omit admin instructions from post-binary dumps unless they mark basic block boundaries.
   if (instr->getBinaryEncoding() &&
       instr->getNode()->getOpCodeValue() != TR::BBStart &&
       instr->getNode()->getOpCodeValue() != TR::BBEnd)
      return;

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
   if (instr->getOpCodeValue() == ARMOp_fence && instr->getFenceNode())
      {
      TR::Node *fenceNode = instr->getFenceNode();
      if (fenceNode->getRelocationType() == TR_AbsoluteAddress)
         trfprintf(pOutFile, " Absolute [");
      else if (fenceNode->getRelocationType() == TR_ExternalAbsoluteAddress)
         trfprintf(pOutFile, " External Absolute [");
      else
         trfprintf(pOutFile, " Relative [");
      for (uint32_t i = 0; i < fenceNode->getNumRelocations(); i++)
         {
         trfprintf(pOutFile, " " POINTER_PRINTF_FORMAT, fenceNode->getRelocationDestination(i));
         }
      trfprintf(pOutFile, " ]");
      }
   else
      {
      trfprintf(pOutFile, "\t\t");
      }

   TR::Node *node = instr->getNode();
   if (node && node->getOpCodeValue() == TR::BBStart)
      trfprintf(pOutFile, " (BBStart (block_%d))", node->getBlock()->getNumber());
   else if (node && node->getOpCodeValue() == TR::BBEnd)
      trfprintf(pOutFile, " (BBEnd (block_%d))", node->getBlock()->getNumber());

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMImmInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t0x%08x", fullOpCodeName(instr), instr->getSourceImmediate());
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMImmSymInstruction * instr)
   {
   uint8_t *bufferPos = instr->getBinaryEncoding();
   int32_t  imm       = instr->getSourceImmediate();

   TR::SymbolReference      *symRef    = instr->getSymbolReference();
   TR::Symbol                *sym       = symRef->getSymbol();
   TR::ResolvedMethodSymbol *calleeSym = sym->getResolvedMethodSymbol();
   TR_ResolvedMethod     *callee    = calleeSym ? calleeSym->getResolvedMethod() : NULL;

   bool longJump = imm &&
                   _cg->directCallRequiresTrampoline((intptr_t)imm, (intptr_t)bufferPos) &&
                   !_comp->isRecursiveMethodTarget(sym);

   if (bufferPos != NULL && longJump)
      {
      if (*(int32_t*)bufferPos == 0xe1a0e00f)
         {
         const char *name = getName(symRef);
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "mov\tgr14, gr15");
         printPrefix(pOutFile, NULL, bufferPos+4, 4);
         trfprintf(pOutFile, "ldr\tgr15, [gr15, #-4]");
         printPrefix(pOutFile, NULL, bufferPos+8, 4);
         if (name)
            trfprintf(pOutFile, "DCD\t0x%x\t; %s (" POINTER_PRINTF_FORMAT ")", imm, name);
         else
            trfprintf(pOutFile, "DCD\t0x%x", imm);
         return;
         }
      else
         {
         printARMHelperBranch(instr->getSymbolReference(), bufferPos, pOutFile, fullOpCodeName(instr));
         }
      }
   else
      {
      const char *name = getName(symRef);
      printPrefix(pOutFile, instr);
      if (name)
         {
         trfprintf(pOutFile, "%s\t%-24s; ", fullOpCodeName(instr), name);

         TR::Snippet *snippet = sym->getLabelSymbol() ? sym->getLabelSymbol()->getSnippet() : NULL;
         if (snippet)
            trfprintf(pOutFile, "(%s)", getName(snippet));
         else
            trfprintf(pOutFile, "(" POINTER_PRINTF_FORMAT ")", imm);
         }
      else
         {
         trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT, fullOpCodeName(instr), imm);
         }
      }
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMTrg1Src2Instruction * instr)
   {
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR_RegisterSizes targetSize = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
   TR_RegisterSizes source1Size = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
   TR_RegisterSizes source2Size = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
#else
   TR_RegisterSizes targetSize = TR_WordReg;
   TR_RegisterSizes source1Size = TR_WordReg;
   TR_RegisterSizes source2Size = TR_WordReg;
#endif

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   if (instr->getOpCodeValue() == ARMOp_fmrs)
      {
      if (instr->getTarget1Register())
         {
         print(pOutFile, instr->getTarget1Register(), targetSize);
         trfprintf(pOutFile, ", ");
         }
      if (instr->getSource1Register())
         {
         print(pOutFile, instr->getSource1Register(), source1Size);
         }
      }
   else if (instr->getOpCodeValue() == ARMOp_fmsr)
      {
      if (instr->getSource1Register())
         {
         print(pOutFile, instr->getSource1Register(), source1Size);
         trfprintf(pOutFile, ", ");
         }
      if (instr->getTarget1Register())
         {
         print(pOutFile, instr->getTarget1Register(), targetSize);
         }
      }
   else if (instr->getOpCodeValue() == ARMOp_fmdrr)
      {
      print(pOutFile, instr->getSource2Operand(), TR_DoubleReg);
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getTarget1Register(), TR_WordReg);
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getSource1Register(), TR_WordReg);
      }
   else if (instr->getOpCodeValue() == ARMOp_fcvtds ||
            instr->getOpCodeValue() == ARMOp_fsitod ||
            instr->getOpCodeValue() == ARMOp_fuitod)
      {
      print(pOutFile, instr->getTarget1Register(), TR_DoubleReg);
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getSource2Operand(), TR_WordReg);
      }
   else if (instr->getOpCodeValue() == ARMOp_fcvtsd  ||
            instr->getOpCodeValue() == ARMOp_ftosizd ||
            instr->getOpCodeValue() == ARMOp_ftouizd)
      {
      print(pOutFile, instr->getTarget1Register(), TR_WordReg);
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getSource2Operand(), TR_DoubleReg);
      }
   else if (instr->getOpCodeValue() == ARMOp_fcmpzd  ||
            instr->getOpCodeValue() == ARMOp_fcmpzs)
      {
      print(pOutFile, instr->getTarget1Register(), (instr->getOpCodeValue() == ARMOp_fcmpzd) ? TR_DoubleReg : TR_WordReg);
#if defined(__ARM_ARCH_7A__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
      trfprintf(pOutFile, ", #0.0");
#endif
      }
   else
#endif
      {
      if (instr->getTarget1Register())
         {
         // TR_RegisterSizes targetSize = TR_WordReg;
         print(pOutFile, instr->getTarget1Register(), targetSize);
         trfprintf(pOutFile, ", ");
         }
      // TR_RegisterSizes source1Size = TR_WordReg;
      if (instr->getSource1Register() && instr->getOpCodeValue() != ARMOp_swp)
         {
         print(pOutFile, instr->getSource1Register(), source1Size);
         trfprintf(pOutFile, ", ");
         }
      // TR_RegisterSizes source2Size = TR_WordReg;
      print(pOutFile, instr->getSource2Operand(), source2Size);
      if (instr->getSource1Register() && instr->getOpCodeValue() == ARMOp_swp)
         {
         trfprintf(pOutFile, ", [");
         print(pOutFile, instr->getSource1Register(), source1Size);
         trfprintf(pOutFile, "]");
         }
      }
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMTrg2Src1Instruction * instr)
   {
   // fmrrd
   TR_RegisterSizes size = TR_WordReg;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
   if (instr->getTarget1Register())
      {
      print(pOutFile, instr->getTarget1Register(), size);
      trfprintf(pOutFile, ", ");
      }
   if (instr->getTarget2Register())
      {
      print(pOutFile, instr->getTarget2Register(), size);
      trfprintf(pOutFile, ", ");
      }
   if (instr->getSource1Register())
      {
      print(pOutFile, instr->getSource1Register(), TR_DoubleReg);
      }
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }
#endif

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMMulInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));

   print(pOutFile, instr->getTargetLoRegister());
   if (instr->getTargetHiRegister())  // if a mull instruction
      {
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getTargetHiRegister());
      }
   trfprintf(pOutFile, ", ");

   print(pOutFile, instr->getSource1Register());
   trfprintf(pOutFile, ", ");

   print(pOutFile, instr->getSource2Register());
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMMemSrc1Instruction * instr)
   {
   if(instr->getBinaryEncoding() &&
      instr->getMemoryReference()->hasDelayedOffset() &&
      !instr->getMemoryReference()->getUnresolvedSnippet())
      {
      TR_ARMOpCodes op = instr->getOpCodeValue();
      int32_t offset = instr->getMemoryReference()->getOffset();
      if(op == ARMOp_strh && !constantIsUnsignedImmed8(offset))
         {
         printARMDelayedOffsetInstructions(pOutFile,instr);
         return;
         }
      else if(!constantIsImmed12(offset))
         {
         printARMDelayedOffsetInstructions(pOutFile,instr);
         return;
         }
	   // deliberate fall through if none of the above cases are taken.
      }

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
   print(pOutFile, instr->getMemoryReference());
   trfprintf(pOutFile, ", ");
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   print(pOutFile, instr->getSourceRegister(), instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg);
#else
   print(pOutFile, instr->getSourceRegister());
#endif
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMTrg1Instruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
   print(pOutFile, instr->getTargetRegister());
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMMemInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
   print(pOutFile, instr->getMemoryReference());
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMTrg1MemInstruction * instr)
   {
   if(instr->getBinaryEncoding() &&
      instr->getMemoryReference()->hasDelayedOffset() &&
      !instr->getMemoryReference()->getUnresolvedSnippet())
      {
      TR_ARMOpCodes op = instr->getOpCodeValue();
      int32_t offset = instr->getMemoryReference()->getOffset();
      if(op == ARMOp_add && instr->getMemoryReference()->getIndexRegister())
         {
         printARMDelayedOffsetInstructions(pOutFile,instr);
         return;
         }
      else if((op == ARMOp_ldrsb || op == ARMOp_ldrh || op == ARMOp_ldrsh) && !constantIsUnsignedImmed8(offset))
         {
         printARMDelayedOffsetInstructions(pOutFile,instr);
         return;
         }
      else if(!constantIsImmed12(offset))
         {
         printARMDelayedOffsetInstructions(pOutFile,instr);
         return;
         }
	   // deliberate fall through if none of the above cases are taken.
      }

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   print(pOutFile, instr->getTargetRegister(), instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg);
#else
   print(pOutFile, instr->getTargetRegister());
#endif
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getMemoryReference());
   TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference().getSymbol();
   if (symbol && symbol->isSpillTempAuto())
      {
      trfprintf(pOutFile, "\t; spilled for %s", getName(instr->getNode()->getOpCode()));
      }
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMTrg1MemSrc1Instruction * instr)
   {
   // strex
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
   print(pOutFile, instr->getTargetRegister());
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getSourceRegister());
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getMemoryReference());
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMControlFlowInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));

   int i;
   int numTargets = instr->getNumTargets();
   int numSources = instr->getNumSources();
   for (i = 0; i < numTargets; i++)
      {
      print(pOutFile, instr->getTargetRegister(i));
      if (i != numTargets + numSources - 1)
         {
         trfprintf(pOutFile, ", ");
         }
      }
   for (i = 0; i < numSources; i++)
      {
      print(pOutFile, instr->getSourceRegister(i));
      if (i != numSources - 1)
         {
         trfprintf(pOutFile, ", ");
         }
      }
   if (instr->getOpCode2Value() != ARMOp_bad)
      {
      trfprintf(pOutFile, ", %s", getOpCodeName(&instr->getOpCode2()));
      }
   if (instr->getLabelSymbol())
      {
      trfprintf(pOutFile, ", ");
      print(pOutFile, instr->getLabelSymbol());
      }
   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMMultipleMoveInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));

   print(pOutFile, instr->getMemoryBaseRegister());
   trfprintf(pOutFile, "%s, {", instr->isWriteBack() ? "!" : "");

   TR::Machine *machine = _cg->machine();

   uint32_t       regList = (uint32_t) instr->getRegisterList();
   if (!instr->getOpCode().isVFPOp())
      {
      for (int i = 0; regList && i < 16; i++)
         {
         if (regList & 1)
            {
            TR::RealRegister::RegNum r;
            r = (TR::RealRegister::RegNum)((int)TR::RealRegister::FirstGPR + i);
            print(pOutFile, machine->getRealRegister(r));
            regList >>= 1;
            if (regList && i != 15)
              trfprintf(pOutFile, ",");
            }
         else
            regList >>= 1;
         }
      }
   else
      {
      int startReg = (regList & 0x0f000) >> 12;
      int nreg = (regList & 0x00ff) >> 1;
      TR_RegisterSizes size = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
      for (int i = 0; i < nreg; i++)
         {
         TR::RealRegister::RegNum r = (TR::RealRegister::RegNum)((int)TR::RealRegister::FirstFPR + startReg + i);
         print(pOutFile, machine->getRealRegister(r), size);
         if (i != (nreg - 1))
            trfprintf(pOutFile, ",");
         }
      }
   trfprintf(pOutFile, "}");

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);
   }

void
TR_Debug::printARMHelperBranch(TR::SymbolReference *symRef, uint8_t *bufferPos, TR::FILE *pOutFile, const char * opcodeName)
   {
   TR::MethodSymbol *methodSym = symRef->getSymbol()->castToMethodSymbol();
   intptr_t         target    = (intptr_t)methodSym->getMethodAddress();
   char             *info      = "";

   if (_cg->directCallRequiresTrampoline(target, (intptr_t)bufferPos))
      {
      int32_t refNum = symRef->getReferenceNumber();
      if (refNum < TR_ARMnumRuntimeHelpers)
         {
         target = TR::CodeCacheManager::instance()->findHelperTrampoline(refNum, (void *)bufferPos);
         info   = " through trampoline";
         }
      else if (*((uintptr_t*)bufferPos) == 0xe28fe004)  // This is a JNI method
         {
         target = *((intptr_t*)(bufferPos+8));
         info   = " long jump";
         }
      else
         {
         target = _comp->fe()->methodTrampolineLookup(_comp, symRef, (void *)bufferPos);
         info   = " through trampoline";
         }
      }

   const char *name = getName(symRef);
   printPrefix(pOutFile, NULL, bufferPos, 4);
   if (name)
      trfprintf(pOutFile, "%s\t%s\t;%s (" POINTER_PRINTF_FORMAT ")", opcodeName, name, info, target);
   else
      trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT "\t\t;%s", opcodeName, target, info);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::MemoryReference * mr)
   {
   trfprintf(pOutFile, "[");
   if (mr->getBaseRegister() != NULL)
      {
      print(pOutFile, mr->getBaseRegister());
      if (mr->isPostIndexed())
         trfprintf(pOutFile, "]");
      trfprintf(pOutFile, ", ");
      }
   if (mr->getIndexRegister() != NULL)
      {
      print(pOutFile, mr->getIndexRegister());
      if(mr->getScale() != 0)
         {
         TR_ASSERT( mr->getScale() == 4, "strides other than 4 unimplemented");
         trfprintf(pOutFile, " LSL #2");
         }
      }
   else
      {
      if (mr->getSymbolReference().getSymbol() != NULL)
         {
         print(pOutFile, &mr->getSymbolReference());
         }
      else
         {
         trfprintf(pOutFile, "%+d", mr->getOffset());
         }
      }
   if (!mr->isPostIndexed())
      {
      trfprintf(pOutFile, "]");
      }
   if (mr->isImmediatePreIndexed())
      {
      trfprintf(pOutFile, "!");
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_ARMOperand2 * op, TR_RegisterSizes size)
   {
   switch (op->_opType)
      {
      case ARMOp2RegLSLImmed:  print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " LSL #%d", op->_shiftOrImmed); break;
      case ARMOp2RegLSRImmed:  print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " LSR #%d", op->_shiftOrImmed); break;
      case ARMOp2RegASRImmed:  print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " ASR #%d", op->_shiftOrImmed); break;
      case ARMOp2RegRORImmed:  print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " ROR #%d", op->_shiftOrImmed); break;
      case ARMOp2Reg:          print(pOutFile, op->_baseRegister, size); break;
      case ARMOp2RegRRX:       print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " RRX"); break;
      case ARMOp2RegLSLReg:    print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " LSL "); print(pOutFile, op->_shiftRegister, size); break;
      case ARMOp2RegLSRReg:    print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " LSR "); print(pOutFile, op->_shiftRegister, size); break;
      case ARMOp2RegASRReg:    print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " ASR "); print(pOutFile, op->_shiftRegister, size); break;
      case ARMOp2RegRORReg:    print(pOutFile, op->_baseRegister, size); trfprintf(pOutFile, " ROR "); print(pOutFile, op->_shiftRegister, size); break;
      case ARMOp2Immed8r:
         if (op->_immedShift)
            trfprintf(pOutFile, "#0x%08x (0x%x << %d)", op->_shiftOrImmed << op->_immedShift, op->_shiftOrImmed, op->_immedShift);
         else
            trfprintf(pOutFile, "#0x%08x", op->_shiftOrImmed);
         break;
      }
   }

void
TR_Debug::printARMGCRegisterMap(TR::FILE *pOutFile, TR::GCRegisterMap * map)
   {
   TR::Machine *machine = _cg->machine();
   trfprintf(pOutFile,"    registers: {");
   for (int i = 15; i>=0; i--)
      {
      if (map->getMap() & (1 << i))
         trfprintf(pOutFile, "%s ", getName(machine->getRealRegister((TR::RealRegister::RegNum)(i + TR::RealRegister::FirstGPR))));
      }

   trfprintf(pOutFile,"}\n");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RealRegister * reg, TR_RegisterSizes size)
   {
   trfprintf(pOutFile, "%s", getName(reg, size));
   }

const char *
TR_Debug::getName(TR::RealRegister * reg, TR_RegisterSizes size)
   {
   return getName(reg->getRegisterNumber(), size);
   }

const char *
TR_Debug::getName(uint32_t realRegisterIndex, TR_RegisterSizes size)
   {
   switch (realRegisterIndex)
      {
      case TR::RealRegister::gr0 : return "gr0";
      case TR::RealRegister::gr1 : return "gr1";
      case TR::RealRegister::gr2 : return "gr2";
      case TR::RealRegister::gr3 : return "gr3";
      case TR::RealRegister::gr4 : return "gr4";
      case TR::RealRegister::gr5 : return "gr5";
      case TR::RealRegister::gr6 : return "gr6";
      case TR::RealRegister::gr7 : return "gr7";
      case TR::RealRegister::gr8 : return "gr8";
      case TR::RealRegister::gr9 : return "gr9";
      case TR::RealRegister::gr10: return "gr10";
      case TR::RealRegister::gr11: return "gr11";
      case TR::RealRegister::gr12: return "gr12";
      case TR::RealRegister::gr13: return "gr13";
      case TR::RealRegister::gr14: return "gr14";
      case TR::RealRegister::gr15: return "gr15";

#if defined(__VFP_FP__) && !defined(__SOFTFP__)
      case TR::RealRegister::fp0 : return ((size == TR_DoubleReg) ? "d0" : "s0");
      case TR::RealRegister::fp1 : return ((size == TR_DoubleReg) ? "d1" : "s2");
      case TR::RealRegister::fp2 : return ((size == TR_DoubleReg) ? "d2" : "s4");
      case TR::RealRegister::fp3 : return ((size == TR_DoubleReg) ? "d3" : "s6");
      case TR::RealRegister::fp4 : return ((size == TR_DoubleReg) ? "d4" : "s8");
      case TR::RealRegister::fp5 : return ((size == TR_DoubleReg) ? "d5" : "s10");
      case TR::RealRegister::fp6 : return ((size == TR_DoubleReg) ? "d6" : "s12");
      case TR::RealRegister::fp7 : return ((size == TR_DoubleReg) ? "d7" : "s14");
      case TR::RealRegister::fp8 : return ((size == TR_DoubleReg) ? "d8" : "s16");
      case TR::RealRegister::fp9 : return ((size == TR_DoubleReg) ? "d9" : "s18");
      case TR::RealRegister::fp10 : return ((size == TR_DoubleReg) ? "d10" : "s20");
      case TR::RealRegister::fp11 : return ((size == TR_DoubleReg) ? "d11" : "s22");
      case TR::RealRegister::fp12 : return ((size == TR_DoubleReg) ? "d12" : "s24");
      case TR::RealRegister::fp13 : return ((size == TR_DoubleReg) ? "d13" : "s26");
      case TR::RealRegister::fp14 : return ((size == TR_DoubleReg) ? "d14" : "s28");
      case TR::RealRegister::fp15 : return ((size == TR_DoubleReg) ? "d15" : "s30");

      // For Single-precision registers group (fs0~fs15/FSR)
      case TR::RealRegister::fs0 : return "S0";
      case TR::RealRegister::fs1 : return "S1";
      case TR::RealRegister::fs2 : return "S2";
      case TR::RealRegister::fs3 : return "S3";
      case TR::RealRegister::fs4 : return "S4";
      case TR::RealRegister::fs5 : return "S5";
      case TR::RealRegister::fs6 : return "S6";
      case TR::RealRegister::fs7 : return "S7";
      case TR::RealRegister::fs8 : return "S8";
      case TR::RealRegister::fs9 : return "S9";
      case TR::RealRegister::fs10 : return "S10";
      case TR::RealRegister::fs11 : return "S11";
      case TR::RealRegister::fs12 : return "S12";
      case TR::RealRegister::fs13 : return "S13";
      case TR::RealRegister::fs14 : return "S14";
      case TR::RealRegister::fs15 : return "S15";
#else
      case TR::RealRegister::fp0 : return "fp0";
      case TR::RealRegister::fp1 : return "fp1";
      case TR::RealRegister::fp2 : return "fp2";
      case TR::RealRegister::fp3 : return "fp3";
      case TR::RealRegister::fp4 : return "fp4";
      case TR::RealRegister::fp5 : return "fp5";
      case TR::RealRegister::fp6 : return "fp6";
      case TR::RealRegister::fp7 : return "fp7";
#endif
      default:                       return "???";
      }
   }

const char *
TR_Debug::getNamea(TR::Snippet * snippet)
   {
   switch (snippet->getKind())
      {
      case TR::Snippet::IsCall:
         return "Call Snippet";
         break;
      case TR::Snippet::IsUnresolvedCall:
         return "Unresolved Call Snippet";
         break;
      case TR::Snippet::IsVirtualUnresolved:
         return "Unresolved Virtual Call Snippet";
         break;
      case TR::Snippet::IsInterfaceCall:
         return "Interface Call Snippet";
         break;
      case TR::Snippet::IsStackCheckFailure:
         return "Stack Check Failure Snippet";
         break;
      case TR::Snippet::IsUnresolvedData:
         return "Unresolved Data Snippet";
         break;
      case TR::Snippet::IsRecompilation:
         return "Recompilation Snippet";
         break;
      case TR::Snippet::IsHelperCall:
         return "Helper Call Snippet";
         break;
      case TR::Snippet::IsMonitorEnter:
         return "MonitorEnter Inc Counter";
         break;
      case TR::Snippet::IsMonitorExit:
         return "MonitorExit Dec Counter";
         break;
      default:
         return "<unknown snippet>";
      }
   }

void
TR_Debug::printa(TR::FILE *pOutFile, TR::Snippet * snippet)
   {
   if (pOutFile == NULL)
      return;

   switch (snippet->getKind())
      {
      case TR::Snippet::IsCall:
         print(pOutFile, (TR::ARMCallSnippet *)snippet);
         break;
      case TR::Snippet::IsUnresolvedCall:
         print(pOutFile, (TR::ARMUnresolvedCallSnippet *)snippet);
         break;
      case TR::Snippet::IsVirtualUnresolved:
         print(pOutFile, (TR::ARMVirtualUnresolvedSnippet *)snippet);
         break;
      case TR::Snippet::IsInterfaceCall:
         print(pOutFile, (TR::ARMInterfaceCallSnippet *)snippet);
         break;
      case TR::Snippet::IsStackCheckFailure:
         print(pOutFile, (TR::ARMStackCheckFailureSnippet *)snippet);
         break;
      case TR::Snippet::IsUnresolvedData:
         print(pOutFile, (TR::UnresolvedDataSnippet *)snippet);
         break;
      case TR::Snippet::IsRecompilation:
         print(pOutFile, (TR::ARMRecompilationSnippet *)snippet);
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::Snippet::IsHelperCall:
         print(pOutFile, (TR::ARMHelperCallSnippet *)snippet);
         break;
#endif
      case TR::Snippet::IsMonitorEnter:
         //print(pOutFile, (TR::ARMMonitorEnterSnippet *)snippet);
         trfprintf(pOutFile, "** MonitorEnterSnippet **\n");
         break;
      case TR::Snippet::IsMonitorExit:
         //print(pOutFile, (TR::ARMMonitorExitSnippet *)snippet);
         trfprintf(pOutFile, "** MonitorExitSnippet **\n");
         break;
      default:
         TR_ASSERT( 0, "unexpected snippet kind");
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMCallSnippet * snippet)
   {
#if J9_PROJECT_SPECIFIC
   TR::Node            *callNode     = snippet->getNode();
   TR::SymbolReference *glueRef      = _cg->getSymRef(snippet->getHelper());;
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(methodSymRef));

   TR::Machine *machine = _cg->machine();
   const TR::ARMLinkageProperties &linkage = _cg->getLinkage(methodSymbol->getLinkageConvention())->getProperties();

   uint32_t numIntArgs   = 0;
   uint32_t numFloatArgs = 0;

   int32_t offset;
   if (linkage.getRightToLeft())
      offset = linkage.getOffsetToFirstParm();
   else
      offset = snippet->getSizeOfArguments() + linkage.getOffsetToFirstParm();

   for (int i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++)
      {
      switch (callNode->getChild(i)->getOpCode().getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (!linkage.getRightToLeft())
               offset -= 4;
            if (numIntArgs < linkage.getNumIntArgRegs())
               {
               printPrefix(pOutFile, NULL, bufferPos, 4);
               trfprintf(pOutFile, "str\t[gr7, %+d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getIntegerArgumentRegister(numIntArgs)));
               bufferPos += 4;
               }
            numIntArgs++;
            if (linkage.getRightToLeft())
               offset += 4;
            break;
         case TR::Int64:
            if (!linkage.getRightToLeft())
               offset -= 8;
            if (numIntArgs < linkage.getNumIntArgRegs())
               {
               printPrefix(pOutFile, NULL, bufferPos, 4);
               trfprintf(pOutFile, "str\t[gr7, %+d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getIntegerArgumentRegister(numIntArgs)));
               bufferPos += 4;
               if (numIntArgs < linkage.getNumIntArgRegs() - 1)
                  {
                  printPrefix(pOutFile, NULL, bufferPos, 4);
                  trfprintf(pOutFile, "str\t[gr7, %+d], ", offset + 4);
                  print(pOutFile, machine->getRealRegister(linkage.getIntegerArgumentRegister(numIntArgs + 1)));
                  bufferPos += 4;
                  }
               }
            numIntArgs += 2;
            if (linkage.getRightToLeft())
               offset += 8;
            break;
// TODO FLOAT
#if 0
         case TR::Float:
            if (!linkage.getRightToLeft())
               offset -= 4;
            if (numFloatArgs < linkage.getNumFloatArgRegs())
               {
               printPrefix(pOutFile, NULL, bufferPos, 4);
               trfprintf(pOutFile, "stfs\t[gr7, %+d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getFloatArgumentRegister(numFloatArgs)));
               bufferPos += 4;
               }
            numFloatArgs++;
            if (linkage.getRightToLeft())
               offset += 4;
            break;
         case TR::Double:
            if (!linkage.getRightToLeft())
               offset -= 8;
            if (numFloatArgs < linkage.getNumFloatArgRegs())
               {
               printPrefix(pOutFile, NULL, bufferPos, 4);
               trfprintf(pOutFile, "stfd\t[gr7, %+d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getFloatArgumentRegister(numFloatArgs)));
               bufferPos += 4;
               }
            numFloatArgs++;
            if (linkage.getRightToLeft())
               offset += 8;
            break;
#endif
         }
      }

   printARMHelperBranch(glueRef, bufferPos, pOutFile);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; return address", snippet->getCallRA());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   if (methodSymRef->isUnresolved())
      trfprintf(pOutFile, "dd\t0x00000000\t\t; method address (unresolved)");
   else
      trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; method address (interpreted)");
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; lock word for compilation");
#endif
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMUnresolvedCallSnippet * snippet)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::SymbolReference *methodSymRef = snippet->getNode()->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   int32_t helperLookupOffset;
   switch (snippet->getNode()->getOpCode().getDataType())
      {
      case TR::NoType:
         helperLookupOffset = 0;
         break;
      case TR::Int32:
      case TR::Address:
         helperLookupOffset = 4;
         break;
      case TR::Int64:
         helperLookupOffset = 8;
         break;
      case TR::Float:
         helperLookupOffset = 12;
         break;
      case TR::Double:
         helperLookupOffset = 16;
         break;
      }

   print(pOutFile, (TR::ARMCallSnippet *)snippet);
   bufferPos += (snippet->getLength(0) - 12);

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; offset|flag|cpIndex", (helperLookupOffset << 24) | methodSymRef->getCPIndex());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; cpAddress", getOwningMethod(methodSymRef)->constantPool());
#endif
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMVirtualUnresolvedSnippet * snippet)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::SymbolReference *callSymRef = snippet->getNode()->getSymbolReference();

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(callSymRef));

   TR::SymbolReference *glueRef = _cg->getSymRef(TR_ARMvirtualUnresolvedHelper);
   printARMHelperBranch(glueRef, bufferPos, pOutFile);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; return address", snippet->getReturnLabel()->getCodeLocation());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; cpAddress", getOwningMethod(callSymRef)->constantPool());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; cpIndex", callSymRef->getCPIndex());
#endif
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMInterfaceCallSnippet * snippet)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::SymbolReference *callSymRef = snippet->getNode()->getSymbolReference();
   TR::SymbolReference *glueRef    = _cg->getSymRef(TR_ARMinterfaceCallHelper);

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(callSymRef));

   printARMHelperBranch(glueRef, bufferPos, pOutFile);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; return address", (uintptr_t)snippet->getReturnLabel()->getCodeLocation());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; cpAddress", (uintptr_t)getOwningMethod(callSymRef)->constantPool());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; cpIndex", callSymRef->getCPIndex());
   bufferPos += 4;

   const char *comment[] =
      {
      "resolved interface class",
      "resolved method index"
      };

   for (int i = 0; i < 2; i++)
      {
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "dd\t0x%08x\t\t; %s", 0, comment[i]);
      bufferPos += 4;
      }
#if 0
   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; resolved interface class", 0);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; resolved method index", 0);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; 1st cached class", -1);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; 1st cached target", 0);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; 2nd cached class", -1);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; 2nd cached target", 0);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; 1st lock word", 0);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; 2nd lock word", 0);
#endif
#endif
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMHelperCallSnippet * snippet)
   {
#ifdef J9_PROJECT_SPECIFIC
   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   TR::LabelSymbol *restartLabel = snippet->getRestartLabel();
   //int32_t          distance  = target - (uintptr_t)bufferPos;

   printARMHelperBranch(snippet->getDestination(), bufferPos, pOutFile);
   bufferPos += 4;

   if (restartLabel)
      {
      printPrefix(pOutFile, NULL, bufferPos, 4);
      //int32_t distance = *((int32_t *) bufferPos) & 0x00ffffff;
      //distance = (distance << 8) >> 8;   // sign extend
      trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; Return to %s", (intptr_t)(restartLabel->getCodeLocation()), getName(restartLabel));
      }
#endif
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMStackCheckFailureSnippet * snippet)
   {
   uint8_t *bufferPos  = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   TR::ResolvedMethodSymbol *bodySymbol = _comp->getJittedMethodSymbol();
   TR::SymbolReference    *sofRef     = _comp->getSymRefTab()->element(TR_stackOverflow);

   bool saveLR = (_cg->getSnippetList().size() <= 1 &&
                  !bodySymbol->isEHAware()                     &&
                  !_cg->machine()->getLinkRegisterKilled()) ? true : false;
   const TR::ARMLinkageProperties &linkage = _cg->getLinkage()->getProperties();
   uint32_t frameSize = _cg->getFrameSizeInBytes();
   int32_t offset = linkage.getOffsetToFirstLocal();
   uint32_t base, rotate;

   if (constantIsImmed8r(frameSize, &base, &rotate))
      {
      // mov R11, frameSize
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "mov\tgr11, #0x%08x", frameSize);
      bufferPos += 4;
      }
   else if (!constantIsImmed8r(frameSize-offset, &base, &rotate) && constantIsImmed8r(-offset, &base, &rotate))
      {
      // sub R11, R11, -offset
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "sub\tgr11, gr11, #0x%08x" , -offset);
      bufferPos += 4;
      }
   else
      {
      // R11 <- frameSize
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "mov\tgr11, #0x%08x", frameSize & 0xFF);
      bufferPos += 4;
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "add\tgr11, gr11, #0x%08x" , (frameSize >> 8 & 0xFF) << 8);
      bufferPos += 4;
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "add\tgr11, gr11, #0x%08x" , (frameSize >> 16 & 0xFF) << 16);
      bufferPos += 4;
      // There is no need in another add, since a stack frame can never be that big.
      }

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "add\tgr7, gr7, gr11");
   bufferPos += 4;
   if (saveLR)
      {
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "str\t[gr7, +0], gr14");
      bufferPos += 4;
      }

   printARMHelperBranch(sofRef, bufferPos, pOutFile);
   bufferPos += 4;

   if (saveLR)
      {
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "ldr\tgr14, [gr7, +0]");
      bufferPos += 4;
      }
   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "sub\tgr7, gr7, gr11");
   bufferPos += 4;
   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "b\t");
   print(pOutFile, snippet->getReStartLabel());
   }

#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::print(TR::FILE *pOutFile, TR::UnresolvedDataSnippet * snippet)
   {
   TR::MemoryReference *mr     = snippet->getMemoryReference();
   TR::SymbolReference    *symRef = snippet->getDataSymbolReference();

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(symRef));

   printARMHelperBranch(_cg->getSymRef(snippet->getHelper()), bufferPos, pOutFile);
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; return address", snippet->getAddressOfDataReference());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; cpIndex", symRef->getCPIndex());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t" POINTER_PRINTF_FORMAT "\t\t; cpAddress", getOwningMethod(symRef)->constantPool());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0x%08x\t\t; memory reference offset",
                 symRef->getSymbol()->isConstObjectRef() ? 0 : mr->getOffset());
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "dd\t0xe3a0%x000\t\t; instruction template",
                 toRealRegister(mr->getModBase())->getRegisterNumber() - TR::RealRegister::FirstGPR);
   }
#endif

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARMRecompilationSnippet * snippet)
   {
#ifdef J9_PROJECT_SPECIFIC
   uint8_t             *cursor        = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Counting Recompilation Snippet");

   TR::SymbolReference *symRef = _cg->getSymRef(TR_ARMcountingRecompileMethod);
   printARMHelperBranch(symRef, cursor, pOutFile);
   cursor += 4;

   // methodInfo
   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "dd \t0x%08x\t\t;%s", _comp->getRecompilationInfo()->getMethodInfo(), "methodInfo");
   cursor += 4;

   // startPC
   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "dd \t0x%08x\t\t; startPC ", _cg->getCodeStart());
#endif
   }

void
TR_Debug::printARMDelayedOffsetInstructions(TR::FILE *pOutFile, TR::ARMMemInstruction *instr)
   {
   bool regSpilled;
   uint8_t *bufferPos   = instr->getBinaryEncoding();
   int32_t offset       = instr->getMemoryReference()->getOffset();
   TR_ARMOpCodes op     = instr->getOpCodeValue();
   TR::RealRegister *base = toRealRegister(instr->getMemoryReference()->getBaseRegister());

   intParts localVal(offset);
   char *regName = (char *)_comp->trMemory()->allocateHeapMemory(6);
   sprintf(regName,"gr%d",(*(uint32_t *)bufferPos >> TR::RealRegister::pos_RD) & 0xf);

   if(op == ARMOp_str || op == ARMOp_strh || op == ARMOp_strb ||
      toRealRegister(instr->getMemoryDataRegister())->getRegisterNumber() == base->getRegisterNumber())
      {
      regSpilled = true;
      printPrefix(pOutFile, instr, bufferPos, 4);
      trfprintf(pOutFile,"str\t[gr7, -4], %s",regName);
      bufferPos+=4;
      }
   else
      {
      regSpilled = false;
      }

   printPrefix(pOutFile, instr, bufferPos, 4);
   trfprintf(pOutFile,"mov\t%s, 0x%08x (0x%x << 24)",regName,localVal.getByte3() << 24,localVal.getByte3());
   bufferPos+=4;

   printPrefix(pOutFile, instr, bufferPos, 4);
   trfprintf(pOutFile,"add\t%s, %s, 0x%08x (0x%x << 16)",regName, regName, localVal.getByte2() << 16,localVal.getByte2());
   bufferPos+=4;

   printPrefix(pOutFile, instr, bufferPos, 4);
   trfprintf(pOutFile,"add\t%s, %s, 0x%08x (0x%x << 8)",regName, regName, localVal.getByte1() << 8, localVal.getByte1());
   bufferPos+=4;

   printPrefix(pOutFile, instr, bufferPos, 4);
   trfprintf(pOutFile,"add\t%s, %s, 0x%08x",regName, regName, localVal.getByte0());
   bufferPos+=4;

   printPrefix(pOutFile, instr, bufferPos, 4);
   trfprintf(pOutFile, "%s\t", fullOpCodeName(instr));
   print(pOutFile, instr->getTargetRegister());
   trfprintf(pOutFile, ", ");
   print(pOutFile, instr->getMemoryReference());
   bufferPos+=4;

   if(regSpilled)
      {
      printPrefix(pOutFile, instr, bufferPos, 4);
      trfprintf(pOutFile,"ldr\t%s, [gr7, -4]",regName);
      bufferPos+=4;
      }

   dumpDependencies(pOutFile, instr);
   trfflush(pOutFile);

   return;
   }

static const char * opCodeToNameMap[] =
   {
   "bad",
   "add",
   "add_r",
   "adc",
   "adc_r",
   "and",
   "and_r",
   "b",
   "bl",
   "bic",
   "bic_r",
   "cmp",
   "cmn",
   "eor",
   "eor_r",
   "ldfs",
   "ldfd",
   "ldm",
   "ldmia",
   "ldr",
   "ldrb",
   "ldrsb",
   "ldrh",
   "ldrsh",
   "mul",
   "mul_r",
   "mla",
   "mla_r",
   "smull",
   "smull_r",
   "umull",
   "umull_r",
   "smlal",
   "smlal_r",
   "umlal",
   "umlal_r",
   "mov",
   "mov_r",
   "mvn",
   "mvn_r",
   "orr",
   "orr_r",
   "teq",
   "tst",
   "sub",
   "sub_r",
   "sbc",
   "sbc_r",
   "rsb",
   "rsb_r",
   "rsc",
   "rsc_r",
   "stfs",
   "stfd",
   "str",
   "strb",
   "strh",
   "stm",
   "stmdb",
   "swp",
   "sxtb",
   "sxth",
   "uxtb",
   "uxth",
   "fence",
   "ret",
   "wrtbar",
   "proc",
   "dd",
   "dmb_v6",
   "dmb",
   "dmb_st",
   "ldrex",
   "strex",
   "iflong",
   "setblong",
   "setbool",
   "setbflt",
   "lcmp",
   "flcmpl",
   "flcmpg",
   "idiv",
   "irem",
   "label",
   "vgdnop",
   "nop",
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
#if defined(__ARM_ARCH_7A__)
   "vabs",  // fabsd (vabs<c>.f64)
   "vabs",  // fabss (vabs<c>.f32)
   "vadd",  // faddd (vadd<c>.f64)
   "vadd",  // fadds (vadd<c>.f32)
   "vcmp",  // fcmpd (vcmp<c>.f64)
   "vcmp",  // fcmps (vcmp<c>.f32)
   "vcmp",  // fcmpzd (vcmp<c>.f64)
   "vcmp",  // fcmpzs (vcmp<c>.f32)
   "vmov",  // fcpyd (vmov<c>.f64)
   "vmov",  // fcpys (vmov<c>.f32)
   "vcvt",  // fcvtds (vcvt<c>.f64.f32)
   "vcvt",  // fcvtsd (vcvt<c>.f32.f64)
   "vdiv",  // fdivd (vdiv<c>.f64)
   "vdiv",  // fdivs (vdiv<c>.f32)
   "vldr",  // fldd  (vldr)
   "vldm",  // fldmd (vldm)
   "vldm",  // fldms (vldm)
   "vldr",  // flds (vldr)
   "vmla",  // fmacd (vmla.f64)
   "vmla",  // fmacs (vmla.f32)
   "vmov",  // fmdrr (vmov)
   "vmov",  // fmrrd (vmov)
   "vmov",  // fmrrs (vmov)
   "vmov",  // fmrs (vmov)
   "vmrs",  // fmrx (vmrs)
   "vmls",  // fmscd (vmls.f64)
   "vmls",  // fmscs (vmls.f32)
   "vmov",  // fmsr (vmov)
   "vmov",  // fmsrr (vmov)
   "vmrs",  // fmstat (vmrs APSR_nzcv, FPSCR)
   "vmul",  // fmuld (vmul.f64)
   "vmul",  // fmuls (vmul.f32)
   "vmsr",  // fmxr (vmsr)
   "vneg",  // fnegd (vneg.f64)
   "vneg",  // fnegs (vneg.f32)
   "vnmla", // fnmacd (vnmla.f64)
   "vnmla", // fnmacs (vnmla.f32)
   "vnmls", // fnmscd (vnmls.f64)
   "vnmls", // fnmscs (vnmls.f32)
   "vnmul", // fnmuld (vnmul.f64)
   "vnmul", // fnmuls (vnmul.f32)
   "vcvt",  // fsitod (vcvt.f64.s32)
   "vcvt",  // fsitos (vcvt.f32.s32)
   "vsqrt", // fsqrtd (vsqrt.f64)
   "vsqrt", // fsqrts (vsqrt.f32)
   "vstr",  // fstd (vstr)
   "vstm",  // fstmd (vstm)
   "vstm",  // fstms (vstm)
   "vstr",  // fsts (vstr)
   "vsub",  // fsubd (vsub.f64)
   "vsub",  // fsubs (vsub.f32)
   "vcvt",  // ftosizd (vcvt.s32.f64)
   "vcvt",  // ftosizs (vcvt.s32.f32)
   "vcvt",  // ftouizd (vcvt.u32.f64)
   "vcvt",  // ftouizs (vcvt.u32.f32)
   "vcvt",  // fuitod (vcvt.f64,u32)
   "vcvt"   // fuitos (vcvt.f32.u32)
#else
   "fabsd",  // fabsd (vabs<c>.f64)
   "fabss",  // fabss (vabs<c>.f32)
   "faddd",  // faddd (vadd<c>.f64)
   "fadds",  // fadds (vadd<c>.f32)
   "fcmpd",  // fcmpd (vcmp<c>.f64)
   "fcmps",  // fcmps (vcmp<c>.f32)
   "fcmpzd", // fcmpzd (vcmp<c>.f64)
   "fcmpzs", // fcmpzs (vcmp<c>.f32)
   "fcpyd",  // fcpyd (vmov<c>.f64)
   "fcpys",  // fcpys (vmov<c>.f32)
   "fcvtds", // fcvtds (vcvt<c>.f64.f32)
   "fcvtsd", // fcvtsd (vcvt<c>.f32.f64)
   "fdivd",  // fdivd (vdiv<c>.f64)
   "fdivs",  // fdivs (vdiv<c>.f32)
   "fldd",   // fldd  (vldr)
   "fldmd",  // fldmd (vldm)
   "fldms",  // fldms (vldm)
   "flds",   // flds (vldr)
   "fmacd",  // fmacd (vmla.f64)
   "fmacs",  // fmacs (vmla.f32)
   "fmdrr",  // fmdrr (vmov)
   "fmrrd",  // fmrrd (vmov)
   "fmrrs",  // fmrrs (vmov)
   "fmrs",   // fmrs (vmov)
   "fmrx",   // fmrx (vmrs)
   "fmscd",  // fmscd (vmls.f64)
   "fmscs",  // fmscs (vmls.f32)
   "fmsr",   // fmsr (vmov)
   "fmsrr",  // fmsrr (vmov)
   "fmstat", // fmstat (vmrs APSR_nzcv, FPSCR)
   "fmuld",  // fmuld (vmul.f64)
   "fmuls",  // fmuls (vmul.f32)
   "fmxr",   // fmxr (vmsr)
   "fnegd",  // fnegd (vneg.f64)
   "fnegs",  // fnegs (vneg.f32)
   "fnmacd", // fnmacd (vnmla.f64)
   "fnmacs", // fnmacs (vnmla.f32)
   "fnmscd", // fnmscd (vnmls.f64)
   "fnmscs", // fnmscs (vnmls.f32)
   "fnmuld", // fnmuld (vnmul.f64)
   "fnmuls", // fnmuls (vnmul.f32)
   "fsitod", // fsitod (vcvt.f64.s32)
   "fsitos", // fsitos (vcvt.f32.s32)
   "fsqrtd", // fsqrtd (vsqrt.f64)
   "fsqrts", // fsqrts (vsqrt.f32)
   "fstd",   // fstd (vstr)
   "fstmd",  // fstmd (vstm)
   "fstms",  // fstms (vstm)
   "fsts",   // fsts (vstr)
   "fsubd",  // fsubd (vsub.f64)
   "fsubs",  // fsubs (vsub.f32)
   "ftosizd",// ftosizd (vcvt.s32.f64)
   "ftosizs",// ftosizs (vcvt.s32.f32)
   "ftouizd",// ftouizd (vcvt.u32.f64)
   "ftouizs",// ftouizs (vcvt.u32.f32)
   "fuitod", // fuitod (vcvt.f64,u32)
   "fuitos"  // fuitos (vcvt.f32.u32)
#endif
#endif
   };

const char *
TR_Debug::getOpCodeName(TR_ARMOpCode * opCode)
   {
   return opCodeToNameMap[opCode->getOpCodeValue()];
   }

#endif
