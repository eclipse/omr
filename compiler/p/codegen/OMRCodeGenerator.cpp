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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterIterator.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/PrivateLinkage.hpp"
#include "control/RecompilationInfo.hpp"
#endif
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "env/PersistentInfo.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOutOfLineCodeSection.hpp"
#include "p/codegen/PPCSystemLinkage.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"

#if defined(AIXPPC)
#include <sys/debug.h>
#endif


OMR::Power::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
   OMR::CodeGenerator(comp),
      _stackPtrRegister(NULL),
      _constantData(NULL),
      _blockCallInfo(NULL),
      _transientLongRegisters(comp->trMemory()),
      conversionBuffer(NULL),
      _outOfLineCodeSectionList(getTypedAllocator<TR_PPCOutOfLineCodeSection*>(comp->allocator())),
      _startPCLabel(NULL)
   {
   }


void
OMR::Power::CodeGenerator::initialize()
   {
   self()->OMR::CodeGenerator::initialize();

   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = self()->comp();

   // Initialize Linkage for Code Generator
   cg->initializeLinkage();

   _unlatchedRegisterList =
      (TR::RealRegister**)cg->trMemory()->allocateHeapMemory(sizeof(TR::RealRegister*)*(TR::RealRegister::NumRegisters + 1));

   _unlatchedRegisterList[0] = 0; // mark that list is empty

   _linkageProperties = &cg->getLinkage()->getProperties();

   // Set up to collect items for later TOC mapping
   if (comp->target().is64Bit())
      {
      cg->setTrackStatics(new (cg->trHeapMemory()) TR_Array<TR::SymbolReference *>(cg->trMemory()));
      cg->setTrackItems(new (cg->trHeapMemory()) TR_Array<TR_PPCLoadLabelItem *>(cg->trMemory()));
      }
   else
      {
      cg->setTrackStatics(NULL);
      cg->setTrackItems(NULL);
      }

   cg->setStackPointerRegister(cg->machine()->getRealRegister(_linkageProperties->getNormalStackPointerRegister()));
   cg->setMethodMetaDataRegister(cg->machine()->getRealRegister(_linkageProperties->getMethodMetaDataRegister()));
   cg->setTOCBaseRegister(cg->machine()->getRealRegister(_linkageProperties->getTOCBaseRegister()));
   cg->getLinkage()->initPPCRealRegisterLinkage();
   cg->getLinkage()->setParameterLinkageRegisterIndex(comp->getJittedMethodSymbol());
   cg->machine()->initREGAssociations();

   // Tactical GRA settings.
   //
   cg->setGlobalGPRPartitionLimit(TR::Machine::getGlobalGPRPartitionLimit());
   cg->setGlobalFPRPartitionLimit(TR::Machine::getGlobalFPRPartitionLimit());
   cg->setGlobalRegisterTable(_linkageProperties->getRegisterAllocationOrder());
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
   cg->setLastGlobalGPR(_lastGPR);
   cg->setLast8BitGlobalGPR(_lastGPR);
   cg->setLastGlobalFPR(_lastFPR);

   _numVRF = _linkageProperties->getNumAllocatableVectorRegisters();
   cg->setFirstGlobalVRF(_lastFPR + 1);
   cg->setLastGlobalVRF(_lastFPR + _numVRF);

   cg->setSupportsGlRegDeps();
   cg->setSupportsGlRegDepOnFirstBlock();

   cg->setSupportsRecompilation();

   if (comp->target().is32Bit())
      cg->setUsesRegisterPairsForLongs();

   cg->setPerformsChecksExplicitly();
   cg->setConsiderAllAutosAsTacticalGlobalRegisterCandidates();

   cg->setEnableRefinedAliasSets();

   if (!debug("noLiveRegisters"))
      {
      cg->addSupportedLiveRegisterKind(TR_GPR);
      cg->addSupportedLiveRegisterKind(TR_FPR);
      cg->addSupportedLiveRegisterKind(TR_CCR);
      cg->addSupportedLiveRegisterKind(TR_VSX_SCALAR);
      cg->addSupportedLiveRegisterKind(TR_VSX_VECTOR);
      cg->addSupportedLiveRegisterKind(TR_VRF);

      cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_GPR);
      cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_FPR);
      cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_CCR);
      cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_VRF);
      cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_VSX_SCALAR);
      cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_VSX_VECTOR);
      }

    cg->setSupportsConstantOffsetInAddressing();
    cg->setSupportsVirtualGuardNOPing();
    cg->setSupportsPrimitiveArrayCopy();
    cg->setSupportsReferenceArrayCopy();
    cg->setSupportsSelect();
    cg->setSupportsByteswap();

    // disabled for now
    //
    if (comp->getOption(TR_AggressiveOpts) &&
        !comp->getOption(TR_DisableArraySetOpts))
       {
       cg->setSupportsArraySet();
       }

   if (!TR::Compiler->om.canGenerateArraylets())
      {
      static const bool disableArrayCmp = feGetEnv("TR_DisableArrayCmp") != NULL;
      if (!disableArrayCmp)
         {
         cg->setSupportsArrayCmp();
         }
      static const bool disableArrayCmpLen = feGetEnv("TR_DisableArrayCmpLen") != NULL;
      if (!disableArrayCmpLen)
         {
         cg->setSupportsArrayCmpLen();
         }
      }

    if (comp->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX))
       {
       static bool disablePPCTRTO = (feGetEnv("TR_disablePPCTRTO") != NULL);
       static bool disablePPCTRTO255 = (feGetEnv("TR_disablePPCTRTO255") != NULL);
       static bool disablePPCTROT = (feGetEnv("TR_disablePPCTROT") != NULL);
       static bool disablePPCTROTNoBreak = (feGetEnv("TR_disablePPCTROTNoBreak") != NULL);

       if (!disablePPCTRTO)
          cg->setSupportsArrayTranslateTRTO();
#ifndef __LITTLE_ENDIAN__
       else if (!disablePPCTRTO255)
          setSupportsArrayTranslateTRTO255();
#endif

       if (!disablePPCTROT)
          cg->setSupportsArrayTranslateTROT();

       if (!disablePPCTROTNoBreak)
          cg->setSupportsArrayTranslateTROTNoBreak();
       }

   _numberBytesReadInaccessible = 0;
   _numberBytesWriteInaccessible = 4096;

   // Poorly named.  Not necessarily Java specific.
   //
   cg->setSupportsJavaFloatSemantics();

   cg->setSupportsDivCheck();
   cg->setSupportsIMulHigh();
   if (comp->target().is64Bit())
      cg->setSupportsLMulHigh();
   cg->setSupportsLoweringConstLDivPower2();

   static bool disableDCAS = (feGetEnv("TR_DisablePPCDCAS") != NULL);

   if (!disableDCAS &&
       comp->target().is64Bit() &&
       TR::Options::useCompressedPointers())
      {
      cg->setSupportsDoubleWordCAS();
      cg->setSupportsDoubleWordSet();
      }

   if (cg->is64BitProcessor())
      cg->setSupportsInlinedAtomicLongVolatiles();

   if (!comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9) &&
       !(comp->target().cpu.isBigEndian() && comp->target().cpu.is(OMR_PROCESSOR_PPC_P8)))
      {
      cg->setSupportsAlignedAccessOnly();
      }

   // TODO: distinguishing among OOO and non-OOO implementations
   if (comp->target().isSMP())
      cg->setEnforceStoreOrder();

   if (!comp->getOption(TR_DisableTraps) && TR::Compiler->vm.hasResumableTrapHandler(comp))
      cg->setHasResumableTrapHandler();

   /*
    * TODO: TM is currently not compatible with read barriers. If read barriers are required, TM is disabled until the issue is fixed.
    *       TM is now disabled by default, due to various reasons (OS, hardware, etc), unless it is explicitly enabled.
    */
   if (comp->target().cpu.supportsFeature(OMR_FEATURE_PPC_HTM) && comp->getOption(TR_EnableTM) &&
       !comp->getOption(TR_DisableTM) && TR::Compiler->om.readBarrierType() == gc_modron_readbar_none)
      cg->setSupportsTM();

   if (comp->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_ALTIVEC) && comp->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX))
      cg->setSupportsAutoSIMD();

   if (!comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      for (int32_t i = 0; i < TR_numSpillKinds; i++)
         _globalRegisterBitVectors[i].init(cg->getNumberOfGlobalRegisters(), cg->trMemory());

      for (TR_GlobalRegisterNumber grn=0; grn < cg->getNumberOfGlobalRegisters(); grn++)
         {
         TR::RealRegister::RegNum reg = (TR::RealRegister::RegNum)cg->getGlobalRegister(grn);
         if (cg->getFirstGlobalGPR() <= grn && grn <= cg->getLastGlobalGPR())
            _globalRegisterBitVectors[ TR_gprSpill ].set(grn);
         else if (cg->getFirstGlobalFPR() <= grn && grn <= cg->getLastGlobalFPR())
            _globalRegisterBitVectors[ TR_fprSpill ].set(grn);

         if (!cg->getProperties().getPreserved(reg))
            _globalRegisterBitVectors[ TR_volatileSpill ].set(grn);
         if (cg->getProperties().getIntegerArgument(reg) || cg->getProperties().getFloatArgument(reg))
            _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);
         }

      }

   // Calculate inverse of getGlobalRegister function
   //
   TR_GlobalRegisterNumber grn;
   int i;

   TR_GlobalRegisterNumber globalRegNumbers[TR::RealRegister::NumRegisters];
   for (i=0; i < cg->getNumberOfGlobalGPRs(); i++)
     {
     grn = cg->getFirstGlobalGPR() + i;
     globalRegNumbers[cg->getGlobalRegister(grn)] = grn;
     }
   for (i=0; i < cg->getNumberOfGlobalFPRs(); i++)
     {
     grn = cg->getFirstGlobalFPR() + i;
     globalRegNumbers[cg->getGlobalRegister(grn)] = grn;
     }

   // Initialize linkage reg arrays
   TR::PPCLinkageProperties linkageProperties = cg->getProperties();
   for (i=0; i < linkageProperties.getNumIntArgRegs(); i++)
     _gprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getIntegerArgumentRegister(i)];
   for (i=0; i < linkageProperties.getNumFloatArgRegs(); i++)
     _fprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getFloatArgumentRegister(i)];

   if (comp->getOption(TR_TraceRA))
      {
      cg->setGPRegisterIterator(new (cg->trHeapMemory()) TR::RegisterIterator(cg->machine(), TR::RealRegister::FirstGPR, TR::RealRegister::LastGPR));
      cg->setFPRegisterIterator(new (cg->trHeapMemory()) TR::RegisterIterator(cg->machine(), TR::RealRegister::FirstFPR, TR::RealRegister::LastFPR));
      }

   cg->setSupportsProfiledInlining();

   TR_ResolvedMethod *method = comp->getJittedMethodSymbol()->getResolvedMethod();
   TR_ReturnInfo      returnInfo = cg->getLinkage()->getReturnInfoFromReturnType(method->returnType());
   comp->setReturnInfo(returnInfo);
   }

TR::Linkage *
OMR::Power::CodeGenerator::deriveCallingLinkage(TR::Node *node, bool isIndirect)
    {
    TR::SymbolReference *symRef     = node->getSymbolReference();
    TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
    TR::Linkage         *linkage = self()->getLinkage(callee->getLinkageConvention());

    return linkage;
    }

uintptr_t *
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
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableTOC))
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
   TR::Register   *gr0 = self()->machine()->getRealRegister(TR::RealRegister::gr0);
   TR::ResolvedMethodSymbol *methodSymbol = self()->comp()->getJittedMethodSymbol();
   TR::SymbolReference    *revertToInterpreterSymRef = self()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCrevertToInterpreterGlue);
   uintptr_t             ramMethod = (uintptr_t)methodSymbol->getResolvedMethod()->resolvedMethodAddress();
   TR::SymbolReference    *helperSymRef = self()->symRefTab()->findOrCreateRuntimeHelper(directToInterpreterHelper(methodSymbol, self()));
   uintptr_t             helperAddr = (uintptr_t)helperSymRef->getMethodAddress();

   // gr0 must contain the saved LR; see Recompilation.s
   cursor = new (self()->trHeapMemory()) TR::PPCTrg1Instruction(TR::InstOpCode::mflr, node, gr0, cursor, self());
   cursor = self()->getLinkage()->flushArguments(cursor);
   cursor = generateDepImmSymInstruction(self(), TR::InstOpCode::bl, node, (uintptr_t)revertToInterpreterSymRef->getMethodAddress(), new (self()->trHeapMemory()) TR::RegisterDependencyConditions(0,0, self()->trMemory()), revertToInterpreterSymRef, NULL, cursor);

   if (self()->comp()->target().is64Bit())
      {
      int32_t highBits = (int32_t)(ramMethod>>32), lowBits = (int32_t)ramMethod;
      cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                  self()->comp()->target().cpu.isBigEndian()?highBits:lowBits, TR_RamMethod, cursor);
      cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                  self()->comp()->target().cpu.isBigEndian()?lowBits:highBits, cursor);
      }
   else
      {
      cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node, (int32_t)ramMethod, TR_RamMethod, cursor);
      }

   if (self()->comp()->getOption(TR_EnableHCR))
      self()->comp()->getStaticHCRPICSites()->push_front(cursor);

  if (self()->comp()->target().is64Bit())
     {
     int32_t highBits = (int32_t)(helperAddr>>32), lowBits = (int32_t)helperAddr;
     cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                 self()->comp()->target().cpu.isBigEndian()?highBits:lowBits,
                 TR_AbsoluteHelperAddress, helperSymRef, cursor);
     cursor = generateImmInstruction(self(), TR::InstOpCode::dd, node,
                 self()->comp()->target().cpu.isBigEndian()?lowBits:highBits, cursor);
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
   if (self()->comp()->target().is64Bit())
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
   if (self()->comp()->target().is64Bit())
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
   switch (self()->comp()->target().cpu.getProcessorDescription().processor)
      {
      case OMR_PROCESSOR_PPC_PWR630: // 2S+1M FXU out-of-order
         return (numOfOperations<=4);

      case OMR_PROCESSOR_PPC_NSTAR:
      case OMR_PROCESSOR_PPC_PULSAR: // 1S+1M FXU in-order
         return (numOfOperations<=8);

      case OMR_PROCESSOR_PPC_GPUL:
      case OMR_PROCESSOR_PPC_GP:
      case OMR_PROCESSOR_PPC_GR:    // 2 FXU out-of-order back-to-back 2 cycles. Mul is only 4 to 6 cycles
         return (numOfOperations<=2);

      case OMR_PROCESSOR_PPC_P6:    // Mul is on FPU for 17cycles blocking other operations
         return (numOfOperations<=16);

      case OMR_PROCESSOR_PPC_P7:    // Mul blocks other operations for up to 4 cycles
         return (numOfOperations<=3);

      default:          // assume a generic design similar to 604
         return (numOfOperations<=3);
      }
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
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6)) // handles P7, P8
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

bool OMR::Power::CodeGenerator::checkAndFetchRequestor(TR::Instruction *instr, TR::Instruction **q)
   {
   if (_constantData)
      {
      return _constantData->getRequestorsFromNibble(instr, q, true);
      }
   return false;
   }

bool OMR::Power::CodeGenerator::supportsAESInstructions()
   {
    if ( self()->comp()->target().cpu.getPPCSupportsAES() && !self()->comp()->getOption(TR_DisableAESInHardware))
      return true;
    else
      return false;
   }

TR::CodeGenerator::TR_RegisterPressureSummary *
OMR::Power::CodeGenerator::calculateRegisterPressure()
   {
   return NULL;
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

void
OMR::Power::CodeGenerator::expandInstructions()
   {
   _binaryEncodingData.estimate = 0;
   self()->generateBinaryEncodingPrologue(&_binaryEncodingData);

   generateLabelInstruction(
      self(),
      TR::InstOpCode::label,
      self()->comp()->getStartTree()->getNode(),
      self()->getStartPCLabel(),
      _binaryEncodingData.preProcInstruction
   );

   for (TR::Instruction *instr = self()->getFirstInstruction(); instr; instr = instr->getNext())
      {
      instr = instr->expandInstruction();
      }
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

   if (self()->supportsJitMethodEntryAlignment())
      {
      self()->setPreJitMethodEntrySize(data->estimate);
      data->estimate += (self()->getJitMethodEntryAlignmentBoundary() - 1);
      }

   self()->getLinkage()->createPrologue(data->cursorInstruction);
   }

static void expandFarConditionalBranches(TR::CodeGenerator *cg)
   {
   for (TR::Instruction *instr = cg->getFirstInstruction(); instr; instr = instr->getNext())
      {
      TR::PPCConditionalBranchInstruction *branch = instr->getPPCConditionalBranchInstruction();

      if (branch && branch->getLabelSymbol())
         {
         int32_t distance = branch->getLabelSymbol()->getEstimatedCodeLocation() - branch->getEstimatedBinaryLocation();

         if (distance < (int32_t)TR::getMinSigned<TR::Int16>() || distance > (int32_t)TR::getMaxSigned<TR::Int16>())
            branch->expandIntoFarBranch();
         }
      }
   }

void OMR::Power::CodeGenerator::doBinaryEncoding()
   {
   TR_PPCBinaryEncodingData& data = _binaryEncodingData;

   data.cursorInstruction = self()->getFirstInstruction();
   data.estimate = 0;

   while (data.cursorInstruction)
      {
      data.estimate          = data.cursorInstruction->estimateBinaryLength(data.estimate);
      data.cursorInstruction = data.cursorInstruction->getNext();
      }

   data.estimate = self()->setEstimatedLocationsForSnippetLabels(data.estimate);
   if (data.estimate > 32768)
      expandFarConditionalBranches(self());

   self()->setEstimatedCodeLength(data.estimate);

   data.cursorInstruction = self()->getFirstInstruction();
   uint8_t *coldCode = NULL;
   uint8_t *temp = self()->allocateCodeMemory(self()->getEstimatedCodeLength(), 0, &coldCode);

   self()->setBinaryBufferStart(temp);
   self()->setBinaryBufferCursor(temp);
   self()->alignBinaryBufferCursor();

   TR::Instruction *nop;
   TR::Register *gr1 = self()->machine()->getRealRegister(TR::RealRegister::gr1);
   bool skipLabel = false;

   bool  isPrivateLinkage = (self()->comp()->getJittedMethodSymbol()->getLinkageConvention() == TR_Private);

   uint8_t *start = temp;

   while (data.cursorInstruction)
      {
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
         uint32_t linkageInfoWord = self()->initializeLinkageInfo(data.preProcInstruction->getBinaryEncoding());
         toPPCImmInstruction(data.preProcInstruction)->setSourceImmediate(linkageInfoWord);
         }

      self()->getLinkage()->performPostBinaryEncoding();
      }

   // Create exception table entries for outlined instructions.
   //
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

void OMR::Power::CodeGenerator::processRelocations()
   {
   // Fill in pTOC entries that are used to load label addresses. This is done during relocation
   // processing to ensure that snippet labels can be handled correctly.
   if (self()->comp()->target().is64Bit())
      {
      for (int32_t idx = 0; idx < self()->getTrackItems()->size(); idx++)
         TR_PPCTableOfConstants::setTOCSlot(self()->getTrackItems()->element(idx)->getTOCOffset(),
            (uintptr_t)self()->getTrackItems()->element(idx)->getLabel()->getCodeLocation());
      }

   OMR::CodeGenerator::processRelocations();
   }

// different from evaluate in that it returns a clobberable register
TR::Register *OMR::Power::CodeGenerator::gprClobberEvaluate(TR::Node *node)
   {

   TR::Register *resultReg = self()->evaluate(node);
   TR_ASSERT( resultReg->getKind() == TR_GPR || resultReg->getKind() == TR_FPR , "gprClobberEvaluate() called for non-GPR or non-FPR.");

   if (!self()->canClobberNodesRegister(node))
      {
      if (self()->comp()->target().is32Bit() && node->getType().isInt64())
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

bool OMR::Power::CodeGenerator::canTransformUnsafeCopyToArrayCopy()
   {
   return !self()->comp()->getOption(TR_DisableArrayCopyOpts);
   }

bool OMR::Power::CodeGenerator::canTransformUnsafeSetMemory()
   {
   return (self()->comp()->target().is64Bit());
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
      TR::RealRegister *realReg = self()->machine()->getRealRegister(
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

bool
OMR::Power::CodeGenerator::considerTypeForGRA(TR::Node *node)
   {
   return !node->getDataType().isMask();
   }

bool
OMR::Power::CodeGenerator::considerTypeForGRA(TR::DataType dt)
   {
   return !dt.isMask();
   }

bool
OMR::Power::CodeGenerator::considerTypeForGRA(TR::SymbolReference *symRef)
   {
   if (symRef && symRef->getSymbol())
      {
      return self()->considerTypeForGRA(symRef->getSymbol()->getDataType());
      }
   else
      {
      return true;
      }
   }

void OMR::Power::CodeGenerator::findOrCreateFloatConstant(void *v, TR::DataType t,
                             TR::Instruction *n0, TR::Instruction *n1,
                             TR::Instruction *n2, TR::Instruction *n3)
   {
   if (_constantData == NULL)
      _constantData = new (self()->trHeapMemory()) TR::ConstantDataSnippet(self());
   _constantData->addConstantRequest(v, t, n0, n1, n2, n3, NULL, false);
   }

void OMR::Power::CodeGenerator::findOrCreateAddressConstant(void *v, TR::DataType t,
                             TR::Instruction *n0, TR::Instruction *n1,
                             TR::Instruction *n2, TR::Instruction *n3,
                             TR::Node *node, bool isUnloadablePicSite)
   {
   if (_constantData == NULL)
      _constantData = new (self()->trHeapMemory()) TR::ConstantDataSnippet(self());
   _constantData->addConstantRequest(v, t, n0, n1, n2, n3, node, isUnloadablePicSite);
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

bool OMR::Power::CodeGenerator::supportsInliningOfIsInstance()
   {
   return !self()->comp()->getOption(TR_DisableInlineIsInstance);
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
       l1OpCode == TR::multianewarray)
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

TR_GlobalRegisterNumber OMR::Power::CodeGenerator::pickRegister(TR::RegisterCandidate              *  regCan,
                                                          TR::Block                          ** barr,
                                                          TR_BitVector                      &  availRegs,
                                                          TR_GlobalRegisterNumber           &  highRegisterNumber,
                                                          TR_LinkHead<TR::RegisterCandidate> *  candidates)
   {
   if (!self()->comp()->getOption(TR_DisableRegisterPressureSimulation))
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
         if (self()->comp()->target().is32Bit() && highRegisterNumber == 0)
            longLow = 1;
         firstIndex = _firstGPR;
         lastIndex  = _lastFPR ;
         lastVolIndex = _lastVolatileGPR;
         break;

      default:
         if (sym->getDataType().isVector())
            {
            if (sym->getDataType().getVectorElementType() == TR::Int32 ||
                sym->getDataType().getVectorElementType() == TR::Double)
               {
               isVector = true;
               firstIndex = self()->getFirstGlobalVRF();
               lastIndex = self()->getLastGlobalVRF();
               lastVolIndex = lastIndex;  // TODO: preserved VRF's !!
               break;
               }
            }

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
         TR::RegisterCandidate *prev;
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

bool OMR::Power::CodeGenerator::allowGlobalRegisterAcrossBranch(TR::RegisterCandidate *rc, TR::Node * branchNode)
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
                       (node->getChild(ii)->getCaseConstant()-node->getChild(ii-1)->getCaseConstant())<=UPPER_IMMED &&
                       (node->getChild(ii)->getCaseConstant()-node->getChild(ii-1)->getCaseConstant())>0; /* in case of an overflow */
                 ii++);

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
      return ( self()->comp()->target().is64Bit()? _numGPR : _numGPR - 4 ); // 4 GPRs are needed for long-compare arguments themselves.
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

static bool isDistanceInRange(uintptr_t distance, uintptr_t distanceMask)
   {
   // We want a mask that will keep only the sign bits, so we need to get everything above the top
   // bit set in distanceMask (including that bit).
   uintptr_t signMask = ~(distanceMask >> 1);

   // The implicit sign bits (that are generated by sign-extending the distance field) must agree
   // with the explicit sign bit in the distance field for the branch to be decoded correctly.
   return (distance & signMask) == 0 || (distance & signMask) == signMask;
   }

void OMR::Power::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   TR_ASSERT_FATAL(label->getCodeLocation(), "Attempt to relocate a label with a NULL address");
   TR_ASSERT_FATAL((*cursor & 0x0000fffcu) == 0u, "Attempt to relocate into an instruction with existing data in the distance field");

   uintptr_t distance = ((uintptr_t)label->getCodeLocation()-(uintptr_t)cursor);

   TR_ASSERT_FATAL((distance & 0x3u) == 0u, "Attempt to encode an unaligned branch distance");
   TR_ASSERT_FATAL(isDistanceInRange(distance, 0x0000ffffu), "Attempt to encode an out-of-range branch distance");

   *cursor |= distance & 0x0000fffcu;
   }

void OMR::Power::CodeGenerator::apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   TR_ASSERT_FATAL(label->getCodeLocation(), "Attempt to relocate a label with a NULL address");
   TR_ASSERT_FATAL((*cursor & 0x03fffffcu) == 0u, "Attempt to relocate into an instruction with existing data in the distance field");

   uintptr_t distance = ((uintptr_t)label->getCodeLocation()-(uintptr_t)cursor);

   TR_ASSERT_FATAL((distance & 0x3u) == 0u, "Attempt to encode an unaligned branch distance");
   TR_ASSERT_FATAL(isDistanceInRange(distance, 0x03ffffffu), "Attempt to encode an out-of-range branch distance");

   *cursor |= distance & 0x03fffffcu;
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
   TR::RealRegister *realReg = self()->machine()->getRealRegister(realNum);
   self()->getLiveRegisters(reg->getKind())->setAssociation(reg, realReg);
   }


void OMR::Power::CodeGenerator::addRealRegisterInterference(TR::Register    *reg,
              TR::RealRegister::RegNum  realNum)
   {
   if (!reg->isLive() || realNum == TR::RealRegister::NoReg)
      return;
   TR::RealRegister *realReg = self()->machine()->getRealRegister(realNum);
   reg->getLiveRegisterInfo()->addInterference(realReg->getRealRegisterMask());
   }

#if defined(AIXPPC)
#include <unistd.h>
static TR::Instruction    *nextIntervalInstructionPtr;
static uint8_t           *nextIntervalBufferPtr;
static bool               segmentInBlock;
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
   TR::ILOpCodes firstOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondOp = secondChild->getOpCodeValue();

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL &&

         (secondOp == TR::iconst &&
          contiguousBits(secondChild->getInt()) &&
          firstChild->getReferenceCount() == 1 &&
          firstChild->getRegister() == NULL &&
          ((firstOp == TR::imul &&
             firstChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
            firstChild->getSecondChild()->getInt() > 0 &&
            isNonNegativePowerOf2(firstChild->getSecondChild()->getInt())) ||
           ((firstOp == TR::ishr ||
             firstOp == TR::iushr) &&
             firstChild->getSecondChild()->getOpCodeValue() == TR::iconst  &&
            firstChild->getSecondChild()->getInt() > 0 &&
            (leadingZeroes(secondChild->getInt()) >=
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

bool OMR::Power::CodeGenerator::isILOpCodeSupported(TR::ILOpCodes o)
   {
   if (!OMR::CodeGenerator::isILOpCodeSupported(o))
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
         TR::addDependency(cond, deltaReg, TR::RealRegister::NoReg, TR_GPR, self());
         }
      self()->stopUsingRegister(deltaReg);
      return cursor;
      }

   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = self()->allocateRegister();
   TR::Register *counterReg = self()->allocateRegister();

   cursor = loadAddressConstant(self(), self()->comp()->compileRelocatableCode(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), cursor);
   cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addi, node, counterReg, counterReg, delta, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), counterReg, cursor);
   if (cond)
      {
      uint32_t preCondCursor = cond->getAddCursorForPre();
      uint32_t postCondCursor = cond->getAddCursorForPost();
      TR::addDependency(cond, addrReg, TR::RealRegister::NoReg, TR_GPR, self());
      cond->getPreConditions()->getRegisterDependency(preCondCursor)->setExcludeGPR0();
      cond->getPostConditions()->getRegisterDependency(postCondCursor)->setExcludeGPR0();
      TR::addDependency(cond, counterReg, TR::RealRegister::NoReg, TR_GPR, self());
      }
   self()->stopUsingRegister(addrReg);
   self()->stopUsingRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::Power::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond)
   {
   TR::Node *node = cursor->getNode();
   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = self()->allocateRegister();
   TR::Register *counterReg = self()->allocateRegister();

   cursor = loadAddressConstant(self(), self()->comp()->compileRelocatableCode(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), cursor);
   cursor = generateTrg1Src2Instruction(self(), TR::InstOpCode::add, node, counterReg, counterReg, deltaReg, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), counterReg, cursor);
   if (cond)
      {
      uint32_t preCondCursor = cond->getAddCursorForPre();
      uint32_t postCondCursor = cond->getAddCursorForPost();
      TR::addDependency(cond, addrReg, TR::RealRegister::NoReg, TR_GPR, self());
      cond->getPreConditions()->getRegisterDependency(preCondCursor)->setExcludeGPR0();
      cond->getPostConditions()->getRegisterDependency(postCondCursor)->setExcludeGPR0();
      TR::addDependency(cond, counterReg, TR::RealRegister::NoReg, TR_GPR, self());
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

   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = srm.findOrCreateScratchRegister();
   TR::Register *counterReg = srm.findOrCreateScratchRegister();

   cursor = loadAddressConstant(self(), self()->comp()->compileRelocatableCode(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), cursor);
   cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addi, node, counterReg, counterReg, delta, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), counterReg, cursor);
   srm.reclaimScratchRegister(addrReg);
   srm.reclaimScratchRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::Power::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm)
   {
   TR::Node *node = cursor->getNode();
   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = srm.findOrCreateScratchRegister();
   TR::Register *counterReg = srm.findOrCreateScratchRegister();

   cursor = loadAddressConstant(self(), self()->comp()->compileRelocatableCode(), node, addr, addrReg, cursor);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, counterReg, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), cursor);
   cursor = generateTrg1Src2Instruction(self(), TR::InstOpCode::add, node, counterReg, counterReg, deltaReg, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(self(), addrReg, 0, 4), counterReg, cursor);
   srm.reclaimScratchRegister(addrReg);
   srm.reclaimScratchRegister(counterReg);
   return cursor;
   }

bool
OMR::Power::CodeGenerator::canEmitDataForExternallyRelocatableInstructions()
   {
   // On Power, data cannot be emitted inside instructions that will be associated with an
   // external relocation record (ex. AOT or Remote compiles in OpenJ9). This is because when the
   // relocation is applied when a method is loaded, the new data in the instruction is OR'ed in (The reason
   // for OR'ing is that sometimes usefule information such as flags and hints can be stored during compilation in these data fields).
   // Hence, for the relocation to be applied correctly, we must ensure that the data fields inside the instruction
   // initially are zero.
   return !self()->comp()->compileRelocatableCode();
   }

bool OMR::Power::CodeGenerator::isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt)
   {
   return self()->machine()->getRealRegister((TR::RealRegister::RegNum)self()->getGlobalRegister(i))->getState() == TR::RealRegister::Free;
   }


bool OMR::Power::CodeGenerator::supportsSinglePrecisionSQRT()
   {
   return self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_HW_SQRT_FIRST);
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




bool OMR::Power::CodeGenerator::getSupportsOpCodeForAutoSIMD(TR::CPU *cpu, TR::ILOpCode opcode)
   {
   TR_ASSERT_FATAL(opcode.isVectorOpCode(), "getSupportsOpCodeForAutoSIMD expects vector opcode\n");

   TR::DataType ot = opcode.getVectorResultDataType();

   if (ot.getVectorLength() != TR::VectorLength128) return false;

   TR::DataType et = ot.getVectorElementType();

   TR_ASSERT_FATAL(et == TR::Int8 || et == TR::Int16 || et == TR::Int32 || et == TR::Int64 || et == TR::Float || et == TR::Double,
                   "Unexpected vector element type\n");

   // alignment issues
   if (!cpu->isAtLeast(OMR_PROCESSOR_PPC_P8) &&
       et != TR::Double &&
       et != TR::Int64)
      return false;

   // implemented vector opcodes
   switch (opcode.getVectorOperation())
      {
      case TR::vadd:
      case TR::vsub:
      case TR::vmul:
         if (et == TR::Int8 || et == TR::Int16 || et == TR::Int32 || (et == TR::Int64 && cpu->isAtLeast(OMR_PROCESSOR_PPC_P8)) || et == TR::Float || et == TR::Double)
            return true;
         else
            return false;
      case TR::vdiv:
         if (et == TR::Int32 || et == TR::Float || et == TR::Double)
            return true;
         else
            return false;
      case TR::vneg:
         return true;
      case TR::vabs:
         if (et == TR::Int8 || et == TR::Int16 || et == TR::Int32 || (et == TR::Int64 && cpu->isAtLeast(OMR_PROCESSOR_PPC_P8)) || et == TR::Float || et == TR::Double)
            return true;
         else
            return false;
      case TR::vsqrt:
         if (et == TR::Float || et == TR::Double)
            return true;
         else
            return false;
      case TR::vreductionAdd:
         return true;
      case TR::vload:
      case TR::vloadi:
      case TR::vstore:
      case TR::vstorei:
      case TR::mload:
      case TR::mloadi:
      case TR::mstore:
      case TR::mstorei:
         // since lxvb16x, stxvb16x, lxvh8x, and stxvh8x are not available on P8 and lower, vector/loads and stores should only be enabled under these conditions:
         if (et == TR::Int32 || et == TR::Int64 || et == TR::Float || et == TR::Double || cpu->isAtLeast(OMR_PROCESSOR_PPC_P9) || cpu->isBigEndian())
            return true;
         else
            return false;
      case TR::vxor:
      case TR::vor:
      case TR::vand:
         if (et == TR::Int8 || et == TR::Int16 || et == TR::Int32 || et == TR::Int64)
            return true;
         else
            return false;
      case TR::vsplats:
            return true;
      case TR::vmin:
      case TR::vmax:
         if (et == TR::Int8 || et == TR::Int16 || et == TR::Int32 || (et == TR::Int64 && cpu->isAtLeast(OMR_PROCESSOR_PPC_P8)) || et == TR::Float || (et == TR::Double && cpu->isAtLeast(OMR_PROCESSOR_PPC_P8)))
            return true;
         else
            return false;
      case TR::vfma:
         if (et == TR::Float || et == TR::Double)
            return true;
         else
            return false;
      case TR::vgetelem:
            if (et == TR::Int32 || et == TR::Int64 || et == TR::Float || et == TR::Double)
               return true;
            else
               return false;
      case TR::vconv:
         if (et == TR::Double &&
             opcode.getVectorSourceDataType().getVectorElementType() == TR::Int64)
            return true;
         else
            return false;
      case TR::vmadd:
         if (et == TR::Int8 || et == TR::Int16 || et == TR::Int32 || (et == TR::Int64 && cpu->isAtLeast(OMR_PROCESSOR_PPC_P8)) || et == TR::Float || et == TR::Double)
            return true;
         else
            return false;
      case TR::vcmpeq:
      case TR::vcmpne:
      case TR::vcmplt:
      case TR::vcmpgt:
      case TR::vcmple:
      case TR::vcmpge:
         if (et == TR::Int8 || et == TR::Int16 || et == TR::Int32 || (et == TR::Int64 && cpu->isAtLeast(OMR_PROCESSOR_PPC_P8)) || et == TR::Float || et == TR::Double)
            return true;
         else
            return false;
      case TR::vbitselect:
      case TR::vcast:
         return true;
      default:
         return false;
      }

   return false;
   }

bool OMR::Power::CodeGenerator::getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode)
   {
   return TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&self()->comp()->target().cpu, opcode);
   }

bool
OMR::Power::CodeGenerator::getSupportsEncodeUtf16LittleWithSurrogateTest()
   {
   return self()->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX) &&
          !self()->comp()->getOption(TR_DisableSIMDUTF16LEEncoder);
   }

bool
OMR::Power::CodeGenerator::getSupportsEncodeUtf16BigWithSurrogateTest()
   {
   return self()->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX) &&
          !self()->comp()->getOption(TR_DisableSIMDUTF16BEEncoder);
   }


void
OMR::Power::CodeGenerator::addMetaDataForLoadAddressConstantFixed(
      TR::Node *node,
      TR::Instruction *firstInstruction,
      TR::Register *tempReg,
      int16_t typeAddress,
      intptr_t value)
   {
   if (value == 0x0)
      return;

   if (typeAddress == -1)
      typeAddress = TR_FixedSequenceAddress2;

   TR_FixedSequenceKind seqKind = tempReg ? fixedSequence5 : fixedSequence1;
   TR::Compilation *comp = self()->comp();

   TR::Relocation *relo = NULL;

   switch (typeAddress)
      {
      case TR_DataAddress:
         {
         TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = (uintptr_t)node->getSymbolReference();
         recordInfo->data2 = (uintptr_t)node->getInlinedSiteIndex();
         recordInfo->data3 = (uintptr_t)seqKind;
         relo = new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
            firstInstruction,
            (uint8_t *)recordInfo,
            TR_DataAddress, self());
         break;
         }

      case TR_DebugCounter:
         {
         TR::DebugCounterBase *counter = comp->getCounterFromStaticAddress(node->getSymbolReference());
         if (counter == NULL)
            comp->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in OMR::Power::CodeGenerator::addMetaDataForLoadAddressConstantFixed\n");

         TR::DebugCounter::generateRelocation(comp, firstInstruction, node, counter, seqKind);
         return;
         }

      case TR_BlockFrequency:
         {
         TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = (uintptr_t)node->getSymbolReference();
         recordInfo->data2 = (uintptr_t)seqKind;
         relo = new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
            firstInstruction,
            (uint8_t *)recordInfo,
            TR_BlockFrequency, self());
         break;
         }

      case TR_AbsoluteHelperAddress:
         {
         value = (uintptr_t)node->getSymbolReference();
         break;
         }

      case TR_MethodEnterExitHookAddress:
         {
         relo = new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
            firstInstruction,
            (uint8_t *)node->getSymbolReference(),
            (uint8_t *)seqKind,
            TR_MethodEnterExitHookAddress, self());
         break;
         }

      case TR_CallsiteTableEntryAddress:
         {
         relo = new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
            firstInstruction,
            (uint8_t *)node->getSymbolReference(),
            (uint8_t *)seqKind,
            TR_CallsiteTableEntryAddress, self());
         break;
         }
      }

   if (comp->getOption(TR_UseSymbolValidationManager) && !relo)
      {
      switch (typeAddress)
         {
         case TR_ClassAddress:
            {
            TR::SymbolReference *symRef = (TR::SymbolReference *)value;

            TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
            recordInfo->data1 = (uintptr_t)symRef->getSymbol()->getStaticSymbol()->getStaticAddress();
            recordInfo->data2 = (uintptr_t)TR::SymbolType::typeClass;
            recordInfo->data3 = (uintptr_t)seqKind;
            relo = new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
               firstInstruction,
               (uint8_t *)recordInfo,
               TR_DiscontiguousSymbolFromManager,
               self());
            break;
            }

         case TR_RamMethodSequence:
            {
            TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
            recordInfo->data1 = (uintptr_t)self()->comp()->getJittedMethodSymbol()->getResolvedMethod()->resolvedMethodAddress();
            recordInfo->data2 = (uintptr_t)TR::SymbolType::typeMethod;
            recordInfo->data3 = (uintptr_t)seqKind;
            relo = new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
               firstInstruction,
               (uint8_t *)recordInfo,
               TR_DiscontiguousSymbolFromManager,
               self());
            break;
            }
         }
      }

   if (!relo)
      {
      relo = new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
         firstInstruction,
         (uint8_t *)value,
         (uint8_t *)seqKind,
         (TR_ExternalRelocationTargetKind)typeAddress,
         self());
      }

   self()->addExternalRelocation(
      relo,
      __FILE__,
      __LINE__,
      node);
   }


TR::Instruction *
OMR::Power::CodeGenerator::loadAddressConstantFixed(
      TR::Node * node,
      intptr_t value,
      TR::Register *trgReg,
      TR::Instruction *cursor,
      TR::Register *tempReg,
      int16_t typeAddress,
      bool doAOTRelocation)
   {
   TR::Compilation *comp = self()->comp();
   bool canEmitData = self()->canEmitDataForExternallyRelocatableInstructions();

   if (self()->comp()->target().is32Bit())
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
      cursor = firstInstruction = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, trgReg, canEmitData ? (value>>48) : 0 , cursor);

      // ori trgReg, trgReg, next 16-bits
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, trgReg, trgReg, canEmitData ? ((value>>32) & 0x0000ffff) : 0, cursor);
      // shiftli trgReg, trgReg, 32
      cursor = generateTrg1Src1Imm2Instruction(self(), TR::InstOpCode::rldicr, node, trgReg, trgReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
      // oris trgReg, trgReg, next 16-bits
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::oris, node, trgReg, trgReg, canEmitData ? ((value>>16) & 0x0000ffff) : 0, cursor);
      // ori trgReg, trgReg, last 16-bits
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, trgReg, trgReg, canEmitData ? (value & 0x0000ffff) : 0, cursor);
      }
   else
      {
      // lis tempReg, bits[0-15]
      cursor = firstInstruction = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, tempReg, canEmitData ? (value>>48) : 0, cursor);

      // lis trgReg, bits[32-47]
      cursor = generateTrg1ImmInstruction(self(), TR::InstOpCode::lis, node, trgReg, canEmitData ? ((int16_t)(value>>16)) : 0, cursor);
      // ori tempReg, tempReg, bits[16-31]
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, tempReg, tempReg, canEmitData ? ((value>>32) & 0x0000ffff) : 0, cursor);
      // ori trgReg, trgReg, bits[48-63]
      cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::ori, node, trgReg, trgReg, canEmitData ? (value & 0x0000ffff) : 0, cursor);
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
      self()->addExternalRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)firstInstruction,
                                                                                          (uint8_t *)secondInstruction,
                                                                                          (uint8_t *)recordInfo,
                                                                                          (TR_ExternalRelocationTargetKind)TR_DataAddress, self()),
                           __FILE__, __LINE__, node);
      }
   else if (typeAddress == TR_DebugCounter)
      {
      TR::DebugCounterBase *counter = comp->getCounterFromStaticAddress(node->getSymbolReference());
      if (counter == NULL)
         {
         comp->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in OMR::Power::CodeGenerator::addMetaDataForLoadAddressConstantFixed\n");
         }

      TR::DebugCounter::generateRelocation(comp, firstInstruction, secondInstruction, node, counter, orderedPairSequence2);
      }
   else if (typeAddress == TR_BlockFrequency)
      {
      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = (uintptr_t)node->getSymbolReference();
      recordInfo->data2 = orderedPairSequence2;
      self()->addExternalRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)firstInstruction,
                                                                                          (uint8_t *)secondInstruction,
                                                                                          (uint8_t *)recordInfo,
                                                                                          (TR_ExternalRelocationTargetKind)typeAddress, self()),
                                                                                          __FILE__, __LINE__, node);
      }
   else if (typeAddress == TR_MethodEnterExitHookAddress)
      {
      self()->addExternalRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)firstInstruction,
                                                                                          (uint8_t *)secondInstruction,
                                                                                          (uint8_t *)node->getSymbolReference(),
                                                                                          (uint8_t *)orderedPairSequence2,
                                                                                          (TR_ExternalRelocationTargetKind)TR_MethodEnterExitHookAddress, self()),
                           __FILE__, __LINE__, node);
      }
   else if (typeAddress == TR_CallsiteTableEntryAddress)
      {
      self()->addExternalRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)firstInstruction,
                                                                                          (uint8_t *)secondInstruction,
                                                                                          (uint8_t *)node->getSymbolReference(),
                                                                                          (uint8_t *)orderedPairSequence2,
                                                                                          (TR_ExternalRelocationTargetKind)TR_CallsiteTableEntryAddress, self()),
                           __FILE__, __LINE__, node);
      }
   else if (typeAddress != -1)
      {
      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = (uintptr_t)value;
      recordInfo->data3 = orderedPairSequence2;
      self()->addExternalRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)firstInstruction,
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

   if (self()->canEmitDataForExternallyRelocatableInstructions())
      {
      self()->addRelocation(new (self()->trHeapMemory()) TR::PPCPairedLabelAbsoluteRelocation(q[0], q[1], q[2], q[3], label));
      }

   self()->addExternalRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(q[0],
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
   if (self()->comp()->target().is64Bit())
      {
      TR_ASSERT_FATAL(!useADDISFor32Bit, "Cannot set useADDISFor32Bit on 64-bit systems");

      if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
         {
         generateTrg1MemInstruction(self(), TR::InstOpCode::paddi, node, trgReg, TR::MemoryReference::createWithLabel(self(), label, 0, 0));
         }
      else
         {
         int32_t offset = TR_PPCTableOfConstants::allocateChunk(1, self());

         if (offset != PTOC_FULL_INDEX)
            {
            offset *= 8;
            self()->itemTracking(offset, label);
            if (offset<LOWER_IMMED||offset>UPPER_IMMED)
               {
               TR_ASSERT_FATAL_WITH_NODE(node, 0x00008000 != self()->hiValue(offset), "TOC offset (0x%x) is unexpectedly high. Can not encode upper 16 bits into an addis instruction.", offset);
               generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addis, node, trgReg, self()->getTOCBaseRegister(), self()->hiValue(offset));
               generateTrg1MemInstruction(self(),TR::InstOpCode::Op_load, node, trgReg, TR::MemoryReference::createWithDisplacement(self(), trgReg, LO_VALUE(offset), 8));
               }
            else
               {
               generateTrg1MemInstruction(self(),TR::InstOpCode::Op_load, node, trgReg, TR::MemoryReference::createWithDisplacement(self(), self()->getTOCBaseRegister(), offset, 8));
               }
            }
         else
            {
            TR::Instruction *q[4];

            fixedSeqMemAccess(self(), node, 0, q, trgReg, trgReg, TR::InstOpCode::addi2, 4, NULL, tempReg);

            self()->addMetaDataFor64BitFixedLoadLabelAddressIntoReg(node, label, tempReg, q);
            }
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
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCarrayCopy_dp);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCarrayCopy);
   }

TR::SymbolReference & OMR::Power::CodeGenerator::getWordArrayCopySymbolReference()
   {
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCwordArrayCopy_dp);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCwordArrayCopy);
   }

TR::SymbolReference & OMR::Power::CodeGenerator::getHalfWordArrayCopySymbolReference()
   {
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPChalfWordArrayCopy_dp);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPChalfWordArrayCopy);
   }

TR::SymbolReference & OMR::Power::CodeGenerator::getForwardArrayCopySymbolReference()
   {
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardArrayCopy_dp);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardArrayCopy);
   }

TR::SymbolReference &OMR::Power::CodeGenerator::getForwardWordArrayCopySymbolReference()
   {
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardWordArrayCopy_dp);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardWordArrayCopy);
   }

TR::SymbolReference &OMR::Power::CodeGenerator::getForwardHalfWordArrayCopySymbolReference()
   {
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardHalfWordArrayCopy_dp);
   else
      return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCforwardHalfWordArrayCopy);
   }

bool OMR::Power::CodeGenerator::supportsTransientPrefetch()
   {
   return TR::comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P7);
   }

bool OMR::Power::CodeGenerator::is64BitProcessor()
   {
   /*
    * If the target is 64 bit, the CPU must also be 64 bit (even if we don't specifically know which Power CPU it is) so this can just return true.
    * If the target is not 64 bit, we need to check the CPU properties to try and find out if the CPU is 64 bit or not.
    */
   if (self()->comp()->target().is64Bit())
      {
      return true;
      }
   else
      {
      return self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_64BIT_FIRST);
      }
   }

bool
OMR::Power::CodeGenerator::supportsNonHelper(TR::SymbolReferenceTable::CommonNonhelperSymbol symbol)
   {
   bool result = false;

   switch (symbol)
      {
      case TR::SymbolReferenceTable::atomicAddSymbol:
      case TR::SymbolReferenceTable::atomicFetchAndAddSymbol:
      case TR::SymbolReferenceTable::atomicSwapSymbol:
         {
         // Atomic non-helpers for Power only supported on 64-bit, for now
         result = self()->comp()->target().is64Bit();
         break;
         }
      default:
         break;
      }

   return result;
   }

uint32_t
OMR::Power::CodeGenerator::getJitMethodEntryAlignmentBoundary()
   {
   return 128;
   }

bool
OMR::Power::CodeGenerator::directCallRequiresTrampoline(intptr_t targetAddress, intptr_t sourceAddress)
   {
   return
      !self()->comp()->target().cpu.isTargetWithinIFormBranchRange(targetAddress, sourceAddress) ||
      self()->comp()->getOption(TR_StressTrampolines);
   }

uint32_t
OMR::Power::CodeGenerator::getHotLoopAlignment()
   {
   if (self()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
      return 16;
   else
      return 32;
   }

TR::LabelSymbol *
OMR::Power::CodeGenerator::getStartPCLabel()
   {
   if (!_startPCLabel)
      _startPCLabel = generateLabelSymbol(self());
   return _startPCLabel;
   }

// Multiply a register by a constant
void mulConstant(TR::Node * node, TR::Register *trgReg, TR::Register *sourceReg, int32_t value, TR::CodeGenerator *cg)
   {
   if (value == 0)
      {
      loadConstant(cg, node, 0, trgReg);
      }
   else if (value == 1)
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, sourceReg);
      }
   else if (value == -1)
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, trgReg, sourceReg);
      }
   else if (isNonNegativePowerOf2(value) || value==TR::getMinSigned<TR::Int32>())
      {
      generateShiftLeftImmediate(cg, node, trgReg, sourceReg, trailingZeroes(value));
      }
   else if (isNonPositivePowerOf2(value))
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediate(cg, node, tempReg, sourceReg, trailingZeroes(value));
      generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, trgReg, tempReg);
      cg->stopUsingRegister(tempReg);
      }
   else if (isNonNegativePowerOf2(value-1) || (value-1)==TR::getMinSigned<TR::Int32>())
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediate(cg, node, tempReg, sourceReg, trailingZeroes(value-1));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, tempReg, sourceReg);
      cg->stopUsingRegister(tempReg);
      }
   else if (isNonNegativePowerOf2(value+1))
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediate(cg, node, tempReg, sourceReg, trailingZeroes(value+1));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, trgReg, sourceReg, tempReg);
      cg->stopUsingRegister(tempReg);
      }
   else if (value >= LOWER_IMMED && value <= UPPER_IMMED)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::mulli, node, trgReg, sourceReg, value);
      }
   else   // constant won't fit
      {
      TR::Register *tempReg = cg->allocateRegister();
      loadConstant(cg, node, value, tempReg);
      // want the smaller of the sources in the RB position of a multiply
      // one crude measure of absolute size is the number of leading zeros
      if (leadingZeroes(abs(value)) >= 24)
         // the constant is fairly small, so put it in RB
         generateTrg1Src2Instruction(cg, TR::InstOpCode::mullw, node, trgReg, sourceReg, tempReg);
      else
         // the constant is fairly big, so put it in RA
         generateTrg1Src2Instruction(cg, TR::InstOpCode::mullw, node, trgReg, tempReg, sourceReg);
      cg->stopUsingRegister(tempReg);
      }
   }

// Multiply a register by a constant
void mulConstant(TR::Node * node, TR::Register *trgReg, TR::Register *sourceReg, int64_t value, TR::CodeGenerator *cg)
   {
   if (value == 0)
      {
      loadConstant(cg, node, 0, trgReg);
      }
   else if (value == 1)
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, sourceReg);
      }
   else if (value == -1)
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, trgReg, sourceReg);
      }
   else if (isNonNegativePowerOf2(value) || value==TR::getMinSigned<TR::Int64>())
      {
      generateShiftLeftImmediateLong(cg, node, trgReg, sourceReg, trailingZeroes(value));
      }
   else if (isNonPositivePowerOf2(value))
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediateLong(cg, node, tempReg, sourceReg, trailingZeroes(value));
      generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, trgReg, tempReg);
      cg->stopUsingRegister(tempReg);
      }
   else if (isNonNegativePowerOf2(value-1) || (value-1)==TR::getMinSigned<TR::Int64>())
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediateLong(cg, node, tempReg, sourceReg, trailingZeroes(value-1));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, tempReg, sourceReg);
      cg->stopUsingRegister(tempReg);
      }
   else if (isNonNegativePowerOf2(value+1))
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediateLong(cg, node, tempReg, sourceReg, trailingZeroes(value+1));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, trgReg, sourceReg, tempReg);
      cg->stopUsingRegister(tempReg);
      }
   else if (value >= LOWER_IMMED && value <= UPPER_IMMED)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::mulli, node, trgReg, sourceReg, value);
      }
   else   // constant won't fit
      {
      TR::Register *tempReg = cg->allocateRegister();
      uint64_t abs_value = value >= 0 ? value : -value;
      loadConstant(cg, node, value, tempReg);
      // want the smaller of the sources in the RB position of a multiply
      // one crude measure of absolute size is the number of leading zeros
      if (leadingZeroes(abs_value) >= 56)
         // the constant is fairly small, so put it in RB
         generateTrg1Src2Instruction(cg, TR::InstOpCode::mulld, node, trgReg, sourceReg, tempReg);
      else
         // the constant is fairly big, so put it in RA
         generateTrg1Src2Instruction(cg, TR::InstOpCode::mulld, node, trgReg, tempReg, sourceReg);
      cg->stopUsingRegister(tempReg);
      }
   }


TR::Register *addConstantToLong(TR::Node *node, TR::Register *srcReg,
                                int64_t value, TR::Register *trgReg, TR::CodeGenerator *cg)
   {
   if (!trgReg)
      trgReg = cg->allocateRegister();

   if ((int16_t)value == value)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, trgReg, srcReg, value);
      }
   // NOTE: the following only works if the second add's immediate is not sign extended
   else if (((int32_t)value == value) && ((value & 0x8000) == 0))
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, trgReg, srcReg, value >> 16);
      if (value & 0x7fff)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, trgReg, trgReg, value & 0x7fff);
      }
   else
      {
      TR::Register *tempReg = cg->allocateRegister();
      loadConstant(cg, node, value, tempReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, srcReg, tempReg);
      cg->stopUsingRegister(tempReg);
      }

   return trgReg;
   }

TR::Register *addConstantToLong(TR::Node * node, TR::Register *srcHighReg, TR::Register *srclowReg,
                               int32_t highValue, int32_t lowValue,
                               TR::CodeGenerator *cg)
   {
   TR::Register *lowReg  = cg->allocateRegister();
   TR::Register *highReg = cg->allocateRegister();
   TR::RegisterPair *trgReg = cg->allocateRegisterPair(lowReg, highReg);

   if (lowValue >= LOWER_IMMED && lowValue <= UPPER_IMMED)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, lowReg, srclowReg, lowValue);
      }
   else   // constant won't fit
      {
      TR::Register *lowValueReg = cg->allocateRegister();
      loadConstant(cg, node, lowValue, lowValueReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::addc, node, lowReg, srclowReg, lowValueReg);
      cg->stopUsingRegister(lowValueReg);
      }
   if (highValue == 0)
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::addze, node, highReg, srcHighReg);
      }
   else if (highValue == -1)
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::addme, node, highReg, srcHighReg);
      }
   else
      {
      TR::Register *highValueReg = cg->allocateRegister();
      loadConstant(cg, node, highValue, highValueReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::adde, node, highReg, srcHighReg, highValueReg);
      cg->stopUsingRegister(highValueReg);
      }
   return trgReg;
   }

TR::Register *addConstantToInteger(TR::Node * node, TR::Register *trgReg, TR::Register *srcReg, int32_t value, TR::CodeGenerator *cg)
   {
   if (value >= LOWER_IMMED && value <= UPPER_IMMED)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, trgReg, srcReg, value);
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, trgReg, srcReg, (int16_t)HI_VALUE(value));

      if (value & 0xFFFF)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, trgReg, trgReg, LO_VALUE(value));
         }
      }
   return trgReg;
   }

TR::Register *addConstantToInteger(TR::Node * node, TR::Register *srcReg, int32_t value, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();

   return addConstantToInteger(node, trgReg, srcReg, value, cg);
   }
