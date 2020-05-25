/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "lljb/IRVisitor.hpp"

#include "ilgen/TypeDictionary.hpp"
#include "ilgen/BytecodeBuilder.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <vector>

namespace lljb
{

void
IRVisitor::visitInstruction(llvm::Instruction &I)
   {
   llvm::outs() << "Unimplemented instruction being visited: " << I << "\n";
   assert(0 && "Unimplemented instruction!");
   }

void
IRVisitor::visitReturnInst(llvm::ReturnInst &I)
   {
   if (!I.getNumOperands())
      {
      _builder->Return();
      }
   else
      {
      llvm::Value * value = I.getOperand(0);
      TR::IlValue * ilValue = getIlValue(value);
      _builder->Return(ilValue);
      }
   }

void
IRVisitor::visitAllocaInst(llvm::AllocaInst &I)
   {
   TR::IlValue * ilValue = nullptr;
   if (I.getAllocatedType()->isStructTy())
      {
      llvm::StructType * llvmStruct = llvm::dyn_cast<llvm::StructType>(I.getAllocatedType());
      ilValue = _builder->CreateLocalStruct(_methodBuilder->getIlType(_td,llvmStruct));
      unsigned elIndex = 0;
      for (auto elIter = llvmStruct->element_begin();
            elIter != llvmStruct->element_end(); ++elIter)
         {
         llvm::Type * fieldType = llvmStruct->getElementType(elIndex);
         if (fieldType->isArrayTy())
            {
            TR::IlValue * arrayField = _builder->CreateLocalArray(
               fieldType->getArrayNumElements(),
               _methodBuilder->getIlType(_td,fieldType));
            _builder->StoreAt(
               _builder->StructFieldInstanceAddress(
                  llvmStruct->getStructName().data(),
                  _methodBuilder->getMemberNameFromIndex(elIndex),
                  ilValue),
               arrayField);
            }
         elIndex++;
         }
      }
   else if (I.getAllocatedType()->isArrayTy())
      {
      llvm::Type * type = I.getAllocatedType();
      unsigned numElements = type->getArrayNumElements();
      type = type->getArrayElementType();
      while (1)
         {
         if (type->isArrayTy())
            {
            numElements *= type->getArrayNumElements();
            type = type->getArrayElementType();
            }
         else
            {
            break;
            }
         }
      ilValue = _builder->CreateLocalArray(
                           numElements,
                           _methodBuilder->getIlType(_td,type));
      }
   else
      {
      _methodBuilder->allocateLocal(
      &I,
      _methodBuilder->getIlType(_td, I.getAllocatedType()));
      }
   _methodBuilder->mapIRtoIlValue(&I, ilValue);
   }

void
IRVisitor::visitLoadInst(llvm::LoadInst &I)
   {
   llvm::Value * source = I.getPointerOperand();
   TR::IlValue * loadedVal = nullptr;
   if (_methodBuilder->isIndirectLoadOrStore(source))
      {
      loadedVal = _builder->LoadAt(
                              _td->PointerTo(_methodBuilder->getIlType(_td,I.getType())),
                              getIlValue(source));
      }
   else
      {
      loadedVal = _builder->Load(_methodBuilder->getLocalNameFromValue(source));
      }
   _methodBuilder->mapIRtoIlValue(&I, loadedVal);
   }

void
IRVisitor::visitStoreInst(llvm::StoreInst &I)
   {
   llvm::Value * dest = I.getPointerOperand();
   llvm::Value * value = I.getOperand(0);
   if (_methodBuilder->isIndirectLoadOrStore(dest))
      {
      _builder->StoreAt(
               getIlValue(dest),
               getIlValue(value));
      }
   else
      {
      _builder->Store(_methodBuilder->getLocalNameFromValue(dest), getIlValue(value));
      }
   }

void
IRVisitor::visitBinaryOperator(llvm::BinaryOperator &I)
   {
   TR::IlValue * lhs = getIlValue(I.getOperand(0));
   TR::IlValue * rhs = getIlValue(I.getOperand(1));
   TR::IlValue * result = nullptr;
   switch (I.getOpcode())
      {
      case llvm::Instruction::BinaryOps::Add:
      case llvm::Instruction::BinaryOps::FAdd:
         result = _builder->Add(lhs,rhs);
         break;
      case llvm::Instruction::BinaryOps::Sub:
      case llvm::Instruction::BinaryOps::FSub:
         result = _builder->Sub(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::Mul:
      case llvm::Instruction::BinaryOps::FMul:
         result = _builder->Mul(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::SDiv:
      case llvm::Instruction::BinaryOps::FDiv:
      case llvm::Instruction::BinaryOps::UDiv:
         result = _builder->Div(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::Shl:
         if (!I.getOperand(1)->getType()->isIntegerTy(32))
               rhs = _builder->ConvertTo(_td->Int32, rhs);
         result = _builder->ShiftL(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::AShr:
         if (!I.getOperand(1)->getType()->isIntegerTy(32))
               rhs = _builder->ConvertTo(_td->Int32, rhs);
         result = _builder->ShiftR(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::LShr:
         if (!I.getOperand(1)->getType()->isIntegerTy(32))
               rhs = _builder->ConvertTo(_td->Int32, rhs);
         result = _builder->UnsignedShiftR(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::And:
         result = _builder->And(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::Or:
         result = _builder->Or(lhs, rhs);
         break;
      case llvm::Instruction::BinaryOps::Xor:
         result = _builder->Xor(lhs, rhs);
         break;
      default:
         llvm::outs() << "Instruction being visited: " << I << "\n";
         assert(0 && "Unknown binary operand");
         break;
      }
   _methodBuilder->mapIRtoIlValue(&I, result);
   }

void
IRVisitor::visitCmpInst(llvm::CmpInst &I)
   {
   TR::IlValue * lhs = getIlValue(I.getOperand(0));
   TR::IlValue * rhs = getIlValue(I.getOperand(1));

   TR::IlValue * result = nullptr;

   switch (I.getPredicate())
      {
      case llvm::CmpInst::Predicate::ICMP_EQ:
      case llvm::CmpInst::Predicate::FCMP_OEQ:
         result = _builder->EqualTo(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_NE:
      case llvm::CmpInst::Predicate::FCMP_ONE:
         result = _builder->NotEqualTo(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_UGT:
         result = _builder->UnsignedGreaterThan(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_UGE:
         result = _builder->UnsignedGreaterOrEqualTo(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_ULT:
         result = _builder->UnsignedLessThan(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_ULE:
         result = _builder->UnsignedLessOrEqualTo(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_SGT:
      case llvm::CmpInst::Predicate::FCMP_OGT:
         result = _builder->GreaterThan(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_SGE:
      case llvm::CmpInst::Predicate::FCMP_OGE:
         result = _builder->GreaterOrEqualTo(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_SLT:
      case llvm::CmpInst::Predicate::FCMP_OLT:
         result = _builder->LessThan(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::ICMP_SLE:
      case llvm::CmpInst::Predicate::FCMP_OLE:
         result = _builder->LessOrEqualTo(lhs, rhs);
         break;
      case llvm::CmpInst::Predicate::FCMP_ULE:
      case llvm::CmpInst::Predicate::FCMP_ULT:
      case llvm::CmpInst::Predicate::FCMP_UGE:
      case llvm::CmpInst::Predicate::FCMP_UNE:
      case llvm::CmpInst::Predicate::FCMP_UEQ:
      case llvm::CmpInst::Predicate::FCMP_UGT:
         assert(0 && "Unordered float comparisions are not supported yet");
         break;
      default:
         assert(0 && "Unknown CmpInst predicate");
         break;
      }
   _methodBuilder->mapIRtoIlValue(&I, result);
   }

void
IRVisitor::visitBranchInst(llvm::BranchInst &I)
   {
   if (I.isUnconditional())
      {
      llvm::Value * dest = I.getSuccessor(0);
      TR::BytecodeBuilder * destBuilder = _methodBuilder->getByteCodeBuilder(dest);
      assert(destBuilder && "failed to find builder for target basic block in unconditional branch");
      _builder->Goto(destBuilder);
      }
   else
      {
      TR::IlValue * condition = getIlValue(I.getCondition());
      TR::BytecodeBuilder * ifTrue = _methodBuilder->getByteCodeBuilder(I.getSuccessor(0));
      TR::BytecodeBuilder * ifFalse = _methodBuilder->getByteCodeBuilder(I.getSuccessor(1));
      assert(ifTrue && ifFalse && condition && "Failed to find destination blocks for conditional branch");
      _builder->IfCmpNotEqualZero(&ifTrue, condition);
      _builder->Goto(&ifFalse);
      }
   }

void
IRVisitor::visitCallInst(llvm::CallInst &I)
   {
   TR::IlValue * result = nullptr;
   llvm::Function * callee = I.getCalledFunction();
   const char * calleeName = callee->getName().data();
   std::size_t numParams = I.arg_size();
   std::vector<TR::IlValue *> params;
   std::vector<TR::IlType *> paramTypes;
   for (int i = 0; i < numParams; i++)
      {
      TR::IlValue * parameter = getIlValue(I.getArgOperand(i));
      params.push_back(parameter);
      paramTypes.push_back(_methodBuilder->getIlType(_td,I.getArgOperand(i)->getType()));
      }
   _methodBuilder->defineFunction(callee,numParams, paramTypes.data());
   result = _builder->Call(calleeName, numParams, params.data());
   _methodBuilder->mapIRtoIlValue(&I, result);
   }

void
IRVisitor::visitCastInst(llvm::CastInst &I)
   {
   TR::IlValue * srcVal = getIlValue(I.getOperand(0));
   TR::IlType * toIlType = _methodBuilder->getIlType(_td,I.getDestTy());
   TR::IlValue * result = _builder->ConvertTo(toIlType, srcVal);
   _methodBuilder->mapIRtoIlValue(&I, result);
   }

void
IRVisitor::visitZExtInst(llvm::ZExtInst &I)
   {
   TR::IlValue * srcVal = getIlValue(I.getOperand(0));
   TR::IlType * toIlType = _methodBuilder->getIlType(_td,I.getDestTy());
   TR::IlValue * result = _builder->UnsignedConvertTo(toIlType, srcVal);
   _methodBuilder->mapIRtoIlValue(&I, result);
   }

void
IRVisitor::visitGetElementPtrInst(llvm::GetElementPtrInst &I)
   {
   // GetElementPtrInst is used for address computions - such as obtaining addresses of array elements,
   // struct members, and for pointer arithmetic
   TR::IlValue * ilValue = nullptr;
   if (I.getSourceElementType()->isStructTy())
      {
      llvm::ConstantInt * indextConstantInt = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(2));
      unsigned elementIndex = indextConstantInt->getZExtValue();
      // JitBuilder requires the name of members to obtain their addresses, whereas LLVM struct members are
      // indexed by the order of the member declarations. MethodBuilder::getMemberNameFromIndex translates the
      // field index we obtain from llvm::GetElementPtrInst to member names used when defining the struct. For
      // example, struct field at index 0 would translate to "m0".
      ilValue = _builder->StructFieldInstanceAddress(I.getSourceElementType()->getStructName().data(),
                                                               _methodBuilder->getMemberNameFromIndex(elementIndex),
                                                               getIlValue(I.getOperand(0)));
      }
   else if (I.getSourceElementType()->isArrayTy())
      {
      if (I.getSourceElementType()->getArrayElementType()->isArrayTy()) // handle 2 dimensional arrays
         {
         llvm::ConstantInt * indexConstantInt = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(2));
         int64_t elementIndex = indexConstantInt->getSExtValue();
         elementIndex *= I.getSourceElementType()->getArrayNumElements();
         ilValue =
               _builder->IndexAt(
                  _td->PointerTo(
                     _methodBuilder->getIlType(_td,I.getSourceElementType()->getArrayElementType())),
                  getIlValue(I.getOperand(0)),
                  _builder->ConstInt32(elementIndex));
         }
      else // handle 1-dimensional arrays
         {
         TR::IlValue * elementIndex = getIlValue(I.getOperand(2));
         ilValue =
               _builder->IndexAt(
                  _td->PointerTo(
                     _methodBuilder->getIlType(_td,I.getSourceElementType()->getArrayElementType())),
                  getIlValue(I.getOperand(0)),
                  elementIndex);
         }
      }
   else // handle general pointer arithmetic
      {
      assert((I.getNumOperands() == 2) && "unhandled getElementPtr case");
      ilValue = _builder->IndexAt(
         _td->PointerTo(
               _methodBuilder->getIlType(_td, I.getSourceElementType())),
         getIlValue(I.getOperand(0)),
         getIlValue(I.getOperand(1)));
      }
   _methodBuilder->mapIRtoIlValue(&I, ilValue);
   }

void
IRVisitor::visitPHINode(llvm::PHINode &I)
   {

   // This initial implementation of PHINode visitor is capable of handling a
   // limited set of phi node variants, such as the phi nodes in the following
   // textual representation LLVM IR snippet:
   // ------------------
   // ; <label>:5:                                      ; preds = %19, %0
   // %6 = load i32, i32* %4, align 4
   // %7 = icmp slt i32 %6, 10
   // br i1 %7, label %8, label %17
   //
   // ; <label>:8:                                      ; preds = %5
   // %9 = load i32, i32* %2, align 4
   // %10 = load i32, i32* %3, align 4
   // %11 = icmp sge i32 %9, %10
   // br i1 %11, label %15, label %12
   //
   // ; <label>:12:                                     ; preds = %8
   // %13 = load i32, i32* %3, align 4
   // %14 = icmp sge i32 %13, 0
   // br label %15

   // ; <label>:15:                                     ; preds = %12, %8
   // %16 = phi i1 [ true, %8 ], [ %14, %12 ]           ; <----------------- phi node 1
   // br label %17
   //
   // ; <label>:17:                                     ; preds = %15, %5
   // %18 = phi i1 [ false, %5 ], [ %16, %15 ]          ; <----------------- phi node 2
   // br i1 %18, label %19, label %24
   // ------------------
   //
   // The IR above was generated from the following C snippet:
   // ------------------
   // while (i < 10 && (x >= y || y >= 0)){
   // ------------------
   //
   // In the example above, the phi node instructions are the first instructions
   // in their basic blocks, serving as merge points for 2 or more incoming
   // basic blocks. We can arrive at phi node 1 from either basic block
   // represented by %12, or basic block represented by %8. In the textual
   // representation of the IR instruction for phi node, %16 is set based on
   // whether we arrived from %8 or from %12. The textual representation of the
   // phi node instruction can have 2 or more operands enclosed in square brackets,
   // representing the "incoming values" (the first element) and  their
   // corresponding "incoming edges" (the second element). %16 is assigned the
   // incoming values true (const 1) or %14, based on whether we arrive from %8
   // or %12, respectively.
   //
   // Notice how the first incoming values are different for phi node 1 and phi
   // node 2. For phi nodes generated as a result of an "or" operation, the first
   // incoming value is "true" (1). If the phi node is generated from the result
   // of an "and" operation, the first incoming value is "false" (0). Note that the
   // "and" and "or" operators being discussed are logical "and" and "or" operators,
   // and not bitwise operators.
   //
   //
   // Unfortunately, there is no straightforward way to translate such semantics to
   // JitBuilder API calls. This initial implementation involves a few "hacks"
   // explained in the in-line comments.
   //
   //
   //

   unsigned incomingEdgeCount = I.getNumIncomingValues(); // every incoming edge will have an incoming value

   // To determine the path taken to the phi node, we need to map the incoming LLVM basic
   // blocks to the incoming IL values (obtained from mapped llvm::Value's to TR::IlValue's).
   // If the incoming value corresponding to an incoming edge is a constant integer (i.e,
   // "true" or "false"),  we need to step back into the incoming edge to map the basic block
   // to the corresponding IlValue.
   llvm::DenseMap<llvm::BasicBlock *, TR::IlValue *> valueMap;
   TR::IlValue * ilValue = nullptr; // the resulting evaluated phi node value
   llvm::ConstantInt * firstCondition = llvm::dyn_cast<llvm::ConstantInt>(I.getIncomingValue(0));
   unsigned isOr = firstCondition->getZExtValue(); // need to know whether we are dealing with phi nodes handling "or" operation

   for (unsigned i = 0; i < incomingEdgeCount; i++)
      {
      llvm::BasicBlock * basicBlock = I.getIncomingBlock(i); // we need to revisit each of the incoming basic blocks
      llvm::Value * value = I.getIncomingValue(i);           // get their corresponding incoming values
      if (value->getValueID() == llvm::Value::ValueTy::ConstantIntVal) // this evaluates to true if the incoming values are "true" or "false", i.e, 0 or 1
         {                                                             // going back to the terminating instruction is necessary if the incoming value is a constant and not the actual value that determine the control flow
         llvm::BranchInst * branchInst = llvm::dyn_cast<llvm::BranchInst>(basicBlock->getTerminator()); // the terminating instruction in incoming basic blocks are always branch instructions
         value = branchInst->getCondition();                                                            // re-assign the 0/1 available to us to the actual condition that determine the control flow
         }
      valueMap[basicBlock] = getIlValue(value);                                                         // finally, create the llvm basic block to IlValue mapping
      }

   // All the IlValues we have obtained are constants of value 0 or 1. Hence, we can use the "Or" and "And" JitBuilder services to evaluate the value that the phi node results in.
   if (isOr) ilValue = _builder->Or(valueMap[I.getIncomingBlock(0)],valueMap[I.getIncomingBlock(1)]);
   else      ilValue = _builder->And(valueMap[I.getIncomingBlock(0)],valueMap[I.getIncomingBlock(1)]);

   if (incomingEdgeCount > 2) // handle further cases of incoming edges
      {
      unsigned currentCaseIndex = 2;
      while (currentCaseIndex < incomingEdgeCount)
         {
         if (isOr) ilValue = _builder->Or(ilValue, valueMap[I.getIncomingBlock(currentCaseIndex)]);
         else      ilValue = _builder->And(ilValue, valueMap[I.getIncomingBlock(currentCaseIndex)]);
         currentCaseIndex++;
         }
      }
   _methodBuilder->mapIRtoIlValue(&I, ilValue);
   }

void
IRVisitor::visitSelectInst(llvm::SelectInst &I)
   {
   TR::IlValue * condition = getIlValue(I.getCondition());
   TR::IlValue * ifTrue = getIlValue(I.getTrueValue());
   TR::IlValue * ifFalse = getIlValue(I.getFalseValue());
   TR::IlValue * result = _builder->Select(condition, ifTrue, ifFalse);
   _methodBuilder->mapIRtoIlValue(&I, result);
   }

TR::IlValue *
IRVisitor::createConstIntIlValue(llvm::Value * value)
   {
   TR::IlValue * ilValue = nullptr;
   llvm::ConstantInt * constInt = llvm::dyn_cast<llvm::ConstantInt>(value);
   int64_t signExtendedValue = constInt->getSExtValue();
   if (constInt->getBitWidth() <= 8) ilValue = _builder->ConstInt8(signExtendedValue);
   else if (constInt->getBitWidth() <= 16) ilValue = _builder->ConstInt16(signExtendedValue);
   else if (constInt->getBitWidth() <= 32) ilValue = _builder->ConstInt32(signExtendedValue);
   else if (constInt->getBitWidth() <= 64) ilValue = _builder->ConstInt64(signExtendedValue);

   return ilValue;
   }

TR::IlValue *
IRVisitor::createConstFPIlValue(llvm::Value * value)
   {
   TR::IlValue * ilValue = nullptr;
   llvm::ConstantFP * constFP = llvm::dyn_cast<llvm::ConstantFP>(value);
   llvm::APFloat apf = constFP->getValueAPF();
   if (apf.getSizeInBits(apf.getSemantics()) <= 32)
      {
      float rawValue = apf.convertToFloat();
      ilValue = _builder->ConstFloat(rawValue);
      }
   else
      {
      double rawValue = apf.convertToDouble();
      ilValue = _builder->ConstDouble(rawValue);
      }

   return ilValue;
}

TR::IlValue *
IRVisitor::loadParameter(llvm::Value * value)
   {
   TR::IlValue * ilValue = nullptr;
   llvm::Argument * arg = llvm::dyn_cast<llvm::Argument>(value);
   ilValue = _builder->Load(_methodBuilder->getParamNameFromIndex(arg->getArgNo()));
   return ilValue;
   }

TR::IlValue *
IRVisitor::loadGlobal(llvm::Value * value)
   {
   TR::IlValue * ilValue = nullptr;
   llvm::GlobalVariable * globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value);
   ilValue = getIlValue(globalVar->getInitializer());
   return ilValue;
   }

TR::IlValue * IRVisitor::createConstExprIlValue(llvm::Value * value)
   {
   TR::IlValue * ilValue = nullptr;
   llvm::ConstantExpr * constExpr = llvm::dyn_cast<llvm::ConstantExpr>(value);
   ilValue = getIlValue(constExpr->getOperand(0));
   return ilValue;
   }

TR::IlValue *
IRVisitor::createConstantDataArrayVal(llvm::Value * value)
   {
   TR::IlValue * ilValue = nullptr;
   llvm::ConstantDataArray * constDataArray = llvm::dyn_cast<llvm::ConstantDataArray>(value);
   ilValue = _builder->ConstString(constDataArray->getAsCString().data());
   return ilValue;
   }

TR::IlValue *
IRVisitor::getIlValue(llvm::Value * value)
   {
   TR::IlValue * ilValue = _methodBuilder->getIlValue(value);
   if (ilValue) return ilValue;
   switch (value->getValueID())
      {
      /* Constants */
      case llvm::Value::ValueTy::ConstantExprVal:
         ilValue = createConstExprIlValue(value);
         break;
      case llvm::Value::ValueTy::GlobalVariableVal:
         ilValue = loadGlobal(value);
         break;
      case llvm::Value::ValueTy::FunctionVal:
      case llvm::Value::ValueTy::GlobalAliasVal:
      case llvm::Value::ValueTy::GlobalIFuncVal:
      case llvm::Value::ValueTy::BlockAddressVal:
         assert(0 && "Unsupported constant value type");
         break;

      /* Constant data value types */
      case llvm::Value::ValueTy::ConstantIntVal:
         ilValue = createConstIntIlValue(value);
         break;
      case llvm::Value::ValueTy::ConstantFPVal:
         ilValue = createConstFPIlValue(value);
         break;
      case llvm::Value::ValueTy::ConstantDataArrayVal:
         ilValue = createConstantDataArrayVal(value);
         break;
      case llvm::Value::ValueTy::ConstantAggregateZeroVal:
      case llvm::Value::ValueTy::ConstantDataVectorVal:
      case llvm::Value::ValueTy::ConstantPointerNullVal:
      case llvm::Value::ValueTy::ConstantTokenNoneVal:
         assert(0 && "Unsupported constant data value type");
         break;

      /* Constant aggregate value types */
      case llvm::Value::ValueTy::ConstantStructVal:
      case llvm::Value::ValueTy::ConstantArrayVal:
      case llvm::Value::ValueTy::ConstantVectorVal:
         assert(0 && "Unsupported constant aggregate value type");
         break;

      /* Other value types */
      case llvm::Value::ValueTy::ArgumentVal:
         ilValue = loadParameter(value);
         break;
      case llvm::Value::ValueTy::BasicBlockVal:
         assert(0 && "Basicblock should not be looked up this way");
         break;

      default:
         break;
      }
   assert (ilValue && "failed to retrieve ilValue from llvm Value!");
   return ilValue;
   }

} // namespace lljb
