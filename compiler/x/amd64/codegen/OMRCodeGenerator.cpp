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

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"

#include <limits.h>
#include <stdint.h>
#include "codegen/FrontEnd.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/X86Evaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "x/codegen/X86Ops.hpp"

OMR::X86::AMD64::CodeGenerator::CodeGenerator() :
   OMR::X86::CodeGenerator()
   {

   if (self()->comp()->getOption(TR_DisableTraps))
      {
      _numberBytesReadInaccessible  = 0;
      _numberBytesWriteInaccessible = 0;
      }
   else
      {
      _numberBytesReadInaccessible  = 4096;
      _numberBytesWriteInaccessible = 4096;
      self()->setHasResumableTrapHandler();
      self()->setEnableImplicitDivideCheck();
      }

   self()->setSupportsDivCheck();

   static char *c = feGetEnv("TR_disableAMD64ValueProfiling");
   if (c)
      self()->comp()->setOption(TR_DisableValueProfiling);

   static char *accessStaticsIndirectly = feGetEnv("TR_AccessStaticsIndirectly");
   if (accessStaticsIndirectly)
      self()->setAccessStaticsIndirectly(true);

   self()->setSupportsDoubleWordCAS();
   self()->setSupportsDoubleWordSet();

   self()->setSupportsGlRegDepOnFirstBlock();
   self()->setConsiderAllAutosAsTacticalGlobalRegisterCandidates();

   // Interpreter frame shape requires all autos to occupy an 8-byte slot on 64-bit.
   //
   if (self()->comp()->getOption(TR_MimicInterpreterFrameShape))
      self()->setMapAutosTo8ByteSlots();

   // Common X86 initialization
   //
   self()->initialize(self()->comp());

   self()->initLinkageToGlobalRegisterMap();

   self()->setRealVMThreadRegister(self()->machine()->getRealRegister(TR::RealRegister::ebp));

   // GRA-related initialization is done after calling initialize() so we can
   // use such things as getNumberOfGlobal[FG]PRs().

   _globalGPRsPreservedAcrossCalls.init(self()->getNumberOfGlobalRegisters(), self()->trMemory());
   _globalFPRsPreservedAcrossCalls.init(self()->getNumberOfGlobalRegisters(), self()->trMemory());

   int16_t i;
   TR_GlobalRegisterNumber grn;
   for (i=0; i < self()->getNumberOfGlobalGPRs(); i++)
      {
      grn = self()->getFirstGlobalGPR() + i;
      if (self()->getProperties().isPreservedRegister((TR::RealRegister::RegNum)self()->getGlobalRegister(grn)))
         _globalGPRsPreservedAcrossCalls.set(grn);
      }
   for (i=0; i < self()->getNumberOfGlobalFPRs(); i++)
      {
      grn = self()->getFirstGlobalFPR() + i;
      if (self()->getProperties().isPreservedRegister((TR::RealRegister::RegNum)self()->getGlobalRegister(grn)))
         _globalFPRsPreservedAcrossCalls.set(grn);
      }

   // Reduce the maxObjectSizeGuaranteedNotToOverflow value on 64-bit such that the
   // runtime comparison does not sign extend (saves an instruction on array allocations).
   // INT_MAX should be sufficiently large.
   //
   if ((uint32_t)_maxObjectSizeGuaranteedNotToOverflow > (uint32_t)INT_MAX)
      {
      _maxObjectSizeGuaranteedNotToOverflow = (uint32_t)INT_MAX;
      }
   }

TR::Register *
OMR::X86::AMD64::CodeGenerator::longClobberEvaluate(TR::Node *node)
   {
   TR_ASSERT(TR::Compiler->target.is64Bit(), "assertion failure");
   TR_ASSERT(node->getOpCode().is8Byte() || node->getOpCode().isRef(), "assertion failure");
   return self()->gprClobberEvaluate(node, MOV8RegReg);
   }

TR_GlobalRegisterNumber
OMR::X86::AMD64::CodeGenerator::getLinkageGlobalRegisterNumber(
      int8_t linkageRegisterIndex,
      TR::DataType type)
   {

   TR_GlobalRegisterNumber result;
   bool isFloat = (type == TR::Float || type == TR::Double);

   if (isFloat)
      {
      if (linkageRegisterIndex >= self()->getProperties().getNumFloatArgumentRegisters())
         return -1;
      else
         result = _fprLinkageGlobalRegisterNumbers[linkageRegisterIndex];
      }
   else
      {
      if (linkageRegisterIndex >= self()->getProperties().getNumIntegerArgumentRegisters())
         return -1;
      else
         result = _gprLinkageGlobalRegisterNumbers[linkageRegisterIndex];
      }

   TR::RealRegister::RegNum realReg = self()->getProperties().getArgumentRegister(linkageRegisterIndex, isFloat);

   TR_ASSERT(self()->getGlobalRegister(result) == realReg, "Result must be consistent with globalRegisterTable");
   return result;
   }

int32_t
OMR::X86::AMD64::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *node)
   {
   // TODO: Currently, lookupEvaluator doesn't deal properly with different
   // glRegDeps on different cases of a lookupswitch.
   //
   static const char *enableLookupswitch = feGetEnv("TR_enableGRAAcrossLookupSwitch");
   if (!enableLookupswitch && node->getOpCode().getOpCodeValue()==TR::lookup)
      return 1;

   if (node->getOpCode().isIf() && node->getFirstChild()->getOpCodeValue() == TR::instanceof)
      return self()->getNumberOfGlobalGPRs() - 6;
   else if(node->getOpCode().isSwitch())
      return self()->getNumberOfGlobalGPRs() - 3; // A memref in a jump for a MemTable might need a base, index and address register.

   return INT_MAX;
   }

void
OMR::X86::AMD64::CodeGenerator::initLinkageToGlobalRegisterMap()
   {
   TR_GlobalRegisterNumber grn;
   int i;

   TR_GlobalRegisterNumber globalRegNumbers[TR::RealRegister::NumRegisters];
   for (i=0; i < self()->getNumberOfGlobalGPRs(); i++)
     {
     grn = self()->getFirstGlobalGPR() + i;
     globalRegNumbers[self()->getGlobalRegister(grn)] = grn;
     }

   for (i=0; i < self()->getNumberOfGlobalFPRs(); i++)
     {
     grn = self()->getFirstGlobalFPR() + i;
     globalRegNumbers[self()->getGlobalRegister(grn)] = grn;
     }

   for (i=0; i < self()->getProperties().getNumIntegerArgumentRegisters(); i++)
     _gprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[self()->getProperties().getIntegerArgumentRegister(i)];

   for (i=0; i < self()->getProperties().getNumFloatArgumentRegisters(); i++)
     _fprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[self()->getProperties().getFloatArgumentRegister(i)];
   }

bool
OMR::X86::AMD64::CodeGenerator::opCodeIsNoOpOnThisPlatform(TR::ILOpCode &opCode)
   {
   return (opCode.getOpCodeValue() == TR::iu2l) ? true : false;
   }
