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

#include "z/codegen/OpMemToMem.hpp"

#include <limits.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/S390Evaluator.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"

bool useLockedLitPoolBaseRegister(TR::CodeGenerator *cg)
   {
   if (cg->isLiteralPoolOnDemandOn())
      return false;
   return true;
   }

TR::Register *allocateRegForLiteralPoolBase(TR::CodeGenerator *cg)
   {
   if (useLockedLitPoolBaseRegister(cg))
      return cg->getLitPoolRealRegister();
   else
      return cg->allocateRegister();
   }

/**
 * Generate the loop that will perform a memory-to-memory instruction
 * on 256 bytes.
 * The format of the loop is:
 *  <copy length (in bytes) into _itersReg>
 *  <shift by 8 to determine number of loop iterations on 256 bytes>
 *  <if 0, branch around loop>
 *  <make virtual call out to specific loop body routine>
 *  <generate BRCT to branch back based on _itersReg>
 *  <create register dependencies for internal control flow>
 *
 * generate a loop of 256 byte mem ops
 */
TR::Instruction *
MemToMemVarLenMacroOp::generateLoop()
   {
   TR::Compilation *comp = _cg->comp();
   bool needs64BitOpCode = comp->target().is64Bit();

   if (useEXForRemainder())
      {

      // need to do this before the branch or some
      // instructions may not be executed. this is a problem
      // when nodes from memrefs are commoned with something
      // down below the branch label
      generateSrcMemRef(0);
      generateDstMemRef(0);

      // non-Java specialization
      if (!_lengthMinusOne)
         {
         generateRIInstruction(_cg, (needs64BitOpCode) ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, _rootNode, _regLen, -1);
         }

      if (_lengthMinusOne)
         generateRRInstruction(_cg, TR::InstOpCode::LTR, _rootNode, _regLen, _regLen); //Because transformLengthMinusOneForMemoryOps uses TR::iadd

      _doneLabel  = generateLabelSymbol(_cg);
      _startControlFlow = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, _rootNode, _doneLabel);
      }
   if (getKind() == MemToMemMacroOp::IsMemInit)
      generateInstruction(0, 1);

   TR::LabelSymbol * topOfLoop = generateLabelSymbol(_cg);
   TR::LabelSymbol * bottomOfLoop = generateLabelSymbol(_cg);

   //
   // But first, load up the branch address into raReg for two reasons:
   // 1) to avoid the AGI on the indirection and (more importantly)
   // 2) to ensure that no weird spilling happens if the code decides it needs
   //    to allocate a register at this point for the literal pool base address.
   intptr_t helper = 0;

   if (!useEXForRemainder())
      {
      helper = getHelper();

      if (_raReg == NULL)
         _raReg = _cg->allocateRegister();

      //use literal for aot to make it easier for relocation
      if (_cg->needRelocationsForHelpers())
         {
         generateRegLitRefInstruction(_cg, TR::InstOpCode::getLoadOpCode(), _rootNode, _raReg, (uintptr_t)getHelperSymRef(), TR_HelperAddress, NULL, NULL, NULL);
         }
      else
         {
         genLoadAddressConstant(_cg, _rootNode, helper, _raReg);
         }
      }

   if (_itersReg == NULL)
      _itersReg = _cg->allocateRegister();

   if (needs64BitOpCode)
      {
      generateRSInstruction(_cg, TR::InstOpCode::SRAG, _rootNode, _itersReg, _regLen, 8);
      }
   else
      {
      if (_cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
         {
         generateRSInstruction(_cg, TR::InstOpCode::SRAK, _rootNode, _itersReg, _regLen, 8);
         }
      else
         {
         generateRRInstruction(_cg, TR::InstOpCode::LR, _rootNode, _itersReg, _regLen);
         generateRSInstruction(_cg, TR::InstOpCode::SRA, _rootNode, _itersReg, 8);
         }
      }

   generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _rootNode, bottomOfLoop);

   generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, topOfLoop);

   generateInstruction(0, 256);
   generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, 256, _cg));
   if (_srcReg != _dstReg)
      {
      generateRXInstruction(_cg, TR::InstOpCode::LA, _dstNode, _dstReg, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 256, _cg));
      }

   generateS390BranchInstruction(_cg, TR::InstOpCode::BRCT, _rootNode, _itersReg, topOfLoop);

   if (_cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && !comp->getOption(TR_DisableInlineEXTarget))
      {
      if (useEXForRemainder())
         {
         generateSrcMemRef(0);
         generateDstMemRef(0);

         generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, _rootNode, bottomOfLoop);

         generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, _EXTargetLabel = generateLabelSymbol(_cg));

         // This instruction will be used as the target of the EXRL in the remainder calculation
         generateInstruction(0, 1);
         }
      }

   TR::Instruction * cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, bottomOfLoop);

   _cg->stopUsingRegister(_itersReg);

   if (!useEXForRemainder())
      _cg->stopUsingRegister(_raReg);

   return cursor;
   }

TR::Instruction *
MemToMemMacroOp::genSrcLoadAddress(int32_t offset, TR::Instruction *cursor)
   {
   TR_ASSERT(_srcMR,"_srcMR must be non-NULL when _srcReg is NULL for node %p\n",_srcNode);
   _srcRegTemp = _cg->allocateRegister();
   cursor = _cg->genLoadAddressToRegister(_srcRegTemp, reuseS390MemoryReference(_srcMR, offset, _srcNode, _cg, false), _srcNode, cursor);

   _srcReg = _srcRegTemp;
   _srcMR = NULL; // use _srcReg from here on out in generateInstruction

   return cursor;
   }

TR::Instruction *
MemToMemMacroOp::genDstLoadAddress(int32_t offset, TR::Instruction *cursor)
   {
   TR_ASSERT(_dstMR,"_dstMR must be non-NULL when _dstReg is NULL for node %p\n",_dstNode);
   _dstRegTemp = _cg->allocateRegister();

   cursor = _cg->genLoadAddressToRegister(_dstRegTemp, reuseS390MemoryReference(_dstMR, offset, _dstNode, _cg, false), _dstNode, cursor);

   _dstReg = _dstRegTemp;
   _dstMR = NULL; // use _dstReg from here on out in generateInstruction

   return cursor;
   }

/**
 * Very similar to the variable length logic in MemToMemVarLenMacroOp::generateLoop, except that
 * since the length is constant, the header of the loop is different,
 * and we can skip the loop generation if the amount to copy is
 * small enough.
 */
TR::Instruction *
MemToMemConstLenMacroOp::generateLoop()
   {
   //extend support of array size upto uint64_t
   //TR_ASSERT(((uint64_t)INT_MAX*8) > _length, "_length should not exceed the maximum array size in bytes,INT_MAX*8 = 0x%x%x, _length = 0x%x%x",(((uint64_t)INT_MAX*8) >>32), (uint32_t)((uint64_t)INT_MAX*8), (_length >> 32), (uint32_t)_length);
   uint64_t len = (uint64_t)_length;
   TR::Compilation *comp = _cg->comp();

   int64_t largeCopies = (len == 0) ? 0 : (len - 1) / 256;
   TR::Instruction * cursor = (_cursor == NULL ? _cg->getAppendInstruction() : _cursor);

   // if the length is small, just generate one instruction
   if (len <= (uint64_t)256)
      {
      return cursor;
      }


   // if a series of instructions can be done instead of a loop of them, do so, but only if it will not exceed the 4K displacement on XC
   if (largeCopies != 0 && largeCopies < _maxCopies)
      {
      int64_t copies = largeCopies;
      int32_t remaining = 0;

      if (!_srcMR && !_srcReg)
         {
         _srcMR = generateS390MemoryReference(_cg, _rootNode, _srcNode, 0, true);
         cursor = _cg->getAppendInstruction();
         }

      if (!_dstMR && !_dstReg)
         {
          _dstMR = generateS390MemoryReference(_cg, _rootNode, _dstNode, 0, true);
         cursor = _cg->getAppendInstruction();
         }

      int32_t srcMROffset = _srcMR ? _srcMR->getOffset() : 0;
      // the offset may put the displacement beyond 4K

      if (largeCopies * 256 + _offset + srcMROffset >= 4096)
         {
         // this early LA isn't needed for correctness as the later generated SS instructions will enforce the limits but doing
         // this upfront instead saves possibly multiple fixup LAs later on
         if (_srcReg == NULL)
            cursor = genSrcLoadAddress(_offset, cursor);
         else
            cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, _offset, _cg), cursor);
         _offset = 0;
         }

      if (_srcReg == NULL || _dstReg == NULL || _srcReg != _dstReg)
         {
         int32_t dstMROffset = _dstMR ? _dstMR->getOffset() : 0;
         bool dstRegIsATemp = false;
         // _offset only applies when srcReg == dstReg see use in initStg

         if (largeCopies * 256 + dstMROffset >= 4096)
            {
            // this early LA isn't needed for correctness as the later generated SS instructions will enforce the limits but doing
            // this upfront instead saves possibly multiple fixup LAs later on
            if (_dstReg == NULL)
               cursor = genDstLoadAddress(0, cursor);
            else
               cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _dstNode, _dstReg, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 0, _cg), cursor);
            }
         }

      if (_inNestedICF)
         {
         _nestedICFDeps = generateRegisterDependencyConditions(0, 6, _cg);
         if (_srcMR) _nestedICFDeps->addAssignAnyPostCondOnMemRef(_srcMR);
         if (_dstMR) _nestedICFDeps->addAssignAnyPostCondOnMemRef(_dstMR);
         if (_srcReg) _nestedICFDeps->addPostConditionIfNotAlreadyInserted(_srcReg, TR::RealRegister::AssignAny);
         if (_dstReg) _nestedICFDeps->addPostConditionIfNotAlreadyInserted(_dstReg, TR::RealRegister::AssignAny);
         }

      setDependencies(false); //Normal dependencies are definitely not necessary

      while (largeCopies > 0)
         {
         cursor = generateInstruction(_offset + (copies - largeCopies) * 256, 256, cursor);
         --largeCopies;
         }
      len = len - copies * 256;
      _length = (int64_t)len;
      _offset = _offset + copies * 256;
      _cursor = cursor;
      return cursor;
      }


   //If the length is at the THRESHOLD=77825 i.e., 4096*19+1, MVCL becomes a better choice.
   //In general, MVCL shows performance gain over LOOP with MVCs when the length is
   //within [4K*i+1, 4K*i+4089], i=19,20,...,4095.
   //Notice that within the small range [4K*i-7, 4K*i], MVCL is significantly degraded. (3 times slower than normal detected)
   //In order to use MVCL, we have also to make sure that the use is safe. i.e., src and dst are NOT aliased.

   const uint64_t MVCL_THRESHOLD_LOW = 77825;     // MVCL is only considered when the length >= 4096*19+1
   const uint64_t MVCL_THRESHOLD_HIGH = 16777216; // 2^24 is technically the maximum length for an MVCL

   bool inRange = len >= MVCL_THRESHOLD_LOW && len < MVCL_THRESHOLD_HIGH &&
                  (len % 4096) >= (uint64_t)1 && (len % 4096) <= (uint64_t)4089;

   bool aliasingPattern = false;

   TR::SymbolReference * srcSymRef = NULL;
   int32_t srcMemClass = 0;
   uint64_t srcOffset = 0;

   TR::SymbolReference * dstSymRef = NULL;
   int32_t dstMemClass = 0;
   uint64_t dstOffset = 0;


   if (inRange && (aliasingPattern || !_cg->storageMayOverlap(_srcNode, len, _dstNode, len)))
      {
      TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::MVCL;

      if (_dstReg == NULL)
         genDstLoadAddress(0, NULL);
      TR::Register *targetEvenRegister = _dstReg;

      TR::Node *targetAddress = _dstNode;
      TR::Register *targetOddRegister = _cg->allocateRegister();
      generateLoad32BitConstant(_cg, _rootNode, len, targetOddRegister, true);

      if (_srcReg == NULL)
         genSrcLoadAddress(0, NULL);
      TR::Register *sourceEvenRegister = _srcReg;

      TR::Node *sourceAddress = _srcNode;
      TR::Register *sourceOddRegister = _cg->allocateRegister();
      generateLoad32BitConstant(_cg, _rootNode, len, sourceOddRegister, true);

      TR::RegisterPair * sourcePairRegister = _cg->allocateConsecutiveRegisterPair(sourceOddRegister,sourceEvenRegister);
      TR::RegisterPair * targetPairRegister = _cg->allocateConsecutiveRegisterPair(targetOddRegister,targetEvenRegister);

      TR::RegisterDependencyConditions * dependencies = _cg->createDepsForRRMemoryInstructions(_rootNode, sourcePairRegister, targetPairRegister);

      TR::Instruction * cursor = generateRRInstruction(_cg, opCode, _rootNode, targetPairRegister, sourcePairRegister);

      _cg->stopUsingRegister(sourcePairRegister);
      _cg->stopUsingRegister(targetPairRegister);

      _cg->stopUsingRegister(targetOddRegister);
      _cg->stopUsingRegister(sourceOddRegister);

      if (!_inNestedICF)
         cursor->setDependencyConditions(dependencies);
      else
         _nestedICFDeps = dependencies;

      setDependencies(false); //Normal dependencies are definitely not necessary

      _cursor = cursor;
      _length = 0;
      return cursor;
      }

   TR_ASSERT( needsLoop(), "We are generating a loop but needsLoop is false");
   if (_srcReg == NULL)
      cursor = genSrcLoadAddress(0, cursor);
   if (_dstReg == NULL)
      cursor = genDstLoadAddress(0, cursor);
   //
   // At this point, we need to generate a loop since the length is large
   //
   TR::LabelSymbol * topOfLoop = generateLabelSymbol(_cg);
   TR::LabelSymbol * bottomOfLoop = generateLabelSymbol(_cg);

   if (_itersReg == NULL)
      _itersReg = (_tmpReg == NULL ? _cg->allocateRegister() : _tmpReg);

   if (_cg->comp()->target().is64Bit())
      cursor = genLoadLongConstant(_cg, _rootNode, largeCopies, _itersReg, cursor, NULL, NULL);
   else
      cursor = generateLoad32BitConstant(_cg, _rootNode, largeCopies, _itersReg, true, cursor, NULL, NULL);

   _startControlFlow = cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, topOfLoop, cursor);

   cursor = generateInstruction(_offset, 256, cursor);
   cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, 256, _cg), cursor);
   if (_srcReg != _dstReg)
      {
      cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _dstNode, _dstReg, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 256, _cg), cursor);
      }

   cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRCT, _rootNode, _itersReg, topOfLoop, cursor);

   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, bottomOfLoop, cursor);

   len = len - (largeCopies * 256);
   _length = (int64_t)len;
   _cursor = cursor;

   _cg->stopUsingRegister(_itersReg);

   return cursor;
   }

TR::Instruction *
MemToMemConstLenMacroOp::generateRemainder()
   {
   TR::Compilation *comp = _cg->comp();
   uint64_t len = (uint64_t)_length;
   TR::Instruction * cursor = (_cursor == NULL ? _cg->getAppendInstruction() : _cursor);

   if (len >= MemToMemMacroOp::MIN_LENGTH_FOR_SS_INSTRUCTION)
      {
      cursor = generateInstruction(_offset, len, cursor);
      }
   _cursor = cursor;
   return cursor;
   }

/**
 * Very similar to the variable length logic in MemToMemVarLenMacroOp::generateLoop, except that
 * since the length is constant, the header of the loop is different,
 * and we can skip the loop generation if the amount to copy is
 * small enough.
 */
TR::Instruction *
MemInitConstLenMacroOp::generateLoop()
   {
   //extend support of array size upto int64_t, the type of length.
   //TR_ASSERT(((uint64_t)INT_MAX*8) > _length, "_length should not exceed the maximum array size in bytes,INT_MAX*8 = 0x%x%x, _length = 0x%x%x",(((uint64_t)INT_MAX*8) >>32), (uint32_t)((uint64_t)INT_MAX*8), (_length >> 32), (uint32_t)_length);
   uint64_t len = uint64_t(_length);
   TR::Compilation *comp = _cg->comp();

   if (_dstReg!=NULL)
      {
      _dstMR=new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, _offset, _cg);
      }
   else
      {
      _dstMR= generateS390MemoryReference(_cg, _rootNode, _dstNode , _offset, true);
      }

   TR::Instruction * cursor =  _cg->getAppendInstruction();

   if(len >= (uint64_t)1)
      {
      if (_useByteVal)
         cursor = generateSIInstruction(_cg, TR::InstOpCode::MVI, _rootNode, _dstMR, _byteVal, cursor);
      else
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STC, _rootNode, _initReg, _dstMR, cursor);

      --len;
      }

   int64_t largeCopies = (len == 0) ? 0 : (len - 1) / 256;

   // if the length is small, just generate one instruction
   if (len <= (uint64_t)256)
      {
      _length = (int64_t)len;
      setDependencies(false);  // Make sure we do not generate dependencies or internalControlFlow
      return cursor;
      }

   // if a series of instructions can be done instead of a loop of them, do so, but only if it will not exceed the 4K displacement on XC
   if (largeCopies != 0 && largeCopies < _maxCopies)
      {
      int64_t copies = largeCopies;
      int32_t remaining = 0;

      // the offset may put the displacement beyond 4K
      if (largeCopies * 256 + _offset >= 4096)
         {
         cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, _offset, _cg), cursor);
         _offset = 0;
         }

      int32_t local_offset = 0;
      while (largeCopies > 0)
         {
         local_offset = _offset + (copies - largeCopies) * 256;
         cursor = generateSS1Instruction(_cg, TR::InstOpCode::MVC, _rootNode, 255, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, local_offset + 1, _cg),
                     new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, local_offset, _cg), cursor);
         --largeCopies;
         }
      len = len - copies * 256;
      _length = (int64_t)len;
      _offset = _offset + copies * 256;
      _cursor = cursor;
      setDependencies(false);  // Make sure we do not generate dependencies or internalControlFlow
      return cursor;
      }

   //
   // At this point, we need to generate a loop since the length is large
   //
   TR::LabelSymbol * topOfLoop = generateLabelSymbol(_cg);
   TR::LabelSymbol * bottomOfLoop = generateLabelSymbol(_cg);

   if (_itersReg == NULL)
      _itersReg = (_tmpReg == NULL ? _cg->allocateRegister() : _tmpReg);

   if (_cg->comp()->target().is64Bit())
      cursor = genLoadLongConstant(_cg, _rootNode, largeCopies, _itersReg, cursor, NULL, NULL);
   else
      cursor = generateLoad32BitConstant(_cg, _rootNode, largeCopies, _itersReg, true, cursor, NULL, NULL);

   _startControlFlow = cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, topOfLoop, cursor);

   cursor = generateSS1Instruction(_cg, TR::InstOpCode::MVC, _rootNode, 255, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, _offset + 1, _cg),
               new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, _offset, _cg), cursor);
   cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, 256, _cg), cursor);
   if (_srcReg != _dstReg)
      {
      cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _dstNode, _dstReg, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 256, _cg), cursor);
      }

   cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRCT, _rootNode, _itersReg, topOfLoop, cursor);

   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, bottomOfLoop, cursor);

   len = len - (uint64_t)(largeCopies * 256);
   _length = (int64_t)len;
   _cursor = cursor;

   _cg->stopUsingRegister(_itersReg);

   return cursor;
   }

TR::Instruction *
MemInitConstLenMacroOp::generateRemainder()
   {
   TR::Compilation *comp = _cg->comp();
   uint64_t len = (uint64_t)_length;
   TR::Instruction * cursor = (_cursor == NULL ? _cg->getAppendInstruction() : _cursor);

   if (len >= MemToMemMacroOp::MIN_LENGTH_FOR_SS_INSTRUCTION)
      {
      cursor = generateInstruction(_offset, len, cursor);
      }

   _cursor = cursor;
   return cursor;
   }

intptr_t
MemInitVarLenMacroOp::getHelper()
   {
   return (intptr_t) _cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arraySetGeneralHelper, false, false, false)->getMethodAddress();
   }

intptr_t
MemClearVarLenMacroOp::getHelper()
   {
   return (intptr_t) _cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arraySetZeroHelper, false, false, false)->getMethodAddress();
   }

intptr_t
MemCpyVarLenMacroOp::getHelper()
   {
   return (intptr_t) _cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arrayCopyHelper, false, false, false)->getMethodAddress();
   }

intptr_t
MemCmpVarLenMacroOp::getHelper()
   {
   return (intptr_t) _cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arrayCmpHelper, false, false, false)->getMethodAddress();
   }

intptr_t
BitOpMemVarLenMacroOp::getHelper()
   {
   switch(_opcode)
      {
      case TR::InstOpCode::XC:
         return (intptr_t) _cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arrayXORHelper, false, false, false)->getMethodAddress();
      case TR::InstOpCode::NC:
         return (intptr_t) _cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arrayANDHelper, false, false, false)->getMethodAddress();
      case TR::InstOpCode::OC:
         return (intptr_t) _cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arrayORHelper, false, false, false)->getMethodAddress();
      default:
         TR_ASSERT( 0, "not support");
      }
   return 0;
   }

TR::SymbolReference *
MemInitVarLenMacroOp::getHelperSymRef()
   {
   return _cg->getSymRef(TR_S390arraySetGeneralHelper);
   }

TR::SymbolReference *
MemClearVarLenMacroOp::getHelperSymRef()
   {
   return _cg->getSymRef(TR_S390arraySetZeroHelper);
   }

TR::SymbolReference *
MemCpyVarLenMacroOp::getHelperSymRef()
   {
   return _cg->getSymRef(TR_S390arrayCopyHelper);
   }

TR::SymbolReference *
MemCmpVarLenMacroOp::getHelperSymRef()
   {
   return _cg->getSymRef(TR_S390arrayCmpHelper);
   }

TR::SymbolReference *
BitOpMemVarLenMacroOp::getHelperSymRef()
   {
   switch(_opcode)
      {
      case TR::InstOpCode::XC:
         return _cg->getSymRef(TR_S390arrayXORHelper);
      case TR::InstOpCode::NC:
         return _cg->getSymRef(TR_S390arrayANDHelper);
      case TR::InstOpCode::OC:
         return _cg->getSymRef(TR_S390arrayORHelper);
      default:
         TR_ASSERT( 0, "not support");
      }
   return 0;
   }

TR::RegisterDependencyConditions *
MemInitConstLenMacroOp::generateDependencies()
   {
   if(!(_dstReg || _itersReg || _initReg) || !needDependencies())
     return NULL;

   TR::RegisterDependencyConditions * dependencies;

   if (noLoop())
      {
      dependencies = generateRegisterDependencyConditions(0, 2, _cg);
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_initReg) dependencies->addPostCondition(_initReg, TR::RealRegister::AssignAny);
      }
   else
      {
      dependencies = generateRegisterDependencyConditions(0, 3, _cg);
      if (useEXForRemainder())
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
         if (_initReg) dependencies->addPostCondition(_initReg, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
         }
      else
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1, RefsAndDefsDependentRegister);
         if (_initReg) dependencies->addPostCondition(_initReg, TR::RealRegister::GPR2);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
         }
      }
   return dependencies;
   }

TR::RegisterDependencyConditions *
MemClearConstLenMacroOp::generateDependencies()
   {
   if(!(_dstReg || _itersReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies = NULL;

   if (needDependencies())
     {
     if (noLoop())
       {
       dependencies = generateRegisterDependencyConditions(0, 1, _cg);
       if (_dstReg!=NULL) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
       }
     else
       {
       dependencies = generateRegisterDependencyConditions(0, 2, _cg);
       if (useEXForRemainder())
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
         }
       else
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
         }
       }
     }
   return dependencies;
   }

TR::RegisterDependencyConditions *
MemCpyConstLenMacroOp::generateDependencies()
   {
   if(!(_dstReg || _srcReg || _itersReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies = NULL;

   if (needDependencies())
      {
      if (noLoop())
         {
         dependencies = generateRegisterDependencyConditions(0, 2, _cg);
         if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
         if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny);
         }
      else
         {
         dependencies = generateRegisterDependencyConditions(0, 3, _cg);
         if (useEXForRemainder())
            {
            dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
            dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
            dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
            }
         else
            {
            dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1, RefsAndDefsDependentRegister);
            dependencies->addPostCondition(_srcReg, TR::RealRegister::GPR2, RefsAndDefsDependentRegister);
            dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
            }
         }
      if (_inNestedICF)
         {
         TR_ASSERT(!_nestedICFDeps, "needDependencies should not be true if _nestedICFDeps have already been created");
         _nestedICFDeps = dependencies;
         }
      }

   return dependencies;
   }

TR::RegisterDependencyConditions *
BitOpMemConstLenMacroOp::generateDependencies()
   {
   if(!(_dstReg || _srcReg || _itersReg ))
     return NULL;

   TR::RegisterDependencyConditions * dependencies;

   if (noLoop())
      {
      dependencies = generateRegisterDependencyConditions(0, 2, _cg);
      if (_dstReg!=NULL) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_srcReg!=NULL) dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny);
      }
   else
      {
      dependencies = generateRegisterDependencyConditions(0, 3, _cg);
      if (useEXForRemainder())
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
         }
      else
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_srcReg, TR::RealRegister::GPR2, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
         }
      }
   return dependencies;
   }

TR::RegisterDependencyConditions *
MemCmpConstLenMacroOp::generateDependencies()
   {
   if(!(_dstReg || _srcReg || _itersReg || _resultReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies;

   if (noLoop())
      {
      dependencies = generateRegisterDependencyConditions(0, 3, _cg);
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny);
      dependencies->addPostCondition(_resultReg, TR::RealRegister::AssignAny);
      }
   else
      {
      dependencies = generateRegisterDependencyConditions(0, 3, _cg);
      if (useEXForRemainder())
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
         }
      else
         {
         dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_srcReg, TR::RealRegister::GPR2, RefsAndDefsDependentRegister);
         dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
         }
      dependencies->addPostCondition(_resultReg, TR::RealRegister::AssignAny);
      }
   return dependencies;
   }

TR::RegisterDependencyConditions *
MemInitVarLenMacroOp::generateDependencies()
   {
   if(!(_raReg || _dstReg || _srcReg || _initReg || _itersReg || _regLen || _litReg || _litPoolReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies = generateRegisterDependencyConditions(0, 7, _cg);

   if (_raReg) dependencies->addPostCondition(_raReg, _cg->getReturnAddressRegister());
   if (useEXForRemainder())
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_initReg) dependencies->addPostCondition(_initReg, TR::RealRegister::AssignAny);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
      }
   else
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1);
      if (_initReg) dependencies->addPostCondition(_initReg, TR::RealRegister::GPR2);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
      }
   if (_regLen) dependencies->addPostCondition(_regLen, TR::RealRegister::AssignAny);
   if (_litReg) dependencies->addPostCondition(_litReg, TR::RealRegister::AssignAny);
   if (_litPoolReg) dependencies->addPostCondition(_litPoolReg, TR::RealRegister::AssignAny);

   return dependencies;
   }

TR::RegisterDependencyConditions *
MemClearVarLenMacroOp::generateDependencies()
   {
   if(!(_raReg || _dstReg || _itersReg || _regLen || _litReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies = generateRegisterDependencyConditions(0, 5, _cg);

   if (useEXForRemainder())
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
      }
   else
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
      }
   if (_raReg) dependencies->addPostCondition(_raReg, _cg->getReturnAddressRegister());
   if (_regLen) dependencies->addPostCondition(_regLen, TR::RealRegister::AssignAny);
   if (_litReg) dependencies->addPostCondition(_litReg, TR::RealRegister::AssignAny);

   return dependencies;
   }

TR::RegisterDependencyConditions *
MemCpyVarLenMacroOp::generateDependencies()
   {
   if(!(_raReg || _dstReg || _srcReg || _itersReg || _regLen || _litReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies = generateRegisterDependencyConditions(0, 6, _cg);
   if (_raReg) dependencies->addPostCondition(_raReg, _cg->getReturnAddressRegister());
   if (useEXForRemainder())
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
      }
   else
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1);
      if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::GPR2);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
      }
   if (_regLen) dependencies->addPostCondition(_regLen, TR::RealRegister::AssignAny);
   if (_litReg) dependencies->addPostCondition(_litReg, TR::RealRegister::AssignAny);

   return dependencies;
   }

TR::RegisterDependencyConditions *
BitOpMemVarLenMacroOp::generateDependencies()
   {
   if(!(_raReg || _dstReg || _srcReg || _itersReg || _regLen || _litReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies = generateRegisterDependencyConditions(0, 6, _cg);

   if (_raReg) dependencies->addPostCondition(_raReg, _cg->getReturnAddressRegister());
   if (useEXForRemainder())
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
      }
   else
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1);
      if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::GPR2);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
      }
   if (_regLen) dependencies->addPostCondition(_regLen, TR::RealRegister::AssignAny);
   if (_litReg) dependencies->addPostCondition(_litReg, TR::RealRegister::AssignAny);

   return dependencies;
   }

TR::RegisterDependencyConditions *
MemCmpVarLenMacroOp::generateDependencies()
   {
   if(!(_raReg || _dstReg || _srcReg || _itersReg || _regLen || _litReg || _resultReg || _litPoolReg))
     return NULL;

   TR::RegisterDependencyConditions * dependencies = generateRegisterDependencyConditions(0, 8, _cg);

   if (_raReg) dependencies->addPostCondition(_raReg, _cg->getReturnAddressRegister());
   if (useEXForRemainder())
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::AssignAny);
      if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::AssignAny);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::AssignAny);
      }
   else
      {
      if (_dstReg) dependencies->addPostCondition(_dstReg, TR::RealRegister::GPR1);
      if (_srcReg) dependencies->addPostCondition(_srcReg, TR::RealRegister::GPR2);
      if (_itersReg) dependencies->addPostCondition(_itersReg, TR::RealRegister::GPR0);
      }
   if (_resultReg) dependencies->addPostCondition(_resultReg, TR::RealRegister::AssignAny);
   if (_regLen) dependencies->addPostCondition(_regLen, TR::RealRegister::AssignAny);
   if (_litReg) dependencies->addPostCondition(_litReg, TR::RealRegister::AssignAny);
   if (_litPoolReg) dependencies->addPostCondition(_litPoolReg, TR::RealRegister::AssignAny);

   return dependencies;
   }

TR::Instruction *
MemToMemVarLenMacroOp::generateRemainder()
   {
   if (useEXForRemainder())
      {
      TR::Compilation* comp = _cg->comp();

      TR::Instruction* cursor = NULL;

      if (!_cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) || comp->getOption(TR_DisableInlineEXTarget))
         {
         cursor = generateInstruction(0, 1);
         }

      // re use _itersReg for lit pool register, since we don't need it anymore
      // not doing this can cause problems as generateEXDispatch will allocate a new register for lit pool
      // we can't do that because MemToMemVarLenMacroOp uses internal control flow
      if(_srcReg == NULL)  // Make sure base register is added to dependencies
        {
        _srcReg = _srcMR->getBaseRegister();
        if(_srcReg && _srcReg->getRealRegister()) _srcReg = NULL; // Must be something like stack pointer
        }
      if(_dstReg == NULL)  // Make sure base register is added to dependencies
        {
        _dstReg = _dstMR->getBaseRegister();
        if((_dstReg && _dstReg->getRealRegister()) || _dstReg == _srcReg) _dstReg = NULL; // Must be something like stack pointer
        }


      if (_cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && !comp->getOption(TR_DisableInlineEXTarget))
         {
         TR_ASSERT(_EXTargetLabel != NULL, "Assert: EXTarget label must not be NULL");

         _cursor = new (_cg->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::EXRL, _rootNode, _regLen, _EXTargetLabel, _cg);
         }
         else
         {
         if(_itersReg==NULL)
            {
            _itersReg=_cg->allocateRegister();
            cursor = generateEXDispatch(_rootNode, _cg, _regLen, _itersReg, cursor);
            _cg->stopUsingRegister(_itersReg);
            }
         else
            {
            cursor = generateEXDispatch(_rootNode, _cg, _regLen, _itersReg, cursor);
            }

         _cursor = cursor;
         }

      if (_doneLabel) _cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, _doneLabel); // required as main logic branches here
      }
   else
      {
      TR::LabelSymbol *remainderDoneLabel = generateLabelSymbol(_cg);
      if (_cg->comp()->target().is64Bit())
         generateShiftThenKeepSelected64Bit(_rootNode, _cg, _regLen, _regLen, 52, 59, 4);
      else
         generateShiftThenKeepSelected31Bit(_rootNode, _cg, _regLen, _regLen, 20, 27, 4);

      TR::MemoryReference * targetMR = new (_cg->trHeapMemory()) TR::MemoryReference(_raReg, _regLen, 0, _cg);
      generateRXInstruction(_cg, TR::InstOpCode::BAS, _rootNode, _raReg, targetMR);
      _cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, remainderDoneLabel);
      }

   return _cursor;
   }

TR::Instruction *
MemInitVarLenMacroOp::generateRemainder()
   {
   TR::Instruction * cursor = NULL;
   if(!useEXForRemainder())
      {
      return MemToMemVarLenMacroOp::generateRemainder();
      }
   if(checkLengthAfterLoop())
      {
      // can't use generateS390ImmOp as it may generate a temporary register
      // which wouldn't have a dependency
      if(_cg->comp()->target().is64Bit())
         {
         generateRILInstruction(_cg, TR::InstOpCode::NILF, _rootNode, _regLen, 0xFF);
         generateRILInstruction(_cg, TR::InstOpCode::NIHF, _rootNode, _regLen, 0);
         }
      else
         {
         generateRILInstruction(_cg, TR::InstOpCode::NILF, _rootNode, _regLen, 0xFF);
         }

      if (!_firstByteInitialized)
         generateInstruction(0, 1);

      if (!_doneLabel)
         _doneLabel  = generateLabelSymbol(_cg);

      if(_cg->comp()->target().is64Bit())
         generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::CG, _rootNode, _regLen, (int32_t)0, TR::InstOpCode::COND_BNH, _doneLabel, false, false);
      else
         generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _rootNode, _regLen, (int32_t)0, TR::InstOpCode::COND_BNH, _doneLabel, false, false);

      if (_firstByteInitialized)
         generateRIInstruction(_cg, _cg->comp()->target().is64Bit() ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, _rootNode, _regLen, -1);

      TR::Instruction * MVCInstr = generateSS1Instruction(_cg, TR::InstOpCode::MVC, _rootNode, 0,
               new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 1, _cg),
               new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, 0, _cg));

      if (_raReg == NULL)
         {
         _raReg = allocateRegForLiteralPoolBase(_cg);
         cursor = generateEXDispatch(_rootNode, _cg, _regLen, _raReg, MVCInstr);
         if (!useLockedLitPoolBaseRegister(_cg))
            _cg->stopUsingRegister(_raReg);
         }
      else
         {
         cursor = generateEXDispatch(_rootNode, _cg, _regLen, _raReg, MVCInstr);
         }
      }
   else
      {
      //Need to compensate the length as the first byte has been set in generateLoop
      //and check if there is a remainder.
      generateRIInstruction(_cg, _cg->comp()->target().is64Bit() ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, _rootNode, _regLen, -1);

      if (!_doneLabel)
         _doneLabel  = generateLabelSymbol(_cg);

      generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, _rootNode, _doneLabel);

      cursor = generateSS1Instruction(_cg, TR::InstOpCode::MVC, _rootNode, 0,
               new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 1, _cg),
               new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, 0, _cg));

      if (_litPoolReg == NULL)
         {
         _litPoolReg = allocateRegForLiteralPoolBase(_cg);
         cursor = generateEXDispatch(_rootNode, _cg, _regLen, _litPoolReg, cursor);
         if (!useLockedLitPoolBaseRegister(_cg))
            _cg->stopUsingRegister(_litPoolReg);
         }
      else
         {
         cursor = generateEXDispatch(_rootNode, _cg, _regLen, _litPoolReg, cursor);
         }
      }
   _cursor = cursor;
   if (_doneLabel) _cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, _doneLabel); // required as main logic branches here

   return _cursor;
   }

TR::Instruction *
MemClearVarLenMacroOp::generateRemainder()
   {
   TR::Compilation *comp = _cg->comp();
   TR::Instruction * cursor = NULL;
   if(!useEXForRemainder())
      {
      return MemToMemVarLenMacroOp::generateRemainder();
      }

   if(_cg->comp()->target().is64Bit())
      {
      cursor = generateS390ImmOp(_cg, TR::InstOpCode::NG, _rootNode, _regLen, _regLen, (int64_t)0xFF);
      generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::CG, _rootNode, _regLen, (int32_t)0, TR::InstOpCode::COND_BL, _doneLabel, false, false);
      }
   else
      {
      cursor = generateS390ImmOp(_cg, TR::InstOpCode::N, _rootNode, _regLen, _regLen, (int32_t)0xFF);
      generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _rootNode, _regLen, (int32_t)0, TR::InstOpCode::COND_BL, _doneLabel, false, false);
      }

   // Check to see if ImmOp generated a lit mem ref
   TR::MemoryReference *litMemRef=cursor->getMemoryReference();
   if(litMemRef)
     _litReg=litMemRef->getBaseRegister();

   TR::Instruction * XCInstr = generateSS1Instruction(_cg, TR::InstOpCode::XC, _rootNode, 0,
         new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 0, _cg),
         new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, 0, _cg));

   if (_raReg == NULL)
      {
      _raReg = allocateRegForLiteralPoolBase(_cg);
      cursor = generateEXDispatch(_rootNode, _cg, _regLen, _raReg, XCInstr);
      if (!useLockedLitPoolBaseRegister(_cg))
         _cg->stopUsingRegister(_raReg);
      }
   else
      {
      cursor = generateEXDispatch(_rootNode, _cg, _regLen, _raReg, XCInstr);
      }

   _cursor = cursor;
   _cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, _doneLabel); // required as main logic branches here

   return _cursor;
   }

TR::Instruction *
MemInitConstLenMacroOp::generateInstruction(int32_t offset, int64_t length, TR::Instruction * cursor1)
   {
   TR::Compilation *comp = _cg->comp();
   TR::Instruction * cursor = _cg->getAppendInstruction();
   if (length == 0)
      {
      return cursor;
      }

   if (_dstNode==_srcNode)
      {
      _srcMR=generateS390MemoryReference(*_dstMR, offset, _cg);
      }
   else if (_srcReg!=NULL)
      {
      _srcMR=new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, offset, _cg);
      }
   else
      {
      _srcMR= generateS390MemoryReference(_cg, _rootNode, _srcNode , offset, true);
      }

   _dstMR=generateS390MemoryReference(*_dstMR, offset+1, _cg);

   cursor = _cg->getAppendInstruction();

   if (length==1 && !_useByteVal)
      {
      cursor = generateRXInstruction(_cg, TR::InstOpCode::STC, _rootNode, _initReg, _dstMR , cursor);
      }
   else if (length == 1 && _useByteVal)
      {
      cursor = generateSIInstruction(_cg, TR::InstOpCode::MVI, _rootNode, _dstMR, _byteVal, cursor);
      }
   else
      {
      cursor = generateSS1Instruction(_cg, TR::InstOpCode::MVC, _rootNode, length - 1, _dstMR, _srcMR, cursor);
      }

   return cursor;
   }

TR::Instruction *
MemClearConstLenMacroOp::generateInstruction(int32_t offset, int64_t length, TR::Instruction * cursor1)
   {
   TR::MemoryReference * srcMR = _srcMR;
   TR::Compilation *comp = _cg->comp();
   TR::MemoryReference * dstMR = _dstMR;
   TR::Instruction * cursor = _cg->getAppendInstruction();
   bool isAppend = (cursor == cursor1);

   if (srcMR == NULL)
      {
      if (_srcReg != NULL)
         {
         srcMR = new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, offset, _cg);
         }
      else
         {
         srcMR = generateS390MemoryReference(_cg, _rootNode, _srcNode , offset, true);
         }
      }
   else
      {
      srcMR = reuseS390MemoryReference(_srcMR, offset, _rootNode, _cg, true); // enforceSSLimits=true
      }

   if (_dstNode == _srcNode)
      {
      // instruction could be generated during generate memory reference
      cursor = _cg->getAppendInstruction();
      cursor = (cursor1 == NULL ? cursor : (isAppend ? cursor : cursor1));

      TR_ASSERT(_srcMR == _dstMR, "memrefs must match if nodes match on node %p\n", _dstNode);

      // For lengths of 1, 2, 4 and 8, the XC sequence is suboptimal, as they require
      // 2 cycles to execute.  If MVI / MVHHI / MVHI / MVGHI are supported, we should
      // generate those instead.
      if (_cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && length <= 8 && TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(length))
         {
         switch(length)
            {
            case 8:
               cursor = generateSILInstruction(_cg, TR::InstOpCode::MVGHI, _rootNode, srcMR, 0, cursor);
               return cursor;
            case 4:
               cursor = generateSILInstruction(_cg, TR::InstOpCode::MVHI, _rootNode, srcMR, 0, cursor);
               return cursor;
            case 2:
               cursor = generateSILInstruction(_cg, TR::InstOpCode::MVHHI, _rootNode, srcMR, 0, cursor);
               return cursor;
            case 1:
               cursor = generateSIInstruction(_cg, TR::InstOpCode::MVI, _rootNode, srcMR, 0, cursor);
               return cursor;
            }
         }
      else if (length == 1)
         {
         cursor = generateSIInstruction(_cg, TR::InstOpCode::MVI, _rootNode, srcMR, 0, cursor);
         return cursor;
         }
      dstMR = generateS390MemoryReference(*srcMR, 0, _cg);
      }
   else if (_dstReg != NULL)
      {
      dstMR = new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, offset, _cg);
      }
   else if (dstMR == NULL)
      {
      dstMR = generateS390MemoryReference(_cg, _rootNode, _dstNode, offset, true);
      }
   else
      {
      dstMR = generateS390MemoryReference(*dstMR, 0, _cg);
      }

   // instruction could be generated during generate memory reference
   cursor = _cg->getAppendInstruction();
   cursor = (cursor1 == NULL ? cursor : (isAppend ? cursor : cursor1));

   cursor = generateSS1Instruction(_cg, TR::InstOpCode::XC, _rootNode, length - 1, dstMR, srcMR, cursor);

   return cursor;
   }

TR::Instruction *
MemToMemConstLenMacroOp::generateInstruction(int32_t offset, int64_t length, TR::Instruction * cursor1)
   {
   TR::Compilation *comp = _cg->comp();
   TR_ASSERT(_opcode != TR::InstOpCode::BAD,"no opcode set for MemToMemConstLenMacroOp node %p\n",_rootNode);
   TR::Instruction * cursor=NULL;
   TR::MemoryReference * srcMR = _srcMR;
   TR::MemoryReference * dstMR = _dstMR;

   if (srcMR == NULL)
      {
      if (_srcReg!=NULL)
         srcMR=new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, offset, _cg);
      else
         srcMR= generateS390MemoryReference(_cg, _rootNode, _srcNode , offset, true);
      _srcMR = srcMR;
      }
   else
      {
      srcMR = reuseS390MemoryReference(srcMR, offset, _rootNode, _cg, true); // enforceSSLimits=true
      }

   if (dstMR == NULL)
      {
      if (_dstReg!=NULL)
         dstMR=new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, offset, _cg);
      else
         dstMR= generateS390MemoryReference(_cg, _rootNode, _dstNode, offset, true);
      _dstMR = dstMR;
      }
   else
      {
      dstMR = reuseS390MemoryReference(dstMR, offset, _rootNode, _cg, true); // enforceSSLimits=true
      }

   cursor = _cg->getAppendInstruction();

   cursor = generateSS1Instruction(_cg, _opcode, _rootNode, length - 1, dstMR, srcMR, cursor);

   return cursor;
   }

TR::Instruction *
MemCmpConstLenMacroOp::generateInstruction(int32_t offset, int64_t length, TR::Instruction * cursor1)
   {
   TR::Compilation *comp = _cg->comp();
   TR::Instruction * cursor=NULL;
   TR::MemoryReference * srcMR;
   TR::MemoryReference * dstMR;

   if (_srcReg!=NULL)
      {
      srcMR=new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, offset, _cg);
      }
   else
      {
      srcMR= generateS390MemoryReference(_cg, _rootNode, _srcNode , offset, true);
      }

   if (_dstReg!=NULL)
      {
      dstMR=new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, offset, _cg);
      }
   else
      {
      dstMR= generateS390MemoryReference(_cg, _rootNode, _dstNode, offset, true);
      }

   cursor = _cg->getAppendInstruction();
   cursor = generateSS1Instruction(_cg, TR::InstOpCode::CLC, _rootNode, length - 1, dstMR, srcMR, cursor);

   if (!inRemainder())
      {
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, _rootNode, _falseLabel);
      }

   return cursor;
   }

TR::Instruction *
MemInitVarLenMacroOp::generateInstruction(int32_t offset, int64_t length)
   {
   TR::Compilation *comp = _cg->comp();
   TR::Instruction * cursor = _cg->getAppendInstruction();
   if (length == 0)
      {
      return cursor;
      }

   if (!_firstByteInitialized)
      {
      if (_useByteVal)
         cursor = generateSIInstruction(_cg, TR::InstOpCode::MVI, _rootNode, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, offset, _cg), _byteVal);
      else
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STC, _rootNode, _initReg, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, offset, _cg));

     _firstByteInitialized=true;
    length--;
    }

   if (length > 0)
      {
      cursor = generateSS1Instruction(_cg, TR::InstOpCode::MVC, _rootNode, length - 1, new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, 1 + offset, _cg),
                  new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, offset, _cg), cursor);
      }

   return cursor;
   }

TR::Instruction *
MemClearVarLenMacroOp::generateInstruction(int32_t offset, int64_t length)
   {
   TR::Compilation *comp = _cg->comp();
   TR::MemoryReference * srcMR = NULL;
   TR::MemoryReference * dstMR = NULL;
   TR::Instruction * cursor = _cg->getAppendInstruction();

   if (_srcReg != NULL)
      {
      srcMR = new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, offset, _cg);
      }
   else
      {
      srcMR = generateS390MemoryReference(_cg, _rootNode, _srcNode , offset, true);
      }
   if (_dstNode == _srcNode)
      {
      dstMR = generateS390MemoryReference(*srcMR, 0, _cg);
      }
   else if (_dstReg != NULL)
      {
      dstMR = new (_cg->trHeapMemory()) TR::MemoryReference(_dstReg, offset, _cg);
      }
   else
      {
      dstMR = generateS390MemoryReference(_cg, _rootNode, _dstNode, offset, true);
      }

   // instruction could be generated during generate memory reference
   cursor = _cg->getAppendInstruction();

   cursor = generateSS1Instruction(_cg, TR::InstOpCode::XC, _rootNode, length - 1, dstMR, srcMR, cursor);

   return cursor;
   }

TR::Instruction *
MemCpyVarLenMacroOp::generateInstruction(int32_t offset, int64_t length)
   {
   TR::Instruction * cursor=NULL;
   TR::Compilation *comp = _cg->comp();

   generateSrcMemRef(offset);
   generateDstMemRef(offset);

   cursor = _cg->getAppendInstruction();
   cursor = generateSS1Instruction(_cg, TR::InstOpCode::MVC, _rootNode, length - 1, _dstMR, _srcMR, cursor);

   return cursor;
   }

TR::Instruction *
BitOpMemVarLenMacroOp::generateInstruction(int32_t offset, int64_t length)
   {
   TR::Instruction * cursor=NULL;
   TR::Compilation *comp = _cg->comp();

   generateSrcMemRef(offset);
   generateDstMemRef(offset);

   cursor = _cg->getAppendInstruction();
   cursor = generateSS1Instruction(_cg, _opcode, _rootNode, length - 1, _dstMR, _srcMR, cursor);

   return cursor;

   }

TR::Instruction *
MemCmpVarLenMacroOp::generateInstruction(int32_t offset, int64_t length)
   {

   TR::Instruction * cursor=NULL;

   generateSrcMemRef(offset);
   generateDstMemRef(offset);
   TR::Compilation *comp = _cg->comp();

   cursor = _cg->getAppendInstruction();
   cursor = generateSS1Instruction(_cg, TR::InstOpCode::CLC, _rootNode, length - 1, _dstMR, _srcMR, cursor);

   if (!useEXForRemainder() || !inRemainder())
      {
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, _rootNode, _falseLabel);
      }

   return cursor;
   }


static TR::Instruction *
generateCmpResult(TR::CodeGenerator * cg, TR::Node * rootNode, TR::Register * dstReg, TR::Register * srcReg, TR::Register * resultReg,
   TR::LabelSymbol * falseLabel, TR::LabelSymbol * trueLabel, TR::LabelSymbol * doneLabel)
   {
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, falseLabel);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, trueLabel);
   getConditionCode(rootNode, cg, resultReg);

   TR::Instruction * cursor;
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, doneLabel);

   return cursor;
   }

TR::Instruction *
MemCmpVarLenMacroOp::generate(TR::Register* dstReg, TR::Register* srcReg, TR::Register* tmpReg, int32_t offset, TR::Instruction *cursor)
   {
   TR::Instruction *cursorBefore = cursor;
   _resultReg = _cg->allocateRegister();
   _dstReg = dstReg;
   _srcReg = srcReg;
   _tmpReg = tmpReg;
   _offset = offset;
   _cursor = cursor;
   _litReg = NULL;
   TR::Compilation *comp = _cg->comp();

   if(cursorBefore == NULL) cursorBefore = _cg->getAppendInstruction();
   generateLoop();
   setInRemainder(true);
   generateRemainder();

   _cursor = generateCmpResult(_cg, _rootNode, _dstReg, _srcReg, _resultReg, _falseLabel, _trueLabel, _doneLabel);
   TR::RegisterDependencyConditions * dependencies = generateDependencies();
   _cursor->setDependencyConditions(dependencies);
   if(_startControlFlow==NULL)
     {
     _startControlFlow=cursorBefore->getNext();
     if(_startControlFlow->getOpCodeValue() == TR::InstOpCode::ASSOCREGS) _startControlFlow=_startControlFlow->getNext();
     }
   if(_startControlFlow != _cursor)
     {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(_cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(_cg);

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionStart, dependencies, _startControlFlow->getPrev());
      cFlowRegionStart->setStartInternalControlFlow();

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionEnd, _cursor->getPrev());
      cFlowRegionEnd->setEndInternalControlFlow();
     }
   return _cursor;
   }

TR::Instruction *
MemCmpConstLenMacroOp::generate(TR::Register* dstReg, TR::Register* srcReg, TR::Register* tmpReg, int32_t offset, TR::Instruction *cursor)
   {
   TR::Instruction *cursorBefore = cursor;
   _resultReg = _cg->allocateRegister();
   _dstReg = dstReg;
   _srcReg = srcReg;
   _tmpReg = tmpReg;
   _offset = offset;
   _cursor = cursor;
   _litReg = NULL;
   TR::Compilation *comp = _cg->comp();


   if(cursorBefore == NULL) cursorBefore = _cg->getAppendInstruction();
   generateLoop();
   setInRemainder(true);
   generateRemainder();

   _cursor = generateCmpResult(_cg, _rootNode, _dstReg, _srcReg, _resultReg, _falseLabel, _trueLabel, _doneLabel);

   TR::RegisterDependencyConditions * dependencies = generateDependencies();
   _cursor->setDependencyConditions(dependencies);
   if(_startControlFlow==NULL)
     {
     _startControlFlow=cursorBefore->getNext();
     if(_startControlFlow->getOpCodeValue() == TR::InstOpCode::ASSOCREGS) _startControlFlow=_startControlFlow->getNext();
     }
   if(_startControlFlow != _cursor)
     {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(_cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(_cg);

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionStart, dependencies, _startControlFlow->getPrev());
      cFlowRegionStart->setStartInternalControlFlow();

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionEnd, _cursor->getPrev());
      cFlowRegionEnd->setEndInternalControlFlow();
     }

   return _cursor;
   }

#define USE_IPM_FORARRAYCMPSIGN 1
static TR::Instruction *
generateCmpSignResult(TR::CodeGenerator * cg, TR::Node * rootNode, TR::Register * dstReg, TR::Register * srcReg, TR::Register * resultReg,
   TR::LabelSymbol * falseLabel, TR::LabelSymbol * gtLabel, TR::LabelSymbol * trueLabel, TR::LabelSymbol * doneLabel)
   {
#if USE_IPM_FORARRAYCMPSIGN
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, falseLabel);
   generateRRInstruction(cg, TR::InstOpCode::IPM, rootNode, resultReg, resultReg);
   if (cg->comp()->target().is64Bit())
      {
      generateRSInstruction(cg, TR::InstOpCode::SLLG, rootNode, resultReg, resultReg, 34);
      generateRSInstruction(cg, TR::InstOpCode::SRAG, rootNode, resultReg, resultReg, 64-2);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SLL, rootNode, resultReg, 34-32);
      generateRSInstruction(cg, TR::InstOpCode::SRA, rootNode, resultReg, (64-2)-32);
      }
   //
   // The reason why we swap dstReg and srcReg is to reduce the LCR(G) instruction here.
   // If we don't swap them, we have to generate "LCR resultReg, resultReg".
   //
#else
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, rootNode, falseLabel);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, trueLabel);
   generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), rootNode, resultReg, resultReg);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, rootNode, doneLabel);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, falseLabel);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, rootNode, gtLabel);

   generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), rootNode, resultReg, -1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, rootNode, doneLabel);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, gtLabel);
   generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), rootNode, resultReg, 1);
#endif

   TR::Instruction * cursor;
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, rootNode, doneLabel);

   return cursor;
   }

TR::Instruction *
MemCmpVarLenSignMacroOp::generate(TR::Register* dstReg, TR::Register* srcReg, TR::Register* tmpReg, int32_t offset, TR::Instruction *cursor)
   {
   TR::Instruction *cursorBefore = cursor;
   _resultReg = _cg->allocateRegister();
   TR::Compilation *comp = _cg->comp();
   if (USE_IPM_FORARRAYCMPSIGN)
      {
      _dstReg = srcReg; // Please see generateCmpSignResult in more details
      _srcReg = dstReg; // Please see generateCmpSignResult in more details
      }
   else
      {
      _dstReg = dstReg;
      _srcReg = srcReg;
      }
   _tmpReg = tmpReg;
   _offset = offset;
   _cursor = cursor;
   _litReg = NULL;

   if(cursorBefore == NULL) cursorBefore = _cg->getAppendInstruction();
   generateLoop();
   setInRemainder(true);
   generateRemainder();

   _cursor = generateCmpSignResult(_cg, _rootNode, _dstReg, _srcReg, _resultReg, _falseLabel, _gtLabel, _trueLabel, _doneLabel);
   TR::RegisterDependencyConditions * dependencies = generateDependencies();
   _cursor->setDependencyConditions(dependencies);
   if(_startControlFlow==NULL)
     {
     _startControlFlow=cursorBefore->getNext();
     if(_startControlFlow->getOpCodeValue() == TR::InstOpCode::ASSOCREGS) _startControlFlow=_startControlFlow->getNext();
     }
   if(_startControlFlow != _cursor)
     {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(_cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(_cg);

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionStart, dependencies, _startControlFlow->getPrev());
      cFlowRegionStart->setStartInternalControlFlow();

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionEnd, _cursor->getPrev());
      cFlowRegionEnd->setEndInternalControlFlow();
     }

   return _cursor;
   }

TR::Instruction *
MemCmpConstLenSignMacroOp::generate(TR::Register* dstReg, TR::Register* srcReg, TR::Register* tmpReg, int32_t offset, TR::Instruction *cursor)
   {
   TR::Instruction *cursorBefore = cursor;
   _resultReg = _cg->allocateRegister();
   if (USE_IPM_FORARRAYCMPSIGN)
      {
      _dstReg = srcReg; // Please see generateCmpSignResult in more details
      _srcReg = dstReg; // Please see generateCmpSignResult in more details
      }
   else
      {
      _dstReg = dstReg;
      _srcReg = srcReg;
      }
   _tmpReg = tmpReg;
   _offset = offset;
   _cursor = cursor;
   _litReg = NULL;
   TR::Compilation *comp = _cg->comp();

   if(cursorBefore == NULL) cursorBefore = _cg->getAppendInstruction();
   generateLoop();
   setInRemainder(true);
   generateRemainder();

   _cursor = generateCmpSignResult(_cg, _rootNode, _dstReg, _srcReg, _resultReg, _falseLabel, _gtLabel, _trueLabel, _doneLabel);
   TR::RegisterDependencyConditions * dependencies = generateDependencies();
   _cursor->setDependencyConditions(dependencies);
   if(_startControlFlow==NULL)
     {
     _startControlFlow=cursorBefore->getNext();
     if(_startControlFlow->getOpCodeValue() == TR::InstOpCode::ASSOCREGS) _startControlFlow=_startControlFlow->getNext();
     }
   if(_startControlFlow != _cursor)
     {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(_cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(_cg);

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionStart, dependencies, _startControlFlow->getPrev());
      cFlowRegionStart->setStartInternalControlFlow();

      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, cFlowRegionEnd, _cursor->getPrev());
      cFlowRegionEnd->setEndInternalControlFlow();
     }

   return _cursor;
   }

int32_t
MemToMemTypedVarLenMacroOp::shiftSize()
   {
   switch (_destType)
      {
      case TR::Int8:
         return 0;
      case TR::Int16:
         return 1;
      case TR::Float:
      case TR::Int32:
         return 2;
      case TR::Double:
      case TR::Int64:
         return 3;
         break;
      default:
         if (_cg->comp()->target().is64Bit())
            return 3;
         else
            return 2;
         break;
      }
   return 0; // can not get here but keep compilers happy
   }


int32_t
MemToMemTypedVarLenMacroOp::strideSize()
   {
   TR::Compilation *comp = _cg->comp();
   switch (_destType)
      {
      case TR::Int8:
         return 1;
      case TR::Int16:
         return 2;
      case TR::Float:
      case TR::Int32:
         return 4;
      case TR::Double:
      case TR::Int64:
         return 8;
         break;
      default:
         if (comp->target().is64Bit() && !comp->useCompressedPointers())
            return 8;
         else
            return 4;
         break;
      }
   return 0; // can not get here but keep compilers happy
   }

TR::Instruction *
MemToMemTypedVarLenMacroOp::generateLoop()
   {
   TR::Instruction * cursor;

   // Skip the loop if length is zero.
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(_cg);
   TR::LabelSymbol * doneLoop = generateLabelSymbol(_cg);

   generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::getCmpOpCode(), _dstNode, _lenReg, (int32_t)0, TR::InstOpCode::COND_BNH, doneLoop, false, false);

   if (_isForward)
      {

      generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _dstNode, _endReg, _startReg);
      generateRIInstruction(_cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), _dstNode, _strideReg, strideSize());

      // Set _startReg to the last element to be copied (i.e. start + length - stride)
      generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _startReg, -1 * strideSize());
      generateRRInstruction(_cg, TR::InstOpCode::getAddRegWidenOpCode(), _dstNode, _startReg, _lenReg);

      TR::LabelSymbol * topOfLoop = generateLabelSymbol(_cg);
      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, topOfLoop);

      generateInstruction();

      if (_srcReg != _startReg)
         generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, strideSize(), _cg));

      generateS390BranchInstruction(_cg, TR::InstOpCode::getBranchRelIndexEqOrLowOpCode(), _dstNode, _bxhReg, _endReg, topOfLoop);
      }
   else
      {
      // Adjust the end point to be one element before the end
      // and adjust the start to be one element earlier to account for
      // the instruction being BXH not bxh
      // If the srcReg is separate from destination, then adjust it as well.

      generateRIInstruction(_cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), _dstNode, _strideReg, -1 * strideSize());

      generateRRInstruction(_cg, TR::InstOpCode::getAddRegOpCode(), _dstNode, _startReg, _strideReg);

      generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _dstNode, _endReg, _startReg);

      generateRRInstruction(_cg, TR::InstOpCode::getAddRegWidenOpCode(), _dstNode, _endReg, _lenReg);

      if (_srcReg != _startReg)
         {
         generateRRInstruction(_cg, TR::InstOpCode::getAddRegWidenOpCode(), _srcNode, _srcReg, _lenReg);
         generateRRInstruction(_cg, TR::InstOpCode::getAddRegWidenOpCode(), _srcNode, _srcReg, _strideReg);
         }

      TR::LabelSymbol * topOfLoop = generateLabelSymbol(_cg);
      generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, topOfLoop);

      generateInstruction();

      if (_srcReg != _startReg)
         {
         generateRXInstruction(_cg, TR::InstOpCode::LAY, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, -1 * strideSize(), _cg));
         }

      // _dstReg is decremented as part of BRXH
      generateS390BranchInstruction(_cg, TR::InstOpCode::getBranchRelIndexHighOpCode(), _dstNode, _bxhReg, _endReg, topOfLoop);
      }
   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, doneLoop);

   createLoopDependencies(cursor);
   doneLoop->setEndInternalControlFlow();

   return cursor;
   }

int32_t
MemToMemTypedVarLenMacroOp::numCoreDependencies()
   {
   if (_srcReg != _startReg)
      {
      return 6; // this must match with addCoreDependencies below
      }
   else
      {
      return 5;
      }
   }

TR::RegisterDependencyConditions *
MemToMemTypedVarLenMacroOp::addCoreDependencies(TR::RegisterDependencyConditions * loopDep)
   {
   loopDep->addPreCondition(_strideReg, TR::RealRegister::LegalEvenOfPair);
   loopDep->addPreCondition(_startReg, TR::RealRegister::LegalOddOfPair);
   loopDep->addPreCondition(_bxhReg, TR::RealRegister::EvenOddPair);
   loopDep->addPreCondition(_endReg, TR::RealRegister::AssignAny);
   loopDep->addPreCondition(_lenReg, TR::RealRegister::AssignAny);
   loopDep->addPostCondition(_strideReg, TR::RealRegister::LegalEvenOfPair);
   loopDep->addPostCondition(_startReg, TR::RealRegister::LegalOddOfPair);
   loopDep->addPostCondition(_bxhReg, TR::RealRegister::EvenOddPair);
   loopDep->addPostCondition(_endReg, TR::RealRegister::AssignAny);
   loopDep->addPostCondition(_lenReg, TR::RealRegister::AssignAny);
   if (_srcReg != _startReg)
      {
      loopDep->addPreCondition(_srcReg, TR::RealRegister::AssignAny);
      loopDep->addPostCondition(_srcReg, TR::RealRegister::AssignAny);
      }

   return loopDep;
   }

TR::Instruction *
MemInitVarLenTypedMacroOp::generateInstruction()
   {
   TR::Instruction * cursor;

   TR::MemoryReference * newMR = generateS390MemoryReference(_endReg, (int32_t) 0, _cg);

   switch (_destType)
      {
      case TR::Int16:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STH, _dstNode, _initReg, newMR);
         break;
      case TR::Int32:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::ST, _dstNode, _initReg, newMR);
         break;
      case TR::Int64:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, _dstNode, _initReg, newMR);
         break;
      case TR::Float:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STE, _dstNode, _initReg, newMR);
         break;
      case TR::Double:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STD, _dstNode, _initReg, newMR);
         break;
      default:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, _dstNode, _initReg, newMR);
         break;
      }
   newMR->stopUsingMemRefRegister(_cg);
   return cursor;
   }

void
MemInitVarLenTypedMacroOp::createLoopDependencies(TR::Instruction * cursor)
   {
   int32_t core = numCoreDependencies();
   TR::RegisterDependencyConditions * loopDep = new (_cg->trHeapMemory()) TR::RegisterDependencyConditions(core, core + 1, _cg);
   loopDep->addPostCondition(_initReg, TR::RealRegister::AssignAny);

   addCoreDependencies(loopDep);
   if (_applyDepLocally)
      {
      cursor->setDependencyConditions(loopDep);
      }
   else
      {
      _macroDependencies = loopDep;
      }
   }

TR::Instruction *
MemCpyVarLenTypedMacroOp::generateInstruction()
   {
   TR::Instruction * cursor;
   TR::Compilation *comp = _cg->comp();
   TR::MemoryReference * dstMR = generateS390MemoryReference(_endReg, (int32_t) 0, _cg);
   TR::MemoryReference * srcMR = generateS390MemoryReference(_srcReg, (int32_t) 0, _cg);

   switch (_destType)
      {
      case TR::Int8:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::IC, _srcNode, _workReg, srcMR);
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STC, _dstNode, _workReg, dstMR);
         break;
      case TR::Int16:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::LH, _srcNode, _workReg, srcMR);
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STH, _dstNode, _workReg, dstMR);
         break;
      case TR::Int32:
      case TR::Float:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::L, _srcNode, _workReg, srcMR);
         cursor = generateRXInstruction(_cg, TR::InstOpCode::ST, _dstNode, _workReg, dstMR);
         break;
      case TR::Int64:
      case TR::Double:
         cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, _srcNode, _workReg, srcMR);
         cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, _dstNode, _workReg, dstMR);
         break;
      case TR::Address:
         if (_cg->comp()->target().is64Bit() && !comp->useCompressedPointers())
            {
            if (_needsGuardedLoad)
               {
               cursor = generateRXInstruction(_cg, TR::InstOpCode::LGG, _srcNode, _workReg, srcMR);
               }
            else
               {
               cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, _srcNode, _workReg, srcMR);
               }
            cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, _dstNode, _workReg, dstMR);
            }
         else
            {
            if (_needsGuardedLoad)
               {
               int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
               cursor = generateRXInstruction(_cg, TR::InstOpCode::LLGFSG, _srcNode, _workReg, srcMR);
               if (shiftAmount != 0)
                  {
                  cursor = generateRSInstruction(_cg, TR::InstOpCode::SRLG, _srcNode, _workReg, _workReg, shiftAmount);
                  }
               }
            else
               {
               cursor = generateRXInstruction(_cg, TR::InstOpCode::L, _srcNode, _workReg, srcMR);
               }
            cursor = generateRXInstruction(_cg, TR::InstOpCode::ST, _dstNode, _workReg, dstMR);
            }
         break;
      default:
         TR_ASSERT_FATAL(false, "_destType of invalid type\n");
         break;
      }
   dstMR->stopUsingMemRefRegister(_cg);
   srcMR->stopUsingMemRefRegister(_cg);
   return cursor;
   }

void
MemCpyVarLenTypedMacroOp::allocWorkReg()
   {
   _workReg = _cg->allocateRegister();
   }

void
MemCpyVarLenTypedMacroOp::createLoopDependencies(TR::Instruction * cursor)
   {
   int32_t core = numCoreDependencies();
   TR::RegisterDependencyConditions * loopDep = new (_cg->trHeapMemory()) TR::RegisterDependencyConditions(core, core + 1, _cg);
   loopDep->addPostCondition(_workReg, TR::RealRegister::AssignAny);

   addCoreDependencies(loopDep);
   if (_applyDepLocally)
      {
      cursor->setDependencyConditions(loopDep);
      }
   else
      {
      _macroDependencies = loopDep;
      }
   }

TR::Instruction * MemCpyAtomicMacroOp::generateConstLoop(TR::InstOpCode::Mnemonic loadOp, TR::InstOpCode::Mnemonic storeOp)
   {
   // generate a matching load/op combo for each element

   int32_t offset = (!_isForward) ? -1 * strideSize() : 0;
   TR::MemoryReference * dstMR = generateS390MemoryReference(_endReg, (int32_t) offset, _cg);
   TR::MemoryReference * srcMR = generateS390MemoryReference(_srcReg, (int32_t) offset, _cg);
   TR::Instruction * cursor;

   for (int i = 0; i < _constLength / strideSize(); i++)
      {
      cursor = generateRXInstruction(_cg, loadOp, _srcNode, _workReg, srcMR);
      cursor = generateRXInstruction(_cg, storeOp, _dstNode, _workReg, dstMR);
      dstMR = generateS390MemoryReference(*dstMR, (!_isForward) ? -1 * strideSize() : strideSize(), _cg);
      srcMR = generateS390MemoryReference(*srcMR, (!_isForward) ? -1 * strideSize() : strideSize(), _cg);
      }

   return cursor;
   }

TR::Instruction * MemCpyAtomicMacroOp::generateSTXLoop(int32_t strideSize, TR::InstOpCode::Mnemonic loadOp, TR::InstOpCode::Mnemonic storeOp, bool unroll)
   {
   TR::Compilation *comp = _cg->comp();
   if (_trace)
      traceMsg(comp, "MemCpyAtomicMacroOp: generateSTX\n");

   TR::Instruction * cursor;
   // update _startReg to _endReg
   cursor = generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _dstNode, _startReg, _endReg);

   // if unroll is false, or if backwards arraycopy, set unroll factor to 1 then restore at end
   int32_t savedUnrollFactor;
   if (!unroll || !_isForward)
      {
      savedUnrollFactor = _unrollFactor;
      _unrollFactor = 1;
      }

   if (!_isForward)
      {
      // Set _startReg to the last element to be copied (index -1 of array) (i.e. start -  length - stride)
      // Initially decrement srcreg and endreg by stride size
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _srcReg, -1 * strideSize);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _endReg, -1 * strideSize);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _startReg, -1 * strideSize);
      cursor = generateRRInstruction(_cg, TR::InstOpCode::getSubstractRegOpCode(), _dstNode, _startReg, _lenReg);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), _dstNode, _strideReg, -1 * strideSize);
      }
   else
      {
      // Set _startReg to the last element to be copied (i.e. start + length - stride)
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _startReg, -1 * _unrollFactor * strideSize);
      cursor = generateRRInstruction(_cg, TR::InstOpCode::getAddRegOpCode(), _dstNode, _startReg, _lenReg);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), _dstNode, _strideReg, _unrollFactor * strideSize);
      }

   TR::LabelSymbol * topOfLoop = generateLabelSymbol(_cg);
   TR::LabelSymbol * endOfLoop = generateLabelSymbol(_cg);
   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, topOfLoop);

   cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::getCmpOpCode(), _lenNode, _lenReg, strideSize * _unrollFactor, TR::InstOpCode::COND_BL, endOfLoop);

   cursor = generateInstruction(loadOp, storeOp, _workReg, (int32_t) 0);

   if (_unrollFactor >= 2)
      cursor = generateInstruction(loadOp, storeOp, _workReg2, (int32_t) strideSize);
   if (_unrollFactor >= 3)
      cursor = generateInstruction(loadOp, storeOp, _workReg3, (int32_t) strideSize * 2);
   if (_unrollFactor >= 4)
      cursor = generateInstruction(loadOp, storeOp, _workReg4, (int32_t) strideSize * 3);
   if (_unrollFactor >= 5)
      cursor = generateInstruction(loadOp, storeOp, _workReg5, (int32_t) strideSize * 4);
   if (_unrollFactor >= 6)
      cursor = generateInstruction(loadOp, storeOp, _workReg6, (int32_t) strideSize * 5);
   if (_unrollFactor >= 7)
      cursor = generateInstruction(loadOp, storeOp, _workReg7, (int32_t) strideSize * 6);
   if (_unrollFactor >= 8)
      cursor = generateInstruction(loadOp, storeOp, _workReg8, (int32_t) strideSize * 7);

   if (_srcReg != _startReg)
      {
      if (!_isForward)
         {
         cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, -1 * _unrollFactor * strideSize, _cg));
         }
      else
         {
         cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, _unrollFactor * strideSize, _cg));
         }
      }

   cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _lenNode, _lenReg, new (_cg->trHeapMemory()) TR::MemoryReference(_lenReg, -1 * _unrollFactor * strideSize, _cg));

   if (!_isForward)
      {
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::getBranchRelIndexHighOpCode(), _dstNode, _bxhReg, _endReg, topOfLoop);
      }
   else
      {
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::getBranchRelIndexEqOrLowOpCode(), _dstNode, _bxhReg, _endReg, topOfLoop);
      }

   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, endOfLoop);

   // Compare stride reg to stridesize, should always be true, sets the condition code to branch to doneCopy later
   cursor = generateRIInstruction(_cg, TR::InstOpCode::CHI, _dstNode, _strideReg, _unrollFactor * strideSize);

   // restore unroll factor
   if (!unroll)
      {
      _unrollFactor = savedUnrollFactor;
      }

   return cursor;
   }

TR::Instruction *
MemCpyAtomicMacroOp::generateSTXLoopLabel(TR::LabelSymbol * oolStartLabel, TR::LabelSymbol * doneCopyLabel, int32_t strideSize, TR::InstOpCode::Mnemonic loadOp, TR::InstOpCode::Mnemonic storeOp)
   {
   TR::Compilation *comp = _cg->comp();
   if (_trace)
      traceMsg(comp, "MemCpyAtomicMacroOp: generateSTXLoopLabel\n");
   TR::Instruction * cursor;

   TR_S390OutOfLineCodeSection *oolPath = new (_cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolStartLabel, doneCopyLabel, _cg);
   _cg->getS390OutOfLineCodeSectionList().push_front(oolPath);
   oolPath->swapInstructionListsWithCompilation();

   // Label to OOL
   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _srcNode, oolStartLabel);

   cursor = generateSTXLoop(strideSize, loadOp, storeOp, _unroll);

   // Branch to end of arraycopy
   cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, _srcNode, doneCopyLabel);

   oolPath->swapInstructionListsWithCompilation();

   return cursor;
   }

TR::Instruction *
MemCpyAtomicMacroOp::generateOneSTXthenSTYLoopLabel(TR::LabelSymbol * oolStartLabel, TR::LabelSymbol * doneCopyLabel, int32_t strideSize1, TR::InstOpCode::Mnemonic loadOp1, TR::InstOpCode::Mnemonic storeOp1,
      int32_t strideSize2, TR::InstOpCode::Mnemonic loadOp2, TR::InstOpCode::Mnemonic storeOp2)
   {
   TR::Compilation *comp = _cg->comp();
   if (_trace)
      traceMsg(comp, "MemCpyAtomicMacroOp: generateOneSTXthenSTYLoopLabel\n");
   TR::Instruction * cursor;

   TR_S390OutOfLineCodeSection *oolPath = new (_cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolStartLabel, doneCopyLabel, _cg);
   _cg->getS390OutOfLineCodeSectionList().push_front(oolPath);
   oolPath->swapInstructionListsWithCompilation();

   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, oolStartLabel);

   TR::LabelSymbol * skipRoutineLabel = generateLabelSymbol(_cg);
   cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::getCmpOpCode(), _lenNode, _lenReg, strideSize1, TR::InstOpCode::COND_BL, skipRoutineLabel);

   // Initialize _startReg to _endReg here
   cursor = generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _dstNode, _startReg, _endReg);

   // Aligned loops disabled for backwards array copy
   if (false && !_isForward)
      {
      // Set _startReg to the last element to be copied (index 0 of array) (i.e. start -  length)
      // Initially decrement srcreg and endreg by stride size
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _srcReg, -1 * strideSize1);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _endReg, -1 * strideSize1);
      cursor = generateRRInstruction(_cg, TR::InstOpCode::getSubstractRegOpCode(), _dstNode, _startReg, _lenReg);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), _dstNode, _strideReg, -1 * strideSize1);
      }
   else
      {
      // Set _startReg to the last element to be copied (i.e. start + length - stride)
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _startReg, -1 * strideSize1);
      cursor = generateRRInstruction(_cg, TR::InstOpCode::getAddRegOpCode(), _dstNode, _startReg, _lenReg);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), _dstNode, _strideReg, strideSize1);
      }

   TR::MemoryReference * dstMR = generateS390MemoryReference(_endReg, (int32_t) 0, _cg);
   TR::MemoryReference * srcMR = generateS390MemoryReference(_srcReg, (int32_t) 0, _cg);

   // Move one element using loadOp1/storeOp1
   cursor = generateRXInstruction(_cg, loadOp1, _srcNode, _workReg, srcMR);
   cursor = generateRXInstruction(_cg, storeOp1, _dstNode, _workReg, dstMR);

   dstMR = generateS390MemoryReference(*dstMR, (int32_t) 0, _cg);
   srcMR = generateS390MemoryReference(*srcMR, (int32_t) 0, _cg);

   if (_srcReg != _startReg)
      {
      if (false && !_isForward)
         {
         cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, -1 * strideSize1, _cg));
         }
      else
         {
         cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _srcNode, _srcReg, new (_cg->trHeapMemory()) TR::MemoryReference(_srcReg, strideSize1, _cg));
         }
      }

   cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, _lenNode, _lenReg, new (_cg->trHeapMemory()) TR::MemoryReference(_lenReg, -1 * strideSize1, _cg));

   // Update _endReg by strideSize1
   if (false && !_isForward)
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _endReg, -1 * strideSize1);
   else
      cursor = generateRIInstruction(_cg, TR::InstOpCode::getAddHalfWordImmOpCode(), _dstNode, _endReg, strideSize1);

   generateSTXLoop(strideSize2, loadOp2, storeOp2, _unroll);

   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, skipRoutineLabel);

   cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _srcNode, doneCopyLabel);

   oolPath->swapInstructionListsWithCompilation();

   return cursor;
   }

TR::Instruction *
MemCpyAtomicMacroOp::generateInstruction(TR::InstOpCode::Mnemonic loadOp, TR::InstOpCode::Mnemonic storeOp, TR::Register * wReg, int32_t offset)
   {
   TR::Instruction * cursor;

   TR::MemoryReference * dstMR = generateS390MemoryReference(_endReg, (int32_t) offset, _cg);
   TR::MemoryReference * srcMR = generateS390MemoryReference(_srcReg, (int32_t) offset, _cg);

   cursor = generateRSInstruction(_cg, loadOp, _srcNode, wReg, srcMR);

   cursor = generateRSInstruction(_cg, storeOp, _dstNode, wReg, dstMR);

   dstMR->stopUsingMemRefRegister(_cg);
   srcMR->stopUsingMemRefRegister(_cg);
   return cursor;
   }

TR::Instruction *
MemCpyAtomicMacroOp::generateInstruction()
   {
   TR_ASSERT( 0, "generateInstruction called on non mvc array copy loop\n");
   return 0;
   }

TR::Instruction *
MemCpyAtomicMacroOp::generateLoop()
   {
   TR::Compilation *comp = _cg->comp();
   if (_trace)
      traceMsg(comp, "MemCpyAtomicMacroOp: generateLoop\n");
   TR::Instruction * cursor;

   static char * traceACM = feGetEnv("TR_ArrayCopyMethods");
   if (traceACM)
      {
      printf("%s\n", comp->signature());
      }

   static char * singular = feGetEnv("TR_ArrayCopySingular");

   // Skip the loop if length is zero.
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(_cg);
   TR::LabelSymbol * doneArrayCopyLabel = generateLabelSymbol(_cg);
   TR::LabelSymbol * remainderLabel = generateLabelSymbol(_cg);
   TR::LabelSymbol * preDoneCopyLabel1 = generateLabelSymbol(_cg);
   TR::LabelSymbol * preDoneCopyLabel2 = generateLabelSymbol(_cg);
   TR::LabelSymbol * preDoneCopyLabel3 = generateLabelSymbol(_cg);
   TR::LabelSymbol * preDoneCopyLabel4 = generateLabelSymbol(_cg);
   TR::LabelSymbol * preDoneCopyLabel5 = generateLabelSymbol(_cg);
   TR::LabelSymbol * preDoneCopyLabel6 = generateLabelSymbol(_cg);
   TR::LabelSymbol * oolStartLabel1 = generateLabelSymbol(_cg);
   TR::LabelSymbol * oolStartLabel2 = generateLabelSymbol(_cg);
   TR::LabelSymbol * oolStartLabel3 = generateLabelSymbol(_cg);
   TR::LabelSymbol * oolStartLabel4 = generateLabelSymbol(_cg);
   TR::LabelSymbol * oolStartLabel5 = generateLabelSymbol(_cg);
   TR::LabelSymbol * oolStartLabel6 = generateLabelSymbol(_cg);

   generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::getCmpOpCode(), _dstNode, _lenReg, (int32_t) 0, TR::InstOpCode::COND_BNH, doneArrayCopyLabel, false, false);

   // backwards array copy
   // update end reg and start reg to be added with length
   if (!_isForward)
      {
      cursor = generateRRInstruction(_cg, TR::InstOpCode::getAddRegOpCode(), _dstNode, _srcReg, _lenReg);
      cursor = generateRRInstruction(_cg, TR::InstOpCode::getAddRegOpCode(), _dstNode, _startReg, _lenReg);
      }

   // Load _lenReg into currLenReg
   // Initialize endReg to startReg here
   cursor = generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _dstNode, _endReg, _startReg);

   TR::InstOpCode::Mnemonic unalignedLoadOp;
   TR::InstOpCode::Mnemonic unalignedStoreOp;

   bool generateRemainder = false;


   if (_trace)
      traceMsg(comp, "MemCpyAtomicMacroOp: strideSize: %d\n", strideSize());
   switch (strideSize())
      {
   case 1:
      unalignedLoadOp = TR::InstOpCode::IC;
      unalignedStoreOp = TR::InstOpCode::STC;
      break;
   case 2:
      unalignedLoadOp = TR::InstOpCode::LH;
      unalignedStoreOp = TR::InstOpCode::STH;
      break;
   case 4:
      unalignedLoadOp = TR::InstOpCode::L;
      unalignedStoreOp = TR::InstOpCode::ST;
      break;
   case 8:
      unalignedLoadOp = TR::InstOpCode::LG;
      unalignedStoreOp = TR::InstOpCode::STG;
      break;
   default:
      TR_ASSERT( 0, "bad stride size in array copy loop\n");
      }

   if (_destType == TR::NoType)
      {
      // OR last 3 bits of array src, dst locations
      // AND with 0x7
      // compare and branch if not 0 to next label
      // else fall through
      // into STG loop
      // compare last bit with 1 if not 1,
      // fall through into ST loop
      // else
      // into STC loop

      TR::LabelSymbol * fourByteLoop = generateLabelSymbol(_cg);
      TR::LabelSymbol * twoByteLoop = generateLabelSymbol(_cg);
      TR::LabelSymbol * oneByteLoop = generateLabelSymbol(_cg);
      if (_trace)
         traceMsg(comp, "MemCpyAtomicMacroOp: unknown type routine\n");

      if (comp->target().cpu.getSupportsArch(TR::CPU::z196))
         {
         auto mnemonic = comp->target().is64Bit() ? TR::InstOpCode::OGRK : TR::InstOpCode::ORK;

         cursor = generateRRRInstruction(_cg, mnemonic, _srcNode, _alignedReg, _srcReg, _startReg);
         }
      else
         {
         cursor = generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _srcNode, _alignedReg, _srcReg);
         cursor = generateRRInstruction(_cg, TR::InstOpCode::getOrRegOpCode(), _srcNode, _alignedReg, _startReg);
         }


      cursor = generateRILInstruction(_cg, TR::InstOpCode::NILF, _srcNode, _alignedReg, 0x7);

      // NILL/NILF will set condition code to 0 if result 0 (8 byte aligned) or to 1 if result not 0 (
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, _srcNode, fourByteLoop); // not aligned with 8 bytes
      //cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _srcNode, _alignedReg, 0x0, TR::InstOpCode::COND_BNE, fourByteLoop); // alignment with 8 bytes
      cursor = generateSTXLoop(8, TR::InstOpCode::LG, TR::InstOpCode::STG, false);

      cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, fourByteLoop);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::NILL, _srcNode, _alignedReg, 0x3); // 0x3 == 11, last 2 bits

      // NILL will set condition code to 0 if result 0 (4 byte aligned) or to 1 if result not 0 (
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, _srcNode, twoByteLoop); // not aligned with 4 bytes
      cursor = generateSTXLoop(4, TR::InstOpCode::L, TR::InstOpCode::ST, false);

      cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, twoByteLoop);
      cursor = generateRIInstruction(_cg, TR::InstOpCode::NILL, _srcNode, _alignedReg, 0x1); // 0x1 == 1, last bit

      // NILL will set condition code to 0 if result 0 (2 byte aligned) or to 1 if result not 0 (
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, _srcNode, oneByteLoop); // not aligned with 2 bytes
      cursor = generateSTXLoop(2, TR::InstOpCode::LH, TR::InstOpCode::STH, false);

      cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _rootNode, oneByteLoop);
      cursor = generateSTXLoop(1, TR::InstOpCode::IC, TR::InstOpCode::STC, false);
      }
   else if (_constLength > 0 && _constLength <= strideSize() * 8)
      {
      if (_trace)
         traceMsg(comp, "MemCpyAtomicMacroOp: const loop\n");
      cursor = generateConstLoop(unalignedLoadOp, unalignedStoreOp);
      }
   else if (strideSize() == 8)
      {
      if (_trace)
         traceMsg(comp, "MemCpyAtomicMacroOp: STG loop\n");
      cursor = generateSTXLoop(8, TR::InstOpCode::LG, TR::InstOpCode::STG, _unroll);
      generateRemainder = true;
      }
   else if (strideSize() == 1)
      {
      if (!_isForward)
         {
         if (_trace)
            {
            traceMsg(comp, "MemCpyAtomicMacroOp: 8 bit element loop\n");
            }
         cursor = generateSTXLoop(1, TR::InstOpCode::IC, TR::InstOpCode::STC, _unroll);
         }
      else
         {
         TR_ASSERT( 0, "Non backwards 8 bit element in non mvc array copy\n");
         }
      }
   else if (_isForward && !singular)
      {
      if (_trace)
         traceMsg(comp, "MemCpyAtomicMacroOp: aligned loop\n");
      if (_destType == TR::Int16)
         {
         if (comp->target().cpu.getSupportsArch(TR::CPU::z196))
            {
            auto mnemonic = comp->target().is64Bit() ? TR::InstOpCode::NGRK : TR::InstOpCode::NRK;

            cursor = generateRRRInstruction(_cg, mnemonic, _srcNode, _alignedReg, _srcReg, _startReg);
            }
         else
            {
            cursor = generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _srcNode, _alignedReg, _srcReg);
            cursor = generateRRInstruction(_cg, TR::InstOpCode::getAndRegOpCode(), _srcNode, _alignedReg, _startReg);
            }

         cursor = generateRILInstruction(_cg, TR::InstOpCode::NILF, _srcNode, _alignedReg, 0x7);

         cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _srcNode, _alignedReg, 0x2, TR::InstOpCode::COND_BE, oolStartLabel2); // 0x2 = 0x010
         cursor = generateOneSTXthenSTYLoopLabel(oolStartLabel2, preDoneCopyLabel2, 2, TR::InstOpCode::LH, TR::InstOpCode::STH, 4, TR::InstOpCode::L, TR::InstOpCode::ST);
         cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, preDoneCopyLabel2);
         cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _srcNode, remainderLabel);

         cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _srcNode, _alignedReg, 0x6, TR::InstOpCode::COND_BE, oolStartLabel3); // 0x6 = 0x110
         cursor = generateOneSTXthenSTYLoopLabel(oolStartLabel3, preDoneCopyLabel3, 2, TR::InstOpCode::LH, TR::InstOpCode::STH, 4, TR::InstOpCode::L, TR::InstOpCode::ST);
         cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, preDoneCopyLabel3);
         cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _srcNode, remainderLabel);
         }

      if (_cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
         {
         auto mnemonic = _cg->comp()->target().is64Bit() ? TR::InstOpCode::AGRK : TR::InstOpCode::ARK;

         cursor = generateRRRInstruction(_cg, mnemonic, _srcNode, _alignedReg, _srcReg, _startReg);
         }
      else
         {
         cursor = generateRRInstruction(_cg, TR::InstOpCode::getLoadRegOpCode(), _srcNode, _alignedReg, _srcReg);
         cursor = generateRRInstruction(_cg, TR::InstOpCode::getAddRegOpCode(), _srcNode, _alignedReg, _startReg);
         }

      cursor = generateRILInstruction(_cg, TR::InstOpCode::NILF, _srcNode, _alignedReg, 0x7);

      cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _srcNode, _alignedReg, 0x0, TR::InstOpCode::COND_BE, oolStartLabel4); // 0x0 = 0x000

      cursor = generateSTXLoopLabel(oolStartLabel4, preDoneCopyLabel4, 8, TR::InstOpCode::LG, TR::InstOpCode::STG);
      cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, preDoneCopyLabel4);
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _srcNode, remainderLabel);
      if (_destType == TR::Int16)
         {
         cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _srcNode, _alignedReg, 0x4, TR::InstOpCode::COND_BE, oolStartLabel5); // 0x4 = 0x100
         cursor = generateSTXLoopLabel(oolStartLabel5, preDoneCopyLabel5, 4, TR::InstOpCode::L, TR::InstOpCode::ST);
         cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, preDoneCopyLabel5);
         cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _srcNode, remainderLabel);
         }
      cursor = generateS390CompareAndBranchInstruction(_cg, TR::InstOpCode::C, _srcNode, _alignedReg, 0x8, TR::InstOpCode::COND_BE, oolStartLabel1); // 0x8 = 0x1000

      cursor = generateOneSTXthenSTYLoopLabel(oolStartLabel1, preDoneCopyLabel1, 4, TR::InstOpCode::L, TR::InstOpCode::ST, 8, TR::InstOpCode::LG, TR::InstOpCode::STG);
      cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, preDoneCopyLabel1);
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _srcNode, remainderLabel);

      // Create separate ool for unalignable arrays
      // reason to separate this from Remainder loop is to help branch prediction

      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, _srcNode, oolStartLabel6);
      cursor = generateSTXLoopLabel(oolStartLabel6, preDoneCopyLabel6, strideSize(), unalignedLoadOp, unalignedStoreOp);
      cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, preDoneCopyLabel6);
      cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, _srcNode, remainderLabel);

      generateRemainder = true;
      }
   else
      {
      if (_trace)
         traceMsg(comp, "MemCpyAtomicMacroOp: unaligned loop\n");
      if (strideSize() == 2)
         {
         cursor = generateSTXLoop(2, TR::InstOpCode::LH, TR::InstOpCode::STH, _unroll);
         }
      else
         {
         cursor = generateSTXLoop(4, TR::InstOpCode::L, TR::InstOpCode::ST, _unroll);
         }

      generateRemainder = true;
      }

   // Label to remainder path

   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, remainderLabel);

   // Generate a remainder loop on stride size

   // don't unroll remainder loop
   // don't generate if const & unlooped or backwards
   if (generateRemainder && _isForward)
      {
      switch (_destType)
         {
      case TR::Int8:
         TR_ASSERT( 0, "Non backwards 8 bit element array should be using MVC\n");
         break;
      case TR::Int16:
         cursor = generateSTXLoop(2, TR::InstOpCode::LH, TR::InstOpCode::STH, false);
         break;
      case TR::Float:
      case TR::Int32:
         cursor = generateSTXLoop(4, TR::InstOpCode::L, TR::InstOpCode::ST, false);
         break;
      case TR::Int64:
      case TR::Double:
         cursor = generateSTXLoop(8, TR::InstOpCode::LG, TR::InstOpCode::STG, false);
         break;
      case TR::Address:
         cursor = generateSTXLoop(strideSize(), unalignedLoadOp, unalignedStoreOp, false);
         break;
      default:
         if (!comp->useCompressedPointers())
            {
            cursor = generateSTXLoop(8, TR::InstOpCode::LG, TR::InstOpCode::STG, false);
            }
         else
            {
            cursor = generateSTXLoop(4, TR::InstOpCode::L, TR::InstOpCode::ST, false);
            }
         break;
         }
      }

   cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _dstNode, doneArrayCopyLabel);

   createLoopDependencies(cursor);
   doneArrayCopyLabel->setEndInternalControlFlow();

   return cursor;
   }
MemCpyAtomicMacroOp::MemCpyAtomicMacroOp(TR::Node* rootNode, TR::Node* dstNode, TR::Node* srcNode, TR::CodeGenerator* cg, TR::DataType destType, TR::Register* lenReg, TR::Node * lenNode, bool isForward , bool unroll , int32_t constLength): MemToMemTypedVarLenMacroOp(rootNode, dstNode, srcNode, cg, destType, lenReg, lenNode, isForward)
   {
      static char * unrollFactor = feGetEnv("TR_ArrayCopyUnrollFactor");
      static char * trace = feGetEnv("TR_ArrayCopyTrace");

      if ((bool)trace)
         _trace = true;
      else
         _trace=false;

      if ((bool) unrollFactor)
         {
         switch (unrollFactor[0])
            {
         case '8':
            _unrollFactor = 8;
            break;
         case '7':
            _unrollFactor = 7;
            break;
         case '6':
            _unrollFactor = 6;
            break;
         case '5':
            _unrollFactor = 5;
            break;
         case '4':
            _unrollFactor = 4;
            break;
         case '3':
            _unrollFactor = 3;
            break;
         case '2':
            _unrollFactor = 2;
            break;
         case '1':
            _unrollFactor = 1;
            break;
         default:
            TR_ASSERT (0, "MemCpyAtomicMacroOp: Unacceptable unroll factor\n");
            }
         }
      else
         _unrollFactor = 3;

      _constLength = constLength;
      _unroll = unroll;
      allocWorkReg();
      }

void MemCpyAtomicMacroOp::allocWorkReg()
   {
   TR::Compilation *comp = _cg->comp();
   if (_trace)
      {
      traceMsg(comp, "MemCpyAtomicMacroOp: allocWorkReg\n");
      traceMsg(comp, "_unrollFactor: %d\n", _unrollFactor);
      }

   _alignedReg = _cg->allocateRegister();

   if (_unroll && _isForward)
      {
      if (_unrollFactor)
         {
         switch (_unrollFactor)
            {
         case 8:
            _workReg8 = _cg->allocateRegister();
         case 7:
            _workReg7 = _cg->allocateRegister();
         case 6:
            _workReg6 = _cg->allocateRegister();
         case 5:
            _workReg5 = _cg->allocateRegister();
         case 4:
            _workReg4 = _cg->allocateRegister();
         case 3:
            _workReg3 = _cg->allocateRegister();
         case 2:
            _workReg2 = _cg->allocateRegister();
            }
         }
      }
   _workReg = _cg->allocateRegister();
   }

void MemCpyAtomicMacroOp::createLoopDependencies(TR::Instruction * cursor)
   {
   TR::Compilation *comp = _cg->comp();
   if (_trace)
      traceMsg(comp, "MemCpyAtomicMacroOp: createLoopDependencies\n");
   TR::RegisterDependencyConditions * loopDep;

   int32_t core = numCoreDependencies();

   loopDep = new (_cg->trHeapMemory()) TR::RegisterDependencyConditions(core, core + 2 + _unrollFactor, _cg);

   loopDep->addPostCondition(_workReg, TR::RealRegister::AssignAny);
   loopDep->addPostCondition(_alignedReg, TR::RealRegister::AssignAny);

   if (_unroll && _isForward)
      {
      if (_unrollFactor)
         {
         switch (_unrollFactor)
            {
         case 8:
            loopDep->addPostCondition(_workReg8, TR::RealRegister::AssignAny);
         case 7:
            loopDep->addPostCondition(_workReg7, TR::RealRegister::AssignAny);
         case 6:
            loopDep->addPostCondition(_workReg6, TR::RealRegister::AssignAny);
         case 5:
            loopDep->addPostCondition(_workReg5, TR::RealRegister::AssignAny);
         case 4:
            loopDep->addPostCondition(_workReg4, TR::RealRegister::AssignAny);
         case 3:
            loopDep->addPostCondition(_workReg3, TR::RealRegister::AssignAny);
         case 2:
            loopDep->addPostCondition(_workReg2, TR::RealRegister::AssignAny);
            }
         }
      }

   addCoreDependencies(loopDep);

   if (_applyDepLocally)
      {
      cursor->setDependencyConditions(loopDep);
      }
   else
      {
      _macroDependencies = loopDep;
      }
   }
