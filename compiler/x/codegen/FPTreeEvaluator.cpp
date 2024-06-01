/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "x/codegen/FPTreeEvaluator.hpp"

#include <stdint.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
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
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/FPBinaryArithmeticAnalyser.hpp"
#include "x/codegen/FPCompareAnalyser.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/RegisterRematerialization.hpp"
#include "x/codegen/X86FPConversionSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "codegen/InstOpCode.hpp"
#include "x/codegen/X86Register.hpp"

namespace TR { class Instruction; }

// Only nodes that are referenced more than this number of times
// will be rematerializable.
//
#define REMATERIALIZATION_THRESHOLD 1

// Prototypes

/**
 * @brief Coerce the value in x87 ST0 into a TR_FPR register.
 *
 * @param[in] node : \c TR::Node under evaluation
 * @param[in] dt : \c TR::DataType in ST0
 * @param[in] cg : \c TR::CodeGenerator object
 * @param[in] xmmReg : Optional \c TR::Register with \c TR_FPR kind to store the
 *               coerced result.  If provided, then St0 will be coerecd into that
 *               register.  Otherwise, a new \c TR_FPR register will be allocated
 *               to hold the result and returned.
 *
 * @return \c TR::Register of kind \c TR_FPR that holds the coerced result
 */
TR::Register *OMR::X86::TreeEvaluator::coerceST0ToFPR(TR::Node *node, TR::DataType dt, TR::CodeGenerator *cg, TR::Register *xmmReg)
   {
   if (!xmmReg)
      {
      xmmReg = cg->allocateRegister(TR_FPR);
      if (dt == TR::Float)
         {
         xmmReg->setIsSinglePrecision();
         }
      }

   TR::InstOpCode::Mnemonic st0FlushOp, xmm0LoadOp;
   TR::MemoryReference *flushMR = cg->machine()->getDummyLocalMR(dt);

   if (node->getDataType() == TR::Float)
      {
      st0FlushOp = TR::InstOpCode::FSTPMemReg;
      xmm0LoadOp = TR::InstOpCode::MOVSSRegMem;
      }
   else
      {
      st0FlushOp = TR::InstOpCode::DSTPMemReg;
      xmm0LoadOp = cg->getXMMDoubleLoadOpCode();
      }

   generateMemInstruction(st0FlushOp, node, flushMR, cg);
   generateRegMemInstruction(xmm0LoadOp, node, xmmReg, generateX86MemoryReference(*flushMR, 0, cg), cg);

   return xmmReg;
   }


TR::Register *OMR::X86::TreeEvaluator::fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);

   if (node->getFloatBits() == 0)
      {
      generateRegRegInstruction(TR::InstOpCode::XORPSRegReg, node, targetRegister, targetRegister, cg);
      }
   else
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(cg->findOrCreate4ByteConstant(node, node->getFloatBits()), cg);
      TR::Instruction *instr = generateRegMemInstruction(TR::InstOpCode::MOVSSRegMem, node, targetRegister, tempMR, cg);
      setDiscardableIfPossible(TR_RematerializableFloat, targetRegister, node, instr, (intptr_t)node->getFloatBits(), cg);
      }

   node->setRegister(targetRegister);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateRegister(TR_FPR);

  if (node->getLongInt() == 0)
     {
     generateRegRegInstruction(TR::InstOpCode::XORPDRegReg, node, targetRegister, targetRegister, cg);
     }
  else
     {
     TR::MemoryReference  *tempMR = generateX86MemoryReference(cg->findOrCreate8ByteConstant(node, node->getLongInt()), cg);
     generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, targetRegister, tempMR, cg);
     }

   node->setRegister(targetRegister);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::performFload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg)
   {
   TR::Register    *targetRegister;
   TR::Instruction *instr;

   if (cg->comp()->target().is64Bit() &&
       sourceMR->getSymbolReference().isUnresolved())
      {
      // The 64-bit mode XMM load instructions may be wider than 8-bytes (our patching
      // window) but we won't know that for sure until after register assignment.
      // Hence, the unresolved memory reference must be evaluated into a register
      // first.
      //
      TR::Register *memReg = cg->allocateRegister(TR_GPR);
      generateRegMemInstruction(TR::InstOpCode::LEA8RegMem, node, memReg, sourceMR, cg);
      sourceMR = generateX86MemoryReference(memReg, 0, cg);
      cg->stopUsingRegister(memReg);

      targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);
      instr = generateRegMemInstruction(TR::InstOpCode::MOVSSRegMem, node, targetRegister, sourceMR, cg);
      }
   else
      {
      targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);
      instr = generateRegMemInstruction(TR::InstOpCode::MOVSSRegMem, node, targetRegister, sourceMR, cg);
      setDiscardableIfPossible(TR_RematerializableFloat, targetRegister, node, instr, sourceMR, cg);
      }

   if (node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);

   node->setRegister(targetRegister);
   return targetRegister;
   }

// also handles TR::floadi
TR::Register *OMR::X86::TreeEvaluator::floadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *tempMR = generateX86MemoryReference(node, cg);
   TR::Register *targetRegister = TR::TreeEvaluator::performFload(node, tempMR, cg);
   tempMR->decNodeReferenceCounts(cg);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::performDload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg)
   {
   TR::Register    *targetRegister;
   TR::Instruction *instr;

   if (cg->comp()->target().is64Bit() && sourceMR->getSymbolReference().isUnresolved())
      {
      // The 64-bit load instructions may be wider than 8-bytes (our patching
      // window) but we won't know that for sure until after register assignment.
      // Hence, the unresolved memory reference must be evaluated into a register
      // first.
      //
      TR::Register *memReg = cg->allocateRegister(TR_GPR);
      generateRegMemInstruction(TR::InstOpCode::LEA8RegMem, node, memReg, sourceMR, cg);
      sourceMR = generateX86MemoryReference(memReg, 0, cg);
      cg->stopUsingRegister(memReg);
      }

   targetRegister = cg->allocateRegister(TR_FPR);
   instr = generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, targetRegister, sourceMR, cg);


   if (node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);
   node->setRegister(targetRegister);
   return targetRegister;
   }

// also handles TR::dloadi
TR::Register *OMR::X86::TreeEvaluator::dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *tempMR = generateX86MemoryReference(node, cg);
   TR::Register *targetRegister = TR::TreeEvaluator::performDload(node, tempMR, cg);
   tempMR->decNodeReferenceCounts(cg);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::floatingPointStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool     nodeIs64Bit    = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   bool     nodeIsIndirect = node->getOpCode().isIndirect()? 1 : 0;
   TR::Node *valueChild     = node->getChild(nodeIsIndirect);

   // Special case storing an int value into a float variable
   //
   if ((valueChild->getOpCodeValue() == TR::ibits2f || valueChild->getOpCodeValue() == TR::lbits2d) &&
       !valueChild->getRegister())
      {
      // Turn the tree into the corresponding integer store
      //
      TR::Node *integerValueChild = valueChild->getFirstChild();
      static TR::ILOpCodes integerOpCodes[2][2] = { {TR::istore, TR::lstore}, {TR::istorei, TR::lstorei} };
      TR::Node::recreate(node, integerOpCodes[nodeIsIndirect][(valueChild->getOpCodeValue() == TR::ibits2f ? 0 : 1)]);
      node->setChild(nodeIsIndirect, integerValueChild);
      integerValueChild->incReferenceCount();

      // valueChild is no longer used here
      //
      cg->recursivelyDecReferenceCount(valueChild);

      // Generate an integer store
      //
      TR::TreeEvaluator::integerStoreEvaluator(node, cg);
      return NULL;
      }

   TR::MemoryReference  *tempMR = generateX86MemoryReference(node, cg);
   TR::Instruction *exceptionPoint;

   if (valueChild->getOpCode().isLoadConst())
      {
      if (nodeIs64Bit)
         {
         if (cg->comp()->target().is64Bit())
            {
            TR::Register *floatConstReg = cg->allocateRegister(TR_GPR);
            if (valueChild->getLongInt() == 0)
               {
               generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, floatConstReg, floatConstReg, cg);
               }
            else
               {
               generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, node, floatConstReg, valueChild->getLongInt(), cg);
               }
            exceptionPoint = generateMemRegInstruction(TR::InstOpCode::S8MemReg, node, tempMR, floatConstReg, cg);
            cg->stopUsingRegister(floatConstReg);
            }
         else
            {
            exceptionPoint = generateMemImmInstruction(TR::InstOpCode::S4MemImm4, node, tempMR, valueChild->getLongIntLow(), cg);
            generateMemImmInstruction(TR::InstOpCode::S4MemImm4, node, generateX86MemoryReference(*tempMR, 4, cg), valueChild->getLongIntHigh(), cg);
            }
         }
      else
         {
         exceptionPoint = generateMemImmInstruction(TR::InstOpCode::S4MemImm4, node, tempMR, valueChild->getFloatBits(), cg);
         }
      TR::Register *firstChildReg = valueChild->getRegister();
      if (firstChildReg && firstChildReg->getKind() == TR_X87 && valueChild->getReferenceCount() == 1)
         generateFPSTiST0RegRegInstruction(TR::InstOpCode::FSTRegReg, valueChild, firstChildReg, firstChildReg, cg);
      }
   else if (debug("useGPRsForFP") &&
            (cg->getLiveRegisters(TR_GPR)->getNumberOfLiveRegisters() <
             cg->getMaximumNumbersOfAssignableGPRs() - 1) &&
            valueChild->getOpCode().isLoadVar() &&
            valueChild->getRegister() == NULL   &&
            valueChild->getReferenceCount() == 1)
      {
      TR::Register *tempRegister = cg->allocateRegister(TR_GPR);
      TR::MemoryReference  *loadMR = generateX86MemoryReference(valueChild, cg);
      generateRegMemInstruction(TR::InstOpCode::LRegMem(nodeIs64Bit), node, tempRegister, loadMR, cg);
      exceptionPoint = generateMemRegInstruction(TR::InstOpCode::SMemReg(nodeIs64Bit), node, tempMR, tempRegister, cg);
      cg->stopUsingRegister(tempRegister);
      loadMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *sourceRegister = cg->evaluate(valueChild);
      if (sourceRegister->getKind() == TR_FPR)
         {
         if (cg->comp()->target().is64Bit() &&
            tempMR->getSymbolReference().isUnresolved())
            {

            if (!tempMR->getSymbolReference().getSymbol()->isShadow() && !tempMR->getSymbolReference().getSymbol()->isClassObject() && !tempMR->getSymbolReference().getSymbol()->isConstObjectRef())
               {
               // The 64-bit static case does not require the LEA instruction as we can resolve the address in the MOV reg, imm  instruction preceeding the store.
               //
               exceptionPoint = generateMemRegInstruction(TR::InstOpCode::MOVSMemReg(nodeIs64Bit), node, tempMR, sourceRegister, cg);
               }
            else
               {
               // The 64-bit store instructions may be wider than 8-bytes (our patching
               // window) but we won't know that for sure until after register assignment.
               // Hence, the unresolved memory reference must be evaluated into a register
               // first.
               //
               TR::Register *memReg = cg->allocateRegister(TR_GPR);
               generateRegMemInstruction(TR::InstOpCode::LEA8RegMem, node, memReg, tempMR, cg);
               TR::MemoryReference *mr = generateX86MemoryReference(memReg, 0, cg);
               TR_ASSERT(nodeIs64Bit != sourceRegister->isSinglePrecision(), "Wrong operand type to floating point store\n");
               exceptionPoint = generateMemRegInstruction(TR::InstOpCode::MOVSMemReg(nodeIs64Bit), node, mr, sourceRegister, cg);

               tempMR->setProcessAsFPVolatile();

               if (cg->comp()->getOption(TR_X86UseMFENCE))
                  insertUnresolvedReferenceInstructionMemoryBarrier(cg, TR::InstOpCode::MFENCE, exceptionPoint, tempMR, sourceRegister, tempMR);
               else
                  insertUnresolvedReferenceInstructionMemoryBarrier(cg, LockOR, exceptionPoint, tempMR, sourceRegister, tempMR);

               cg->stopUsingRegister(memReg);
               }
            }
         else
            {
            TR_ASSERT(nodeIs64Bit != sourceRegister->isSinglePrecision(), "Wrong operand type to floating point store\n");
            exceptionPoint = generateMemRegInstruction(TR::InstOpCode::MOVSMemReg(nodeIs64Bit), node, tempMR, sourceRegister, cg);
            }
         }
      else
         {
         exceptionPoint = generateFPMemRegInstruction(TR::InstOpCode::FSTMemReg, node, tempMR, sourceRegister, cg);
         }
      }

   cg->decReferenceCount(valueChild);
   tempMR->decNodeReferenceCounts(cg);
   if (nodeIsIndirect)
      cg->setImplicitExceptionPoint(exceptionPoint);

   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::fpReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());
   TR_ASSERT(returnRegister, "Return node's child should evaluate to a register");
   TR::Compilation *comp = cg->comp();

   const TR::X86LinkageProperties &linkageProperties = cg->getProperties();
   TR::RealRegister::RegNum machineReturnRegister =
         (returnRegister->isSinglePrecision())? linkageProperties.getFloatReturnRegister() : linkageProperties.getDoubleReturnRegister();
   bool x87Return = false;

   /**
    *  On 32-bit targets, regardless of whether the target processor
    *  supports SSE or not, some linkages may still require a floating
    *  point value to be returned on the x87 stack (in ST0, for
    *  example).  If so, the value in an XMM register needs to be
    *  coerced into the appropriate x87 register.
    */
   if (cg->comp()->target().is32Bit() &&
       (machineReturnRegister >= TR::RealRegister::FirstFPR && machineReturnRegister <= TR::RealRegister::LastFPR) &&
       returnRegister->getKind() == TR_FPR)
      {
      TR::DataType mrType = TR::Double;
      TR::InstOpCode::Mnemonic xmmOpCode = TR::InstOpCode::MOVSDMemReg;
      TR::InstOpCode::Mnemonic x87OpCode = TR::InstOpCode::DLDMem;
      x87Return = true;

      if (returnRegister->isSinglePrecision())
         {
         mrType = TR::Float;
         xmmOpCode = TR::InstOpCode::MOVSSMemReg;
         x87OpCode = TR::InstOpCode::FLDMem;
         }

      TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(mrType);
      generateMemRegInstruction(xmmOpCode, node, tempMR, returnRegister, cg);
      generateMemInstruction(x87OpCode, node, generateX86MemoryReference(*tempMR, 0, cg), cg);
      }

   TR::RegisterDependencyConditions *dependencies = NULL;
   if (machineReturnRegister != TR::RealRegister::NoReg)
      {
      dependencies = generateRegisterDependencyConditions((uint8_t)1, 0, cg);

      if (!x87Return)
         {
         dependencies->addPreCondition(returnRegister, machineReturnRegister, cg);
         }

      dependencies->stopAddingConditions();
      }

   if (linkageProperties.getCallerCleanup())
      {
      generateFPReturnInstruction(TR::InstOpCode::RET, node, dependencies, cg);
      }
   else
      {
      generateFPReturnImmInstruction(TR::InstOpCode::RETImm2, node, 0, dependencies, cg);
      }

   if (comp->getJittedMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      comp->setReturnInfo((returnRegister->isSinglePrecision()) ? TR_FloatXMMReturn : TR_DoubleXMMReturn);
      }

   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::fpUnaryMaskEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static uint8_t MASK_FABS[] =
      {
      0xff, 0xff, 0xff, 0x7f,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };
   static uint8_t MASK_DABS[] =
      {
      0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0x7f,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };
   static uint8_t MASK_FNEG[] =
      {
      0x00, 0x00, 0x00, 0x80,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };
   static uint8_t MASK_DNEG[] =
      {
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x80,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };

   uint8_t*      mask;
   TR::InstOpCode::Mnemonic opcode;
   switch (node->getOpCodeValue())
      {
      case TR::fabs:
         mask = MASK_FABS;
         opcode = TR::InstOpCode::PANDRegMem;
         break;
      case TR::dabs:
         mask = MASK_DABS;
         opcode = TR::InstOpCode::PANDRegMem;
         break;
      case TR::fneg:
         mask = MASK_FNEG;
         opcode = TR::InstOpCode::PXORRegMem;
         break;
      case TR::dneg:
         mask = MASK_DNEG;
         opcode = TR::InstOpCode::PXORRegMem;
         break;
      default:
         TR_ASSERT(false, "Unsupported OpCode");
      }

   auto child = node->getFirstChild();
   auto value = cg->evaluate(child);
   auto result = child->getReferenceCount() == 1 ? value : cg->allocateRegister(value->getKind());

   if (result != value && value->isSinglePrecision())
      {
      result->setIsSinglePrecision();
      }

   TR::MemoryReference *mr = generateX86MemoryReference(cg->findOrCreate16ByteConstant(node, mask), cg);

   if (cg->comp()->target().cpu.supportsAVX())
      {
      generateRegRegMemInstruction(opcode, node, result, value, mr, cg);
      }
   else
      {
      if (result != value)
         {
         generateRegRegInstruction(TR::InstOpCode::MOVDQURegReg, node, result, value, cg);
         }
      generateRegMemInstruction(opcode, node, result, mr, cg);
      }

   node->setRegister(result);
   cg->decReferenceCount(child);
   return result;
   }

TR::Register *OMR::X86::TreeEvaluator::fpSqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   auto value = cg->evaluate(node->getFirstChild());
   auto result = cg->allocateRegister(value->getKind());

   if (value->isSinglePrecision())
      {
      result->setIsSinglePrecision();
      }

   generateRegRegInstruction(value->isSinglePrecision() ? TR::InstOpCode::SQRTSSRegReg : TR::InstOpCode::SQRTSDRegReg, node, result, value, cg);

   node->setRegister(result);
   cg->decReferenceCount(node->getFirstChild());
   return result;
   }

TR::Register *OMR::X86::TreeEvaluator::dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *operand = node->getFirstChild();
   TR::Register *opRegister = cg->evaluate(operand);
   TR::Register *targetRegister = cg->allocateRegister(TR_FPR);

   generateRegRegInstruction(TR::InstOpCode::SQRTSDRegReg, node, targetRegister, opRegister, cg);

   node->setRegister(targetRegister);
   cg->decReferenceCount(operand);
   return targetRegister;
   }

TR::Register* OMR::X86::TreeEvaluator::vsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL(node->getDataType().getVectorElementType().isFloatingPoint(), "Unsupported datatype for vsqrt opcode");
   return TR::TreeEvaluator::unaryVectorArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::faddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::daddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointBinaryArithmeticEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fpRemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool         nodeIsDouble = node->getDataType() == TR::Double;
   TR::Compilation *comp = cg->comp();
   const TR::X86LinkageProperties &linkageProperties = cg->getLinkage(comp->getJittedMethodSymbol()->getLinkageConvention())->getProperties();

   TR::Node *divisor = node->getSecondChild();
   TR::Node *dividend = node->getFirstChild();
   TR_RuntimeHelper remainderHelper;

   if (cg->comp()->target().is64Bit())
      {
      remainderHelper = nodeIsDouble ? TR_AMD64doubleRemainder : TR_AMD64floatRemainder;
      }
   else
      {
      remainderHelper = nodeIsDouble ? TR_IA32doubleRemainderSSE : TR_IA32floatRemainderSSE;
      }

   TR::SymbolReference *helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(remainderHelper);
   return TR::TreeEvaluator::performHelperCall(node, helperSymRef, nodeIsDouble ? TR::dcall : TR::fcall, false, cg);
   }

// also handles b2f, bu2f, s2f, su2f evaluators
TR::Register *OMR::X86::TreeEvaluator::i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::Register            *target;
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL &&
       child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      tempMR = generateX86MemoryReference(child, cg);
      target = cg->allocateSinglePrecisionRegister(TR_FPR);
      generateRegMemInstruction(TR::InstOpCode::CVTSI2SSRegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);

      switch (node->getOpCodeValue())
         {
         case TR::b2f:
            generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1, node, intReg, intReg, cg);
            break;
         case TR::bu2f:
            generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg1, node, intReg, intReg, cg);
            break;
         case TR::s2f:
            generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg2, node, intReg, intReg, cg);
            break;
         case TR::su2f:
            generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg2, node, intReg, intReg, cg);
            break;
         case TR::i2f:
            break;
         default:
            TR_ASSERT(0, "INVALID OP CODE");
            break;
         }
      target = cg->allocateSinglePrecisionRegister(TR_FPR);
      generateRegRegInstruction(TR::InstOpCode::CVTSI2SSRegReg4, node, target, intReg, cg);

      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }

// also handles b2d, bu2d, s2d, su2d evaluators
TR::Register *OMR::X86::TreeEvaluator::i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target;
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 && child->getOpCode().isLoadVar())
      {
      tempMR = generateX86MemoryReference(child, cg);
      target = cg->allocateRegister(TR_FPR);
      generateRegMemInstruction(TR::InstOpCode::CVTSI2SDRegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);

      switch (node->getOpCodeValue())
         {
         case TR::b2d:
            generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1, node, intReg, intReg, cg);
            break;
         case TR::bu2d:
            generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg1, node, intReg, intReg, cg);
            break;
         case TR::s2d:
            generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg2, node, intReg, intReg, cg);
            break;
         case TR::su2d:
            generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg2, node, intReg, intReg, cg);
            break;
         case TR::i2d:
            break;
         default:
            TR_ASSERT(0, "INVALID OP CODE");
            break;
         }

         target = cg->allocateRegister(TR_FPR);
         generateRegRegInstruction(TR::InstOpCode::CVTSI2SDRegReg4, node, target, intReg, cg);

      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }

// 32-bit float/double convert to long
//
TR::Register *OMR::X86::TreeEvaluator::fpConvertToLong(TR::Node *node, TR::SymbolReference *helperSymRef, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT_FATAL(comp->target().is32Bit(), "AMD64 doesn't use this logic");

   TR::Node *child = node->getFirstChild();

   if (child->getOpCode().isDouble())
      {
      TR::RegisterDependencyConditions  *deps;

      TR::Register        *doubleReg = cg->evaluate(child);
      TR::Register        *lowReg    = cg->allocateRegister(TR_GPR);
      TR::Register        *highReg   = cg->allocateRegister(TR_GPR);
      TR::RealRegister    *espReal   = cg->machine()->getRealRegister(TR::RealRegister::esp);

      deps = generateRegisterDependencyConditions((uint8_t) 0, 3, cg);
      deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(doubleReg, TR::RealRegister::NoReg, cg);
      deps->stopAddingConditions();

      TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);   // exit routine label
      TR::LabelSymbol *CallLabel    = TR::LabelSymbol::create(cg->trHeapMemory(),cg);   // label where long (64-bit) conversion will start
      TR::LabelSymbol *StartLabel   = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      StartLabel->setStartInternalControlFlow();
      reStartLabel->setEndInternalControlFlow();

      // Attempt to convert a double in an XMM register to an integer using CVTTSD2SI.
      // If the conversion succeeds, put the integer in lowReg and sign-extend it to highReg.
      // If the conversion fails (the double is too large), call the helper.
      generateRegRegInstruction(TR::InstOpCode::CVTTSD2SIReg4Reg, node, lowReg, doubleReg, cg);
      generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, lowReg, 0x80000000, cg);

      generateLabelInstruction(TR::InstOpCode::label, node, StartLabel, cg);
      generateLabelInstruction(TR::InstOpCode::JE4, node, CallLabel, cg);

      generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, highReg ,lowReg, cg);
      generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, highReg , 31, cg);

      generateLabelInstruction(TR::InstOpCode::label, node, reStartLabel, deps, cg);

      TR::Register *targetRegister = cg->allocateRegisterPair(lowReg, highReg);
      TR::SymbolReference *d2l = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_IA32double2LongSSE);
      d2l->getSymbol()->getMethodSymbol()->setLinkage(TR_Helper);
      TR::Node::recreate(node, TR::lcall);
      node->setSymbolReference(d2l);
      TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::lcall, targetRegister, CallLabel, reStartLabel, cg);
      cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

      cg->decReferenceCount(child);
      node->setRegister(targetRegister);

      return targetRegister;
      }
   else
      {
      TR::Register *accReg    = NULL;
      TR::Register *lowReg    = cg->allocateRegister(TR_GPR);
      TR::Register *highReg   = cg->allocateRegister(TR_GPR);
      TR::Register *floatReg = cg->evaluate(child);

      TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *startLabel   = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      startLabel->setStartInternalControlFlow();
      reStartLabel->setEndInternalControlFlow();

      generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);

      // These instructions must be set appropriately prior to the creation
      // of the snippet near the end of this method. Also see warnings below.
      //
      TR::X86RegMemInstruction          *loadHighInstr;    // loads the high dword of the converted long
      TR::X86RegMemInstruction          *loadLowInstr;     // loads the low dword of the converted long

      TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(TR::Float);
      generateMemRegInstruction(TR::InstOpCode::MOVSSMemReg, node, tempMR, floatReg, cg);
      generateMemInstruction(TR::InstOpCode::FLDMem, node, generateX86MemoryReference(*tempMR, 0, cg), cg);

      generateInstruction(TR::InstOpCode::FLDDUP, node, cg);

      // For slow conversion only, change the rounding mode on the FPU via its control word register.
      //
      TR::MemoryReference  *convertedLongMR = (cg->machine())->getDummyLocalMR(TR::Int64);

      if (cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_SSE3))
         {
         generateMemInstruction(TR::InstOpCode::FLSTTPMem, node, convertedLongMR, cg);
         }
      else
         {
         int16_t fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ?
                        SINGLE_PRECISION_ROUND_TO_ZERO : DOUBLE_PRECISION_ROUND_TO_ZERO;
         generateMemInstruction(TR::InstOpCode::LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);
         generateMemInstruction(TR::InstOpCode::FLSTPMem, node, convertedLongMR, cg);

         fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ?
                SINGLE_PRECISION_ROUND_TO_NEAREST : DOUBLE_PRECISION_ROUND_TO_NEAREST;

         generateMemInstruction(TR::InstOpCode::LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);
         }

      // WARNING:
      //
      // The following load instructions are dissected in the snippet to determine the target registers.
      // If they or their format is changed, you may need to change the snippet also.
      //
      loadHighInstr = generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, highReg,
                                                generateX86MemoryReference(*convertedLongMR, 4, cg), cg);

      loadLowInstr = generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, lowReg,
                                               generateX86MemoryReference(*convertedLongMR, 0, cg), cg);

      // Jump to the snippet if the converted value is an indefinite integer; otherwise continue.
      //
      generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, highReg, INT_MIN, cg);
      generateLabelInstruction(TR::InstOpCode::JNE4, node, reStartLabel, cg);
      generateRegRegInstruction(TR::InstOpCode::TEST4RegReg, node, lowReg, lowReg, cg);
      generateLabelInstruction(TR::InstOpCode::JE4, node, snippetLabel, cg);

      // Create the conversion snippet.
      //
      cg->addSnippet( new (cg->trHeapMemory()) TR::X86FPConvertToLongSnippet(reStartLabel,
                                                                             snippetLabel,
                                                                             helperSymRef,
                                                                             node,
                                                                             loadHighInstr,
                                                                             loadLowInstr,
                                                                             cg) );

      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, accReg ? 3 : 2, cg);

      // Make sure the high and low long registers are assigned to something.
      //
      if (accReg)
         {
         deps->addPostCondition(accReg, TR::RealRegister::eax, cg);
         }

      deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);

      generateLabelInstruction(TR::InstOpCode::label, node, reStartLabel, deps, cg);

      cg->decReferenceCount(child);
      generateInstruction(TR::InstOpCode::FSTPST0, node, cg);

      TR::Register *targetRegister = cg->allocateRegisterPair(lowReg, highReg);
      node->setRegister(targetRegister);
      return targetRegister;
      }
   }

// On AMD64, all four [fd]2[il] conversions are handled here
// On IA32, both [fd]2i conversions are handled here
TR::Register *OMR::X86::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool doubleSource;
   bool longTarget;
   TR::InstOpCode::Mnemonic cvttOpCode;

   switch (node->getOpCodeValue())
      {
      case TR::f2i:
         cvttOpCode   = TR::InstOpCode::CVTTSS2SIReg4Reg;
         doubleSource = false;
         longTarget   = false;
         break;
      case TR::f2l:
         cvttOpCode   = TR::InstOpCode::CVTTSS2SIReg8Reg;
         doubleSource = false;
         longTarget   = true;
         break;
      case TR::d2i:
         cvttOpCode   = TR::InstOpCode::CVTTSD2SIReg4Reg;
         doubleSource = true;
         longTarget   = false;
         break;
      case TR::d2l:
         cvttOpCode   = TR::InstOpCode::CVTTSD2SIReg8Reg;
         doubleSource = true;
         longTarget   = true;
         break;
      default:
         TR_ASSERT_FATAL(0, "Unknown opcode value in f2iEvaluator");
         break;
      }
   TR_ASSERT_FATAL(cg->comp()->target().is64Bit() || !longTarget, "Incorrect opcode value in f2iEvaluator");

   TR::Node        *child          = node->getFirstChild();
   TR::Register    *sourceRegister = NULL;
   TR::Register    *targetRegister = cg->allocateRegister(TR_GPR);
   TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *exceptionLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   sourceRegister = cg->evaluate(child);
   generateRegRegInstruction(cvttOpCode, node, targetRegister, sourceRegister, cg);

   startLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);

   if (longTarget)
      {
      TR_ASSERT_FATAL(cg->comp()->target().is64Bit(), "We should only get here on AMD64");
      // We can't compare with 0x8000000000000000.
      // Instead, rotate left 1 bit and compare with 0x0000000000000001.
      generateRegInstruction(TR::InstOpCode::ROL8Reg1, node, targetRegister, cg);
      generateRegImmInstruction(TR::InstOpCode::CMP8RegImms, node, targetRegister, 1, cg);
      }
   else
      {
      generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, targetRegister, INT_MIN, cg);
      }

   generateLabelInstruction(TR::InstOpCode::JE4, node, exceptionLabel, cg);

   //TODO: (omr issue #4969): Remove once support for spills in OOL paths is added
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)2, cg);
   deps->addPostCondition(targetRegister, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(sourceRegister, TR::RealRegister::NoReg, cg);

      {
      TR_OutlinedInstructionsGenerator og(exceptionLabel, node, cg);
      // at this point, target is set to -INF and there can only be THREE possible results: -INF, +INF, NaN
      // compare source with ZERO
      generateRegMemInstruction(doubleSource ? TR::InstOpCode::UCOMISDRegMem : TR::InstOpCode::UCOMISSRegMem,
                                node,
                                sourceRegister,
                                generateX86MemoryReference(doubleSource ? cg->findOrCreate8ByteConstant(node, 0) : cg->findOrCreate4ByteConstant(node, 0), cg),
                                cg);
      // load max int if source is positive, note that for long case, LLONG_MAX << 1 is loaded as it will be shifted right
      generateRegMemInstruction(TR::InstOpCode::CMOVARegMem(longTarget),
                                node,
                                targetRegister,
                                generateX86MemoryReference(longTarget ? cg->findOrCreate8ByteConstant(node, LLONG_MAX << 1) : cg->findOrCreate4ByteConstant(node, INT_MAX), cg),
                                cg);
      // load zero if source is NaN
      generateRegMemInstruction(TR::InstOpCode::CMOVPRegMem(longTarget),
                                node,
                                targetRegister,
                                generateX86MemoryReference(longTarget ? cg->findOrCreate8ByteConstant(node, 0) : cg->findOrCreate4ByteConstant(node, 0), cg),
                                cg);

      generateLabelInstruction(TR::InstOpCode::JMP4, node, endLabel, cg);
      og.endOutlinedInstructionSequence();
      }

   generateLabelInstruction(TR::InstOpCode::label, node, endLabel, deps, cg);
   if (longTarget)
      {
      generateRegInstruction(TR::InstOpCode::ROR8Reg1, node, targetRegister, cg);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(child);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 uses f2iEvaluator for this");
   return TR::TreeEvaluator::fpConvertToLong(node, cg->symRefTab()->findOrCreateRuntimeHelper(TR_IA32floatToLong), cg);
   }


TR::Register *OMR::X86::TreeEvaluator::f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *targetRegister = cg->floatClobberEvaluate(child);

   targetRegister->setIsSinglePrecision(false);
   generateRegRegInstruction(TR::InstOpCode::CVTSS2SDRegReg, node, targetRegister, targetRegister, cg);

   node->setRegister(targetRegister);
   cg->decReferenceCount(child);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("f2b not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("f2s not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::f2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("f2c not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 uses f2iEvaluator for this");

   return TR::TreeEvaluator::fpConvertToLong(node, cg->symRefTab()->findOrCreateRuntimeHelper(TR_IA32doubleToLong), cg);
   }


TR::Register *OMR::X86::TreeEvaluator::d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *targetRegister = cg->doubleClobberEvaluate(child);
   targetRegister->setIsSinglePrecision(true);
   generateRegRegInstruction(TR::InstOpCode::CVTSD2SSRegReg, node, targetRegister, targetRegister, cg);

   node->setRegister(targetRegister);
   cg->decReferenceCount(child);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("d2b not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("d2s not expected!");
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("d2s not expected!");
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::Register            *target;
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL &&
       child->getOpCode().isLoadVar())
      {
      // Load up the child as a float, then as an int if necessary.
      tempMR = generateX86MemoryReference(child, cg);
      target = cg->allocateSinglePrecisionRegister(TR_FPR);
      generateRegMemInstruction(TR::InstOpCode::MOVSSRegMem, node, target, tempMR, cg);

      if (child->getReferenceCount() > 1)
         {
         TR::Register *intReg = cg->allocateRegister();
         generateRegRegInstruction(TR::InstOpCode::MOVDReg4Reg, node, intReg, target, cg);
         child->setRegister(intReg);
         }

      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);
      target = cg->allocateSinglePrecisionRegister(TR_FPR);
      generateRegRegInstruction(TR::InstOpCode::MOVDRegReg4, node, target, intReg, cg);
      }

   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }

TR::Register *OMR::X86::TreeEvaluator::fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = cg->allocateRegister();
   TR::MemoryReference  *tempMR;

   // Ref count == 1 check might not be reqd for statics/autos (?)
   //
   if (child->getRegister() == NULL &&
       child->getOpCode().isLoadVar() &&
       (child->getReferenceCount() == 1))
      {
      // Load up the child as an int.
      //
      tempMR = generateX86MemoryReference(child, cg);
      generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      // Move the float value from a FPR or XMMR to a GPR.
      //
      TR::Register *floatReg = cg->evaluate(child);
      if (floatReg->getKind() == TR_FPR)
         {
         tempMR = cg->machine()->getDummyLocalMR(TR::Int32);
         generateMemRegInstruction(TR::InstOpCode::MOVSSMemReg, node, tempMR, floatReg, cg);
         generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      else
         {
         tempMR = cg->machine()->getDummyLocalMR(TR::Int32);
         generateFPMemRegInstruction(TR::InstOpCode::FSTMemReg, node, tempMR, floatReg, cg);
         generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      }

   // Check for the special case where the child is a NaN,
   // which has to be normalized to a particular NaN value.
   //
   if (node->normalizeNanValues())
      {
      static char *disableFastNormalizeNaNs = feGetEnv("TR_disableFastNormalizeNaNs");
      TR::LabelSymbol *lab0 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      if (disableFastNormalizeNaNs)
         {
         TR::LabelSymbol *lab1 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *lab2 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         lab0->setStartInternalControlFlow();
         lab2->setEndInternalControlFlow();
         generateLabelInstruction(TR::InstOpCode::label, node, lab0, cg);
         generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, target, FLOAT_NAN_1_LOW, cg);
         generateLabelInstruction(TR::InstOpCode::JGE4, node, lab1, cg);
         generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, target, FLOAT_NAN_2_LOW, cg);
         generateLabelInstruction(TR::InstOpCode::JB4, node, lab2, cg);
         generateLabelInstruction(TR::InstOpCode::label, node, lab1, cg);
         generateRegImmInstruction(TR::InstOpCode::MOV4RegImm4, node, target, FLOAT_NAN, cg);
         TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         deps->addPostCondition(target, TR::RealRegister::NoReg, cg);
         generateLabelInstruction(TR::InstOpCode::label, node, lab2, deps, cg);
         }
      else
         {
         // A bunch of bookkeeping
         //
         TR::Register *treg    = target;
         uint32_t nanDetector = FLOAT_NAN_2_LOW;

         TR::RegisterDependencyConditions  *internalControlFlowDeps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         internalControlFlowDeps->addPostCondition(treg, TR::RealRegister::NoReg, cg);

         TR::RegisterDependencyConditions  *helperDeps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
         helperDeps->addPreCondition( treg, TR::RealRegister::eax, cg);
         helperDeps->addPostCondition(treg, TR::RealRegister::eax, cg);

         TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *slowPathLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *normalizeLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         startLabel->setStartInternalControlFlow();
         endLabel  ->setEndInternalControlFlow();

         // Fast path: if subtracting nanDetector leaves CF=0 or OF=1, then it
         // must be a NaN.
         //
         generateLabelInstruction(  TR::InstOpCode::label,       node, startLabel,        cg);
         generateRegImmInstruction( TR::InstOpCode::CMP4RegImm4, node, treg, nanDetector, cg);
         generateLabelInstruction(  TR::InstOpCode::JAE4,        node, slowPathLabel,     cg);
         generateLabelInstruction(  TR::InstOpCode::JO4,         node, slowPathLabel,     cg);

         // Slow path
         //
         {
         TR_OutlinedInstructionsGenerator og(slowPathLabel, node, cg);
         generateRegImmInstruction( TR::InstOpCode::MOV4RegImm4, node, treg, FLOAT_NAN, cg);
         generateLabelInstruction(  TR::InstOpCode::JMP4,        node, endLabel,        cg);
         og.endOutlinedInstructionSequence();
         }

         // Merge point
         //
         generateLabelInstruction(TR::InstOpCode::label, node, endLabel, internalControlFlowDeps, cg);
         }
      }

   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }

TR::Register *OMR::X86::TreeEvaluator::fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateSinglePrecisionRegister(TR_FPR);
      node->setRegister(globalReg);
      }

   return globalReg;
   }

TR::Register *OMR::X86::TreeEvaluator::dRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister(TR_FPR);
      node->setRegister(globalReg);
      }

   return globalReg;
   }


TR::Register *OMR::X86::TreeEvaluator::fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   int32_t globalRegNum = node->getGlobalRegisterNumber();
   TR::Machine *machine = cg->machine();
   int32_t fpStackSlot = globalRegNum - machine->getNumGlobalGPRs();
   TR::Register *childGlobalReg = cg->machine()->getFPStackRegister(fpStackSlot);
   TR::Register *globalReg = cg->evaluate(child);

   TR_ASSERT_FATAL(globalReg->getKind() == TR_FPR, "Register must be type TR_FPR in fRegStoreEvaluator");

   machine->setXMMGlobalRegister(globalRegNum - machine->getNumGlobalGPRs(), globalReg);
   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *OMR::X86::TreeEvaluator::dRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   int32_t globalRegNum = node->getGlobalRegisterNumber();
   TR::Machine *machine = cg->machine();
   int32_t fpStackSlot = globalRegNum - machine->getNumGlobalGPRs();
   TR::Register *childGlobalReg = cg->machine()->getFPStackRegister(fpStackSlot);
   TR::Register *globalReg = cg->evaluate(child);

   TR_ASSERT_FATAL(globalReg->getKind() == TR_FPR, "Register must be type TR_FPR in dRegStoreEvaluator");

   machine->setXMMGlobalRegister(globalRegNum - machine->getNumGlobalGPRs(), globalReg);
   cg->decReferenceCount(child);
   return globalReg;
   }



TR::Register *OMR::X86::TreeEvaluator::generateBranchOrSetOnFPCompare(
      TR::Node *node,
      bool generateBranch,
      TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = NULL;
   TR::RegisterDependencyConditions *deps = NULL;

   if (generateBranch)
      {
      if (node->getNumChildren() == 3 && node->getChild(2)->getNumChildren() != 0)
         {
         TR::Node *third = node->getChild(2);
         cg->evaluate(third);
         deps = generateRegisterDependencyConditions(third, cg, 1);
         deps->stopAddingConditions();
         }
      }

   // If using UCOMISS/UCOMISD *and* we could not avoid (if)(f|d)cmp(neu|eq),
   // generate multiple instructions. We always avoid FCOMI in these cases.
   // Otherwise, select the appropriate branch or set opcode and emit the
   // instruction.
   //
   TR::ILOpCodes cmpOp = node->getOpCodeValue();

   if (cmpOp == TR::iffcmpneu || cmpOp == TR::fcmpneu ||
       cmpOp == TR::ifdcmpneu || cmpOp == TR::dcmpneu )
      {
      if (generateBranch)
         {
         // We want the pre-conditions on the first branch, and the post-conditions
         // on the last one.
         //
         TR::RegisterDependencyConditions  *deps1 = NULL;
         if (deps && deps->getPreConditions())
            {
            deps1 = deps->clone(cg);
            deps1->setNumPostConditions(0, cg->trMemory());
            deps->setNumPreConditions(0, cg->trMemory());
            }
         generateLabelInstruction(TR::InstOpCode::JPE4, node, node->getBranchDestination()->getNode()->getLabel(), deps1, cg);
         generateLabelInstruction(TR::InstOpCode::JNE4, node, node->getBranchDestination()->getNode()->getLabel(), deps, cg);
         }
      else
         {
         TR::Register *tempRegister = cg->allocateRegister();
         targetRegister = cg->allocateRegister();
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(tempRegister);
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(TR::InstOpCode::SETPE1Reg, node, tempRegister, cg);
         generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, targetRegister, cg);
         generateRegRegInstruction(TR::InstOpCode::OR1RegReg, node, targetRegister, tempRegister, cg);
         generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
         cg->stopUsingRegister(tempRegister);
         }
      }
   else if (cmpOp == TR::iffcmpeq || cmpOp == TR::fcmpeq ||
            cmpOp == TR::ifdcmpeq || cmpOp == TR::dcmpeq )
      {
      if (generateBranch)
         {
         TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *fallThroughLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         startLabel->setStartInternalControlFlow();
         fallThroughLabel->setEndInternalControlFlow();

         TR::RegisterDependencyConditions  *deps1 = NULL;
         if (deps && deps->getPreConditions())
            {
            deps1 = deps->clone(cg);
            deps1->setNumPostConditions(0, cg->trMemory());
            deps->setNumPreConditions(0, cg->trMemory());
            }
         generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);
         generateLabelInstruction(TR::InstOpCode::JPE4, node, fallThroughLabel, deps1, cg);
         generateLabelInstruction(TR::InstOpCode::JE4, node, node->getBranchDestination()->getNode()->getLabel(), cg);
         generateLabelInstruction(TR::InstOpCode::label, node, fallThroughLabel, deps, cg);
         }
      else
         {
         TR::Register *tempRegister = cg->allocateRegister();
         targetRegister = cg->allocateRegister();
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(tempRegister);
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(TR::InstOpCode::SETPO1Reg, node, tempRegister, cg);
         generateRegInstruction(TR::InstOpCode::SETE1Reg, node, targetRegister, cg);
         generateRegRegInstruction(TR::InstOpCode::AND1RegReg, node, targetRegister, tempRegister, cg);
         generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
         cg->stopUsingRegister(tempRegister);
         }
      }
   else
      {
      TR::InstOpCode::Mnemonic op = getBranchOrSetOpCodeForFPComparison(node->getOpCodeValue());
      if (generateBranch)
         {
         generateLabelInstruction(op, node, node->getBranchDestination()->getNode()->getLabel(), deps, cg);
         }
      else
         {
         targetRegister = cg->allocateRegister();
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(op, node, targetRegister, cg);
         generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
         }
      }

   node->setRegister(targetRegister);
   return targetRegister;
   }


// Create FP compare code for the specified comparison class.
//
void OMR::X86::TreeEvaluator::compareFloatOrDoubleForOrder(
      TR::Node *node,
      TR::InstOpCode::Mnemonic xmmCmpRegRegOpCode,
      TR::InstOpCode::Mnemonic xmmCmpRegMemOpCode,
      TR::CodeGenerator *cg)
   {
   TR_IA32XMMCompareAnalyser temp(cg);
   temp.xmmCompareAnalyser(node, xmmCmpRegRegOpCode, xmmCmpRegMemOpCode);
   }


TR::Register *OMR::X86::TreeEvaluator::generateFPCompareResult(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();
   generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);

   TR::Register *targetRegister = cg->allocateRegister(TR_GPR);
   cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
   generateRegInstruction(TR::InstOpCode::SETA1Reg, node, targetRegister, cg);
   generateLabelInstruction(TR::InstOpCode::JAE4, node, doneLabel, cg);

   if (node->getOpCodeValue() == TR::fcmpg || node->getOpCodeValue() == TR::dcmpg)
      {
      generateRegInstruction(TR::InstOpCode::SETPE1Reg, node, targetRegister, cg);
      generateLabelInstruction(TR::InstOpCode::JPE4, node, doneLabel, cg);
      }

   generateRegInstruction(TR::InstOpCode::DEC1Reg, node, targetRegister, cg);

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
   deps->addPostCondition(targetRegister, TR::RealRegister::NoReg, cg);
   generateLabelInstruction(TR::InstOpCode::label, node, doneLabel, deps, cg);

   generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1, node, targetRegister, targetRegister, cg);

   node->setRegister(targetRegister);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::compareFloatAndBranchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareFloatOrDoubleForOrder(
      node,
      TR::InstOpCode::UCOMISSRegReg,
      TR::InstOpCode::UCOMISSRegMem,
      cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareDoubleAndBranchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareFloatOrDoubleForOrder(
      node,
      TR::InstOpCode::UCOMISDRegReg,
      TR::InstOpCode::UCOMISDRegMem,
      cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareFloatAndSetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareFloatOrDoubleForOrder(
      node,
      TR::InstOpCode::UCOMISSRegReg,
      TR::InstOpCode::UCOMISSRegMem,
      cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareDoubleAndSetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareFloatOrDoubleForOrder(
      node,
      TR::InstOpCode::UCOMISDRegReg,
      TR::InstOpCode::UCOMISDRegMem,
      cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareFloatEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareFloatOrDoubleForOrder(
      node,
      TR::InstOpCode::UCOMISSRegReg,
      TR::InstOpCode::UCOMISSRegMem,
      cg);
   return TR::TreeEvaluator::generateFPCompareResult(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareDoubleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareFloatOrDoubleForOrder(
      node,
      TR::InstOpCode::UCOMISDRegReg,
      TR::InstOpCode::UCOMISDRegMem,
      cg);
   return TR::TreeEvaluator::generateFPCompareResult(node, cg);
   }
