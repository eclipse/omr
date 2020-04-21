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

#include "z/codegen/S390GenerateInstructions.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/S390Snippets.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/S390Evaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/S390Instruction.hpp"


class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_VirtualGuardSite;

#define INSN_HEAP cg->trHeapMemory()

////////////////////////////////////////////////////////////////////////////////
// Generate methods
////////////////////////////////////////////////////////////////////////////////
TR::Instruction *
generateS390LabelInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::LabelSymbol * sym,
   TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390LabelInstruction(op, n, sym, preced, cg);
      }
   return new (INSN_HEAP) TR::S390LabelInstruction(op, n, sym, cg);
   }

TR::Instruction *
generateS390LabelInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::LabelSymbol * sym,
   TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390LabelInstruction(op, n, sym, cond, preced, cg);
      }
   return new (INSN_HEAP) TR::S390LabelInstruction(op, n, sym, cond, cg);
   }

TR::Instruction *
generateS390LabelInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Snippet * s,
   TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390LabelInstruction(op, n, s, cond, preced, cg);
      }
   return new (INSN_HEAP) TR::S390LabelInstruction(op, n, s, cond, cg);
   }

TR::Instruction *
generateS390LabelInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Snippet * s, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390LabelInstruction(op, n, s, preced, cg);
      }
   return new (INSN_HEAP) TR::S390LabelInstruction(op, n, s, cg);
   }

////////////////////////////////////////////////////////////////////////////////
// Generate methods
////////////////////////////////////////////////////////////////////////////////
TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::InstOpCode::S390BranchCondition brCond,
                              TR::Node * n, TR::LabelSymbol * sym, TR::Instruction * preced)
   {
   TR::S390BranchInstruction * cursor ;
   if (preced)
      cursor = new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, sym, preced, cg);
   else
      cursor = new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, sym, cg);
   return cursor;
   }

TR::Instruction* generateS390BranchInstruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* node, TR::InstOpCode::S390BranchCondition compareOpCode, TR::Register* targetReg, TR::Instruction* preced)
   {
   TR::Instruction* returnInstruction = generateS390RegInstruction(cg, op, node, targetReg, preced);

   // RR type branch instructions use the first operand register field as a mask value
   static_cast<TR::S390RegInstruction*>(returnInstruction)->setBranchCondition(compareOpCode);

   return returnInstruction;
   }

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg,
                              TR::LabelSymbol * sym, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390BranchOnCountInstruction(op, n, targetReg, sym, preced, cg);
      }
   return new (INSN_HEAP) TR::S390BranchOnCountInstruction(op, n, targetReg, sym, cg);
   }

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg,
                              TR::RegisterDependencyConditions *cond, TR::LabelSymbol * sym, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390BranchOnCountInstruction(op, n, targetReg, cond, sym, preced, cg);
      }
   return new (INSN_HEAP) TR::S390BranchOnCountInstruction(op, n, targetReg, cond, sym, cg);
   }

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * sourceReg,
                              TR::Register * targetReg, TR::LabelSymbol * sym, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390BranchOnIndexInstruction(op, n, sourceReg, targetReg, sym, preced, cg);
      }
   return new (INSN_HEAP) TR::S390BranchOnIndexInstruction(op, n, sourceReg, targetReg, sym, cg);
   }

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * sourceReg,
                              TR::Register * targetReg, TR::LabelSymbol * sym, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390BranchOnIndexInstruction(op, n, sourceReg, targetReg, sym, preced, cg);
      }
   return new (INSN_HEAP) TR::S390BranchOnIndexInstruction(op, n, sourceReg, targetReg, sym, cg);
   }

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::InstOpCode::S390BranchCondition brCond,
                              TR::Node * n, TR::LabelSymbol * sym, TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   TR::S390BranchInstruction * cursor ;
   if (preced)
      cursor = new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, sym, cond, preced, cg);
   else
      cursor =  new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, sym, cond, cg);
   return cursor;
   }

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::InstOpCode::S390BranchCondition brCond,
                              TR::Node * n, TR::Snippet * s, TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   TR::S390BranchInstruction * cursor ;
   if (preced)
      cursor = new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, s, cond, preced, cg);
   else
      cursor = new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, s, cond, cg);
   return cursor;
   }

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::InstOpCode::S390BranchCondition brCond,
                              TR::Node * n, TR::Snippet * s, TR::Instruction * preced)
   {
   TR::S390BranchInstruction * cursor ;
   if (preced)
      cursor = new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, s, preced, cg);
   else
      cursor = new (INSN_HEAP) TR::S390BranchInstruction(op, brCond, n, s, cg);
   return cursor;
   }

/**
 * Given a compare-only opCode, return its corresponding compare-and-branch-relative opCode
*/
TR::InstOpCode::Mnemonic getReplacementCompareAndBranchOpCode(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic compareOpCode)
   {
   static char * disableS390CompareAndBranch = feGetEnv("TR_DISABLES390CompareAndBranch");

   if (disableS390CompareAndBranch)
      return TR::InstOpCode::BAD ;

   switch(compareOpCode)
      {
      case TR::InstOpCode::CR:
         return TR::InstOpCode::CRJ;
         break;
      case TR::InstOpCode::CLR:
         return TR::InstOpCode::CLRJ;
         break;
      case TR::InstOpCode::CGR:
         return TR::InstOpCode::CGRJ;
         break;
      case TR::InstOpCode::CLGR:
         return TR::InstOpCode::CLGRJ;
         break;
      case TR::InstOpCode::C:
         return TR::InstOpCode::CIJ;
         break;
      case TR::InstOpCode::CL:
         return TR::InstOpCode::CLIJ;
         break;
      case TR::InstOpCode::CG:
         return TR::InstOpCode::CGIJ;
         break;
      case TR::InstOpCode::CLG:
         return TR::InstOpCode::CLGIJ;
         break;
      default:
         return TR::InstOpCode::BAD;
         break;
      }
   }

TR::InstOpCode::Mnemonic
getReplacementLongDisplacementOpCode(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::MemoryReference* memRef)
   {
   if (!memRef->hasTemporaryNegativeOffset() && memRef->isLongDisplacementRequired())
      {
      auto longDisplacementMnemonic = TR::InstOpCode::getEquivalentLongDisplacementMnemonic(op);

      if (longDisplacementMnemonic != TR::InstOpCode::BAD)
         {
         op = longDisplacementMnemonic;

         TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "z/memref/long-displacement-upgrade/(%s)", cg->comp()->signature()));
         }
      }

   return op;
   }

/**
 * Generate a compare and a branch instruction.  if z10 is available, this will
 * attempt to generate a COMPARE AND BRANCH instruction, otherwise the a
 * compare with the compareOpCode passed and a TR::InstOpCode::BRC with the passed branch
 * condition will be generated.
 *
 * You are responsible for setting start and end of internal control flow if
 * applicable.
 *
 * You can force the TR::InstOpCode::C* + TR::InstOpCode::BRC to be generated by setting the needsCC
 * parameter to true (the default).
 */
TR::Instruction *
generateS390CompareAndBranchInstruction(TR::CodeGenerator * cg,
                                        TR::InstOpCode::Mnemonic compareOpCode,
                                        TR::Node * node,
                                        TR::Register * first ,
                                        TR::Register * second,
                                        TR::InstOpCode::S390BranchCondition bc,
                                        TR::LabelSymbol * branchDestination,
                                        bool needsCC,
                                        bool targetIsFarAndCold)
   {
   // declare a space for the instruction we'll return (the compare and branch
   // instruction, or the branch instruction if z6 support is off).
   TR::Instruction * returnInstruction = NULL;

   // test to see if this node is suitable for compare and branch, and which
   // compare and branch op code to use if so.  if we get TR::InstOpCode::BAD, it isn't
   // suitable for compare and branch, and we'll generate the old fashioned way.
   TR::InstOpCode::Mnemonic replacementOpCode = getReplacementCompareAndBranchOpCode(cg, compareOpCode);

   // if we do not need the CC, we can try to use a compare and branch instruction.
   // compare-and-branch instructions are zEC12 and above
   if( !cg->comp()->getOption(TR_DisableCompareAndBranchInstruction) &&
           !needsCC &&
           replacementOpCode != TR::InstOpCode::BAD &&
           cg->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12))
      {
      // generate a compare and branch.
      returnInstruction = (TR::S390RIEInstruction *)generateRIEInstruction(cg, replacementOpCode, node, first, second, branchDestination, bc);
      }
   // otherwise we'll generate with the compare opcode pased and an TR::InstOpCode::BRC.
   else
      {
      // we'll generate a compare which sets CC in a manner which flags the
      // condition we want to check, indicated by the passed-in opcode and condition.
      // DXL we may have CC from previouse compare op of the same operands, so we don't need to
      TR::Instruction* ccInst = NULL;
      if (cg->hasCCInfo()) ccInst = cg->ccInstruction();
      TR::Compilation *comp = cg->comp();

      TR::InstOpCode opcTmp = TR::InstOpCode(compareOpCode);
      TR_Debug * debugObj = cg->getDebug();
      if (!needsCC && comp->getOption(TR_EnableEBBCCInfo) &&
          cg->isActiveCompareCC(compareOpCode, first, second) &&
          performTransformation(comp, "O^O generateS390CompareAndBranchInstruction case 1 remove RR Compare[%s\t %s, %s]: reuse CC from ccInst [%p].", opcTmp.getMnemonicName(), debugObj->getName(first),debugObj->getName(second),ccInst) )
            returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, bc, node, branchDestination);
      else if ( !needsCC && comp->getOption(TR_EnableEBBCCInfo) &&
                cg->isActiveCompareCC(compareOpCode, second, first) &&
                performTransformation(comp, "O^O generateS390CompareAndBranchInstruction case 2 remove RR Compare[%s\t %s, %s]: reuse CC from ccInst [%p].", opcTmp.getMnemonicName(), debugObj->getName(first),debugObj->getName(second),ccInst) )
            returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, getReverseBranchCondition(bc), node, branchDestination);
      else
         {
         generateRRInstruction(cg, compareOpCode, node, first, second, (TR::RegisterDependencyConditions*) 0);

         // generate a branch on condition such that if CC is set as
         // directed above, we take the branch.
         returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, bc, node, branchDestination);
         }
      }

   return returnInstruction;
   }
   
/**
 * Generate a compare and a branch instruction.  if z10 is available, this will
 * attempt to generate a COMPARE AND BRANCH instruction, otherwise the a
 * compare with the compareOpCode passed and a TR::InstOpCode::BRC with the passed branch
 * condition will be generated.
 *
 * When not generating a COMPARE AND BRANCH instruction, a new register may be allocated
 * to contain the immediate value, which may need to be added into register dependencies.
 *
 * You are responsible for setting start and end of internal control flow if
 * applicable.
 *
 * You can force the TR::InstOpCode::C* + TR::InstOpCode::BRC to be generated by setting the needsCC
 * parameter to true (the default).
 */
template <typename imm32Or64Bit>
TR::Instruction *
generateS390CompareAndBranchInstruction(TR::CodeGenerator * cg,
                                        TR::InstOpCode::Mnemonic compareOpCode,
                                        TR::Node * node,
                                        TR::Register * first ,
                                        imm32Or64Bit second,
                                        TR::InstOpCode::S390BranchCondition bc,
                                        TR::LabelSymbol * branchDestination,
                                        bool needsCC,
                                        bool targetIsFarAndCold,
                                        TR::Instruction *preced,
                                        TR::RegisterDependencyConditions * cond)
   {
   // declare a space for the instruction we'll return (the compare and branch
   // instruction, or the branch instruction if z6 support is off).
   TR::Instruction * cursor = NULL;
   TR::InstOpCode::Mnemonic replacementOpCode = TR::InstOpCode::BAD;

   // test to see if this node is suitable for compare and branch, and which
   // compare and branch op code to use if so.  if we get TR::InstOpCode::BAD, it isn't
   // suitable for compare and branch, and we'll generate the old fashioned way.
   bool canUseReplacementOpCode = false;
   switch(compareOpCode)
   {
   case TR::InstOpCode::CLR:
   case TR::InstOpCode::CLGR:
   case TR::InstOpCode::CL:
   case TR::InstOpCode::CLG:
      if(second == (second & 0xFF))
         canUseReplacementOpCode = true;
   default:
      if ((second >= MIN_IMMEDIATE_BYTE_VAL) && (second <= MAX_IMMEDIATE_BYTE_VAL))
         canUseReplacementOpCode = true;
   }
   if (canUseReplacementOpCode)
      replacementOpCode = getReplacementCompareAndBranchOpCode(cg, compareOpCode);

   // if we do not need the CC, we can try to use a compare and branch instruction.
   // compare-and-branch instructions are zEC12 and above
   if( !cg->comp()->getOption(TR_DisableCompareAndBranchInstruction) &&
           !needsCC &&
           replacementOpCode != TR::InstOpCode::BAD &&
           cg->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12))
      {
      cursor = (TR::S390RIEInstruction *)generateRIEInstruction(cg, replacementOpCode, node, first, (int8_t) second, branchDestination, bc, preced);
      }
   // otherwise we'll generate with the compare opcode pased and an TR::InstOpCode::BRC.
   else
      {
      // we'll generate a compare which sets CC in a manner which flags the
      // condition we want to check, indicated by the passed-in opcode and condition.
      cursor = generateS390ImmOp(cg, compareOpCode, node, first, first, second, cond, NULL, preced);

      // generate a branch on condition such that if CC is set as
      // directed above, we take the branch.
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, bc, node, branchDestination, cursor);
      }
   return cursor;
   }


TR::Instruction *
generateS390RegInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RegInstruction(op, n, treg, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RegInstruction(op, n, treg, cg);
   }

TR::Instruction *
generateS390RegInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg,
                           TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RegInstruction(op, n, treg, cond, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RegInstruction(op, n, treg, cond, cg);
   }

TR::Instruction *
generateRRInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                      TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RRInstruction(op, n, treg, sreg, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RRInstruction(op, n, treg, sreg, cg);

   return instr;
   }

TR::Instruction *
generateRRInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, int8_t secondConstant,
                      TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RRInstruction(op, n, treg, secondConstant, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RRInstruction(op, n, treg, secondConstant, cg);
   }

TR::Instruction *
generateRRInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, int8_t firstConstant, int8_t secondConstant,
                      TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RRInstruction(op, n, firstConstant, secondConstant, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RRInstruction(op, n, firstConstant, secondConstant, cg);
   }

TR::Instruction *
generateRRInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, int8_t firstConstant, TR::Register * sreg,
                      TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RRInstruction(op, n, firstConstant, sreg, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RRInstruction(op, n, firstConstant, sreg, cg);
   }

TR::Instruction *
generateRRInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                      TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   TR::Instruction *instr;

   if (preced)
      instr = new (INSN_HEAP) TR::S390RRInstruction(op, n, treg, sreg, cond, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RRInstruction(op, n, treg, sreg, cond, cg);

   return instr;
   }

TR::Instruction *
generateRRDInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       TR::Register * sreg2, TR::Instruction * preced)
   {
   bool encodeAsRRD = true;
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RRFInstruction(encodeAsRRD, op, n, treg, sreg, sreg2, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RRFInstruction(encodeAsRRD, op, n, treg, sreg, sreg2, cg);
   }

TR::Instruction *
generateRRFInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint8_t mask, bool isMask3,
                       TR::Instruction * preced)
   {
   return new (INSN_HEAP) TR::S390RRFInstruction(op, n, mask, isMask3, cg);
   }

TR::Instruction *
generateRRFInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       TR::Register * sreg2, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RRFInstruction(op, n, treg, sreg, sreg2, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RRFInstruction(op, n, treg, sreg, sreg2, cg);
   }

TR::Instruction *
generateRRFInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       uint8_t mask, bool isMask3, TR::Instruction * preced)
   {
   return new (INSN_HEAP) TR::S390RRFInstruction(op, n, treg, sreg, mask, isMask3, cg);
   }

TR::Instruction *
generateRRFInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       uint8_t mask, bool isMask3, TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   return new (INSN_HEAP) TR::S390RRFInstruction(op, n, treg, sreg, mask, isMask3, cond, cg);
   }

TR::Instruction *
generateRRFInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       TR::Register * sreg2, uint8_t mask, TR::Instruction * preced)
   {
   return new (INSN_HEAP) TR::S390RRFInstruction(op, n, treg, sreg, sreg2, mask, cg);
   }

TR::Instruction *
generateRRFInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg,
                       TR::Register * sreg, uint8_t mask3, uint8_t mask4, TR::Instruction * preced)
   {
   return new (INSN_HEAP) TR::S390RRFInstruction(op, n, treg, sreg, mask3, mask4, cg);
   }

TR::Instruction *
generateRRRInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       TR::Register * sreg2,  TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RRRInstruction(op, n, treg, sreg, sreg2, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RRRInstruction(op, n, treg, sreg, sreg2, cg);
   }

/**
 * Iterate through conds1 and conds2, checking for a specific merge conflict:
 * This function returns true if two dependency conditions exist with the same real register but different virtual registers
 * If this function returns true, it can optionally output the two conflicting register dependency objects.
 * Otherwise, this function returns false.
 * Optionally, the caller can limit the search for conflicts containing only one type of real register.
 */
bool CheckForRegisterDependencyConditionsRealRegisterMergeConflict( TR_S390RegisterDependencyGroup * conds1,
                                                                    int conds1_addCursor,
                                                                    TR_S390RegisterDependencyGroup * conds2,
                                                                    int conds2_addCursor,
                                                                    TR::RegisterDependency ** conflict1,
                                                                    TR::RegisterDependency ** conflict2,
                                                                    TR::CodeGenerator *cg,
                                                                    TR::RealRegister::RegNum checkForThisRealRegister = TR::RealRegister::NoReg)
   {
   for( int i = 0; i < conds1_addCursor; i++ )
      {
      TR::RealRegister::RegNum conds1_real = conds1->getRegisterDependency( i )->getRealRegister();

      if(( checkForThisRealRegister != TR::RealRegister::NoReg ) && ( conds1_real != checkForThisRealRegister ))
         {
         continue;
         }

      for( int j = 0; j < conds2_addCursor; j++ )
         {
         TR::RealRegister::RegNum conds2_real = conds2->getRegisterDependency( j )->getRealRegister();

         if(( checkForThisRealRegister != TR::RealRegister::NoReg ) && ( conds2_real != checkForThisRealRegister ))
            {
            continue;
            }

         if(( conds1_real == conds2_real ) &&
             ( conds1->getRegisterDependency(i)->getRegister() != conds2->getRegisterDependency(j)->getRegister() ))
            {
            // Conflict found
            if( conflict1 && conflict2 )
               {
               *conflict1 = conds1->getRegisterDependency( i );
               *conflict2 = conds2->getRegisterDependency( j );
               }

            return true;
            }
         }
      }

   return false;
   }

TR::Instruction *
generateRXInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::MemoryReference * mf,
                      TR::Instruction * preced)
   {
   TR::Compilation* comp = cg->comp();

   TR_ASSERT(treg->getRealRegister()!=NULL || // Not in RA
           op != TR::InstOpCode::L || !n->isExtendedTo64BitAtSource(), "Generating an TR::InstOpCode::L, when LLGF|LGF should be used");

   // Handle long displacement if necessary
   op = getReplacementLongDisplacementOpCode(cg, op, mf);

   TR::Instruction* result = NULL;

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == RXa_FORMAT)
      {
      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RXInstruction(op, n, treg, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RXInstruction(op, n, treg, mf, cg);
      }
   else
      {
      TR_ASSERT_FATAL(instructionFormat == RXYa_FORMAT, "Mnemonic (%s) is incorrectly used as an RXY instruction", TR::InstOpCode::metadata[op].name);

      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RXYInstruction(op, n, treg, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RXYInstruction(op, n, treg, mf, cg);
      }

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::CVB || op == TR::InstOpCode::CVBY || op == TR::InstOpCode::CVBG || op == TR::InstOpCode::EX)
      {
      // NOP padding needed only for RX instructions which are CVB and EX
      const bool isNOPNeeded = op == TR::InstOpCode::CVB || op == TR::InstOpCode::EX;

      generateS390DAAExceptionRestoreSnippet(cg, n, result, op, isNOPNeeded);
      }
#endif

   return result;
   }

TR::Instruction*
generateRXInstruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* n, uint8_t mask, TR::MemoryReference* mf, TR::Instruction* preced)
   {
   // Handle long displacement if necessary
   op = getReplacementLongDisplacementOpCode(cg, op, mf);

   TR::Instruction* result = NULL;

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == RXb_FORMAT)
      {
      TR_ASSERT_FATAL(false, "RX-b format instructions for mnemonic (%s) are currently not implemented", TR::InstOpCode::metadata[op].name);
      }
   else
      {
      TR_ASSERT_FATAL(instructionFormat == RXYb_FORMAT, "Mnemonic (%s) is incorrectly used as an RXY instruction", TR::InstOpCode::metadata[op].name);

      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RXYbInstruction(op, n, mask, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RXYbInstruction(op, n, mask, mf, cg);
      }

   return result;
   }

TR::Instruction *
generateRXEInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register *treg, TR::MemoryReference *mf,
                       uint8_t mask3, TR::Instruction *preced)
   {
   TR::Instruction * instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXEInstruction(op, n, treg, mf, mask3, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RXEInstruction(op, n, treg, mf, mask3, cg);

   return instr;
   }

TR::Instruction *
generateRXFInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       TR::MemoryReference * mf, TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXFInstruction(op, n, treg, sreg, mf, preced, cg);
   else
           instr = new (INSN_HEAP) TR::S390RXFInstruction(op, n, treg, sreg, mf, cg);

   return instr;
   }

TR::Instruction *
generateRIInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Instruction * preced)
    {
    if (preced)
       {
       return new (INSN_HEAP) TR::S390RIInstruction(op, n, preced, cg);
       }
    return new (INSN_HEAP) TR::S390RIInstruction(op, n, cg);
    }

TR::Instruction *
generateRIInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Instruction * preced)
    {
    if (preced)
       {
       return new (INSN_HEAP) TR::S390RIInstruction(op, n, treg, preced, cg);
       }
    return new (INSN_HEAP) TR::S390RIInstruction(op, n, treg, cg);
    }

TR::Instruction *
generateRIInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, int32_t imm, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RIInstruction(op, n, treg, imm, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RIInstruction(op, n, treg, imm, cg);
   }

TR::Instruction *
generateRIInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, char *data, TR::Instruction * preced)
    {
    if (preced)
       {
       return new (INSN_HEAP) TR::S390RIInstruction(op, n, treg, data, preced, cg);
       }
    return new (INSN_HEAP) TR::S390RIInstruction(op, n, treg, data, cg);
    }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::SymbolReference * sr, void * addr, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, addr, sr, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, addr, sr, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::LabelSymbol * label, TR::Instruction * preced)
   {
   if (preced)
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, label, preced, cg);
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, label, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t imm, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, int32_t imm, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, void * addr, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, addr, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, addr, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t mask, void * addr, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, mask, addr, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, mask, addr, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg, TR::Snippet *ts, TR::Instruction *preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, ts, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, ts, cg);
   }


TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t imm, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, imm, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, imm, cg);
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t imm,
                      TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, imm, cond, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, imm, cond, cg);
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::MemoryReference * mf,
                      TR::Instruction * preced)
   {
   // RS and RSY instructions do not have an index register
   preced = mf->separateIndexRegister(n, cg, false, preced);

   // Handle long displacement if necessary
   op = getReplacementLongDisplacementOpCode(cg, op, mf);

   TR::Instruction* result = NULL;

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == RSa_FORMAT)
      {
      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mf, cg);
      }
   else
      {
      TR_ASSERT_FATAL(instructionFormat == RSYa_FORMAT, "Mnemonic (%s) is incorrectly used as an RSY instruction", TR::InstOpCode::metadata[op].name);

      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, static_cast<uint32_t>(0), mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, static_cast<uint32_t>(0), mf, cg);
      }

   return result;
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t mask,
                      TR::MemoryReference * mf, TR::Instruction * preced)
   {
   // RS and RSY instructions do not have an index register
   preced = mf->separateIndexRegister(n, cg, false, preced);

   // Handle long displacement if necessary
   op = getReplacementLongDisplacementOpCode(cg, op, mf);

   TR::Instruction* result = NULL;

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == RSb_FORMAT)
      {
      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mask, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mask, mf, cg);
      }
   else
      {
      TR_ASSERT_FATAL(instructionFormat == RSYb_FORMAT, "Mnemonic (%s) is incorrectly used as an RSY instruction", TR::InstOpCode::metadata[op].name);

      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, mask, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, mask, mf, cg);
      }

   return result;
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * treg, TR::RegisterPair * sreg,
                      TR::MemoryReference * mf, TR::Instruction * preced)
   {
   // RS and RSY instructions do not have an index register
   preced = mf->separateIndexRegister(n, cg, false, preced);

   // Handle long displacement if necessary
   op = getReplacementLongDisplacementOpCode(cg, op, mf);

   TR::Instruction* result = NULL;

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == RSa_FORMAT)
      {
      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, sreg, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, sreg, mf, cg);
      }
   else
      {
      TR_ASSERT_FATAL(instructionFormat == RSYa_FORMAT, "Mnemonic (%s) is incorrectly used as an RSY instruction", TR::InstOpCode::metadata[op].name);

      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, sreg, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, sreg, mf, cg);
      }

   return result;
   }


TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * freg, TR::Register * lreg,
                      TR::MemoryReference * mf, TR::Instruction * preced)
   {
   // RS and RSY instructions do not have an index register
   preced = mf->separateIndexRegister(n, cg, false, preced);

   // Handle long displacement if necessary
   op = getReplacementLongDisplacementOpCode(cg, op, mf);

   TR::Instruction* result = NULL;

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == RSa_FORMAT)
      {
      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSInstruction(op, n, freg, lreg, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSInstruction(op, n, freg, lreg, mf, cg);
      }
   else
      {
      TR_ASSERT_FATAL(instructionFormat == RSYa_FORMAT, "Mnemonic (%s) is incorrectly used as an RSY instruction", TR::InstOpCode::metadata[op].name);

      result = preced != NULL ?
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, freg, lreg, mf, preced, cg) :
         new (INSN_HEAP) TR::S390RSYInstruction(op, n, freg, lreg, mf, cg);
      }

   return result;
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * regp, TR::MemoryReference * mf,
                      TR::Instruction * preced)
   {
   return generateRSInstruction(cg, op, n, static_cast<TR::Register*>(regp), mf, preced);
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg, uint32_t imm,
                      TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, sreg, imm, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, sreg, imm, cg);
   }


TR::Instruction *
generateRSWithImplicitPairStoresInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * treg, TR::RegisterPair * sreg,
                                            TR::MemoryReference * mf, TR::Instruction * preced)
   {
   TR_ASSERT((op == TR::InstOpCode::CLCLE) || (op == TR::InstOpCode::MVCLE) || (op == TR::InstOpCode::CLCLU) || (op == TR::InstOpCode::MVCLU),
          "generateRSWithImplicitPairStoresInstruction is only to be use with CLCLE, MVCLE, CLCLU, and MVCLU");

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSWithImplicitPairStoresInstruction(op, n, treg, sreg, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSWithImplicitPairStoresInstruction(op, n, treg, sreg, mf, cg);

   return instr;
   }

TR::Instruction *
generateRRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg, TR::MemoryReference * branch, TR::InstOpCode::S390BranchCondition cond, TR::Instruction * preced)
   {
   if (branch) preced = branch->separateIndexRegister(n, cg, true, preced); // enforce4KDisplacementLimit=true (no Y form)

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RRSInstruction(op, n, treg, sreg, branch, cond, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RRSInstruction(op, n, treg, sreg, branch, cond, cg);

   return instr;
   }

TR::Instruction *
generateRREInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg,
   TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RREInstruction(op, n, treg, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RREInstruction(op, n, treg, cg);
   return instr;
   }

TR::Instruction *
generateRREInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RREInstruction(op, n, treg, sreg, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RREInstruction(op, n, treg, sreg, cg);


   return instr;
   }

TR::Instruction *
generateRREInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg,
                       TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RREInstruction(op, n, treg, sreg, cond, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RREInstruction(op, n, treg, sreg, cond, cg);


   return instr;
   }

TR::Instruction * generateRIEInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg, TR::LabelSymbol * branch, TR::InstOpCode::S390BranchCondition mask, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sreg, branch, mask, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sreg, branch, mask, cg);
   }

TR::Instruction * generateRIEInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, int8_t immCompare, TR::LabelSymbol * branch, TR::InstOpCode::S390BranchCondition mask, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, immCompare, branch, mask, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, immCompare, branch, mask, cg);
   }

TR::Instruction * generateRIEInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg, int8_t immOne, int8_t immTwo, int8_t immThree, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sreg, immOne, immTwo, immThree, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sreg, immOne, immTwo, immThree, cg);
   }

TR::Instruction * generateRIEInstruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* n, TR::Register * treg, int16_t sourceImmediate, TR::InstOpCode::S390BranchCondition branchCondition, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sourceImmediate, branchCondition, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sourceImmediate, branchCondition, cg);
   }

TR::Instruction * generateRIEInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::Register * sreg, int16_t imm, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sreg, imm, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RIEInstruction(op, n, treg, sreg, imm, cg);
   }

TR::Instruction * generateRISInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * leftSide, int8_t immCompare, TR::MemoryReference * branch, TR::InstOpCode::S390BranchCondition cond, TR::Instruction * preced)
   {
   if (branch) preced = branch->separateIndexRegister(n, cg, true, preced); // enforce4KDisplacementLimit=true (no Y form)

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RISInstruction(op, n, leftSide, immCompare, branch, cond, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RISInstruction(op, n, leftSide, immCompare, branch, cond, cg);

   return instr;
   }

TR::Instruction *
generateS390MemInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference * mf,
                           TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390MemInstruction(op, n, mf, preced, cg);
   else
      instr =new (INSN_HEAP) TR::S390MemInstruction(op, n, mf, cg);

   return instr;
   }

TR::Instruction *
generateS390MemInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, int8_t memAccessMode, TR::MemoryReference * mf,
                           TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390MemInstruction(op, n, memAccessMode, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390MemInstruction(op, n, memAccessMode, mf, cg);

   return instr;
   }

TR::Instruction *
generateS390MemInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, int8_t constantField, int8_t memAccessMode, TR::MemoryReference * mf,
                           TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390MemInstruction(op, n, constantField, memAccessMode, mf, preced, cg);
   else
           instr = new (INSN_HEAP) TR::S390MemInstruction(op, n, constantField, memAccessMode, mf, cg);

   return instr;
   }

TR::Instruction *
generateRSLInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint16_t len, TR::MemoryReference * mf1,
                       TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceRSLFormatLimits(n, cg, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSLInstruction(op, n, len, mf1, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSLInstruction(op, n, len, mf1, cg);

   return instr;
   }

TR::Instruction *
generateRSLbInstruction(TR::CodeGenerator * cg,
                        TR::InstOpCode::Mnemonic op,
                        TR::Node * n,
                        TR::Register *reg,
                        uint16_t length,
                        TR::MemoryReference *mf1,
                        uint8_t mask,
                        TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceRSLFormatLimits(n, cg, preced);

   TR::Instruction *instr = NULL;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSLbInstruction(op, n, reg, length, mf1, mask, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSLbInstruction(op, n, reg, length, mf1, mask, cg);

   return instr;
   }

TR::Instruction *
generateSS1Instruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint16_t len, TR::MemoryReference * mf1,
                       TR::MemoryReference * mf2, TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   TR::Instruction *instr = NULL;
   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   if (preced)
      instr = new (INSN_HEAP) TR::S390SS1Instruction(op, n, len, mf1, mf2, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS1Instruction(op, n, len, mf1, mf2, cg);

   return instr;
   }

TR::Instruction *
generateSS1Instruction(TR::CodeGenerator * cg,
                       TR::InstOpCode::Mnemonic op, TR::Node * n, uint16_t len,
                       TR::MemoryReference * mf1,
                       TR::MemoryReference * mf2,
                       TR::RegisterDependencyConditions *cond,
                       TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS1Instruction(op, n, len, mf1, mf2, cond, preced, cg);
   else
           instr = new (INSN_HEAP) TR::S390SS1Instruction(op, n, len, mf1, mf2, cond, cg);

   return instr;
   }

TR::Instruction *
generateSS1WithImplicitGPRsInstruction(TR::CodeGenerator * cg,
                                       TR::InstOpCode::Mnemonic op, TR::Node * n, uint16_t len,
                                       TR::MemoryReference * mf1,
                                       TR::MemoryReference * mf2,
                                       TR::RegisterDependencyConditions *cond,
                                       TR::Register * implicitRegSrc0,
                                       TR::Register * implicitRegSrc1,
                                       TR::Register * implicitRegTrg0,
                                       TR::Register * implicitRegTrg1,
                                       TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS1WithImplicitGPRsInstruction(op, n, len, mf1, mf2, cond, preced, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS1WithImplicitGPRsInstruction(op, n, len, mf1, mf2, cond, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);

   return instr;
   }

TR::Instruction *
generateSS2Instruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint16_t len1, TR::MemoryReference * mf1, uint16_t len2,
                       TR::MemoryReference * mf2, TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS2Instruction(op, n, len1, mf1, len2, mf2, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS2Instruction(op, n, len1, mf1, len2, mf2, cg);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::ZAP ||
           op == TR::InstOpCode::CP ||
           op == TR::InstOpCode::AP ||
           op == TR::InstOpCode::SP ||
           op == TR::InstOpCode::MP ||
           op == TR::InstOpCode::DP)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif

   return instr;
   }

TR::Instruction *
generateSS3Instruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t len, TR::MemoryReference * mf1,
                       TR::MemoryReference * mf2, uint32_t roundAmount, TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS2Instruction(op, n, len, mf1, roundAmount, mf2, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS2Instruction(op, n, len, mf1, roundAmount, mf2, cg);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::SRP)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif
   return instr;
   }

TR::Instruction *
generateSS3Instruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t len, TR::MemoryReference * mf1,
                       int32_t shiftAmount, uint32_t roundAmount, TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS2Instruction(op, n, len, mf1, shiftAmount, roundAmount, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS2Instruction(op, n, len, mf1, shiftAmount, roundAmount, cg);
#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::SRP)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif
   return instr;
   }

TR::Instruction *
generateSS4Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * lengthReg, TR::MemoryReference * mf1,
                       TR::MemoryReference * mf2, TR::Register * sourceKeyReg, TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS4Instruction(op, n, lengthReg, mf1, mf2, sourceKeyReg, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS4Instruction(op, n, lengthReg, mf1, mf2, sourceKeyReg, cg);

   return instr;
   }

TR::Instruction *
generateSS5Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * op1Reg, TR::MemoryReference * mf2,
                       TR::Register * op3Reg, TR::MemoryReference * mf4, TR::Instruction * preced)
   {
   TR::Instruction * instr = generateSS4Instruction(cg, op, n, op1Reg, mf2, mf4, op3Reg, preced);
   return instr;
   }

TR::Instruction *
generateSS5Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * op1Reg, TR::MemoryReference * mf2,
                       TR::Instruction * preced)
   {
   TR::Instruction * instr = generateSS4Instruction(cg, op, n, op1Reg, mf2, NULL, NULL, preced);
   return instr;
   }

TR::Instruction *
generateSS5Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * op1Reg, TR::MemoryReference * mf2,
                       TR::MemoryReference * mf4, TR::Instruction * preced)
   {
   TR::Instruction * instr = generateSS4Instruction(cg, op, n, op1Reg, mf2, mf4, NULL, preced);
   return instr;
   }

TR::Instruction *
generateSS5WithImplicitGPRsInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * op1Reg, TR::MemoryReference * mf2,
                                       TR::Register * op3Reg, TR::MemoryReference * mf4,
                                       TR::Register * implicitRegSrc0,
                                       TR::Register * implicitRegSrc1,
                                       TR::Register * implicitRegTrg0,
                                       TR::Register * implicitRegTrg1,
                                       TR::Instruction * preced)
   {
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);
   if (mf4) preced = mf4->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf2 && mf4 && (mf2->getBaseRegister() == mf4->getBaseRegister());

   if (sameBaseRegister && mf2->getBaseRegister() != mf4->getBaseRegister()) // avoid register copies on restricted regs
      mf4->setBaseRegister(mf2->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS5WithImplicitGPRsInstruction(op, n, op1Reg, mf2, op3Reg, mf4, preced, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS5WithImplicitGPRsInstruction(op, n, op1Reg, mf2, op3Reg, mf4, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);

   return instr;
   }

TR::Instruction *
generateSS5WithImplicitGPRsInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * op1Reg, TR::MemoryReference * mf2,
                                       TR::Register * implicitRegSrc0,
                                       TR::Register * implicitRegSrc1,
                                       TR::Register * implicitRegTrg0,
                                       TR::Register * implicitRegTrg1,
                                       TR::Instruction * preced)
   {
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS5WithImplicitGPRsInstruction(op, n, op1Reg, mf2, NULL, NULL, preced, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS5WithImplicitGPRsInstruction(op, n, op1Reg, mf2, NULL, NULL, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);

   return instr;
   }

TR::Instruction *
generateSS5WithImplicitGPRsInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * op1Reg, TR::MemoryReference * mf2,
                                       TR::MemoryReference * mf4,
                                       TR::Register * implicitRegSrc0,
                                       TR::Register * implicitRegSrc1,
                                       TR::Register * implicitRegTrg0,
                                       TR::Register * implicitRegTrg1,
                                       TR::Instruction * preced)
   {
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);
   if (mf4) preced = mf4->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf2 && mf4 && (mf2->getBaseRegister() == mf4->getBaseRegister());

   if (sameBaseRegister && mf2->getBaseRegister() != mf4->getBaseRegister()) // avoid register copies on restricted regs
      mf4->setBaseRegister(mf2->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SS5WithImplicitGPRsInstruction(op, n, op1Reg, mf2, NULL, mf4, preced, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);
   else
      instr = new (INSN_HEAP) TR::S390SS5WithImplicitGPRsInstruction(op, n, op1Reg, mf2, NULL, mf4, implicitRegSrc0, implicitRegSrc1, implicitRegTrg0, implicitRegTrg1, cg);

   return instr;
   }

TR::Instruction *
generateSSEInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,TR::MemoryReference * mf1,
                       TR::MemoryReference * mf2, TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SSEInstruction(op, n, mf1, mf2, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SSEInstruction(op, n, mf1, mf2, cg);

   return instr;
   }

TR::Instruction *
generateSSFInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * regp, TR::MemoryReference * mf1,
                       TR::MemoryReference * mf2, TR::Instruction * preced)
   {
   if (mf1) preced = mf1->enforceSSFormatLimits(n, cg, preced);
   if (mf2) preced = mf2->enforceSSFormatLimits(n, cg, preced);

   bool sameBaseRegister = mf1 && mf2 && (mf1->getBaseRegister() == mf2->getBaseRegister());
   if (sameBaseRegister && mf1->getBaseRegister() != mf2->getBaseRegister()) // avoid register copies on restricted regs
      mf2->setBaseRegister(mf1->getBaseRegister(), cg);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SSFInstruction(op, n, regp, mf1, mf2, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SSFInstruction(op, n, regp, mf1, mf2, cg);

   return instr;
   }

TR::Instruction *
generateSIInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference * mf, uint32_t imm,
                      TR::Instruction * preced)
   {
   // SI and SIY instructions do not have an index register
   preced = mf->separateIndexRegister(n, cg, false, preced);

   // Handle long displacement if necessary
   op = getReplacementLongDisplacementOpCode(cg, op, mf);

   TR::Instruction* result = NULL;

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == SI_FORMAT)
      {
      result = preced != NULL ?
         new (INSN_HEAP) TR::S390SIInstruction(op, n, mf, imm, preced, cg) :
         new (INSN_HEAP) TR::S390SIInstruction(op, n, mf, imm, cg);
      }
   else
      {
      TR_ASSERT_FATAL(instructionFormat == SIY_FORMAT, "Mnemonic (%s) is incorrectly used as an SIY instruction", TR::InstOpCode::metadata[op].name);

      result = preced != NULL ?
         new (INSN_HEAP) TR::S390SIYInstruction(op, n, mf, imm, preced, cg) :
         new (INSN_HEAP) TR::S390SIYInstruction(op, n, mf, imm, cg);
      }

   return result;
   }

TR::Instruction *
generateSILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference * mf, uint32_t imm, TR::Instruction * preced)
   {
   if (mf->getIndexRegister() != NULL && mf->getBaseRegister() == NULL)
      {
      mf->setBaseRegister(mf->getIndexRegister(), cg);
      mf->setIndexRegister(NULL);
      }
   else if (mf->getIndexRegister() && mf->getBaseRegister())
      {
      if (mf) preced = mf->separateIndexRegister(n, cg, true, preced); // enforce4KDisplacementLimit=true (no Y form)
      }

   if (mf && !cg->afterRA())
      preced = mf->enforce4KDisplacementLimit(n, cg, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SILInstruction(op, n, mf, imm, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SILInstruction(op, n, mf, imm, cg);

   return instr;
   }

TR::Instruction *
generateSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference * mf, TR::Instruction * preced)
   {
   if (mf) preced = mf->separateIndexRegister(n, cg, true, preced); // enforce4KDisplacementLimit=true (no Y form)

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SInstruction(op, n, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SInstruction(op, n, mf, cg);

   return instr;
   }

TR::Instruction *
generateSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n,TR::Instruction * preced)
   {
   TR::Instruction * instr = 0;

   if (preced)
      {
      instr = new (INSN_HEAP) TR::S390OpCodeOnlyInstruction(op, n, preced, cg);
      }
   else
      {
      instr = new (INSN_HEAP) TR::S390OpCodeOnlyInstruction(op, n, cg);
      }

   return instr;
   }

TR::Instruction *
generateRRInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Instruction *preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390OpCodeOnlyInstruction(op, n, preced, cg);
      }
   return new (INSN_HEAP) TR::S390OpCodeOnlyInstruction(op, n, cg);
   }

/*********************************** Vector Instructions *******************************************/

/**
 * Note: We leave the mask fields (3,4,5,6) to the consumer of these methods since their use varies significantly between
 *      VRI: a,b,c,d,e,f,g,h,i
 *      VRR: a,b,c,d,e,f,g,h,i
 *      VRS: a,b,c,d
 * ..and even within each of them.
 *
 * The naming convention for parameters follows zPops.
 *      bitSet            B
 *      displacement      D
 *      constantIMM       I
 *      mask              M
 *      source/targetReg  V/R depending on instruction
 *
 * Unless specified (such as in VRS-b and VRS-c), source/target registers are VRFs.
 *
 * Data-sizes are mentioned in bits wherever non-obvious (e.g some displacement fields are 12 bit while others are 16 bit)
 * Sanity checking will be done but care must be taken while using them to prevent over-flow
 */
TR::Instruction *
generateVRIaInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, uint16_t constantImm2 /* 16 bits */,
                        uint8_t mask3)
   {
   return new (INSN_HEAP) TR::S390VRIaInstruction(cg, op, n, targetReg, constantImm2, mask3);
   }

TR::Instruction *
generateVRIbInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg,
                        uint8_t constantImm2 /* 8 or 16 bits */, uint8_t constantImm3 /* 12 bits */, uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRIbInstruction(cg, op, n, targetReg, constantImm2, constantImm3, mask4);
   }

TR::Instruction *
generateVRIcInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg3,
                        uint16_t constantImm2 /* 8 or 16 bits */, uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRIcInstruction(cg, op, n, targetReg, sourceReg3, constantImm2, mask4);
   }

TR::Instruction *
generateVRIdInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        TR::Register * sourceReg3, uint8_t constantImm4 /* 8 bit */, uint8_t mask5 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRIdInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3, constantImm4, mask5);
   }

TR::Instruction *
generateVRIeInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        uint16_t constantImm3 /* 12 bits */, uint8_t mask5 /* 4 bits */, uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRIeInstruction(cg, op, n, targetReg, sourceReg2, constantImm3, mask5, mask4);
   }

TR::Instruction *
generateVRIfInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      TR::Register           * sourceReg2,
                      TR::Register           * sourceReg3,
                      uint8_t                constantImm4,   /* 8 bits  */
                      uint8_t                 mask5   )         /* 4 bits */
   {
   TR::Instruction* instr = new (INSN_HEAP) TR::S390VRIfInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3, constantImm4, mask5);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::VAP ||
           op == TR::InstOpCode::VSP ||
           op == TR::InstOpCode::VDP ||
           op == TR::InstOpCode::VMP ||
           op == TR::InstOpCode::VMSP ||
           op == TR::InstOpCode::VSDP ||
           op == TR::InstOpCode::VRP)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif

   return instr;
   }

TR::Instruction *
generateVRIgInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      TR::Register           * sourceReg2,
                      uint8_t                constantImm3,   /* 8 bits  */
                      uint8_t                constantImm4,   /* 8 bits  */
                      uint8_t                 mask5   )     /* 4 bits  */
   {
   TR::Instruction* instr = new (INSN_HEAP) TR::S390VRIgInstruction(cg, op, n, targetReg, sourceReg2, constantImm3, constantImm4, mask5);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::VPSOP || op == TR::InstOpCode::VSRP)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif

   return instr;
   }

TR::Instruction *
generateVRIhInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      uint16_t               constantImm2,   /* 16 bits  */
                      uint8_t                constantImm3 )  /* 4 bits  */
   {
   return new (INSN_HEAP) TR::S390VRIhInstruction(cg, op, n, targetReg, constantImm2, constantImm3);
   }

TR::Instruction *
generateVRIiInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      TR::Register           * sourceReg2,
                      uint8_t                constantImm3,   /* 8 bits  */
                      uint8_t                 mask4)         /* 4 bits  */
   {
   TR::Instruction* instr =  new (INSN_HEAP) TR::S390VRIiInstruction(cg, op, n, targetReg, sourceReg2, constantImm3, mask4);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::VCVD || op == TR::InstOpCode::VCVDG)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif

   return instr;
   }

/****** VRR ******/
TR::Instruction *
generateVRRaInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        uint8_t mask5 /* 4 bits */, uint8_t mask4 /* 4 bits */, uint8_t mask3 /* 4 bits */,
                        TR::Instruction * preced)
   {
   if (preced)
      return new (INSN_HEAP) TR::S390VRRaInstruction(cg, op, n, targetReg, sourceReg2, mask5, mask4, mask3, preced);
   else
      return new (INSN_HEAP) TR::S390VRRaInstruction(cg, op, n, targetReg, sourceReg2, mask5, mask4, mask3);
   }

TR::Instruction *
generateVRRaInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        TR::Instruction * preced)
   {
   if (preced)
      return new (INSN_HEAP) TR::S390VRRaInstruction(cg, op, n, targetReg, sourceReg2, 0 /* mask5 */, 0 /* mask4 */, 0 /* mask3 */, preced);
   else
      return new (INSN_HEAP) TR::S390VRRaInstruction(cg, op, n, targetReg, sourceReg2, 0 /* mask5 */, 0 /* mask4 */, 0 /* mask3 */);
   }

TR::Instruction *
generateVRRbInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        TR::Register * sourceReg3, uint8_t mask5 /* 4 bits */, uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRRbInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3, mask5, mask4);
   }

TR::Instruction *
generateVRRcInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        TR::Register * sourceReg3, uint8_t mask6 /* 4 bits */, uint8_t mask5 /* 4 bits */, uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRRcInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3, mask6, mask5, mask4);
   }

TR::Instruction *
generateVRRcInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        TR::Register * sourceReg3, uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRRcInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3, 0, 0, mask4);
   }

TR::Instruction *
generateVRRdInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        TR::Register * sourceReg3, TR::Register * sourceReg4, uint8_t mask6 /* 4 bits */, uint8_t mask5 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRRdInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3, sourceReg4, mask6, mask5);
   }

TR::Instruction *
generateVRReInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2,
                        TR::Register * sourceReg3, TR::Register * sourceReg4, uint8_t mask6 /* 4 bits */, uint8_t mask5 /* 4 bits */)

   {
   return new (INSN_HEAP) TR::S390VRReInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3, sourceReg4, mask6, mask5);
   }

TR::Instruction *
generateVRRfInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg2 /* GPR */,
                        TR::Register * sourceReg3 /* GPR */)
   {
   return new (INSN_HEAP) TR::S390VRRfInstruction(cg, op, n, targetReg, sourceReg2, sourceReg3);
   }


TR::Instruction *
generateVRRgInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic  op         ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  )   /* Vector register */
   {
   return new (INSN_HEAP) TR::S390VRRgInstruction(cg, op, n, targetReg);
   }


TR::Instruction *
generateVRRhInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic  op         ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  ,    /* Vector register */
                      TR::Register            * sourceReg2 ,    /* Vector register */
                      uint8_t                   mask3)          /* 4 bits*/
   {
   TR::Instruction* instr = new (INSN_HEAP) TR::S390VRRhInstruction(cg, op, n, targetReg, sourceReg2, mask3);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::VCP)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif

   return instr;
   }

TR::Instruction * generateVRRiInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic  op         ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  ,    /* GPR */
                      TR::Register            * sourceReg2 ,    /* VRF */
                      uint8_t                   mask3      ,    /* 4 bits*/
                      uint8_t                   mask4)
   {
   TR::Instruction* instr = new (INSN_HEAP) TR::S390VRRiInstruction(cg, op, n, targetReg, sourceReg2, mask3, mask4);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::VCVB || op == TR::InstOpCode::VCVBG)
      {
      generateS390DAAExceptionRestoreSnippet(cg, n, instr, op, false);
      }
#endif

   return instr;
   }

/****** VRS ******/
/* Note subtle differences between register types and optionality of masks between these 3*/
TR::Instruction *
generateVRSaInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg, TR::MemoryReference * mr,
                        uint8_t mask4 /* 4 bits */, TR::Instruction* preced)
   {
   preced = mr->enforceSSFormatLimits(n, cg, preced);

   if (preced)
      return new (INSN_HEAP) TR::S390VRSaInstruction(cg, op, n, targetReg, sourceReg, mr, mask4, preced);
   else
      return new (INSN_HEAP) TR::S390VRSaInstruction(cg, op, n, targetReg, sourceReg, mr, mask4);
   }

TR::Instruction *
generateVRSbInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg, TR::MemoryReference * mr,
                        uint8_t mask4 /* 4 bits */, TR::Instruction* preced)
   {
   preced = mr->enforceSSFormatLimits(n, cg, preced);

   if (preced)
      return new (INSN_HEAP) TR::S390VRSbInstruction(cg, op, n, targetReg, sourceReg, mr, mask4, preced);
   else
      return new (INSN_HEAP) TR::S390VRSbInstruction(cg, op, n, targetReg, sourceReg, mr, mask4);
   }

TR::Instruction *
generateVRScInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg, TR::MemoryReference * mr,
                        uint8_t mask4 /* 4 bits */, TR::Instruction* preced)
   {
   preced = mr->enforceSSFormatLimits(n, cg, preced);

   if (preced)
      return new (INSN_HEAP) TR::S390VRScInstruction(cg, op, n, targetReg, sourceReg, mr, mask4, preced);
   else
      return new (INSN_HEAP) TR::S390VRScInstruction(cg, op, n, targetReg, sourceReg, mr, mask4);
   }

TR::Instruction *
generateVRSdInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic op          ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  ,   /* VRF */
                      TR::Register            * sourceReg3 ,   /* GPR R3 */
                      TR::MemoryReference     * mr         ,
                      TR::Instruction         * preced)
   {
   preced = mr->enforceSSFormatLimits(n, cg, preced);

   if (preced)
      return new (INSN_HEAP) TR::S390VRSdInstruction(cg, op, n, targetReg, sourceReg3, mr, preced);
   else
      return new (INSN_HEAP) TR::S390VRSdInstruction(cg, op, n, targetReg, sourceReg3, mr);
   }

/****** VRV ******/
TR::Instruction *
generateVRVInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register *sourceReg, TR::MemoryReference * mr,
                       uint8_t mask3 /* 4 bits */, TR::Instruction* preced)
   {
   preced = mr->enforceSSFormatLimits(n, cg, preced);

   if (preced != NULL)
      return new (INSN_HEAP) TR::S390VRVInstruction(cg, op, n, sourceReg, mr, mask3, preced);
   else
      return new (INSN_HEAP) TR::S390VRVInstruction(cg, op, n, sourceReg, mr, mask3);
   }

/****** VRX ******/
TR::Instruction *
generateVRXInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * reg,
                       TR::MemoryReference * memRef, uint8_t mask3, TR::Instruction * preced)
   {
   preced = memRef->enforceVRXFormatLimits(n, cg, preced);

   if (preced)
      return new (INSN_HEAP) TR::S390VRXInstruction(cg, op, n, reg, memRef, mask3, preced);
   else
      return new (INSN_HEAP) TR::S390VRXInstruction(cg, op, n, reg, memRef, mask3);
   }

/****** VSI ******/
TR::Instruction * generateVSIInstruction(
                      TR::CodeGenerator      * cg ,
                      TR::InstOpCode::Mnemonic op ,
                      TR::Node               * n  ,
                      TR::Register           * reg,   /* VRF */
                      TR::MemoryReference    * mr ,
                      uint8_t                  imm3,    /* 8 bits */
                      TR::Instruction        * preced)
   {
   preced = mr->enforceSSFormatLimits(n, cg, preced);

   if (preced != NULL)
      return new (INSN_HEAP) TR::S390VSIInstruction(cg, op, n, reg, mr, imm3, preced);
   else
      return new (INSN_HEAP) TR::S390VSIInstruction(cg, op, n, reg, mr, imm3);
   }

/************************************************************ Misc Instructions ************************************************************/
TR::Instruction*
generateS390PseudoInstruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* n, TR::Node* fenceNode, TR::Instruction* preced)
   {
   TR::Instruction* cursor;

   if (preced)
      {
      cursor = new (INSN_HEAP) TR::S390PseudoInstruction(op, n, fenceNode, preced, cg);
      }
   else
      {
      cursor = new (INSN_HEAP) TR::S390PseudoInstruction(op, n, fenceNode, cg);
      }

   return cursor;
   }

TR::Instruction *
generateS390PseudoInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterDependencyConditions * cond,
                              TR::Node * fenceNode, TR::Instruction * preced)
   {
   TR::S390PseudoInstruction *instr;
   TR::Compilation *comp = cg->comp();

   if (preced)
      {
      instr=new (INSN_HEAP) TR::S390PseudoInstruction(op, n, fenceNode, cond, preced, cg);
      }
   else
           {
      instr= new (INSN_HEAP) TR::S390PseudoInstruction(op, n, fenceNode, cond, cg);
      }

   return instr;
   }

TR::Instruction *
generateS390PseudoInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, int32_t regNum, TR::Node *fenceNode, TR::Instruction * preced)
   {
   TR_ASSERT(0, "WCode only");
   return NULL;
   }

TR::Instruction *
generateS390PseudoInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterDependencyConditions * cond,
                              int32_t regNum, TR::Node *fenceNode, TR::Instruction * preced)
   {
   TR_ASSERT(0, "WCode only");
   return NULL;
   }

/**
 * Generate a debug counter bump pseudo instruction
 *
 * @param cg      Code Generator Pointer
 * @param op      Runtime Instrumentation opcode: TR::InstOpCode::DCB
 * @param n       The associated node
 * @param cs      The snippet that holds the debugCounter's counter address in persistent memory
 * @param preced  A pointer to the preceding instruction. It is required if this is called after register assignment
 */
TR::Instruction *
generateS390DebugCounterBumpInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Snippet* cs, int32_t delta, TR::Instruction * preced)
   {
   if (preced)
      {
         return new (INSN_HEAP) TR::S390DebugCounterBumpInstruction(op, n, cs, cg, delta, preced);
      }
   return new (INSN_HEAP) TR::S390DebugCounterBumpInstruction(op, n, cs, cg, delta);
   }

TR::Instruction *
generateS390ImmSymInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t imm, TR::SymbolReference * sr,
                              TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390ImmSymInstruction(op, n, imm, sr, cond, preced, cg);
      }
   return new (INSN_HEAP) TR::S390ImmSymInstruction(op, n, imm, sr, cond, cg);
   }

TR::Instruction *
generateShiftRightImmediate(TR::CodeGenerator *cg, TR::Node *node, TR::Register *trgReg, TR::Register *srcReg, int32_t imm, TR::Instruction *preced)
   {
   TR::Instruction *instr = NULL;
   if (cg->comp()->target().is64Bit())
      {
      instr = generateRSInstruction(cg, TR::InstOpCode::SRAG, node, trgReg, srcReg, imm, preced);
      }
   else
      {
      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
         {
         instr = generateRSInstruction(cg, TR::InstOpCode::SRAK, node, trgReg, srcReg, imm, preced);
         }
      else
         {
         instr = generateRRInstruction(cg, TR::InstOpCode::LR, node, trgReg, srcReg, preced);
         instr = generateRSInstruction(cg, TR::InstOpCode::SRA, node, trgReg, imm, instr);
         }
      }
   return instr;
   }

#ifdef J9_PROJECT_SPECIFIC
TR::Instruction *
generateVirtualGuardNOPInstruction(TR::CodeGenerator * cg, TR::Node * n, TR_VirtualGuardSite * site,
                                   TR::RegisterDependencyConditions * cond, TR::LabelSymbol * sym, TR::Instruction * preced)
   {
   TR::S390BranchInstruction * cursor ;
   if (preced)
      cursor = new (INSN_HEAP) TR::S390VirtualGuardNOPInstruction(n, site, cond, sym, preced, cg);
   else
      cursor =  new (INSN_HEAP) TR::S390VirtualGuardNOPInstruction(n, site, cond, sym, cg);
   return cursor;
   }
#endif

////////////////////////////////////////////////////////////////////////////////
// Helper Routines for call
////////////////////////////////////////////////////////////////////////////////

/**
 * generateDirectCall - generate branch and link with a given address (direct call)
 */
TR::Instruction *
generateDirectCall(TR::CodeGenerator * cg, TR::Node * callNode, bool myself, TR::SymbolReference * callSymRef,
                   TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   TR::Compilation *comp = cg->comp();
   // G5
   TR::Register * RegEP = cond->searchPostConditionRegister(cg->getEntryPointRegister());
   TR::Register * RegRA = cond->searchPostConditionRegister(cg->getReturnAddressRegister());

   bool isHelper = callSymRef->getSymbol()->castToMethodSymbol()->isHelper();

   // Direct calls on 64-bit systems may require a trampoline. As the trampoline will kill the EP register we should
   // always see the EP register defined in the post-dependencies of such calls.
#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      TR_ASSERT(RegEP != NULL, "Trampoline support on 64-bit systems requires EP register to be defined.\n");
      }
#endif

   // If the EP is not specified, we assume re-use of the RA.
   // e.g.  BASR R14,R14
   if (RegEP == NULL)
      {
      RegEP = RegRA;
      }
   TR_ASSERT( RegEP != NULL,"generateDirectCall: Undefined entry point register\n");

   TR::Symbol * sym = callSymRef->getSymbol();
   uintptr_t imm = 0;

   // if it is not calling myself, get the method address
   if (!myself)
      {
      imm = (uintptr_t) callSymRef->getMethodAddress();
      }

   AOTcgDiag2(comp, "\nimm=%x isHelper=%x\n", imm, isHelper);

   // Since N3 generate TR::InstOpCode::BRASL -- only need 1 instruction, and no worry
   // about the displacement
   // Calling myself
   // address is unknown at this point so we need to pass the symbol to the
   // instruction and calculate the address in generateBinary phase
   if (myself)
      {
      TR::Instruction * instr = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, sym, callSymRef, cg);

      return instr;
      }
   else // address known
      {
#if !defined(TR_TARGET_64BIT) || (defined(TR_TARGET_64BIT) && defined(J9ZOS390))
      if (cg->canUseRelativeLongInstructions(imm) || isHelper || comp->getOption(TR_EnableRMODE64))
#endif
         {
         if (!isHelper && cg->supportsBranchPreloadForCalls())
            {
            static int minFR = (feGetEnv("TR_minFR") != NULL) ? atoi(feGetEnv("TR_minFR")) : 0;
            static int maxFR = (feGetEnv("TR_maxFR") != NULL) ? atoi(feGetEnv("TR_maxFR")) : 0;

            int32_t frequency = comp->getCurrentBlock()->getFrequency();
            if (frequency > 6 && frequency >= minFR && (maxFR == 0 || frequency > maxFR))
               {
               TR::LabelSymbol * callLabel = generateLabelSymbol(cg);
               TR::Instruction * instr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, callNode, callLabel);
               cg->createBranchPreloadCallData(callLabel, callSymRef, instr);
               }
            }

         TR::S390RILInstruction *tempInst;
#if defined(TR_TARGET_64BIT)
#if defined (J9ZOS390)
         if (comp->getOption(TR_EnableRMODE64))
#endif
            {
            tempInst = (new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, reinterpret_cast<void*>(imm), callSymRef, cg));
            }
#endif
#if !defined(TR_TARGET_64BIT) || (defined(TR_TARGET_64BIT) && defined(J9ZOS390))
#if (defined(TR_TARGET_64BIT) && defined(J9ZOS390))
         if (!comp->getOption(TR_EnableRMODE64))
#endif

            {
            tempInst = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, reinterpret_cast<void*>(imm), cg);


            if (isHelper)
               tempInst->setSymbolReference(callSymRef);
            }
#endif
         AOTcgDiag1(comp, "\ntempInst=%x\n", tempInst);
         return tempInst;
         }
#if !defined(TR_TARGET_64BIT) || (defined(TR_TARGET_64BIT) && defined(J9ZOS390))
      else
         {
         genLoadAddressConstant(cg, callNode, imm, RegEP, preced, cond);
         TR::Instruction * instr = new (INSN_HEAP) TR::S390RRInstruction(TR::InstOpCode::BASR, callNode, RegRA, RegEP, cg);

         return instr;
         }
#endif

      }
   }

TR::Instruction* generateDataConstantInstruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* node, uint32_t data, TR::Instruction* preced)
   {
   if (preced != NULL)
      {
      return new (INSN_HEAP) TR::S390ImmInstruction(op, node, data, preced, cg);
      }
   else
      {
      return new (INSN_HEAP) TR::S390ImmInstruction(op, node, data, cg);
      }
   }

/**
 * generateSnippetCall - branch and link to a snippet (PicBuilder call)
 */
TR::Instruction *
generateSnippetCall(TR::CodeGenerator * cg, TR::Node * callNode, TR::Snippet * s, TR::RegisterDependencyConditions * cond, TR::SymbolReference *callSymRef, TR::Instruction * preced)
   {
   TR::Register * RegRA = cond->searchPostConditionRegister(cg->getReturnAddressRegister());

   TR::Instruction * callInstr;

   if (s->getKind() == TR::Snippet::IsVirtualUnresolved)
      {
      TR::RegisterDependencyConditions * preDeps = new (INSN_HEAP)
         TR::RegisterDependencyConditions(cond->getPreConditions(), NULL,
            cond->getAddCursorForPre(), 0, cg);

      TR::RegisterDependencyConditions *postDeps = new (INSN_HEAP) TR::RegisterDependencyConditions(0, 2, cg);
      TR::Register * killRegRA = cg->allocateRegister();
      TR::Register * killRegEP = cg->allocateRegister();
      postDeps->addPostCondition(killRegRA, cg->getReturnAddressRegister());
      postDeps->addPostCondition(killRegEP, cg->getEntryPointRegister());

      // Need to put the preDeps on the label, and not on the BRASL
      // because we use virtual reg from preDeps after the BRASL
      // In particular, we use the this pointer reg, which  has a preDep to GPR1
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, callNode, generateLabelSymbol(cg), preDeps);

      callInstr = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, killRegRA, s,
         postDeps, callSymRef, cg);

      cg->stopUsingRegister(killRegRA);
      cg->stopUsingRegister(killRegEP);
      }
   else
      {
      callInstr = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, s, cond, callSymRef, cg);
      }

   ((TR::S390CallSnippet *) s)->setBranchInstruction(callInstr);
   return callInstr;
   }

////////////////////////////////////////////////////////////////////////////////
// Helper Routines for literal pool generation
////////////////////////////////////////////////////////////////////////////////

/**
 * Generate a Lit pool entry, address it using a mem ref to be applied to targetReg.
 * For 4 byte integer constant
 */
TR::Instruction *
generateLoadLiteralPoolAddress(TR::CodeGenerator * cg, TR::Node * node, TR::Register * treg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Instruction *cursor;

   //support f/w only so far
   TR::S390RILInstruction *LARLinst = (TR::S390RILInstruction *) generateRILInstruction(cg, TR::InstOpCode::LARL, node, treg, reinterpret_cast<void*>(0xBABE), 0);
   LARLinst->setIsLiteralPoolAddress();
   cursor = LARLinst;

   treg->setIsUsedInMemRef();

   // Add a comment on to the instrurction
   //
   TR_Debug * debugObj = cg->getDebug();
   if (debugObj)
      {
      debugObj->addInstructionComment(cursor, "LoadLitPool");
      }

   return  cursor;
   }

TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, int32_t imm,
                             TR::RegisterDependencyConditions * cond, TR::Instruction * preced, TR::Register * base, bool isPICCandidate)
   {
   TR::Compilation *comp = cg->comp();
   bool alloc = false;
   TR::Instruction * cursor;
   TR::S390ConstantDataSnippet * targetsnippet = 0;
   TR::MemoryReference * dataref = 0;
   TR::S390RILInstruction *LRLinst = 0;
   if (cg->isLiteralPoolOnDemandOn() && (base == 0))
      {
      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && op == TR::InstOpCode::L)
         {
         targetsnippet = cg->findOrCreate4ByteConstant(node, imm);
         LRLinst = (TR::S390RILInstruction *) generateRILInstruction(cg, TR::InstOpCode::LRL, node, treg, targetsnippet, 0);
         cursor = LRLinst;
         }
      else
         {
         base = cg->allocateRegister();
         if (cond != 0)
            {
            cond->addPostCondition(base, TR::RealRegister::AssignAny);
            }
         generateLoadLiteralPoolAddress(cg, node, base);
         alloc = true;
         }
      }
   else if (!cg->isLiteralPoolOnDemandOn())
      {
      base = NULL;
      }
   if (!LRLinst)
      {
      dataref = generateS390MemoryReference(imm, TR::Int32, cg, base);
      targetsnippet = dataref->getConstantDataSnippet();
      cursor = generateRXInstruction(cg, op, node, treg, dataref);
      }
   // HCR in generateRegLitRefInstruction 32-bit: register const data snippet for common case
   if (comp->getOption(TR_EnableHCR) && isPICCandidate )
      {
      comp->getSnippetsToBePatchedOnClassRedefinition()->push_front(targetsnippet);
      if (node->isClassUnloadingConst())
         {
         TR_OpaqueClassBlock* unloadableClass = NULL;
         bool isMethod = node->getOpCodeValue() == TR::loadaddr ? false : node->isMethodPointerConstant();
       if (isMethod)
            {
            unloadableClass = (TR_OpaqueClassBlock *) cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *)(intptr_t)imm,
               comp->getCurrentMethod())->classOfMethod();
            if (!TR::Compiler->cls.sameClassLoaders(comp, unloadableClass, comp->getCurrentMethod()->classOfMethod()))
               comp->getMethodSnippetsToBePatchedOnClassUnload()->push_front(targetsnippet);
            }
         else
            {
            unloadableClass = (TR_OpaqueClassBlock *) (intptr_t)imm;
            if (!TR::Compiler->cls.sameClassLoaders(comp, unloadableClass, comp->getCurrentMethod()->classOfMethod()))
               comp->getSnippetsToBePatchedOnClassUnload()->push_front(targetsnippet);
            }
         }
      }
   if (alloc)
      {
      cg->stopUsingRegister(base);
      }
   if (!LRLinst)
      {
      dataref->stopUsingMemRefRegister(cg);
      }

   return cursor;
   }

/**
 * For AOT relocatable Symbol
 */
TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, uintptr_t imm, int32_t reloType,
                             TR::RegisterDependencyConditions * cond, TR::Instruction * preced, TR::Register * base)
   {
   bool alloc = false;
   TR::Instruction * cursor;
   TR::Compilation *comp = cg->comp();

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      if (op == TR::InstOpCode::LG || op == TR::InstOpCode::L)
         {
         TR::S390ConstantDataSnippet *targetSnippet = op == TR::InstOpCode::LG ?
               targetSnippet = cg->findOrCreate8ByteConstant(node, (int64_t)imm) :
               targetSnippet = cg->findOrCreate4ByteConstant(node, (int32_t)imm);

         targetSnippet->setSymbolReference(new (INSN_HEAP) TR::SymbolReference(comp->getSymRefTab()));
         targetSnippet->setReloType(reloType);
         AOTcgDiag4(comp, "generateRegLitRefInstruction constantDataSnippet=%x symbolReference=%x symbol=%x reloType=%x\n", targetSnippet, targetSnippet->getSymbolReference(), targetSnippet->getSymbolReference()->getSymbol(), reloType);

         cursor = (TR::S390RILInstruction *) generateRILInstruction(cg, (op == TR::InstOpCode::LG)?TR::InstOpCode::LGRL:TR::InstOpCode::LRL, node, treg, targetSnippet, preced);
         return cursor;
         }
      }


   TR::MemoryReference * dataref;
   if (cg->isLiteralPoolOnDemandOn() && (base == 0))
      {
      base = cg->allocateRegister();
      if (cond != 0)
         {
         cond->addPostCondition(base, TR::RealRegister::AssignAny);
         }
      generateLoadLiteralPoolAddress(cg, node, base);
      alloc = true;
      }
   else if (!cg->isLiteralPoolOnDemandOn())
      {
      base = NULL;
      }
   if (cg->comp()->target().is64Bit())
      {
      dataref = generateS390MemoryReference((int64_t)imm, TR::Int64, cg, base, node);
      }
   else
      {
      dataref = generateS390MemoryReference((int32_t)imm, TR::Int32, cg, base, node);
      }
   AOTcgDiag5(comp, "generateRegLitRefInstruction dataref=%x constantDataSnippet=%x symbolReference=%x symbol=%x reloType=%x\n",
      dataref, dataref->getConstantDataSnippet(), dataref->getSymbolReference(),
      dataref->getSymbolReference()->getSymbol(), reloType);
   dataref->getConstantDataSnippet()->setSymbolReference(dataref->getSymbolReference());
   dataref->getConstantDataSnippet()->setReloType(reloType);
   cursor = generateRXInstruction(cg, op, node, treg, dataref);
   if (alloc)
      {
      cg->stopUsingRegister(base);
      }
   dataref->stopUsingMemRefRegister(cg);

   return cursor;
   }

/**
 * For snippets
 */
TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, TR::Snippet* snippet,
                             TR::RegisterDependencyConditions * cond, TR::Instruction * preced, TR::Register * base)
   {

   bool alloc = false;
   TR::Instruction * cursor;
   if (cg->isLiteralPoolOnDemandOn() && (base == 0))
      {
      base = cg->allocateRegister();
      if (cond != 0)
         {
         cond->addPostCondition(base, TR::RealRegister::AssignAny);
         }
      generateLoadLiteralPoolAddress(cg, node, base);
      alloc = true;
      }
   else if (!cg->isLiteralPoolOnDemandOn())
      {
      base = NULL;
      }
   TR::MemoryReference * dataref = generateS390MemoryReference(snippet, cg, base, node);
   cursor = generateRXInstruction(cg, op, node, treg, dataref);
   if (alloc)
      {
      cg->stopUsingRegister(base);
      }
   dataref->stopUsingMemRefRegister(cg);

   return cursor;
   }

/**
 * For 8 byte integer constant
 */
TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, int64_t imm,
                             TR::RegisterDependencyConditions * cond, TR::Instruction * preced, TR::Register * base, bool isPICCandidate)
   {
   bool alloc = false;
   TR::Instruction * cursor;
   TR::S390ConstantDataSnippet * targetsnippet = 0;
   TR::MemoryReference * dataref = 0;
   TR::S390RILInstruction *LGRLinst = 0;
   TR::Compilation *comp = cg->comp();

   auto instructionFormat = TR::InstOpCode(op).getInstructionFormat();

   if (instructionFormat == RILa_FORMAT ||
       instructionFormat == RILb_FORMAT ||
       instructionFormat == RILc_FORMAT)
      {
      TR::S390ConstantDataSnippet * constDataSnip = cg->create64BitLiteralPoolSnippet(TR::Int64, imm);

      // HCR in generateRegLitRefInstruction 64-bit: register const data snippet used by z10
      if (comp->getOption(TR_EnableHCR) && isPICCandidate)
         {
         comp->getSnippetsToBePatchedOnClassRedefinition()->push_front(constDataSnip);
         if (node->isClassUnloadingConst())
            {
            TR_OpaqueClassBlock* unloadableClass = NULL;
            bool isMethod = node->getOpCodeValue() == TR::loadaddr ? false : node->isMethodPointerConstant();
            if (isMethod)
               {
               unloadableClass = (TR_OpaqueClassBlock *) cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) imm,
                  comp->getCurrentMethod())->classOfMethod();
               if (!TR::Compiler->cls.sameClassLoaders(comp, unloadableClass, comp->getCurrentMethod()->classOfMethod()))
                  comp->getMethodSnippetsToBePatchedOnClassUnload()->push_front(constDataSnip);
               }
            else
               {
               unloadableClass = (TR_OpaqueClassBlock *) imm;
               if (!TR::Compiler->cls.sameClassLoaders(comp, unloadableClass, comp->getCurrentMethod()->classOfMethod()))
                  comp->getSnippetsToBePatchedOnClassUnload()->push_front(constDataSnip);
               }
            }
         }

      cursor = new (INSN_HEAP) TR::S390RILInstruction(op, node, treg, constDataSnip, cg);

      return cursor;
      }
   else if (cg->isLiteralPoolOnDemandOn() && (base == 0))
      {
      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && op == TR::InstOpCode::LG)
         {
         targetsnippet = cg->findOrCreate8ByteConstant(node, imm);
         LGRLinst = (TR::S390RILInstruction *) generateRILInstruction(cg, TR::InstOpCode::LGRL, node, treg, targetsnippet, 0);
         cursor = LGRLinst;
         }
      else
         {
         base = cg->allocateRegister();
         if (cond != 0)
            {
            cond->addPostCondition(base, TR::RealRegister::AssignAny);
            }
         generateLoadLiteralPoolAddress(cg, node, base);
         alloc = true;
         }
      }
   else if (!cg->isLiteralPoolOnDemandOn())
      {
      base = NULL;
      }
   if (!LGRLinst)
      {
      dataref = generateS390MemoryReference(imm, TR::Int64, cg, base);
      targetsnippet = dataref->getConstantDataSnippet();
      }
   // HCR in generateRegLitRefInstruction 64-bit: register const data snippet for common case
   if (comp->getOption(TR_EnableHCR) && isPICCandidate )
      {
      comp->getSnippetsToBePatchedOnClassRedefinition()->push_front(targetsnippet);
      if (node->isClassUnloadingConst())
         {
         TR_OpaqueClassBlock* unloadableClass = NULL;
         bool isMethod = node->getOpCodeValue() == TR::loadaddr ? false : node->isMethodPointerConstant();
         if (isMethod)
            {
            unloadableClass = (TR_OpaqueClassBlock *) cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) imm,
               comp->getCurrentMethod())->classOfMethod();
            if (!TR::Compiler->cls.sameClassLoaders(comp, unloadableClass, comp->getCurrentMethod()->classOfMethod()))
               comp->getMethodSnippetsToBePatchedOnClassUnload()->push_front(targetsnippet);
            }
         else
            {
            unloadableClass = (TR_OpaqueClassBlock *) imm;
            if (!TR::Compiler->cls.sameClassLoaders(comp, unloadableClass, comp->getCurrentMethod()->classOfMethod()))
               comp->getSnippetsToBePatchedOnClassUnload()->push_front(targetsnippet);
            }
         }
      }

   if (!LGRLinst)
      {
      cursor = instructionFormat == RXE_FORMAT ?
         generateRXEInstruction(cg, op, node, treg, dataref, 0) :
         generateRXInstruction(cg, op, node, treg, dataref);
      }
   if (alloc)
      {
      cg->stopUsingRegister(base);
      }
   if (!LGRLinst)
      {
      dataref->stopUsingMemRefRegister(cg);
      }

   return cursor;
   }

/**
 * For unsigned integer pointer constant
 */
TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, uintptr_t imm,
                             TR::RegisterDependencyConditions * cond, TR::Instruction * preced, TR::Register * base, bool isPICCandidate)
   {
   if (cg->comp()->target().is64Bit())
      {
      return generateRegLitRefInstruction(cg, op, node, treg, (int64_t) imm, cond, preced, base, isPICCandidate);
      }
   else
      {
      return generateRegLitRefInstruction(cg, op, node, treg, (int32_t) imm, cond, preced, base, isPICCandidate);
      }
   }
/**
 * For float constant
 */
TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, float imm,
                             TR::Instruction * preced)
   {

   TR::MemoryReference * dataref = generateS390MemoryReference(imm, TR::Float, cg, node);
   return generateRXInstruction(cg, op, node, treg, dataref);
   }

/**
 * For double constant
 */
TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, double imm,
                             TR::Instruction * preced)
   {

   TR::MemoryReference * dataref = generateS390MemoryReference(imm, TR::Double, cg, node);
   return generateRXInstruction(cg, op, node, treg, dataref);
   }

////////////////////////////////////////////////////////////////////////////////
// Helper Routines for loading/storing unresolved reference symbol
////////////////////////////////////////////////////////////////////////////////
/**
 * Generate a snippet address entry, address it using a mem ref to be applied to targetReg.
 */
TR::Instruction *
generateRegUnresolvedSym(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, TR::SymbolReference * symRef,
                         TR::UnresolvedDataSnippet * uds, TR::Instruction * preced)
   {
   TR::Symbol * symbol = symRef->getSymbol();

   TR_ASSERT(symRef->isUnresolved(), "SymRef must be unresolved.");

   // allocate link register for data resolution
   // it is used in the picbuilder to hold the return address of the main code cache
   TR::RegisterDependencyConditions * deps;
   TR::Register * tempReg = cg->allocateRegister();
   TR::Instruction * gcPoint;

   // Return address register is used in the snippet code so we need to kill it after the call
   deps = new (INSN_HEAP) TR::RegisterDependencyConditions(0, 1, cg);
   deps->addPostCondition(tempReg, cg->getReturnAddressRegister());

   TR::Register * tempReg2 = treg;
   if (node->getOpCode().isStore() || tempReg2 == NULL)
      {
      tempReg2 = cg->allocateRegister();
      }

   // load and branch to the address of the unresolved data snippet
   // these two instructions will be overwritten by a branch around after the data is resolved

   TR::Register * treg2 = (tempReg2->getRegisterPair() != NULL) ? tempReg2->getLowOrder() : tempReg2;
   //Since N3 instructions supportted, only need a single BRCL instr.
   gcPoint = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRCL, node, 0xF, uds, deps, cg);

   gcPoint->setNeedsGCMap(0xFFFFFFFF);
   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(tempReg2);
   return gcPoint;
   }


TR::Instruction *
generateEXDispatch(TR::Node * node, TR::CodeGenerator *cg, TR::Register * maskReg, TR::Register * useThisLitPoolReg, TR::Instruction * instr, TR::Instruction * preced, TR::RegisterDependencyConditions *deps)
   {
   instr->setOutOfLineEX();

   // We need to grab all registers from the EX itself and the EX target instruction and attach them as post dependencies to the EX inst
   // This is to make sure that RA assigns all these registers when we assign the EX instruction
   // Otherwise RA may insert register movement instructions attached to the out of line (lit pool) EX target instruction.

   instr->setUseDefRegisters(false);

   int32_t depsNeeded = 0;
   int32_t depsUsed = 0;
   int32_t i = 0;
   int32_t j = 0;

   TR::Compilation *comp = cg->comp();

   while (instr->getSourceRegister(i))
      {
      if (instr->getSourceRegister(i)->getRealRegister() == NULL &&
          instr->getSourceRegister(i)->getAssignedRegister() == NULL)
         {
         depsNeeded++;
         }
      i++;
      }

   while (instr->getTargetRegister(j))
      {
      if (instr->getTargetRegister(j)->getRealRegister() == NULL &&
          instr->getTargetRegister(j)->getAssignedRegister() == NULL)
         {
         depsNeeded++;
         }
      j++;
      }

   TR::MemoryReference * instrMR = instr->getMemoryReference();

   if (instrMR)
      {
      if (instrMR->getBaseRegister() &&
          instrMR->getBaseRegister()->getRealRegister() == NULL &&
          instrMR->getBaseRegister()->getAssignedRegister() == NULL)
         {
         depsNeeded++;
         }
      if (instrMR->getIndexRegister() &&
          instrMR->getIndexRegister()->getRealRegister() == NULL &&
          instrMR->getIndexRegister()->getAssignedRegister() == NULL)
         {
         depsNeeded++;
         }
      }

   depsNeeded += 3; // + 3 for EX itself
   TR::RegisterDependencyConditions * conditions;

   if (deps == NULL)
      conditions = new (INSN_HEAP) TR::RegisterDependencyConditions(0, depsNeeded, cg);
   else
      conditions = deps;

   i = 0;
   while (instr->getSourceRegister(i))
      {
      if (instr->getSourceRegister(i)->getRealRegister() == NULL &&
          instr->getSourceRegister(i)->getAssignedRegister() == NULL)
         {
         TR::Register * reg = instr->getSourceRegister(i);
         conditions->addPostConditionIfNotAlreadyInserted(reg, TR::RealRegister::AssignAny);
         depsUsed++;
         }
      i++;
      }

   j = 0;
   while (instr->getTargetRegister(j))
      {
      if (instr->getTargetRegister(j)->getRealRegister() == NULL &&
          instr->getTargetRegister(j)->getAssignedRegister() == NULL)
         {
         TR::Register * reg = instr->getTargetRegister(j);
         conditions->addPostConditionIfNotAlreadyInserted(reg, TR::RealRegister::AssignAny);
         depsUsed++;
         }
      j++;
      }

   if (instrMR)
      {
      if (instrMR->getBaseRegister() &&
          instrMR->getBaseRegister()->getRealRegister() == NULL &&
          instrMR->getBaseRegister()->getAssignedRegister() == NULL)
         {
         TR::Register * reg = instrMR->getBaseRegister();
         conditions->addPostConditionIfNotAlreadyInserted(reg, TR::RealRegister::AssignAny);
         depsUsed++;
         }
      if (instrMR->getIndexRegister() &&
          instrMR->getIndexRegister()->getRealRegister() == NULL &&
          instrMR->getIndexRegister()->getAssignedRegister() == NULL)
         {
         TR::Register * reg = instrMR->getIndexRegister();
         conditions->addPostConditionIfNotAlreadyInserted(reg, TR::RealRegister::AssignAny);
         depsUsed++;
         }
      }

   instr->resetUseDefRegisters();

   TR::Instruction * cursor = NULL;

   static char * disableEXRLDispatch = feGetEnv("TR_DisableEXRLDispatch");

      {
      TR::Register *litPool = NULL;

      if (useThisLitPoolReg == NULL)
         litPool = cg->allocateRegister();
      else
         litPool = useThisLitPoolReg;

      // Only generate a LARL if we haven't locked down the literal pool base into a register
      if (cg->isLiteralPoolOnDemandOn() || useThisLitPoolReg != cg->getLitPoolRealRegister())
         generateLoadLiteralPoolAddress(cg, node, litPool);

      if (litPool->getRealRegister() == NULL && litPool->getAssignedRegister() == NULL)
         {
         conditions->addPostConditionIfNotAlreadyInserted(litPool, TR::RealRegister::AssignAny);
         depsUsed++;
         }

      //create a memory reference to that instruction
      TR::S390ConstantInstructionSnippet * cis = cg->createConstantInstruction(cg, node, instr);
      TR::MemoryReference * tempMR = generateS390MemoryReference(cis, cg, litPool, node);

      //the memory reference should create a constant data snippet
      cursor = generateRXInstruction(cg, TR::InstOpCode::EX, node, maskReg, tempMR, (preced != NULL ? preced : cg->getAppendInstruction()));
      if (maskReg->getRealRegister() == NULL && maskReg->getAssignedRegister() == NULL)
         {
         conditions->addPostConditionIfNotAlreadyInserted(maskReg, TR::RealRegister::AssignAny);
         depsUsed++;
         }

      cursor->setOutOfLineEX();
      if (depsUsed > 0)
         {
         cursor->setDependencyConditions(conditions);
         }

      // Unhook instr from the instruction stream
      instr->remove();

      cg->stopUsingRegister(litPool);
      }

   return cursor;
   }

TR::Instruction *
generateEXDispatch(TR::Node * node, TR::CodeGenerator *cg, TR::Register * maskReg, TR::Instruction * instr, TR::Instruction * preced, TR::RegisterDependencyConditions *deps)
   {
   return generateEXDispatch(node, cg, maskReg, NULL, instr, preced, deps);
   }

TR::Instruction *
generateS390EInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390EInstruction(op, n, preced, cg);
      }
   return new (INSN_HEAP) TR::S390EInstruction(op, n,  cg);
   }

TR::Instruction *
generateS390EInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register * tgt, TR::Register * tgt2, TR::Register * src, TR::Register * src2,
TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390EInstruction(op, n, preced, cg, tgt, tgt2, src, src2 ,cond);
      }
   return new (INSN_HEAP) TR::S390EInstruction(op, n,  cg, tgt, tgt2, src, src2, cond);
   }

TR::Instruction *
generateS390IInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, uint8_t im, TR::Node *n, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390IInstruction(op, n, im, preced, cg);
      }
   return new (INSN_HEAP) TR::S390IInstruction(op,n,im,cg);
   }

TR::Instruction *
generateRuntimeInstrumentationInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node, TR::Register *target, TR::Instruction *preced)
   {
   if (preced != NULL)
      if (target != NULL)
         return new (INSN_HEAP) TR::S390RIInstruction(op, node, target, preced, cg);
      else
         return new (INSN_HEAP) TR::S390RIInstruction(op, node, preced, cg);
   else
      if (target != NULL)
         return new (INSN_HEAP) TR::S390RIInstruction(op, node, target, cg);
      else
         return new (INSN_HEAP) TR::S390RIInstruction(op, node, cg);
   }

TR::Instruction *
generateS390IEInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, uint8_t im1, uint8_t im2, TR::Node *n, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390IEInstruction(op, n, im1, im2, preced, cg);
      }
   return new (INSN_HEAP) TR::S390IEInstruction(op, n, im1, im2, cg);
   }

TR::Instruction *
generateS390BranchPredictionRelativePreloadInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
                                                       TR::LabelSymbol * sym, uint8_t mask, TR::SymbolReference *sym3, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (cg->trHeapMemory()) TR::S390MIIInstruction(op, n, mask, sym, sym3, preced, cg);
      }
   return new (cg->trHeapMemory()) TR::S390MIIInstruction(op, n, mask, sym, sym3, cg);
   }

TR::Instruction *
generateS390BranchPredictionPreloadInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
                                               TR::LabelSymbol * sym, uint8_t mask, TR::MemoryReference *mf3, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390SMIInstruction(op, n, mask, sym, mf3, preced, cg);
      }
   return new (INSN_HEAP) TR::S390SMIInstruction(op, n, mask, sym, mf3, cg);
   }

TR::Instruction *
generateSerializationInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Instruction *preced)
   {
   // BCR R15, 0 is the defacto serialization instruction on Z, however on z196, a fast serialization
   // facilty was added, and hence BCR R14, 0 is preferred
   TR::InstOpCode::S390BranchCondition cond = cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196) ? TR::InstOpCode::COND_MASK14 : TR::InstOpCode::COND_MASK15;

   // We needed some special handling in TR::Instruction::assignRegisterNoDependencies
   // to recognize real register GPR0 being passed in.
   TR::RealRegister *gpr0 = cg->machine()->getRealRegister(TR::RealRegister::GPR0);

   TR::Instruction * instr = NULL;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RegInstruction(TR::InstOpCode::BCR, node, cond, gpr0, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RegInstruction(TR::InstOpCode::BCR, node, cond, gpr0, cg);

   return instr;
   }

/**
 * @deprecated - DO NOT USE. Use vsplatsEvaluator instead
 * Replicates node containing element to the given target vector register.
 * Can reuse zeroed GPR for old fall-back method which may be necessary for non-const, direct loads.
 * Consumer must handle refcount of nodes
 */
TR::Instruction *
generateReplicateNodeInVectorReg(TR::Node * node, TR::CodeGenerator *cg, TR::Register * targetVRF, TR::Node * srcElementNode,
                                 int elementSize, TR::Register *zeroReg, TR::Instruction * preced)
   {
   int mask;
   switch(elementSize)
      {
      case 1: mask = 0; break;
      case 2: mask = 1; break;
      case 4: mask = 2; break;
      case 8: mask = 3; break;
      default: TR_ASSERT(false, "Unhandled element size %i\n", elementSize); break;
      }

   TR::Instruction *cursor;
   if(srcElementNode->getOpCode().isLoadConst())
      {
      if(srcElementNode->getDataType() == TR::Double)
         {
         TR::MemoryReference *doubleMR = generateS390MemoryReference(srcElementNode->getDouble(), TR::Double, cg, node);
         cursor = generateVRXInstruction(cg, TR::InstOpCode::VLREP, node, targetVRF, doubleMR, mask, preced);
         }
      else if(srcElementNode->getIntegerNodeValue<uint64_t>() <= (uint64_t)(uint16_t)(-1))      //if source is integral loadconst with value <= max uint16_t
         {
         cursor = generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, targetVRF, srcElementNode->getIntegerNodeValue<uint16_t>(), mask);
         }
      else
         {
         TR::MemoryReference *constMR = generateS390MemoryReference(srcElementNode->getIntegerNodeValue<int64_t>(), TR::Int64, cg, NULL, node);
         cursor = generateVRXInstruction(cg, TR::InstOpCode::VLREP, node, targetVRF, constMR, mask, preced);
         }
      }
   else if(srcElementNode->getOpCode().isLoadIndirect())
      {
      TR::Node *addr = srcElementNode->getChild(0);
      TR::MemoryReference *addrMR = generateS390MemoryReference(cg->evaluate(addr), 0, cg);
      cursor = generateVRXInstruction(cg, TR::InstOpCode::VLREP, node, targetVRF, addrMR, mask, preced);
      cg->decReferenceCount(addr);
      }
   else       //old fall back method
      {
      bool allocatedNewZeroReg = false;
      if(zeroReg == NULL)
         {
         allocatedNewZeroReg = true;
         zeroReg = cg->allocateRegister();
         }
      generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, zeroReg, zeroReg);
      // Load first element (index = 0)
      if(srcElementNode->getDataType() == TR::Double)
         {
         TR::MemoryReference* loadMR = generateS390MemoryReference(srcElementNode->getDouble(), TR::Double, cg, node);
         generateVRXInstruction(cg, TR::InstOpCode::VLEG, node, targetVRF, loadMR, 0, NULL);

         // Node is not evaluated, so explicitly decrement
         cg->decReferenceCount(srcElementNode);
         }
      else
         {
         TR::Register *srcReg = cg->evaluate(srcElementNode);

         // On 31-bit an 8-byte sized child may come in a register pair so we have to handle this case specially
         if(srcReg->getRegisterPair() != NULL)
            {
            if (mask == 3)
               {
               generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, targetVRF, srcReg->getHighOrder(), generateS390MemoryReference(zeroReg, 0, cg), 2);
               generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, targetVRF, srcReg->getLowOrder(), generateS390MemoryReference(zeroReg, 1, cg), 2);
               }
            else
               generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, targetVRF, srcReg->getLowOrder(), generateS390MemoryReference(zeroReg, 0, cg), mask);
            }
         else
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, targetVRF, srcReg, generateS390MemoryReference(zeroReg, 0, cg), mask);
         }

      // Replicate to rest of the indices
      cursor = generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, targetVRF, targetVRF, 0, mask);
      if(allocatedNewZeroReg)
         cg->stopUsingRegister(zeroReg);
      }

   return cursor;
   }

/**
 * Rotate the second register and put its selected bits into the first register
 * with an option to clear the rest of first register's bits.
 */
void generateShiftAndKeepSelected64Bit(TR::Node * node, TR::CodeGenerator *cg,
                                       TR::Register * aFirstRegister, TR::Register * aSecondRegister,
                                       int aFromBit, int aToBit, int aShiftAmount, bool aClearOtherBits, bool aSetConditionCode)
   {
   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12) && !aSetConditionCode)
      {
      generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, aFirstRegister, aSecondRegister, aFromBit, aToBit|(aClearOtherBits ? 0x80 : 0x00), aShiftAmount);
      }
   else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, aFirstRegister, aSecondRegister, aFromBit, aToBit|(aClearOtherBits ? 0x80 : 0x00), aShiftAmount);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, aFirstRegister, aSecondRegister, (63 - aToBit) - aShiftAmount);
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, aFirstRegister, aFirstRegister, (63 - aToBit) + aFromBit);
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, aFirstRegister, aFirstRegister, aFromBit);
      }
   }

/**
 * Rotate the second register and put its selected bits into the first register
 * with an option to clear the rest of first register's bits.
 */
void
generateShiftAndKeepSelected31Bit(TR::Node * node, TR::CodeGenerator *cg,
                                  TR::Register * aFirstRegister, TR::Register * aSecondRegister,
                                  int aFromBit, int aToBit, int aShiftAmount, bool aClearOtherBits, bool aSetConditionCode)
   {
   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, aFirstRegister, aSecondRegister, aFromBit, aToBit|(aClearOtherBits ? 0x80 : 0x00), aShiftAmount);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LR, node, aFirstRegister, aSecondRegister);
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, aFirstRegister, (31 - aToBit) - aShiftAmount);
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, aFirstRegister, (31 - aToBit) + aFromBit);
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, aFirstRegister, aFromBit);
      }
   }

TR::Instruction *generateZeroVector(TR::Node *node, TR::CodeGenerator *cg, TR::Register *vecZeroReg)
   {
   return generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vecZeroReg, 0, 3);
   }

#ifdef J9_PROJECT_SPECIFIC

/**
 * \brief
 *    Determines if an instruction can throw a decimal overflow exceptions.
 *
 * \param cg
 *    The code generator used to generate the instructions.
 *
 * \param op
 *    The instruction to check.
 *
 * \return
 *    true if \p op is designed to throw decimal overflow exception according to the PoP; false otherwise.
 *
 * \details
 *    Linux on z runs a process with this exception disabled. As a result of this, the JIT signal handler
 *    does not receive a signal in case decimal overflow exception happens. This helper function decides
 *    whether to rely on condition code in such cases.
*/
bool
canThrowDecimalOverflowException(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op)
   {
   switch(op)
      {
      // Decimal Overflow exception instructions
      case TR::InstOpCode::AP:
      case TR::InstOpCode::SP:
      case TR::InstOpCode::SRP:
      case TR::InstOpCode::ZAP:
      case TR::InstOpCode::VAP:
      case TR::InstOpCode::VCVD:
      case TR::InstOpCode::VCVDG:
      case TR::InstOpCode::VDP:
      case TR::InstOpCode::VMP:
      case TR::InstOpCode::VMSP:
      case TR::InstOpCode::VPSOP:
      case TR::InstOpCode::VRP:
      case TR::InstOpCode::VSDP:
      case TR::InstOpCode::VSRP:
      case TR::InstOpCode::VSP:
      // Fixed point overflow exception instructions
      case TR::InstOpCode::VCVB:
      case TR::InstOpCode::VCVBG:
          return !cg->getIgnoreDecimalOverflowException();
      default:
          return false;
      }
   }

void
generateS390DAAExceptionRestoreSnippet(TR::CodeGenerator* cg,
                                       TR::Node* n,
                                       TR::Instruction* instr,
                                       TR::InstOpCode::Mnemonic op,
                                       bool hasNOP)
   {
   TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

   if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
      {
      // Explicitly mark as a DAA intrinsic instruction
      instr->setThrowsImplicitException();

      //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
      //so we will assume that at this moment the second child of BCDCHKNode is the label.
      TR::LabelSymbol * handlerLabel = cg->getCurrentBCDCHKHandlerLabel();
      TR_ASSERT(handlerLabel, "BCDCHK node handler label should not be null");

      TR::LabelSymbol* restoreGPR7SnippetHandler = generateLabelSymbol(cg);
      TR::S390RestoreGPR7Snippet * restoreSnippet =
                     new (INSN_HEAP) TR::S390RestoreGPR7Snippet(cg, n, restoreGPR7SnippetHandler, handlerLabel);
      cg->addSnippet(restoreSnippet);

      if(hasNOP)
         {
         TR::Instruction * nop = new (INSN_HEAP) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, n, cg);
         }

      auto bc = TR::InstOpCode::COND_NOP;

      if (canThrowDecimalOverflowException(cg, op))
         {
         bc = TR::InstOpCode::COND_CC3;
         }

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreGPR7SnippetHandler);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, bc, n, handlerLabel);

      //generate register deps so that we will not see spills between the instruction and the following branch
      TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
      }
   }
#endif

template TR::Instruction * generateS390CompareAndBranchInstruction<int32_t>(
                    TR::CodeGenerator * cg,
                    TR::InstOpCode::Mnemonic compareOpCode,
                    TR::Node * node,
                    TR::Register * first,
                    int32_t second,
                    TR::InstOpCode::S390BranchCondition bc,
                    TR::LabelSymbol * branchDestination,
                    bool needsCC = true,
                    bool targetIsFarAndCold = false,
                    TR::Instruction        *preced = 0,
                    TR::RegisterDependencyConditions *cond = 0);

template TR::Instruction * generateS390CompareAndBranchInstruction<int64_t>(
                    TR::CodeGenerator * cg,
                    TR::InstOpCode::Mnemonic compareOpCode,
                    TR::Node * node,
                    TR::Register * first,
                    int64_t second,
                    TR::InstOpCode::S390BranchCondition bc,
                    TR::LabelSymbol * branchDestination,
                    bool needsCC = true,
                    bool targetIsFarAndCold = false,
                    TR::Instruction        *preced = 0,
                    TR::RegisterDependencyConditions *cond = 0); 

TR::Instruction*
generateAlignmentNopInstruction(TR::CodeGenerator *cg, TR::Node *node, uint32_t alignment, TR::Instruction *preced)
   {
   if (preced)
      return new (INSN_HEAP) TR::S390AlignmentNopInstruction(node, alignment, preced, cg);
   return new (INSN_HEAP) TR::S390AlignmentNopInstruction(node, alignment, cg);
   }
