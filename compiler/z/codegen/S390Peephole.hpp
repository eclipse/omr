/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef OMR_Z_PEEPHOLE
#define OMR_Z_PEEPHOLE

#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "env/FilePointerDecl.hpp"

class TR_S390Peephole
   {
public:
   TR_S390Peephole(TR::Compilation* comp);

protected:
   void printInfo(const char* info)
      {
      if (_outFile && comp()->getOption(TR_TraceCG))
         {
         trfprintf(_outFile, info);
         }
      }

   void printInst()
      {
      if (_outFile && comp()->getOption(TR_TraceCG))
         {
         comp()->getDebug()->print(_outFile, _cursor);
         }
      }

   TR::Compilation * comp() { return TR::comp(); }

protected:
   TR_FrontEnd * _fe;
   TR::FILE *_outFile;
   TR::Instruction *_cursor;
   TR::CodeGenerator *_cg;
   };

class TR_S390PreRAPeephole : private TR_S390Peephole
   {
public:
   TR_S390PreRAPeephole(TR::Compilation* comp);

   void perform();

private:
   /** \brief
    *     Attempts to reduce L[' '|FH|G] R,MR1  ST[' '|FH|G] R,MR2 sequences to MVC MR2, MR1
    *     to save a register and instruction.
    *
    *  \param storeOpCode
    *     The store op code that matches the load.
    *
    *  \param size
    *     The number of bits being moved.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool attemptLoadStoreReduction(TR::InstOpCode::Mnemonic storeOpCode, uint16_t size);
   };

class TR_S390PostRAPeephole : private TR_S390Peephole
   {
public:
   TR_S390PostRAPeephole(TR::Compilation* comp);

   void perform();

private:
   bool LLCReduction();
   bool LGFRReduction();
   bool AGIReduction();
   bool ICMReduction();
   bool LAReduction();
   bool NILHReduction();
   bool duplicateNILHReduction();
   bool unnecessaryNILHReduction();
   bool clearsHighBitOfAddressInReg(TR::Instruction *inst, TR::Register *reg);
   bool branchReduction();
   bool forwardBranchTarget();
   bool seekRegInFutureMemRef(int32_t ,TR::Register *);
   bool LRReduction();
   bool ConditionalBranchReduction(TR::InstOpCode::Mnemonic branchOPReplacement);
   bool CompareAndBranchReduction();
   bool LoadAndMaskReduction(TR::InstOpCode::Mnemonic LZOpCode);
   bool removeMergedNullCHK();
   bool trueCompEliminationForCompareAndBranch();
   bool trueCompEliminationForCompare();
   bool trueCompEliminationForLoadComp();
   bool attemptZ7distinctOperants();
   bool DeadStoreToSpillReduction();
   bool tryMoveImmediate();
   bool isBarrierToPeepHoleLookback(TR::Instruction *current);

   /** \brief
    *     Attempts to reduce LHI R,0 instructions to XR R,R instruction to save 2 bytes of icache.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool ReduceLHIToXR();

   // DAA related Peephole optimizations
   bool DAARemoveOutlinedLabelNop   (bool hasPadding);
   bool DAARemoveOutlinedLabelNopCVB(bool hasPadding);

   bool DAAHandleMemoryReferenceSpill(bool hasPadding);

   bool revertTo32BitShift();
   bool inlineEXtargetHelper(TR::Instruction *, TR::Instruction *);
   bool inlineEXtarget();
   void markBlockThatModifiesRegister(TR::Instruction *, TR::Register *);
   void reloadLiteralPoolRegisterForCatchBlock();
   };
#endif

