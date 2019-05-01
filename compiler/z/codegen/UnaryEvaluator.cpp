/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "ras/Debug.hpp"
#include "ras/Delimiter.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "z/codegen/S390Register.hpp"
#endif

namespace TR { class SymbolReference; }

/**
 * 64bit version lconstHelper: load long integer constant (64-bit signed 2's complement)
 */
TR::Register *
lconstHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register * longRegister = cg->allocateRegister();
   node->setRegister(longRegister);
   int64_t longValue = node->getLongInt();
   genLoadLongConstant(cg, node, longValue, longRegister);

   return longRegister;
   }

/**
 * iconst Evaluator: load integer constant (32-bit signed 2's complement)
 * @see generateLoad32BitConstant
 */
TR::Register *
OMR::Z::TreeEvaluator::iconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
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
   return lconstHelper64(node, cg);
   }

/**
 * bconst Evaluator: load byte integer constant (8-bit signed 2's complement)
 */
TR::Register *
OMR::Z::TreeEvaluator::bconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
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
   TR::Register * tempReg = node->setRegister(cg->allocateRegister());
   generateLoad32BitConstant(cg, node, node->getConst<uint16_t>(), tempReg, true);
   return tempReg;
   }

/**
 * labsEvaluator -
 */
TR::Register *
OMR::Z::TreeEvaluator::labsEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = cg->allocateRegister();;
   TR::Register * sourceRegister = cg->evaluate(firstChild);
   generateRRInstruction(cg, TR::InstOpCode::LPGR, node, targetRegister, sourceRegister);
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
