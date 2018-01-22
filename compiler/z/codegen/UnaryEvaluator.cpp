/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <stddef.h>                                // for NULL, size_t
#include <stdint.h>                                // for int32_t, uint32_t, etc
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                    // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/Instruction.hpp"                 // for Instruction
#include "codegen/MemoryReference.hpp"             // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                // for RegisterPair
#include "codegen/TreeEvaluator.hpp"               // for TreeEvaluator, etc
#include "compile/Compilation.hpp"                 // for Compilation
#include "compile/ResolvedMethod.hpp"              // for TR_ResolvedMethod
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"                        // for TR_HeapMemory, etc
#include "il/DataTypes.hpp"                        // for TR::DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"           // for AutomaticSymbol
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/Bit.hpp"                           // for trailingZeroes, etc
#include "ras/Debug.hpp"                           // for TR_DebugBase
#include "ras/Delimiter.hpp"                       // for Delimiter
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "z/codegen/S390Register.hpp"              // for TR_PseudoRegister, etc
#endif

namespace TR { class SymbolReference; }

//#define TRACE_EVAL
#if defined(TRACE_EVAL)
#define EVAL_BLOCK
#if defined (EVAL_BLOCK)
#define PRINT_ME(string,node,cg) TR::Delimiter evalDelimiter(cg->comp(),cg->comp()->getOption(TR_TraceCG),"EVAL", string)
#else
extern void PRINT_ME(char * string, TR::Node * node, TR::CodeGenerator * cg);
#endif
#else
#define PRINT_ME(string,node,cg)
#endif

/**
 * 32bit version lconstHelper: load long integer constant (64-bit signed 2's complement)
 */
TR::Register *
lconstHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT(TR::Compiler->target.is32Bit(), "lconstHelper() is for 32bit only!");
   TR::Register * lowRegister = cg->allocateRegister();
   TR::Register * highRegister = cg->allocateRegister();

   TR::RegisterPair * longRegister = cg->allocateConsecutiveRegisterPair(lowRegister, highRegister);

   node->setRegister(longRegister);

   int32_t highValue = node->getLongIntHigh();
   int32_t lowValue = node->getLongIntLow();

   generateLoad32BitConstant(cg, node, lowValue, lowRegister, true);
   generateLoad32BitConstant(cg, node, highValue, highRegister, true);

   return longRegister;
   }

/**
 * 64bit version lconstHelper: load long integer constant (64-bit signed 2's complement)
 */
TR::Register *
lconstHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT(TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(),
      "lconstHelper64() is for 64bit only!");
   TR::Register * longRegister = cg->allocate64bitRegister();
   node->setRegister(longRegister);
   int64_t longValue = node->getLongInt();
   genLoadLongConstant(cg, node, longValue, longRegister);

   // on 64bit, lconst returns 64bit registers
   if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && TR::Compiler->target.is64Bit())
      {
      longRegister->setIs64BitReg(true);
      }

   return longRegister;
   }

/**
 * iconst Evaluator: load integer constant (32-bit signed 2's complement)
 * @see generateLoad32BitConstant
 */
TR::Register *
OMR::Z::TreeEvaluator::iconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("iconst", node, cg);
   TR::Register * tempReg = node->setRegister(cg->allocateRegister());
   generateLoad32BitConstant(cg, node, node->getInt(), tempReg, true);
   return tempReg;
   }

/**
 * aconst Evaluator: load address constant (zero value means NULL)
 */
TR::Register *
OMR::Z::TreeEvaluator::aconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("aconst", node, cg);
   TR::Register* targetRegister = cg->allocateRegister();

   node->setRegister(targetRegister);

   genLoadAddressConstant(cg, node, node->getAddress(), targetRegister);

   return targetRegister;
   }

/**
 * lconst Evaluator: load long integer constant (64-bit signed 2's complement)
 */
TR::Register *
OMR::Z::TreeEvaluator::lconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("lconst", node, cg);
   TR::Register * longRegister;
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return lconstHelper64(node, cg);
      }
   else
      {
      return lconstHelper(node, cg);
      }
   }

/**
 * bconst Evaluator: load byte integer constant (8-bit signed 2's complement)
 */
TR::Register *
OMR::Z::TreeEvaluator::bconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("bconst", node, cg);
   TR::Register * tempReg = node->setRegister(cg->allocateRegister());
   if (node->getOpCodeValue() == TR::buconst)
      generateLoad32BitConstant(cg, node, node->getByte() & 0xFF, tempReg, true);
   else
      {
      TR_ASSERT(1, "Should not have bconst node in the final il-tree!\n");
      generateLoad32BitConstant(cg, node, node->getByte(), tempReg, true);
      }
   return tempReg;
   }

/**
 * sconst Evaluator: load short integer constant (16-bit signed 2's complement)
 */
TR::Register *
OMR::Z::TreeEvaluator::sconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("sconst", node, cg);
   TR::Register * tempReg = node->setRegister(cg->allocateRegister());
   int32_t value = node->isUnsigned() ? node->getConst<uint16_t>() : node->getShortInt();
   generateLoad32BitConstant(cg, node, value, tempReg, true);
   return tempReg;
   }

/**
 * cconst Evaluator: load unicode integer constant (16-bit unsigned)
 */
TR::Register *
OMR::Z::TreeEvaluator::cconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("cconst", node, cg);
   TR::Register * tempReg = node->setRegister(cg->allocateRegister());
   generateLoad32BitConstant(cg, node, node->getConst<uint16_t>(), tempReg, true);
   return tempReg;
   }

#ifdef J9_PROJECT_SPECIFIC
TR::Register *
OMR::Z::TreeEvaluator::o2xEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("o2x", node, cg);
   cg->traceBCDEntry("o2x",node);
   int32_t destSize = node->getSize();

   TR::Node *srcNode = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(srcNode);
   TR_OpaquePseudoRegister *srcAggrReg = srcReg->getOpaquePseudoRegister();
   TR_ASSERT(srcAggrReg,"srcAggrReg is not an opaque register for srcNode %p\n",srcNode);
   size_t srcSize = srcAggrReg->getSize();

   if (cg->traceBCDCodeGen())
      traceMsg(cg->comp(),"\to2xEval : %s (%p) srcReg->aggrSize = %d, destSize = %d\n",node->getOpCode().getName(),node,srcSize,node->getSize());

   TR::Register *targetReg = cg->evaluateAggregateToGPR(destSize, srcNode, srcAggrReg, NULL); // srcMR = NULL

   cg->decReferenceCount(srcNode);
   node->setRegister(targetReg);
   cg->traceBCDExit("o2x",node);
   return targetReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::x2oEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("x2o", node, cg);

   cg->traceBCDEntry("x2o",node);
   TR::Node *srcNode = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(srcNode);
   size_t dstAggrSize = 0;

   TR_OpaquePseudoRegister *targetReg = cg->allocateOpaquePseudoRegister(node->getDataType());
   TR::Compilation *comp = cg->comp();

   TR_StorageReference *hint = node->getStorageReferenceHint();
   TR_StorageReference *targetStorageReference = NULL;
   if (hint)
      targetStorageReference = hint;
   else
      targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(dstAggrSize, comp);
   targetReg->setStorageReference(targetStorageReference, node);

   TR::MemoryReference *destMR = generateS390MemRefFromStorageRef(node, targetStorageReference, cg, false); // enforceSSLimits=false

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tx2oEval : %s (%p) srcSize = %d (srcAddrPrec %d), destNode->aggrSize = %d\n",
         node->getOpCode().getName(),node,srcNode->getSize(),srcNode->getType().isAddress() ? (TR::Compiler->target.is64Bit() ? 8 : 4 ) : -1, dstAggrSize);

   bool srcIsRegPair = srcReg->getRegisterPair();
   TR::Register * srcRegLow = NULL;
   TR::Register * srcRegHi  = NULL;
   TR::Register * srcReg64  = NULL;
   if (srcIsRegPair)
      {
      srcRegLow = srcReg->getLowOrder();
      srcRegHi  = srcReg->getHighOrder();
      }
   else
      {
      srcReg64 = srcReg;
      }

   size_t srcSize = srcNode->getSize();
   size_t storeSize = dstAggrSize;
   if (storeSize > srcSize)
      {
      int64_t bytesToClear = storeSize - srcSize;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tisWidening=true : clear upper %lld bytes, set storeSize to %d, add %lld to destMR offset (dstAggrSize %d > srcIntSize %d)\n",
            bytesToClear,srcSize,bytesToClear,dstAggrSize,srcSize);
      cg->genMemClear(destMR, node, bytesToClear);
      storeSize = srcSize;
      destMR = reuseS390MemoryReference(destMR, bytesToClear, node, cg, true); // enforceSSLimits=true
      }

   switch (storeSize)
      {
      case 1:
         generateRXInstruction(cg, TR::InstOpCode::STC, node, srcIsRegPair ? srcRegLow : srcReg64, destMR);
         break;
      case 2:
         generateRXInstruction(cg, TR::InstOpCode::STH, node, srcIsRegPair ? srcRegLow : srcReg64, destMR);
         break;
      case 3:
         generateRSInstruction(cg, TR::InstOpCode::STCM, node, srcIsRegPair ? srcRegLow : srcReg64, (uint32_t)0x7, destMR);
         break;
      case 4:
         generateRXInstruction(cg, TR::InstOpCode::ST, node, srcIsRegPair ? srcRegLow : srcReg64, destMR);
         break;
      case 5:
      case 6:
      case 7:
         generateRSYInstruction(cg, srcIsRegPair ? TR::InstOpCode::STCM : TR::InstOpCode::STCMH, node, srcIsRegPair ? srcRegHi : srcReg64, (uint32_t)((1 << (dstAggrSize-4)) - 1), destMR);
         generateRXInstruction(cg, TR::InstOpCode::ST, node, srcIsRegPair ? srcRegLow : srcReg64, generateS390MemoryReference(*destMR, dstAggrSize-4, cg));
         break;
      case 8:
         if (srcIsRegPair)
            {
            generateRXInstruction(cg, TR::InstOpCode::ST, node, srcRegHi, destMR);
            generateRXInstruction(cg, TR::InstOpCode::ST, node, srcRegLow, generateS390MemoryReference(*destMR, 4, cg));
            }
         else
            {
            generateRXInstruction(cg, TR::InstOpCode::STG, node, srcReg64, destMR);
            }
         break;
      default:
         TR_ASSERT(false,"unexpected dstAggrSize %d on node %p\n", dstAggrSize,node);
      }

   targetReg->setIsInitialized();
   cg->decReferenceCount(srcNode);
   node->setRegister(targetReg);
   cg->traceBCDExit("x2o",node);
   return targetReg;
   }
#endif

/**
 * labsEvaluator -
 */
TR::Register *
OMR::Z::TreeEvaluator::labsEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("labs", node, cg);
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister;
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      TR::Register * sourceRegister = cg->evaluate(firstChild);
      targetRegister = cg->allocate64bitRegister();
      generateRRInstruction(cg, TR::InstOpCode::LPGR, node, targetRegister, sourceRegister);
      }
   else
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);
      TR_ASSERT(targetRegister->getRegisterPair() != NULL, "in 32-bit mode, a 64-bit value is not in a register pair");
      TR::Register *tempReg = cg->allocateRegister();
      generateRRInstruction(cg, TR::InstOpCode::LR, node, tempReg, targetRegister->getHighOrder());
      generateRSInstruction(cg, TR::InstOpCode::SRA, node, tempReg, 31);
      generateRRInstruction(cg, TR::InstOpCode::XR, node, targetRegister->getHighOrder(), tempReg);
      generateRRInstruction(cg, TR::InstOpCode::XR, node, targetRegister->getLowOrder(), tempReg);
      generateRRInstruction(cg, TR::InstOpCode::SLR, node, targetRegister->getLowOrder(), tempReg);
      generateRRInstruction(cg, TR::InstOpCode::SLBR, node, targetRegister->getHighOrder(), tempReg);
      cg->stopUsingRegister(tempReg);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * iabsEvaluator -
 */
TR::Register *
OMR::Z::TreeEvaluator::iabsEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("iabs", node, cg);
   TR::Node * firstChild = node->getFirstChild();
   TR::Compilation *comp = cg->comp();

   TR::Register * sourceRegister = cg->evaluate(firstChild);
   TR::Register * targetRegister;
   TR::InstOpCode::Mnemonic opCode  = TR::InstOpCode::BAD;
   if (node->getOpCodeValue() == TR::fabs)
      opCode = TR::InstOpCode::LPEBR;
   else if (node->getOpCodeValue() == TR::dabs)
      opCode = TR::InstOpCode::LPDBR;
#ifdef J9_PROJECT_SPECIFIC
   else if (node->getOpCodeValue() == TR::dfabs || node->getOpCodeValue() == TR::ddabs || node->getOpCodeValue() == TR::deabs)
      opCode = TR::InstOpCode::LPDFR;
#endif
   else if (node->getOpCodeValue() == TR::iabs)
      opCode = TR::InstOpCode::getLoadPositiveRegWidenOpCode();
   else if (TR::Compiler->target.is64Bit())
      opCode = TR::InstOpCode::LPGR;
   else
      TR_ASSERT( 0,"labs for 32 bit not implemented yet");

#ifdef J9_PROJECT_SPECIFIC
   if (node->getOpCodeValue() == TR::deabs)
      {
      if (cg->canClobberNodesRegister(firstChild))
         targetRegister = sourceRegister;
      else
         targetRegister = cg->allocateFPRegisterPair();
      }
   else
#endif
        if (node->getOpCodeValue() == TR::fabs
#ifdef J9_PROJECT_SPECIFIC
            || node->getOpCodeValue() == TR::dabs
            || node->getOpCodeValue() == TR::dfabs
            || node->getOpCodeValue() == TR::ddabs
#endif
            )
      {
      if (cg->canClobberNodesRegister(firstChild))
         targetRegister = sourceRegister;
      else
         targetRegister = cg->allocateRegister(TR_FPR);
      }
   else
      {
      if (cg->canClobberNodesRegister(firstChild))
         targetRegister = sourceRegister;
      else
         targetRegister = cg->allocateRegister();
      }

#ifdef J9_PROJECT_SPECIFIC
   if (node->getOpCodeValue() == TR::deabs)
      {
      generateRRInstruction(cg, opCode, node, targetRegister->getHighOrder(), sourceRegister->getHighOrder());

      // If we aren't clobbering, also need to move the low half; otherwise, a load positive here is setting
      // the highest bit in the high-order register, so the low half can remain unchanged
      if (!cg->canClobberNodesRegister(firstChild))
         generateRRInstruction(cg, TR::InstOpCode::LDR, node, targetRegister->getLowOrder(), sourceRegister->getLowOrder());
      }
   else
#endif
      {
      generateRRInstruction(cg, opCode, node, targetRegister, sourceRegister);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * l2aEvaluator - compressed pointers
 */
TR::Register *
OMR::Z::TreeEvaluator::l2aEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   if (!comp->useCompressedPointers())
      {
      return TR::TreeEvaluator::addressCastEvaluator<64, true>(node, cg);
      }

   // if comp->useCompressedPointers
   //
   // pattern match the sequence under the l2a
   //    iaload f      l2a                       <- node
   //       aload O       ladd
   //                       lshl
   //                          i2l
   //                            iiload f        <- load
   //                               aload O
   //                          iconst shftKonst
   //                       lconst HB
   //
   // -or- if the load is known to be null
   //  l2a
   //    i2l
   //      iiload f
   //         aload O
   //
   PRINT_ME("l2a", node, cg);
   TR::Node *firstChild = node->getFirstChild();
   // after remat changes, this assume is no longer valid
   //
   /*
   bool hasCompPtrs = false;
   if ((firstChild->getOpCode().isAdd() ||
        firstChild->getOpCodeValue() == TR::lshl) &&
         firstChild->containsCompressionSequence())
      hasCompPtrs = true;
   else if (node->isNull())
      hasCompPtrs = true;

   TR_ASSERT(hasCompPtrs, "no compression sequence found under l2a node [%p]\n", node);
   */

   TR::Register* source = cg->evaluate(firstChild);

   // first child is either iu2l (when shift==0) or
   // the first child is a compression sequence. in the
   // latter case, we may need to make a copy if the register
   // is shared across the decompression sequence
   if (comp->useCompressedPointers() && (TR::Compiler->om.compressedReferenceShift() == 0 || firstChild->containsCompressionSequence()) && !node->isl2aForCompressedArrayletLeafLoad())
      source->setContainsCollectedReference();

   node->setRegister(source);

   cg->decReferenceCount(firstChild);

   return source;
   }

TR::Register *
OMR::Z::TreeEvaluator::dsqrtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   TR::Register * opRegister = cg->evaluate(firstChild);

   if (cg->canClobberNodesRegister(firstChild))
      {
      targetRegister = opRegister;
      }
   else
      {
      targetRegister = cg->allocateRegister(TR_FPR);
      }
   generateRRInstruction(cg, TR::InstOpCode::SQDBR, node, targetRegister, opRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::fsqrtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *targetRegister = NULL;
   TR::Register *opRegister = cg->evaluate(firstChild);

   if(cg->canClobberNodesRegister(firstChild))
      {
      targetRegister = opRegister;
      }
   else
      {
      targetRegister = cg->allocateRegister(TR_FPR);
      }
   generateRRInstruction(cg, TR::InstOpCode::SQEBR, node, targetRegister, opRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::dnintEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *targetRegister = NULL;
   TR::Register *opRegister = cg->evaluate(firstChild);

   if(cg->canClobberNodesRegister(firstChild))
      {
      targetRegister = opRegister;
      }
   else
      {
      targetRegister = cg->allocateRegister(TR_FPR);
      }
   generateRRInstruction(cg, TR::InstOpCode::FIDBR, node, targetRegister, opRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::fnintEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *targetRegister = NULL;
   TR::Register *opRegister = cg->evaluate(firstChild);

   if(cg->canClobberNodesRegister(firstChild))
      {
      targetRegister = opRegister;
      }
   else
      {
      targetRegister = cg->allocateRegister(TR_FPR);
      }
   generateRRInstruction(cg, TR::InstOpCode::FIEBR, node, targetRegister, opRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::getpmEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::IPM, node, targetRegister, targetRegister);

   node->setRegister(targetRegister);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::setpmEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *opRegister = cg->evaluate(node->getFirstChild());
   generateRRInstruction(cg, TR::InstOpCode::SPM, node, opRegister, opRegister);

   node->setRegister(NULL);
   cg->decReferenceCount(node->getFirstChild());
   return node->getRegister();
   }
