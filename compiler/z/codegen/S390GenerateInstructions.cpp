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

#include "z/codegen/S390GenerateInstructions.hpp"

#include <stdint.h>                               // for uint8_t, int32_t, etc
#include <stdio.h>                                // for sprintf
#include <stdlib.h>                               // for atoi
#include <string.h>                               // for NULL, strlen, etc
#include "codegen/CodeGenerator.hpp"              // for CodeGenerator, etc
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                   // for feGetEnv, etc
#include "codegen/InstOpCode.hpp"                 // for InstOpCode, etc
#include "codegen/Instruction.hpp"                // for Instruction
#include "codegen/Linkage.hpp"                    // for Linkage
#include "codegen/Machine.hpp"                    // for Machine, etc
#include "codegen/MemoryReference.hpp"            // for MemoryReference, etc
#include "codegen/RealRegister.hpp"               // for RealRegister, etc
#include "codegen/Register.hpp"                   // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"   // for RegisterDependency
#include "codegen/RegisterPair.hpp"               // for RegisterPair
#include "codegen/Relocation.hpp"                 // for AOTcgDiag1, etc
#include "codegen/Snippet.hpp"                    // for Snippet, etc
#include "codegen/S390Snippets.hpp"
#include "codegen/TreeEvaluator.hpp"              // for generateS390ImmOp
#include "codegen/S390Evaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"                // for Compilation
#include "compile/ResolvedMethod.hpp"             // for TR_ResolvedMethod
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                         // for uintptrj_t
#include "il/Block.hpp"                           // for Block
#include "il/DataTypes.hpp"                       // for DataTypes::Double, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                           // for ILOpCode
#include "il/Node.hpp"                            // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                          // for Symbol, etc
#include "il/SymbolReference.hpp"                 // for SymbolReference
#include "il/symbol/LabelSymbol.hpp"              // for LabelSymbol
#include "il/symbol/MethodSymbol.hpp"             // for MethodSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                       // for TR_ASSERT
#include "infra/List.hpp"                         // for List
#include "ras/Debug.hpp"                          // for TR_DebugBase
#include "z/codegen/CallSnippet.hpp"              // for TR_S390CallSnippet
#include "z/codegen/S390Instruction.hpp"          // for etc


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

TR::Instruction *
generateS390BranchInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg,
                              TR::LabelSymbol * sym, TR::Instruction * preced)
   {
   if (cg->supportsHighWordFacility() && !cg->comp()->getOption(TR_DisableHighWordRA))
      {
      if (op == TR::InstOpCode::BRCT && targetReg->assignToHPR())
         {
         // upgrade to Highword branch on count
         op = TR::InstOpCode::BRCTH;
         }
      }
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
   if (cg->supportsHighWordFacility() && !cg->comp()->getOption(TR_DisableHighWordRA))
      {
      if (op == TR::InstOpCode::BRCT && targetReg->assignToHPR())
         {
         // upgrade to Highword branch on count
         op = TR::InstOpCode::BRCTH;
         }
      }
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
           cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12))
      {
      // generate a compare and branch.
      returnInstruction = (TR::S390RIEInstruction *)generateRIEInstruction(cg, replacementOpCode, node, first, second, branchDestination, bc);

      // Generate a temporary warm trampoline for warm -> cold code cache branches.
      if (cg->getIsInWarmCodeCache() && targetIsFarAndCold)
         {
         TR_S390WarmToColdTrampolineSnippet * trampolineSnippet = new (INSN_HEAP) TR_S390WarmToColdTrampolineSnippet(cg, node,  TR::LabelSymbol::create(INSN_HEAP,cg), branchDestination);
         ((TR::S390RIEInstruction*)returnInstruction)->setWarmToColdTrampolineSnippet(trampolineSnippet);
         cg->addSnippet(trampolineSnippet);
         }
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
          performTransformation(comp, "O^O generateS390CompareAndBranchInstruction case 1 remove RR Compare[%s\t %s, %s]: reuse CC from ccInst [%p].", debugObj->getOpCodeName(&opcTmp), debugObj->getName(first),debugObj->getName(second),ccInst) )
            returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, bc, node, branchDestination);
      else if ( !needsCC && comp->getOption(TR_EnableEBBCCInfo) &&
                cg->isActiveCompareCC(compareOpCode, second, first) &&
                performTransformation(comp, "O^O generateS390CompareAndBranchInstruction case 2 remove RR Compare[%s\t %s, %s]: reuse CC from ccInst [%p].", debugObj->getOpCodeName(&opcTmp), debugObj->getName(first),debugObj->getName(second),ccInst) )
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
TR::Instruction *
generateS390CompareAndBranchInstruction(TR::CodeGenerator * cg,
                                        TR::InstOpCode::Mnemonic compareOpCode,
                                        TR::Node * node,
                                        TR::Register * first ,
                                        int32_t second,
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
           cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12))
      {
      cursor = (TR::S390RIEInstruction *)generateRIEInstruction(cg, replacementOpCode, node, first, (int8_t) second, branchDestination, bc, preced);
      // Generate a temporary warm trampoline for warm -> cold code cache branches.
      if (cg->getIsInWarmCodeCache() && targetIsFarAndCold)
         {
         TR_S390WarmToColdTrampolineSnippet * trampolineSnippet = new (INSN_HEAP) TR_S390WarmToColdTrampolineSnippet(cg, node,  TR::LabelSymbol::create(INSN_HEAP,cg), branchDestination);
         ((TR::S390RIEInstruction*)cursor)->setWarmToColdTrampolineSnippet(trampolineSnippet);
         cg->addSnippet(trampolineSnippet);
         }
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
   if (cg->supportsHighWordFacility() && !cg->comp()->getOption(TR_DisableHighWordRA))
      {
      switch(op)
         {
         case TR::InstOpCode::AR:
            if (treg->assignToHPR())
               {
               if (sreg->assignToHPR())
                  {
                  return generateRRRInstruction(cg, TR::InstOpCode::AHHHR, n, treg, treg, sreg, preced);
                  }
               //cracked insn
               //return generateRRRInstruction(cg, TR::InstOpCode::AHHLR, n, treg, treg, sreg, preced);
               }
            break;
         case TR::InstOpCode::ALR:
            if (treg->assignToHPR())
               {
               if (sreg->assignToHPR())
                  {
                  return generateRRRInstruction(cg, TR::InstOpCode::ALHHHR, n, treg, treg, sreg, preced);
                  }
               //return generateRRRInstruction(cg, TR::InstOpCode::ALHHLR, n, treg, treg, sreg, preced);
               }
            break;
         case TR::InstOpCode::CR:
            if (treg->assignToHPR())
               {
               op = TR::InstOpCode::CHLR;
               if (sreg->assignToHPR())
                  {
                  op = TR::InstOpCode::CHHR;
                  }
               }
            break;
         case TR::InstOpCode::CLR:
            if (treg->assignToHPR())
               {
               op = TR::InstOpCode::CLHLR;
               if (sreg->assignToHPR())
                  {
                  op = TR::InstOpCode::CLHHR;
                  }
               }
            break;
         case TR::InstOpCode::LR:
            if (treg->assignToHPR())
               {
               if (sreg->assignToHPR())
                  {
                  return generateExtendedHighWordInstruction(n, cg, TR::InstOpCode::LHHR, treg, sreg, 0, preced);
                  }
               return generateExtendedHighWordInstruction(n, cg, TR::InstOpCode::LHLR, treg, sreg, 0, preced);
               }
            if (sreg->assignToHPR())
               {
               return generateExtendedHighWordInstruction(n, cg, TR::InstOpCode::LLHFR, treg, sreg, 0, preced);
               }
            break;
         case TR::InstOpCode::SR:
            if (treg->assignToHPR())
               {
               if (sreg->assignToHPR())
                  {
                  return generateRRRInstruction(cg, TR::InstOpCode::SHHHR, n, treg, treg, sreg, preced);
                  }
               //return generateRRRInstruction(cg, TR::InstOpCode::SHHLR, n, treg, treg, sreg, preced);
               }
            break;
         case TR::InstOpCode::SLR:
            if (treg->assignToHPR())
               {
               if (sreg->assignToHPR())
                  {
                  return generateRRRInstruction(cg, TR::InstOpCode::SLHHHR, n, treg, treg, sreg, preced);
                  }
               //return generateRRRInstruction(cg, TR::InstOpCode::SLHHLR, n, treg, treg, sreg, preced);
               }
            break;
         case TR::InstOpCode::LHHR:
            return generateExtendedHighWordInstruction(n, cg, TR::InstOpCode::LHHR, treg, sreg, 0, preced);
            break;
         case TR::InstOpCode::LLHFR:
            return generateExtendedHighWordInstruction(n, cg, TR::InstOpCode::LLHFR, treg, sreg, 0, preced);
            break;
         case TR::InstOpCode::LHLR:
            return generateExtendedHighWordInstruction(n, cg, TR::InstOpCode::LHLR, treg, sreg, 0, preced);
            break;
         }
      }

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
             ( conds1->getRegisterDependency( i )->getRegister(cg) != conds2->getRegisterDependency( j )->getRegister(cg) ))
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
   TR::Compilation *comp = cg->comp();

   TR_ASSERT(treg->getRealRegister()!=NULL || // Not in RA
           op != TR::InstOpCode::L || n->couldIgnoreExtend() || !n->force64BitLoad(), "Generating an TR::InstOpCode::L, when LLGF|LGF should be used");
   if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && treg->assignToHPR())
      {
      switch(op)
         {
         case TR::InstOpCode::C:
            return generateRXYInstruction(cg, TR::InstOpCode::CHF, n, treg, mf, preced);
            break;
         case TR::InstOpCode::CL:
            return generateRXYInstruction(cg, TR::InstOpCode::CLHF, n, treg, mf, preced);
            break;
         case TR::InstOpCode::ST:
            return generateRXYInstruction(cg, TR::InstOpCode::STFH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::STC:
            return generateRXYInstruction(cg, TR::InstOpCode::STCH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::STH:
            return generateRXYInstruction(cg, TR::InstOpCode::STHH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::L:
            return generateRXYInstruction(cg, TR::InstOpCode::LFH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::LB:
            return generateRXYInstruction(cg, TR::InstOpCode::LBH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::LH:
            return generateRXYInstruction(cg, TR::InstOpCode::LHH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::LLC:
            return generateRXYInstruction(cg, TR::InstOpCode::LLCH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::LLH:
            return generateRXYInstruction(cg, TR::InstOpCode::LLHH, n, treg, mf, preced);
            break;
         default:
            break;
         }
      }

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXInstruction(op, n, treg, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RXInstruction(op, n, treg, mf, cg);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::CVB || op == TR::InstOpCode::EX)
      {
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction * nop = new (INSN_HEAP) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, n, cg);
         TR::Instruction* brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         //generate register deps for CVB so that we will not see spills between CVB and the following branch
         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
         }
      }
#endif


   return instr;
   }

TR::Instruction *
generateRXInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t  constForMR,
                      TR::Instruction * preced)
   {
   TR::Instruction * instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXInstruction(op, n, treg, constForMR, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RXInstruction(op, n, treg, constForMR, cg);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::CVB || op == TR::InstOpCode::EX)
      {
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction * nop = new (INSN_HEAP) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, n, cg);
         TR::Instruction* brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         //generate register deps for CVB so that we will not see spills between CVB and the following branch
         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
         }
      }
#endif

   return instr;
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
generateRXYInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * regp, TR::MemoryReference * mf,
                       TR::Instruction * preced)
   {
   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXYInstruction(op, n, regp, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RXYInstruction(op, n, regp, mf, cg);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::CVBY || op == TR::InstOpCode::CVBG)
      {
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction* brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         //generate register deps for CVBY/CVBG so that we will not see spills between CVB and the following branch
         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);

         }
      }
#endif

   return instr;
   }

TR::Instruction *
generateRXYInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::MemoryReference * mf,
                       TR::Instruction * preced)
   {
   TR::Compilation *comp = cg->comp();

   if (cg->supportsHighWordFacility() && comp->getOption(TR_DisableHighWordRA) && treg->assignToHPR())
      {
      TR::Instruction * instr = 0;

      switch(op)
         {
         case TR::InstOpCode::STY:
            instr = generateRXYInstruction(cg, TR::InstOpCode::STFH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::STCY:
            instr = generateRXYInstruction(cg, TR::InstOpCode::STCH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::STHY:
            instr = generateRXYInstruction(cg, TR::InstOpCode::STHH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::LY:
            instr = generateRXYInstruction(cg, TR::InstOpCode::LFH, n, treg, mf, preced);
            break;
         case TR::InstOpCode::LHY:
            instr = generateRXYInstruction(cg, TR::InstOpCode::LHH, n, treg, mf, preced);
            break;
         default:
            break;
         }

      if( instr )
         {
         return instr;
         }
      }

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXYInstruction(op, n, treg, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RXYInstruction(op, n, treg, mf, cg);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::CVBY || op == TR::InstOpCode::CVBG)
      {
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction* brcInstr =generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
         }
      }
#endif

   return instr;
   }

TR::Instruction *
generateRXYbInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint8_t mask, TR::MemoryReference * mf,  TR::Instruction * preced)
   {
   TR::Compilation *comp = cg->comp();

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXYbInstruction(op, n, mask, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RXYbInstruction(op, n, mask, mf, cg);

   return instr;
   }

TR::Instruction *
generateRXYInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t  constForMR, TR::Instruction * preced)
   {
   TR::Instruction * instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RXYInstruction(op, n, treg, constForMR, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RXYInstruction(op, n, treg, constForMR, cg);

#ifdef J9_PROJECT_SPECIFIC
   if (op == TR::InstOpCode::CVBY || op == TR::InstOpCode::CVBG)
      {
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction* brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
         }
      }
#endif

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
   TR::Compilation *comp = cg->comp();
   if (cg->supportsHighWordFacility() && comp->getOption(TR_DisableHighWordRA) && treg->assignToHPR())
      {
      switch(op)
         {
         case TR::InstOpCode::LHI:
            return generateRILInstruction(cg, TR::InstOpCode::IIHF, n, treg, imm, preced);
            break;
         case TR::InstOpCode::AHI:
            return generateRILInstruction(cg, TR::InstOpCode::AIH, n, treg, imm, preced);
            break;
         default:
            break;
         }
      }
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
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::SymbolReference * sr, uintptrj_t imm, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, sr, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, sr, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, TR::LabelSymbol * label, TR::Instruction * preced)
   {
   if (preced)
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, label, preced, cg);
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, label, cg);
   }

TR::Instruction *
generateRILInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uintptrj_t imm, TR::Instruction * preced)
   {
   if (cg->supportsHighWordFacility() && cg->comp()->getOption(TR_DisableHighWordRA) && treg->assignToHPR())
      {
      switch(op)
         {
         case TR::InstOpCode::AFI:
            return generateRILInstruction(cg, TR::InstOpCode::AIH, n, treg, imm, preced);
            break;
         default:
            break;
         }
      }
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RILInstruction(op, n, treg, imm, cg);
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
   // non-Trex cannot upgrade to Y form so take this opportunity to fold in a large offset
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mf, cg);

   return instr;
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t mask,
                      TR::MemoryReference * mf, TR::Instruction * preced)
   {
   // non-Trex cannot upgrade to Y form so take this opportunity to fold in a large offset
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mask, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, mask, mf, cg);

   return instr;
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * treg, TR::RegisterPair * sreg,
                      TR::MemoryReference * mf, TR::Instruction * preced)
   {
   // non-Trex cannot upgrade to Y form so take this opportunity to fold in a large offset
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, sreg, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, treg, sreg, mf, cg);

   return instr;
   }


TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * freg, TR::Register * lreg,
                      TR::MemoryReference * mf, TR::Instruction * preced)
   {
   // non-Trex cannot upgrade to Y form so take this opportunity to fold in a large offset
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, freg, lreg, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, freg, lreg, mf, cg);

   return instr;
   }

TR::Instruction *
generateRSInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::RegisterPair * regp, TR::MemoryReference * mf,
                      TR::Instruction * preced)
   {
   // non-Trex cannot upgrade to Y form so take this opportunity to fold in a large offset
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, regp, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSInstruction(op, n, regp, mf, cg);

   return instr;
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
generateRSYInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * treg, uint32_t mask,
                       TR::MemoryReference * mf, TR::Instruction * preced)
   {
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced); // enforce4KDisplacementLimit=false

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, mask, mf, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RSYInstruction(op, n, treg, mask, mf, cg);

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
   if (op == TR::InstOpCode::ZAP || op == TR::InstOpCode::CP || op == TR::InstOpCode::AP || op == TR::InstOpCode::SP || op == TR::InstOpCode::MP || op == TR::InstOpCode::DP)
      {
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction* brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         //generate register deps for CVB so that we will not see spills between CVB and the following branch
         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
         }
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
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction* brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         //generate register deps for SRP so that we will not see spills between CVB and the following branch
         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
         }
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
      TR::Node* BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated();

      if (BCDCHKNode && BCDCHKNode->getOpCodeValue() == TR::BCDCHK)
         {
         // Explicitly mark as a DAA intrinsic instruction
         instr->setThrowsImplicitException();

         //there might be cases that two or more insts generated will throw exception, and we only need one of the label.
         //so we will assume that at this moment the second child of BCDCHKNode is the label.
         TR::LabelSymbol * label = (TR::LabelSymbol *)BCDCHKNode->getChild(1);

         TR_S390RestoreGPR7Snippet * restoreSnippet =
                        new (INSN_HEAP) TR_S390RestoreGPR7Snippet(cg, n,  TR::LabelSymbol::create(INSN_HEAP,cg), label);
         cg->addSnippet(restoreSnippet);
         TR::Instruction* brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, n, restoreSnippet->getSnippetLabel());

         //generate register deps for SRP so that we will not see spills between CVB and the following branch
         TR::TreeEvaluator::createDAACondDeps(n, cg->getCurrentCheckNodeRegDeps(), instr, cg);
         }
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
   // non-Trex cannot upgrade to Y form so take this opportunity to fold in a large offset
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced);

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SIInstruction(op, n, mf, imm, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SIInstruction(op, n, mf, imm, cg);

   return instr;
   }

TR::Instruction *
generateSIYInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference * mf, uint32_t imm,
                       TR::Instruction * preced)
   {
   if (mf) preced = mf->separateIndexRegister(n, cg, false, preced); // enforce4KDisplacementLimit=false

   TR::Instruction *instr;
   if (preced)
      instr = new (INSN_HEAP) TR::S390SIYInstruction(op, n, mf, imm, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390SIYInstruction(op, n, mf, imm, cg);

   return instr;
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
 *      VRI: a,b,c,d,e
 *      VRR: a,b,c,d,e,f
 *      VRS: a,b,c
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

/****** VRS ******/
/* Note subtle differences between register types and optionality of masks between these 3*/
TR::Instruction *
generateVRSaInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg, TR::MemoryReference * mr,
                        uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRSaInstruction(cg, op, n, targetReg, sourceReg, mr, mask4);
   }

TR::Instruction *
generateVRSbInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg, TR::MemoryReference * mr,
                        uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRSbInstruction(cg, op, n, targetReg, sourceReg, mr, mask4);
   }

TR::Instruction *
generateVRScInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * targetReg, TR::Register * sourceReg, TR::MemoryReference * mr,
                        uint8_t mask4 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRScInstruction(cg, op, n, targetReg, sourceReg, mr, mask4);
   }

/****** VRV ******/
TR::Instruction *
generateVRVInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register *sourceReg, TR::MemoryReference * mr,
                       uint8_t mask3 /* 4 bits */)
   {
   return new (INSN_HEAP) TR::S390VRVInstruction(cg, op, n, sourceReg, mr, mask3);
   }

/****** VRX ******/
TR::Instruction *
generateVRXInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * reg,
                       TR::MemoryReference * memRef, uint8_t mask3, TR::Instruction * preced)
   {
   if (memRef) preced = memRef->enforceVRXFormatLimits(n, cg, preced);

   if (preced)
      return new (INSN_HEAP) TR::S390VRXInstruction(cg, op, n, reg, memRef, mask3, preced);
   else
      return new (INSN_HEAP) TR::S390VRXInstruction(cg, op, n, reg, memRef, mask3);
   }

/************************************************************ Misc Instructions ************************************************************/
TR::Instruction *
generateS390PseudoInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Node * fenceNode, TR::Instruction * preced)
   {
   TR::S390PseudoInstruction *instr;
   TR::Compilation *comp = cg->comp();

   if (preced)
   {
     instr= new (INSN_HEAP) TR::S390PseudoInstruction(op, n, fenceNode, preced, cg);
   }
   else
   {
     instr= new (INSN_HEAP) TR::S390PseudoInstruction(op, n, fenceNode, cg);
   }


   return instr;
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
 * Generate a debug counter bump pseudo instruction. Should be passed a pointer to preceding instruction if this is called after register assignment. 
 */
TR::Instruction *
generateS390DebugCounterBumpInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Snippet* cas, int32_t delta, TR::Instruction * preced)
   {
   if (preced)
      {
         return new (INSN_HEAP) TR::S390DebugCounterBumpInstruction(op, n, cas, cg, delta, preced);
      }
   return new (INSN_HEAP) TR::S390DebugCounterBumpInstruction(op, n, cas, cg, delta);
   }

TR::Instruction *
generateRegRegInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register * sreg, TR::Register * treg,
                          TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   if (preced)
      {
      return new (INSN_HEAP) TR::S390RRInstruction(op, n, sreg, treg, cond, preced, cg);
      }
   return new (INSN_HEAP) TR::S390RRInstruction(op, n, sreg, treg, cond, cg);
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
generateLogicalImmediate(TR::CodeGenerator * cg, TR::Node * node, TR::InstOpCode::Mnemonic defaultOp, TR::InstOpCode::Mnemonic lhOp, TR::InstOpCode::Mnemonic llOp,
                         TR::Register * reg, int32_t imm)
   {
   if ((imm & 0x0000FFFF) == 0x0000FFFF)
      {
      return new (INSN_HEAP) TR::S390RIInstruction(lhOp, node, reg, ((imm & 0xFFFF0000) >> 16), cg);
      }

   if ((imm & 0xFFFF0000) == 0xFFFF0000)
      {
      return new (INSN_HEAP) TR::S390RIInstruction(llOp, node, reg, imm, cg);
      }

   TR::MemoryReference * dataref = generateS390MemoryReference(imm, TR::Int32, cg, 0);

   TR::Instruction *instr;
   instr = (new (INSN_HEAP) TR::S390RXInstruction(defaultOp, node, reg, dataref, cg));

   return instr;
   }

TR::Instruction *
generateAndImmediate(TR::CodeGenerator * cg, TR::Node * node, TR::Register * reg, int32_t imm)
   {
   return generateLogicalImmediate(cg, node, TR::InstOpCode::N, TR::InstOpCode::NILH, TR::InstOpCode::NILL, reg, imm);
   }

TR::Instruction *
generateOrImmediate(TR::CodeGenerator * cg, TR::Node * node, TR::Register * reg, int32_t imm)
   {
   return generateLogicalImmediate(cg, node, TR::InstOpCode::O, TR::InstOpCode::OILH, TR::InstOpCode::OILL, reg, imm);
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

   // Direct Call on zlinux64 can require a trampoline.  As the trampoline will kill EPReg
   // we should always see EPReg defined in the post-deps of such calls.
#if defined(TR_TARGET_64BIT) && !defined(J9ZOS390)
   TR_ASSERT(RegEP != NULL,
      "generateDirectCall: zLinx64 requires EPReg be defined on directCalls to correclty handle trampolines.\n");
#endif

   // If the EP is not specified, we assume re-use of the RA.
   // e.g.  BASR R14,R14
   if (RegEP == NULL)
      {
      RegEP = RegRA;
      }
   TR_ASSERT( RegEP != NULL,"generateDirectCall: Undefined entry point register\n");

   TR::Symbol * sym = callSymRef->getSymbol();
   uintptrj_t imm = 0;

   // if it is not calling myself, get the method address
   if (!myself)
      {
      imm = (uintptrj_t) callSymRef->getMethodAddress();
      }

   AOTcgDiag2(comp, "\nimm=%x isHelper=%x\n", imm, isHelper);

   TR_S390TargetAddressSnippet * targetsnippet;

   // Since N3 generate TR::InstOpCode::BRASL -- only need 1 instruction, and no worry
   // about the displacement
   // Calling myself
   // address is unknown at this point so we need to pass the symbol to the
   // instruction and calculate the address in generateBinary phase
   if (myself)
      {
      TR::Instruction * instr = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, sym, callSymRef, cg);
      instr->setJITExit();

      return instr;
      }
   else // address known
      {
#if !defined(J9ZOS390) && defined(TR_TARGET_64BIT)
      // With MultiCodeCache, we ALWAYS can jump to at least a trampoline.

      if (!isHelper && cg->supportsBranchPreloadForCalls())
         {
         static int minFR = (feGetEnv("TR_minFR")!=NULL) ? atoi(feGetEnv("TR_minFR")) : 0;
         static int maxFR = (feGetEnv("TR_maxFR")!=NULL) ? atoi(feGetEnv("TR_maxFR")) : 0;

         int32_t frequency = comp->getCurrentBlock()->getFrequency();
         if (frequency > 6 && frequency >= minFR && (maxFR == 0 || frequency > maxFR))
            {
            TR::LabelSymbol * callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            TR::Instruction * instr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, callNode, callLabel);
            cg->createBranchPreloadCallData(callLabel, callSymRef, instr);
            }

         }

      TR::S390RILInstruction *tempInst =
            (new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, imm, callSymRef, cg));

      AOTcgDiag1(comp, "\ntempInst=%p\n", tempInst);
      return tempInst;
#else
      if (cg->canUseRelativeLongInstructions(imm) || isHelper)
         {
         if (!isHelper && cg->supportsBranchPreloadForCalls())
            {
            static int minFR = (feGetEnv("TR_minFR")!=NULL) ? atoi(feGetEnv("TR_minFR")) : 0;
            static int maxFR = (feGetEnv("TR_maxFR")!=NULL) ? atoi(feGetEnv("TR_maxFR")) : 0;

            int32_t frequency = comp->getCurrentBlock()->getFrequency();
            if (frequency > 6 && frequency >= minFR && (maxFR == 0 || frequency > maxFR))
               {

               TR::LabelSymbol * callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
               TR::Instruction * instr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, callNode, callLabel);
               cg->createBranchPreloadCallData(callLabel, callSymRef, instr);

               }

            }

         TR::S390RILInstruction *tempInst =
            new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, imm, cg);
         tempInst->setJITExit();

         if (isHelper)
            tempInst->setSymbolReference(callSymRef);
         AOTcgDiag1(comp, "\ntempInst=%x\n", tempInst);
         return tempInst;
         }
      else
         {
         genLoadAddressConstant(cg, callNode, imm, RegEP, preced, cond);
         TR::Instruction * instr = new (INSN_HEAP) TR::S390RRInstruction(TR::InstOpCode::BASR, callNode, RegRA, RegEP, cg);
         instr->setJITExit();

         return instr;
         }
#endif
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
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, callNode, TR::LabelSymbol::create(INSN_HEAP, cg), preDeps);

      callInstr = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, killRegRA, s,
         postDeps, callSymRef, cg);
      callInstr->setJITExit();

      cg->stopUsingRegister(killRegRA);
      cg->stopUsingRegister(killRegEP);
      }
   else
      {
      callInstr = new (INSN_HEAP) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, RegRA, s, cond, callSymRef, cg);
      callInstr->setJITExit();
      }

   TR_ASSERT( s->isCallSnippet(), "targetSnippet is NOT CallSnippet ");
   ((TR_S390CallSnippet *) s)->setBranchInstruction(callInstr);
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
   TR::S390RILInstruction *LARLinst = (TR::S390RILInstruction *) generateRILInstruction(cg, TR::InstOpCode::LARL, node, treg, 0xBABE, 0);
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
   TR_S390ConstantDataSnippet * targetsnippet = 0;
   TR::MemoryReference * dataref = 0;
   TR::S390RILInstruction *LRLinst = 0;
   if (cg->isLiteralPoolOnDemandOn() && (base == 0))
      {
      if (op == TR::InstOpCode::L)
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
      cursor = new (INSN_HEAP) TR::S390RXInstruction(op, node, treg, dataref, cg);
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
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, uintptrj_t imm, int32_t reloType,
                             TR::RegisterDependencyConditions * cond, TR::Instruction * preced, TR::Register * base)
   {
   bool alloc = false;
   TR::Instruction * cursor;
   TR::Compilation *comp = cg->comp();

   if (op == TR::InstOpCode::LG || op == TR::InstOpCode::L)
      {
      TR_S390ConstantDataSnippet *targetSnippet;
      if(op == TR::InstOpCode::LG)
         targetSnippet = cg->findOrCreate8ByteConstant(node, (int64_t)imm);
      else
         targetSnippet = cg->findOrCreate4ByteConstant(node, (int32_t)imm);
      targetSnippet->setSymbolReference(new (INSN_HEAP) TR::SymbolReference(comp->getSymRefTab()));
      targetSnippet->setReloType(reloType);
      AOTcgDiag4(comp, "generateRegLitRefInstruction constantDataSnippet=%x symbolReference=%x symbol=%x reloType=%x\n",
         targetSnippet, targetSnippet->getSymbolReference(), targetSnippet->getSymbolReference()->getSymbol(), reloType);

      cursor = (TR::S390RILInstruction *) generateRILInstruction(cg, (op == TR::InstOpCode::LG)?TR::InstOpCode::LGRL:TR::InstOpCode::LRL, node, treg, targetSnippet, preced);
      return cursor;
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
   if (TR::Compiler->target.is64Bit())
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
   cursor = new (INSN_HEAP) TR::S390RXInstruction(op, node, treg, dataref, cg);
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
   cursor = new (INSN_HEAP) TR::S390RXInstruction(op, node, treg, dataref, cg);
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
   TR_S390ConstantDataSnippet * targetsnippet = 0;
   TR::MemoryReference * dataref = 0;
   TR::S390RILInstruction *LGRLinst = 0;
   TR::Compilation *comp = cg->comp();

   if (TR::InstOpCode(op).getInstructionFormat() == RIL_FORMAT)
      {
      TR_S390ConstantDataSnippet * constDataSnip = cg->create64BitLiteralPoolSnippet(TR::Int64, imm);

      // HCR in generateRegLitRefInstruction 64-bit: register const data snippet used by Z6
      if (comp->getOption(TR_EnableHCR) && isPICCandidate )
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
      if (op == TR::InstOpCode::LG)
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
      cursor = new (INSN_HEAP) TR::S390RXInstruction(op, node, treg, dataref, cg);
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
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, uintptrj_t imm,
                             TR::RegisterDependencyConditions * cond, TR::Instruction * preced, TR::Register * base, bool isPICCandidate)
   {
   if (TR::Compiler->target.is64Bit())
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
   return new (INSN_HEAP) TR::S390RXInstruction(op, node, treg, dataref, cg);
   }

/**
 * For double constant
 */
TR::Instruction *
generateRegLitRefInstruction(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::Register * treg, double imm,
                             TR::Instruction * preced)
   {

   TR::MemoryReference * dataref = generateS390MemoryReference(imm, TR::Double, cg, node);
   return new (INSN_HEAP) TR::S390RXInstruction(op, node, treg, dataref, cg);
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
      TR_S390ConstantInstructionSnippet * cis = cg->createConstantInstruction(cg, node, instr);
      TR::MemoryReference * tempMR = generateS390MemoryReference(cis, cg, litPool, node);

      //the memory reference should create a constant data snippet
      cursor = generateRXInstruction(cg, TR::InstOpCode::EX, node, maskReg, tempMR, (preced != NULL ? preced : comp->getAppendInstruction()));
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
   // BCR R15, 0 is the defacto serialization instruction on Z
   // On z196, a fast serialization facilty was added, and BCR R14, 0 is preferred.

   TR::InstOpCode::S390BranchCondition cond = TR::InstOpCode::COND_MASK14;

   // We needed some special handling in TR::Instruction::assignRegisterNoDependencies
   // to recognize real register GPR0 being passed in.
   TR::RealRegister *gpr0 = cg->machine()->getS390RealRegister(TR::RealRegister::GPR0);

   TR::Instruction * instr = NULL;
   if (preced)
      instr = new (INSN_HEAP) TR::S390RegInstruction(TR::InstOpCode::BCR, node, cond, gpr0, preced, cg);
   else
      instr = new (INSN_HEAP) TR::S390RegInstruction(TR::InstOpCode::BCR, node, cond, gpr0, cg);

   return instr;
   }

// detecting case of SS instructions
// with AR base register
// with the same GPR, but different AR registers
// For example:
// MVC      0(5,GPR_1968:AR_1920(GPR_1968:AR_1920)), 0(GPR_1968:AR_1888(GPR_1968:AR_1888))
//
// also need to take care of the reverse, i.e different GPR's but the same AR's, i.e:
// MVC      0(1,GPR64_8472:AR_8455(GPR64_8472:AR_8455)), 0(GPR64_8450:AR_8455(GPR64_8450:AR_8455))


TR::Instruction *
splitBaseRegisterIfNeeded(TR::MemoryReference *mf1, TR::MemoryReference *mf2, TR::CodeGenerator *cg, TR::Node *node, TR::Instruction *preced)
   {
   TR::Instruction * instr = preced;

   if (mf1) return instr;
   if (mf2) return instr;

   if (!mf1->getBaseRegister()->isArGprPair())  return instr;
   if (!mf2->getBaseRegister()->isArGprPair())  return instr;

   if (mf1->getBaseRegister()->getGPRofArGprPair() == mf2->getBaseRegister()->getGPRofArGprPair())
      {
      // same GPR's and same AR's - all good
      if (mf1->getBaseRegister()->getARofArGprPair() == mf2->getBaseRegister()->getARofArGprPair()) return instr;

      TR::Register * baseGPRmf2 = mf2->getBaseRegister()->getGPRofArGprPair();
      TR::Register * baseARmf2 = mf2->getBaseRegister()->getARofArGprPair();

      TR::Register * tempReg = cg->allocateRegister(baseGPRmf2->getKind());

      TR::InstOpCode::Mnemonic copyOp = TR::InstOpCode::LR;

      if (baseGPRmf2->getKind() == TR_GPR64) copyOp = TR::InstOpCode::LGR;

      instr = generateRRInstruction(cg, copyOp, node, tempReg, baseGPRmf2, preced);

      TR::RegisterPair *regpair = cg->allocateArGprPair(baseARmf2, tempReg);
      mf2->setBaseRegister(regpair, cg);
      cg->stopUsingRegister(regpair);
      cg->stopUsingRegister(tempReg);
    }
   else
      {
      // different GPR's and different AR's - all good
      if (mf1->getBaseRegister()->getARofArGprPair() != mf2->getBaseRegister()->getARofArGprPair()) return instr;

      TR::Register * baseGPRmf2 = mf2->getBaseRegister()->getGPRofArGprPair();
      TR::Register * baseARmf2 = mf2->getBaseRegister()->getARofArGprPair();

      TR::Register * tempReg = cg->allocateRegister(baseARmf2->getKind());

      instr = generateRRInstruction(cg, TR::InstOpCode::CPYA, node, tempReg, baseARmf2, preced);

      TR::RegisterPair *regpair = cg->allocateArGprPair(tempReg, baseGPRmf2);
      mf2->setBaseRegister(regpair, cg);
      cg->stopUsingRegister(regpair);
      cg->stopUsingRegister(tempReg);
      }

   return instr;

   }

/**
 * Generate Highword instructions using extended-mnemonic syntax
 * for example:
 * Load (High <- Low)       LHLR R1 R2   =      RISBHGZ (R1, R2, 0, 31, 32) z7 only
 */
TR::Instruction *
generateExtendedHighWordInstruction(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op,
                                    TR::Register * targetReg, TR::Register * srcReg, int8_t imm8, TR::Instruction * preced)
   {
   TR::Instruction * cursor = NULL;
   TR_Debug * debugObj = cg->getDebug();
   char * COMMENT;
   bool isTargetHWUsed, isSrcHWUsed, isTargetLWUsed, isSrcLWUsed; // make sure to not clobber the flags
   TR::Compilation *comp = cg->comp();

   if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      switch(op)
         {
         case TR::InstOpCode::LHHR:         // Load (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, 0, 31+0x80, 0, preced);
            COMMENT = "LHHR : Load (High <- High)";
            break;
         case TR::InstOpCode::LHLR:         // Load (High <- Low)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, 0, 31+0x80, 32, preced);
            COMMENT = "LHLR : Load (High <- Low)";
            break;
         case TR::InstOpCode::LLHFR:        // Load (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetReg, srcReg, 0, 31+0x80, 32, preced);
            COMMENT = "LLHFR : Load (Low <- High)";
            break;
         case TR::InstOpCode::LLHHHR:       // Load Logical Halfword (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, 16, 31+0x80, 0, preced);
            COMMENT = "LLHHHR : Load Logical Halfword (High <- High)";
            break;
         case TR::InstOpCode::LLHHLR:       // Load Logical Halfword (High <- Low)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, 16, 31+0x80, 32, preced);
            COMMENT = "LLHHLR : Load Logical Halfword (High <- Low)";
            break;
         case TR::InstOpCode::LLHLHR:       // Load Logical Halfword (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetReg, srcReg, 16, 31+0x80, 32, preced);
            COMMENT = "LLHLHR : Load Logical Halfword (Low <- High)";
            break;
         case TR::InstOpCode::LLCHHR:       // Load Logical Character (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, 24, 31+0x80, 0, preced);
            COMMENT = "LLCHHR : Load Logical Character (High <- High)";
            break;
         case TR::InstOpCode::LLCHLR:       // Load Logical Character (High <- Low)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, 24, 31+0x80, 32, preced);
            COMMENT = "LLCHLR : Load Logical Character (High <- Low)";
            break;
         case TR::InstOpCode::LLCLHR:       // Load Logical Character (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetReg, srcReg, 24, 31+0x80, 32, preced);
            COMMENT =  "LLCLHR : Load Logical Character (Low <- High)";
            break;
         case TR::InstOpCode::SLLHH:        // Shift Left Logical (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, 0, 31-imm8+0x80, imm8, preced);
            COMMENT = "SLLHH : Shift Left Logical (High <- High)";
            break;
         case TR::InstOpCode::SLLLH:        // Shift Left Logical (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetReg, srcReg, 0, 31-imm8+0x80, 32+imm8, preced);
            COMMENT = "SLLLH : Shift Left Logical (Low <- High)";
            break;
         case TR::InstOpCode::SRLHH:        // Shift Right Logical (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBHG, node, targetReg, srcReg, imm8, 31+0x80, -imm8, preced);
            COMMENT = "SRLHH : Shift Right Logical (High <- High)";
            break;
         case TR::InstOpCode::SRLLH:        // Shift Right Logical (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetReg, srcReg, imm8, 31+0x80, 32-imm8, preced);
            COMMENT = "SRLHH : Shift Right Logical (Low <- High)";
            break;
         case TR::InstOpCode::NHHR:         // AND High (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RNSBG, node, targetReg, srcReg, 0, 31, 0, preced);
            COMMENT = "NHHR : AND High (High <- High)";
            break;
         case TR::InstOpCode::NHLR:         // AND High (High <- Low)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RNSBG, node, targetReg, srcReg, 0, 31, 32, preced);
            COMMENT = "NHLR : AND High (High <- Low)";
            break;
         case TR::InstOpCode::NLHR:         // AND High (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RNSBG, node, targetReg, srcReg, 32, 63, 32, preced);
            COMMENT = "NLHR : AND High (Low <- High)";
            break;
         case TR::InstOpCode::XHHR:         // XOR High (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RXSBG, node, targetReg, srcReg, 0, 31, 0, preced);
            COMMENT = "XHHR : XOR High (High <- High)";
            break;
         case TR::InstOpCode::XHLR:         // XOR High (High <- Low)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RXSBG, node, targetReg, srcReg, 0, 31, 32, preced);
            COMMENT = "XHLR : XOR High (High <- Low)";
            break;
         case TR::InstOpCode::XLHR:         // XOR High (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::RXSBG, node, targetReg, srcReg, 32, 63, 32, preced);
            COMMENT = "XLHR : XOR High (Low <- High)";
            break;
         case TR::InstOpCode::OHHR:         // OR High (High <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::ROSBG, node, targetReg, srcReg, 0, 31, 0, preced);
            COMMENT = "OHHR : OR High (High <- High)";
            break;
         case TR::InstOpCode::OHLR:         // OR High (High <- Low)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::ROSBG, node, targetReg, srcReg, 0, 31, 32, preced);
            COMMENT = "OHLR : OR High (High <- Low)";
            break;
         case TR::InstOpCode::OLHR:         // OR High (Low <- High)
            cursor = generateRIEInstruction(cg, TR::InstOpCode::ROSBG, node, targetReg, srcReg, 32, 63, 32, preced);
            COMMENT = "OLHR : OR High (Low <- High)";
           break;
         default:
            TR_ASSERT(0, "OpCode not supported when calling generateExtendedHighWordInstruction()");
            break;
         }

      ((TR::S390RIEInstruction *)cursor)->setExtendedHighWordOpCode(op);

      if (debugObj)
         {
         debugObj->addInstructionComment(cursor, COMMENT);
         }

      }
   return cursor;
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
   TR::Compilation * comp = cg->comp();

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12) && (!aSetConditionCode)  && performTransformation(comp, "O^O Using RISBG instead of SLL/SRL pairs."))
      generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, aFirstRegister, aSecondRegister, aFromBit, aToBit|(aClearOtherBits ? 0x80 : 0x00), aShiftAmount);
   else
      generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, aFirstRegister, aSecondRegister, aFromBit, aToBit|(aClearOtherBits ? 0x80 : 0x00), aShiftAmount);
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
   TR::Compilation * comp = cg->comp();

   generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, aFirstRegister, aSecondRegister, aFromBit, aToBit|(aClearOtherBits ? 0x80 : 0x00), aShiftAmount);
   }

TR::Instruction *generateZeroVector(TR::Node *node, TR::CodeGenerator *cg, TR::Register *vecZeroReg)
   {
   return generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vecZeroReg, 0, 3);
   }
