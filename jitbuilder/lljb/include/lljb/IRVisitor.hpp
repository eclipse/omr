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

#ifndef LLJB_IRVISITOR_HPP
#define LLJB_IRVISITOR_HPP

#include "lljb/MethodBuilder.hpp"

#include "llvm/IR/InstVisitor.h"

namespace lljb
{

struct IRVisitor : public llvm::InstVisitor<IRVisitor>
   {
   IRVisitor(
      MethodBuilder* methodBuilder,
      TR::BytecodeBuilder* builder,
      TR::TypeDictionary * td) :
   _methodBuilder(methodBuilder),
   _builder(builder),
   _td(td)
   {}

   /************************************************************************
    *
    *  LLVM IR Instruction visitors
    *
    ************************************************************************/

   /************************************************************************
    * Implemented visitors
    ************************************************************************/
   void visitReturnInst(llvm::ReturnInst &I);
   void visitAllocaInst(llvm::AllocaInst &I);
   void visitLoadInst(llvm::LoadInst     &I);
   void visitStoreInst(llvm::StoreInst   &I);
   void visitBinaryOperator(llvm::BinaryOperator &I);

   /**
    * visitCmpInst handles the following instructions:
    * llvm::ICmpInst
    * llvm::FCmpInst
    */
   void visitCmpInst(llvm::CmpInst &I); // handles both llvm::ICmpInst and llvm::FCmpInst

   void visitBranchInst(llvm::BranchInst &I);
   void visitCallInst(llvm::CallInst &I);

   /**
    * visitCastInst handles the following instructions:
    * llvm::AddrSpaceCastInst
    * llvm::BitCastInst
    * llvm::FPExtInst
    * llvm::FPToSIInst
    * llvm::FPToUIInst
    * llvm::FPTruncInst
    * llvm::IntToPtrInst
    * llvm::PtrToIntInst
    * llvm::SExtInst
    * llvm::SIToFPInst
    * llvm::TruncInst
    * llvm::UIToFPInst
    */
   void visitCastInst(llvm::CastInst &I);
   /**
    * ZExtInst needs to be handled separately from other cast instructions
    * because we need to use UnsignedConvertTo service instead of ConvertTo
    *
    */
   void visitZExtInst(llvm::ZExtInst &I);

   void visitGetElementPtrInst(llvm::GetElementPtrInst &I);
   void visitPHINode(llvm::PHINode &I);
   void visitSelectInst(llvm::SelectInst &I);


   /************************************************************************
    * Unimplemented visitors
    ************************************************************************/

   /* Handles cases of visiting an unimplemented instruction */
   void visitInstruction(llvm::Instruction &I);

   //void visitUnaryOperator(llvm::UnaryOperator &I);
   //void visitAtomicCmpXchgInst(llvm::AtomicCmpXchgInst &I);
   //void visitAtomicRMWInst(llvm::AtomicRMWInst &I);
   //void visitFenceInst(llvm::FenceInst   &I);
   //void visitVAArgInst(llvm::VAArgInst   &I);
   //void visitExtractElementInst(llvm::ExtractElementInst &I);
   //void visitInsertElementInst(llvm::InsertElementInst &I);
   //void visitShuffleVectorInst(llvm::ShuffleVectorInst &I);
   //void visitExtractValueInst(llvm::ExtractValueInst &I);
   //void visitInsertValueInst(llvm::InsertValueInst &I);
   //void visitLandingPadInst(llvm::LandingPadInst &I);
   //void visitFuncletPadInst(llvm::FuncletPadInst &I);
   //void visitCleanupPadInst(llvm::CleanupPadInst &I);
   //void visitCatchPadInst(llvm::CatchPadInst &I);
   //void visitDbgDeclareInst(llvm::DbgDeclareInst &I);
   //void visitDbgValueInst(llvm::DbgValueInst &I);
   //void visitDbgVariableIntrinsic(llvm::DbgVariableIntrinsic &I);
   //void visitDbgLabelInst(llvm::DbgLabelInst &I);
   //void visitDbgInfoIntrinsic(llvm::DbgInfoIntrinsic &I);
   //void visitMemSetInst(llvm::MemSetInst &I);
   //void visitMemCpyInst(llvm::MemCpyInst &I);
   //void visitMemMoveInst(llvm::MemMoveInst &I);
   //void visitMemTransferInst(llvm::MemTransferInst &I);
   //void visitMemIntrinsic(llvm::MemIntrinsic &I);
   //void visitVAStartInst(llvm::VAStartInst &I);
   //void visitVAEndInst(llvm::VAEndInst &I);
   //void visitVACopyInst(llvm::VACopyInst &I);
   //void visitIntrinsicInst(llvm::IntrinsicInst &I);
   //void visitInvokeInst(llvm::InvokeInst &I);
   //void visitSwitchInst(llvm::SwitchInst &I);
   //void visitIndirectBrInst(llvm::IndirectBrInst &I);
   //void visitResumeInst(llvm::ResumeInst &I);
   //void visitUnreachableInst(llvm::UnreachableInst &I);
   //void visitCleanupReturnInst(llvm::CleanupReturnInst &I);
   //void visitCatchReturnInst(llvm::CatchReturnInst &I);
   //void visitCatchSwitchInst(llvm::CatchSwitchInst &I);
   //void visitTerminator(llvm::Instruction &I);
   //void visitUnaryInstruction(llvm::UnaryInstruction &I);
   //void visitCallBase(llvm::CallBase &I);
   //void visitCallSite(llvm::CallSite CS);



private:
   /**
    * Helpers
    */

   TR::IlValue * createConstIntIlValue(llvm::Value * value);
   TR::IlValue * createConstFPIlValue(llvm::Value * value);
   TR::IlValue * createConstExprIlValue(llvm::Value * value);
   TR::IlValue * createConstantDataArrayVal(llvm::Value * value);
   TR::IlValue * loadParameter(llvm::Value * value);
   TR::IlValue * loadGlobal(llvm::Value * value);
   TR::IlValue * getIlValue(llvm::Value * value);

   /**
    * Private fields
    */
   MethodBuilder * _methodBuilder;
   TR::BytecodeBuilder * _builder;
   TR::TypeDictionary * _td;

   }; // struct IRVisitor

} // namespace lljb

#endif
