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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if defined (_MSC_VER) && (_MSC_VER < 1900)
#define snprintf _snprintf_s
#endif

#include <limits.h>                                // for INT_MIN
#include <stdint.h>                                // for int32_t, uint8_t, etc
#include <stdio.h>                                 // for sprintf, snprintf
#include <string.h>                                // for strcmp, NULL, etc
#include "codegen/CodeGenPhase.hpp"                // for CodeGenPhase, etc
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator, etc
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                    // for TR_FrontEnd
#include "codegen/GCRegisterMap.hpp"               // for GCRegisterMap
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/Instruction.hpp"                 // for Instruction, etc
#include "codegen/Linkage.hpp"                     // for Linkage
#include "codegen/Machine.hpp"                     // for Machine
#include "codegen/MemoryReference.hpp"             // for MemoryReference
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"                // for RegisterPair
#include "codegen/Snippet.hpp"                     // for Snippet
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"                 // for Compilation, comp
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"             // for TR::Options, etc
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                              // for IO
#include "env/TRMemory.hpp"                        // for TR_Memory, etc
#include "env/defines.h"                           // for TR_HOST_X86
#include "env/jittypes.h"                          // for intptrj_t
#include "il/DataTypes.hpp"                        // for DataTypes, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"                     // for Node::getDataType, etc
#include "il/Symbol.hpp"                           // for Symbol, etc
#include "il/SymbolReference.hpp"                  // for SymbolReference
#include "il/symbol/AutomaticSymbol.hpp"           // for AutomaticSymbol
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "il/symbol/MethodSymbol.hpp"              // for MethodSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/List.hpp"                          // for ListIterator, etc
#include "infra/SimpleRegex.hpp"
#include "ras/Debug.hpp"                           // for TR_Debug, etc
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"


namespace TR { class Block; }

#if defined (J9ZOS390)
#define GPR_EP " " GPR15 " "
#else
#define GPR_EP " " GPR4 " "
#endif

#define PRINT_START_COMMENT         true
#define DO_NOT_PRINT_START_COMMENT  false

#define OPCODE_SPACING           8

extern const char *BranchConditionToNameMap[];
extern const char *BranchConditionToNameMap_ForListing[];

/** Need to use this since xlc doesn't seem to understand %hx modifier for fprintf */
#define maskHalf(val) (0x0000FFFF & (val))

/**
 * Given a value, return a string in binary, max bits = 64
 */
static char *binary_string(int numbits, int val)
   {
   static char b[64];  int z=0,i=0;

   if (numbits > 64) numbits=64;
   memset(b,0,64);
   z = 1 << (numbits-1);
   for (i=0 ; z > 0; i++)
     {
     b[i] =  ((val & z) == z) ? '1' : '0';
     z >>= 1;
     }
   return b;
   }

void
TR_Debug::printPrefix(TR::FILE *pOutFile, TR::Instruction * instr)
   {

   if (pOutFile == NULL)
      {
      return;
      }

   printPrefix(pOutFile, instr, instr->getBinaryEncoding(), instr->getBinaryLength());

   if (_comp->cg()->traceBCDCodeGen())
      {
#ifdef J9_PROJECT_SPECIFIC
      if (instr->getNode() && (instr->getNode()->getOpCode().isBinaryCodedDecimalOp()) &&
          (instr->getOpCodeValue() == TR::InstOpCode::CVB || instr->getOpCodeValue() == TR::InstOpCode::CVBG ||
           instr->getOpCodeValue() == TR::InstOpCode::CVD || instr->getOpCodeValue() == TR::InstOpCode::CVDG ||
           instr->getOpCodeValue() == TR::InstOpCode::OI  || instr->getOpCodeValue() == TR::InstOpCode::NI || instr->getOpCodeValue() == TR::InstOpCode::MVI ||
           instr->getKind() == TR::Instruction::IsSIL || instr->getKind() == TR::Instruction::IsSI || instr->getKind() == TR::Instruction::IsSIY ||
           instr->getKind() == TR::Instruction::IsSS1 || instr->getKind() == TR::Instruction::IsSS2))
         {
         if (instr->getOpCodeValue() == TR::InstOpCode::CVB || instr->getOpCodeValue() == TR::InstOpCode::CVBG)
            {
            trfprintf(pOutFile, "               , #%d  ", instr->getMemoryReference()->getSymbolReference()->getReferenceNumber());
            }
         else
            {
            if (instr->getNode() && instr->getNode()->getOpCodeValue() != TR::BBStart && instr->getRegisterOperand(1))
               trfprintf(pOutFile, "#%d (%s)", instr->getMemoryReference()->getSymbolReference()->getReferenceNumber(),instr->getRegisterOperand(1)->getRegisterName(TR::comp()));
            else
               trfprintf(pOutFile, "#%d           ", instr->getMemoryReference()->getSymbolReference()->getReferenceNumber());

            if (instr->getKind() == TR::Instruction::IsSS1 || instr->getKind() == TR::Instruction::IsSS2)
               {
               TR::MemoryReference *memRef = (instr->getKind() == TR::Instruction::IsSS1) ?
                                                   ((TR::S390SS1Instruction *)(instr))->getMemoryReference2() :
                                                   ((TR::S390SS2Instruction *)(instr))->getMemoryReference2();
               if (memRef && memRef->getSymbolReference() && instr->getNode()->getOpCodeValue() != TR::BBStart && instr->getRegisterOperand(0))
                  trfprintf(pOutFile, ", #%d (%s)", memRef->getSymbolReference()->getReferenceNumber(), instr->getRegisterOperand(0)->getRegisterName(TR::comp()));
               else if (memRef && memRef->getSymbolReference())
                  trfprintf(pOutFile, ", #%d  ", memRef->getSymbolReference()->getReferenceNumber());
               else
                  trfprintf(pOutFile, "        ");
               }
            else
               {
               trfprintf(pOutFile, "        ");
               }
            }
         }
      else
#endif
         {
         trfprintf(pOutFile, "                       ");
         }
      }
   }


void
TR_Debug::printz(TR::FILE *pOutFile, TR::Instruction * instr, const char *title)
   {
   printz(pOutFile, instr);
   }

void
TR_Debug::printz(TR::FILE *pOutFile, TR::Instruction * instr)
   {

   if (pOutFile == NULL)
      {
      return;
      }

   //  dump the inst's pre deps
   if (instr->getOpCodeValue() != TR::InstOpCode::ASSOCREGS && _comp->cg()->getCodeGeneratorPhase() <= TR::CodeGenPhase::BinaryEncodingPhase)
      dumpDependencies(pOutFile, instr, true, false);

   switch (instr->getKind())
      {
      case TR::Instruction::IsLabel:
         print(pOutFile, (TR::S390LabelInstruction *) instr);
         break;
      case TR::Instruction::IsBranch:
         print(pOutFile, (TR::S390BranchInstruction *) instr);
         break;
      case TR::Instruction::IsBranchOnCount:
         print(pOutFile, (TR::S390BranchOnCountInstruction *) instr);
         break;
      case TR::Instruction::IsBranchOnIndex:
         print(pOutFile, (TR::S390BranchOnIndexInstruction *) instr);
         break;
      case TR::Instruction::IsImm:
         print(pOutFile, (TR::S390ImmInstruction *) instr);
         break;
      case TR::Instruction::IsImmSnippet:
         print(pOutFile, (TR::S390ImmSnippetInstruction *) instr);
         break;
      case TR::Instruction::IsImmSym:
         print(pOutFile, (TR::S390ImmSymInstruction *) instr);
         break;
      case TR::Instruction::IsImm2Byte:
         print(pOutFile, (TR::S390Imm2Instruction *) instr);
         break;
      case TR::Instruction::IsReg:
         print(pOutFile, (TR::S390RegInstruction *) instr);
         break;
      case TR::Instruction::IsRR:
         print(pOutFile, (TR::S390RRInstruction *) instr);
         break;
      case TR::Instruction::IsRRE:
         {
         TR::InstOpCode::Mnemonic opCode = instr->getOpCodeValue();
         if (opCode == TR::InstOpCode::TROO || opCode == TR::InstOpCode::TRTO || opCode == TR::InstOpCode::TROT || opCode == TR::InstOpCode::TRTT)
            print(pOutFile, (TR::S390TranslateInstruction *) instr);
         else
            print(pOutFile, (TR::S390RRInstruction *) instr);
         break;
         }
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
      case TR::Instruction::IsRRF2:
      case TR::Instruction::IsRRF3:
      case TR::Instruction::IsRRF4:
      case TR::Instruction::IsRRF5:
         print(pOutFile, (TR::S390RRFInstruction *) instr);
         break;
      case TR::Instruction::IsRRR:
         print(pOutFile, (TR::S390RRRInstruction *) instr);
         break;
      case TR::Instruction::IsRI:
         print(pOutFile, (TR::S390RIInstruction *) instr);
         break;
      case TR::Instruction::IsRIL:
         print(pOutFile, (TR::S390RILInstruction *) instr);
         break;
      case TR::Instruction::IsRS:
         print(pOutFile, (TR::S390RSInstruction *) instr);
         break;
      case TR::Instruction::IsRSL:
         print(pOutFile, (TR::S390RSLInstruction *) instr);
         break;
      case TR::Instruction::IsRSLb:
         print(pOutFile, (TR::S390RSLbInstruction *) instr);
         break;
      case TR::Instruction::IsRSY:
         print(pOutFile, (TR::S390RSInstruction *) instr);
         break;
      case TR::Instruction::IsRX:
         print(pOutFile, (TR::S390RXInstruction *) instr);
         break;
      case TR::Instruction::IsRXE:
         print(pOutFile, (TR::S390RXEInstruction *) instr);
         break;
      case TR::Instruction::IsRXY:
         print(pOutFile, (TR::S390RXInstruction *) instr);
         break;
      case TR::Instruction::IsRXYb:
         print(pOutFile, (TR::S390MemInstruction *) instr);
         break;
      case TR::Instruction::IsRXF:
         print(pOutFile, (TR::S390RXFInstruction *) instr);
         break;
      case TR::Instruction::IsSMI:
         print(pOutFile, (TR::S390SMIInstruction *) instr);
         break;
      case TR::Instruction::IsMII:
         print(pOutFile, (TR::S390MIIInstruction *) instr);
         break;
      case TR::Instruction::IsMem:
         print(pOutFile, (TR::S390MemInstruction *) instr);
         break;
      case TR::Instruction::IsSS1:
         print(pOutFile, (TR::S390SS1Instruction *) instr);
         break;
      case TR::Instruction::IsSS2:
         print(pOutFile, (TR::S390SS2Instruction *) instr);
         break;
      case TR::Instruction::IsSS4:
         print(pOutFile, (TR::S390SS4Instruction *) instr);
         break;
      case TR::Instruction::IsSSF:
         print(pOutFile, (TR::S390SSFInstruction *) instr);
         break;
      case TR::Instruction::IsSI:
      case TR::Instruction::IsSIY:
         print(pOutFile, (TR::S390SIInstruction *) instr);
         break;
      case TR::Instruction::IsSIL:
         print(pOutFile, (TR::S390SILInstruction *) instr);
         break;
      case TR::Instruction::IsS:
         print(pOutFile, (TR::S390SInstruction *) instr);
         break;
      case TR::Instruction::IsNOP:
         print(pOutFile, (TR::S390NOPInstruction *) instr);
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::Instruction::IsVirtualGuardNOP:
         print(pOutFile, (TR::S390VirtualGuardNOPInstruction *) instr);
         break;
#endif
      case TR::Instruction::IsAnnot:
         print(pOutFile, (TR::S390AnnotationInstruction *) instr);
         break;
      case TR::Instruction::IsPseudo:
            print(pOutFile, (TR::S390PseudoInstruction *) instr);
         break;
      case TR::Instruction::IsRRS:
            print(pOutFile, (TR::S390RRSInstruction *) instr);
         break;
      case TR::Instruction::IsRIE:
            print(pOutFile, (TR::S390RIEInstruction *) instr);
         break;
      case TR::Instruction::IsRIS:
            print(pOutFile, (TR::S390RISInstruction *) instr);
         break;
      case TR::Instruction::IsOpCodeOnly:
            print(pOutFile, (TR::S390OpCodeOnlyInstruction *) instr);
         break;
      case TR::Instruction::IsI:
            print(pOutFile, (TR::S390IInstruction *) instr);
         break;
      case TR::Instruction::IsSSE:
            print(pOutFile, (TR::S390SSEInstruction *) instr);
         break;
      case TR::Instruction::IsIE:
            print(pOutFile, (TR::S390IEInstruction *) instr);
         break;
      case TR::Instruction::IsVRIa:
      case TR::Instruction::IsVRIb:
      case TR::Instruction::IsVRIc:
      case TR::Instruction::IsVRId:
      case TR::Instruction::IsVRIe:
      case TR::Instruction::IsVRIf:
      case TR::Instruction::IsVRIg:
      case TR::Instruction::IsVRIh:
      case TR::Instruction::IsVRIi:
            print(pOutFile, (TR::S390VRIInstruction *) instr);
         break;
      case TR::Instruction::IsVRRa:
      case TR::Instruction::IsVRRb:
      case TR::Instruction::IsVRRc:
      case TR::Instruction::IsVRRd:
      case TR::Instruction::IsVRRe:
      case TR::Instruction::IsVRRf:
      case TR::Instruction::IsVRRg:
      case TR::Instruction::IsVRRh:
      case TR::Instruction::IsVRRi:
            print(pOutFile, (TR::S390VRRInstruction *) instr);
         break;
      case TR::Instruction::IsVRSa:
      case TR::Instruction::IsVRSb:
      case TR::Instruction::IsVRSc:
      case TR::Instruction::IsVRSd:
      case TR::Instruction::IsVRV:
      case TR::Instruction::IsVRX:
      case TR::Instruction::IsVSI:
            print(pOutFile, (TR::S390VStorageInstruction *) instr);
         break;
      default:
         TR_ASSERT( 0, "unexpected instruction kind");
         // fall thru
      case TR::Instruction::IsNotExtended:
      case TR::Instruction::IsE:
         {
         // ASSOCREGS piggy backs on a vanilla TR::Instruction
         // if (instr->getOpCodeValue() == TR::InstOpCode::ASSOCREGS) break;

         if ((instr->getOpCodeValue() == TR::InstOpCode::ASSOCREGS) && /*(debug("traceMsg90RA"))*/
             (_comp->getOptions()->getRegisterAssignmentTraceOption(TR_TraceRABasic)))
            {
            if (_comp->cg()->getCodeGeneratorPhase() < TR::CodeGenPhase::BinaryEncodingPhase)
               printAssocRegDirective(pOutFile, instr);
            }
         else
            {
            printPrefix(pOutFile, instr);
            trfprintf(pOutFile, "%s", getOpCodeName(&instr->getOpCode()));
            trfflush(pOutFile);
            }
         }
      }

   //  dump the inst's post deps
   if (instr->getOpCodeValue() != TR::InstOpCode::ASSOCREGS && _comp->cg()->getCodeGeneratorPhase() <= TR::CodeGenPhase::BinaryEncodingPhase)
      dumpDependencies(pOutFile, instr, false, true);

   if(instr->isStartInternalControlFlow())
      {
      trfprintf(pOutFile, "\t# (Start of internal control flow)");
      }
   if(instr->isEndInternalControlFlow())
      {
      trfprintf(pOutFile, "\t# (End of internal control flow)");
      }

   }

TR::Instruction*
TR_Debug::getOutlinedTargetIfAny(TR::Instruction *instr)
   {
   TR::LabelSymbol *label;

   switch (instr->getKind())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::Instruction::IsVirtualGuardNOP:
         label = ((TR::S390VirtualGuardNOPInstruction *)instr)->getLabelSymbol();
         break;
#endif
      case TR::Instruction::IsBranch:
         label = ((TR::S390BranchInstruction *)instr)->getLabelSymbol();
         break;
      case TR::Instruction::IsRIE:
         label = ((TR::S390RIEInstruction *)instr)->getLabelSymbol();
         break;
      default:
         return NULL;
      }

   if (!label || !label->isStartOfColdInstructionStream())
      return NULL;

   return label->getInstruction();
   }

void TR_Debug::printS390OOLSequences(TR::FILE *pOutFile)
   {
   auto oiIterator = _cg->getS390OutOfLineCodeSectionList().begin();

   while (oiIterator != _cg->getS390OutOfLineCodeSectionList().end())
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
TR_Debug::dumpDependencies(TR::FILE *pOutFile, TR::Instruction * instr, bool pre, bool post)
   {

   TR::RegisterDependencyConditions *deps = instr->getDependencyConditions();

   if (pOutFile == NULL || !deps || _comp->getOption(TR_DisableTraceRegDeps))
      return;

   if (pre)
      {
      if (deps->getNumPreConditions() > 0)
         {
         trfprintf(pOutFile, "\n  PRE:");
         printRegisterDependencies(pOutFile, deps->getPreConditions(), deps->getNumPreConditions());
         }
      }

   if (post)
      {
      if (deps->getNumPostConditions() > 0)
         {
         trfprintf(pOutFile, "\n POST:");
         printRegisterDependencies(pOutFile, deps->getPostConditions(), deps->getNumPostConditions());
         }
      }

   trfflush(pOutFile);
   }

void
TR_Debug::printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction *instr, bool needsStartComment)
   {
   while (tabStops-- > 0)
      {
      trfprintf(pOutFile, "\t");
      }

   dumpInstructionComments(pOutFile, instr, needsStartComment);
   }

void
TR_Debug::printAssocRegDirective(TR::FILE *pOutFile, TR::Instruction * instr)
   {
   TR_S390RegisterDependencyGroup * depGroup = instr->getDependencyConditions()->getPostConditions();

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%s", getOpCodeName(&instr->getOpCode()));
   trfflush(pOutFile);

   int first = TR::RealRegister::FirstGPR;
   int last = TR::RealRegister::LastFPR;

   for (int j = 0; j < last; ++j)
      {
      TR::RegisterDependency * dependency = depGroup->getRegisterDependency(j);
      if ((intptr_t) dependency->getRegister(_comp->cg()) > 0)
         {
         TR::Register * virtReg = dependency->getRegister(_comp->cg());
         printS390RegisterDependency(pOutFile, virtReg, j+1, dependency->getRefsRegister(), dependency->getDefsRegister());
         }
      }

   if (0 && !_comp->getOption(TR_DisableHighWordRA))
      {
      // 16 HPRs
      for (int j = 0; j <= TR::RealRegister::LastHPR - TR::RealRegister::FirstHPR; ++j)
         {
         TR::RegisterDependency * dependency = depGroup->getRegisterDependency(j+last);
         if ((intptr_t) dependency->getRegister(_comp->cg()) > 0)
            {
            TR::Register * virtReg = dependency->getRegister(_comp->cg());
            printS390RegisterDependency(pOutFile, virtReg, j+TR::RealRegister::FirstHPR, dependency->getRefsRegister(), dependency->getDefsRegister());
            }
         }
      }
   //  trfprintf(pOutFile,"\n");

   trfflush(pOutFile);
   }



void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RRSInstruction * instr)
   {

   // Prints RRS format in "Opcode  R1,R2,D4(B4) (mask=M3)"

   // print the line prefix.
   printPrefix(pOutFile, instr);

   // print the opcode.
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // grab the registers.
   TR::Register * targetRegister = instr->getRegisterOperand(1);
   TR::Register * sourceRegister = instr->getRegisterOperand(2);

   // print the registers
   print(pOutFile, targetRegister);
   trfprintf(pOutFile, ",");
   print(pOutFile, sourceRegister);
   trfprintf(pOutFile, ",");

   // print the branch destination memref
   print(pOutFile, instr->getBranchDestinationLabel(), instr);

   // finally, print the branch mask.
   TR::InstOpCode::S390BranchCondition cond = instr->getBranchCondition();

   const char * brCondName;
   uint8_t mask = instr->getMask(); mask >>= 4;
   brCondName = BranchConditionToNameMap[cond];

   trfprintf(pOutFile, "%s(mask=0x%1x), ", brCondName, mask );
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390IEInstruction * instr)
   {
   if (pOutFile == NULL)
      {
      return;
      }
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s%d,%d", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()), instr->getImmediateField1(), instr->getImmediateField2());

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

/**
 * Prints RIE format, either:
 *   "Opcode  R1,R2,imm (mask=)"
 *   "Opcode  R1,imm,I2 (mask=)"
 *   "Opcode  R1,I2 (mask=)"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RIEInstruction * instr)
   {



   // let's determine what form of RIE we are dealing with
   bool RIE1 = (instr->getRieForm() == TR::S390RIEInstruction::RIE_RR);
   bool RIE2 = (instr->getRieForm() == TR::S390RIEInstruction::RIE_RI8);
   bool RIE3 = (instr->getRieForm() == TR::S390RIEInstruction::RIE_RI16A);
   bool RIE4 = (instr->getRieForm() == TR::S390RIEInstruction::RIE_RRI16);
   bool RIE5 = (instr->getRieForm() == TR::S390RIEInstruction::RIE_IMM);
   bool RIE6 = (instr->getRieForm() == TR::S390RIEInstruction::RIE_RI16G);

   // print the line prefix
   printPrefix(pOutFile, instr);

   // print the opcode
   if (RIE5 && instr->getExtendedHighWordOpCode().getOpCodeValue() != TR::InstOpCode::BAD)
      {
      // print extended-mnemonics for highword rotate instructions on zG
      trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getExtendedHighWordOpCode()));
      }
   else
      {
      trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
      }

   // grab the registers.
   TR::Register * targetRegister = instr->getRegisterOperand(1);
   TR::Register * sourceRegister = instr->getRegisterOperand(2);

   // we can print the first register since we should always have it
   print(pOutFile, targetRegister);
   trfprintf(pOutFile, ",");

   if(RIE1)
      {
      // we have the rightSide, so go ahead and print that now
      print(pOutFile, sourceRegister);
      trfprintf(pOutFile, ",");
      // we'll print the immedate branch info now
      print(pOutFile, instr->getBranchDestinationLabel());
      trfprintf(pOutFile, ",");
      }
   else if(RIE2)
      {
      // we'll print the immedate branch info now
      print(pOutFile, instr->getBranchDestinationLabel());
      trfprintf(pOutFile, ",");
      // print the immediate value
      trfprintf(pOutFile, "%d,", instr->getSourceImmediate8());
      }
   else if(RIE4)
      {
      print(pOutFile, sourceRegister);
      trfprintf(pOutFile, ",");
      // print the immediate value
      trfprintf(pOutFile, "%d,", instr->getSourceImmediate16());
      }
   else if(RIE5)
      {
      // print the source regiser (R2)
      print(pOutFile, sourceRegister);

      // do not print out the rest for highword extended-mnemonics
      if (instr->getExtendedHighWordOpCode().getOpCodeValue() == TR::InstOpCode::BAD)
         {
         trfprintf(pOutFile, ",");
         // print the immediate value
         trfprintf(pOutFile, "%u,", (uint8_t)instr->getSourceImmediate8One());
         // print the immediate value
         trfprintf(pOutFile, "%u,", (uint8_t)instr->getSourceImmediate8Two());
         }
      if (instr->getExtendedHighWordOpCode().getOpCodeValue() == TR::InstOpCode::SLLHH)
         {
         trfprintf(pOutFile, ",");
         // print the immediate value
         trfprintf(pOutFile, "%u", (uint8_t)instr->getSourceImmediate8());
         }
      if (instr->getExtendedHighWordOpCode().getOpCodeValue() == TR::InstOpCode::SLLLH)
         {
         trfprintf(pOutFile, ",");
         // print the immediate value
         trfprintf(pOutFile, "%u", (uint8_t)instr->getSourceImmediate8()-32);
         }
      if (instr->getExtendedHighWordOpCode().getOpCodeValue() == TR::InstOpCode::SRLHH ||
          instr->getExtendedHighWordOpCode().getOpCodeValue() == TR::InstOpCode::SRLLH)
         {
         trfprintf(pOutFile, ",");
         // print the immediate value
         trfprintf(pOutFile, "%u", (uint8_t)instr->getSourceImmediate8One());
         }
      }
   else
      {
      // print the immediate value
      trfprintf(pOutFile, "%d,", instr->getSourceImmediate16());
      }

   if (RIE5)
      {
      if (instr->getExtendedHighWordOpCode().getOpCodeValue() == TR::InstOpCode::BAD)
         {
         // print the immediate value
         trfprintf(pOutFile, "%u", (uint8_t)instr->getSourceImmediate8());
         }
      }
   else if(!RIE4)
      {
      // finally, print the branch mask.
      TR::InstOpCode::S390BranchCondition cond = instr->getBranchCondition();
      const char * brCondName;
      uint8_t mask = instr->getMask(); mask >>= 4;
      brCondName = BranchConditionToNameMap[cond];

      trfprintf(pOutFile, "%s(mask=0x%1x), ", brCondName, mask );
      }
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);

   }

/**
 * Prints RIS format in "Opcode  R1,D4(B4),I2 (mask=)"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RISInstruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // we can print the first register since we should always have it
   print(pOutFile, instr->getRegisterOperand(1));
   trfprintf(pOutFile, ",");


   // print the branch destination memref.
   print(pOutFile, instr->getMemoryReference(), instr);
   trfprintf(pOutFile, ",");

   // print the branch destination memref.
   trfprintf(pOutFile,"%d", instr->getSourceImmediate(), instr);

   // finally, print the branch mask.
   TR::InstOpCode::S390BranchCondition cond = instr->getBranchCondition();
   const char * brCondName;
   uint8_t mask = instr->getMask(); mask >>= 4;
   brCondName = BranchConditionToNameMap[cond];

   trfprintf(pOutFile, "%s(mask=0x%1x), ", brCondName, mask );
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390LabelInstruction * instr)
   {

   TR::LabelSymbol * label = instr->getLabelSymbol();
   const char * symbolName = getName(label);
   printPrefix(pOutFile, instr);

   if (instr->getOpCodeValue() == TR::InstOpCode::LABEL)
      {
         {
         trfprintf(pOutFile, symbolName);
         trfprintf(pOutFile, ":");
         }

      if (label->isStartInternalControlFlow())
         {
         trfprintf(pOutFile, "\t# (Start of internal control flow)");
         }
      else if (label->isEndInternalControlFlow())
         {
         trfprintf(pOutFile, "\t# (End of internal control flow)");
         }
      }
   else
      {
      trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
      if (instr->getCallSnippet())
         {
         print(pOutFile, instr->getCallSnippet()->getSnippetLabel());
         intptrj_t labelLoc = (intptrj_t) instr->getCallSnippet()->getSnippetLabel()->getCodeLocation();
         if (labelLoc)
            {
            trfprintf(pOutFile, ", labelTargetAddr=0x%p", labelLoc);
            }
         }
      else
         {
         print(pOutFile, instr->getLabelSymbol());
         intptrj_t labelLoc = (intptrj_t) instr->getLabelSymbol()->getCodeLocation();
         if (labelLoc)
            {
            trfprintf(pOutFile, ", labelTargetAddr=0x%p", labelLoc);
            }
         }
      }

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);

   trfflush(pOutFile);
   }

#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390VirtualGuardNOPInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   TR::LabelSymbol * label = instr->getLabelSymbol();
   if (instr->getNode()->isHCRGuard() && instr->getBinaryLength() == 0)
      trfprintf(pOutFile, "VGNOP (empty patch) \t");
   else
      trfprintf(pOutFile, "VGNOP \t");
   print(pOutFile, instr->getLabelSymbol());
   intptrj_t labelLoc = (intptrj_t) instr->getLabelSymbol()->getCodeLocation();
   if (labelLoc)
      {
      trfprintf(pOutFile, ", labelTargetAddr=0x%p", labelLoc);
      }
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);

   trfflush(pOutFile);
   }
#endif

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390BranchInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   TR::InstOpCode::S390BranchCondition cond = instr->getBranchCondition();
   const char * brCondName;
   uint8_t mask = instr->getMask(); mask >>= 4;
   brCondName = BranchConditionToNameMap[cond];


   TR::LabelSymbol * label = instr->getLabelSymbol();
   TR_ASSERT(instr->getOpCodeValue() != TR::InstOpCode::LABEL,  "assertion failure");

   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   trfprintf(pOutFile, "%s(0x%1x), ", brCondName, mask );

    if (instr->getCallSnippet())
       {
       print(pOutFile, instr->getCallSnippet()->getSnippetLabel());
       intptrj_t labelLoc = (intptrj_t) instr->getCallSnippet()->getSnippetLabel()->getCodeLocation();
       if (labelLoc)
          {
          trfprintf(pOutFile, ", labelTargetAddr=0x%p", labelLoc);
          }
       }
    else
       {
       print(pOutFile, instr->getLabelSymbol());
       intptrj_t labelLoc = (intptrj_t) instr->getLabelSymbol()->getCodeLocation();
       if (labelLoc)
          {
          trfprintf(pOutFile, ", labelTargetAddr=0x%p", labelLoc);
          }
       }

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390BranchOnCountInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   TR::LabelSymbol * label = instr->getLabelSymbol();
   TR_ASSERT(instr->getOpCodeValue() != TR::InstOpCode::LABEL, "assertion failure");
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getRegisterOperand(1));
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getLabelSymbol());
   intptrj_t labelLoc = (intptrj_t) instr->getLabelSymbol()->getCodeLocation();
   if (labelLoc)
      {
      trfprintf(pOutFile, ", labelTargetAddr=0x%p", labelLoc);
      }
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390BranchOnIndexInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   TR::LabelSymbol * label = instr->getLabelSymbol();
   TR_ASSERT(instr->getOpCodeValue() != TR::InstOpCode::LABEL, "assertion failure");
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getRegisterOperand(1));
   trfprintf(pOutFile, ",");
   TR::Register * sourceRegister = instr->getRegisterOperand(2);
   TR::RegisterPair * regPair = sourceRegister->getRegisterPair();
   if (regPair)
      {
      trfprintf(pOutFile, "(");
      print(pOutFile, sourceRegister->getHighOrder());
      trfprintf(pOutFile, ",");
      print(pOutFile, sourceRegister->getLowOrder());
      trfprintf(pOutFile, ")");
      }
   else
      {
      print(pOutFile, sourceRegister);
      }
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getLabelSymbol());
   intptrj_t labelLoc = (intptrj_t) instr->getLabelSymbol()->getCodeLocation();
   if (labelLoc)
      {
      trfprintf(pOutFile, ", labelTargetAddr=0x%p", labelLoc);
      }
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390AnnotationInstruction * instr)
   {
   if (pOutFile == NULL)
      {
      return;
      }

   trfprintf(pOutFile, instr->getAnnotation());
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390PseudoInstruction * instr)
   {
   if (pOutFile == NULL)
      {
      return;
      }

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   if (instr->getOpCodeValue() == TR::InstOpCode::DCB)
      {
      
      if (static_cast<TR::S390DebugCounterBumpInstruction*>(instr)->getAssignableReg())
         {
         print(pOutFile, static_cast<TR::S390DebugCounterBumpInstruction*>(instr)->getAssignableReg());
         }
      else
         {
         trfprintf(pOutFile, "Spill Reg");
         }
         
      trfprintf(pOutFile, ", Debug Counter Bump");
      }

   if (instr->getOpCodeValue() == TR::InstOpCode::FENCE)
      {
      if (instr->getFenceNode() != NULL)
         {
         if (instr->getFenceNode()->getRelocationType() == TR_AbsoluteAddress)
            {
            trfprintf(pOutFile, "Absolute [");
            }
         else if (instr->getFenceNode()->getRelocationType() == TR_ExternalAbsoluteAddress)
            {
            trfprintf(pOutFile, "External Absolute [");
            }
         else
            {
            trfprintf(pOutFile, "Relative [");
            }
         for (int32_t i = 0; i < instr->getFenceNode()->getNumRelocations(); ++i)
            {
            if (!_comp->getOption(TR_MaskAddresses))
               trfprintf(pOutFile, " %p", instr->getFenceNode()->getRelocationDestination(i));
            }
         trfprintf(pOutFile, " ]");

         printBlockInfo(pOutFile, instr->getNode());
         }
      else
         {
         if (instr->getNode()->getOpCodeValue() == TR::loadFence)
            {
            trfprintf(pOutFile, "Load Fence");
            }
         else if (instr->getNode()->getOpCodeValue() == TR::storeFence)
            {
            trfprintf(pOutFile, "Store Fence");
            }
         }
      }
#if TODO
   dumpDependencies(pOutFile, instr);
#endif
   if (instr->getOpCodeValue() == TR::InstOpCode::XPCALLDESC && instr->getCallDescLabel() != NULL)
      {
      trfprintf(pOutFile, " - BRC *+%d ; <%d-byte padding>", 12 + instr->getNumPadBytes(), instr->getNumPadBytes() / 2); // + instr->getPadBytes(), instr->getPadBytes());
      trfflush(pOutFile);

      trfprintf(pOutFile, "; DC <0x%llX>", instr->getCallDescValue());
      trfflush(pOutFile);
      }
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390ImmInstruction * instr)
   {
   if (pOutFile == NULL)
      {
      return;
      }
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s0x%08x", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()), instr->getSourceImmediate());

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390Imm2Instruction * instr)
   {
   if (pOutFile == NULL)
      {
      return;
      }
   printPrefix(pOutFile, instr);
   // DC looks better in the tracefile than DC2 does....
   trfprintf(pOutFile, "%-*s0x%04x", OPCODE_SPACING, "DC", instr->getSourceImmediate());
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390ImmSnippetInstruction * instr)
   {
   if (pOutFile == NULL)
      {
      return;
      }
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s0x%08x", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()), instr->getSourceImmediate());
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390ImmSymInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s0x%08x", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()), instr->getSourceImmediate());
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RegInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   if (instr->getOpCodeValue() == TR::InstOpCode::BCR)
      {
      TR::InstOpCode::S390BranchCondition cond = instr->getBranchCondition();
      uint8_t mask = instr->getMask(); mask >>= 4;
      const char * brCondName;
      brCondName = BranchConditionToNameMap[cond];
      trfprintf(pOutFile, "%s(mask=0x%1x), ", brCondName, mask );
      }

   TR::Register * targetRegister = instr->getRegisterOperand(1);
      TR::RegisterPair * regPair = targetRegister->getRegisterPair();
      if (regPair)
         {
         print(pOutFile, targetRegister->getHighOrder());
         }
      else
         {
         print(pOutFile, targetRegister);
         }

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RRInstruction * instr)
   {
   int32_t i=1;
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   if (instr->getFirstConstant() >=0)
      trfprintf(pOutFile, "%d", instr->getFirstConstant());
   else
      {
      TR::Register * targetRegister = instr->getRegisterOperand(i++);
      TR::RegisterPair * regPair = targetRegister->getRegisterPair();
      if (regPair)
         {
         print(pOutFile, targetRegister->getHighOrder());
         }
      else
         {
         print(pOutFile, targetRegister);
         }
      }

   if (instr->getSecondConstant() >=0)
      {
      trfprintf(pOutFile, ",");
      trfprintf(pOutFile, "%d", instr->getSecondConstant());
      }
   else
      {
      TR::Register * sourceRegister = instr->getRegisterOperand(i);
      if (sourceRegister != NULL)
         {
         trfprintf(pOutFile, ",");
         TR::RegisterPair *regPair = sourceRegister->getRegisterPair();
         if (regPair)
            {
            print(pOutFile, sourceRegister->getHighOrder());
            }
         else
            {
            print(pOutFile,sourceRegister);
            }
         }
      }
   if ((instr->getOpCodeValue() == TR::InstOpCode::BASR || instr->getOpCodeValue() == TR::InstOpCode::BRASL) &&
       instr->getNode() &&
       instr->getNode()->getOpCode().hasSymbolReference() &&
       instr->getNode()->getSymbolReference())
      {
      trfprintf(pOutFile, " \t\t# Call \"%s\"", getName(instr->getNode()->getSymbolReference()));
      }

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390TranslateInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   TR::Register * targetRegister = instr->getRegisterOperand(1);
   TR::RegisterPair * regPair = targetRegister->getRegisterPair();
   if (regPair)
      {
      print(pOutFile, targetRegister->getHighOrder());
      trfprintf(pOutFile, ":");
      print(pOutFile, targetRegister->getLowOrder());
      }
   else
      {
      print(pOutFile, targetRegister);
      }

   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getRegisterOperand(2));
   if (instr->isMaskPresent())
      {
      trfprintf(pOutFile, ",%04x", instr->getMask());
      }
   if (instr->getOpCodeValue() == TR::InstOpCode::BASR && instr->getNode()->getOpCode().hasSymbolReference())
      {
      trfprintf(pOutFile, " \t\t# Call \"%s\"", getName(instr->getNode()->getSymbolReference()));
      }
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RRFInstruction * instr)
   {
   printPrefix(pOutFile, instr);

      {
      trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
      }

   print(pOutFile, instr->getRegisterOperand(1));
   trfprintf(pOutFile, ",");

   if (instr->isSourceRegister2Present()) //RRF or RRF2
      {
      print(pOutFile, instr->getRegisterOperand(3));
      }
   else // Then mask must be present (RRF2)
      {
      trfprintf(pOutFile, "%p", instr->isMask3Present() ? instr->getMask3() : instr->getMask4());
      }
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getRegisterOperand(2));
   if ((instr->getRegisterOperand(3) != NULL) && instr->isMask3Present()) //RRF3
      {
      trfprintf(pOutFile, ",%p", instr->getMask3());
      }
   if (instr->getOpCodeValue() == TR::InstOpCode::BASR && instr->getNode()->getOpCode().hasSymbolReference())
      {
      trfprintf(pOutFile, " \t\t# Call \"%s\"", getName(instr->getNode()->getSymbolReference()));
      }
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RRRInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   print(pOutFile, instr->getRegisterOperand(1));
   trfprintf(pOutFile, ",");

   print(pOutFile, instr->getRegisterOperand(2));
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getRegisterOperand(3));
   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RIInstruction * instr)
   {
   int16_t imm = instr->getSourceImmediate();
   uint8_t *cursor = (uint8_t *)instr->getBinaryEncoding();

   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   if (instr->getRegisterOperand(1))
      print(pOutFile, instr->getRegisterOperand(1));

   if (instr->isImm())
      trfprintf(pOutFile, ",0x%x", maskHalf(imm));

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RILInstruction * instr)
   {
   uint8_t * cursor = (uint8_t *)instr->getBinaryEncoding();

   printPrefix(pOutFile, instr);

   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   if (instr->getRegisterOperand(1))
      {
      print(pOutFile, instr->getRegisterOperand(1));
      }
   else
      {
      trfprintf(pOutFile, "0x%01x", instr->getMask());
      }

   // Now print the target of the RIL Instruction

   if (instr->getTargetSnippet() != NULL)
      {
      trfprintf(pOutFile, ", 0x%p", instr->getTargetSnippet());
      }
   else
      {
      if (instr->isLiteralPoolAddress())
         {
         trfprintf(pOutFile, ", &<LiteralPool Base Address>");
         }
      else if (instr->getOpCode().isExtendedImmediate() != 0)
         {
         // LL: Print immediate value
         trfprintf(pOutFile, ",%ld", instr->getSourceImmediate());
         }
      else
         {
         trfprintf(pOutFile, ",0x%p", instr->getTargetSnippet());
         }
      }

   cursor = (uint8_t *)instr->getBinaryEncoding();
   if (cursor)
      {
      // LL: If it is not an extended immediate instruction, then compute the target addresses for printing on log file.
      if (!(instr->getOpCode().isExtendedImmediate() != 0))
         {
         if (*cursor == 0x18) cursor += 2; //if padding NOP, skip it
         int32_t offsetInHalfWords =(int32_t) (*((int32_t *)(cursor+2)));
         intptrj_t offset = ((intptrj_t)offsetInHalfWords) * 2;
         intptrj_t targetAddress = (intptrj_t)cursor + offset;
         if (TR::Compiler->target.is32Bit())
            targetAddress &= 0x7FFFFFFF;

         if (offsetInHalfWords<0)
            trfprintf(pOutFile, ", targetAddr=0x%p (offset=-0x%p)", targetAddress, -offset);
         else
            trfprintf(pOutFile, ", targetAddr=0x%p (offset=0x%p)", targetAddress, offset);
         }
      }

   printInstructionComment(pOutFile, 1, instr, PRINT_START_COMMENT);
   trfflush(pOutFile);
   }

static char *
bitString(int8_t val)
   {
   static char bits[16][5] =
      {
      "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111", "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"
      };
   if (val > 15 || val < 0)
      {
      return "????";
      }
   else
      {
      return bits[val];
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RSLInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getMemoryReference(), instr);
   // print long displacement field
   if (instr->isExtDisp())
      {
      trfprintf(pOutFile, "\t\t/* LONG DISP NEEDED _binFree=0x%x ",instr->getBinLocalFreeRegs());
      if (instr->getLocalLocalSpillReg1())
         {
         trfprintf(pOutFile, " spillReg=%p",instr->getLocalLocalSpillReg1());
         }

      trfprintf(pOutFile, " */");
      }
   trfflush(pOutFile);
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390RSLInstruction Class Definition
//    ________ _______________________________________________
//   | op     |     L1     |  B2 |   D2  |  R1 |  M3 |    op  |
//   |________|____________|_____|_______|_____|_____|________|
//   0         8           16    20     32     36    40      47
//
////////////////////////////////////////////////////////////////////////////
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RSLbInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   TR::Register *targetRegister = instr->getRegisterOperand(1);
   TR::RegisterPair *regPair = targetRegister->getRegisterPair();
   if (regPair)
      {
      print(pOutFile, regPair->getHighOrder());
      trfprintf(pOutFile, ":");
      print(pOutFile, regPair->getLowOrder());
      }
   else
      {
      print(pOutFile, targetRegister);
      }

   trfprintf(pOutFile, ",");

   print(pOutFile, instr->getMemoryReference(), instr);

   trfprintf(pOutFile, ",0x%1x", instr->getMask());

   // print long displacement field
   if (instr->isExtDisp())
      {
      trfprintf(pOutFile, "\t\t/* LONG DISP NEEDED _binFree=0x%x ",instr->getBinLocalFreeRegs());
      if (instr->getLocalLocalSpillReg1())
         {
         trfprintf(pOutFile, " spillReg=%p",instr->getLocalLocalSpillReg1());
         }

      trfprintf(pOutFile, " */");
      }
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RSInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   if (instr->getSourceImmediate())
      {
      if (instr->getRegisterOperand(1)->getRegisterPair())
         {
         print(pOutFile, instr->getRegisterOperand(1)->getHighOrder());
         }
      else
         {
         print(pOutFile, instr->getRegisterOperand(1));
         }
      if (!instr->isTargetPair() && instr->getLastRegister() != NULL)
         {
         trfprintf(pOutFile, ",");
         print(pOutFile, instr->getLastRegister());
         }

      trfprintf(pOutFile, ",%d", instr->getSourceImmediate());
      }
   else if (instr->getMaskImmediate())
      {
      print(pOutFile, instr->getFirstRegister());
      trfprintf(pOutFile, ",0x%1x,", instr->getMaskImmediate());
      print(pOutFile, instr->getMemoryReference(), instr);
      }
   else if (instr->getOpCode().usesRegPairForTarget() && instr->getOpCode().usesRegPairForSource())
      {
      print(pOutFile, instr->getRegisterOperand(1)->getHighOrder());
      trfprintf(pOutFile, ",");
      print(pOutFile, instr->getSecondRegister()->getHighOrder());
      trfprintf(pOutFile, ",");
      print(pOutFile, instr->getMemoryReference(), instr);
      }
   else if (instr->getLastRegister() == NULL || instr->getOpCode().usesRegPairForTarget())
      {
      print(pOutFile, instr->getFirstRegister());
      trfprintf(pOutFile, ",");
      print(pOutFile, instr->getMemoryReference(), instr);
      }
   else
      {
      print(pOutFile, instr->getFirstRegister());
      trfprintf(pOutFile, ",");
      print(pOutFile, instr->getLastRegister());
      trfprintf(pOutFile, ",");
      print(pOutFile, instr->getMemoryReference(), instr);
      }

   if (instr->isExtDisp())
      {
      trfprintf(pOutFile, " \t\t# LONG DISP NEEDED _binFree=0x%x ",instr->getBinLocalFreeRegs());
      if (instr->getLocalLocalSpillReg1())
         {
         trfprintf(pOutFile, " spillReg=%p",instr->getLocalLocalSpillReg1());
         }
      }

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390MemInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   if (instr->getOpCodeValue() == TR::InstOpCode::STCMH ||
       instr->getOpCodeValue() == TR::InstOpCode::LCTL  ||
       instr->getOpCodeValue() == TR::InstOpCode::STCTL ||
       instr->getOpCodeValue() == TR::InstOpCode::LCTLG ||
       instr->getOpCodeValue() == TR::InstOpCode::STCTG)
      {
      trfprintf(pOutFile, "%d, ", instr->getConstantField());
      trfprintf(pOutFile, "%d, ", instr->getMemAccessMode());
      }
   // Prefetch instruction contains mode
   if (instr->getOpCodeValue() == TR::InstOpCode::PFD)
      {
      int8_t memAccessMode = instr->getMemAccessMode();
      trfprintf(pOutFile, "%d, ", memAccessMode);
      print(pOutFile, instr->getMemoryReference(), instr);
      // Print comment on mode type
      switch (memAccessMode)
         {
         case 0:
            trfprintf(pOutFile, " # Prefetch is No Operation");
            break;
         case 1:
            trfprintf(pOutFile, " # Prefetch for load");
            break;
         case 2:
            trfprintf(pOutFile, " # Prefetch for store");
            break;
         case 6:
            trfprintf(pOutFile, " # Release - Done with store");
            break;
         case 7:
            trfprintf(pOutFile, " # Release - Done with all");
            break;
         default:
            TR_ASSERT(false, "Unexpected memory access mode for PFD: %d\n", memAccessMode);
         }
      }
   else
      {
      print(pOutFile, instr->getMemoryReference(), instr);
      }
   trfflush(pOutFile);
   }

/**
 * Print SSE format in "Opcode  D1(B1),D2(B2)"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SSEInstruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // print first storage information D1(L,B1)
   print(pOutFile, instr->getMemoryReference(), instr);

   trfprintf(pOutFile, ",");

   // print second storage information D2(B2)
   print(pOutFile, instr->getMemoryReference2(), instr);
   trfflush(pOutFile);
   }

/**
 * Print SS1 format in "Opcode  D1(L,B1),D2(B2)"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SS1Instruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // print first storage information D1(L,B1)
   print(pOutFile, instr->getMemoryReference(), instr);

   trfprintf(pOutFile, ",");

   // print second storage information D2(B2)
   print(pOutFile, instr->getMemoryReference2(), instr);
   trfflush(pOutFile);
   }

/**
 * Print SS2 format in "Opcode  D1(L1,B1),D2(L2,B2)"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SS2Instruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // print first storage information D1(L1,B1)
   print(pOutFile, instr->getMemoryReference(), instr);

   trfprintf(pOutFile, ",");

   if (instr->getOpCodeValue() == TR::InstOpCode::SRP)
      {
      // print shift amount
      if (instr->getMemoryReference2())
         print(pOutFile, instr->getMemoryReference2(), instr);
      else
         trfprintf(pOutFile, "%d",instr->getShiftAmount());
      trfprintf(pOutFile, ",%d",instr->getImm3());
      }
   else
      {
      // print first storage information D2(L2,B1)
      print(pOutFile, instr->getMemoryReference2(), instr);
      }
   trfflush(pOutFile);
   }

/**
 * Print SS4 format in "Opcode  D1(R1,B1),D2(B2),R3"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SS4Instruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   if (instr->getOpCodeValue() == TR::InstOpCode::PLO)
      {
      bool prev = false;
      if(instr->getLengthReg())
         {
         if(instr->getLengthReg()->getRegisterPair())
            print(pOutFile, instr->getLengthReg()->getHighOrder());
         else
            print(pOutFile, instr->getLengthReg());
         prev = true;
         }
      if(instr->getMemoryReference())
         {
         if(prev)
            trfprintf(pOutFile, ",");
         print(pOutFile, instr->getMemoryReference(), instr);
         prev = true;
         }
      if(instr->getSourceKeyReg())
         {
         if(prev)
            trfprintf(pOutFile, ",");
         if(instr->getSourceKeyReg()->getRegisterPair())
            print(pOutFile, instr->getSourceKeyReg()->getHighOrder());
         else
            print(pOutFile, instr->getSourceKeyReg());
         prev = true;
         }
      if(instr->getMemoryReference2())
         {
         if(prev)
            trfprintf(pOutFile, ",");
         print(pOutFile, instr->getMemoryReference2(), instr);
         }
      }
   else
      {
      // print first storage information D1(R1,B1) [do not use 'print' of mem ref since this is 'special' with length register encoded
      trfprintf(pOutFile, "%d(", instr->getMemoryReference()->getOffset());

      print(pOutFile,instr->getLengthReg());
      trfprintf(pOutFile, ",");

      print(pOutFile, instr->getMemoryReference()->getBaseRegister());
      trfprintf(pOutFile, "),");

      print(pOutFile, instr->getMemoryReference2(), instr);
      trfprintf(pOutFile, ",");

      print(pOutFile, instr->getSourceKeyReg());
      }
   trfflush(pOutFile);
   }

/**
 * Print SSF format in "Opcode  R3, D1(B1),D2(B2)"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SSFInstruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // target register R3
   print(pOutFile, instr->getFirstRegister());
   trfprintf(pOutFile, ",");

   // print first storage information D1(B1)
   print(pOutFile, instr->getMemoryReference(), instr);

   trfprintf(pOutFile, ",");

   // print second storage information D2(B2)
   print(pOutFile, instr->getMemoryReference2(), instr);
   trfflush(pOutFile);
   }

/**
 * Print SI format in "Opcode  D1(B1),imm"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SIInstruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // print first storage information D1(B1)
   print(pOutFile, instr->getMemoryReference(), instr);

   // print immediate field
   if (instr->getOpCodeValue() == TR::InstOpCode::ASI ||
            instr->getOpCodeValue() == TR::InstOpCode::AGSI ||
            instr->getOpCodeValue() == TR::InstOpCode::ALSI ||
            instr->getOpCodeValue() == TR::InstOpCode::ALGSI)
      trfprintf(pOutFile, ", %d", (int8_t)instr->getSourceImmediate());
   else
      trfprintf(pOutFile, ", 0x%02x", instr->getSourceImmediate());

   // LL: print long displacement field
   if (instr->isExtDisp())
      {
      trfprintf(pOutFile, "\t\t/* LONG DISP NEEDED _binFree=0x%x ",instr->getBinLocalFreeRegs());
      if (instr->getLocalLocalSpillReg1())
         {
         trfprintf(pOutFile, " spillReg=%p",instr->getLocalLocalSpillReg1());
         }

      trfprintf(pOutFile, " */");
      }

   trfflush(pOutFile);
   }

/**
 * Prints SIL format in "Opcode  D1(B1),imm"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SILInstruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // print first storage information D1(B1)
   if (instr->getMemoryReference())
      print(pOutFile, instr->getMemoryReference(), instr);

   // print immediate
   trfprintf(pOutFile, ",0x%04x", (uint16_t)instr->getSourceImmediate());
   }

/**
 * Print S format in "Opcode  D1(B1)"
 */
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SInstruction * instr)
   {

   // print opcode
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));

   // print first storage information D1(B1)
   print(pOutFile, instr->getMemoryReference(), instr);

   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390OpCodeOnlyInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390IInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s%d", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()), instr->getImmediateField());
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RXInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   if(instr->getRegisterOperand(1)->getRegisterPair())
     {
     print(pOutFile, instr->getRegisterOperand(1)->getHighOrder());
     }
   else
     {
     print(pOutFile, instr->getRegisterOperand(1));
     }

   trfprintf(pOutFile, ",");
   if (instr->getMemoryReference() == NULL)
      {
      trfprintf(pOutFile, "%x", instr->getConstForMRField());
      }
   else
      {
      print(pOutFile, instr->getMemoryReference(), instr);
      TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference() ? instr->getMemoryReference()->getSymbolReference()->getSymbol() : 0;
      if ((instr->getOpCode().isLoad() != 0) && symbol && symbol->isSpillTempAuto())
         {
          trfprintf(pOutFile, "\t\t#/* spilled for %s */", getName(instr->getNode()->getOpCode()));
         }
      }

   if (instr->isExtDisp())
      {
      trfprintf(pOutFile, "\t\t/* LONG DISP NEEDED _binFree=0x%x ",instr->getBinLocalFreeRegs());
      if (instr->getLocalLocalSpillReg1())
         {
         trfprintf(pOutFile, " spillReg=%p",instr->getLocalLocalSpillReg1());
         }
      trfprintf(pOutFile, " */");
      }


   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RXEInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getRegisterOperand(1));
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getMemoryReference(), instr);
   trfprintf(pOutFile, ",%d", instr->getM3());
   trfflush(pOutFile);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RXFInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   print(pOutFile, instr->getRegisterOperand(1));
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getRegisterOperand(2));
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getMemoryReference(), instr);
   trfflush(pOutFile);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390MIIInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   trfprintf(pOutFile, "(mask=0x%1x), ", instr->getMask() );

   print(pOutFile, instr->getLabelSymbol());

   uint8_t * cursor = (uint8_t *)instr->getBinaryEncoding();
   if (cursor)
      {
      int32_t offsetInHalfWords =(int32_t) (*((int32_t *)(cursor+3)));
      offsetInHalfWords = offsetInHalfWords >> 8;
      intptrj_t offset = ((intptrj_t)offsetInHalfWords) * 2;
      intptrj_t targetAddress = (intptrj_t)cursor + offset;
      if (TR::Compiler->target.is32Bit())
         targetAddress &= 0x7FFFFFFF;
      if (offsetInHalfWords<0)
         trfprintf(pOutFile, ", targetAddr=0x%p (offset=-0x%p)", targetAddress, -offset);
      else
         trfprintf(pOutFile, ", targetAddr=0x%p (offset=0x%p)", targetAddress, offset);
      }

   trfflush(pOutFile);

   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390SMIInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, getOpCodeName(&instr->getOpCode()));
   trfprintf(pOutFile, "(mask=0x%1x), ", instr->getMask() );

   trfprintf(pOutFile, ",");

   print(pOutFile, instr->getMemoryReference(), instr);
   TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference() ? instr->getMemoryReference()->getSymbolReference()->getSymbol() : 0;
   trfprintf(pOutFile, ",");
   print(pOutFile, instr->getLabelSymbol());

   trfflush(pOutFile);

   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::MemoryReference * mr, TR::Instruction * instr)
   {

   TR::SymbolReference * symRef = mr->getSymbolReference();
   TR::Symbol * sym = symRef ? symRef->getSymbol() : NULL;

   int32_t displacement = mr->getOffset();
   int32_t alignmentBump = mr->getRightAlignmentBump(instr, _comp->cg()) + mr->getLeftAlignmentBump(instr, _comp->cg());
   int32_t sizeIncreaseBump = mr->getSizeIncreaseBump(instr, _comp->cg());
   int32_t extraBump = alignmentBump + sizeIncreaseBump;
   bool fullyMapped = true;
   bool sawWcodeName = false;
   char comments[1024] ;
   bool firstPrint = PRINT_START_COMMENT;
   bool useTobeyFormat = true;

   // For some indirect loads and stores the _originalSymbolReference
   // field of the MemoryReference more accurately reflects the correct
   // name of the memory location being loaded/stored.

   comments[0] = '\0';

   TR::SymbolReference* listingSymRef = mr->getListingSymbolReference();
   TR::Symbol*          listingSym = listingSymRef ? listingSymRef->getSymbol() : NULL;

   printSymbolName(pOutFile, sym, symRef, mr);

   if (sym)
      {
      if (sym->isRegisterMappedSymbol())
         {
         displacement += sym->castToRegisterMappedSymbol()->getOffset();
         }

      if (sym->isSpillTempAuto())
         {
         if (sym->getDataType() == TR::Float || sym->getDataType() == TR::Double)
            sprintf(comments, "FPRSpill");
         else
            sprintf(comments, "GPRSpill");
         }

      switch (sym->getKind())
         {
         case TR::Symbol::IsAutomatic:
         case TR::Symbol::IsParameter:
         case TR::Symbol::IsMethodMetaData:
         case TR::Symbol::IsLabel:
            fullyMapped = false;
            break;
         case TR::Symbol::IsStatic:
            if (mr->getConstantDataSnippet()==NULL)
               {
               displacement += sym->getOffset();
               }
            break;
         case TR::Symbol::IsShadow:
            if (mr->getUnresolvedSnippet() != NULL)
               {
               displacement += mr->getSymbolReference()->getCPIndex();
               fullyMapped = false;
               }
            break;
         case TR::Symbol::IsResolvedMethod:
         case TR::Symbol::IsMethod:
            break;
         default:
          TR_ASSERT( 0, "unexpected symbol kind");
         }


      if (mr->getConstantDataSnippet() != NULL)
         {
         uint64_t value;
         TR::S390ConstantDataSnippet * cnstDataSnip = mr->getConstantDataSnippet();

         switch (cnstDataSnip->getConstantSize())
            {
            case 2:
               value = (uint64_t) cnstDataSnip->getDataAs2Bytes();
               break;
            case 4:
               value = (uint64_t) cnstDataSnip->getDataAs4Bytes();
               break;
            case 8:
               value = (uint64_t) cnstDataSnip->getDataAs8Bytes();
               break;
            }

            trfprintf(pOutFile, " =X(%llx)", value);

         }

#ifdef J9_PROJECT_SPECIFIC
      if (mr->getUnresolvedSnippet() != NULL)
         {
         if (mr->getUnresolvedSnippet()->getUnresolvedData() != NULL)
            {
            trfprintf(pOutFile, " target is [%p]",
             mr->getUnresolvedSnippet()->getUnresolvedData()->getSnippetLabel());
            }
         }
#endif
      }

   // After Binary Encoding, the _displacement field of the MemRef has the
   // correct final displacement. This includes all fixups applied for long
   // displacements. Therefore, we get the displacement value directly from
   // the memory reference instead of trying to compute it

   if (_comp->cg()->getCodeGeneratorPhase() > TR::CodeGenPhase::BinaryEncodingPhase)
      displacement = mr->getDisp();

   if (!_comp->cg()->getMappingAutomatics() && !fullyMapped)
      {
      // indicate mapping not complete
      displacement+=alignmentBump;
      trfprintf(pOutFile, " ?+%d", displacement);
      }
   else
      {
      // print out full displacement. For listing mode we defer until later
      if (extraBump)
         {
         trfprintf(pOutFile, " %d+%d", displacement-extraBump,extraBump);
         }
      else
         {
         // Check the character one before. If is comma, then don't print spa
         // If not, then print space
         trfprintf(pOutFile, " %d", displacement);
         }
      }


   // print out index and base register
   if (mr->getIndexRegister() != NULL)
      {
      if (mr->getBaseRegister() != NULL)
         {
         trfprintf(pOutFile,"(");
         print(pOutFile, mr->getIndexRegister());
         trfprintf(pOutFile, ",");
         print(pOutFile, mr->getBaseRegister());
         // For Listings print out addressing in the form "(index,base,disp)"
         trfprintf(pOutFile,")");
         }
      else
         {
         trfprintf(pOutFile,"(");
         print(pOutFile, mr->getIndexRegister());
         // For Listings print out addressing in the form "(index,disp)"
         trfprintf(pOutFile,")");
         }
      }
   else
      {
      if (mr->getBaseRegister() != NULL)
         {
         trfprintf(pOutFile,"(");
         bool isRSLForm = instr->getKind()==TR::Instruction::IsRSL;
         bool isRSLbForm = instr->getKind()==TR::Instruction::IsRSLb;
         bool isSS1Form = instr->getKind()==TR::Instruction::IsSS1;
         bool isSS2Form = instr->getKind()==TR::Instruction::IsSS2;
         bool isPKUorPKA = isSS1Form && (instr->getOpCodeValue() == TR::InstOpCode::PKU || instr->getOpCodeValue() == TR::InstOpCode::PKA); // SS1 but len is for 2nd operand
         if (isRSLForm || isSS1Form || isSS2Form || isRSLbForm)
            {
            if (isPKUorPKA && mr->is2ndMemRef())
               trfprintf(pOutFile,"%d,",toS390SS1Instruction(instr)->getLen()+1);    // SS1 PKU/PKA print Len1 as part of 2ndMemRef
            else if (isRSLForm)
              trfprintf(pOutFile,"%d,",toS390RSLInstruction(instr)->getLen()+1);     // RSL op print Len
            else if (isRSLbForm)
              trfprintf(pOutFile,"%d,",toS390RSLbInstruction(instr)->getLen()+1);    // RSLb op print Len
            else if (isSS2Form && mr->is2ndMemRef())
               trfprintf(pOutFile,"%d,",toS390SS2Instruction(instr)->getLen2()+1);   // SS2 op2 print Len2
            else if (isSS2Form)
               trfprintf(pOutFile,"%d,",toS390SS2Instruction(instr)->getLen()+1);    // SS2 op1 print Len1
            else if (isSS1Form && !isPKUorPKA && !mr->is2ndMemRef())
               trfprintf(pOutFile,"%d,",toS390SS1Instruction(instr)->getLen()+1);    // SS1 op1 print Len1
            }

         print(pOutFile, mr->getBaseRegister());
         // For Listings print out addressing in the form "(base,index,disp)"
         trfprintf(pOutFile,")");
         }
      }
   if (strlen(comments) > 0)
     {
     TR::SimpleRegex * regex = _comp->getOptions()->getTraceForCodeMining();
     if (regex && TR::SimpleRegex::match(regex, comments))
        {
        trfprintf(pOutFile, "\t ; %s", comments);
        firstPrint = DO_NOT_PRINT_START_COMMENT;
        }
     }

   printInstructionComment(pOutFile, 0, instr, firstPrint );
   trfflush(pOutFile);
   }



static void intToString(int num, char *str, int len=10, bool padWithZeros=false)
   {
   unsigned char packedDecValue[16];
   unsigned char c[16];
   int i=0,j=0,n=0;

   #if !defined(LINUX) && !defined(TR_HOST_X86)
   __cvd(num, (char *)packedDecValue);
   __unpk(c, len-1, packedDecValue, 7 );
   c[len-1] = c[len-1] | 0xf0;

   if (!padWithZeros) while((c[j]=='0') && (j < len-1)) { j++; }
   if (num < 0) str[n++]='-';
   for (i=0; i < len-j; i++) str[n+i] = c[i+j];
   str[n+len-j]=NULL;

   #else
   if (!padWithZeros) sprintf(str, "%d", num);
   else  sprintf(str, "%0*d", len, num);
   #endif
   }

static void intToHex(uint32_t num, char *str, int len)
   {
   unsigned char temp[16];
   const unsigned char ebcdic_translate[16] =
                     { 0xF0, 0xF1, 0xF2, 0xF3,
                       0xF4, 0xF5, 0xF6, 0xF7,
           0xF8, 0xF9, 0xC1, 0xC2,
           0xC3, 0xC4, 0xC5, 0xC6
                     };

   #if !defined(LINUX) && !defined(TR_HOST_X86)
   memcpy(temp, &num, 4);
   __unpk((unsigned char *)str, len, temp, 4);
   __tr((unsigned char *)str, ebcdic_translate-240, len);
   str[len]=NULL;
   #else
   sprintf(str, "%0*X", len,num);
   #endif

   }

char *
TR_Debug::printSymbolName(TR::FILE *pOutFile, TR::Symbol *sym,  TR::SymbolReference *symRef, TR::MemoryReference * mr)
   {
   char *str;
   bool sawWCodeName = false;
   #define LSTBUFSZ 1024
   #define MAXBUFSZ LSTBUFSZ-1
   char outString[LSTBUFSZ];
   outString[0]='\0'; outString[MAXBUFSZ]='\0';
   int32_t prefixLen=0;

   #define ADD_WITH_PREFIX(s)  { strcpy(outString, internalNamePrefix());  \
                                 strncat(outString, s, MAXBUFSZ-prefixLen); }

   snprintf(outString, MAXBUFSZ,  "#%d", symRef ? symRef->getReferenceNumber() : 0);


   if (sym && !sawWCodeName)
      {
      switch (sym->getKind())
         {
         case TR::Symbol::IsAutomatic:
            if (sym->getAutoSymbol()->getName() == NULL)
               {
               snprintf(outString, MAXBUFSZ, " Auto[%s]", getName(symRef));
               }
            else
               {
               snprintf(outString,  MAXBUFSZ, " %s[%s]", sym->getAutoSymbol()->getName(), getName(symRef));
               }
            break;
         case TR::Symbol::IsParameter:
            snprintf(outString, MAXBUFSZ, " Parm[%s]", getName(symRef));
            break;
         case TR::Symbol::IsStatic:
            if (mr && mr->getConstantDataSnippet()==NULL)
               {
               snprintf(outString, MAXBUFSZ, " Static[%s]", getName(symRef));
               }
            break;
         case TR::Symbol::IsResolvedMethod:
         case TR::Symbol::IsMethod:
            snprintf(outString, MAXBUFSZ, " Method[%s]", getName(symRef));
            break;
         case TR::Symbol::IsShadow:
            if (mr->getConstantDataSnippet() == NULL)
               {
               const  char *symRefName = getName(symRef);
               if (sym->isNamedShadowSymbol() && sym->getNamedShadowSymbol()->getName() != NULL)
                  {
                  snprintf(outString, MAXBUFSZ, "   %s[%s]", sym->getNamedShadowSymbol()->getName(), symRefName);
                  }
               else
                  {
                  snprintf(outString, MAXBUFSZ, " Shadow[%s]", getName(symRef));
                  }
               }
            break;
         case TR::Symbol::IsMethodMetaData:
            snprintf(outString, MAXBUFSZ, " MethodMeta[%s]", getName(symRef));
            break;
         case TR::Symbol::IsLabel:
            strncpy(outString, getName(sym->castToLabelSymbol()), MAXBUFSZ);
            break;

         default:
          TR_ASSERT( 0, "unexpected symbol kind");
         }
      }


   if (pOutFile != NULL)
      {
      trfprintf(pOutFile, "%s", outString);

      return NULL;
      }
   else
      {
      char *str = (char *) _comp->trMemory()->allocateHeapMemory(2+strlen(outString));
      strcpy(str, outString);
      return str;
      }
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390NOPInstruction * instr)
   {
   printPrefix(pOutFile, instr);
   trfprintf(pOutFile, "NOP");
   trfflush(pOutFile);
   }

void
TR_Debug::printS390GCRegisterMap(TR::FILE *pOutFile, TR::GCRegisterMap * map)
   {
   TR::Machine *machine = _cg->machine();

   trfprintf(pOutFile, "    registers: {");

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; ++i)
      {
      if (map->getMap() & (1 << (i - 1)))
         {
         trfprintf(pOutFile, "%s ", getName(machine->getS390RealRegister((TR::RealRegister::RegNum) i)));
         }
      }
   trfprintf(pOutFile, "}\n");

   if (0 != map->getHPRMap())
      {
      trfprintf(pOutFile, "    compressed ptr in registers: {");
      for (int32_t i = TR::RealRegister::FirstHPR; i <= TR::RealRegister::LastHPR; i++)
         {
         // 2 bits per register, '10' means HPR has collectible, '11' means both HPR and GPR have collectibles
         if (map->getHPRMap() & (1 << (i - TR::RealRegister::FirstHPR)*2 + 1))
            {
            trfprintf(pOutFile, "%s ", getName(machine->getS390RealRegister((TR::RealRegister::RegNum) i)));
            }
         // Compressed collectible in lower GPR.
         if (map->getHPRMap() & (1 << (i - TR::RealRegister::FirstHPR)*2))
            {
            trfprintf(pOutFile, "%s ", getName(machine->getS390RealRegister((TR::RealRegister::RegNum) (i - TR::RealRegister::FirstHPR + TR::RealRegister::FirstGPR))));
            }
         }
      trfprintf(pOutFile, "}\n");
      }

   trfprintf(pOutFile, "}\n");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RealRegister * reg, TR_RegisterSizes size)
   {
   if (reg == NULL) // zero based ptr
      trfprintf(pOutFile, "%s", "GPR0");
   else
      trfprintf(pOutFile, "%s", getName(reg, size));
   }

const char *
getRegisterName(TR::RealRegister::RegNum num, bool isVRF = false)
   {
   switch (num)
      {
      case TR::RealRegister::NoReg:
         return "NoReg";
      case TR::RealRegister::EvenOddPair:
         return "EvenOddPair";
      case TR::RealRegister::LegalEvenOfPair:
         return "LegalEvenOfPair";
      case TR::RealRegister::LegalOddOfPair:
         return "LegalOddOfPair";
      case TR::RealRegister::AssignAny:
         return "AssignAny";
      case TR::RealRegister::GPR0:
         return "GPR0";
      case TR::RealRegister::GPR1:
         return "GPR1";
      case TR::RealRegister::GPR2:
         return "GPR2";
      case TR::RealRegister::GPR3:
         return "GPR3";
      case TR::RealRegister::GPR4:
         return "GPR4";
      case TR::RealRegister::GPR5:
         return "GPR5";
      case TR::RealRegister::GPR6:
         return "GPR6";
      case TR::RealRegister::GPR7:
         return "GPR7";
      case TR::RealRegister::GPR8:
         return "GPR8";
      case TR::RealRegister::GPR9:
         return "GPR9";
      case TR::RealRegister::GPR10:
         return "GPR10";
      case TR::RealRegister::GPR11:
         return "GPR11";
      case TR::RealRegister::GPR12:
         return "GPR12";
      case TR::RealRegister::GPR13:
         return "GPR13";
      case TR::RealRegister::GPR14:
         return "GPR14";
      case TR::RealRegister::GPR15:
         return "GPR15";
      case TR::RealRegister::FPR0:
         return (isVRF)?"VRF0":"FPR0";
      case TR::RealRegister::FPR1:
         return (isVRF)?"VRF1":"FPR1";
      case TR::RealRegister::FPR2:
         return (isVRF)?"VRF2":"FPR2";
      case TR::RealRegister::FPR3:
         return (isVRF)?"VRF3":"FPR3";
      case TR::RealRegister::FPR4:
         return (isVRF)?"VRF4":"FPR4";
      case TR::RealRegister::FPR5:
         return (isVRF)?"VRF5":"FPR5";
      case TR::RealRegister::FPR6:
         return (isVRF)?"VRF6":"FPR6";
      case TR::RealRegister::FPR7:
         return (isVRF)?"VRF7":"FPR7";
      case TR::RealRegister::FPR8:
         return (isVRF)?"VRF8":"FPR8";
      case TR::RealRegister::FPR9:
         return (isVRF)?"VRF9":"FPR9";
      case TR::RealRegister::FPR10:
         return (isVRF)?"VRF10":"FPR10";
      case TR::RealRegister::FPR11:
         return (isVRF)?"VRF11":"FPR11";
      case TR::RealRegister::FPR12:
         return (isVRF)?"VRF12":"FPR12";
      case TR::RealRegister::FPR13:
         return (isVRF)?"VRF13":"FPR13";
      case TR::RealRegister::FPR14:
         return (isVRF)?"VRF14":"FPR14";
      case TR::RealRegister::FPR15:
         return (isVRF)?"VRF15":"FPR15";
      case TR::RealRegister::VRF16:
         return "VRF16";
      case TR::RealRegister::VRF17:
         return "VRF17";
      case TR::RealRegister::VRF18:
         return "VRF18";
      case TR::RealRegister::VRF19:
         return "VRF19";
      case TR::RealRegister::VRF20:
         return "VRF20";
      case TR::RealRegister::VRF21:
         return "VRF21";
      case TR::RealRegister::VRF22:
         return "VRF22";
      case TR::RealRegister::VRF23:
         return "VRF23";
      case TR::RealRegister::VRF24:
         return "VRF24";
      case TR::RealRegister::VRF25:
         return "VRF25";
      case TR::RealRegister::VRF26:
         return "VRF26";
      case TR::RealRegister::VRF27:
         return "VRF27";
      case TR::RealRegister::VRF28:
         return "VRF28";
      case TR::RealRegister::VRF29:
         return "VRF29";
      case TR::RealRegister::VRF30:
         return "VRF30";
      case TR::RealRegister::VRF31:
         return "VRF31";
      case TR::RealRegister::AR0:
         return "AR0";
      case TR::RealRegister::AR1:
         return "AR1";
      case TR::RealRegister::AR2:
         return "AR2";
      case TR::RealRegister::AR3:
         return "AR3";
      case TR::RealRegister::AR4:
         return "AR4";
      case TR::RealRegister::AR5:
         return "AR5";
      case TR::RealRegister::AR6:
         return "AR6";
      case TR::RealRegister::AR7:
         return "AR7";
      case TR::RealRegister::AR8:
         return "AR8";
      case TR::RealRegister::AR9:
         return "AR9";
      case TR::RealRegister::AR10:
         return "AR10";
      case TR::RealRegister::AR11:
         return "AR11";
      case TR::RealRegister::AR12:
         return "AR12";
      case TR::RealRegister::AR13:
         return "AR13";
      case TR::RealRegister::AR14:
         return "AR14";
      case TR::RealRegister::AR15:
         return "AR15";
      case TR::RealRegister::HPR0:
         return "HPR0";
      case TR::RealRegister::HPR1:
         return "HPR1";
      case TR::RealRegister::HPR2:
         return "HPR2";
      case TR::RealRegister::HPR3:
         return "HPR3";
      case TR::RealRegister::HPR4:
         return "HPR4";
      case TR::RealRegister::HPR5:
         return "HPR5";
      case TR::RealRegister::HPR6:
         return "HPR6";
      case TR::RealRegister::HPR7:
         return "HPR7";
      case TR::RealRegister::HPR8:
         return "HPR8";
      case TR::RealRegister::HPR9:
         return "HPR9";
      case TR::RealRegister::HPR10:
         return "HPR10";
      case TR::RealRegister::HPR11:
         return "HPR11";
      case TR::RealRegister::HPR12:
         return "HPR12";
      case TR::RealRegister::HPR13:
         return "HPR13";
      case TR::RealRegister::HPR14:
         return "HPR14";
      case TR::RealRegister::HPR15:
         return "HPR15";
      case TR::RealRegister::FPPair:
         return "FPPair";
      case TR::RealRegister::LegalFirstOfFPPair:
         return "LegalFirstOfFPPair";
      case TR::RealRegister::LegalSecondOfFPPair:
         return "LegalSecondOfFPPair";
      case TR::RealRegister::SpilledReg:
         return "Spilled";
      case TR::RealRegister::KillVolAccessRegs:
         return "KillVolAccessRegs";
      case TR::RealRegister::KillVolHighRegs:
         return "KillVolHighRegs";
      default:
         return "???";
      }
   }

const char *
getPlxRegisterName(TR::RealRegister::RegNum num)
   {
   switch (num)
      {
      case TR::RealRegister::NoReg:
         return "NoReg";
      case TR::RealRegister::EvenOddPair:
         return "EvenOddPair";
      case TR::RealRegister::LegalEvenOfPair:
         return "LegalEvenOfPair";
      case TR::RealRegister::LegalOddOfPair:
         return "LegalOddOfPair";
      case TR::RealRegister::AssignAny:
         return "AssignAny";
      case TR::RealRegister::GPR0:
         return "@00";
      case TR::RealRegister::GPR1:
         return "@01";
      case TR::RealRegister::GPR2:
         return "@02";
      case TR::RealRegister::GPR3:
         return "@03";
      case TR::RealRegister::GPR4:
         return "@04";
      case TR::RealRegister::GPR5:
         return "@05";
      case TR::RealRegister::GPR6:
         return "@06";
      case TR::RealRegister::GPR7:
         return "@07";
      case TR::RealRegister::GPR8:
         return "@08";
      case TR::RealRegister::GPR9:
         return "@09";
      case TR::RealRegister::GPR10:
         return "@10";
      case TR::RealRegister::GPR11:
         return "@11";
      case TR::RealRegister::GPR12:
         return "@12";
      case TR::RealRegister::GPR13:
         return "@13";
      case TR::RealRegister::GPR14:
         return "@14";
      case TR::RealRegister::GPR15:
         return "@15";
      case TR::RealRegister::AR0:
         return "@AR00";
      case TR::RealRegister::AR1:
         return "@AR01";
      case TR::RealRegister::AR2:
         return "@AR02";
      case TR::RealRegister::AR3:
         return "@AR03";
      case TR::RealRegister::AR4:
         return "@AR04";
      case TR::RealRegister::AR5:
         return "@AR05";
      case TR::RealRegister::AR6:
         return "@AR06";
      case TR::RealRegister::AR7:
         return "@AR07";
      case TR::RealRegister::AR8:
         return "@AR08";
      case TR::RealRegister::AR9:
         return "@AR09";
      case TR::RealRegister::AR10:
         return "@AR10";
      case TR::RealRegister::AR11:
         return "@AR11";
      case TR::RealRegister::AR12:
         return "@AR12";
      case TR::RealRegister::AR13:
         return "@AR13";
      case TR::RealRegister::AR14:
         return "@AR14";
      case TR::RealRegister::AR15:
         return "@AR15";
      default:
         return "???";
      }
   }
const char *
getRegisterNameListing(TR::RealRegister::RegNum num, bool isVRF = false)
   {
   switch (num)
      {
      case TR::RealRegister::NoReg:
         return "NoReg";
      case TR::RealRegister::EvenOddPair:
         return "EvenOddPair";
      case TR::RealRegister::LegalEvenOfPair:
         return "LegalEvenOfPair";
      case TR::RealRegister::LegalOddOfPair:
         return "LegalOddOfPair";
      case TR::RealRegister::AssignAny:
         return "AssignAny";
      case TR::RealRegister::GPR0:
         return "r0";
      case TR::RealRegister::GPR1:
         return "r1";
      case TR::RealRegister::GPR2:
         return "r2";
      case TR::RealRegister::GPR3:
         return "r3";
      case TR::RealRegister::GPR4:
         return "r4";
      case TR::RealRegister::GPR5:
         return "r5";
      case TR::RealRegister::GPR6:
         return "r6";
      case TR::RealRegister::GPR7:
         return "r7";
      case TR::RealRegister::GPR8:
         return "r8";
      case TR::RealRegister::GPR9:
         return "r9";
      case TR::RealRegister::GPR10:
         return "r10";
      case TR::RealRegister::GPR11:
         return "r11";
      case TR::RealRegister::GPR12:
         return "r12";
      case TR::RealRegister::GPR13:
         return "r13";
      case TR::RealRegister::GPR14:
         return "r14";
      case TR::RealRegister::GPR15:
         return "r15";
      case TR::RealRegister::FPR0:
         return (isVRF)?"vrf0":"fp0";
      case TR::RealRegister::FPR1:
         return (isVRF)?"vrf1":"fp1";
      case TR::RealRegister::FPR2:
         return (isVRF)?"vrf2":"fp2";
      case TR::RealRegister::FPR3:
         return (isVRF)?"vrf3":"fp3";
      case TR::RealRegister::FPR4:
         return (isVRF)?"vrf4":"fp4";
      case TR::RealRegister::FPR5:
         return (isVRF)?"vrf5":"fp5";
      case TR::RealRegister::FPR6:
         return (isVRF)?"vrf6":"fp6";
      case TR::RealRegister::FPR7:
         return (isVRF)?"vrf7":"fp7";
      case TR::RealRegister::FPR8:
         return (isVRF)?"vrf8":"fp8";
      case TR::RealRegister::FPR9:
         return (isVRF)?"vrf9":"fp9";
      case TR::RealRegister::FPR10:
         return (isVRF)?"vrf10":"fp10";
      case TR::RealRegister::FPR11:
         return (isVRF)?"vrf11":"fp11";
      case TR::RealRegister::FPR12:
         return (isVRF)?"vrf12":"fp12";
      case TR::RealRegister::FPR13:
         return (isVRF)?"vrf13":"fp13";
      case TR::RealRegister::FPR14:
         return (isVRF)?"vrf14":"fp14";
      case TR::RealRegister::FPR15:
         return (isVRF)?"vrf15":"fp15";
      case TR::RealRegister::VRF16:
         return "vrf16";
      case TR::RealRegister::VRF17:
         return "vrf17";
      case TR::RealRegister::VRF18:
         return "vrf18";
      case TR::RealRegister::VRF19:
         return "vrf19";
      case TR::RealRegister::VRF20:
         return "vrf20";
      case TR::RealRegister::VRF21:
         return "vrf21";
      case TR::RealRegister::VRF22:
         return "vrf22";
      case TR::RealRegister::VRF23:
         return "vrf23";
      case TR::RealRegister::VRF24:
         return "vrf24";
      case TR::RealRegister::VRF25:
         return "vrf25";
      case TR::RealRegister::VRF26:
         return "vrf26";
      case TR::RealRegister::VRF27:
         return "vrf27";
      case TR::RealRegister::VRF28:
         return "vrf28";
      case TR::RealRegister::VRF29:
         return "vrf29";
      case TR::RealRegister::VRF30:
         return "vrf30";
      case TR::RealRegister::VRF31:
         return "vrf31";
      case TR::RealRegister::AR0:
         return "ar0";
      case TR::RealRegister::AR1:
         return "ar1";
      case TR::RealRegister::AR2:
         return "ar2";
      case TR::RealRegister::AR3:
         return "ar3";
      case TR::RealRegister::AR4:
         return "ar4";
      case TR::RealRegister::AR5:
         return "ar5";
      case TR::RealRegister::AR6:
         return "ar6";
      case TR::RealRegister::AR7:
         return "ar7";
      case TR::RealRegister::AR8:
         return "ar8";
      case TR::RealRegister::AR9:
         return "ar9";
      case TR::RealRegister::AR10:
         return "ar10";
      case TR::RealRegister::AR11:
         return "ar11";
      case TR::RealRegister::AR12:
         return "ar12";
      case TR::RealRegister::AR13:
         return "ar13";
      case TR::RealRegister::AR14:
         return "ar14";
      case TR::RealRegister::AR15:
         return "ar15";
      case TR::RealRegister::HPR0:
         return "hpr0";
      case TR::RealRegister::HPR1:
         return "hpr1";
      case TR::RealRegister::HPR2:
         return "hpr2";
      case TR::RealRegister::HPR3:
         return "hpr3";
      case TR::RealRegister::HPR4:
         return "hpr4";
      case TR::RealRegister::HPR5:
         return "hpr5";
      case TR::RealRegister::HPR6:
         return "hpr6";
      case TR::RealRegister::HPR7:
         return "hpr7";
      case TR::RealRegister::HPR8:
         return "hpr8";
      case TR::RealRegister::HPR9:
         return "hpr9";
      case TR::RealRegister::HPR10:
         return "hpr10";
      case TR::RealRegister::HPR11:
         return "hpr11";
      case TR::RealRegister::HPR12:
         return "hpr12";
      case TR::RealRegister::HPR13:
         return "hpr13";
      case TR::RealRegister::HPR14:
         return "hpr14";
      case TR::RealRegister::HPR15:
         return "hpr15";
      default:
         return "???";
      }
   }

const char *
TR_Debug::getName(TR::RealRegister * reg, TR_RegisterSizes size)
   {
   if (!reg) return "<null>";

   TR::RealRegister::RegNum regNum = reg->getRegisterNumber();
   bool isVRF = (size == TR_VectorReg &&
                 regNum >= TR::RealRegister::FirstVRF &&
                 regNum <= TR::RealRegister::LastVRF);

   return getRegisterName(regNum,isVRF);
   }

const char *
TR_Debug::getS390RegisterName(uint32_t regNum, bool isVRF)
   {
   return getRegisterName((TR::RealRegister::RegNum) regNum, isVRF);
   }



uint8_t *
TR_Debug::printS390ArgumentsFlush(TR::FILE *pOutFile, TR::Node * node, uint8_t * bufferPos, int32_t argSize)
   {
   int32_t offset = 0, intArgNum = 0, floatArgNum = 0;

   TR::MethodSymbol *methodSymbol = node->getSymbol()->castToMethodSymbol();
   TR::Linkage * privateLinkage = _cg->getLinkage(methodSymbol->getLinkageConvention());

   TR::Machine *machine = _cg->machine();
   TR::RealRegister * stackPtr = privateLinkage->getStackPointerRealRegister();

   if (privateLinkage->getRightToLeft())
      {
      offset = privateLinkage->getOffsetToFirstParm();
      }
   else
      {
      offset = argSize + privateLinkage->getOffsetToFirstParm();
      }

   for (int i = node->getFirstArgumentIndex(); i < node->getNumChildren(); i++)
      {
      TR::Node * child = node->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (!privateLinkage->getRightToLeft())
               {
               offset -= TR::Compiler->target.is64Bit() ? 8 : 4;
               }
            if (intArgNum < privateLinkage->getNumIntegerArgumentRegisters())
               {
               if (TR::Compiler->target.is64Bit() && child->getDataType() == TR::Address)
                  {
                  printPrefix(pOutFile, NULL, bufferPos, 6);
                  trfprintf(pOutFile, "STG  \t");
                  }
               else
                  {
                  printPrefix(pOutFile, NULL, bufferPos, 4);
                  trfprintf(pOutFile, "ST   \t");
                  }

               print(pOutFile, machine->getS390RealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum)));
               trfprintf(pOutFile, ",%d(,", offset);
               print(pOutFile, stackPtr);
               trfprintf(pOutFile, ")");

               if (TR::Compiler->target.is64Bit() && child->getDataType() == TR::Address)
                  {
                  bufferPos += 6;
                  }
               else
                  {
                  bufferPos += 4;
                  }
               }
            intArgNum++;
            if (privateLinkage->getRightToLeft())
               {
               offset -= TR::Compiler->target.is64Bit() ? 8 : 4;
               }
            break;

         case TR::Int64:
            if (!privateLinkage->getRightToLeft())
               {
               offset -= (TR::Compiler->target.is64Bit() ? 16 : 8);
               }
            if (intArgNum < privateLinkage->getNumIntegerArgumentRegisters())
               {
               if (TR::Compiler->target.is64Bit())
                  {
                  printPrefix(pOutFile, NULL, bufferPos, 6);
                  trfprintf(pOutFile, "STG  \t");
                  print(pOutFile, machine->getS390RealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum)));
                  trfprintf(pOutFile, ",%d(,", offset);
                  print(pOutFile, stackPtr);
                  trfprintf(pOutFile, ")");
                  bufferPos += 6;
                  }
               else
                  {
                  printPrefix(pOutFile, NULL, bufferPos, 4);
                  trfprintf(pOutFile, "ST   \t");
                  print(pOutFile, machine->getS390RealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum)));
                  trfprintf(pOutFile, ", %d(,", offset);
                  print(pOutFile, stackPtr);
                  trfprintf(pOutFile, ")");
                  bufferPos += 4;

                  if (intArgNum < privateLinkage->getNumIntegerArgumentRegisters() - 1)
                     {
                     printPrefix(pOutFile, NULL, bufferPos, 4);
                     trfprintf(pOutFile, "ST   \t");

                     print(pOutFile, machine->getS390RealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum + 1)));
                     trfprintf(pOutFile, ",%d(,", offset + 4);
                     print(pOutFile, stackPtr);
                     trfprintf(pOutFile, ")");
                     bufferPos += 4;
                     }
                  }
               }
            intArgNum += TR::Compiler->target.is64Bit() ? 1 : 2;
            if (privateLinkage->getRightToLeft())
               {
               offset += TR::Compiler->target.is64Bit() ? 16 : 8;
               }
            break;

         case TR::Float:
            if (!privateLinkage->getRightToLeft())
               {
               offset -= 4;
               }
            if (floatArgNum < privateLinkage->getNumFloatArgumentRegisters())
               {
               printPrefix(pOutFile, NULL, bufferPos, 4);
               trfprintf(pOutFile, "STD   \t");
               print(pOutFile, machine->getS390RealRegister(privateLinkage->getFloatArgumentRegister(floatArgNum)));
               trfprintf(pOutFile, ",%d(,", offset);
               print(pOutFile, stackPtr);
               trfprintf(pOutFile, ")");
               bufferPos += 4;
               }
            floatArgNum++;
            if (privateLinkage->getRightToLeft())
               {
               offset += 4;
               }
            break;

         case TR::Double:
            if (!privateLinkage->getRightToLeft())
               {
               offset -= 8;
               }
            if (floatArgNum < privateLinkage->getNumFloatArgumentRegisters())
               {
               printPrefix(pOutFile, NULL, bufferPos, 4);
               trfprintf(pOutFile, "STE  \t");
               print(pOutFile, machine->getS390RealRegister(privateLinkage->getFloatArgumentRegister(floatArgNum)));
               trfprintf(pOutFile, ",%d(,", offset);
               print(pOutFile, stackPtr);
               trfprintf(pOutFile, ")");
               bufferPos += 4;
               }
            floatArgNum++;
            if (privateLinkage->getRightToLeft())
               {
               offset += 8;
               }
            break;
         }
      }

   return bufferPos;
   }

void
TR_Debug::printFullRegInfo(TR::FILE *pOutFile, TR::RealRegister * reg)
   {
   if (pOutFile == NULL)
      {
      return;
      }

   trfprintf(pOutFile, "[ ");
   trfprintf(pOutFile, "%-12s ][ ", getName(reg));

   static const char * stateNames[5] =
      {
      "Free", "Unlatched", "Assigned", "Blocked", "Locked"
      };

   trfprintf(pOutFile, "%-10s ][ ", stateNames[reg->getState()]);
   trfprintf(pOutFile, "%-12s ]", reg->getAssignedRegister() ? getName(reg->getAssignedRegister()) : " ");


   if (reg->getAssignedRegister() != NULL)
      {
      trfprintf(pOutFile, " ][%5d][%5d][%5d][%d]\n", reg->getAssignedRegister()->getTotalUseCount(),
               reg->getAssignedRegister()->getFutureUseCount(), reg->getWeight(), reg->getAssignedRegister()->isUsedInMemRef());
      }
   else
      {
      trfprintf(pOutFile, " ][-----][-----][%5d][-]\n", reg->getWeight());
      }

   trfflush(pOutFile);
   }

const char *
TR_Debug::getOpCodeName(TR::InstOpCode * opCode)
   {
   return TR::InstOpCode::opCodeToNameMap[opCode->getOpCodeValue()];
   }

void
TR_Debug::printGPRegisterStatus(TR::FILE *pOutFile, TR::Machine *machine)
   {
   if (pOutFile == NULL)
      {
      return;
      }
   trfprintf(pOutFile, "\n                         GP Reg Status:          Register         State        Assigned\n");
   for (int i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
      {
      trfprintf(pOutFile, "%p                      ", machine->getRegisterFile(i));
      printFullRegInfo(pOutFile, machine->getRegisterFile(i));
      }
   if ( _comp->getOption(TR_Enable390AccessRegs) )
      {
      for (int i = TR::RealRegister::FirstAR; i <= TR::RealRegister::LastAR; i++)
         {
         trfprintf(pOutFile, "%p                      ", machine->getRegisterFile(i));
         printFullRegInfo(pOutFile, machine->getRegisterFile(i));
         }
      }
   trfflush(pOutFile);
   }
void
TR_Debug::printFPRegisterStatus(TR::FILE *pOutFile, TR::Machine *machine)
   {
   if (pOutFile == NULL)
      {
      return;
      }
   trfprintf(pOutFile, "\n                         FP Reg Status:          Register         State        Assigned\n");
   for (int i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
      {
      trfprintf(pOutFile, "%p                      ", machine->getRegisterFile(i));
      printFullRegInfo(pOutFile, machine->getRegisterFile(i));
      }
   trfflush(pOutFile);
   }

void
TR_Debug::printS390RegisterDependency(TR::FILE *pOutFile, TR::Register * virtReg, int realReg, bool refsReg, bool defsReg)
   {
   bool isVRF = (virtReg->getKind() == TR_VRF);
   trfprintf(pOutFile, " {%s:%s:%s%s}", getS390RegisterName(realReg,isVRF), getName(virtReg),refsReg?"R":"",defsReg?"D":"");
   if (virtReg->isPlaceholderReg())
      {
      trfprintf(pOutFile, "*");
      }
   }

void
TR_Debug::printRegisterDependencies(TR::FILE *pOutFile, TR_S390RegisterDependencyGroup * rgd, int numberOfRegisters)
   {
   if (pOutFile == NULL || rgd == NULL)
      {
      return;
      }
   for (int i = 0; i < numberOfRegisters; i++)
      {
      TR::Register * virtReg = rgd->getRegisterDependency(i)->getRegister(_comp->cg());
      TR::RealRegister::RegNum realReg = rgd->getRegisterDependency(i)->getRealRegister();

      if (virtReg != NULL)
         {
         if (i % 8 == 0)
            trfprintf(pOutFile, "\n");
         printS390RegisterDependency(pOutFile, virtReg, realReg,
                                     rgd->getRegisterDependency(i)->getRefsRegister(),
                                     rgd->getRegisterDependency(i)->getDefsRegister());
         }
      }
   }

uint32_t
TR_Debug::getBitRegNum(TR::RealRegister * reg)
   {
   switch (reg->getRegisterNumber())
      {
      case TR::RealRegister::GPR0:
      case TR::RealRegister::FPR0:
         return 0x00000001;
      case TR::RealRegister::GPR1:
      case TR::RealRegister::FPR1:
         return 0x00000002;
      case TR::RealRegister::GPR2:
      case TR::RealRegister::FPR2:
         return 0x00000004;
      case TR::RealRegister::GPR3:
      case TR::RealRegister::FPR3:
         return 0x00000008;
      case TR::RealRegister::GPR4:
      case TR::RealRegister::FPR4:
         return 0x00000010;
      case TR::RealRegister::GPR5:
      case TR::RealRegister::FPR5:
         return 0x00000020;
      case TR::RealRegister::GPR6:
      case TR::RealRegister::FPR6:
         return 0x00000040;
      case TR::RealRegister::GPR7:
      case TR::RealRegister::FPR7:
         return 0x00000080;
      case TR::RealRegister::GPR8:
      case TR::RealRegister::FPR8:
         return 0x00000100;
      case TR::RealRegister::GPR9:
      case TR::RealRegister::FPR9:
         return 0x00000200;
      case TR::RealRegister::GPR10:
      case TR::RealRegister::FPR10:
         return 0x00000400;
      case TR::RealRegister::GPR11:
      case TR::RealRegister::FPR11:
         return 0x00000800;
      case TR::RealRegister::GPR12:
      case TR::RealRegister::FPR12:
         return 0x00001000;
      case TR::RealRegister::GPR13:
      case TR::RealRegister::FPR13:
         return 0x00002000;
      case TR::RealRegister::GPR14:
      case TR::RealRegister::FPR14:
         return 0x00004000;
      case TR::RealRegister::GPR15:
      case TR::RealRegister::FPR15:
         return 0x00008000;
      default:
         return 0x00000000;
      }
   }

uint8_t *
TR_Debug::printLoadVMThreadInstruction(TR::FILE *pOutFile, uint8_t* cursor)
   {
   if (_comp->getOption(TR_Enable390FreeVMThreadReg) && _comp->cg()->getVMThreadRegister()->getBackingStorage())
      {
      uint32_t offset = ( *(uint32_t *)cursor & 0xFFF);
      if (TR::Compiler->target.is64Bit())
         {
         printPrefix(pOutFile, NULL, cursor, 6);
         trfprintf(pOutFile, "LG \tGPR13, %d(,GPR5)\t#Load VM Thread value", offset);
         cursor += 6;
         }
      else
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "L \tGPR13, %d(,GPR5)\t#Load VM Thread value", offset);
         cursor += 4;
         }
      }
   return cursor;
   }

uint8_t *
TR_Debug::printRuntimeInstrumentationOnOffInstruction(TR::FILE *pOutFile, uint8_t* cursor, bool isRION, bool isPrivateLinkage)
   {
   TR::CodeGenerator * cg = _comp->cg();

   if (cg->getSupportsRuntimeInstrumentation())
      {
      if (!isPrivateLinkage || cg->getEnableRIOverPrivateLinkage())
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         if (isRION)
            trfprintf(pOutFile, "RION");
         else
            trfprintf(pOutFile, "RIOFF");
         cursor += sizeof(int32_t);
         }
      }
   return cursor;
   }

const char *
TR_Debug::updateBranchName(const char * opCodeName, const char * brCondName)
   {
   if (strcmp(opCodeName, "BCR") == 0)
      {
      if (strcmp(brCondName, "UNCOND") == 0)
         return "BR";
      else if (strcmp(brCondName, "O") == 0)
         return "BOR";
      else if (strcmp(brCondName, "HT") == 0)
         return "BHR";
      else if (strcmp(brCondName, "LT") == 0)
         return "BLR";
      else if (strcmp(brCondName, "NE") == 0)
         return "BNER";
      else if (strcmp(brCondName, "EQ") == 0)
         return "BER";
      else if (strcmp(brCondName, "HE") == 0)
         return "BNLR";
      else if (strcmp(brCondName, "LE") == 0)
         return "BNHR";
      else if (strcmp(brCondName, "NO") == 0)
         return "BNOR";
      else
         return "NOPR";
      }
   else if (strcmp(opCodeName, "BRC") == 0)
      {
      if (strcmp(brCondName, "UNCOND") == 0)
         return "J";
      else if (strcmp(brCondName, "O") == 0)
         return "JO";
      else if (strcmp(brCondName, "HT") == 0)
         return "JH";
      else if (strcmp(brCondName, "LT") == 0)
         return "JL";
      else if (strcmp(brCondName, "NE") == 0)
         return "JNE";
      else if (strcmp(brCondName, "EQ") == 0)
         return "JE";
      else if (strcmp(brCondName, "HE") == 0)
         return "JNL";
      else if (strcmp(brCondName, "LE") == 0)
         return "JNH";
      else if (strcmp(brCondName, "NO") == 0)
         return "JNO";
      else
         return "JNOP";
      }
   else if (strcmp(opCodeName, "BC") == 0)
      {
      if (strcmp(brCondName, "UNCOND") == 0)
         return "B";
      else if (strcmp(brCondName, "O") == 0)
         return "BO";
      else if (strcmp(brCondName, "HT") == 0)
         return "BH";
      else if (strcmp(brCondName, "LT") == 0)
         return "BL";
      else if (strcmp(brCondName, "NE") == 0)
         return "BNE";
      else if (strcmp(brCondName, "EQ") == 0)
         return "BE";
      else if (strcmp(brCondName, "HE") == 0)
         return "BNL";
      else if (strcmp(brCondName, "LE") == 0)
         return "BNH";
      else if (strcmp(brCondName, "NO") == 0)
         return "BNO";
      else
         return "NOP";
      }
   else if (strcmp(opCodeName, "BRCL") == 0)
      {
      if (strcmp(brCondName, "UNCOND") == 0)
         return "BRUL";
      else if (strcmp(brCondName, "O") == 0)
         return "BROL";
      else if (strcmp(brCondName, "HT") == 0)
         return "BRHL";
      else if (strcmp(brCondName, "LT") == 0)
         return "BRLL";
      else if (strcmp(brCondName, "NE") == 0)
         return "BRNEL";
      else if (strcmp(brCondName, "EQ") == 0)
         return "BREL";
      else if (strcmp(brCondName, "HE") == 0)
         return "BRNLL";
      else if (strcmp(brCondName, "LE") == 0)
         return "BRNHL";
      else if (strcmp(brCondName, "NO") == 0)
         return "BRNOL";
      else
         return "NOP";
      }

   return "";
   }

/**
 *
 * TR_Debug print VRI instruction info
*/
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390VRIInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, instr->getExtendedMnemonicName());

   for(int i = 1; instr->getRegisterOperand(i); i++)
      {
      if (i != 1)
         trfprintf(pOutFile, ",");
      print(pOutFile, instr->getRegisterOperand(i), TR_VectorReg);
      }

   switch(instr->getKind())
      {
      case TR::Instruction::IsVRIa:
         trfprintf(pOutFile, ",0x%x", static_cast<TR::S390VRIaInstruction*>(instr)->getImmediateField2());
         break;
      case TR::Instruction::IsVRIb:
         trfprintf(pOutFile, ",0x%x,0x%x",
               maskHalf(static_cast<TR::S390VRIbInstruction*>(instr)->getImmediateField2()),
               maskHalf(static_cast<TR::S390VRIbInstruction*>(instr)->getImmediateField3()));
         break;
      case TR::Instruction::IsVRIc:
         trfprintf(pOutFile, ",0x%x", (maskHalf(static_cast<TR::S390VRIcInstruction*>(instr)->getImmediateField2())));
         break;
      case TR::Instruction::IsVRId:
         trfprintf(pOutFile, ",0x%x",
               maskHalf(static_cast<TR::S390VRIdInstruction*>(instr)->getImmediateField4()));
         break;
      case TR::Instruction::IsVRIe:
         trfprintf(pOutFile, ",0x%x",
               maskHalf(static_cast<TR::S390VRIeInstruction*>(instr)->getImmediateField3()));
         break;
      case TR::Instruction::IsVRIf:
         trfprintf(pOutFile, ",0x%x",
               maskHalf(static_cast<TR::S390VRIfInstruction*>(instr)->getImmediateField4()));
         break;
      case TR::Instruction::IsVRIg:
         trfprintf(pOutFile, ",0x%x, 0x%x",
               maskHalf(static_cast<TR::S390VRIgInstruction*>(instr)->getImmediateField3()),
               maskHalf(static_cast<TR::S390VRIgInstruction*>(instr)->getImmediateField4()));
         break;
      case TR::Instruction::IsVRIh:
         trfprintf(pOutFile, ",0x%x",
               maskHalf(static_cast<TR::S390VRIhInstruction*>(instr)->getImmediateField3()));
         break;
      case TR::Instruction::IsVRIi:
         trfprintf(pOutFile, ",0x%x",
               maskHalf(static_cast<TR::S390VRIiInstruction*>(instr)->getImmediateField3()));
         break;
      default:
         TR_ASSERT(false, "Unknown VRI type");
      }

   if (instr->getPrintM3())
      trfprintf(pOutFile, ",%d", instr->getM3());
   if (instr->getPrintM4())
      trfprintf(pOutFile, ",%d", instr->getM4());
   if (instr->getPrintM5())
      trfprintf(pOutFile, ",%d", instr->getM5());

   trfflush(pOutFile);
   }

/**
 *
 * TR_Debug print VRR instruction info
*/
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390VRRInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, instr->getExtendedMnemonicName());

   // iterate through all Register operands
   for(int i = 1; instr->getRegisterOperand(i); i++)
      {
      if (i != 1)
         trfprintf(pOutFile, ",");

      // Register operand is GPR for VRR-f's 2nd and 3rd operand, or VRR-i's 1st operand
      bool isGPR = (instr->getKind() == TR::Instruction::IsVRRf && (i == 2 || i == 3)) ||
              (instr->getKind() == TR::Instruction::IsVRRi && (i == 1));

      print(pOutFile, instr->getRegisterOperand(i), (isGPR)?TR_WordReg:TR_VectorReg);
      }

   if (instr->getPrintM3())
      trfprintf(pOutFile, ",%d", instr->getM3());
   if (instr->getPrintM4())
      trfprintf(pOutFile, ",%d", instr->getM4());
   if (instr->getPrintM5())
      trfprintf(pOutFile, ",%d", instr->getM5());
   if (instr->getPrintM6())
      trfprintf(pOutFile, ",%d", instr->getM6());

   trfflush(pOutFile);
   }

/**
 *
 * TR_Debug print VStroage instruction info
*/
void
TR_Debug::print(TR::FILE *pOutFile, TR::S390VStorageInstruction * instr)
   {
   printPrefix(pOutFile, instr);

   trfprintf(pOutFile, "%-*s", OPCODE_SPACING, instr->getExtendedMnemonicName());

   OMR::Instruction::Kind instKind = instr->getKind();
   bool firstRegIsGPR = (instKind == TR::Instruction::IsVRSc);
   bool secondRegIsGPR = (instKind == TR::Instruction::IsVRSb) || (instKind == TR::Instruction::IsVRSd);

   // 1st register operand
   print(pOutFile, instr->getRegisterOperand(1), (firstRegIsGPR)?TR_WordReg:TR_VectorReg);

   // 2nd register operand, if any
   trfprintf(pOutFile, ",");
   if (instKind != TR::Instruction::IsVRX &&
       instKind != TR::Instruction::IsVRV &&
       instKind != TR::Instruction::IsVSI)
      {
      print(pOutFile, instr->getRegisterOperand(2),(secondRegIsGPR)?TR_WordReg:TR_VectorReg);
      trfprintf(pOutFile, ",");
      }

   // memory reference
   print(pOutFile, instr->getMemoryReference(), instr);
   TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference() ?
               instr->getMemoryReference()->getSymbolReference()->getSymbol() : 0;

   if (instr->getOpCode().isLoad() && symbol && symbol->isSpillTempAuto())
      {
      trfprintf(pOutFile, "\t\t#/* spilled for %s */", getName(instr->getNode()->getOpCode()));
      }

   // mask, if any
   if (instr->getPrintMaskField())
      trfprintf(pOutFile, ",%d", instr->getMaskField());

   // immediates. VSI only for now. 8-bit long.
   if(instKind == TR::Instruction::IsVSI)
      {
      trfprintf(pOutFile, ",0x%x", maskHalf(static_cast<TR::S390VSIInstruction*>(instr)->getImmediateField3()));
      }

   trfflush(pOutFile);
   }
#undef maskHalf
