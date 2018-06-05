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

#include <stdint.h>                                 // for int32_t, etc
#include <stdio.h>                                  // for NULL
#include <stdlib.h>                                 // for abs, atoi
#include "codegen/AheadOfTimeCompile.hpp"           // for AheadOfTimeCompile
#include "codegen/BackingStore.hpp"                 // for TR_BackingStore
#include "codegen/CodeGenPhase.hpp"                 // for CodeGenPhase, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                     // for feGetEnv, etc
#include "codegen/GCStackAtlas.hpp"                 // for GCStackAtlas
#include "codegen/GCStackMap.hpp"                   // for TR_GCStackMap, etc
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction, etc
#include "codegen/Linkage.hpp"                      // for Linkage
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"                 // for TR_LiveRegisters, etc
#include "codegen/Machine.hpp"                      // for Machine, etc
#include "codegen/MemoryReference.hpp"              // for MemoryReference
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/RecognizedMethods.hpp"            // for RecognizedMethod, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterIterator.hpp"             // for RegisterIterator
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"                      // for Snippet, etc
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"                  // for Compilation, etc
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"              // for TR::Options, etc
#include "control/Recompilation.hpp"                // for TR_Recompilation
#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"                // for TR_Recompilation
#endif
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"                              // for Cpu
#include "env/PersistentInfo.hpp"                   // for PersistentInfo
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                           // for uintptrj_t, intptrj_t
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for DataTypes, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                             // for ILOpCode, etc
#include "il/Node.hpp"                              // for Node, vcount_t
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode, etc
#include "il/symbol/AutomaticSymbol.hpp"            // for AutomaticSymbol
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol, etc
#include "il/symbol/MethodSymbol.hpp"               // for MethodSymbol
#include "il/symbol/ParameterSymbol.hpp"            // for ParameterSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"               // for StaticSymbol
#include "infra/Array.hpp"                          // for TR_Array
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/Bit.hpp"                            // for contiguousBits, etc
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/Link.hpp"                           // for TR_LinkHead
#include "infra/List.hpp"                           // for List, etc
#include "optimizer/Optimizer.hpp"                  // for Optimizer
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOutOfLineCodeSection.hpp"
#include "p/codegen/PPCSystemLinkage.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"                      // for TR_LinkageInfo, etc

#if defined(AIXPPC)
#include <sys/debug.h>
#endif

static int32_t identifyFarConditionalBranches(int32_t estimate, TR::CodeGenerator *cg);
extern TR::Register *computeCC_bitwise(TR::CodeGenerator *cg, TR::Node *node, TR::Register *targetReg, bool needsZeroExtension = true);







OMR::Power::CodeGenerator::CodeGenerator() :
   OMR::CodeGenerator(),
     _stackPtrRegister(NULL),
     _constantData(NULL),
     _blockCallInfo(NULL),
     _transientLongRegisters(self()->trMemory()),
     conversionBuffer(NULL),
     _outOfLineCodeSectionList(getTypedAllocator<TR_PPCOutOfLineCodeSection*>(self()->comp()->allocator()))
   {
   // Initialize Linkage for Code Generator
   self()->initializeLinkage();

   _unlatchedRegisterList =
      (TR::RealRegister**)self()->trMemory()->allocateHeapMemory(sizeof(TR::RealRegister*)*(TR::RealRegister::NumRegisters + 1));

   _unlatchedRegisterList[0] = 0; // mark that list is empty

   _linkageProperties = &self()->getLinkage()->getProperties();

   // Set up to collect items for later TOC mapping
   if (TR::Compiler->target.is64Bit())
      {
      self()->setTrackStatics(new (self()->trHeapMemory()) TR_Array<TR::SymbolReference *>(self()->trMemory()));
      self()->setTrackItems(new (self()->trHeapMemory()) TR_Array<TR_PPCLoadLabelItem *>(self()->trMemory()));
      }
   else
      {
      self()->setTrackStatics(NULL);
      self()->setTrackItems(NULL);
      }

   self()->setStackPointerRegister(self()->machine()->getPPCRealRegister(_linkageProperties->getNormalStackPointerRegister()));
   self()->setMethodMetaDataRegister(self()->machine()->getPPCRealRegister(_linkageProperties->getMethodMetaDataRegister()));
   self()->setTOCBaseRegister(self()->machine()->getPPCRealRegister(_linkageProperties->getTOCBaseRegister()));
   self()->getLinkage()->initPPCRealRegisterLinkage();
   self()->getLinkage()->setParameterLinkageRegisterIndex(self()->comp()->getJittedMethodSymbol());
   self()->machine()->initREGAssociations();

   // Tactical GRA settings.
   //
   self()->setGlobalGPRPartitionLimit(TR::Machine::getGlobalGPRPartitionLimit());
   self()->setGlobalFPRPartitionLimit(TR::Machine::getGlobalFPRPartitionLimit());
   self()->setGlobalRegisterTable(_linkageProperties->getRegisterAllocationOrder());
   _numGPR = _linkageProperties->getNumAllocatableIntegerRegisters();
   _firstGPR = 0;
   _lastGPR = _numGPR - 1;
   _firstParmGPR = _linkageProperties->getFirstAllocatableIntegerArgumentRegister();
   _lastVolatileGPR = _linkageProperties->getLastAllocatableIntegerVolatileRegister();
   _numFPR = _linkageProperties->getNumAllocatableFloatRegisters();
   _firstFPR = _numGPR;
   _lastFPR = _firstFPR + _numFPR - 1;
   _firstParmFPR = _linkageProperties->getFirstAllocatableFloatArgumentRegister();
   _lastVolatileFPR = _linkageProperties->getLastAllocatableFloatVolatileRegister();
   self()->setLastGlobalGPR(_lastGPR);
   self()->setLast8BitGlobalGPR(_lastGPR);
   self()->setLastGlobalFPR(_lastFPR);

   _numVRF = _linkageProperties->getNumAllocatableVectorRegisters();
   self()->setFirstGlobalVRF(_lastFPR + 1);
   self()->setLastGlobalVRF(_lastFPR + _numVRF);

   self()->setSupportsGlRegDeps();
   self()->setSupportsGlRegDepOnFirstBlock();

   if (TR::Compiler->target.is32Bit())
      self()->setUsesRegisterPairsForLongs();

   self()->setPerformsChecksExplicitly();
   self()->setConsiderAllAutosAsTacticalGlobalRegisterCandidates();

   self()->setEnableRefinedAliasSets();

   static char * accessStaticsIndirectly = feGetEnv("TR_AccessStaticsIndirectly");
   if (accessStaticsIndirectly)
      self()->setAccessStaticsIndirectly(true);

   if (!debug("noLiveRegisters"))
      {
      self()->addSupportedLiveRegisterKind(TR_GPR);
      self()->addSupportedLiveRegisterKind(TR_FPR);
      self()->addSupportedLiveRegisterKind(TR_CCR);
      self()->addSupportedLiveRegisterKind(TR_VSX_SCALAR);
      self()->addSupportedLiveRegisterKind(TR_VSX_VECTOR);
      self()->addSupportedLiveRegisterKind(TR_VRF);

      self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(self()->comp()), TR_GPR);
      self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(self()->comp()), TR_FPR);
      self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(self()->comp()), TR_CCR);
      self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(self()->comp()), TR_VRF);
      self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(self()->comp()), TR_VSX_SCALAR);
      self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(self()->comp()), TR_VSX_VECTOR);
      }


    self()->setSupportsConstantOffsetInAddressing();
    self()->setSupportsVirtualGuardNOPing();
    self()->setSupportsPrimitiveArrayCopy();
    self()->setSupportsReferenceArrayCopy();

    // disabled for now
    //
    if (self()->comp()->getOption(TR_AggressiveOpts) &&
        !self()->comp()->getOption(TR_DisableArraySetOpts))
       {
       self()->setSupportsArraySet();
       }
    self()->setSupportsArrayCmp();

    if (TR::Compiler->target.cpu.getPPCSupportsVSX())
       {
       static bool disablePPCTRTO = (feGetEnv("TR_disablePPCTRTO") != NULL);
       static bool disablePPCTRTO255 = (feGetEnv("TR_disablePPCTRTO255") != NULL);
       static bool disablePPCTROT = (feGetEnv("TR_disablePPCTROT") != NULL);
       static bool disablePPCTROTNoBreak = (feGetEnv("TR_disablePPCTROTNoBreak") != NULL);

       if (!disablePPCTRTO)
          self()->setSupportsArrayTranslateTRTO();
#ifndef __LITTLE_ENDIAN__
       else if (!disablePPCTRTO255)
          setSupportsArrayTranslateTRTO255();
#endif

       if (!disablePPCTROT)
          self()->setSupportsArrayTranslateTROT();

       if (!disablePPCTROTNoBreak)
          self()->setSupportsArrayTranslateTROTNoBreak();
       }

    if (!self()->comp()->getOption(TR_DisableShrinkWrapping))
       self()->setSupportsShrinkWrapping();

   _numberBytesReadInaccessible = 0;
   _numberBytesWriteInaccessible = 4096;

   // Poorly named.  Not necessarily Java specific.
   //
   self()->setSupportsJavaFloatSemantics();

   self()->setSupportsDivCheck();
   self()->setSupportsLoweringConstIDiv();
   if (TR::Compiler->target.is64Bit())
      self()->setSupportsLoweringConstLDiv();
   self()->setSupportsLoweringConstLDivPower2();

   static bool disableDCAS = (feGetEnv("TR_DisablePPCDCAS") != NULL);

   if (!disableDCAS &&
       TR::Compiler->target.is64Bit() &&
       TR::Options::useCompressedPointers())
      {
      self()->setSupportsDoubleWordCAS();
      self()->setSupportsDoubleWordSet();
      }

   if (self()->is64BitProcessor())
      self()->setSupportsInlinedAtomicLongVolatiles();

   // Standard allows only offset multiple of 4
   // TODO: either improves the query to be size-based or gets the offset into a register
   if (TR::Compiler->target.is64Bit())
      self()->setSupportsAlignedAccessOnly();

   // TODO: distinguishing among OOO and non-OOO implementations
   if (TR::Compiler->target.isSMP())
      self()->setEnforceStoreOrder();

   if (TR::Compiler->vm.hasResumableTrapHandler(self()->comp()))
      self()->setHasResumableTrapHandler();

   if (TR::Compiler->target.cpu.getPPCSupportsTM() && !self()->comp()->getOption(TR_DisableTM))
      self()->setSupportsTM();

   // enable LM if hardware supports instructions and running the reduced-pause GC policy
   if (TR::Compiler->target.cpu.getPPCSupportsLM())
      self()->setSupportsLM();

   if(self()->getSupportsTM())
      self()->setSupportsTMHashMapAndLinkedQueue();

   if (TR::Compiler->target.cpu.getPPCSupportsVMX() && TR::Compiler->target.cpu.getPPCSupportsVSX())
      self()->setSupportsAutoSIMD();

   static bool disableTMDCAS = (feGetEnv("TR_DisablePPCTMDCAS") != NULL);
   if (self()->getSupportsTM() && !disableTMDCAS &&
       TR::Compiler->target.is64Bit() &&
       !TR::Options::useCompressedPointers())
      {
      self()->setSupportsTMDoubleWordCASORSet();
      }


   if (!self()->comp()->getOption(TR_DisableRegisterPressureSimulation))
      {
      for (int32_t i = 0; i < TR_numSpillKinds; i++)
         _globalRegisterBitVectors[i].init(self()->getNumberOfGlobalRegisters(), self()->trMemory());

      for (TR_GlobalRegisterNumber grn=0; grn < self()->getNumberOfGlobalRegisters(); grn++)
         {
         TR::RealRegister::RegNum reg = (TR::RealRegister::RegNum)self()->getGlobalRegister(grn);
         if (self()->getFirstGlobalGPR() <= grn && grn <= self()->getLastGlobalGPR())
            _globalRegisterBitVectors[ TR_gprSpill ].set(grn);
         else if (self()->getFirstGlobalFPR() <= grn && grn <= self()->getLastGlobalFPR())
            _globalRegisterBitVectors[ TR_fprSpill ].set(grn);

         if (!self()->getProperties().getPreserved(reg))
            _globalRegisterBitVectors[ TR_volatileSpill ].set(grn);
         if (self()->getProperties().getIntegerArgument(reg) || self()->getProperties().getFloatArgument(reg))
            _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);
         }

      }

   // Calculate inverse of getGlobalRegister function
   //
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

   // Initialize linkage reg arrays
   TR::PPCLinkageProperties linkageProperties = self()->getProperties();
   for (i=0; i < linkageProperties.getNumIntArgRegs(); i++)
     _gprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getIntegerArgumentRegister(i)];
   for (i=0; i < linkageProperties.getNumFloatArgRegs(); i++)
     _fprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getFloatArgumentRegister(i)];

   if (self()->comp()->getOptions()->getRegisterAssignmentTraceOption(TR_TraceRARegisterStates))
      {
      self()->setGPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_GPR));
      self()->setFPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_FPR));
      }

   self()->setSupportsProfiledInlining();

   TR_ResolvedMethod *method = self()->comp()->getJittedMethodSymbol()->getResolvedMethod();
   TR_ReturnInfo      returnInfo = self()->getLinkage()->getReturnInfoFromReturnType(method->returnType());
   self()->comp()->setReturnInfo(returnInfo);
   }

uintptrj_t *
OMR::Power::CodeGenerator::getTOCBase()
    {
        TR_PPCTableOfConstants *pTOC = toPPCTableOfConstants(self()->comp()->getPersistentInfo()->getPersistentTOC());
        return pTOC->getTOCBase();
    }

TR_PPCScratchRegisterManager*
OMR::Power::CodeGenerator::generateScratchRegisterManager(int32_t capacity)
   {
   return new (self()->trHeapMemory()) TR_PPCScratchRegisterManager(capacity, self());
   }

// PPC codeGenerator hook for class unloading events
//
void
OMR::Power::CodeGenerator::ppcCGOnClassUnloading(void *loaderPtr)
   {
   TR_PPCTableOfConstants::onClassUnloading(loaderPtr);
   }

TR_RuntimeHelper
directToInterpreterHelper(
      TR::ResolvedMethodSymbol *methodSymbol,
      TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   return methodSymbol->getVMCallHelper();
#else
   return (TR_RuntimeHelper)0;
#endif
   }

TR::Instruction *
OMR::Power::CodeGenerator::generateSwitchToInterpreterPrePrologue(
      TR::Instruction *cursor,
      TR::Node *node)
   {
   TR::Register   *gr0 = self()->machine()->getPPCRealRegister(TR::RealRegister::gr0);
   TR::ResolvedMethodSymbol *methodSymbol = self()->comp()->getJittedMethodSymbol();
   TR::SymbolReference    *revertToInterpreterSymRef = self()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCrevertToInterpreterGlue, false, false, false);
   uintptrj_t             ramMethod = (uintptrj_t)methodSymbol->getResolvedMethod()->resolvedMethodAddress();
   TR::SymbolReference    *helperSymRef = self()->symRefTab()->findOrCreateRuntimeHelper(directToInterpreterHelper(methodSymbol, self()), false, false, false);
   uintptrj_t             helperAddr = (uintptrj_t)helperSymRef->getMethodAddress();

   // gr0 must contain the saved LR; see Recompilation.s
   cursor = new (self()->trHeapMemory()) TR::PPCTrg1Instruction(TR::InstOpCode::mflr, node, gr0, cursor, self());
   cursor = self()->getLinkage()->flushArguments(cursor);
   cursor = generateDepImmSymInstruction(self(), TR::InstOpCode::bl, node, (uintptrj_t)revertToInterpreterSymRef->getMethodAddress(), new (self()->trHeapMemory()) TR::RegisterDependencyConditions(0,0, self()->trMemory()), revertToInterpreterSymRef, NULL, cursor);

   if (TR::Compiler->target.is64Bit())
      {
      int32_t highBits = (int32_t)(ramMethod>>32), lowBits = (int32_t)ramMethod;
      cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                  TR::Compiler->target.cpu.isBigEndian()?highBits:lowBits, TR_RamMethod, cursor);
      cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                  TR::Compiler->target.cpu.isBigEndian()?lowBits:highBits, cursor);
      }
   else
      {
      cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node, (int32_t)ramMethod, TR_RamMethod, cursor);
      }

   if (self()->comp()->getOption(TR_EnableHCR))
      self()->comp()->getStaticHCRPICSites()->push_front(cursor);

  if (TR::Compiler->target.is64Bit())
     {
     int32_t highBits = (int32_t)(helperAddr>>32), lowBits = (int32_t)helperAddr;
     cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                 TR::Compiler->target.cpu.isBigEndian()?highBits:lowBits,
                 TR_AbsoluteHelperAddress, helperSymRef, cursor);
     cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                 TR::Compiler->target.cpu.isBigEndian()?lowBits:highBits, cursor);
     }
  else
     {
     cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node, (int32_t)helperAddr, TR_AbsoluteHelperAddress, helperSymRef, cursor);
     }

   // Used in FSD to store an  instruction
   cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node, 0, cursor);

   return cursor;
   }

void
OMR::Power::CodeGenerator::beginInstructionSelection()
   {

   TR::Node *firstNode = self()->comp()->getStartTree()->getNode();
   _returnTypeInfoInstruction = NULL;
   if ((self()->comp()->getJittedMethodSymbol()->getLinkageConvention() == TR_Private))
      {
      _returnTypeInfoInstruction = new (self()->trHeapMemory()) TR::PPCImmInstruction(TR::InstOpCode::dd, firstNode, 0, NULL, self());
      }

   generateAdminInstruction(self(), TR::InstOpCode::proc, firstNode);
   }

void
OMR::Power::CodeGenerator::endInstructionSelection()
   {
   if (_returnTypeInfoInstruction != NULL)
      {
      _returnTypeInfoInstruction->setSourceImmediate(static_cast<uint32_t>(self()->comp()->getReturnInfo()));
      }
   }

void
OMR::Power::CodeGenerator::staticTracking(TR::SymbolReference *symRef)
   {
   if (TR::Compiler->target.is64Bit())
      {
      TR_Array<TR::SymbolReference *> *symRefArray = self()->getTrackStatics();
      int32_t idx;

      for (idx=0; idx<symRefArray->size(); idx++)
         if (symRefArray->element(idx) == symRef)
            return;
      symRefArray->add(symRef);
      }
   }

void
OMR::Power::CodeGenerator::itemTracking(
      int32_t offset,
      TR::LabelSymbol *label)
   {
   if (TR::Compiler->target.is64Bit())
      {
      self()->getTrackItems()->add(new (self()->trHeapMemory()) TR_PPCLoadLabelItem(offset, label));
      }
   }


bool
OMR::Power::CodeGenerator::mulDecompositionCostIsJustified(
      int numOfOperations,
      char bitPosition[],
      char operationType[],
      int64_t value)
   {
   if (numOfOperations <= 0)
      return false;

   // Notice: the total simple operations is (2*numOfOperations-1). The following checking is a heuristic
   //         on processors we still care about.
   switch (TR::Compiler->target.cpu.id())
      {
      case TR_PPCpwr630: // 2S+1M FXU out-of-order
         return (numOfOperations<=4);

      case TR_PPCnstar:
      case TR_PPCpulsar: // 1S+1M FXU in-order
         return (numOfOperations<=8);

      case TR_PPCgpul:
      case TR_PPCgp:
      case TR_PPCgr:    // 2 FXU out-of-order back-to-back 2 cycles. Mul is only 4 to 6 cycles
         return (numOfOperations<=2);

      case TR_PPCp6:    // Mul is on FPU for 17cycles blocking other operations
         return (numOfOperations<=16);

      case TR_PPCp7:    // Mul blocks other operations for up to 4 cycles
         return (numOfOperations<=3);

      default:          // assume a generic design similar to 604
         return (numOfOperations<=3);
      }
   }

void OMR::Power::CodeGenerator::doRegisterAssignment(TR_RegisterKinds kindsToAssign)
   {
   TR::Instruction *prevInstruction;

   // gprs, fprs, and ccrs are all assigned in backward direction

   TR::Instruction *instructionCursor = self()->getAppendInstruction();

   TR::Block *currBlock = NULL;
   TR::Instruction * currBBEndInstr = instructionCursor;

   if (!self()->comp()->getOption(TR_DisableOOL))
      {
      TR::list<TR::Register*> *spilledRegisterList = new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::Register*>(self()->comp()->allocator()));
      self()->setSpilledRegisterList(spilledRegisterList);
      }

   if (self()->getDebug())
      self()->getDebug()->startTracingRegisterAssignment();

   while (instructionCursor)
      {
      prevInstruction = instructionCursor->getPrev();

      self()->tracePreRAInstruction(instructionCursor);

      self()->setCurrentBlockIndex(instructionCursor->getBlockIndex());

      instructionCursor->assignRegisters(TR_GPR);

      // Maintain Internal Control Flow Depth
      // Track internal control flow on labels
      if (instructionCursor->isLabel())
         {
         TR::PPCLabelInstruction *li = (TR::PPCLabelInstruction *)instructionCursor;

         if (li->getLabelSymbol() != NULL)
            {
            if (li->getLabelSymbol()->isStartInternalControlFlow())
               {
               self()->decInternalControlFlowNestingDepth();
               }
            if (li->getLabelSymbol()->isEndInternalControlFlow())
               {
               self()->incInternalControlFlowNestingDepth();
               }
            }
         }

      self()->freeUnlatchedRegisters();
      self()->buildGCMapsForInstructionAndSnippet(instructionCursor);

      self()->tracePostRAInstruction(instructionCursor);

      instructionCursor = prevInstruction;
      }

   if (self()->getDebug())
      self()->getDebug()->stopTracingRegisterAssignment();
   }

TR::Instruction *OMR::Power::CodeGenerator::generateNop(TR::Node *n, TR::Instruction *preced, TR_NOPKind nopKind)
   {
   TR::InstOpCode::Mnemonic nop;

   switch (nopKind)
      {
      case TR_NOPEndGroup:
         nop = TR::InstOpCode::genop;
         break;
      case TR_ProbeNOP:
         nop = TR::InstOpCode::probenop;
         break;
      default:
         nop = TR::InstOpCode::nop;
      }

   if (preced)
      return new (self()->trHeapMemory()) TR::Instruction(nop, n, preced, self());
   else
      return new (self()->trHeapMemory()) TR::Instruction(nop, n,  self());
   }

TR::Instruction *OMR::Power::CodeGenerator::generateGroupEndingNop(TR::Node *node , TR::Instruction *preced)
   {
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6) // handles P7, P8
      {
      preced = self()->generateNop(node , preced , TR_NOPEndGroup);
      }
   else
      {
      preced = self()->generateNop(node , preced);
      preced = self()->generateNop(node , preced);
      preced = self()->generateNop(node , preced);
      }

   return preced;
   }

TR::Instruction *OMR::Power::CodeGenerator::generateProbeNop(TR::Node *node , TR::Instruction *preced)
   {
   preced = self()->generateNop(node , preced , TR_ProbeNOP);
   return preced;
   }

static bool isEBBTerminatingBranch(TR::Instruction *instr)
   {
   TR::Instruction *ppcInstr = instr;
   switch (ppcInstr->getOpCodeValue())
      {
      case TR::InstOpCode::b:                // Unconditional branch
      case TR::InstOpCode::ba:               // Branch to absolute address
      case TR::InstOpCode::bctr:             // Branch to count register
      case TR::InstOpCode::bctrl:            // Branch to count register and link
      case TR::InstOpCode::bfctr:            // Branch false to count register
      case TR::InstOpCode::btctr:            // Branch true to count register
      case TR::InstOpCode::bl:               // Branch and link
      case TR::InstOpCode::bla:              // Branch and link to absolute address
      case TR::InstOpCode::blr:              // Branch to link register
      case TR::InstOpCode::blrl:             // Branch to link register and link
      case TR::InstOpCode::beql:             // Branch and link if equal
      case TR::InstOpCode::bgel:             // Branch and link if greater than or equal
      case TR::InstOpCode::bgtl:             // Branch and link if greater than
      case TR::InstOpCode::blel:             // Branch and link if less than or equal
      case TR::InstOpCode::bltl:             // Branch and link if less than
      case TR::InstOpCode::bnel:             // Branch and link if not equal
      case TR::InstOpCode::vgdnop:           // A vgdnop can be backpatched to a branch
         return true;
      default:
         return false;
      }
   }

// PPC CodeGen Peephole routines
static bool isWAWOrmrPeepholeCandidateInstr(TR::Instruction *instr)
   {
   if (instr != NULL && !instr->willBePatched() &&
      (!instr->isLabel() ||
       (instr->getNode()->getOpCodeValue() == TR::BBStart &&
        instr->getNode()->getBlock()->isExtensionOfPreviousBlock())) &&
      //FIXME: properly -- defs/refs of instructions with Mem are not quite right.
       (instr->getMemoryReference()==NULL ||
        instr->getMemoryReference()->getUnresolvedSnippet()==NULL))
      {
      return true;
      }
   return false;
   }

// look for the patterns:
// 1.  redundant mr:
//       mr  rX, rX
//     and remove the mr
// 2.  mr copyback:
//       mr  rY, rX
//       ... <no modification of rY or rX>
//       mr  rX, rY
//     and remove the second mr
// 3.  rewrite base or index register:
//       mr  rY, rX
//       ... <no modification of rY or rX>
//       load or store with rY as a base or index register
//     and rewrite the load or store to replace base or index rY with rX
//     (except for the special cases where a base register cannot be changed
//     to gr0 or in an update form)
// 4.  rewrite source operand:
//       mr  rY, rX
//       ... <no modification of rY or rX>
//       <op> with rY as a source register
//     and rewrite the <op> to replace source rY with rX (which potentially
//     allows the mr and <op> to be dispatched/issued together)
//     (except for the special cases where rX is gr0 and cannot be used)
// 5,  remove mr if all uses rewritten
//       mr  rY, rX
//       ... <no modification of rY or rX><all uses of rY rewritten to rX>
//       <op> with rY as a target register
//     and remove the mr
//     and change any intervening reg maps that contain rY to rX
static void mrPeepholes(TR::CodeGenerator *cg, TR::Instruction *mrInstruction)
   {
   TR::Compilation *comp = cg->comp();
   int32_t windowSize = 0;
   const int32_t maxWindowSize = comp->isOptServer() ? 16 : 8;
   static char *disableMRPeepholes = feGetEnv("TR_DisableMRPeepholes");

   if (disableMRPeepholes != NULL) return;
   TR::list<TR_GCStackMap*> stackMaps(getTypedAllocator<TR_GCStackMap*>(comp->allocator()));
   TR::Register *mrSourceReg = mrInstruction->getSourceRegister(0);
   TR::Register *mrTargetReg = mrInstruction->getTargetRegister(0);

   if (mrTargetReg == mrSourceReg)
      {
      if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove redundant mr %p.\n", mrInstruction))
         mrInstruction->remove();
      return;
      }

   TR::Instruction *current = mrInstruction->getNext();
   bool all_mr_source_uses_rewritten = true;
   bool inEBB = false;
   bool extendWindow = false;

   while (isWAWOrmrPeepholeCandidateInstr(current) &&
          !isEBBTerminatingBranch(current) &&
          windowSize < maxWindowSize)
      {
      // Keep track of any GC points we cross in order to fix them if we remove a register move
      // Note: Don't care about GC points in snippets, since we won't remove a reg move if inEBB, i.e. if we've encountered a branch (to a snippet or elsewhere)
      if (current->getGCMap() &&
          current->getGCMap()->getRegisterMap() & cg->registerBitMask(toRealRegister(mrTargetReg)->getRegisterNumber()))
         stackMaps.push_front(current->getGCMap());

      if (current->isBranchOp())
         inEBB = true;

      if (current->getOpCodeValue() == TR::InstOpCode::mr &&
          current->getSourceRegister(0) == mrTargetReg &&
          current->getTargetRegister(0) == mrSourceReg)
         {
         if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove mr copyback %p.\n", current))
            {
            current->remove();
            extendWindow = true;
            }
         else
            return;
         // bypass the test below for an overwrite of the source register
         current = current->getNext();
         if (extendWindow)
            {
            windowSize = 0;
            extendWindow = false;
            }
         else
            windowSize++;
         continue;
         }

      TR::MemoryReference *memRef = current->getMemoryReference();

      if (memRef != NULL)
         {
         // See FIXME above: these are all due to defs/refs not accurate.
         // We didn't try to test if base or index will be modified in delayedIndexForm
         if (memRef->isUsingDelayedIndexedForm() &&
             (memRef->getBaseRegister() == mrSourceReg ||
              memRef->getIndexRegister() == mrSourceReg ||
              memRef->getBaseRegister() == mrTargetReg ||
              memRef->getIndexRegister() == mrTargetReg))
            return;
         if (current->isUpdate() &&
             (memRef->getBaseRegister() == mrSourceReg ||
              memRef->getBaseRegister() == mrTargetReg))
            return;
         if (memRef->getBaseRegister() == mrTargetReg)
            {
            if (toRealRegister(mrSourceReg)->getRegisterNumber() == TR::RealRegister::gr0)
               all_mr_source_uses_rewritten = false;
            else if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite base register %p.\n", current))
               {
               memRef->setBaseRegister(mrSourceReg);
               extendWindow = true;
               }
            else
               return;
            }
         if (memRef->getIndexRegister() == mrTargetReg)
            {
            if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite index register %p.\n", current))
               {
               memRef->setIndexRegister(mrSourceReg);
               extendWindow = true;
               }
            else
               return;
            }
         if (current->getKind() == OMR::Instruction::IsMemSrc1 &&
             ((TR::PPCMemSrc1Instruction *)current)->getSourceRegister() == mrTargetReg)
            {
            if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite store source register %p.\n", current))
               {
               ((TR::PPCMemSrc1Instruction *)current)->setSourceRegister(mrSourceReg);
               extendWindow = true;
               }
            else
               return;
            }
         }
      else if (current->getOpCodeValue() == TR::InstOpCode::vgdnop)
         // a vgdnop can be backpatched to a branch, and so we must suppress
         // any later attempt to remove the mr
         all_mr_source_uses_rewritten = false;
      else
         {
         switch(current->getKind())
            {
            case OMR::Instruction::IsSrc1:
               {
               TR::PPCSrc1Instruction *inst = (TR::PPCSrc1Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Src1 source register %p.\n", current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src1:
               {
               TR::PPCTrg1Src1Instruction *inst = (TR::PPCTrg1Src1Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Trg1Src1 source register %p.\n", current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src1Imm:
               {
               TR::PPCTrg1Src1ImmInstruction *inst = (TR::PPCTrg1Src1ImmInstruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (toRealRegister(mrSourceReg)->getRegisterNumber() == TR::RealRegister::gr0 &&
                      (current->getOpCodeValue() == TR::InstOpCode::addi ||
                       current->getOpCodeValue() == TR::InstOpCode::addi2 ||
                       current->getOpCodeValue() == TR::InstOpCode::addis))
                     all_mr_source_uses_rewritten = false;
                  else if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Trg1Src1Imm source register %p.\n", current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src1Imm2:
               {
               TR::PPCTrg1Src1Imm2Instruction *inst = (TR::PPCTrg1Src1Imm2Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Trg1Src1Imm2 source register %p.\n", current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }

                  if(current->getOpCode().usesTarget() &&
                     inst->getTargetRegister() == mrTargetReg)
                     return;
               break;
               }
            case OMR::Instruction::IsSrc2:
               {
               TR::PPCSrc2Instruction *inst = (TR::PPCSrc2Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Src2 1st source register %p.\n", current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               if (inst->getSource2Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Src2 2nd source register %p.\n", current))
                     {
                     inst->setSource2Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src2:
               {
               if (current->getOpCodeValue() == TR::InstOpCode::xvmaddadp ||
                   current->getOpCodeValue() == TR::InstOpCode::xvmsubadp ||
                   current->getOpCodeValue() == TR::InstOpCode::xvnmsubadp)
                  return;

               TR::PPCTrg1Src2Instruction *inst = (TR::PPCTrg1Src2Instruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Trg1Src2 1st source register %p.\n", current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               if (inst->getSource2Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Trg1Src2 2nd source register %p.\n", current))
                     {
                     inst->setSource2Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               break;
               }
            case OMR::Instruction::IsTrg1Src2Imm:
               {
               TR::PPCTrg1Src2ImmInstruction *inst = (TR::PPCTrg1Src2ImmInstruction *)current;
               if (inst->getSource1Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Trg1Src2Imm 1st source register %p.\n", current))
                     {
                     inst->setSource1Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               if (inst->getSource2Register() == mrTargetReg)
                  {
                  if (performTransformation(comp, "O^O PPC PEEPHOLE: Rewrite Trg1Src2Imm 2nd source register %p.\n", current))
                     {
                     inst->setSource2Register(mrSourceReg);
                     extendWindow = true;
                     }
                  else
                     return;
                  }
               break;
               }
            }
         }

      // if instruction overwrites either of the mr source or target
      // registers, bail out

      if (current->defsRegister(toRealRegister(mrTargetReg)))
         {
         // having rewritten all uses of mrTargetReg, the mr can be removed
         if (!inEBB && all_mr_source_uses_rewritten &&
             performTransformation(comp, "O^O PPC PEEPHOLE: Remove mr %p when target redefined at %p.\n", mrInstruction, current))
            {
            // Adjust any register maps that will be invalidated by the removal
            if (!stackMaps.empty())
               {
               for (auto stackMapIter = stackMaps.begin(); stackMapIter != stackMaps.end(); ++stackMapIter)
                  {
                  if (cg->getDebug())
                     traceMsg(comp, "Adjusting register map %p; removing %s, adding %s due to removal of mr %p\n",
                             *stackMapIter,
                             cg->getDebug()->getName(mrTargetReg),
                             cg->getDebug()->getName(mrSourceReg),
                             mrInstruction);
                  (*stackMapIter)->resetRegistersBits(cg->registerBitMask(toRealRegister(mrTargetReg)->getRegisterNumber()));
                  (*stackMapIter)->setRegisterBits(cg->registerBitMask(toRealRegister(mrSourceReg)->getRegisterNumber()));
                  }
               }

            mrInstruction->remove();
            }
         return;
         }

      if (current->defsRegister(toRealRegister(mrSourceReg)))
         return;

      current = current->getNext();
      if (extendWindow)
         {
         windowSize = 0;
         extendWindow = false;
         }
      else
         windowSize++;
      }
   }

// look for the pattern:
//   <op>  rX, ...
//   ... <no modification of rX or cr0>
//   cmpi  cr0, rX, 0
// and remove the cmpi and convert <op> to a record form
static void recordFormPeephole(TR::CodeGenerator *cg, TR::Instruction *cmpiInstruction)
   {
   TR::Compilation *comp = cg->comp();
   static char *disableRecordFormPeephole = feGetEnv("TR_DisableRecordFormPeephole");

   if (disableRecordFormPeephole != NULL) return;

   TR::Register *cmpiSourceReg = cmpiInstruction->getSourceRegister(0);
   TR::Register *cmpiTargetReg = cmpiInstruction->getTargetRegister(0);

   if (cmpiInstruction->getSourceImmediate() != 0 ||
       toRealRegister(cmpiTargetReg)->getRegisterNumber() != TR::RealRegister::cr0)
      return;

   TR::Instruction *current = cmpiInstruction->getPrev();
   while (current != NULL &&
          !current->isLabel() &&
          (!isEBBTerminatingBranch(current) ||
          current->getKind() == OMR::Instruction::IsControlFlow) &&
          current->getOpCodeValue() != TR::InstOpCode::wrtbar)
      {
      if (current->getKind() == OMR::Instruction::IsControlFlow)
         {
         current = current->getPrev();
         continue;
         }

      if (current->getPrimaryTargetRegister() != NULL &&
          current->getPrimaryTargetRegister() == cmpiSourceReg)
         // if target reg is same as source of compare
         {
         if (current->getOpCode().isRecordForm())
            {
            if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove redundant compare immediate %p.\n", cmpiInstruction))
               cmpiInstruction->remove();
            return;
            }
         if (current->getOpCode().hasRecordForm())
            {
            // avoid certain record forms on POWER4/POWER5
            if (TR::Compiler->target.cpu.id() == TR_PPCgp ||
                TR::Compiler->target.cpu.id() == TR_PPCgr)
               {
               TR::InstOpCode::Mnemonic opCode = current->getOpCodeValue();
               // addc_r, subfc_r, divw_r and divd_r are microcoded
               if (opCode == TR::InstOpCode::addc || opCode == TR::InstOpCode::subfc ||
                   opCode == TR::InstOpCode::divw || opCode == TR::InstOpCode::divd)
                  return;
               // rlwinm_r is cracked, so avoid if possible
               if (opCode == TR::InstOpCode::rlwinm)
                  {
                  TR::PPCTrg1Src1Imm2Instruction *inst = (TR::PPCTrg1Src1Imm2Instruction *)current;
                  // see if we can convert to an andi_r or andis_r
                  if (inst->getSourceImmediate() == 0 &&
                      (inst->getMask()&0xffff0000) == 0)
                     {
                     if (performTransformation(comp, "O^O PPC PEEPHOLE: Change %p to andi_r, remove compare immediate %p.\n", current, cmpiInstruction))
                        {
                        generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, inst->getNode(), inst->getPrimaryTargetRegister(), inst->getSourceRegister(0),cmpiTargetReg, inst->getMask(), current);
                        current->remove();
                        cmpiInstruction->remove();
                        }
                     return;
                     }
                  else if (inst->getSourceImmediate() == 0 &&
                      (inst->getMask()&0x0000ffff) == 0)
                     {
                     if (performTransformation(comp, "O^O PPC PEEPHOLE: Change %p to andis_r, remove compare immediate %p.\n", current, cmpiInstruction))
                        {
                        generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andis_r, inst->getNode(), inst->getPrimaryTargetRegister(), inst->getSourceRegister(0), cmpiTargetReg, ((uint32_t)inst->getMask()) >> 16, current);
                        current->remove();
                        cmpiInstruction->remove();
                        }
                     return;
                     }
                  }
               }
            // change the opcode to get record form
            if (performTransformation(comp, "O^O PPC PEEPHOLE: Change %p to record form, remove compare immediate %p.\n", current, cmpiInstruction))
               {
               current->setOpCodeValue(current->getRecordFormOpCode());
               cmpiInstruction->remove();
               }
            return;
            }
         return;
         }
      else
         {
         if (current->defsRegister(toRealRegister(cmpiSourceReg)) ||
             current->defsRegister(toRealRegister(cmpiTargetReg)) ||
             current->getOpCode().isRecordForm())
            return;
         }
      current = current->getPrev();
      }
   }

static void lhsPeephole(TR::CodeGenerator *cg, TR::Instruction *storeInstruction)
   {
   static bool disableLHSPeephole = feGetEnv("TR_DisableLHSPeephole") != NULL;
   if (disableLHSPeephole)
      {
      return;
      }

   TR::Compilation     *comp = cg->comp();
   TR::Instruction   *loadInstruction = storeInstruction->getNext();
   TR::InstOpCode&      storeOp = storeInstruction->getOpCode();
   TR::InstOpCode&      loadOp = loadInstruction->getOpCode();
   TR::MemoryReference *storeMemRef = ((TR::PPCMemSrc1Instruction *)storeInstruction)->getMemoryReference();
   TR::MemoryReference *loadMemRef = ((TR::PPCTrg1MemInstruction *)loadInstruction)->getMemoryReference();

   // Next instruction has to be a load,
   // the store and load have to agree on size,
   // they have to both be GPR ops or FPR ops,
   // neither can be the update form,
   // and they both have to be resolved.
   // TODO: Relax the first constraint a bit and handle cases were unrelated instructions appear between the store and the load
   if (!loadOp.isLoad() ||
       storeMemRef->getLength() != loadMemRef->getLength() ||
       !(storeOp.gprOp() == loadOp.gprOp() ||
         storeOp.fprOp() == loadOp.fprOp()) ||
       storeOp.isUpdate() ||
       loadOp.isUpdate() ||
       storeMemRef->getUnresolvedSnippet() ||
       loadMemRef->getUnresolvedSnippet())
      {
      return;
      }

   // TODO: Don't bother with indexed loads/stores for now, fix them later
   if (storeMemRef->getIndexRegister() || loadMemRef->getIndexRegister())
      {
      return;
      }

   // The store and load mem refs have to agree on whether or not they need delayed offsets
   if (storeMemRef->hasDelayedOffset() != loadMemRef->hasDelayedOffset())
      {
      // Memory references that use delayed offsets will not have the correct offset
      // at this point, so we can't compare their offsets to the offsets used by regular references,
      // however we can still compare two mem refs that both use delayed offsets.
      return;
      }

   // The store and load have to use the same base and offset
   if (storeMemRef->getBaseRegister() != loadMemRef->getBaseRegister() ||
       storeMemRef->getOffset(*comp) != loadMemRef->getOffset(*comp))
      {
      return;
      }

   TR::Register *srcReg = ((TR::PPCMemSrc1Instruction *)storeInstruction)->getSourceRegister();
   TR::Register *trgReg = ((TR::PPCTrg1MemInstruction *)loadInstruction)->getTargetRegister();

   if (srcReg == trgReg)
      {
      // Found the pattern:
      //   stw/std rX, ...
      //   lwz/ld rX, ...
      // will remove ld
      // and replace lwz with rlwinm, rX, rX, 0, 0xffffffff
      if (loadInstruction->getOpCodeValue() == TR::InstOpCode::lwz)
         {
         if (performTransformation(comp, "O^O PPC PEEPHOLE: Replace lwz " POINTER_PRINTF_FORMAT " with rlwinm after store " POINTER_PRINTF_FORMAT ".\n", loadInstruction, storeInstruction))
            {
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, loadInstruction->getNode(), trgReg, srcReg, 0, 0xffffffff, storeInstruction);
            loadInstruction->remove();
            }
         }
      else if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove redundant load " POINTER_PRINTF_FORMAT " after store " POINTER_PRINTF_FORMAT ".\n", loadInstruction, storeInstruction))
         {
         loadInstruction->remove();
         }
      return;
      }

   // Found the pattern:
   //   stw/std rX, ...
   //   lwz/ld rY, ...
   // and will remove lwz, which will result in:
   //   stw rX, ...
   //   rlwinm rY, rX, 0, 0xffffffff
   // or will remove ld, which will result in:
   //   std rX, ...
   //   mr rY, rX
   // and then the mr peephole should run on the resulting mr
   if (loadInstruction->getOpCodeValue() == TR::InstOpCode::lwz)
      {
      if (performTransformation(comp, "O^O PPC PEEPHOLE: Replace redundant load " POINTER_PRINTF_FORMAT " after store " POINTER_PRINTF_FORMAT " with rlwinm.\n", loadInstruction, storeInstruction))
         {
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, loadInstruction->getNode(), trgReg, srcReg, 0, 0xffffffff, storeInstruction);
         loadInstruction->remove();
         }
      }
   else if (performTransformation(comp, "O^O PPC PEEPHOLE: Replace redundant load " POINTER_PRINTF_FORMAT " after store " POINTER_PRINTF_FORMAT " with mr.\n", loadInstruction, storeInstruction))
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, loadInstruction->getNode(), trgReg, srcReg, storeInstruction);
      loadInstruction->remove();
      }
   }

static void syncPeephole(TR::CodeGenerator *cg, TR::Instruction *instructionCursor, int window)
   {
   TR::Compilation *comp = cg->comp();
   TR::Instruction *first = instructionCursor;
   TR::InstOpCode::Mnemonic nextOp = instructionCursor->getNext()->getOpCodeValue();
   static char *disableSyncPeepholes = feGetEnv("TR_DisableSyncPeepholes");
   int visited = 0;
   bool removeFirst, removeNext;

   if (disableSyncPeepholes != NULL) return;

   while (visited < window)
      {
      instructionCursor = first;

      if (instructionCursor->getNext()->isSyncSideEffectFree())
         {
         for (; visited < window && instructionCursor->getNext()->isSyncSideEffectFree(); visited++)
            {
            instructionCursor = instructionCursor->getNext();
            nextOp = instructionCursor->getNext()->getOpCodeValue();
            }
         }
      else
         {
         visited++;
         nextOp = instructionCursor->getNext()->getOpCodeValue();
         }

      if (visited >= window) return;

      removeFirst = removeNext = false;

      switch(first->getOpCodeValue())
         {
         case TR::InstOpCode::isync:
            {
            if(nextOp == TR::InstOpCode::sync || nextOp == TR::InstOpCode::lwsync || nextOp == TR::InstOpCode::isync)
               removeFirst = true;
            break;
            }
         case TR::InstOpCode::lwsync:
            {
            if(nextOp == TR::InstOpCode::sync)
               removeFirst = true;
            else if(nextOp == TR::InstOpCode::lwsync || nextOp == TR::InstOpCode::isync)
               removeNext = true;
            break;
            }
         case TR::InstOpCode::sync:
            {
            if(nextOp == TR::InstOpCode::sync || nextOp == TR::InstOpCode::lwsync || nextOp == TR::InstOpCode::isync)
               removeNext = true;
            break;
            }
         default:
            return;
         }

      if (removeFirst)
         {
         if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove redundant syncronization instruction %p.\n", first))
            {
            cg->generateNop(first->getNode(), first->getPrev(), TR_NOPStandard);
            first->remove();
            return;
            }
         }
      else if(removeNext)
         {
         if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove redundant syncronization instruction %p.\n", instructionCursor->getNext()))
            {
            cg->generateNop(instructionCursor->getNext()->getNode(), instructionCursor, TR_NOPStandard);
            instructionCursor->getNext()->getNext()->remove();
            }
         }
      }
   }

static void trapPeephole(TR::CodeGenerator *cg, TR::Instruction *instructionCursor)
   {
   static char *disableTrapPeephole = feGetEnv("TR_DisableTrapPeephole");
   int WINDOW = 2;
   TR::RealRegister *RAr;
   TR::Register *RAv;

   if ((RAv = instructionCursor->getSourceRegister(0)) != NULL)
      RAr = instructionCursor->getSourceRegister(0)->getRealRegister();
   else
      return;

   if (disableTrapPeephole != NULL || instructionCursor->getSourceRegister(1) != NULL ||
      instructionCursor->getOffset() != 0)
      {
      return;
      }

   if ((RAv->getStartOfRange() != NULL) && RAv->getStartOfRange()->isLoad())
      return;

   TR::RealRegister *RTr;
   TR::Instruction *next = instructionCursor->getNext();

   for(int i = 0; i < WINDOW  && next != NULL; i++)
      {
      if (next->isStore())
         return;
      if (!next->isLoad() && !next->isStore() && !next->isSyncSideEffectFree())
         return;
      if (NULL != next->getMemoryReference() &&
          NULL != next->getMemoryReference()->getUnresolvedSnippet())
         return;
      if (NULL != next->getGCMap())
         return;

      for (int j = 0; next->getTargetRegister(j) != NULL; j++)
         {
         if (next->getTargetRegister(j)->getRealRegister() == RAr)
            return;
         }
      if (next->isLoad() && next->getSourceRegister(0) != NULL && next->getSourceRegister(0)->getRealRegister() == RAr)
         {
         if (next->getSourceRegister(1) != NULL)
            return;

         //Page 0 which is readable on AIX is 4096 bytes long, and the largest possible load is 16 bytes.
         if (next->getOffset() >= (4096-16) || next->getOffset() < 0)
            return;

         TR::Instruction *trap = instructionCursor;
         TR::Instruction *load = next;

         if (trap->getPrev() != NULL && load->getNext() != NULL)
            {
            for (TR::Instruction *prev = load->getPrev(); prev != trap->getPrev(); prev = prev->getPrev())
               {
               for (int k = 0; load->getTargetRegister(k) != NULL; k++)
                  {
                  for (int l = 0; prev->getSourceRegister(l) != NULL; l++)
                     {
                     if (prev->getSourceRegister(l)->getRealRegister() == load->getTargetRegister(k)->getRealRegister())
                        return;
                     }
                  for (int m = 0; prev->getTargetRegister(m) != NULL; m++)
                     {
                     if (prev->getTargetRegister(m)->getRealRegister() == load->getTargetRegister(k)->getRealRegister())
                        return;
                     }
                  }
               }
            performTransformation(cg->comp(), "O^O PPC PEEPHOLE: Swap trap %p and load %p instructions.\n", trap, load);

            if (i > 0)
               {
               TR::Instruction *loadPrev = load->getPrev();
               TR::Instruction *loadNext = load->getNext();
               trap->getPrev()->setNext(load);
               trap->getNext()->setPrev(load);
               load->setNext(trap->getNext());
               load->setPrev(trap->getPrev());
               loadPrev->setNext(trap);
               loadNext->setPrev(trap);
               trap->setNext(loadNext);
               trap->setPrev(loadPrev);
               }
            else
               {
               trap->getPrev()->setNext(load);
               load->setPrev(trap->getPrev());
               trap->setNext(load->getNext());
               load->getNext()->setPrev(trap);
               load->setNext(trap);
               trap->setPrev(load);
               }
            return;
            }
         }
      next = next->getNext();
      }
   }

static void wawPeephole(TR::CodeGenerator *cg, TR::Instruction *instr)
   {
   TR::Compilation *comp = cg->comp();
   static char *disableWAWPeephole = feGetEnv("TR_DisableWAWPeephole");
   int32_t window = 0, maxWindowSize = comp->isOptServer() ? 20 : 10;

   if (disableWAWPeephole)
      return;

   // Get and check if there is a target register
   TR::Register *currTargetReg = instr->getTargetRegister(0);

   if (currTargetReg && currTargetReg->getKind() != TR_CCR && isWAWOrmrPeepholeCandidateInstr(instr) &&
       !instr->isBranchOp() && !instr->isCall() &&
       !instr->getOpCode().isRecordForm() &&
       !instr->getOpCode().setsCarryFlag())
      {
      TR::Instruction *current = instr->getNext();

      while (isWAWOrmrPeepholeCandidateInstr(current) &&
             !current->isBranchOp() &&
             !current->isCall() &&
             window < maxWindowSize)
         {

         if (current->getOpCode().usesTarget())
            return;
         switch(current->getKind())
            {
            case OMR::Instruction::IsSrc1:
            case OMR::Instruction::IsTrg1Src1:
            case OMR::Instruction::IsTrg1Src1Imm:
            case OMR::Instruction::IsTrg1Src1Imm2:
               if (currTargetReg == current->getSourceRegister(0))
                  return;
               break;
            case OMR::Instruction::IsSrc2:
            case OMR::Instruction::IsTrg1Src2:
            case OMR::Instruction::IsTrg1Src2Imm:
            case OMR::Instruction::IsMem:
            case OMR::Instruction::IsTrg1Mem:
               if (currTargetReg == current->getSourceRegister(0) ||
                   currTargetReg == current->getSourceRegister(1))
                  return;
               break;
            case OMR::Instruction::IsTrg1Src3:
            case OMR::Instruction::IsMemSrc1:
               if (currTargetReg == current->getSourceRegister(0) ||
                   currTargetReg == current->getSourceRegister(1) ||
                   currTargetReg == current->getSourceRegister(2))
                  return;
               break;
            default:
                  return;
               break;
            }
         if (current->getTargetRegister(0) == currTargetReg)
            {
            TR::Instruction *q[4];
            // In case the instruction is part of the requestor
            bool isConstData = cg->checkAndFetchRequestor(instr, q);
            if (isConstData)
               {
               if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove dead instrcution group from WAW %p -> %p.\n", instr, current))
                  {
                  for (int32_t i = 0; i < 4; i++)
                     {
                     if (q[i])  q[i]->remove();
                     }
                  }
               return;
               }
            else
               {
               if (performTransformation(comp, "O^O PPC PEEPHOLE: Remove dead instrcution from WAW %p -> %p.\n", instr, current))
                  {
                  instr->remove();
                  }
               }
            return;
            }
         current = current->getNext();
         window++;
         }
      }
   }

bool OMR::Power::CodeGenerator::checkAndFetchRequestor(TR::Instruction *instr, TR::Instruction **q)
   {
   if (_constantData)
      {
      return _constantData->getRequestorsFromNibble(instr, q, true);
      }
   return false;
   }

// PPC Codegen Peephole Pass
// one forward pass over the code looking for a variety of peephole
// opportunities
void OMR::Power::CodeGenerator::doPeephole()
   {
   static char *disablePeephole = feGetEnv("TR_DisablePeephole");
   if (disablePeephole)
      return;

   int syncPeepholeWindow = self()->comp()->isOptServer() ? 12 : 6;

   if (self()->comp()->getOptLevel() == noOpt)
      return;

   TR::Instruction *instructionCursor = self()->getFirstInstruction();

   while (instructionCursor)
      {
      self()->setCurrentBlockIndex(instructionCursor->getBlockIndex());

      if ((TR::Compiler->target.cpu.id() == TR_PPCp6) && instructionCursor->isTrap())
         {
#if defined(AIXPPC)
         trapPeephole(self(),instructionCursor);
#endif
         }
      else
         {
         switch(instructionCursor->getOpCodeValue())
            {
            case TR::InstOpCode::stw:
#if defined(TR_HOST_64BIT)
            case TR::InstOpCode::std:
#endif
               {
               lhsPeephole(self(), instructionCursor);
               break;
               }
            case TR::InstOpCode::mr:
               {
               mrPeepholes(self(), instructionCursor);
               break;
               }
            case TR::InstOpCode::cmpi4:
               {
               if (TR::Compiler->target.is32Bit())
                  recordFormPeephole(self(), instructionCursor);
               break;
               }
            case TR::InstOpCode::cmpi8:
               {
               if (TR::Compiler->target.is64Bit())
                  recordFormPeephole(self(), instructionCursor);
               break;
               }
            case TR::InstOpCode::sync:
               {
               syncPeephole(self(), instructionCursor, syncPeepholeWindow);
               break;
               }
            case TR::InstOpCode::lwsync:
               {
               syncPeephole(self(), instructionCursor, syncPeepholeWindow);
               break;
               }
            case TR::InstOpCode::isync:
               {
               syncPeephole(self(), instructionCursor, syncPeepholeWindow);
               break;
               }
            default:
               {
               wawPeephole(self(), instructionCursor);
               break;
               }
            }
         }
      instructionCursor = instructionCursor->getNext();
      }
   }
// end of PPC Codegen Peephole routines


// routines for shrinkWrapping
//
bool OMR::Power::CodeGenerator::processInstruction(TR::Instruction *instr,
                                             TR_BitVector **registerUsageInfo,
                                             int32_t &blockNum,
                                             int32_t &blockMarker,
                                             bool traceIt)
   {
   TR::Instruction *ppcInstr = instr;

   if (ppcInstr->getOpCode().isCall())
      {
      TR::Node *callNode = ppcInstr->getNode();
      if (callNode->getSymbolReference())
         {
         bool callPreservesRegisters = true;
         TR::SymbolReference *symRef = callNode->getSymbolReference();
         TR::MethodSymbol *m = NULL;
         if (symRef->getSymbol()->isMethod())
            {
            m = symRef->getSymbol()->castToMethodSymbol();
            if ((m->isHelper() && !m->preservesAllRegisters()) ||
                m->isVMInternalNative() ||
                m->isJITInternalNative() ||
                m->isJNI() ||
                m->isSystemLinkageDispatch() || (m->getLinkageConvention() == TR_System))
               callPreservesRegisters = false;
            }

         if (!callPreservesRegisters)
            {
            if (traceIt)
               traceMsg(self()->comp(), "call instr [%p] does not preserve regs\n", ppcInstr);
            registerUsageInfo[blockNum]->setAll(TR::RealRegister::NumRegisters);
            }
         else
            {
            if (traceIt)
               traceMsg(self()->comp(), "call instr [%p] preserves regs\n", ppcInstr);
            }
         }
      }

   switch (ppcInstr->getKind())
      {
      case OMR::Instruction::IsAdmin:
         {
         TR::Node *node = ((TR::PPCAdminInstruction *)ppcInstr)->getNode();
         if (node)
            {
            TR::Block *block = node->getBlock();
            if (node->getOpCodeValue() == TR::BBStart)
               {
               blockMarker = 1;
               if (traceIt)
                  traceMsg(self()->comp(), "Now generating register use information for block_%d\n", blockNum);
               }
            else if (node->getOpCodeValue() == TR::BBEnd)
               {
               blockMarker = 2;
               if (traceIt)
                  traceMsg(self()->comp(), "Done generating register use information for block_%d\n", blockNum);
               }
            }
         return false;
         }
      case OMR::Instruction::IsTrg1:
      case OMR::Instruction::IsTrg1Imm:
      case OMR::Instruction::IsTrg1Src1:
      case OMR::Instruction::IsTrg1Src1Imm:
      case OMR::Instruction::IsTrg1Src1Imm2:
      case OMR::Instruction::IsTrg1Src2:
      case OMR::Instruction::IsTrg1Src2Imm:
      case OMR::Instruction::IsTrg1Src3:
      case OMR::Instruction::IsTrg1Mem:
         {
         int32_t tgtRegNum = ((TR::RealRegister*)((TR::PPCTrg1Instruction *)ppcInstr)->getTargetRegister())->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "instr [%p] USES register [%d]\n", ppcInstr, tgtRegNum);
         registerUsageInfo[blockNum]->set(tgtRegNum);
         return true;
         }
      case OMR::Instruction::IsControlFlow:
         {
         int32_t numTargets = ((TR::PPCControlFlowInstruction *)ppcInstr)->getNumTargets();
         for (int32_t i = 0; i < numTargets; i++)
            {
            int32_t tgtRegNum = ((TR::RealRegister*)((TR::PPCControlFlowInstruction *)ppcInstr)->getTargetRegister(i))->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "control flow instr [%p] USES register [%d]\n", ppcInstr, tgtRegNum);
            registerUsageInfo[blockNum]->set(tgtRegNum);
            }
         return true;
         }
      default:
         {
         return false;
         }
      }

   return true;
   }

uint32_t OMR::Power::CodeGenerator::isPreservedRegister(int32_t regIndex)
   {
   // is register preserved in prologue
   //
   for (int32_t pindex = TR::RealRegister::gr15;
         pindex <= TR::RealRegister::LastGPR;
         pindex++)
      {
      TR::RealRegister::RegNum idx = (TR::RealRegister::RegNum)pindex;
      if (idx == regIndex)
         return pindex;
      }
   return -1;
   }

bool OMR::Power::CodeGenerator::isReturnInstruction(TR::Instruction *instr)
   {
   return (instr->getOpCodeValue() == TR::InstOpCode::ret);
   }

bool OMR::Power::CodeGenerator::isBranchInstruction(TR::Instruction *instr)
   {
   return (instr->isBranchOp() || instr->getOpCodeValue() == TR::InstOpCode::vgdnop);
   }

bool OMR::Power::CodeGenerator::isLabelInstruction(TR::Instruction *instr)
   {
   return (instr->isLabel());
   }

int32_t OMR::Power::CodeGenerator::isFenceInstruction(TR::Instruction *instr)
   {
   TR::Instruction *ppcInstr = instr;

   if (ppcInstr->getOpCodeValue() == TR::InstOpCode::fence)
      {
      TR::Node *fenceNode = ppcInstr->getNode();
      if (fenceNode->getOpCodeValue() == TR::BBStart)
         return 1;
      else if (fenceNode->getOpCodeValue() == TR::BBEnd)
         return 2;
      }

   return 0;
   }

bool OMR::Power::CodeGenerator::isAlignmentInstruction(TR::Instruction *instr)
   {
   return false;
   }

TR::Instruction *
OMR::Power::CodeGenerator::splitEdge(TR::Instruction *instr,
                                 bool isFallThrough,
                                 bool needsJump,
                                 TR::Instruction *newSplitLabel,
                                 TR::list<TR::Instruction*> *jmpInstrs,
                                 bool firstJump)
   {
   // instr is the jump instruction containing the target
   // this is the edge that needs to be split
   // if !isFallThrough, then the instr points at the BBEnd
   // of the block containing the jump
   //
   TR::LabelSymbol *newLabel = NULL;
   if (!newSplitLabel)
      newLabel = generateLabelSymbol(self());
   else
      {
      newLabel = ((TR::PPCLabelInstruction *)newSplitLabel)->getLabelSymbol();
      }
   TR::LabelSymbol *targetLabel = NULL;
   TR::Instruction *location = NULL;
   if (isFallThrough)
      {
      location = instr;
      }
   else
      {
      TR::PPCLabelInstruction *labelInstr = (TR::PPCLabelInstruction *)instr;
      targetLabel = labelInstr->getLabelSymbol();
      labelInstr->setLabelSymbol(newLabel);
      location = targetLabel->getInstruction()->getPrev();
      // now fixup any remaining jmp instrs that jmp to the target
      // so that they now jmp to the new label
      //
      for (auto jmpItr = jmpInstrs->begin(); jmpItr != jmpInstrs->end(); ++jmpItr)
         {
         TR::PPCLabelInstruction *l = (TR::PPCLabelInstruction *)(*jmpItr);
         if (l->getLabelSymbol() == targetLabel)
            {
            traceMsg(self()->comp(), "split edge fixing jmp instr %p\n", (*jmpItr));
            l->setLabelSymbol(newLabel);
            }
         }
      }

   TR::Instruction *cursor = newSplitLabel;
   if (!cursor)
      cursor = generateLabelInstruction(self(), TR::InstOpCode::label, location->getNode(), newLabel, location);

   if (!isFallThrough && needsJump)
      {
      TR::Instruction *jmpLocation = cursor->getPrev();
      TR::Instruction *i = generateLabelInstruction(self(), TR::InstOpCode::b, jmpLocation->getNode(), targetLabel, jmpLocation);
      traceMsg(self()->comp(), "split edge jmp instr at [%p]\n", i);
      }
   return cursor;
   }

TR::Instruction *
OMR::Power::CodeGenerator::splitBlockEntry(TR::Instruction *instr)
   {
   TR::LabelSymbol *newLabel = generateLabelSymbol(self());
   TR::Instruction *location = instr->getPrev();
   return generateLabelInstruction(self(), TR::InstOpCode::label, location->getNode(), newLabel, location);
   }

int32_t
OMR::Power::CodeGenerator::computeRegisterSaveDescription(TR_BitVector *regs, bool populateInfo)
   {
   uint32_t rsd = 0;
   // if the reg is present in this bitvector,
   // then it must have been assigned.
   //
   TR_BitVectorIterator regIt(*regs);
   while (regIt.hasMoreElements())
      {
      int32_t regIndex = self()->isPreservedRegister(regIt.getNextElement());
      if (regIndex != -1)
         {
         rsd |= 1 << (regIndex - TR::RealRegister::gr15);
         }
      }
   // place the register save size in the top half
   rsd |= self()->getLinkage()->getRegisterSaveSize() << 17;
   return rsd;
   }

void
OMR::Power::CodeGenerator::processIncomingParameterUsage(TR_BitVector **registerUsageInfo, int32_t blockNum)
   {
   TR::ResolvedMethodSymbol             *bodySymbol = self()->comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>  paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol               *paramCursor;

   for (paramCursor = paramIterator.getFirst();
       paramCursor != NULL;
       paramCursor = paramIterator.getNext())
      {
      int32_t ai = paramCursor->getAllocatedIndex();
      int32_t ai_l = paramCursor->getAllocatedLow();

      traceMsg(self()->comp(), "found %d %d used as parms\n", ai, ai_l);
      if (ai > 0)
         registerUsageInfo[blockNum]->set(ai);
      if (ai_l > 0)
         registerUsageInfo[blockNum]->set(ai_l);
      }
   }

void
OMR::Power::CodeGenerator::updateSnippetMapWithRSD(TR::Instruction *instr, int32_t rsd)
   {
   // At this point, instr is a branch instruction
   // determine if it branches to a snippet and if so
   // mark the maps in the snippet with the right rsd
   //
   TR::PPCLabelInstruction *labelInstr = (TR::PPCLabelInstruction *)instr;
   TR::LabelSymbol *targetLabel = labelInstr->getLabelSymbol();

   TR_PPCOutOfLineCodeSection *oiCursor = self()->findOutLinedInstructionsFromLabel(targetLabel);
   if (oiCursor)
      {
      TR::Instruction *cur = oiCursor->getFirstInstruction();
      TR::Instruction *end = oiCursor->getAppendInstruction();
      traceMsg(self()->comp(), "found oi start %p end %p\n", cur, end);
      while (cur != end)
         {
         if (cur->needsGCMap())
            {
            TR_GCStackMap *map = cur->getGCMap();
            if (map)
               {
               traceMsg(self()->comp(), "      instr %p rsd %x map %p\n", cur, rsd, map);
               map->setRegisterSaveDescription(rsd);
               }
            }
         cur = cur->getNext();
         }
      }
   }

bool
OMR::Power::CodeGenerator::isTargetSnippetOrOutOfLine(TR::Instruction *instr, TR::Instruction **start, TR::Instruction **end)
   {
   TR::PPCLabelInstruction *labelInstr = (TR::PPCLabelInstruction *)instr;
   TR::LabelSymbol *targetLabel = labelInstr->getLabelSymbol();
   TR_PPCOutOfLineCodeSection *oiCursor = self()->findOutLinedInstructionsFromLabel(targetLabel);
   if (oiCursor)
      {
      *start = oiCursor->getFirstInstruction();
      *end = oiCursor->getAppendInstruction();
      return true;
      }
   else
      return false;
   }

bool OMR::Power::CodeGenerator::supportsAESInstructions()
   {
    if ( TR::Compiler->target.cpu.getPPCSupportsAES() && !self()->comp()->getOptions()->getOption(TR_DisableAESInHardware))
      return true;
    else
      return false;
   }

TR::CodeGenerator::TR_RegisterPressureSummary *
OMR::Power::CodeGenerator::calculateRegisterPressure()
   {
   return NULL;
   }


//FIXME: should this be common code in CodeGen?
void OMR::Power::CodeGenerator::deleteInst(TR::Instruction* old)
   {
   TR::Instruction* prv = old->getPrev();
   TR::Instruction* nxt = old->getNext();
   prv->setNext(nxt);
   nxt->setPrev(prv);
   }

TR::Linkage *
OMR::Power::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage = NULL;

   switch (lc)
      {
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::PPCSystemLinkage(self());
         break;

      default:
         linkage = new (self()->trHeapMemory()) TR::PPCSystemLinkage(self());
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

void OMR::Power::CodeGenerator::generateBinaryEncodingPrologue(
      TR_PPCBinaryEncodingData *data)
   {
   TR::Compilation *comp = self()->comp();
   data->recomp = NULL;
   data->cursorInstruction = self()->getFirstInstruction();
   data->preProcInstruction = data->cursorInstruction;

   data->jitTojitStart = data->cursorInstruction;
   data->cursorInstruction = NULL;

   self()->getLinkage()->loadUpArguments(data->cursorInstruction);

   data->cursorInstruction = self()->getFirstInstruction();

   while (data->cursorInstruction && data->cursorInstruction->getOpCodeValue() != TR::InstOpCode::proc)
      {
      data->estimate          = data->cursorInstruction->estimateBinaryLength(data->estimate);
      data->cursorInstruction = data->cursorInstruction->getNext();
      }

   int32_t boundary = comp->getOptions()->getJitMethodEntryAlignmentBoundary(self());
   if (boundary && (boundary > 4) && ((boundary & (boundary - 1)) == 0))
      {
      comp->getOptions()->setJitMethodEntryAlignmentBoundary(boundary);
      self()->setPreJitMethodEntrySize(data->estimate);
      data->estimate += (boundary - 4);
      }

   self()->getLinkage()->createPrologue(data->cursorInstruction);
   }

void OMR::Power::CodeGenerator::doBinaryEncoding()
   {
   TR_PPCBinaryEncodingData data;
   data.estimate = 0;

   generateBinaryEncodingPrologue(&data);

   bool skipOneReturn = false;
   while (data.cursorInstruction)
      {
      if (data.cursorInstruction->getOpCodeValue() == TR::InstOpCode::ret)
         {
         if (skipOneReturn == false)
            {
            TR::Instruction *temp = data.cursorInstruction->getPrev();
            self()->getLinkage()->createEpilogue(temp);
            if (temp->getNext() != data.cursorInstruction)
               skipOneReturn = true;
            data.cursorInstruction = temp->getNext();
            }
         else
            {
            skipOneReturn = false;
            }
         }
      data.estimate          = data.cursorInstruction->estimateBinaryLength(data.estimate);
      data.cursorInstruction = data.cursorInstruction->getNext();
      }

   data.estimate = self()->setEstimatedLocationsForSnippetLabels(data.estimate);
   if (data.estimate > 32768)
      {
      data.estimate = identifyFarConditionalBranches(data.estimate, self());
      }

   self()->setEstimatedCodeLength(data.estimate);

   data.cursorInstruction = self()->getFirstInstruction();
   uint8_t *coldCode = NULL;
   uint8_t *temp = self()->allocateCodeMemory(self()->getEstimatedCodeLength(), 0, &coldCode);

   self()->setBinaryBufferStart(temp);
   self()->setBinaryBufferCursor(temp);
   self()->alignBinaryBufferCursor();

   TR::Instruction *nop;
   TR::Register *gr1 = self()->machine()->getPPCRealRegister(TR::RealRegister::gr1);
   bool skipLabel = false;
   static int count=0;

   bool  isPrivateLinkage = (self()->comp()->getJittedMethodSymbol()->getLinkageConvention() == TR_Private);

   uint8_t *start = temp;

   while (data.cursorInstruction)
      {
      if(data.cursorInstruction->isLabel())
         {
         if ((data.cursorInstruction)->isNopCandidate())
            {
            uintptrj_t uselessFetched;

            uselessFetched = ((uintptrj_t)self()->getBinaryBufferCursor()/4)%8;

            if (uselessFetched >= 8 - data.cursorInstruction->MAX_LOOP_ALIGN_NOPS())
               {
               if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
                  {
                  nop = self()->generateNop(data.cursorInstruction->getNode(), data.cursorInstruction->getPrev(), TR_NOPEndGroup); // handles P6, P7
                  nop->setNext(data.cursorInstruction);
                  data.cursorInstruction = nop;
                  uselessFetched++;
                  }
               for (; uselessFetched < 8; uselessFetched++)
                  {
                  nop = self()->generateNop(data.cursorInstruction->getNode(), data.cursorInstruction->getPrev(), TR_NOPStandard);
                  nop->setNext(data.cursorInstruction);
                  data.cursorInstruction = nop;
                  }
               skipLabel = true;
               }
            }
         else
            {
            skipLabel = false;
            }
         }

      self()->setBinaryBufferCursor(data.cursorInstruction->generateBinaryEncoding());

      self()->addToAtlas(data.cursorInstruction);

      if (data.cursorInstruction == data.preProcInstruction)
         {
         self()->setPrePrologueSize(self()->getBinaryBufferCursor() - self()->getBinaryBufferStart() - self()->getJitMethodEntryPaddingSize());
         self()->comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()->getSymbol()->getStaticSymbol()->setStaticAddress(self()->getBinaryBufferCursor());
         }

      data.cursorInstruction = data.cursorInstruction->getNext();

      if (isPrivateLinkage && data.cursorInstruction==data.jitTojitStart)
         {
         uint32_t magicWord = ((self()->getBinaryBufferCursor()-self()->getCodeStart())<<16) | static_cast<uint32_t>(self()->comp()->getReturnInfo());

         *(uint32_t *)(data.preProcInstruction->getBinaryEncoding()) = magicWord;

#ifdef J9_PROJECT_SPECIFIC
         if (data.recomp!=NULL && data.recomp->couldBeCompiledAgain())
            {
            TR_LinkageInfo *lkInfo = TR_LinkageInfo::get(self()->getCodeStart());
            if (data.recomp->useSampling())
               lkInfo->setSamplingMethodBody();
            else
               lkInfo->setCountingMethodBody();
            }
#endif

         toPPCImmInstruction(data.preProcInstruction)->setSourceImmediate(*(uint32_t *)(data.preProcInstruction->getBinaryEncoding()));
         }
      }

   // We late-processing TOC entries here: obviously cannot deal with snippet labels.
   // If needed, we should move this step to common code around relocation processing.
   if (TR::Compiler->target.is64Bit())
      {
      int32_t idx;
      for (idx=0; idx<self()->getTrackItems()->size(); idx++)
         TR_PPCTableOfConstants::setTOCSlot(self()->getTrackItems()->element(idx)->getTOCOffset(),
            (uintptrj_t)self()->getTrackItems()->element(idx)->getLabel()->getCodeLocation());
      }

   // Create exception table entries for outlined instructions.
   //
   if (!self()->comp()->getOption(TR_DisableOOL))
      {
      auto oiIterator = self()->getPPCOutOfLineCodeSectionList().begin();
      while (oiIterator != self()->getPPCOutOfLineCodeSectionList().end())
         {
         uint32_t startOffset = (*oiIterator)->getFirstInstruction()->getBinaryEncoding() - self()->getCodeStart();
         uint32_t endOffset   = (*oiIterator)->getAppendInstruction()->getBinaryEncoding() - self()->getCodeStart();

         TR::Block * block = (*oiIterator)->getBlock();
         bool needsETE = (*oiIterator)->getFirstInstruction()->getNode()->getOpCode().hasSymbolReference() &&
                         (*oiIterator)->getFirstInstruction()->getNode()->getSymbolReference() &&
                         (*oiIterator)->getFirstInstruction()->getNode()->getSymbolReference()->canCauseGC();

         if (needsETE && block && !block->getExceptionSuccessors().empty())
            block->addExceptionRangeForSnippet(startOffset, endOffset);

         ++oiIterator;
         }
      }
   }

// different from evaluate in that it returns a clobberable register
TR::Register *OMR::Power::CodeGenerator::gprClobberEvaluate(TR::Node *node)
   {

   TR::Register *resultReg = self()->evaluate(node);
   TR_ASSERT( resultReg->getKind() == TR_GPR || resultReg->getKind() == TR_FPR , "gprClobberEvaluate() called for non-GPR or non-FPR.");

   if (!self()->canClobberNodesRegister(node))
      {
      if (TR::Compiler->target.is32Bit() && node->getType().isInt64())
         {
         TR::Register     *lowReg  = self()->allocateRegister();
         TR::Register     *highReg = self()->allocateRegister();
         TR::RegisterPair *longReg = self()->allocateRegisterPair(lowReg, highReg);

         generateTrg1Src1Instruction(self(), TR::InstOpCode::mr, node,
                                     lowReg,
                                     resultReg->getLowOrder());

         generateTrg1Src1Instruction(self(), TR::InstOpCode::mr, node,
                                     highReg,
                                     resultReg->getHighOrder());
         return longReg;
         }
      else
         {
         TR::Register *targetReg = resultReg->containsCollectedReference() ? self()->allocateCollectedReferenceRegister() : self()->allocateRegister(resultReg->getKind());

         if (resultReg->containsInternalPointer())
            {
            targetReg->setContainsInternalPointer();
            targetReg->setPinningArrayPointer(resultReg->getPinningArrayPointer());
            }

         generateTrg1Src1Instruction(self(), resultReg->getKind() == TR_GPR ? TR::InstOpCode::mr : TR::InstOpCode::fmr, node, targetReg, resultReg);
         return targetReg;
         }
      }
   else
      {
      return resultReg;
      }
   }


static int32_t identifyFarConditionalBranches(int32_t estimate, TR::CodeGenerator *cg)
   {
   TR_Array<TR::PPCConditionalBranchInstruction *> candidateBranches(cg->trMemory(), 256);
   TR::Instruction *cursorInstruction = cg->getFirstInstruction();

   while (cursorInstruction)
      {
      TR::PPCConditionalBranchInstruction *branch = cursorInstruction->getPPCConditionalBranchInstruction();
      if (branch != NULL)
         {
         if (abs(branch->getEstimatedBinaryLocation() - branch->getLabelSymbol()->getEstimatedCodeLocation()) > 16384)
            {
            candidateBranches.add(branch);
            }
         }
      cursorInstruction = cursorInstruction->getNext();
      }

   // The following heuristic algorithm penalizes backward branches in
   // estimation, since it should be rare that a backward branch needs
   // a far relocation.

   for (int32_t i=candidateBranches.lastIndex(); i>=0; i--)
      {
      int32_t myLocation=candidateBranches[i]->getEstimatedBinaryLocation();
      int32_t targetLocation=candidateBranches[i]->getLabelSymbol()->getEstimatedCodeLocation();
      int32_t  j;

      if (targetLocation >= myLocation)
         {
         for (j=i+1; j<candidateBranches.size() &&
                     targetLocation>candidateBranches[j]->getEstimatedBinaryLocation();
              j++)
            ;

         // We might be branching to a Interface snippet which might contain a 4
         // byte alignment in 64bit. This means we need to add 4 to the target
         // in order to be conservatively correct for the offset calculation
         // We could improve this by only adding 4 when the target is a Interface Call Snippet
         if (TR::Compiler->target.is64Bit())
            targetLocation+=4;
         if ((targetLocation-myLocation + (j-i-1)*4) >= 32768)
            {
            candidateBranches[i]->setFarRelocation(true);
            }
         else
            {
            candidateBranches.remove(i);
            }
         }
      else    // backward branch
         {
         for (j=i-1; j>=0 && targetLocation<=candidateBranches[j]->getEstimatedBinaryLocation(); j--)
            ;
         if ((myLocation-targetLocation + (i-j-1)*4) > 32768)
            {
            candidateBranches[i]->setFarRelocation(true);
            }
         else
            {
            candidateBranches.remove(i);
            }
         }
      }
      return(estimate+4*candidateBranches.size());
   }

bool OMR::Power::CodeGenerator::canTransformUnsafeSetMemory()
   {
   return (TR::Compiler->target.is64Bit());
   }

void OMR::Power::CodeGenerator::buildRegisterMapForInstruction(TR_GCStackMap *map)
   {
   TR_InternalPointerMap *internalPtrMap = NULL;
   TR::GCStackAtlas *atlas = self()->getStackAtlas();

   // Build the register map
   //
   for (int i=TR::RealRegister::FirstGPR;
            i<=TR::RealRegister::LastGPR; i++)
      {
      TR::RealRegister *realReg = self()->machine()->getPPCRealRegister(
              (TR::RealRegister::RegNum)i);

      if (realReg->getHasBeenAssignedInMethod())
         {
         TR::Register *virtReg = realReg->getAssignedRegister();
         if (virtReg)
            {
            if (virtReg->containsInternalPointer())
               {
               if (!internalPtrMap)
                  internalPtrMap = new (self()->trHeapMemory()) TR_InternalPointerMap(self()->trMemory());
               internalPtrMap->addInternalPointerPair(virtReg->getPinningArrayPointer(), 31 - (i-1));
               atlas->addPinningArrayPtrForInternalPtrReg(virtReg->getPinningArrayPointer());
               }
            else if (virtReg->containsCollectedReference())
               map->setRegisterBits(TR::CodeGenerator::registerBitMask(i));
            }
         }
      }

   map->setInternalPointerMap(internalPtrMap);
   }


int32_t OMR::Power::CodeGenerator::findOrCreateFloatConstant(void *v, TR::DataType t,
                             TR::Instruction *n0, TR::Instruction *n1,
                             TR::Instruction *n2, TR::Instruction *n3)
   {
   if (_constantData == NULL)
      _constantData = new (self()->trHeapMemory()) TR::ConstantDataSnippet(self());
   return(_constantData->addConstantRequest(v, t, n0, n1, n2, n3, NULL, false));
   }

int32_t OMR::Power::CodeGenerator::findOrCreateAddressConstant(void *v, TR::DataType t,
                             TR::Instruction *n0, TR::Instruction *n1,
                             TR::Instruction *n2, TR::Instruction *n3,
                             TR::Node *node, bool isUnloadablePicSite)
   {
   if (_constantData == NULL)
      _constantData = new (self()->trHeapMemory()) TR::ConstantDataSnippet(self());
   return(_constantData->addConstantRequest(v, t, n0, n1, n2, n3, node, isUnloadablePicSite));
   }

bool OMR::Power::CodeGenerator::isSnippetMatched(TR::Snippet *snippet, int32_t snippetKind, TR::SymbolReference *symRef)
   {
   if ((int32_t)(snippet->getKind()) != snippetKind)
      return false;

   switch (snippetKind)
      {
      case TR::Snippet::IsHelperCall:
         {
         TR::PPCHelperCallSnippet *helperSnippet = (TR::PPCHelperCallSnippet *)snippet;
         if (helperSnippet->getRestartLabel()==NULL && helperSnippet->getDestination()==symRef)
            return true;
         return false;
         }
      case TR::Snippet::IsForceRecompilation:
      case TR::Snippet::IsAllocPrefetch:
      case TR::Snippet::IsNonZeroAllocPrefetch:
         return true;
      default:
         return false;
      }
   }

bool OMR::Power::CodeGenerator::hasDataSnippets()
   {
   return (_constantData==NULL)?false:true;
   }

void OMR::Power::CodeGenerator::emitDataSnippets()
   {
   self()->setBinaryBufferCursor(_constantData->emitSnippetBody());
   }

int32_t OMR::Power::CodeGenerator::setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart)
   {
   return estimatedSnippetStart+_constantData->getLength();
   }

inline static bool callInTree(TR::TreeTop *treeTop)
   {
   TR::Node      *node = treeTop->getNode();
   TR::ILOpCodes  l1OpCode = node->getOpCodeValue();

   // Cover instanceOf and those NEW opCodes
   if (l1OpCode == TR::treetop || l1OpCode == TR::ificmpeq || l1OpCode == TR::ificmpne)
      l1OpCode = node->getFirstChild()->getOpCodeValue();

   if (l1OpCode == TR::monent          ||
       l1OpCode == TR::monexit         ||
       l1OpCode == TR::checkcast       ||
       l1OpCode == TR::instanceof      ||
       l1OpCode == TR::ArrayStoreCHK   ||
       l1OpCode == TR::MethodEnterHook ||
       l1OpCode == TR::MethodExitHook  ||
       l1OpCode == TR::New             ||
       l1OpCode == TR::newarray        ||
       l1OpCode == TR::anewarray       ||
       l1OpCode == TR::multianewarray  ||
       l1OpCode == TR::MergeNew)
      return(true);

   return(node->getNumChildren()!=0 && node->getFirstChild()->getOpCode().isCall() &&
          node->getFirstChild()->getOpCodeValue()!=TR::arraycopy);
   }

TR_BitVector *OMR::Power::CodeGenerator::computeCallInfoBitVector()
   {
   uint32_t         bcount = self()->comp()->getFlowGraph()->getNextNodeNumber();
   TR_BitVector     bvec, *ebvec = new (self()->trHeapMemory()) TR_BitVector(bcount, self()->trMemory());
   TR::TreeTop      *pTree, *exitTree;
   TR::Block        *block;
   uint32_t         bnum, btemp;

   bvec.init(bcount, self()->trMemory());

   for (pTree=self()->comp()->getStartTree(); pTree!=NULL; pTree=exitTree->getNextTreeTop())
      {
      block = pTree->getNode()->getBlock();
      exitTree = block->getExit();
      bnum = block->getNumber();
      TR_ASSERT(bnum<bcount, "Block index must be less than the total number of blocks.\n");

      while (pTree != exitTree)
         {
         if (callInTree(pTree))
            {
            bvec.set(bnum);
            break;
            }
         pTree = pTree->getNextTreeTop();
         }
      if (pTree==exitTree && callInTree(pTree))
         {
         bvec.set(bnum);
         }
      }

   for (pTree=self()->comp()->getStartTree(); pTree!=NULL; pTree=exitTree->getNextTreeTop())
      {
      block = pTree->getNode()->getBlock();
      exitTree = block->getExit();
      bnum = block->getNumber();

      TR::Block *eblock = block->startOfExtendedBlock();
      btemp = eblock->getNumber();
      while (!bvec.isSet(btemp))
         {
         eblock = eblock->getNextBlock();
         if (eblock==NULL || !eblock->isExtensionOfPreviousBlock())
            break;
         btemp = eblock->getNumber();
         }
      if (bvec.isSet(btemp))
         ebvec->set(bnum);
      }

   return(ebvec);
   }

void OMR::Power::CodeGenerator::simulateNodeEvaluation(TR::Node * node, TR_RegisterPressureState * state, TR_RegisterPressureSummary * summary)
   {
   TR::Node *immediateChild = (node->getNumChildren() == 2 && !node->getOpCode().isCall() && !node->getOpCode().isLoad())? node->getSecondChild() : NULL;
   if (immediateChild && immediateChild->getOpCode().isLoadConst() &&
      (immediateChild->getType().isIntegral() && !immediateChild->getType().isInt64() || immediateChild->getType().isAddress()))
      {
      self()->simulateSkippedTreeEvaluation(immediateChild, state, summary, 'i');
      self()->simulateDecReferenceCount(immediateChild, state);
      self()->simulateTreeEvaluation(node->getFirstChild(), state, summary);
      self()->simulateDecReferenceCount(node->getFirstChild(), state);
      self()->simulatedNodeState(node)._childRefcountsHaveBeenDecremented = 1;
      self()->simulateNodeGoingLive(node, state);
      }
   else
      {
      OMR::CodeGenerator::simulateNodeEvaluation(node, state, summary);
      }
   }

TR_GlobalRegisterNumber OMR::Power::CodeGenerator::pickRegister(TR_RegisterCandidate              *  regCan,
                                                          TR::Block                          ** barr,
                                                          TR_BitVector                      &  availRegs,
                                                          TR_GlobalRegisterNumber           &  highRegisterNumber,
                                                          TR_LinkHead<TR_RegisterCandidate> *  candidates)
   {
   if (!self()->comp()->getOptions()->getOption(TR_DisableRegisterPressureSimulation))
      return OMR::CodeGenerator::pickRegister(regCan, barr, availRegs, highRegisterNumber, candidates);


   if (self()->getBlockCallInfo() == NULL)
      {
      self()->setBlockCallInfo(self()->computeCallInfoBitVector());
      }

   uint32_t      firstIndex, lastIndex, lastVolIndex, longLow = 0;
   TR::Symbol     *sym = regCan->getSymbolReference()->getSymbol();
   TR_BitVector *ebvec = self()->getBlockCallInfo();
   TR_BitVectorIterator livec(regCan->getBlocksLiveOnEntry());
   bool          hasCallInPath = false, isParm = sym->isParm(), isAddressType=false, isFloatType=false;
   bool isVector  = false;
   switch (sym->getDataType())
      {
      case TR::Float:
      case TR::Double:
         isFloatType = true;
         firstIndex = _firstFPR;
         lastIndex = _lastFPR;
         lastVolIndex = _lastVolatileFPR;
         break;

      case TR::Address:
         isAddressType = true;
         firstIndex = _firstGPR;
         lastIndex  = _lastFPR ;
         lastVolIndex = _lastVolatileGPR;
         break;

      case TR::Int64:
         if (TR::Compiler->target.is32Bit() && highRegisterNumber == 0)
            longLow = 1;
         firstIndex = _firstGPR;
         lastIndex  = _lastFPR ;
         lastVolIndex = _lastVolatileGPR;
         break;

      case TR::VectorInt32:
      case TR::VectorDouble:
          isVector = true;
          firstIndex = self()->getFirstGlobalVRF();
          lastIndex = self()->getLastGlobalVRF();
          lastVolIndex = lastIndex;  // TODO: preserved VRF's !!
         break;

      default:
         firstIndex = _firstGPR;
         lastIndex  = _lastFPR ;
         lastVolIndex = _lastVolatileGPR;
         break;
      }

   bool gprCandidate = true;
   if ((sym->getDataType() == TR::Float) ||
       (sym->getDataType() == TR::Double) ||
       sym->getDataType().isVector())
      gprCandidate = false;

   if (gprCandidate)
      {
      int32_t numExtraRegs = 0;
      int32_t maxRegisterPressure = 0;

      vcount_t visitCount = self()->comp()->incVisitCount();
      int32_t maxFrequency = 0;

      while (livec.hasMoreElements())
         {
         int32_t liveBlockNum = livec.getNextElement();
         TR::Block *block = barr[liveBlockNum];
         if (block->getFrequency() > maxFrequency)
            maxFrequency = block->getFrequency();
         }

      int32_t maxStaticFrequency = 0;
      if (maxFrequency == 0)
         {
         livec.setBitVector(regCan->getBlocksLiveOnEntry());
         while (livec.hasMoreElements())
            {
            int32_t liveBlockNum = livec.getNextElement();
            TR::Block *block = barr[liveBlockNum];
            TR_BlockStructure *blockStructure = block->getStructureOf();
            int32_t blockWeight = 1;
            if (blockStructure &&  !block->isCold())
               {
               blockStructure->calculateFrequencyOfExecution(&blockWeight);
               if (blockWeight > maxStaticFrequency)
                  maxStaticFrequency = blockWeight;
               }
            }
         }

      if (!_assignedGlobalRegisters)
         _assignedGlobalRegisters = new (self()->trStackMemory()) TR_BitVector(self()->comp()->getSymRefCount(), self()->trMemory(), stackAlloc, growable);

      bool vmThreadUsed = false;
      bool assigningEDX = false;

      livec.setBitVector(regCan->getBlocksLiveOnEntry());
      while (livec.hasMoreElements())
         {
         int32_t liveBlockNum = livec.getNextElement();
         TR::Block *block = barr[liveBlockNum];

         _assignedGlobalRegisters->empty();
         int32_t numAssignedGlobalRegs = 0;
         TR_RegisterCandidate *prev;
         for (prev = candidates->getFirst(); prev; prev = prev->getNext())
            {
            bool gprCandidate = true;
            if ((prev->getSymbol()->getDataType() == TR::Float) ||
                (prev->getSymbol()->getDataType() == TR::Double) ||
                sym->getDataType().isVector())
               gprCandidate = false;
            if (gprCandidate && prev->getBlocksLiveOnEntry().get(liveBlockNum))
               {
               numAssignedGlobalRegs++;
               if (prev->getDataType() == TR::Int64)
                  numAssignedGlobalRegs++;
               _assignedGlobalRegisters->set(prev->getSymbolReference()->getReferenceNumber());
               }
           }
        maxRegisterPressure = self()->estimateRegisterPressure(block, visitCount, maxStaticFrequency, maxFrequency, vmThreadUsed, numAssignedGlobalRegs, _assignedGlobalRegisters, regCan->getSymbolReference(), assigningEDX);
        if (maxRegisterPressure >= _numGPR)
           break;
        }

     // Determine if we can spare any extra registers for this candidate without spilling
     // in any hot (critical) blocks
     if (maxRegisterPressure < _numGPR)
        numExtraRegs = _numGPR - maxRegisterPressure;

     if (numExtraRegs <= 0 )
        return -1;
     }

  int32_t i1;
  livec.setBitVector(regCan->getBlocksLiveOnEntry());
  while (livec.hasMoreElements())
     {
     i1 = livec.getNextElement();
     if (ebvec->isSet(barr[i1]->getNumber()))
        {
        hasCallInPath = true;
        break;
        }
     }

  if (hasCallInPath)
     {
     i1 = lastVolIndex+1;
     while (i1<=lastIndex && !availRegs.isSet(i1))
        i1++;
     if (i1<=lastIndex)
        return(i1);
     if (isParm)
        {
        i1 = sym->getParmSymbol()->getLinkageRegisterIndex();
        if (i1>=0)
           {
           if (isVector)
              TR_ASSERT(false, "assertion failure");   // TODO
           i1 = (isFloatType?_firstParmFPR:_firstParmGPR) - i1 - longLow;
           if (availRegs.isSet(i1))
              return(i1);
           }
        }
     else
        {
        i1 = firstIndex;
        while (i1<=lastVolIndex && !availRegs.isSet(i1))
           i1++;
        if (i1<lastVolIndex || (i1==_lastVolatileGPR && !isAddressType))
           return(i1);
        }
     return(-1);
     }
  else
     {
     if (isParm)
        {
        i1 = sym->getParmSymbol()->getLinkageRegisterIndex();
        if (i1>=0)
           {
           if (isVector)
              TR_ASSERT(false, "assertion failure");   // TODO
           i1 = (isFloatType?_firstParmFPR:_firstParmGPR) - i1 - longLow;
           if (availRegs.isSet(i1))
              return(i1);
           }
        }

     if (!isParm || sym->getParmSymbol()->getLinkageRegisterIndex()<0)
        {
        i1 = firstIndex;
        while (i1<=lastVolIndex &&
               (!availRegs.isSet(i1) ||
               (isAddressType && i1==_lastVolatileGPR)))
           i1++;

        if (i1<=lastVolIndex)
           return(i1);
        }

     i1 = lastVolIndex+1;
     while (i1<=lastIndex && !availRegs.isSet(i1))
        i1++;
     if (i1<=lastIndex)
        return(i1);
     else
        {
        if (isAddressType && availRegs.isSet(_lastVolatileGPR))
           return(_lastVolatileGPR);
        return(-1);
        }
     }
  }

bool OMR::Power::CodeGenerator::allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *rc, TR::Node * branchNode)
   {
   // If return false, processLiveOnEntryBlocks has to dis-qualify any candidates which are referenced
   // within any CASE of a SWITCH statement.
   return true;
   }

int32_t OMR::Power::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node * node)
   {
   if (node->getOpCode().isSwitch())
      {
      // This code should be separated out in a function in TreeEvaluatore.cpp
      // for better coordination with the evaluator code.
      if (node->getOpCodeValue() == TR::table)
         {
         // Have to be conservative here since we don't know whether it is balanced.
         return _numGPR - 3; // 3 GPRs are needed for binary search
         }
      else
         {
         int32_t total = node->getCaseIndexUpperBound(), ii;
         if (total <= 12)
            {
            for (ii=2; ii<total && node->getChild(ii)->getCaseConstant()>=LOWER_IMMED && node->getChild(ii)->getCaseConstant()<=UPPER_IMMED; ii++)
            ;
            if (ii == total)
               return _numGPR - 1;
            }

         if (total <= 9)
            {
            for (ii=3; ii<total &&
                       (node->getChild(ii)->getCaseConstant()-node->getChild(ii-1)->getCaseConstant())<=UPPER_IMMED; ii++)
            ;
            if (ii == total)
               return _numGPR - 2;
            }

         if (total <= 8)
            return _numGPR - 3;
         return _numGPR - 6;  // Be conservative when we don't know whether it is balanced.
         }
      }
   else if (node->getOpCode().isIf() && node->getFirstChild()->getType().isInt64())
      {
      return ( TR::Compiler->target.is64Bit()? _numGPR : _numGPR - 4 ); // 4 GPRs are needed for long-compare arguments themselves.
      }

   return _numGPR; // pickRegister will ensure that the number of free registers isn't exceeded
   }

int32_t OMR::Power::CodeGenerator::getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node * node)
   {
   return 32; // pickRegister will ensure that the number of free registers isn't exceeded
   }

TR_GlobalRegisterNumber OMR::Power::CodeGenerator::getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type)
   {
   TR_GlobalRegisterNumber result;
   if (type == TR::Float || type == TR::Double)
      {
      if (linkageRegisterIndex >= self()->getProperties()._numFloatArgumentRegisters)
         result = -1;
      else
         result = _fprLinkageGlobalRegisterNumbers[linkageRegisterIndex];
      }
   else if (type.isVector())
      TR_ASSERT(false, "assertion failure");    // TODO
   else
      {
      if (linkageRegisterIndex >= self()->getProperties()._numIntegerArgumentRegisters)
         result = -1;
      else
         result = _gprLinkageGlobalRegisterNumbers[linkageRegisterIndex];
      }
   diagnostic("getLinkageGlobalRegisterNumber(%d, %s) = %d\n", linkageRegisterIndex, self()->comp()->getDebug()->getName(type), result);
   return result;
   }

void OMR::Power::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   TR_ASSERT( label->getCodeLocation(), "Attempt to relocate to a NULL label address!" );

   *cursor |= ((uintptrj_t)label->getCodeLocation()-(uintptrj_t)cursor) & 0x0000fffc;
   }

void OMR::Power::CodeGenerator::apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   TR_ASSERT( label->getCodeLocation(), "Attempt to relocate to a NULL label address!" );

   *cursor = ((*cursor) & 0xfc000003) | (((uintptrj_t)label->getCodeLocation()-(uintptrj_t)cursor) & 0x03fffffc);
   }

void OMR::Power::CodeGenerator::apply16BitLoadLabelRelativeRelocation(TR::Instruction *liInstruction, TR::LabelSymbol *startLabel, TR::LabelSymbol *endLabel, int32_t deltaToStartLabel)
   {
   // Patch up the mainline code that stores an offset value into a register using:
   // li   addrReg, Value & 0x0000ffff

   TR::Instruction *liPPCInstruction = liInstruction;
   TR_ASSERT(liPPCInstruction->getOpCodeValue() == TR::InstOpCode::li,"wrong load instruction used for apply32BitLoadLabelRelativeRelocation\n");

   int32_t distance = int32_t((endLabel->getCodeLocation() - startLabel->getCodeLocation())/4);
   TR_ASSERT(!((endLabel->getCodeLocation() - startLabel->getCodeLocation())%4),"misaligned code");
   TR_ASSERT(!(distance & 0xffff0000), "relative label does not fit in 16-bit li instruction");

   int32_t *patchAddr = (int32_t *)(liPPCInstruction->getBinaryEncoding());

   TR_ASSERT((*(int32_t *)patchAddr & 0x0000ffff) == 0,"offset should be 0 to start\n");

   // This type of relocation is used in some projects so an exception handler can get the return address off of the stack
   // and then use this return address and the offset loaded here to find an exit label in the generated code.
   // If the register deps are not done properly then the register assigner may insert instructions after the call but before
   // the startLabel. If this bad register motion occurs then the loaded offset would be wrong (when used in this way)
   // and if an exception occurs we may end up with a wild branch.
   // This would be a very tough bug to find and fix so it is worth some ugliness in the function interface.
   *patchAddr |= distance & 0x0000ffff;
   }

void OMR::Power::CodeGenerator::apply64BitLoadLabelRelativeRelocation(TR::Instruction *lastInstruction, TR::LabelSymbol *label)
   {
   // Patch up the mainline code that materializes a base register and does a load with update from this base register
   // This is the instruction sequence generated by fixedSeqMemAccess
   // hiAddr = HI_VALUE(addrValue)
   // patchAddr-4   lis    tempReg, hiAddr>>32
   // patchAddr-3   lis    baseReg, hiAddr & 0x0000FFFF
   // patchAddr-2   ori    tempReg, tempReg, (hiAddr>>16) & 0x0000ffff
   // patchAddr-1   rldimi baseReg, tempReg, 32, 0xffffFFFF00000000  (not patched)
   // patchAddr     ldu    trgReg, [baseReg+addrValue]  <-- lastInstruction

   TR::Instruction *lastPPCInstruction = lastInstruction;
   TR_ASSERT((lastPPCInstruction->getOpCodeValue() == TR::InstOpCode::ldu || lastPPCInstruction->getOpCodeValue() == TR::InstOpCode::ld) &&
           lastPPCInstruction->getPrev()->getPrev()->getOpCodeValue() == TR::InstOpCode::ori &&
           lastPPCInstruction->getPrev()->getPrev()->getPrev()->getOpCodeValue() == TR::InstOpCode::lis &&
           lastPPCInstruction->getPrev()->getPrev()->getPrev()->getPrev()->getOpCodeValue() == TR::InstOpCode::lis,
           "fixedSeqMemAccess instruction sequence has changed.\n");

   int32_t  *patchAddr = (int32_t *)(lastPPCInstruction->getBinaryEncoding());
   intptr_t addrValue = (intptr_t)label->getCodeLocation();
   *patchAddr |= addrValue & 0x0000ffff;
   addrValue = HI_VALUE(addrValue);
   *(patchAddr-2) |= (addrValue>>16) & 0x0000ffff;
   *(patchAddr-3) |= addrValue & 0x0000ffff;
   *(patchAddr-4) |= (addrValue>>32) & 0x0000ffff;
   }


static int32_t getAssociationIndex(TR::Register *reg, TR::RealRegister::RegNum realNum)
   {
   int32_t  associationIndex, maxIndex=32;

   switch (reg->getKind())
      {
      case TR_GPR:
         associationIndex = realNum - TR::RealRegister::NoReg;
         break;
      case TR_FPR:
         associationIndex = realNum - TR::RealRegister::LastGPR;
         break;
      case TR_CCR:
         associationIndex = realNum - TR::RealRegister::LastVSR;
         maxIndex = 8;
         break;
      default:
         TR_ASSERT(0, "Unrecognized register kind.\n");
      }
   TR_ASSERT(associationIndex>=1 && associationIndex<=maxIndex, "Invalid real register to be associated.\n");
   return associationIndex;
   }


void OMR::Power::CodeGenerator::setRealRegisterAssociation(TR::Register     *reg,
                             TR::RealRegister::RegNum  realNum)
   {
   if (!reg->isLive() || realNum == TR::RealRegister::NoReg)
      return;
   TR::RealRegister *realReg = self()->machine()->getPPCRealRegister(realNum);
   self()->getLiveRegisters(reg->getKind())->setAssociation(reg, realReg);
   }


void OMR::Power::CodeGenerator::addRealRegisterInterference(TR::Register    *reg,
              TR::RealRegister::RegNum  realNum)
   {
   if (!reg->isLive() || realNum == TR::RealRegister::NoReg)
      return;
   TR::RealRegister *realReg = self()->machine()->getPPCRealRegister(realNum);
   reg->getLiveRegisterInfo()->addInterference(realReg->getRealRegisterMask());
   }

#if defined(AIXPPC)
#include <unistd.h>
class  TR_Method;
FILE                     *j2Profile;
static TR::Instruction    *nextIntervalInstructionPtr;
static uint8_t           *nextIntervalBufferPtr;
static bool               segmentInBlock;

uintptrj_t
j2Prof_startInterval(int32_t *preambLen, int32_t *ipAssistLen, int32_t *prologueLen, TR::Compilation *comp)
   {
   TR::CodeGenerator *cg = comp->cg();
   int32_t         prePrologue = cg->getPrePrologueSize();
   TR::Instruction *currentIntervalStart;
   uint8_t        *intervalBinaryStart;

   segmentInBlock = false;
   *preambLen = prePrologue;

   int32_t  *sizePtr = (int32_t *)(cg->getBinaryBufferStart()+prePrologue-4);
   *ipAssistLen = ((*sizePtr)>>16) & 0x0000ffff;

   currentIntervalStart = cg->getFirstInstruction();
   while (currentIntervalStart->getOpCodeValue() != TR::InstOpCode::proc)
      currentIntervalStart = currentIntervalStart->getNext();
   intervalBinaryStart = currentIntervalStart->getBinaryEncoding();
   while (currentIntervalStart!=NULL &&
          (currentIntervalStart->getOpCodeValue()!=TR::InstOpCode::fence ||
           currentIntervalStart->getNode()==NULL ||
           currentIntervalStart->getNode()->getOpCodeValue()!=TR::BBStart))
      currentIntervalStart = currentIntervalStart->getNext();

   nextIntervalInstructionPtr = currentIntervalStart;
   nextIntervalBufferPtr = (currentIntervalStart == NULL)?cg->getBinaryBufferCursor():
                                                          currentIntervalStart->getBinaryEncoding();
   *prologueLen = nextIntervalBufferPtr - intervalBinaryStart;
   return (uintptrj_t)cg->getBinaryBufferStart();
   }

uint32_t
j2Prof_nextInterval(TR::Compilation *comp)
   {
   TR::CodeGenerator *cg = comp->cg();
   TR::Instruction *currentIntervalStart;
   uint8_t        *intervalBinaryStart;
   static uint32_t        remainInBlock;

   if (segmentInBlock)
      {
      if (remainInBlock > 120)
         {
         remainInBlock -= 80;
         return 80;
         }
      segmentInBlock = false;
      return remainInBlock;
      }

   currentIntervalStart = nextIntervalInstructionPtr;
   if (currentIntervalStart == NULL)
      {
      if (nextIntervalBufferPtr == cg->getBinaryBufferCursor())
         return 0;
      else
         {
         uint32_t retVal = cg->getBinaryBufferCursor()-nextIntervalBufferPtr;
         nextIntervalBufferPtr += retVal;
         return retVal;
         }
      }

   intervalBinaryStart = currentIntervalStart->getBinaryEncoding();
   TR::Instruction    *nextBBend = currentIntervalStart;
   uint32_t           intervalLen = 0;
   while (nextBBend->getNext()!=NULL && intervalLen==0)
      {
      nextBBend = nextBBend->getNext();
      while (nextBBend->getNext()!=NULL &&
             (nextBBend->getOpCodeValue()!=TR::InstOpCode::fence ||
              nextBBend->getNode()==NULL ||
              nextBBend->getNode()->getOpCodeValue()!=TR::BBEnd))
         nextBBend = nextBBend->getNext();
      intervalLen = nextBBend->getBinaryEncoding()-intervalBinaryStart;
      }

   nextIntervalInstructionPtr = nextBBend->getNext();
   nextIntervalBufferPtr += intervalLen;

   // Ditch out the remaining generated code
   if (intervalLen!=0)
      {
      if (intervalLen > 120)
         {
         segmentInBlock = true;
         remainInBlock = intervalLen - 80;
         return 80;
         }
      return intervalLen;
      }
   nextIntervalBufferPtr = cg->getBinaryBufferCursor();
   return (uint32_t)(nextIntervalBufferPtr-intervalBinaryStart);
   }

int32_t j2Prof_initialize()
   {
   char processId[16];
   char fname[32];

   sprintf(processId, "%d", (int32_t)getpid());
   strcpy(fname, "profile.");
   strcat(fname, processId);
   j2Profile = fopen(fname, "w");
   if (j2Profile == NULL)
      return(0);
   return(1);
   }

void j2Prof_methodReport(TR_Method *method, TR::Compilation *comp)
   {
   char                  buffer[1024];
   int32_t               len, plen, ilen, prolen;
   uintptrj_t             bufferStart, bufferPtr;
   char                 *classFile;

   bufferStart = bufferPtr = j2Prof_startInterval(&plen, &ilen, &prolen, comp);
   fprintf(j2Profile, "# {\n");
   fprintf(j2Profile, "0x%p+0x%04x\n", bufferPtr, plen);
   bufferPtr += plen;
   fprintf(j2Profile, "0x%p+0x%04x\n", bufferPtr, ilen);
   bufferPtr += ilen;
   fprintf(j2Profile, "0x%p+0x%04x\n", bufferPtr, prolen);
   bufferPtr += prolen;

   while ((len = j2Prof_nextInterval(comp)) != 0)
      {
      fprintf(j2Profile, "0x%p+0x%04x\n", bufferPtr, len);
      bufferPtr += len;
      }

   len = method->classNameLength();
   if (len >= 1024)
      len = 1023;
   memcpy(buffer, method->classNameChars(), len);
   buffer[len] = '\0';

   len = len - 1;
   while (len>=0 && buffer[len]!='/')
      len -= 1;
   classFile = &buffer[len+1];
   fprintf(j2Profile, "# FILE\t%s.java\tCLASS\t%s\t", classFile, buffer);

   len = method->nameLength();
   if (len >= 1024)
      len = 1023;
   memcpy(buffer, method->nameChars(), len);
   buffer[len] = '\0';
   fprintf(j2Profile, "METHOD\t%s\t", buffer);

   len = method->signatureLength();
   if (len >= 1024)
      len = 1023;
   memcpy(buffer, method->signatureChars(), len);
   buffer[len] = '\0';
   fprintf(j2Profile, "SIGNATURE\t%s\tSTART=0x%p\tEND=0x%p\tHOTNESS\t", buffer, bufferStart, bufferPtr);

   fprintf(j2Profile, "%s%s\n",
           comp->getMethodHotness(),
           comp->isProfilingCompilation() ? "/profiled" : "");

   fprintf(j2Profile, "# }\n");
   }

void j2Prof_thunkReport(uint8_t *startP, uint8_t *endP, uint8_t *sig)
   {
   static uint32_t thunkCount;

   fprintf(j2Profile, "# {\n");
   fprintf(j2Profile, "0x%08x+0x%04x\n", (uintptrj_t)startP, (uint16_t)(endP-startP));
   fprintf(j2Profile, "# FILE\t__thunk__.java\tCLASS\t__thunk__\tMETHOD\t");
   fprintf(j2Profile, "thunk_%d\t", thunkCount++);
   if (sig == NULL) {
      sig = (uint8_t *)"no_signature";
   }
   fprintf(j2Profile, "SIGNATURE\t%s\tSTART=0x%p\tEND=0x%p\n", sig, startP, endP);

   fprintf(j2Profile, "# }\n");
   }

void j2Prof_trampolineReport(uint8_t *startP, uint8_t *endP, int32_t num_trampoline)
   {
   int32_t  tindex, len;

#if defined(TR_TARGET_64BIT)
   len = 12;
#else
   len = 16;
#endif

   fprintf(j2Profile, "%% {\n");
   fprintf(j2Profile, "%% JIT Library Area1 : 0x%p - 0x%p\n", startP, endP);

   for (tindex=1; tindex<num_trampoline; tindex++)
      {
      intptrj_t tstart = (intptrj_t)startP + len*(tindex-1);
      fprintf(j2Profile, "%% trampoline%d : 0x%p - 0x%p\n", tindex, tstart, tstart+len-1);
      }

   fprintf(j2Profile, "%% }\n");
   }
#endif

#if DEBUG
void OMR::Power::CodeGenerator::dumpDataSnippets(TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   _constantData->print(outFile);
   }
#endif

TR_BackingStore * OMR::Power::CodeGenerator::allocateStackSlot()
   {
   ListElement<TR_BackingStore> * tempElement;
   if (conversionBuffer == NULL)
     {
     conversionBuffer = new (self()->trHeapMemory()) List<TR_BackingStore>(self()->trMemory());
     static const char *TR_ConversionSlots = feGetEnv("TR_ConversionSlots");
     int numSlots = (TR_ConversionSlots == NULL ? 2 : atoi(TR_ConversionSlots));
     for(int i = 0; i < numSlots; i++)
        {
        conversionBuffer->add(NULL);
        }
     tempElement = conversionBuffer->getLastElement();
     tempElement->setNextElement(conversionBuffer->getListHead());
     conversionBufferIt = new (self()->trHeapMemory()) ListIterator<TR_BackingStore>(conversionBuffer);
     }

   conversionBufferIt->getNext();
   tempElement = conversionBufferIt->getCurrentElement();
   if (tempElement->getData() != NULL)
     {
     return tempElement->getData();
     }
   else
     {
     TR::AutomaticSymbol *spillSymbol = TR::AutomaticSymbol::create(self()->trHeapMemory(),TR::Int64,8);
     spillSymbol->setSpillTempAuto();
     self()->comp()->getMethodSymbol()->addAutomatic(spillSymbol);
     return tempElement->setData(new (self()->trHeapMemory()) TR_BackingStore(self()->comp()->getSymRefTab(), spillSymbol, 0));
     }
   }


int32_t OMR::Power::CodeGenerator::getMaximumNumbersOfAssignableGPRs()
   {
   return TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR  + 1;
   }

int32_t OMR::Power::CodeGenerator::getMaximumNumbersOfAssignableFPRs()
   {
   return TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR  + 1;
   }

int32_t OMR::Power::CodeGenerator::getMaximumNumbersOfAssignableVRs()
   {
   return TR::RealRegister::LastVSR - TR::RealRegister::FirstVSR  + 1;
   }

/**
 * \brief
 * Determine if value fits in the immediate field (if any) of instructions that machine provides.
 *
 * \details
 *
 * A node with a large constant can be materialized and left as commoned nodes.
 * Smaller constants can be uncommoned so that they re-materialize every time when needed as a call
 * parameter. This query is platform specific as constant loading can be expensive on some platforms
 * but cheap on others, depending on their magnitude.
 */
bool OMR::Power::CodeGenerator::shouldValueBeInACommonedNode(int64_t value)
   {
   int64_t smallestPos = self()->getSmallestPosConstThatMustBeMaterialized();
   int64_t largestNeg = self()->getLargestNegConstThatMustBeMaterialized();

   return ((value >= smallestPos) || (value <= largestNeg));
   }

bool OMR::Power::CodeGenerator::isRotateAndMask(TR::Node * node)
   {
   if (!node->getOpCode().isAnd())
      return false;

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::ILOpCodes secondOp = secondChild->getOpCodeValue();

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL &&

         ((secondOp == TR::iconst || secondOp == TR::iuconst) &&
          contiguousBits(secondChild->getInt()) &&
          firstChild->getReferenceCount() == 1 &&
          firstChild->getRegister() == NULL &&
          (((firstChild->getOpCodeValue() == TR::imul ||
             firstChild->getOpCodeValue() == TR::iumul) &&
            (firstChild->getSecondChild()->getOpCodeValue() == TR::iconst ||
             firstChild->getSecondChild()->getOpCodeValue() == TR::iuconst) &&
            firstChild->getSecondChild()->getInt() > 0 &&
            isNonNegativePowerOf2(firstChild->getSecondChild()->getInt())) ||
           ((firstChild->getOpCodeValue() == TR::ishr ||
             firstChild->getOpCodeValue() == TR::iushr) &&
            (firstChild->getSecondChild()->getOpCodeValue() == TR::iconst ||
             firstChild->getSecondChild()->getOpCodeValue() == TR::iuconst) &&
            firstChild->getSecondChild()->getInt() > 0 &&
            (firstChild->getOpCodeValue() == TR::iushr ||
             leadingZeroes(secondChild->getInt()) >=
              firstChild->getSecondChild()->getInt())))))
      return true;
   else
      return false;
   }


TR_PPCOutOfLineCodeSection *OMR::Power::CodeGenerator::findOutLinedInstructionsFromLabel(TR::LabelSymbol *label)
   {
   auto oiIterator = self()->getPPCOutOfLineCodeSectionList().begin();
   while (oiIterator != self()->getPPCOutOfLineCodeSectionList().end())
      {
      if ((*oiIterator)->getEntryLabel() == label)
         return *oiIterator;
      ++oiIterator;
      }

   return NULL;
   }

TR::Snippet *OMR::Power::CodeGenerator::findSnippetInstructionsFromLabel(TR::LabelSymbol *label)
   {
   auto snippetIterator = self()->getSnippetList().begin();
   while(snippetIterator != self()->getSnippetList().end())
      {
      if ((*snippetIterator)->getSnippetLabel() == label)
         return *snippetIterator;
      ++snippetIterator;
      }

   return NULL;
   }

bool OMR::Power::CodeGenerator::ilOpCodeIsSupported(TR::ILOpCodes o)
   {
   if (!OMR::CodeGenerator::ilOpCodeIsSupported(o))
      {
      return false;
      }
   else
      {
	   return (_nodeToInstrEvaluators[o] != TR::TreeEvaluator::unImpOpEvaluator);
      }
   }

TR::Instruction *OMR::Power::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond)
   {
   TR::Node *node = cursor->getNode();

   if (delta > UPPER_IMMED || delta < LOWER_IMMED)
      {
      TR::Register *deltaReg = self()->allocateRegister();
      cursor = loadConstant(self(), node, delta, deltaReg, cursor);
      cursor = self()->generateDebugCounterBump(cursor, counter, deltaReg, cond);
      if (cond)
         {
         addDependency(cond, deltaReg, TR::RealRegister::NoReg, TR_GPR, self());
         }
      self()->stopUsingRegister(deltaReg);
      return cursor;
      }

   intptrj_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = self()->allocateRegister();
   TR::Register *counterReg = self()->allocateRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), cursor);
   cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addi, node, counterReg, counterReg, delta, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), counterReg, cursor);
   if (cond)
      {
      uint32_t preCondCursor = cond->getAddCursorForPre();
      uint32_t postCondCursor = cond->getAddCursorForPost();
      addDependency(cond, addrReg, TR::RealRegister::NoReg, TR_GPR, self());
      cond->getPreConditions()->getRegisterDependency(preCondCursor)->setExcludeGPR0();
      cond->getPostConditions()->getRegisterDependency(postCondCursor)->setExcludeGPR0();
      addDependency(cond, counterReg, TR::RealRegister::NoReg, TR_GPR, self());
      }
   self()->stopUsingRegister(addrReg);
   self()->stopUsingRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::Power::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond)
   {
   TR::Node *node = cursor->getNode();
   intptrj_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = self()->allocateRegister();
   TR::Register *counterReg = self()->allocateRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), cursor);
   cursor = generateTrg1Src2Instruction(self(), TR::InstOpCode::add, node, counterReg, counterReg, deltaReg, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), counterReg, cursor);
   if (cond)
      {
      uint32_t preCondCursor = cond->getAddCursorForPre();
      uint32_t postCondCursor = cond->getAddCursorForPost();
      addDependency(cond, addrReg, TR::RealRegister::NoReg, TR_GPR, self());
      cond->getPreConditions()->getRegisterDependency(preCondCursor)->setExcludeGPR0();
      cond->getPostConditions()->getRegisterDependency(postCondCursor)->setExcludeGPR0();
      addDependency(cond, counterReg, TR::RealRegister::NoReg, TR_GPR, self());
      }
   self()->stopUsingRegister(addrReg);
   self()->stopUsingRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::Power::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm)
   {
   TR::Node *node = cursor->getNode();

   if (delta > UPPER_IMMED || delta < LOWER_IMMED)
      {
      TR::Register *deltaReg = srm.findOrCreateScratchRegister();
      cursor = loadConstant(self(), node, delta, deltaReg, cursor);
      cursor = self()->generateDebugCounterBump(cursor, counter, deltaReg, srm);
      srm.reclaimScratchRegister(deltaReg);
      return cursor;
      }

   intptrj_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = srm.findOrCreateScratchRegister();
   TR::Register *counterReg = srm.findOrCreateScratchRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), cursor);
   cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addi, node, counterReg, counterReg, delta, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), counterReg, cursor);
   srm.reclaimScratchRegister(addrReg);
   srm.reclaimScratchRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::Power::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm)
   {
   TR::Node *node = cursor->getNode();
   intptrj_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = srm.findOrCreateScratchRegister();
   TR::Register *counterReg = srm.findOrCreateScratchRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), cursor);
   cursor = generateTrg1Src2Instruction(self(), TR::InstOpCode::add, node, counterReg, counterReg, deltaReg, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node,
                                       new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, self()), counterReg, cursor);
   srm.reclaimScratchRegister(addrReg);
   srm.reclaimScratchRegister(counterReg);
   return cursor;
   }


bool OMR::Power::CodeGenerator::isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt)
   {
   return self()->machine()->getPPCRealRegister((TR::RealRegister::RegNum)self()->getGlobalRegister(i))->getState() == TR::RealRegister::Free;
   }


bool OMR::Power::CodeGenerator::supportsSinglePrecisionSQRT()
   {
   return TR::Compiler->target.cpu.getSupportsHardwareSQRT();
   }


int32_t
OMR::Power::CodeGenerator::getPreferredLoopUnrollFactor()
   {
   return self()->comp()->getMethodHotness() > warm ? (self()->comp()->isOptServer() ? 12 : 8) : 4;
   }

bool
OMR::Power::CodeGenerator::needsNormalizationBeforeShifts()
   {
   return true;
   }

uint32_t
OMR::Power::CodeGenerator::registerBitMask(int32_t reg)
   {
   return 1 << (31 - (reg - TR::RealRegister::FirstGPR));
   }

bool
OMR::Power::CodeGenerator::doRematerialization()
   {
   return true;
   }

void
OMR::Power::CodeGenerator::freeAndResetTransientLongs()
   {
   int32_t num_lReg;

   for (num_lReg=0; num_lReg < _transientLongRegisters.size(); num_lReg++)
      {
      self()->stopUsingRegister(_transientLongRegisters[num_lReg]);
      }

   _transientLongRegisters.setSize(0);
   }




bool OMR::Power::CodeGenerator::getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode, TR::DataType dt)
   {

   // alignment issues
   if (TR::Compiler->target.cpu.id() < TR_PPCp8 &&
       dt != TR::Double &&
       dt != TR::Int64)
      return false;

   if (TR::Compiler->target.cpu.id() >= TR_PPCp8 &&
       (opcode.getOpCodeValue() == TR::vadd || opcode.getOpCodeValue() == TR::vsub) &&
       dt == TR::Int64)
      return true;

   // implemented vector opcodes
   switch (opcode.getOpCodeValue())
      {
      case TR::vadd:
      case TR::vsub:
      case TR::vmul:
      case TR::vdiv:
      case TR::vneg:
         if (dt == TR::Int32 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vrem:
         return false;
      case TR::vload:
      case TR::vloadi:
      case TR::vstore:
      case TR::vstorei:
         if (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vxor:
      case TR::vor:
      case TR::vand:
         if (dt == TR::Int32 || dt == TR::Int64)
            return true;
         else
            return false;
      case TR::vsplats:
      case TR::getvelem:
         if (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vl2vd:
         return true;
      default:
         return false;
      }

   return false;
   }

bool
OMR::Power::CodeGenerator::getSupportsEncodeUtf16LittleWithSurrogateTest()
   {
   return TR::Compiler->target.cpu.getPPCSupportsVSX() &&
          !self()->comp()->getOptions()->getOption(TR_DisableSIMDUTF16LEEncoder);
   }

bool
OMR::Power::CodeGenerator::getSupportsEncodeUtf16BigWithSurrogateTest()
   {
   return TR::Compiler->target.cpu.getPPCSupportsVSX() &&
          !self()->comp()->getOptions()->getOption(TR_DisableSIMDUTF16BEEncoder);
   }


void
OMR::Power::CodeGenerator::addMetaDataForLoadAddressConstantFixed(
      TR::Node *node,
      TR::Instruction *firstInstruction,
      TR::Register *tempReg,
      int16_t typeAddress,
      intptrj_t value)
   {

   bool doAOTRelocation = true;
   TR::Compilation *comp = self()->comp();

   if (tempReg == NULL)
      {
      if (value != 0x0)
         {
         TR_FixedSequenceKind seqKind = fixedSequence1;
         if (typeAddress == -1)
            {
            if (doAOTRelocation)
               self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 firstInstruction,
                                 (uint8_t *)value,
                                 (uint8_t *)seqKind,
                                 TR_FixedSequenceAddress2, self()),
                                 __FILE__,
                                 __LINE__,
                                 node);
            }
         else
            {
            if (typeAddress == TR_DataAddress)
               {
               if (doAOTRelocation)
                  {
                  TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
                  recordInfo->data1 = (uintptr_t)node->getSymbolReference();
                  recordInfo->data2 = (uintptr_t)node->getInlinedSiteIndex();
                  recordInfo->data3 = (uintptr_t)seqKind;
                  self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                    firstInstruction,
                                    (uint8_t *)recordInfo,
                                    TR_DataAddress, self()),
                                    __FILE__,
                                    __LINE__,
                                    node);
                  }
               }
            else
               {
               if (doAOTRelocation)
                  self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                    firstInstruction,
                                    (uint8_t *)value,
                                    (uint8_t *)seqKind,
                                    (TR_ExternalRelocationTargetKind)typeAddress, self()),
                                    __FILE__,
                                    __LINE__,
                                    node);
               }
            }
         }
      }
   else
      {
      if (value != 0x0)
         {
         TR_FixedSequenceKind seqKind = fixedSequence5;
         if (typeAddress == -1)
            {
            if (doAOTRelocation)
               self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 firstInstruction,
                                 (uint8_t *)value,
                                 (uint8_t *)seqKind,
                                 TR_FixedSequenceAddress2, self()),
                                 __FILE__,
                                 __LINE__,
                                 node);
            }
         else
            {
            if (typeAddress == TR_RamMethodSequence) // NOTE: 32 bit changed to use TR_RamMethodSequence for ordered pair, hence, we should check this instead
               {
               if (doAOTRelocation)
                  self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                    firstInstruction,
                                    (uint8_t *)value,
                                    (uint8_t *)seqKind,
                                    TR_RamMethodSequenceReg, self()),
                                    __FILE__,
                                    __LINE__,
                                    node);
               }
            else if (typeAddress == TR_DataAddress)
               {
               if (doAOTRelocation)
                  {
                  TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
                  recordInfo->data1 = (uintptr_t)node->getSymbolReference();
                  recordInfo->data2 = (uintptr_t)node->getInlinedSiteIndex();
                  recordInfo->data3 = (uintptr_t)seqKind;
                  self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                    firstInstruction,
                                    (uint8_t *)recordInfo,
                                    TR_DataAddress, self()),
                                    __FILE__,
                                    __LINE__,
                                    node);
                  }
               }
            else
               {
               if (doAOTRelocation)
                  self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                    firstInstruction,
                                    (uint8_t *)value,
                                    (uint8_t *)seqKind,
                                    (TR_ExternalRelocationTargetKind)typeAddress, self()),
                                    __FILE__,
                                    __LINE__,
                                    node);
               }
            }
         }
      }

   }


TR::Instruction *
OMR::Power::CodeGenerator::loadAddressConstantFixed(
      TR::Node * node,
      intptrj_t value,
      TR::Register *trgReg,
      TR::Instruction *cursor,
      TR::Register *tempReg,
      int16_t typeAddress,
      bool doAOTRelocation)
   {
   TR::Compilation *comp = self()->comp();
   bool isAOT = comp->compileRelocatableCode();

   if (TR::Compiler->target.is32Bit())
      {
      return self()->loadIntConstantFixed(node, (int32_t)value, trgReg, cursor, typeAddress);
      }

   // load a 64-bit constant into a register with a fixed 5 instruction sequence
   TR::Instruction *temp = cursor;
   TR::Instruction *firstInstruction;

   if (cursor == NULL)
      cursor = self()->getAppendInstruction();

   if (tempReg == NULL)
      {
      // lis trgReg, upper 16-bits
      cursor = firstInstruction = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, trgReg, isAOT? 0: (value>>48) , cursor);

      // ori trgReg, trgReg, next 16-bits
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : ((value>>32) & 0x0000ffff), cursor);
      // shiftli trgReg, trgReg, 32
      cursor = generateTrg1Src1Imm2Instruction(self(), TR::InstOpCode::rldicr, node, trgReg, trgReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
      // oris trgReg, trgReg, next 16-bits
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::oris, node, trgReg, trgReg, isAOT ? 0 : ((value>>16) & 0x0000ffff), cursor);
      // ori trgReg, trgReg, last 16-bits
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : (value & 0x0000ffff), cursor);
      }
   else
      {
      // lis tempReg, bits[0-15]
      cursor = firstInstruction = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, tempReg, isAOT ? 0 : value>>48, cursor);

      // lis trgReg, bits[32-47]
      cursor = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, trgReg, isAOT ? 0 : (value>>16) & 0x0000ffff, cursor);
      // ori tempReg, tempReg, bits[16-31]
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, tempReg, tempReg, isAOT ? 0 : (value>>32) & 0x0000ffff, cursor);
      // ori trgReg, trgReg, bits[48-63]
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : value & 0x0000ffff, cursor);
      // rldimi trgReg, tempReg, 32, 0
      cursor = generateTrg1Src1Imm2Instruction(self(), TR::InstOpCode::rldimi, node, trgReg, tempReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
      }

   if (doAOTRelocation)
      {
      self()->addMetaDataForLoadAddressConstantFixed(node, firstInstruction, tempReg, typeAddress, value);
      }

   if (temp == NULL)
      self()->setAppendInstruction(cursor);

   return(cursor);
   }


void
OMR::Power::CodeGenerator::addMetaDataForLoadIntConstantFixed(
      TR::Node *node,
      TR::Instruction *firstInstruction,
      TR::Instruction *secondInstruction,
      int16_t typeAddress,
      int32_t value)
   {

   TR::Compilation *comp = self()->comp();

   if (typeAddress == TR_DataAddress)
      {
      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = (uintptr_t)node->getSymbolReference();
      recordInfo->data2 = node ? (uintptr_t)node->getInlinedSiteIndex() : (uintptr_t)-1;
      recordInfo->data3 = orderedPairSequence2;
      self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)firstInstruction,
                                                                                          (uint8_t *)secondInstruction,
                                                                                          (uint8_t *)recordInfo,
                                                                                          (TR_ExternalRelocationTargetKind)TR_DataAddress, self()),
                           __FILE__, __LINE__, node);
      }
   else if (typeAddress != -1)
      {
      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = (uintptr_t)value;
      recordInfo->data3 = orderedPairSequence2;
      self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)firstInstruction,
                                                                                          (uint8_t *)secondInstruction,
                                                                                          (uint8_t *)recordInfo,
                                                                                          (TR_ExternalRelocationTargetKind)typeAddress, self()),
                           __FILE__, __LINE__, node);
      }

   }


// Load a 32-bit constant into a register with a fixed 2 instruction sequence
//
TR::Instruction *
OMR::Power::CodeGenerator::loadIntConstantFixed(
      TR::Node * node,
      int32_t value,
      TR::Register *trgReg,
      TR::Instruction *cursor,
      int16_t typeAddress)
   {

   TR::Instruction *temp = cursor;
   TR::Instruction *firstInstruction, *secondInstruction;

   TR::Compilation *comp = self()->comp();

   if (cursor == NULL)
      {
      cursor = self()->getAppendInstruction();
      }

   cursor = firstInstruction = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, trgReg, value>>16, cursor);
   cursor = secondInstruction = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, trgReg, trgReg, value&0x0000ffff, cursor);

   self()->addMetaDataForLoadIntConstantFixed(node, firstInstruction, secondInstruction, typeAddress, value);

   if (temp == NULL)
      {
      self()->setAppendInstruction(cursor);
      }

   return(cursor);
   }


void
OMR::Power::CodeGenerator::addMetaDataFor32BitFixedLoadLabelAddressIntoReg(
      TR::Node *node,
      TR::LabelSymbol *label,
      TR::Instruction *firstInstruction,
      TR::Instruction *secondInstruction)
   {
   TR::Compilation *comp = self()->comp();

   TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
   recordInfo->data3 = orderedPairSequence1;

   self()->getAheadOfTimeCompile()->getRelocationList().push_front(new (self()->trHeapMemory()) TR::PPCPairedRelocation
                        (firstInstruction, secondInstruction, (uint8_t *)recordInfo,
                         TR_AbsoluteMethodAddressOrderedPair, node));

   self()->addRelocation(new (self()->trHeapMemory()) TR::PPCPairedLabelAbsoluteRelocation
                     (firstInstruction, secondInstruction, NULL, NULL, label));

   }


void
OMR::Power::CodeGenerator::addMetaDataFor64BitFixedLoadLabelAddressIntoReg(
      TR::Node *node,
      TR::LabelSymbol *label,
      TR::Register *tempReg,
      TR::Instruction **q)
   {

   if (!self()->comp()->compileRelocatableCode())
      {
      self()->addRelocation(new (self()->trHeapMemory()) TR::PPCPairedLabelAbsoluteRelocation(q[0], q[1], q[2], q[3], label));
      }

   self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(q[0],
                                                                            (uint8_t *)(label),
                                                                            (uint8_t *) (tempReg ? fixedSequence4 : fixedSequence2),
                                                                            TR_FixedSequenceAddress, self()),
                        __FILE__, __LINE__, node);

   }


TR::Instruction *
OMR::Power::CodeGenerator::fixedLoadLabelAddressIntoReg(
      TR::Node *node,
      TR::Register *trgReg,
      TR::LabelSymbol *label,
      TR::Instruction *cursor,
      TR::Register *tempReg,
      bool useADDISFor32Bit)
   {
   TR::Compilation *comp = self()->comp();
   if (TR::Compiler->target.is64Bit())
      {
      int32_t offset = TR_PPCTableOfConstants::allocateChunk(1, self());

      if (offset != PTOC_FULL_INDEX)
         {
         offset *= 8;
         self()->itemTracking(offset, label);
         if (offset<LOWER_IMMED||offset>UPPER_IMMED)
            {
            generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addis, node, trgReg, self()->getTOCBaseRegister(), self()->hiValue(offset));
            generateTrg1MemInstruction(self(),TR::InstOpCode::Op_load, node, trgReg, new (self()->trHeapMemory()) TR::MemoryReference(trgReg, LO_VALUE(offset), 8, self()));
            }
         else
            {
            generateTrg1MemInstruction(self(),TR::InstOpCode::Op_load, node, trgReg, new (self()->trHeapMemory()) TR::MemoryReference(self()->getTOCBaseRegister(), offset, 8, self()));
            }
         }
      else
         {
         TR::Instruction *q[4];

         fixedSeqMemAccess(self(), node, 0, q, trgReg, trgReg, TR::InstOpCode::addi2, 4, NULL, tempReg);

         self()->addMetaDataFor64BitFixedLoadLabelAddressIntoReg(node, label, tempReg, q);
         }
      }
   else
      {
      TR::Instruction *instr1, *instr2;

      if (!useADDISFor32Bit)
         {
         instr1 = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, trgReg, 0);
         }
      else
         {
         instr1 = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addis, node, trgReg, trgReg, 0);
         }
      instr2 = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addi2 , node, trgReg, trgReg, 0);

      self()->addMetaDataFor32BitFixedLoadLabelAddressIntoReg(node, label, instr1, instr2);
      }

   return cursor;
   }

void TR_PPCScratchRegisterManager::addScratchRegistersToDependencyList(
   TR::RegisterDependencyConditions *deps, bool excludeGPR0)
   {
   ListIterator<TR_ManagedScratchRegister> iterator(&_msrList);
   TR_ManagedScratchRegister *msr = iterator.getFirst();

   while (msr)
      {
      deps->unionNoRegPostCondition(msr->_reg, _cg);
      if (excludeGPR0 && msr->_reg->getKind() == TR_GPR)
         {
         TR_ASSERT(deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->getRegister() == msr->_reg, "Virtual register for last post condition dependency is not the expected one");
         deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
         }
      msr = iterator.getNext();
      }
   }


TR::SymbolReference & OMR::Power::CodeGenerator::getArrayCopySymbolReference()
   {
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCarrayCopy_dp, false, false, false);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCarrayCopy, false, false, false);
   }

TR::SymbolReference & OMR::Power::CodeGenerator::getWordArrayCopySymbolReference()
   {
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCwordArrayCopy_dp, false, false, false);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCwordArrayCopy, false, false, false);
   }

TR::SymbolReference & OMR::Power::CodeGenerator::getHalfWordArrayCopySymbolReference()
   {
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPChalfWordArrayCopy_dp, false, false, false);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPChalfWordArrayCopy, false, false, false);
   }

TR::SymbolReference & OMR::Power::CodeGenerator::getForwardArrayCopySymbolReference()
   {
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardArrayCopy_dp, false, false, false);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardArrayCopy, false, false, false);
   }

TR::SymbolReference &OMR::Power::CodeGenerator::getForwardWordArrayCopySymbolReference()
   {
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardWordArrayCopy_dp, false, false, false);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardWordArrayCopy, false, false, false);
   }

TR::SymbolReference &OMR::Power::CodeGenerator::getForwardHalfWordArrayCopySymbolReference()
   {
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardHalfWordArrayCopy_dp, false, false, false);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardHalfWordArrayCopy, false, false, false);
   }


bool OMR::Power::CodeGenerator::supportsTransientPrefetch()
   {
   return TR::Compiler->target.cpu.id() >= TR_PPCp7;
   }


bool OMR::Power::CodeGenerator::is64BitProcessor()
   {
   /*
    * If the target is 64 bit, the CPU must also be 64 bit (even if we don't specifically know which Power CPU it is) so this can just return true.
    * If the target is not 64 bit, we need to check the CPU properties to try and find out if the CPU is 64 bit or not.
    */
   if (TR::Compiler->target.is64Bit())
      {
      return true;
      }
   else
      {
      return TR::Compiler->target.cpu.getPPCis64bit();
      }
   }

bool OMR::Power::CodeGenerator::getSupportsIbyteswap()
   {
   return true;
   }
