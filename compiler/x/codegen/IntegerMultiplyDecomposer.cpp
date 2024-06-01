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

#include "x/codegen/IntegerMultiplyDecomposer.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "codegen/InstOpCode.hpp"
#include "env/CompilerEnv.hpp"

namespace TR { class Register; }

// This is duplicated from TR::TreeEvaluator
inline bool getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg)
   {
   return cg->comp()->target().is64Bit() && node->getSize() > 4;
   }

// tempRegArray is an array of temporary registers
// decomposeIntegerMultiplier needs to update this array if it allocates new temporary registers inside
TR::Register *TR_X86IntegerMultiplyDecomposer::decomposeIntegerMultiplier(int32_t &tempRegArraySize, TR::Register **tempRegArray)
   {
   TR::Compilation* comp = _cg->comp();
   bool nodeIs64Bit = getNodeIs64Bit(_node, _cg);
   int64_t absMultiplier = _multiplier;
   if (_multiplier < 0)
      {
      absMultiplier = -_multiplier;
      }
   int32_t decompositionIndex = findDecomposition(absMultiplier);
   TR::Register * target;
   static char * reportFails = feGetEnv("TR_ReportIntMulDecompFailures");
   static char * report      = feGetEnv("TR_ReportIntMulDecomp");
   static char * disable     = feGetEnv("TR_DisableIntMulDecomp");

   if (disable            &&
       absMultiplier != 3 &&
       absMultiplier != 5 &&
       absMultiplier != 9 &&
       !isPowerOf2(absMultiplier))
      {
      if (report || reportFails)
         {
         diagnostic("integer multiply by %d failed: method %s\n", _multiplier, comp->signature());
         }
      return 0;
      }
   if (decompositionIndex != -1)
      {
      target = generateDecompositionInstructions(decompositionIndex, tempRegArraySize, tempRegArray);
      if (_multiplier != absMultiplier) // treats TR::getMinSigned<TR::Int64>() and TR::getMinSigned<TR::Int32>() properly, _multiplier < 0 does not
         {
         generateRegInstruction(TR::InstOpCode::NEGReg(nodeIs64Bit), _node, target, _cg);
         }

      if (report)
         {
         diagnostic("integer multiply by %d decomposed: method %s\n", _multiplier, comp->signature());
         }
      return target;
      }
   else
      {
      int32_t shiftAmount = trailingZeroes(absMultiplier);
      decompositionIndex = findDecomposition((uint64_t)absMultiplier >> shiftAmount); // cast needed so TR::getMinSigned<TR::Int64>() works
      if (decompositionIndex != -1)
         {
         const integerMultiplyComposition& composition = _integerMultiplySolutions[decompositionIndex];
         if (composition._subsequentShiftTooExpensive == false)
            {
            TR_ASSERT_FATAL(comp->compileRelocatableCode() || comp->isOutOfProcessCompilation() || comp->compilePortableCode() || comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_CORE2) == cg()->getX86ProcessorInfo().isIntelCore2(), "isIntelCore2 failed\n");
            TR_ASSERT_FATAL(comp->compileRelocatableCode() || comp->isOutOfProcessCompilation() || comp->compilePortableCode() || comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_NEHALEM) == cg()->getX86ProcessorInfo().isIntelNehalem(), "isIntelNehalem failed\n");
            TR_ASSERT_FATAL(comp->compileRelocatableCode() || comp->isOutOfProcessCompilation() || comp->compilePortableCode() || comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_WESTMERE) == cg()->getX86ProcessorInfo().isIntelWestmere(), "isIntelWestmere failed\n");
            TR_ASSERT_FATAL(comp->compileRelocatableCode() || comp->isOutOfProcessCompilation() || comp->compilePortableCode() || comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_SANDYBRIDGE) == cg()->getX86ProcessorInfo().isIntelSandyBridge(), "isIntelSandyBridge failed\n");
            TR_ASSERT_FATAL(comp->compileRelocatableCode() || comp->isOutOfProcessCompilation() || comp->compilePortableCode() || comp->target().cpu.is(OMR_PROCESSOR_X86_AMD_FAMILY15H) == cg()->getX86ProcessorInfo().isAMD15h(), "isAMD15h failed\n");
            TR_ASSERT_FATAL(comp->compileRelocatableCode() || comp->isOutOfProcessCompilation() || comp->compilePortableCode() || comp->target().cpu.is(OMR_PROCESSOR_X86_AMD_OPTERON) == cg()->getX86ProcessorInfo().isAMDOpteron(), "isAMDOpteron failed\n");

            target = generateDecompositionInstructions(decompositionIndex, tempRegArraySize, tempRegArray);
            if (shiftAmount < 3 &&
               !comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_CORE2) &&
               !comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_NEHALEM) &&
               !comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_WESTMERE) &&
               !comp->target().cpu.is(OMR_PROCESSOR_X86_INTEL_SANDYBRIDGE) &&
               !comp->target().cpu.is(OMR_PROCESSOR_X86_AMD_FAMILY15H) &&
               !comp->target().cpu.is(OMR_PROCESSOR_X86_AMD_OPTERON)) // TODO:: P3 should go straight to else and use shift always
               {
               for (; shiftAmount > 0; --shiftAmount)
                  {
                  generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, target, target, _cg);
                  }
               }
            else
               {
               generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
               }
            if (_multiplier != absMultiplier) // treats TR::getMinSigned<TR::Int64>() properly, _multiplier < 0 does not
               {
               generateRegInstruction(TR::InstOpCode::NEGReg(nodeIs64Bit), _node, target, _cg);
               }
            if (report)
               {
               diagnostic("integer multiply by %d decomposed: method %s\n", _multiplier, comp->signature());
               }
            return target;
            }
         }
      int32_t onesCount = populationCount(absMultiplier);
      if (onesCount == 2)  // sum of 2 powers of 2
         {
         if (_sourceRegister)
            {
            if (!_canClobberSource && (absMultiplier & 1) == 0)
               {
               TR::Register *temp = _cg->allocateRegister();
               if (tempRegArray)
                  {
                  TR_ASSERT(tempRegArraySize<=MAX_NUM_REGISTERS,"Too many temporary registers to handle");
                  tempRegArray[tempRegArraySize++] = temp;
                  }
               generateRegRegInstruction(TR::InstOpCode::MOVRegReg(nodeIs64Bit), _node, temp, _sourceRegister, _cg);
               _sourceRegister = temp;
               }
            }
         else
            {
            _sourceRegister = _cg->gprClobberEvaluate(_node->getFirstChild(), TR::InstOpCode::MOVRegReg(nodeIs64Bit));
            }
         target = _cg->allocateRegister();
         if (tempRegArray)
            {
            TR_ASSERT(tempRegArraySize<=MAX_NUM_REGISTERS,"Too many temporary registers to handle");
            tempRegArray[tempRegArraySize++] = target;
            }
         generateRegRegInstruction(TR::InstOpCode::MOVRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
         if (absMultiplier & 1)
            {
            shiftAmount = trailingZeroes(absMultiplier - 1);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         else if (absMultiplier & 2)
            {
            shiftAmount = trailingZeroes(absMultiplier - 2);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, _sourceRegister, _sourceRegister, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         else if (absMultiplier & 4)
            {
            shiftAmount = trailingZeroes(absMultiplier - 4);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, _sourceRegister, _sourceRegister, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, _sourceRegister, _sourceRegister, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         else
            {
            shiftAmount = trailingZeroes(absMultiplier);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            shiftAmount = trailingZeroes((int64_t)(absMultiplier - (1ll << shiftAmount)));
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, _sourceRegister, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         if (_multiplier != absMultiplier) // treats TR::getMinSigned<TR::Int64>() properly, _multiplier < 0 does not
            {
            generateRegInstruction(TR::InstOpCode::NEGReg(nodeIs64Bit), _node, target, _cg);
            }
         if (_sourceRegister != _node->getFirstChild()->getRegister())
            {
            _cg->stopUsingRegister(_sourceRegister);
            }

         if (report)
            {
            diagnostic("integer multiply by %d decomposed: method %s\n", _multiplier, comp->signature());
            }

         return target;
         }
      else if (onesCount + trailingZeroes(absMultiplier) + leadingZeroes(absMultiplier) == 64)  // difference of 2 powers of two
         {
         if (_sourceRegister)
            {
            if (!_canClobberSource && (absMultiplier & 1) == 0)
               {
               TR::Register *temp = _cg->allocateRegister();
               if (tempRegArray)
                  {
                  TR_ASSERT(tempRegArraySize<=MAX_NUM_REGISTERS,"Too many temporary registers to handle");
                  tempRegArray[tempRegArraySize++] = temp;
                  }
               generateRegRegInstruction(TR::InstOpCode::MOVRegReg(nodeIs64Bit), _node, temp, _sourceRegister, _cg);
               _sourceRegister = temp;
               }
            }
         else
            {
            _sourceRegister = _cg->gprClobberEvaluate(_node->getFirstChild(), TR::InstOpCode::MOVRegReg(nodeIs64Bit));
            }
         target = _cg->allocateRegister();
         if (tempRegArray)
            {
            TR_ASSERT(tempRegArraySize<=MAX_NUM_REGISTERS,"Too many temporary registers to handle");
            tempRegArray[tempRegArraySize++] = target;
            }
         generateRegRegInstruction(TR::InstOpCode::MOVRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
         if (absMultiplier & 1)
            {
            shiftAmount = trailingZeroes(absMultiplier + 1);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::SUBRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         else if (absMultiplier & 2)
            {
            shiftAmount = trailingZeroes(absMultiplier + 2);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, _sourceRegister, _sourceRegister, _cg);
            generateRegRegInstruction(TR::InstOpCode::SUBRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         else if (absMultiplier & 4)
            {
            shiftAmount = trailingZeroes(absMultiplier + 4);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, _sourceRegister, _sourceRegister, _cg);
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit), _node, _sourceRegister, _sourceRegister, _cg);
            generateRegRegInstruction(TR::InstOpCode::SUBRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         else
            {
            shiftAmount = trailingZeroes(absMultiplier);
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, _sourceRegister, shiftAmount, _cg);
            shiftAmount = trailingZeroes((int64_t)(absMultiplier + (1ll << shiftAmount)));
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit), _node, target, shiftAmount, _cg);
            generateRegRegInstruction(TR::InstOpCode::SUBRegReg(nodeIs64Bit), _node, target, _sourceRegister, _cg);
            }
         if (_sourceRegister != _node->getFirstChild()->getRegister())
            {
            _cg->stopUsingRegister(_sourceRegister);
            }
         if (_multiplier != absMultiplier) // treats TR::getMinSigned<TR::Int64>() properly, _multiplier < 0 does not
            {
            generateRegInstruction(TR::InstOpCode::NEGReg(nodeIs64Bit), _node, target, _cg);
            }
         if (report)
            {
            diagnostic("integer multiply by %d decomposed: method %s\n", _multiplier, comp->signature());
            }
         return target;
         }
      }
      if (report || reportFails)
         {
         diagnostic("integer multiply by %d failed: method %s\n", _multiplier, comp->signature());
         }
      return 0;
   }

int32_t TR_X86IntegerMultiplyDecomposer::findDecomposition(int64_t multiplier)
   {
   // linear search for expediency
   // todo: binary search
   int i;
   for (i = 0; (i < NUM_CONSTS_DECOMPOSED) && (_integerMultiplySolutions[i]._multiplier < multiplier); ++i)
      {;}

   if ((i < NUM_CONSTS_DECOMPOSED) && (_integerMultiplySolutions[i]._multiplier == multiplier))
      {
      const integerMultiplyComposition& composition = _integerMultiplySolutions[i];
      int32_t correctionIfTakeAdvantageOfClobberableSource = 0;
      if (_canClobberSource && composition._sourceDisjointWithFirstRegister)
         {
         correctionIfTakeAdvantageOfClobberableSource = 1;
         }
      int32_t numLiveRegs = _cg->getLiveRegisters(TR_GPR)->getNumberOfLiveRegisters();
      int32_t numGPRRegs  = _cg->machine()->getNumberOfGPRs();
      int32_t registerCostOfDecomposition = composition._numAdditionalRegistersNeeded - correctionIfTakeAdvantageOfClobberableSource;
      if (registerCostOfDecomposition <= 1 || // mul would have a default register cost, too
          registerCostOfDecomposition <
          numGPRRegs  -                 // total GPR regs available from machine
          numLiveRegs -                 // live registers at evaluation point
          1)                            // stack pointer not taken into account by numLiveRegs
         {
         return i; // success
         }
      }

   return -1;  // not found
   }

TR::Register *TR_X86IntegerMultiplyDecomposer::generateDecompositionInstructions(
                        int32_t index, int32_t &tempRegArraySize, TR::Register **tempRegArray)
   {
   const integerMultiplyComposition& composition = _integerMultiplySolutions[index];
   bool nodeIs64Bit = getNodeIs64Bit(_node, _cg);
   // initialize and allocate registers to be used by the composition
   // source register is defined to be ordinal register 0
   TR::Register *registerMap[MAX_NUM_REGISTERS];
   // source register should only be null if integer multiply, so safe to assume first child
   // is evaluated to generate it.
   if (composition._mustClobberSource == true)
      {
      if (_sourceRegister)
         {
         if (!_canClobberSource)
            {
            TR::Register *temp = _cg->allocateRegister();
            if (tempRegArray)
               {
               TR_ASSERT(tempRegArraySize<=MAX_NUM_REGISTERS,"Too many temporary registers to handle");
               tempRegArray[tempRegArraySize++] = temp;
               }
            generateRegRegInstruction(TR::InstOpCode::MOVRegReg(nodeIs64Bit), _node, temp, _sourceRegister, _cg);
            _sourceRegister = temp;
            }
         }
      else
         {
         _sourceRegister = _cg->gprClobberEvaluate(_node->getFirstChild(), TR::InstOpCode::MOVRegReg(nodeIs64Bit));
         }
      }
   else if (_sourceRegister == 0)
      {
      _sourceRegister = _cg->evaluate(_node->getFirstChild());
      }
   registerMap[0] = _sourceRegister;
   for (int32_t i = 1; i <= composition._numAdditionalRegistersNeeded; ++i)
      {
      registerMap[i] = _cg->allocateRegister();
      if (tempRegArray)
         {
         TR_ASSERT(tempRegArraySize<=MAX_NUM_REGISTERS,"Too many temporary registers to handle");
         tempRegArray[tempRegArraySize++] = registerMap[i];
         }
      }

   // main loop to generate instructions for the decomposition
   for (int32_t cursor = 0; cursor < MAX_NUM_COMPONENTS; ++cursor)
      {
      const componentOperation& component = composition._components[cursor];
      switch (component._operation)
         {
         case shlRegImm:
            generateRegImmInstruction(TR::InstOpCode::SHLRegImm1(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      component._baseOrImmed,
                                      _cg);
         break;
         case addRegReg:
            generateRegRegInstruction(TR::InstOpCode::ADDRegReg(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      registerMap[component._baseOrImmed],
                                      _cg);
         break;
         case subRegReg:
            generateRegRegInstruction(TR::InstOpCode::SUBRegReg(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      registerMap[component._baseOrImmed],
                                      _cg);
         break;
         case movRegReg:
            generateRegRegInstruction(TR::InstOpCode::MOVRegReg(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      registerMap[component._baseOrImmed],
                                      _cg);
         break;
         case leaRegReg2:
            generateRegMemInstruction(TR::InstOpCode::LEARegMem(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      generateX86MemoryReference(0,
                                                                 registerMap[component._index],
                                                                 TR::MemoryReference::convertMultiplierToStride(2),
                                                                 _cg),
                                      _cg);
         break;
         case leaRegReg4:
            generateRegMemInstruction(TR::InstOpCode::LEARegMem(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      generateX86MemoryReference(0,
                                                                 registerMap[component._index],
                                                                 TR::MemoryReference::convertMultiplierToStride(4),
                                                                 _cg),
                                      _cg);
         break;
         case leaRegReg8:
            generateRegMemInstruction(TR::InstOpCode::LEARegMem(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      generateX86MemoryReference(0,
                                                                 registerMap[component._index],
                                                                 TR::MemoryReference::convertMultiplierToStride(8),
                                                                 _cg),
                                      _cg);
         break;
         case leaRegRegReg:
            generateRegMemInstruction(TR::InstOpCode::LEARegMem(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      generateX86MemoryReference(registerMap[component._baseOrImmed],
                                                                 registerMap[component._index],
                                                                 TR::MemoryReference::convertMultiplierToStride(1),
                                                                 _cg),
                                      _cg);
         break;
         case leaRegRegReg2:
            generateRegMemInstruction(TR::InstOpCode::LEARegMem(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      generateX86MemoryReference(registerMap[component._baseOrImmed],
                                                                 registerMap[component._index],
                                                                 TR::MemoryReference::convertMultiplierToStride(2),
                                                                 _cg),
                                      _cg);
         break;
         case leaRegRegReg4:
            generateRegMemInstruction(TR::InstOpCode::LEARegMem(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      generateX86MemoryReference(registerMap[component._baseOrImmed],
                                                                 registerMap[component._index],
                                                                 TR::MemoryReference::convertMultiplierToStride(4),
                                                                 _cg),
                                      _cg);
         break;
         case leaRegRegReg8:
            generateRegMemInstruction(TR::InstOpCode::LEARegMem(nodeIs64Bit),
                                      _node,
                                      registerMap[component._target],
                                      generateX86MemoryReference(registerMap[component._baseOrImmed],
                                                                 registerMap[component._index],
                                                                 TR::MemoryReference::convertMultiplierToStride(8),
                                                                 _cg),
                                      _cg);
         break;
         case done:
            {
            if (component._target != 0 &&
                registerMap[0] != _node->getFirstChild()->getRegister())
               {
               _cg->stopUsingRegister(registerMap[0]);
               }
            for (int32_t i = 1; i <= composition._numAdditionalRegistersNeeded; ++i)
               {
               if (i != component._target)
                  {
                  _cg->stopUsingRegister(registerMap[i]);
                  }
               }
            return registerMap[component._target];
            }
         }
      }

   return NULL;
   }

bool TR_X86IntegerMultiplyDecomposer::hasDecomposition(int64_t multiplier)
   {
   // linear search for expediency
   // todo: binary search
   int i;
   for (i = 0; (i < NUM_CONSTS_DECOMPOSED) && (_integerMultiplySolutions[i]._multiplier < multiplier); ++i)
      {;}

   return (i < NUM_CONSTS_DECOMPOSED) && (_integerMultiplySolutions[i]._multiplier == multiplier);
   }

const TR_X86IntegerMultiplyDecomposer::integerMultiplyComposition TR_X86IntegerMultiplyDecomposer::_integerMultiplySolutions[NUM_CONSTS_DECOMPOSED] =
   {
    {1,                                   // multiplier
     true,                                // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     0,                                   // number additional registers needed
     {{done,           0, 0, 0}}          // source * 1, holder used to catch pure shifts
    },
    {3,                                   // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {done,           1, 0, 0}}
    },
    {5,                                   // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {done,           1, 0, 0}}
    },
    {6,                                   // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {addRegReg,      1, 0, 0},          // source * 6
      {done,           1, 0, 0}}
    },
    {7,                                   // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegReg8,     1, 0, 0},          // source * 8
      {subRegReg,      1, 0, 0},          // source * 7
      {done,           1, 0, 0}}
    },
    {9,                                   // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {done,           1, 0, 0}}
    },
    {10,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {addRegReg,      1, 0, 0},          // source * 10
      {done,           1, 0, 0}}
    },
    {11,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {addRegReg,      1, 0, 0},          // source * 10
      {addRegReg,      1, 0, 0},          // source * 11
      {done,           1, 0, 0}}
    },
    // 12 handled as 3 with shl by 2
    {13,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {leaRegRegReg4,  1, 1, 0},          // source * 13
      {done,           1, 0, 0}}
    },
    // 14 handled as 7 with shl by 1
    {15,                                  // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {leaRegRegReg4,  1, 1, 1},          // source * 15
      {done,           1, 0, 0}}
    },
    {17,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {addRegReg,      1, 1, 0},          // source * 18
      {subRegReg,      1, 0, 0},          // source * 17
      {done,           1, 0, 0}}
    },
    {19,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {addRegReg,      1, 1, 0},          // source * 18
      {addRegReg,      1, 0, 0},          // source * 19
      {done,           1, 0, 0}}
    },
    // 20 handled as 5 with shl by 2
    {21,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegReg8,     1, 0, 0},          // source * 8
      {subRegReg,      1, 0, 0},          // source * 7
      {leaRegRegReg2,  1, 1, 1},          // source * 21
      {done,           1, 0, 0}}
    },
    // 22 handled as 11 with shl by 1
    {23,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {addRegReg,      1, 1, 0},          // source * 6
      {addRegReg,      1, 1, 0},          // source * 12
      {addRegReg,      1, 1, 0},          // source * 24
      {subRegReg,      1, 0, 0},          // source * 23
      {done,           1, 0, 0}}
    },
    // 24 handled as 3 with shl by 3
    {25,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {shlRegImm,      0, 4, 0},          // source * 16
      {addRegReg,      0, 1, 0},          // source * 25
      {done,           0, 0, 0}}
    },
    {26,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {leaRegRegReg8,  1, 1, 1},          // source * 27
      {subRegReg,      1, 0, 0},          // source * 26
      {done,           1, 0, 0}}
    },
    {27,                                  // multiplier
     true,                                // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      0, 5, 0},          // source * 32
      {subRegReg,      0, 1, 0},          // source * 27
      {done,           0, 0, 0}}
    },
    // 28 handled as 7 with shl by 2
    {29,                                  // multiplier
     true,                                // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},
      {shlRegImm,      0, 5, 0},
      {subRegReg,      0, 1, 0},
      {done,           0, 0, 0}}
    },
    {30,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{movRegReg,      1, 0, 0},          // copy
      {addRegReg,      0, 0, 0},          // source * 2
      {shlRegImm,      1, 5, 0},          // source * 32
      {subRegReg,      1, 0, 0},          // source * 30
      {done,           1, 0, 0}}
     },
    {31,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{movRegReg,      1, 0, 0},          // copy
      {shlRegImm,      1, 5, 0},          // source * 32
      {subRegReg,      1, 0, 0},          // source * 31
      {done,           1, 0, 0}}
    },
    {33,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{movRegReg,      1, 0, 0},          // copy
      {shlRegImm,      1, 5, 0},          // source * 32
      {addRegReg,      1, 0, 0},          // source * 33
      {done,           1, 0, 0}}
    },
    {34,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{movRegReg,      1, 0, 0},          // copy
      {addRegReg,      0, 0, 0},          // source * 2
      {shlRegImm,      1, 5, 0},          // source * 32
      {addRegReg,      1, 0, 0},          // source * 34
      {done,           1, 0, 0}}
    },
    {35,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 5, 0},          // source * 32
      {addRegReg,      1, 0, 0},          // source * 35
      {done,           1, 0, 0}}
    },
    {36,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{movRegReg,      1, 0, 0},          // copy
      {shlRegImm,      1, 5, 0},          // source * 32
      {leaRegRegReg4,  1, 1, 0},          // source * 36
      {done,           1, 0, 0}}
    },
    {37,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      0, 5, 0},          // source * 32
      {addRegReg,      1, 0, 0},          // source * 37
      {done,           1, 0, 0}}
    },
    {41,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,     1, 0, 0},       // source * 9
      {shlRegImm,      0, 5, 0},          // source * 32
      {addRegReg,      1, 0, 0},          // source * 41
      {done,           1, 0, 0}}
    },
    // 44 handled as 11 with shl by 2
    {45,                                  // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg8,  1, 1, 1},          // source * 45
      {done,           1, 0, 0}}
    },
    {49,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg8,  1, 1, 1},          // source * 45
      {leaRegRegReg4,  1, 1, 0},          // source * 49
      {done,           1, 0, 0}}
    },
    {51,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg4,  1, 1, 1},          // source * 25
      {addRegReg,      1, 1, 0},          // source * 50
      {addRegReg,      1, 0, 0},          // source * 51
      {done,           1, 0, 0}}
    },
    {52,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg4,  1, 1, 1},          // source * 25
      {addRegReg,      1, 1, 0},          // source * 50
      {addRegReg,      1, 0, 0},          // source * 51
      {addRegReg,      1, 0, 0},          // source * 52
      {done,           1, 0, 0}}
    },
    {53,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg8,  1, 1, 1},          // source * 45
      {leaRegRegReg8,  1, 1, 0},          // source * 53
      {done,           1, 0, 0}}
    },
    {54,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg4,  1, 1, 1},          // source * 25
      {addRegReg,      1, 1, 0},          // source * 50
      {leaRegRegReg4,  1, 1, 0},          // source * 54
      {done,           1, 0, 0}}
    },
    {55,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {shlRegImm,      0, 6, 0},          // source * 64
      {subRegReg,      0, 1, 0},          // source * 55
      {done,           0, 0, 0}}
    },
    // 56 handled by 7 with shl by 3
    {59,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      0, 6, 0},          // source * 64
      {subRegReg,      0, 1, 0},          // source * 59
      {done,           0, 0, 0}}
    },
    {61,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 6, 0},          // source * 64
      {subRegReg,      0, 1, 0},          // source * 61
      {done,           0, 0, 0}}
    },
    {67,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 6, 0},          // source * 64
      {addRegReg,      0, 1, 0},          // source * 67
      {done,           0, 0, 0}}
    },
    {69,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      0, 6, 0},          // source * 64
      {addRegReg,      0, 1, 0},          // source * 69
      {done,           0, 0, 0}}
    },
    {70,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 6, 0},          // source * 64
      {addRegReg,      0, 1, 0},          // source * 67
      {addRegReg,      0, 1, 0},          // source * 70
      {done,           0, 0, 0}}
    },
    {73,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {shlRegImm,      0, 6, 0},          // source * 64
      {addRegReg,      0, 1, 0},          // source * 73
      {done,           0, 0, 0}}
    },
    {74,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      0, 6, 0},          // source * 64
      {addRegReg,      0, 1, 0},          // source * 69
      {addRegReg,      0, 1, 0},          // source * 74
      {done,           0, 0, 0}}
    },
    {75,                                  // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg4,  1, 1, 1},          // source * 25
      {leaRegRegReg2,  1, 1, 1},          // source * 75
      {done,           1, 0, 0}}
    },
    {79,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      1, 4, 0},          // source * 80
      {subRegReg,      1, 0, 0},          // source * 79
      {done,           1, 0, 0}}
    },
    {81,                                  // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {leaRegRegReg8,  1, 1, 1},          // source * 81
      {done,           1, 0, 0}}
    },
    {82,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {leaRegRegReg8,  1, 1, 1},          // source * 81
      {addRegReg,      1, 0, 0},          // source * 82
      {done,           1, 0, 0}}
    },
    {83,                                  // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {leaRegRegReg8,  1, 1, 1},          // source * 81
      {addRegReg,      1, 0, 0},          // source * 82
      {addRegReg,      1, 0, 0},          // source * 83
      {done,           1, 0, 0}}
    },
   {90,                                  // multiplier
    false,                               // can be assigned with one register
    false,                               // must clobber input register
    false,                               // subsequent shift unaffordable
    1,                                   // number additional registers needed
    {{leaRegRegReg8,  1, 0, 0},          // source * 9
     {addRegReg,      1, 0, 0},          // source * 10
     {leaRegRegReg8,  1, 1, 1},          // source * 90
     {done,           1, 0, 0}}
    },
    {99,                                  // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {addRegReg,      0, 0, 0},          // source * 2
      {addRegReg,      1, 0, 0},          // source * 11
      {leaRegRegReg8,  1, 1, 1},          // source * 99
      {done,           1, 0, 0}}
    },
    {102,                                 // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{movRegReg,      1, 0, 0},          // copy
      {shlRegImm,      1, 5, 0},          // source * 32
      {addRegReg,      1, 0, 0},          // source * 33
      {addRegReg,      1, 0, 0},          // source * 34
      {leaRegRegReg2,  1, 1, 1},          // source * 102
      {done,           1, 0, 0}}
    },
    {125,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 7, 0},          // source * 128
      {subRegReg,      0, 1, 0},          // source * 125
      {done,           0, 0, 0}}
    },
    {225,                                 // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {leaRegRegReg4,  1, 1, 1},          // source * 25
      {leaRegRegReg8,  1, 1, 1},          // source * 225
      {done,           1, 0, 0}}
    },
    {243,                                 // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {leaRegRegReg8,  1, 1, 1},          // source * 81
      {leaRegRegReg2,  1, 1, 1},          // source * 243
      {done,           1, 0, 0}}
    },
    {250,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {addRegReg,      1, 1, 0},          // source * 6
      {shlRegImm,      0, 8, 0},          // source * 256
      {subRegReg,      0, 1, 0},          // source * 250
      {done,           0, 0, 0}}
    },
    {341,                                 // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {addRegReg,      1, 1, 0},          // source * 18
      {addRegReg,      1, 0, 0},          // source * 19
      {addRegReg,      1, 1, 0},          // source * 38
      {leaRegRegReg8,  1, 1, 1},          // source * 342
      {subRegReg,      1, 0, 0},          // source * 341
      {done,           1, 0, 0}}
    },
    {342,                                 // multiplier
     false,                               // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {addRegReg,      1, 1, 0},          // source * 18
      {addRegReg,      1, 0, 0},          // source * 19
      {addRegReg,      1, 1, 0},          // source * 38
      {leaRegRegReg8,  1, 1, 1},          // source * 342
      {done,           1, 0, 0}}
    },
    {365,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {shlRegImm,      0, 6, 0},          // source * 64
      {addRegReg,      1, 0, 0},          // source * 73
      {leaRegRegReg4,  1, 1, 1},          // source * 365
      {done,           1, 0, 0}}
    },
    {375,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 7, 0},          // source * 128
      {subRegReg,      0, 1, 0},          // source * 125
      {leaRegRegReg2,  0, 0, 0},          // source * 375
      {done,           0, 0, 0}}
    },
    {405,                                 // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {leaRegRegReg8,  1, 1, 1},          // source * 81
      {leaRegRegReg4,  1, 1, 1},          // source * 405
      {done,           1, 0, 0}}
    },
    {487,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      0, 9, 0},          // source * 512
      {leaRegRegReg4,  1, 1, 1},          // source * 25
      {subRegReg,      0, 1, 0},          // source * 487
      {done,           0, 0, 0}}
    },
    {500,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {addRegReg,      1, 1, 0},          // source * 6
      {addRegReg,      1, 1, 0},          // source * 12
      {shlRegImm,      0, 9, 0},          // source * 512
      {subRegReg,      0, 1, 0},          // source * 500
      {done,           0, 0, 0}}
    },
    {625,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 7, 0},          // source * 128
      {subRegReg,      0, 1, 0},          // source * 125
      {leaRegRegReg4,  0, 0, 0},          // source * 625
      {done,           0, 0, 0}}
    },
    {729,                                 // multiplier
     true,                                // can be assigned with one register
     false,                               // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg8,  1, 0, 0},          // source * 9
      {leaRegRegReg8,  1, 1, 1},          // source * 81
      {leaRegRegReg8,  1, 1, 1},          // source * 729
      {done,           1, 0, 0}}
    },
    {750,                                 // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 7, 0},          // source * 128
      {subRegReg,      0, 1, 0},          // source * 125
      {leaRegRegReg2,  0, 0, 0},          // source * 375
      {addRegReg,      0, 0, 0},          // source * 750
      {done,           0, 0, 0}}
    },
    {1000,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegReg8,     1, 0, 0},          // source * 8
      {shlRegImm,      0,10, 0},          // source * 1024
      {leaRegRegReg2,  1, 1, 1},          // source * 24
      {subRegReg,      0, 1, 0},          // source * 1000
      {done,           0, 0, 0}}
    },
    {1125,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 7, 0},          // source * 128
      {subRegReg,      0, 1, 0},          // source * 125
      {leaRegRegReg8,  0, 0, 0},          // source * 1125
      {done,           0, 0, 0}}
    },
    {1250,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 7, 0},          // source * 128
      {subRegReg,      0, 1, 0},          // source * 125
      {leaRegRegReg4,  0, 0, 0},          // source * 625
      {addRegReg,      0, 0, 0},          // source * 1250
      {done,           0, 0, 0}}
    },
    {1461,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg4,  1, 0, 0},          // source * 5
      {shlRegImm,      0, 9, 0},          // source * 512
      {leaRegRegReg4,  1, 1, 1},          // source * 25
      {subRegReg,      0, 1, 0},          // source * 487
      {leaRegRegReg2,  0, 0, 0},          // source * 1491
      {done,           0, 0, 0}}
    },
    {1500,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {addRegReg,      1, 1, 0},          // source * 6
      {addRegReg,      1, 1, 0},          // source * 12
      {shlRegImm,      0, 9, 0},          // source * 512
      {subRegReg,      0, 1, 0},          // source * 500
      {leaRegRegReg2,  0, 0, 0},          // source * 1500
      {done,           0, 0, 0}}
    },
    {2000,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegReg8,     1, 0, 0},          // source * 8
      {shlRegImm,      0,10, 0},          // source * 1024
      {leaRegRegReg2,  1, 1, 1},          // source * 24
      {subRegReg,      0, 1, 0},          // source * 1000
      {addRegReg,      0, 0, 0},          // source * 2000
      {done,           0, 0, 0}}
    },
    {2250,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0, 7, 0},          // source * 128
      {subRegReg,      0, 1, 0},          // source * 125
      {leaRegRegReg8,  0, 0, 0},          // source * 1125
      {addRegReg,      0, 0, 0},          // source * 2250
      {done,           0, 0, 0}}
    },
    {2500,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {addRegReg,      1, 1, 0},          // source * 6
      {addRegReg,      1, 1, 0},          // source * 12
      {shlRegImm,      0, 9, 0},          // source * 512
      {subRegReg,      0, 1, 0},          // source * 500
      {leaRegRegReg4,  0, 0, 0},          // source * 2500
      {done,           0, 0, 0}}
    },
    {4500,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     true,                                // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {addRegReg,      1, 1, 0},          // source * 6
      {addRegReg,      1, 1, 0},          // source * 12
      {shlRegImm,      0, 9, 0},          // source * 512
      {subRegReg,      0, 1, 0},          // source * 500
      {leaRegRegReg8,  0, 0, 0},          // source * 4500
      {done,           0, 0, 0}}
    },
    {8189,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0,13, 0},          // source * 8192
      {subRegReg,      0, 1, 0},          // source * 8189
      {done,           0, 0, 0}}
    },
    {8195,                                // multiplier
     false,                               // can be assigned with one register
     true,                                // must clobber input register
     false,                               // subsequent shift unaffordable
     1,                                   // number additional registers needed
     {{leaRegRegReg2,  1, 0, 0},          // source * 3
      {shlRegImm,      0,13, 0},          // source * 8192
      {addRegReg,      1, 0, 0},          // source * 8195
      {done,           1, 0, 0}}
    },
   };
