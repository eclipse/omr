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

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"

#include <limits.h>                                    // for INT_MAX
#include <stdint.h>                                    // for int32_t, etc
#include <string.h>                                    // for NULL, strstr, etc
#include "codegen/BackingStore.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                        // for feGetEnv, etc
#include "codegen/GCStackAtlas.hpp"                    // for GCStackAtlas
#include "codegen/GCStackMap.hpp"                      // for TR_GCStackMap, etc
#include "codegen/Instruction.hpp"                     // for Instruction, etc
#include "codegen/Linkage.hpp"                         // for Linkage, etc
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"                         // for Machine, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                    // for RealRegister, etc
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"                        // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterIterator.hpp"
#include "codegen/RegisterPressureSimulatorInner.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/Snippet.hpp"                         // for Snippet, etc
#include "codegen/TreeEvaluator.hpp"                   // for TreeEvaluator, etc
#include "codegen/X86Evaluator.hpp"
#ifdef TR_TARGET_64BIT
#include "x/amd64/codegen/AMD64SystemLinkage.hpp"
#else
#include "x/i386/codegen/IA32SystemLinkage.hpp"
#endif
#include "compile/Compilation.hpp"                     // for Compilation, etc
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"                 // for TR::Options, etc
#include "control/Recompilation.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"
#endif
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                                  // for IO
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"                                // for Block
#include "il/DataTypes.hpp"                            // for TR::DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                                // for ILOpCode, etc
#include "il/Node.hpp"                                 // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                               // for Symbol
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                              // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"                   // for LabelSymbol, etc
#include "il/symbol/MethodSymbol.hpp"                  // for MethodSymbol
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"                  // for StaticSymbol
#include "infra/Assert.hpp"                            // for TR_ASSERT
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"                         // for TR_BitVector, etc
#include "infra/Flags.hpp"                             // for flags8_t, etc
#include "infra/IGNode.hpp"                            // for TR_IGNode
#include "infra/InterferenceGraph.hpp"
#include "infra/List.hpp"                              // for ListIterator, etc
#include "infra/Stack.hpp"                             // for TR_Stack
#include "optimizer/RegisterCandidate.hpp"
#include "ras/Debug.hpp"                               // for TR_DebugBase
#include "ras/DebugCounter.hpp"
#include "x/codegen/DataSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                        // for TR_X86OpCode, etc
#include "x/codegen/X86Ops_inlines.hpp"

namespace OMR { class RegisterUsage; }
namespace TR { class RegisterDependencyConditions; }

// Hack markers
#define CANT_REMATERIALIZE_ADDRESSES (TR::Compiler->target.is64Bit()) // AMD64 produces a memref with an unassigned addressRegister


TR_X86ProcessorInfo OMR::X86::CodeGenerator::_targetProcessorInfo;

void TR_X86ProcessorInfo::initialize(TR::Compilation *comp)
   {
   // For now, we only convert the feature bits into a flags32_t, for easier querying.
   // To retrieve other information, the VM functions can be called directly.
   //
   _featureFlags.set(TR::Compiler->target.cpu.getX86ProcessorFeatureFlags(comp));
   _featureFlags2.set(TR::Compiler->target.cpu.getX86ProcessorFeatureFlags2(comp));
   _featureFlags8.set(TR::Compiler->target.cpu.getX86ProcessorFeatureFlags8(comp));

   // Determine the processor vendor.
   //
   const char *vendor = TR::Compiler->target.cpu.getX86ProcessorVendorId(comp);
   if (!strncmp(vendor, "GenuineIntel", 12))
      _vendorFlags.set(TR_GenuineIntel);
   else if (!strncmp(vendor, "AuthenticAMD", 12))
      _vendorFlags.set(TR_AuthenticAMD);
   else
      _vendorFlags.set(TR_UnknownVendor);

   // Finally set this bit so we don't attempt to re-initialize.
   //
   _featureFlags.set(TR_X86ProcessorInfoInitialized);

   // initialize the processor model description
   _processorDescription = 0;

   // set up the processor family and cache description

   uint32_t _processorSignature = TR::Compiler->target.cpu.getX86ProcessorSignature(comp);

   if (isGenuineIntel())
      {
      switch (getCPUFamily(_processorSignature))
         {
         case 0x05: _processorDescription |= TR_ProcessorIntelPentium; break;
         case 0x06:
            {
            uint32_t extended_model = getCPUModel(_processorSignature) + (getCPUExtendedModel(_processorSignature) << 4);
            switch (extended_model)
               {
               case 0x55:
                  _processorDescription |= TR_ProcessorIntelSkylake; break;
               case 0x4f:
                  _processorDescription |= TR_ProcessorIntelBroadwell; break;
               case 0x3f:
               case 0x3c:
                  _processorDescription |= TR_ProcessorIntelHaswell; break;
               case 0x3e:
               case 0x3a:
                  _processorDescription |= TR_ProcessorIntelIvyBridge; break;
               case 0x2a:
               case 0x2d:  // SandyBridge EP
                  _processorDescription |= TR_ProcessorIntelSandyBridge; break;
               case 0x2c:  // WestmereEP
               case 0x2f:  // WestmereEX
                  _processorDescription |= TR_ProcessorIntelWestmere; break;
               case 0x1a:  // Nehalem
                  _processorDescription |= TR_ProcessorIntelNehalem; break;
               case 0x17:  // Harpertown
               case 0x0f:  // Woodcrest/Clovertown
                  _processorDescription |= TR_ProcessorIntelCore2; break;
               default:  _processorDescription |= TR_ProcessorIntelP6; break;
               }
            break;
            }
         case 0x0f: _processorDescription |= TR_ProcessorIntelPentium4; break;
         default:   _processorDescription |= TR_ProcessorUnknown; break;
         }
      }
   else if (isAuthenticAMD())
      {
      switch (getCPUFamily(_processorSignature))
         {
         case 0x05:
            if (getCPUModel(_processorSignature) < 0x04)
               _processorDescription |= TR_ProcessorAMDK5;
            else
               _processorDescription |= TR_ProcessorAMDK6;
            break;
         case 0x06: _processorDescription |= TR_ProcessorAMDAthlonDuron; break;
         case 0x0f:
            if (getCPUExtendedFamily(_processorSignature) < 6)
               _processorDescription |= TR_ProcessorAMDOpteron;
            else
               _processorDescription |= TR_ProcessorAMDFamily15h;
            break;
         default:   _processorDescription |= TR_ProcessorUnknown; break;
         }
      }
   }


void
OMR::X86::CodeGenerator::initialize(TR::Compilation *comp)
   {

   bool supportsSSE2 = false;
   _targetProcessorInfo.initialize(comp);

   // Pick a padding table
   //
   if (_targetProcessorInfo.isGenuineIntel() && TR::Compiler->target.is32Bit())
      {
      _paddingTable = &_old32BitPaddingTable;
      }
   else if (_targetProcessorInfo.isAuthenticAMD())
      _paddingTable = &_K8PaddingTable;
   else if (_targetProcessorInfo.prefersMultiByteNOP() && !comp->getOption(TR_DisableZealousCodegenOpts))
      _paddingTable = &_intelMultiBytePaddingTable;
   else if (TR::Compiler->target.is32Bit())
      _paddingTable = &_old32BitPaddingTable; // Unknown 32-bit target
   else
      _paddingTable = &_K8PaddingTable; // Unknown 64-bit target

   // Determine whether or not x87 or SSE should be used for floating point.
   //

#if defined(TR_TARGET_X86) && !defined(J9HAMMER)
   if (_targetProcessorInfo.supportsSSE2() && TR::Compiler->target.cpu.testOSForSSESupport(comp))
      supportsSSE2 = true;
#endif // defined(TR_TARGET_X86) && !defined(J9HAMMER)

   if (_targetProcessorInfo.supportsTM() && !comp->getOption(TR_DisableTM))
      {
      /**
        * Due to many verions of Haswell and a small number of Broadwell have defects for TM and then disabled by Intel,
        * we will return false for any versions before Broadwell.
        *
        * TODO: Need to figure out from which mode of Broadwell start supporting TM
        */
      if (!_targetProcessorInfo.isIntelHaswell())
         {
         if (TR::Compiler->target.is64Bit())
            {
            self()->setSupportsTM(); // disable tm on 32bits for now
            }
         }
      }

   if (TR::Compiler->target.is64Bit()
#if defined(TR_TARGET_X86) && !defined(J9HAMMER)
       || supportsSSE2
#endif
      )
      {
      self()->setUseSSEForSinglePrecision();
      self()->setUseSSEForDoublePrecision();
      self()->setSupportsAutoSIMD();
      self()->setSupportsJavaFloatSemantics();
      }

   // Choose the best XMM double precision load instruction for the target architecture.
   //
   if (self()->useSSEForDoublePrecision())
      {
      static char *forceMOVLPD = feGetEnv("TR_forceMOVLPDforDoubleLoads");
      if (_targetProcessorInfo.isAuthenticAMD() || forceMOVLPD)
         {
         self()->setXMMDoubleLoadOpCode(MOVLPDRegMem);
         }
      else
         {
         self()->setXMMDoubleLoadOpCode(MOVSDRegMem);
         }
      }

#if defined(TR_TARGET_X86) && !defined(J9HAMMER)
   // Determine if software prefetches are supported.
   //
   // 32-bit platforms must check the processor and OS.
   // 64-bit platforms unconditionally support prefetching.
   //
   if (_targetProcessorInfo.supportsSSE() && TR::Compiler->target.cpu.testOSForSSESupport(comp))
#endif // defined(TR_TARGET_X86) && !defined(J9HAMMER)
      {
      self()->setTargetSupportsSoftwarePrefetches();
      }

   // Enable software prefetch of the TLH and configure the TLH prefetching
   // geometry.
   //
   if (((!comp->getOption(TR_DisableTLHPrefetch) && (comp->cg()->getX86ProcessorInfo().isIntelCore2() || comp->cg()->getX86ProcessorInfo().isIntelNehalem())) ||
       (comp->getOption(TR_TLHPrefetch) && self()->targetSupportsSoftwarePrefetches())))
      {
      self()->setEnableTLHPrefetching();
      }

   self()->setGlobalGPRPartitionLimit(self()->machine()->getGlobalGPRPartitionLimit());
   self()->setGlobalFPRPartitionLimit(self()->machine()->getGlobalFPRPartitionLimit());
   self()->setLastGlobalGPR(self()->machine()->getLastGlobalGPRRegisterNumber());
   self()->setLast8BitGlobalGPR(self()->machine()->getLast8BitGlobalGPRRegisterNumber());
   self()->setLastGlobalFPR(self()->machine()->getLastGlobalFPRRegisterNumber());
   /*
    * GRA does not work with vector registers on 32 bit due to a bug where xmm registers are not being assigned.
    * This disables GRA for vector registers on 32 bit.
    * This code will be reenabled as part of Issue 2035 which tracks the progress of fixing the GRA bug.
    * GRA does not work with vector registers on 64 bit either. So GRA is now being disabled vector registers.
    * This code will be reenabled as part of Issue 2280
    */
#if 0
   if (TR::Compiler->target.is64Bit())
      {
      self()->setFirstGlobalVRF(self()->getFirstGlobalFPR());
      self()->setLastGlobalVRF(self()->getLastGlobalFPR());
      }
#endif // closes the if 0.

   // Initialize Linkage for Code Generator
   self()->initializeLinkage();

   _linkageProperties = &self()->getLinkage()->getProperties();

   _unlatchedRegisterList = (TR::RealRegister**)self()->trMemory()->allocateHeapMemory(sizeof(TR::RealRegister*)*(TR::RealRegister::NumRegisters + 1));
   _unlatchedRegisterList[0] = 0; // mark that list is empty

   self()->setGlobalRegisterTable(self()->machine()->getGlobalRegisterTable(*_linkageProperties));

   self()->machine()->initialiseRegisterFile(*_linkageProperties);

   self()->getLinkage()->copyLinkageInfoToParameterSymbols();

   _switchToInterpreterLabel = NULL;

   // Use a virtual frame pointer register.  The real frame pointer register to be used
   // will be substituted based on _vfpState during size estimation.
   //
   _frameRegister = self()->machine()->getX86RealRegister(TR::RealRegister::vfp);

   TR::Register            *vmThreadRegister = self()->setVMThreadRegister(self()->allocateRegister());
   TR::RealRegister::RegNum vmThreadIndex    = _linkageProperties->getMethodMetaDataRegister();
   if (vmThreadIndex != TR::RealRegister::NoReg)
      {
      TR::RealRegister *vmThreadReal = self()->machine()->getX86RealRegister(vmThreadIndex);
      vmThreadRegister->setAssignedRegister(vmThreadReal);
      vmThreadRegister->setAssociation(vmThreadIndex);
      vmThreadReal->setAssignedRegister(vmThreadRegister);
      vmThreadReal->setState(TR::RealRegister::Assigned);
      }

   if (!debug("disableBetterSpillPlacements"))
      self()->setEnableBetterSpillPlacements();

   self()->setEnableRematerialisation();
   self()->setEnableRegisterAssociations();
   self()->setEnableRegisterWeights();
   self()->setEnableRegisterInterferences();

   self()->setEnableSinglePrecisionMethods();

   if (!debug("disableRefinedAliasSets"))
      self()->setEnableRefinedAliasSets();

   if (!comp->getOption(TR_DisableLiveRangeSplitter))
      comp->setOption(TR_EnableRangeSplittingGRA);

   self()->addSupportedLiveRegisterKind(TR_GPR);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_GPR);
   self()->addSupportedLiveRegisterKind(TR_FPR);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_FPR);
   self()->addSupportedLiveRegisterKind(TR_VRF);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_VRF);

   self()->setSupportsArrayCmp();
   self()->setSupportsPrimitiveArrayCopy();
   self()->setSupportsReferenceArrayCopy();

   if (!comp->getOption(TR_DisableArraySetOpts))
      {
      self()->setSupportsArraySet();
      }

   self()->setSupportsScaledIndexAddressing();
   self()->setSupportsConstantOffsetInAddressing();
   self()->setSupportsCompactedLocals();
   self()->setSupportsGlRegDeps();
   self()->setSupportsEfficientNarrowIntComputation();
   self()->setSupportsEfficientNarrowUnsignedIntComputation();
   self()->setSupportsVirtualGuardNOPing();

   // allows [i/l]div to decompose to [i/l]mulh in TreeSimplifier
   //
   static char * enableMulHigh = feGetEnv("TR_X86MulHigh");
   if (enableMulHigh)
      {
      self()->setSupportsLoweringConstIDiv();

      if (TR::Compiler->target.is64Bit())
         self()->setSupportsLoweringConstLDiv();
      }

   if (!comp->getOption(TR_DisableShrinkWrapping))
      self()->setSupportsShrinkWrapping();

   self()->setSpillsFPRegistersAcrossCalls(); // TODO:AMD64: Are the preserved XMMRs relevant here?

   // Make a conservative estimate of the boundary over which an executable instruction cannot
   // be patched.
   //
   int32_t boundary;
   if (_targetProcessorInfo.isGenuineIntel() || (_targetProcessorInfo.isAuthenticAMD() && _targetProcessorInfo.isAMD15h()))
      boundary = 32;
   else
      {
      // TR_AuthenticAMD
      // TR_UnknownVendor
      //
      boundary = 8;
      }

   self()->setInstructionPatchAlignmentBoundary(boundary);

   // Smallest code patching boundary that is known to work on all processors we support.
   //
   self()->setLowestCommonCodePatchingAlignmentBoundary(8);

   self()->setPicSlotCount(0);

   static bool disableX86TRTO = feGetEnv("TR_disableX86TRTO") != NULL;
   static bool disableX86TROT = feGetEnv("TR_disableX86TROT") != NULL;
   if ( comp->cg()->getX86ProcessorInfo().supportsSSE4_1())
      {
      if (!disableX86TRTO)
         self()->setSupportsArrayTranslateTRTO();
      if (!disableX86TROT)
         self()->setSupportsArrayTranslateTROT();
      }
   if (!disableX86TROT && comp->cg()->getX86ProcessorInfo().supportsSSE2())
      {
      self()->setSupportsArrayTranslateTROTNoBreak();
      }

   if (!comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      for (int32_t i = 0; i < TR_numSpillKinds; i++)
         _globalRegisterBitVectors[i].init(self()->getNumberOfGlobalRegisters(), comp->trMemory());

      TR::RealRegister::RegNum vmThreadRealRegisterIndex = _linkageProperties->getMethodMetaDataRegister();
      for (TR_GlobalRegisterNumber grn=0; grn < self()->getNumberOfGlobalRegisters(); grn++)
         {
         TR::RealRegister::RegNum reg = (TR::RealRegister::RegNum)self()->getGlobalRegister(grn);
         if (grn < self()->getFirstGlobalFPR())
            _globalRegisterBitVectors[ TR_gprSpill ].set(grn);
         else
            _globalRegisterBitVectors[ TR_fprSpill ].set(grn);

         if (!self()->getProperties().isPreservedRegister(reg))
            _globalRegisterBitVectors[ TR_volatileSpill ].set(grn);
         if (self()->getProperties().isIntegerArgumentRegister(reg) || self()->getProperties().isFloatArgumentRegister(reg))
            _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);
         if (reg == vmThreadRealRegisterIndex)
            _globalRegisterBitVectors[ TR_vmThreadSpill ].set(grn);
         if (reg == TR::RealRegister::eax)
            _globalRegisterBitVectors[ TR_eaxSpill      ].set(grn);
         if (reg == TR::RealRegister::ecx)
            _globalRegisterBitVectors[ TR_ecxSpill      ].set(grn);
         if (reg == TR::RealRegister::edx)
            _globalRegisterBitVectors[ TR_edxSpill      ].set(grn);
         }
      }

   if (TR::Compiler->target.cpu.isI386())
      self()->setGenerateMasmListingSyntax();

   if (comp->getOptions()->getRegisterAssignmentTraceOption(TR_TraceRARegisterStates))
      {
      self()->setGPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_GPR));
      self()->setFPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_FPR));
      self()->setX87RegisterIterator(new (self()->trHeapMemory()) TR_X86FPStackIterator(self()->machine()));
      }

   self()->setSupportsProfiledInlining();
   }


OMR::X86::CodeGenerator::CodeGenerator() :
   OMR::CodeGenerator(),
   _wordConversionTemp(NULL),
   _doubleWordConversionTemp(NULL),
   _currentTimeMillisTemp(NULL),
   _nanoTimeTemp(NULL),
   _assignmentDirection(Backward),
   _lastCatchAppendInstruction(NULL),
   _betterSpillPlacements(NULL),
   _dataSnippetList(getTypedAllocator<TR::IA32DataSnippet*>(TR::comp()->allocator())),
   _spilledIntRegisters(getTypedAllocator<TR::Register*>(TR::comp()->allocator())),
   _liveDiscardableRegisters(getTypedAllocator<TR::Register*>(TR::comp()->allocator())),
   _dependentDiscardableRegisters(getTypedAllocator<TR::Register*>(TR::comp()->allocator())),
   _clobberingInstructions(getTypedAllocator<TR::ClobberingInstruction*>(TR::comp()->allocator())),
   _outlinedInstructionsList(getTypedAllocator<TR_OutlinedInstructions*>(TR::comp()->allocator())),
   _flags(0)
   {
   _clobIterator = _clobberingInstructions.begin();
   }

TR::Linkage *
OMR::X86::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Compilation *comp = self()->comp();
   TR::Linkage *linkage = NULL;

   switch (lc)
      {
      case TR_Private:
         // HACK HACK HACK "intentional" fall through to system linkage
      case TR_Helper:
         // Intentional fall through
      case TR_System:
         if (TR::Compiler->target.isLinux() || TR::Compiler->target.isOSX())
            {
#if defined(TR_TARGET_64BIT)
            linkage = new (self()->trHeapMemory()) TR::AMD64ABILinkage(self());
#else
            linkage = new (self()->trHeapMemory()) TR::IA32SystemLinkage(self());
#endif
            }
         else if (TR::Compiler->target.isWindows())
            {
#if defined(TR_TARGET_64BIT)
            linkage = new (self()->trHeapMemory()) TR::AMD64Win64FastCallLinkage(self());
#else
            linkage = new (self()->trHeapMemory()) TR::IA32SystemLinkage(self());
#endif
            }
         else
            {
            TR_ASSERT(0, "\nTestarossa error: Illegal linkage convention %d\n", lc);
            }
         break;

      default :
         TR_ASSERT(0, "\nTestarossa error: Illegal linkage convention %d\n", lc);
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

void
OMR::X86::CodeGenerator::beginInstructionSelection()
   {
   TR::Compilation *comp = self()->comp();
   _returnTypeInfoInstruction = NULL;
   TR::ResolvedMethodSymbol * methodSymbol = comp->getJittedMethodSymbol();
   TR::Recompilation * recompilation = comp->getRecompilationInfo();
   TR::Node * startNode = comp->getStartTree()->getNode();

   if (recompilation && recompilation->generatePrePrologue() != NULL)
      {
      // Return type info will have been generated by recompilation info
      //
      if (methodSymbol->getLinkageConvention() == TR_Private)
         _returnTypeInfoInstruction = (TR::X86ImmInstruction*)(self()->getAppendInstruction());

      if (methodSymbol->getLinkageConvention() == TR_System)
         _returnTypeInfoInstruction = (TR::X86ImmInstruction*)(self()->getAppendInstruction());
      }

   if (methodSymbol->getLinkageConvention() == TR_Private && !_returnTypeInfoInstruction)
      {
      // linkageInfo word
      if (self()->getAppendInstruction())
         _returnTypeInfoInstruction = generateImmInstruction(DDImm4, startNode, 0, self());
      else
         _returnTypeInfoInstruction = new (self()->trHeapMemory()) TR::X86ImmInstruction((TR::Instruction *)NULL, DDImm4, 0, self());
      }

   if (methodSymbol->getLinkageConvention() == TR_System && !_returnTypeInfoInstruction)
      {
      // linkageInfo word
      if (self()->getAppendInstruction())
         _returnTypeInfoInstruction = generateImmInstruction(DDImm4, startNode, 0, self());
      else
         _returnTypeInfoInstruction = new (self()->trHeapMemory()) TR::X86ImmInstruction((TR::Instruction *)NULL, DDImm4, 0, self());
      }

   if (self()->getAppendInstruction())
      generateInstruction(PROCENTRY, startNode, self());
   else
      new (self()->trHeapMemory()) TR::Instruction(PROCENTRY, (TR::Instruction *)NULL, self());

   // Set the default FPCW to single precision mode if we are allowed to.
   //
   if (self()->enableSinglePrecisionMethods() && comp->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      TR::IA32ConstantDataSnippet * cds = self()->findOrCreate2ByteConstant(startNode, SINGLE_PRECISION_ROUND_TO_NEAREST);
      generateMemInstruction(LDCWMem, startNode, generateX86MemoryReference(cds, self()), self());
      }
   }

void
OMR::X86::CodeGenerator::endInstructionSelection()
   {
   TR::Compilation *comp = self()->comp();
   if (_returnTypeInfoInstruction != NULL)
      {
      TR_ReturnInfo returnInfo = comp->getReturnInfo();

      // Note: this will get clobbered again in code generation on AMD64
      _returnTypeInfoInstruction->setSourceImmediate(returnInfo);
      }

   // Reset the FPCW in the dummy finally block.
   //
   if (self()->enableSinglePrecisionMethods() &&
       comp->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      TR_ASSERT(self()->getLastCatchAppendInstruction(),
             "endInstructionSelection() ==> Could not find the dummy finally block!\n");

      TR::IA32ConstantDataSnippet * cds = self()->findOrCreate2ByteConstant(self()->getLastCatchAppendInstruction()->getNode(), DOUBLE_PRECISION_ROUND_TO_NEAREST);
      generateMemInstruction(self()->getLastCatchAppendInstruction(), LDCWMem, generateX86MemoryReference(cds, self()), self());
      }
   }

int32_t OMR::X86::CodeGenerator::getMaximumNumbersOfAssignableGPRs()
   {
   return TR::RealRegister::LastAssignableGPR - TR::RealRegister::FirstGPR + 1; // TODO:AMD64: This is including rsp
   }

/*
 * This method returns TRUE for all the cases we decide NOT to replace the call to CAS
 * with inline assembly. The GRA and Evaluator should be consistent about whether to inline CAS natives.
 */
static bool willNotInlineCompareAndSwapNative(TR::Node *node,
      int8_t size,
      TR::Compilation *comp)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::SymbolReference *callSymRef = node->getSymbolReference();
   TR::MethodSymbol *methodSymbol = callSymRef->getSymbol()->castToMethodSymbol();

   if (TR::Compiler->om.canGenerateArraylets() && !node->isUnsafeGetPutCASCallOnNonArray())
      return true;
   static char *disableCASInlining = feGetEnv("TR_DisableCASInlining");

   if (disableCASInlining /* || comp->useCompressedPointers() */)
      return true;

   // In Java9 the sun.misc.Unsafe JNI methods have been moved to jdk.internal,
   // with a set of wrappers remaining in sun.misc to delegate to the new package.
   // We can be called in this function for the wrappers (which we will
   // not be converting to assembly), the new jdk.internal JNI methods or the
   // Java8 sun.misc JNI methods (both of which we will convert). We can
   // differentiate between these cases by testing with isNative() on the method.
   if (!methodSymbol->isNative())
      return true;

   if (size == 4)
      {
      return false;
      }
   else if (size == 8 && TR::Compiler->target.is64Bit())
      {
      return false;
      }
   else
      {
      if (!comp->cg()->getX86ProcessorInfo().supportsCMPXCHG8BInstruction())
         return true;

      return false;
      }
#else
   return true;
#endif
   }


/** @brief Identify methods which are not transformed into inline assembly.

    Some recognized methods are transformed into very simple hardcoded
    assembly sequences which don't need register spills as real calls do.

    @param node The TR::Node for the method call. NB this function assumes the Node is a call.

    @return true if the method will be treated as a normal call, false if the method
    will be converted to inline assembly.
 */
bool OMR::X86::CodeGenerator::willBeEvaluatedAsCallByCodeGen(TR::Node *node, TR::Compilation *comp)
   {
   TR::SymbolReference *callSymRef = node->getSymbolReference();
   TR::MethodSymbol *methodSymbol = callSymRef->getSymbol()->castToMethodSymbol();
   switch (methodSymbol->getRecognizedMethod())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
         return willNotInlineCompareAndSwapNative(node, 8, comp);
      case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
         return willNotInlineCompareAndSwapNative(node, 4, comp);
      case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
         return willNotInlineCompareAndSwapNative(node, (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) ? 8 : 4, comp);
#endif
      default:
         return true;
      }
   }

int32_t OMR::X86::CodeGenerator::getMaximumNumbersOfAssignableFPRs()
   {
   return TR::RealRegister::LastAssignableFPR - TR::RealRegister::FirstFPR + 1;
   }

// X has a different concept of VR's compared to p/z but since LocalOpts needs this to be hoisted within CG,
// this method is placed here as a dummy until platform specific code is removed from LocalOpt
int32_t OMR::X86::CodeGenerator::getMaximumNumbersOfAssignableVRs()
   {
   return INT_MAX;
   }

void OMR::X86::CodeGenerator::addLiveDiscardableRegister(TR::Register * reg)
   {
   _liveDiscardableRegisters.push_front(reg);
   reg->setIsDiscardable();
   }

void OMR::X86::CodeGenerator::removeLiveDiscardableRegister(TR::Register * reg)
   {
   _liveDiscardableRegisters.remove(reg);
   reg->resetIsDiscardable();
   }

bool OMR::X86::CodeGenerator::canNullChkBeImplicit(TR::Node * node)
   {
   return self()->comp()->cg()->canNullChkBeImplicit(node, true);
   }

void OMR::X86::CodeGenerator::clobberLiveDiscardableRegisters(
   TR::Instruction *instr,
   TR::MemoryReference *mr)
   {
   TR::Symbol * symbol = mr->getSymbolReference().getSymbol();

   if (symbol)
      {
      TR::ClobberingInstruction  * clob = NULL;
      TR_IGNode                 *IGNodeSym = NULL;

      if (self()->getLocalsIG())
         IGNodeSym = self()->getLocalsIG()->getIGNodeForEntity(symbol);

      // If this instruction clobbers the source of any memory discardable register(s),
      // those registers must be deactivated (marked as non-discardable) after this point.
      // We record which registers are deactivated, so that we can re-activate them when
      // we have assigned registers for this instruction.
      auto  iterator = self()->getLiveDiscardableRegisters().begin();
      while (iterator != self()->getLiveDiscardableRegisters().end())
         {
         TR::Register *registerCursor = *iterator;
         if (registerCursor->getRematerializationInfo()->isRematerializableFromMemory())
            {
            TR::SymbolReference * rmSymRef = registerCursor->getRematerializationInfo()->getSymbolReference();

            if ((rmSymRef->getSymbol() == symbol) ||
                (rmSymRef->getOffset() == mr->getSymbolReference().getOffset()))
               {
               if (!clob)
                  {
                  clob = new (self()->trHeapMemory()) TR::ClobberingInstruction(instr, self()->trMemory());
                  self()->addClobberingInstruction(clob);
                  }

               clob->addClobberedRegister(registerCursor);
               iterator = self()->getLiveDiscardableRegisters().erase(iterator);
               registerCursor->resetIsDiscardable();

               if (debug("dumpRemat"))
                  {
                  diagnostic("---> Clobbering %s discardable register %s at instruction %p in %s\n",
                              self()->getDebug()->toString(registerCursor->getRematerializationInfo()),
                              self()->getDebug()->getName(registerCursor),
                              instr, self()->comp()->signature());
                  }
               }

            // If the symbols are different but will share the same stack slot then a clobber has occurred.
            //
            else if (IGNodeSym != NULL)
               {
               TR_IGNode *IGNodeRematSym = self()->getLocalsIG()->getIGNodeForEntity(rmSymRef->getSymbol());
               if ((IGNodeRematSym != NULL) &&
                   (IGNodeSym->getColour() == IGNodeRematSym->getColour()))
                  {
                  if (!clob)
                     {
                     clob = new (self()->trHeapMemory()) TR::ClobberingInstruction(instr, self()->trMemory());
                     self()->addClobberingInstruction(clob);
                     }

                  clob->addClobberedRegister(registerCursor);
                  iterator = self()->getLiveDiscardableRegisters().erase(iterator);
                  registerCursor->resetIsDiscardable();

                  if (debug("dumpRemat"))
                     {
                     diagnostic("---> Clobbering %s discardable register %s at instruction %p because of shared slot in %s\n",
                                 self()->getDebug()->toString(registerCursor->getRematerializationInfo()),
                                 self()->getDebug()->getName(registerCursor),
                                 instr, self()->comp()->signature());
                     }
                  }
               else
                  ++iterator;
               }
            else
               ++iterator;
            }
         else
            ++iterator;
         }

      // If a register-dependent discardable register depends on any of the deactivated
      // registers, it (and those that depend on it if any) must also be deactivated.
      //
      if (clob && self()->supportsIndirectMemoryRematerialization())
         {
         iterator = clob->getClobberedRegisters().begin();
         while (iterator != clob->getClobberedRegisters().end())
            {
            self()->clobberLiveDependentDiscardableRegisters(clob, *iterator);
            ++iterator;
            }
         }
      }
   }

// During instruction selection, if we know that an instruction clobbers a
// register, we iteratively deactivate all registers that depend on registers
// that have been clobbered/deactivated.
//
void OMR::X86::CodeGenerator::clobberLiveDependentDiscardableRegisters(TR::ClobberingInstruction * clob,
                                                                    TR::Register              * baseReg)
   {
   TR_Stack<TR::Register *> worklist(self()->trMemory());
   worklist.push(baseReg);

   while (!worklist.isEmpty())
      {
      baseReg = worklist.pop();

      for (auto iterator = self()->getLiveDiscardableRegisters().begin(); iterator != self()->getLiveDiscardableRegisters().end();)
         {
         TR::Register * candidate = *iterator;
         TR_RematerializationInfo * info = candidate->getRematerializationInfo();

         if (info->isIndirect() && info->getBaseRegister() == baseReg)
            {
            clob->addClobberedRegister(candidate);
            iterator = self()->getLiveDiscardableRegisters().erase(iterator);
            candidate->resetIsDiscardable();
            worklist.push(candidate);

            if (debug("dumpRemat"))
               {
               diagnostic("---> Clobbering %s discardable register %s at instruction %p in %s\n",
                           self()->getDebug()->toString(info), self()->getDebug()->getName(candidate), clob->getInstruction(),
                           self()->comp()->signature());
               }
            }
         else
             ++iterator;
         }
      }
   }

// During register assignment, rematerialisable registers dependent on
// a base register B are re-activated when the spill state of B is
// reversed (i.e. when the register assigner inserts a spill store for it).
// A candidate is only re-activated if it is discardable at the current
// program point.
//
void OMR::X86::CodeGenerator::reactivateDependentDiscardableRegisters(TR::Register * baseReg)
   {
   TR_Stack<TR::Register *> worklist(self()->trMemory());
   worklist.push(baseReg);

   if (debug("dumpRemat"))
      diagnostic("---> Re-activating rematerializable registers dependent on %s: ",
                  self()->getDebug()->getName(baseReg));

   while (!worklist.isEmpty())
      {
      baseReg = worklist.pop();

      for (auto iterator = self()->getDependentDiscardableRegisters().begin(); iterator != self()->getDependentDiscardableRegisters().end(); ++iterator)
         {
         if ((*iterator)->isDiscardable() &&
             (*iterator)->getRematerializationInfo()->getBaseRegister() == baseReg)
            {
            if (debug("dumpRemat"))
               diagnostic("%s ", self()->getDebug()->getName(*iterator));

            (*iterator)->getRematerializationInfo()->setActive();

            // If this candidate is currently assigned, then discardable registers
            // dependent on it can also be re-activated.
            //
            if ((*iterator)->getAssignedRegister())
               worklist.push(*iterator);
            }
         }
      }

   if (debug("dumpRemat"))
      diagnostic("\n");
   }

// During register assignment, rematerialisable registers dependent on
// a base register B must be deactivated when B is spilled (i.e. when
// the assigner inserts a reload instruction for it).
//
void OMR::X86::CodeGenerator::deactivateDependentDiscardableRegisters(TR::Register * baseReg)
   {
   TR_Stack<TR::Register *> worklist(self()->trMemory());
   worklist.push(baseReg);

   if (debug("dumpRemat"))
      diagnostic("---> Deactivating rematerialisable registers dependent on %s: ",
                  self()->getDebug()->getName(baseReg));

   while (!worklist.isEmpty())
      {
      baseReg = worklist.pop();

      for (auto iterator = self()->getDependentDiscardableRegisters().begin(); iterator != self()->getDependentDiscardableRegisters().end(); ++iterator)
         {
         if (baseReg == (*iterator)->getRematerializationInfo()->getBaseRegister())
            {
            if (debug("dumpRemat"))
               diagnostic("%s ", self()->getDebug()->getName(*iterator));

            (*iterator)->getRematerializationInfo()->resetActive();
            worklist.push(*iterator);
            }
         }
      }

   if (debug("dumpRemat"))
      diagnostic("\n");
   }

#define ALLOWED_TO_REMATERIALIZE(x) \
   (getRematerializationOptString() && strstr(getRematerializationOptString(), (x)))

#define CAN_REMATERIALIZE(x) \
   (!getRematerializationOptString() || strstr(getRematerializationOptString(), (x)))

static const char *getRematerializationOptString()
   {
   static char *optString = feGetEnv("TR_REMAT");
   return optString;
   }

bool OMR::X86::CodeGenerator::supportsConstantRematerialization()        { static bool b = CAN_REMATERIALIZE("constant"); return b; }
bool OMR::X86::CodeGenerator::supportsLocalMemoryRematerialization()     { static bool b = CAN_REMATERIALIZE("local"); return b; }
bool OMR::X86::CodeGenerator::supportsStaticMemoryRematerialization()    { static bool b = CAN_REMATERIALIZE("static"); return !CANT_REMATERIALIZE_ADDRESSES && b; }
bool OMR::X86::CodeGenerator::supportsXMMRRematerialization()            { static bool b = CAN_REMATERIALIZE("xmmr"); return b; }
bool OMR::X86::CodeGenerator::supportsIndirectMemoryRematerialization()  { static bool b = ALLOWED_TO_REMATERIALIZE("indirect"); return !CANT_REMATERIALIZE_ADDRESSES && b;}
bool OMR::X86::CodeGenerator::supportsAddressRematerialization()         { static bool b = ALLOWED_TO_REMATERIALIZE("address"); return !CANT_REMATERIALIZE_ADDRESSES && b; }

bool OMR::X86::CodeGenerator::allowVMThreadRematerialization()
   {
   return false;
   }

bool OMR::X86::CodeGenerator::supportsFS0VMThreadRematerialization()
   {
   return false;
   }

#undef ALLOWED_TO_REMATERIALIZE
#undef CAN_REMATERIALIZE

bool
OMR::X86::CodeGenerator::getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode, TR::DataType dt)
   {
   /*
    * Most of the vector evaluators for opcodes used in AutoSIMD have been implemented.
    * The cases that return false are placeholders that should be updated as support for more vector evaluators is added.
    */
   // implemented vector opcodes
   switch (opcode.getOpCodeValue())
      {
      case TR::vadd:
      case TR::vsub:
         if (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vmul:
         if (dt == TR::Float || dt == TR::Double || (dt == TR::Int32 && self()->getX86ProcessorInfo().supportsSSE4_1()))
            return true;
         else
            return false;
      case TR::vdiv:
         if (dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vneg:
      case TR::vrem:
         return false;
      case TR::vxor:
      case TR::vor:
      case TR::vand:
         if (dt == TR::Int32 || dt == TR::Int64)
            return true;
         else
            return false;
      case TR::vload:
      case TR::vloadi:
      case TR::vstore:
      case TR::vstorei:
      case TR::vsplats:
         if (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      /*
       * GRA does not work with vector registers on 32 bit due to a bug where xmm registers are not being assigned.
       * This can potentially cause a performance problem in autosimd reductions.
       * This function is where AutoSIMD checks to see if getvelem is suppored for use in reductions.
       * The getvelem case was changed to disable the use of getvelem on 32 bit x86.
       * This code will be reenabled as part of Issue 2035 which tracks the progress of fixing the GRA bug.
       * GRA does not work with vector registers on 64 bit either.
       * getvelem is now being disabled on 64 bit for the same reasons as 32 bit.
       * This code will be reenabled as part of Issue 2280
       */
      case TR::getvelem:
#if 0
         if (TR::Compiler->target.is64Bit() && (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double))
            return true;
         else
#endif //closes the if 0
            return false;
      default:
         return false;
      }

   return false;
   }


bool
OMR::X86::CodeGenerator::getSupportsEncodeUtf16LittleWithSurrogateTest()
   {
   return TR::CodeGenerator::getX86ProcessorInfo().supportsSSE4_1() &&
          !self()->comp()->getOptions()->getOption(TR_DisableSIMDUTF16LEEncoder);
   }

bool
OMR::X86::CodeGenerator::getSupportsEncodeUtf16BigWithSurrogateTest()
   {
   return TR::CodeGenerator::getX86ProcessorInfo().supportsSSE4_1() &&
          !self()->comp()->getOptions()->getOption(TR_DisableSIMDUTF16BEEncoder);
   }

bool
OMR::X86::CodeGenerator::getSupportsIbyteswap()
   {
   return true;
   }

bool
OMR::X86::CodeGenerator::getSupportsBitPermute()
   {
   return true;
   }

bool
OMR::X86::CodeGenerator::supportsMergingGuards()
   {
   return self()->getSupportsVirtualGuardNOPing() &&
          self()->comp()->performVirtualGuardNOPing() &&
          allowGuardMerging();
   }

TR::RealRegister *
OMR::X86::CodeGenerator::getMethodMetaDataRegister()
   {
   return toRealRegister(self()->getVMThreadRegister());
   }

TR::SymbolReference *
OMR::X86::CodeGenerator::getWordConversionTemp()
   {
   if (_wordConversionTemp == NULL)
      {
      _wordConversionTemp = self()->allocateLocalTemp();
      }
   return _wordConversionTemp;
   }

TR::SymbolReference *
OMR::X86::CodeGenerator::getDoubleWordConversionTemp()
   {
   if (_doubleWordConversionTemp == NULL)
      {
      _doubleWordConversionTemp = self()->allocateLocalTemp(TR::Int64);
      }
   return _doubleWordConversionTemp;
   }

TR::SymbolReference *
OMR::X86::CodeGenerator::findOrCreateCurrentTimeMillisTempSymRef()
   {
   if (_currentTimeMillisTemp == NULL)
      {
      int32_t symSize;

#if defined(LINUX) || defined(OSX)
      symSize = sizeof(struct timeval);
#else
      symSize = 8;
#endif

      TR::AutomaticSymbol *sym = TR::AutomaticSymbol::create(self()->trHeapMemory(),TR::Aggregate,symSize);
      self()->comp()->getMethodSymbol()->addAutomatic(sym);
      _currentTimeMillisTemp = new (self()->trHeapMemory()) TR::SymbolReference(self()->comp()->getSymRefTab(), sym);
      }

   return _currentTimeMillisTemp;
   }

TR::SymbolReference *
OMR::X86::CodeGenerator::getNanoTimeTemp()
   {
   if (_nanoTimeTemp == NULL)
      {
      TR::AutomaticSymbol *sym;
#if defined(LINUX) || defined(OSX)
      sym = TR::AutomaticSymbol::create(self()->trHeapMemory(),TR::Aggregate,sizeof(struct timeval));
#else
      sym = TR::AutomaticSymbol::create(self()->trHeapMemory(),TR::Aggregate,8);
#endif
      self()->comp()->getMethodSymbol()->addAutomatic(sym);
      _nanoTimeTemp = new (self()->trHeapMemory()) TR::SymbolReference(self()->comp()->getSymRefTab(), sym);
      }
   return _nanoTimeTemp;
   }

bool
OMR::X86::CodeGenerator::canTransformUnsafeCopyToArrayCopy()
   {
   return !self()->comp()->getOption(TR_DisableArrayCopyOpts);
   }

void OMR::X86::CodeGenerator::saveBetterSpillPlacements(TR::Instruction * branchInstruction)
   {
   // Get the set of currently-free real registers as a bitmap.
   //
   int32_t numFreeRealRegisters = 0;
   uint32_t freeRealRegisters = 0;
   int32_t i;
   TR::RealRegister * realReg;

   for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
      {
      realReg = self()->machine()->getX86RealRegister((TR::RealRegister::RegNum)i);

      // Skip non-assignable registers
      //
      if (realReg->getState() == TR::RealRegister::Locked)
         continue;

      if (realReg->getAssignedRegister() == NULL)
         {
         numFreeRealRegisters++;
         freeRealRegisters |= TR::RealRegister::getRealRegisterMask(realReg->getKind(), realReg->getRegisterNumber());
         }
      }

   if (!freeRealRegisters)
      return;

   // Add the current spilled virtual registers and this instruction to the list
   // of candidates for better spill placement.
   // For each, remember the current set of free real registers.
   //
   for (auto regElement = _spilledIntRegisters.begin(); regElement != _spilledIntRegisters.end() && numFreeRealRegisters; ++regElement)
      {
      // For now only consider non-collected registers. Need to add
      // a mechanism for modifying the GC information for intervening
      // instructions in order to handle collected references.
      //
      if ((*regElement)->containsCollectedReference() || (*regElement)->containsInternalPointer() || (*regElement)->hasBetterSpillPlacement())
         continue;

      self()->traceRegisterAssignment("Saved better spill placement for %R, mask = %x.", *regElement, freeRealRegisters);

      TR_BetterSpillPlacement * info = new (self()->trHeapMemory()) TR_BetterSpillPlacement;
      info->_virtReg                = *regElement;
      info->_freeRealRegs           = freeRealRegisters;
      info->_branchInstruction      = branchInstruction;
      info->_prev                   = 0;
      info->_next                   = _betterSpillPlacements;
      if (info->_next)
         info->_next->_prev = info;
      _betterSpillPlacements = info;
      (*regElement)->setHasBetterSpillPlacement(true);
      }
   }

void OMR::X86::CodeGenerator::removeBetterSpillPlacementCandidate(TR::RealRegister * realReg)
   {
   // Remove the given real register as a candidate for better spill placement
   // of any virtual registers.
   //
   int32_t regNum = realReg->getRegisterNumber();
   uint32_t mask = ~TR::RealRegister::getRealRegisterMask(realReg->getKind(), realReg->getRegisterNumber()); // TODO:AMD64: Use the proper mask value
   TR_BetterSpillPlacement * info, * next;

   if (_betterSpillPlacements)
     self()->traceRegisterAssignment("Removed better spill placement candidate %d.", regNum);

   for (info = _betterSpillPlacements, next = NULL; info; info = next)
      {
      next = info->_next;
      info->_freeRealRegs &= mask;
      if (info->_freeRealRegs == 0)
         {
         // Remove the better spill placement info from the list.
         //
         if (info->_prev)
            info->_prev->_next = info->_next;
         else
            _betterSpillPlacements = info->_next;
         if (info->_next)
            info->_next->_prev = info->_prev;

         // Mark the virtual register as not a candidate for better spill
         // placement.
         //
         info->_virtReg->setHasBetterSpillPlacement(false);
         self()->traceRegisterAssignment("%R is no longer a candidate for better spill placement.", info->_virtReg);
         }
      }
   }

TR::Instruction *
OMR::X86::CodeGenerator::findBetterSpillPlacement(
      TR::Register *virtReg,
      int32_t realRegNum)
   {
   TR::Instruction          * placement;
   TR_BetterSpillPlacement * info;
   for (info = _betterSpillPlacements; info; info = info->_next)
      {
      if (info->_virtReg == virtReg)
         break;
      }
   if (info && (info->_freeRealRegs & TR::RealRegister::getRealRegisterMask(virtReg->getKind(), (TR::RealRegister::RegNum)realRegNum)))
      {
      placement = info->_branchInstruction;
      self()->traceRegisterAssignment("Successful better spill placement for %R at [" POINTER_PRINTF_FORMAT "].", virtReg, placement);
      }
   else
      {
      placement = NULL;
      self()->traceRegisterAssignment("Failed better spill placement for %R.", virtReg);
      }

   // Remove the better spill placement info from the list.
   //
   if (info->_prev)
      info->_prev->_next = info->_next;
   else
      _betterSpillPlacements = info->_next;
   if (info->_next)
      info->_next->_prev = info->_prev;

   // Mark the virtual register as not a candidate for better spill placement.
   //
   info->_virtReg->setHasBetterSpillPlacement(false);

   return placement;
   }


void
OMR::X86::CodeGenerator::performNonLinearRegisterAssignmentAtBranch(
      TR::X86LabelInstruction *branchInstruction,
      TR_RegisterKinds kindsToBeAssigned)
   {
   TR::Machine *xm = self()->machine();
   TR_RegisterAssignerState *branchRAState = new (self()->trHeapMemory()) TR_RegisterAssignerState(xm);

   // Take a snapshot of the current register assigner state.
   //
   branchRAState->capture();

   TR_OutlinedInstructions *oi =
      self()->findOutlinedInstructionsFromLabel(branchInstruction->getLabelSymbol());

   TR_ASSERT(oi, "Could not find OutlinedInstructions from branch label on instr=%p\n", branchInstruction);

   // Restore the register use counts on the registers used in the out-of-line path
   // to accurately reflect their state.
   //
   TR::list<OMR::RegisterUsage*> *outlinedRUL = oi->getOutlinedPathRegisterUsageList();
   if (outlinedRUL)
      {
      xm->adjustRegisterUseCountsUp(outlinedRUL, true);
      }

   // Adjust the register use counts based on the contributions from the mainline
   // path.  Note that the future use counts have already been adjusted because
   // the mainline path has been register assigned already.
   //
   TR::list<OMR::RegisterUsage*> *mainlineRUL = oi->getMainlinePathRegisterUsageList();
   if (mainlineRUL)
      {
      xm->adjustRegisterUseCountsDown(mainlineRUL, false);
      }

   // Create register dependencies from the current register assigner state and
   // attach them after the start of the out-of-line sequence.
   //
   TR::RegisterDependencyConditions *deps =
      branchRAState->createDependenciesFromRegisterState(oi);

   if (deps)
      {
      TR::Instruction *ins =
         generateLabelInstruction(oi->getFirstInstruction(), LABEL, generateLabelSymbol(self()), deps, self());

      if (self()->comp()->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
         {
         traceMsg(self()->comp(), "creating LABEL instruction %p for dependencies\n", ins);
         }
      }

   // Restore the register assigner state as it was at the merge point.
   //
   oi->getRegisterAssignerStateAtMerge()->install();

   // Kill any live register whose future use count has been exhausted.  This can happen
   // when a register is live at the merge point, dies in the mainline path, and is not
   // used in the OOL path.
   //
   xm->purgeDeadRegistersFromRegisterFile();

   // Do the register assignment.
   //
   TR_ASSERT(!oi->hasBeenRegisterAssigned(), "outlined instructions should not have been register assigned already");
   oi->assignRegistersOnOutlinedPath(kindsToBeAssigned, generateVFPSaveInstruction(branchInstruction->getPrev(), self()));

   // Restore the register use counts on the registers used in the mainline path
   // to accurately reflect their state.
   //
   // This basically amounts to adjusting the total use counts back to where they
   // were.  This may not really be necessary...
   //
   if (mainlineRUL)
      {
      xm->adjustRegisterUseCountsUp(mainlineRUL, false);
      }

   // Unlock the free spill list.
   //
   // TODO: live registers that are not spilled at this point should have their backing
   // storage returned to the free spill list.
   //
   self()->unlockFreeSpillList();

   // Disassociate backing storage that was previously reserved for a spilled virtual if
   // virtual is no longer spilled.  This occurs because the the free spill list was
   // locked.
   //
   xm->disassociateUnspilledBackingStorage();

   }


void OMR::X86::CodeGenerator::prepareForNonLinearRegisterAssignmentAtMerge(
      TR::X86LabelInstruction *mergeInstruction)
   {
   TR::Machine *xm = self()->machine();
   TR_RegisterAssignerState *ras = new (self()->trHeapMemory()) TR_RegisterAssignerState(xm);

   // Take a snapshot of the current register assigner state.
   //
   ras->capture();

   TR_OutlinedInstructions *oi =
      self()->findOutlinedInstructionsFromMergeLabel(mergeInstruction->getLabelSymbol());

   TR_ASSERT(oi, "Could not find OutlinedInstructions from merge label on instr=%p\n", mergeInstruction);

   TR::list<OMR::RegisterUsage *> *outlinedRUL = oi->getOutlinedPathRegisterUsageList();

   if (outlinedRUL)
      {
      // Reset the register use counts on the registers used in the out-of-line path
      // to accurately reflect their use on the mainline path.
      //
      xm->adjustRegisterUseCountsDown(outlinedRUL, true);
      }

   // Cache the register assigner state at the merge point in the outlined
   // instructions object.
   //
   oi->setRegisterAssignerStateAtMerge(ras);

   // Prevent spilled registers from reclaiming their backing store if they
   // become unspilled.  This will ensure a spilled register will receive the
   // same backing store if it is spilled on either path of the control flow.
   //
   self()->lockFreeSpillList();
   }


void OMR::X86::CodeGenerator::processClobberingInstructions(TR::ClobberingInstruction * clobInstructionCursor, TR::Instruction *instructionCursor)
   {
   // Activate any discardable registers that this instruction may have clobbered.
   //
   // Use a loop just in case an instruction got added twice.  (For
   // efficiency's sake, we shouldn't add two ClobberingInstructions for a
   // single Instruction, but for practical reasons, we might.  If we do,
   // for whatever reason, we must deal with them properly for correctness.)
   //
   while (clobInstructionCursor &&
    (clobInstructionCursor->getInstruction() == instructionCursor) && self()->enableRematerialisation())
      {
      auto regIterator = clobInstructionCursor->getClobberedRegisters().begin();
      while (regIterator != clobInstructionCursor->getClobberedRegisters().end())
         {
         (*regIterator)->setIsDiscardable();

         // If the discardable register is dependent, it can only be activated at a
         // clobbering instruction if its base register is currently assigned to a
         // real register.
         //
         TR_RematerializationInfo * info = (*regIterator)->getRematerializationInfo();

         if (!info->isIndirect() || info->getBaseRegister()->getAssignedRegister())
            {
            info->setActive();

            if (debug("dumpRemat"))
               {
               diagnostic("---> Activating %s discardable register %s at instruction %p in %s\n",
                           self()->getDebug()->toString(info), self()->getDebug()->getName(*regIterator), instructionCursor,
                           self()->comp()->signature());
               }
            }

         regIterator++;
         }
      if(_clobIterator == --(_clobberingInstructions.end()))
         clobInstructionCursor = 0;
      else if(_clobIterator == _clobberingInstructions.end())
         clobInstructionCursor = 0;
      else
         {
         ++_clobIterator;
         clobInstructionCursor = *_clobIterator;
         }
      }
   }

void OMR::X86::CodeGenerator::doBackwardsRegisterAssignment(
      TR_RegisterKinds kindsToAssign,
      TR::Instruction *instructionCursor,
      TR::Instruction *appendInstruction)
   {
   TR::Compilation *comp = self()->comp();
   TR::Instruction *prevInstruction;

#ifdef DEBUG
   TR::Instruction *origNextInstruction;
   bool dumpPreGP = (debug("dumpGPRA") || debug("dumpGPRA0")) && comp->getOutFile() != NULL;
   bool dumpPostGP = (debug("dumpGPRA") || debug("dumpGPRA1")) && comp->getOutFile() != NULL;
#endif

   if (self()->getUseNonLinearRegisterAssigner())
      {
      if (!self()->getSpilledRegisterList())
         {
         self()->setSpilledRegisterList(new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::Register*>(comp->allocator())));
         }
      }

   if (self()->getDebug())
      self()->getDebug()->startTracingRegisterAssignment("backward", kindsToAssign);

   while (instructionCursor && instructionCursor != appendInstruction)
      {
      TR::Instruction  *inst = instructionCursor;

#ifdef DEBUG
      if (dumpPreGP)
         {
         origNextInstruction = instructionCursor->getNext();
         self()->dumpPreGPRegisterAssignment(instructionCursor);
         }
#endif
      self()->tracePreRAInstruction(instructionCursor);

      prevInstruction = instructionCursor->getPrev();
      instructionCursor->assignRegisters(kindsToAssign);
      //code to increment or decrement counter when a internal control flow end or start label is hit
      TR::LabelSymbol *label;
      if (((TR::X86LabelInstruction *)instructionCursor)->getOpCodeValue() == LABEL && (label = ((TR::X86LabelInstruction *)instructionCursor)->getLabelSymbol()))
      {
         if (label->isStartInternalControlFlow())
         {
         self()->decInternalControlFlowNestingDepth();
         }
      else if (label->isEndInternalControlFlow())
         {
         self()->incInternalControlFlowNestingDepth();
         }
      }

      self()->freeUnlatchedRegisters();

      self()->buildGCMapsForInstructionAndSnippet(instructionCursor);

#ifdef DEBUG
      if (dumpPostGP)
         self()->dumpPostGPRegisterAssignment(instructionCursor, origNextInstruction);
#endif
      self()->tracePostRAInstruction(instructionCursor);
      TR::ClobberingInstruction * clobInst;
      if(_clobIterator == self()->getClobberingInstructions().end())
         clobInst = 0;
      else
         clobInst = *_clobIterator;
      self()->processClobberingInstructions(clobInst, instructionCursor);

      // Skip over any instructions that may have been inserted prior to the
      // current instruction.  Any such instructions will already have real
      // registers assigned for them.
      //
      instructionCursor = prevInstruction;
      }

   if (self()->getDebug())
      self()->getDebug()->stopTracingRegisterAssignment();
   }


void OMR::X86::CodeGenerator::doRegisterAssignment(TR_RegisterKinds kindsToAssign)
   {
   TR::Instruction *instructionCursor;
   TR::Instruction *nextInstruction;

#if defined(DEBUG)
   TR::Instruction *origPrevInstruction;
   bool            dumpPreFP = (debug("dumpFPRA") || debug("dumpFPRA0")) && self()->comp()->getOutFile() != NULL;
   bool            dumpPostFP = (debug("dumpFPRA") || debug("dumpFPRA1")) && self()->comp()->getOutFile() != NULL;
   bool            dumpPreGP = (debug("dumpGPRA") || debug("dumpGPRA0")) && self()->comp()->getOutFile() != NULL;
   bool            dumpPostGP = (debug("dumpGPRA") || debug("dumpGPRA1")) && self()->comp()->getOutFile() != NULL;
#endif

   LexicalTimer pt1("total register assignment", self()->comp()->phaseTimer());

   // Assign FPRs in a forward pass
   //
   if (kindsToAssign & TR_X87_Mask)
      {
      if (self()->getDebug())
         self()->getDebug()->startTracingRegisterAssignment("forward", TR_X87_Mask);

#if defined(DEBUG)
      if (dumpPreFP || dumpPostFP)
         diagnostic("\n\nFP Register Assignment (forward pass):\n");
#endif

      LexicalTimer pt2("FP register assignment", self()->comp()->phaseTimer());

      self()->setAssignmentDirection(Forward);
      instructionCursor = self()->getFirstInstruction();
      while (instructionCursor)
         {
         self()->tracePreRAInstruction(instructionCursor);
#if defined(DEBUG)
         if (dumpPreFP)
            {
            origPrevInstruction = instructionCursor->getPrev();
            self()->dumpPreFPRegisterAssignment(instructionCursor);
            }
#endif
         nextInstruction = instructionCursor->getNext();
         instructionCursor->assignRegisters(TR_X87_Mask);

#if defined(DEBUG)
         if (dumpPostFP)
            self()->dumpPostFPRegisterAssignment(instructionCursor, origPrevInstruction);
#endif
         self()->tracePostRAInstruction(instructionCursor);

         instructionCursor = nextInstruction;
         }

      if (self()->getDebug())
         self()->getDebug()->stopTracingRegisterAssignment();
      }

   // Use new float/double slots for XMMR spills, to avoid
   // interfering with existing FPR spills.
   //
   self()->jettisonAllSpills();

#if defined(DEBUG)
   if (dumpPreGP || dumpPostGP)
      diagnostic("\n\nGP Register Assignment (backward pass):\n");
#endif

   LexicalTimer pt2("GP register assignment", self()->comp()->phaseTimer());
   // Assign GPRs and XMMRs in a backward pass
   //
   kindsToAssign = TR_RegisterKinds(kindsToAssign & (TR_GPR_Mask | TR_FPR_Mask | TR_VRF_Mask));
   if (kindsToAssign)
      {
      self()->getVMThreadRegister()->setFutureUseCount(self()->getVMThreadRegister()->getTotalUseCount());
      self()->setAssignmentDirection(Backward);
      self()->getFrameRegister()->setFutureUseCount(self()->getFrameRegister()->getTotalUseCount());

      if (self()->enableRematerialisation())
         _clobIterator = self()->getClobberingInstructions().begin();

      if (self()->enableRegisterAssociations())
         self()->machine()->setGPRWeightsFromAssociations();

      self()->doBackwardsRegisterAssignment(kindsToAssign, self()->getAppendInstruction());
      }
   }

bool OMR::X86::CodeGenerator::isReturnInstruction(TR::Instruction *instr)
   {
   if (instr->getOpCodeValue() == RET ||
       instr->getOpCodeValue() == RETImm2 ||
       instr->getOpCodeValue() == ReturnMarker
      )
      return true;
   else
      return false;
   }

bool OMR::X86::CodeGenerator::isBranchInstruction(TR::Instruction *instr)
   {
   return (instr->getOpCode().isBranchOp() || instr->getOpCode().getOpCodeValue() == CALLImm4 ? true : false);
   }

bool OMR::X86::CodeGenerator::isLabelInstruction(TR::Instruction *instr)
   {
   TR::Instruction *x86Instr = instr;
   return (x86Instr->getKind() == TR::Instruction::IsLabel);
   }

int32_t OMR::X86::CodeGenerator::isFenceInstruction(TR::Instruction *instr)
   {
   TR::Instruction *x86Instr = instr;

   if (x86Instr->getKind() == TR::Instruction::IsFence)
      {
      TR::Node *fenceNode = x86Instr->getNode();
      if (fenceNode->getOpCodeValue() == TR::BBStart)
         return 1;
      else if (fenceNode->getOpCodeValue() == TR::BBEnd)
         return 2;
      }
   return 0;
   }

bool OMR::X86::CodeGenerator::isAlignmentInstruction(TR::Instruction *instr)
   {
   return (instr->getKind() == TR::Instruction::IsAlignment);
   }

void OMR::X86::CodeGenerator::doBinaryEncoding()
   {
   LexicalTimer pt1("code generation", self()->comp()->phaseTimer());

   // Generate fixup code for the interpreter entry point right before PROCENTRY
   //
   TR::Instruction * procEntryInstruction = self()->getFirstInstruction();
   while (procEntryInstruction && procEntryInstruction->getOpCodeValue() != PROCENTRY)
      {
      procEntryInstruction = procEntryInstruction->getNext();
      }

   TR::Instruction * interpreterEntryInstruction;
   if (TR::Compiler->target.is64Bit())
      {
      if (self()->comp()->getMethodSymbol()->getLinkageConvention() != TR_System)
         interpreterEntryInstruction = self()->getLinkage()->copyStackParametersToLinkageRegisters(procEntryInstruction);
      else
         interpreterEntryInstruction = procEntryInstruction;

      // Patching can occur at the jit entry point, so insert padding if necessary.
      //
      if (TR::Compiler->target.isSMP())
         {
         TR::Recompilation * recompilation = self()->comp()->getRecompilationInfo();
         const TR_AtomicRegion *atomicRegions;
#ifdef J9_PROJECT_SPECIFIC
         if (recompilation && !recompilation->useSampling())
            {
            // Counting recomp can patch a 5-byte call instruction
            static const TR_AtomicRegion countingAtomicRegions[] = { {0,5}, {0,0} };
            atomicRegions = countingAtomicRegions;
            }
         else
#endif
            {
            // It's safe to protect just the first 2 bytes because we won't patch anything other than a 2-byte jmp
            static const TR_AtomicRegion samplingAtomicRegions[] = { {0,2}, {0,0} };
            atomicRegions = samplingAtomicRegions;
            }

         TR::Instruction *pcai = generatePatchableCodeAlignmentInstruction(atomicRegions, procEntryInstruction, self());
         if (interpreterEntryInstruction == procEntryInstruction)
            {
            // Interpreter prologue contains no instructions other than this
            // nop, so the nop becomes the interpreter entry instruction
            interpreterEntryInstruction = pcai;
            }
         }
      }
   else
      interpreterEntryInstruction = procEntryInstruction;

   /////////////////////////////////////////////////////////////////
   //
   // Pass 1: Binary length estimation and prologue creation
   //

   if (self()->comp()->getOption(TR_TraceCG))
      {
      traceMsg(self()->comp(), "<proepilogue>\n");
      }

   TR::Instruction * estimateCursor = self()->getFirstInstruction();
   int32_t estimate = 0;

   // Estimate the binary length up to PROCENTRY
   //
   while (estimateCursor && estimateCursor->getOpCodeValue() != PROCENTRY)
      {
      estimate       = estimateCursor->estimateBinaryLength(estimate);
      estimateCursor = estimateCursor->getNext();
      }

   /* Set offset for jitted method entry alignment */
   if (self()->comp()->getRecompilationInfo())
      {
      TR_ASSERT(estimate >= 3, "Estimate should not be less than 3");
      self()->setPreJitMethodEntrySize(estimate - 3);
      }
   else
      self()->setPreJitMethodEntrySize(estimate);

   // Emit the spill instruction to set up the vmThread reloads if needed
   // (i.e., if the vmThread was ever spilled to make room for another register)
   //
   TR::Instruction *vmThreadSpillCursor = self()->getVMThreadSpillInstruction();
   if (vmThreadSpillCursor && self()->getVMThreadRegister()->getBackingStorage())
      {
      if (vmThreadSpillCursor == (TR::Instruction *)0xffffffff ||
         self()->comp()->mayHaveLoops())
         {
         vmThreadSpillCursor = estimateCursor;
         }

      new (self()->trHeapMemory()) TR::X86MemRegInstruction(vmThreadSpillCursor,
                                 SMemReg(),
                                 generateX86MemoryReference(self()->getVMThreadRegister()->getBackingStorage()->getSymbolReference(), self()),
                                 self()->machine()->getX86RealRegister(_linkageProperties->getMethodMetaDataRegister()), self());
      }

   // Create prologue
   //
   TR::Instruction * prologueCursor = estimateCursor;

   // The recompilation prologue
   //
   TR::Recompilation * recompilation = self()->comp()->getRecompilationInfo();
   if (recompilation)
      prologueCursor = recompilation->generatePrologue(prologueCursor);

   // Establish the VFP ground state.
   // This instruction actually ends up immediately AFTER the prologue.
   //
   _vfpResetInstruction = generateVFPSaveInstruction(prologueCursor, self());

   self()->getLinkage()->createPrologue(prologueCursor); // The linkage prologue

   for (TR::Instruction *gcMapCursor = prologueCursor; gcMapCursor != _vfpResetInstruction; gcMapCursor = gcMapCursor->getNext())
      {
      if (gcMapCursor->needsGCMap())
         gcMapCursor->setGCMap(self()->getStackAtlas()->getParameterMap()->clone(self()->trMemory()));
      }

   /* Adjust estimate based on jitted method entry alignment requirement */
   uintptr_t boundary = self()->comp()->getOptions()->getJitMethodEntryAlignmentBoundary(self());
   if (boundary && (boundary & boundary - 1) == 0)
      estimate += boundary - 1;

   if (self()->comp()->getOption(TR_TraceVFPSubstitution))
      traceMsg(self()->comp(), "\n<instructions\n"
                                "\ttitle=\"VFP Substitution\">");

   // Estimate instruction length of prologue and remainder of method,
   // determine adjustments if using esp-relative addressing, and generate
   // epilogues.
   //
   bool skipOneReturn = false;
   int32_t estimatedPrologueStartOffset = estimate;
   while (estimateCursor)
      {
      // Update the info bits on the register mask.
      //
      if (estimateCursor->needsGCMap())
         {
         uint32_t mask = estimateCursor->getGCMap()->getRegisterMap();
         uint32_t numSlotPushes = (mask & 0x00ff0000) >> 16;

         if (numSlotPushes == 0)
            {
            // The GCMap's info bits should contain the number of slots pushed
            // on the Java stack on top of the VFP "ground state"; that is, the
            // depth of the stack relative to what it was at the end of the
            // prologue.
            //
            // Since the VFP state mechanism has no concept of "Java stack" versus
            // "native stack", we approximate this by checking whether the current
            // VFP register is esp.  If not, we assume we're in the middle of a
            // native call, in which case the number of pushes is zero.
            //
            // TODO: This is not very robust.  The VFP mechanism really needs
            // to know about Java stack vs. native stack, and to track pushes
            // on the Java stack regardless of which register is the current VFP.
            //
            if (_vfpState._register == TR::RealRegister::esp)
               {
               // TODO: Is it ok to assume that nothing in the prologue needs a GC
               // map, and that therefore to assume that the _vfpResetInstruction has
               // already had its internal VFP state established before we get here?
               //
               estimateCursor->getGCMap()->setInfoBits(
                  (_vfpState._displacement - _vfpResetInstruction->getSavedState()._displacement)<<14);
               }
            else
               {
               estimateCursor->getGCMap()->setInfoBits(0<<14);
               }
            }
         }

      // Insert epilogue before each RET.
      //
      if (self()->isReturnInstruction(estimateCursor))
         {
         if (skipOneReturn == false)
            {
            // Generate epilogue
            //
            TR::Instruction * temp = estimateCursor->getPrev();
            self()->getLinkage()->createEpilogue(temp);

            // Resume estimation from the first instruction of the epilogue
            //
            if (estimateCursor == temp->getNext())
               {
               // Epilogue is empty
               }
            else
               {
               estimateCursor = temp->getNext();

               // Make sure we don't process the same RET again when we hit it
               // at the end of the epilogue.
               //
               skipOneReturn = true;
               }

            }
         else
            {
            // We've already seen this RET; don't process it again.
            //
            skipOneReturn = false;
            }
         }

      estimate = estimateCursor->estimateBinaryLength(estimate);
      TR_VFPState prevState = _vfpState;
      estimateCursor->adjustVFPState(&_vfpState, self());

      if (self()->comp()->getOption(TR_TraceVFPSubstitution))
         self()->getDebug()->dumpInstructionWithVFPState(estimateCursor, &prevState);

      if (estimateCursor == _vfpResetInstruction)
         self()->generateDebugCounter(estimateCursor, "cg.prologues:#instructionBytes", estimate - estimatedPrologueStartOffset, TR::DebugCounter::Expensive);

      estimateCursor = estimateCursor->getNext();
      }

   if (self()->comp()->getOption(TR_TraceVFPSubstitution))
      traceMsg(self()->comp(), "\n</instructions>\n");

   estimate = self()->setEstimatedLocationsForSnippetLabels(estimate);
   // When using copyBinaryToBuffer() to copy the encoding of an instruction we
   // indiscriminatelly copy a whole integer, even if the size of the encoding
   // is less than that. This may cause the write to happen beyond the allocated
   // area. If the memory allocated for this method comes from a reclaimed
   // block, then the write could potentially destroy data that resides in the
   // adjacent block. For this reason it is better to overestimate
   // the allocated size by 4.
   #define OVER_ESTIMATION 4
   self()->setEstimatedCodeLength(estimate+OVER_ESTIMATION);

   if (self()->comp()->getOption(TR_TraceCG))
      {
      traceMsg(self()->comp(), "</proepilogue>\n");
      }

   /////////////////////////////////////////////////////////////////
   //
   // Pass 2: Binary encoding
   //

   if (self()->comp()->getOption(TR_TraceCG))
      {
      traceMsg(self()->comp(), "<encode>\n");
      }

   uint8_t * coldCode = NULL;
   uint8_t * temp = self()->allocateCodeMemory(self()->getEstimatedCodeLength(), 0, &coldCode);
   TR_ASSERT(temp, "Failed to allocate primary code area.");

   if (TR::Compiler->target.is64Bit() && self()->comp()->getCodeCacheSwitched() && self()->getPicSlotCount() != 0)
      {
      int32_t numTrampolinesToReserve = self()->getPicSlotCount() - self()->comp()->getNumReservedIPICTrampolines();
      TR_ASSERT(numTrampolinesToReserve >= 0, "Discrepancy with number of IPIC trampolines to reserve getPicSlotCount()=%d getNumReservedIPICTrampolines()=%d",
         self()->getPicSlotCount(), self()->comp()->getNumReservedIPICTrampolines());
      reserveNTrampolines(numTrampolinesToReserve);
      }

   self()->setBinaryBufferStart(temp);
   self()->setBinaryBufferCursor(temp);
   self()->alignBinaryBufferCursor();

   TR::Instruction * cursorInstruction = self()->getFirstInstruction();

   // Generate binary for all instructions before the interpreter entry point
   //
   while (cursorInstruction && cursorInstruction != interpreterEntryInstruction)
      {
      self()->setBinaryBufferCursor(cursorInstruction->generateBinaryEncoding());
      cursorInstruction = cursorInstruction->getNext();
      }

   // Now we know the buffer size before the entry point
   //
   self()->setPrePrologueSize(self()->getBinaryBufferLength());

#ifdef J9_PROJECT_SPECIFIC
   if ((!self()->comp()->getOptions()->getOption(TR_DisableGuardedCountingRecompilations) && TR::Options::getCmdLineOptions()->allowRecompilation()) ||
       (self()->comp()->getOptions()->getEnableGPU(TR_EnableGPU) && self()->comp()->hasIntStreamForEach()) ||
       self()->comp()->isJProfilingCompilation())
#else
   if (!self()->comp()->getOptions()->getOption(TR_DisableGuardedCountingRecompilations) &&
       TR::Options::getCmdLineOptions()->allowRecompilation())
#endif
      self()->comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()->getSymbol()->getStaticSymbol()->setStaticAddress(self()->getBinaryBufferCursor());

   // Generate binary for the rest of the instructions
   //
   while (cursorInstruction)
      {
      uint8_t * const instructionStart = self()->getBinaryBufferCursor();
      self()->setBinaryBufferCursor(cursorInstruction->generateBinaryEncoding());
      TR_ASSERT(cursorInstruction->getEstimatedBinaryLength() >= self()->getBinaryBufferCursor() - instructionStart,
              "Instruction length estimate must be conservatively large (instr=%s, opcode=%s, estimate=%d, actual=%d",
              self()->getDebug()? self()->getDebug()->getName(cursorInstruction) : "(unknown)",
              self()->getDebug()? self()->getDebug()->getOpCodeName(&cursorInstruction->getOpCode()) : "(unknown)",
              cursorInstruction->getEstimatedBinaryLength(),
              self()->getBinaryBufferCursor() - instructionStart);

      if (TR::Compiler->target.is64Bit() &&
          (cursorInstruction->getOpCodeValue() == PROCENTRY))
         {
         // A hack to set the linkage info word
         //
         TR_ASSERT(_returnTypeInfoInstruction->getOpCodeValue() == DDImm4, "assertion failure");
         uint32_t magicWord = ((self()->getBinaryBufferCursor()-self()->getCodeStart())<<16) | static_cast<uint32_t>(self()->comp()->getReturnInfo());
         uint32_t recompFlag = 0;
         TR::Recompilation * recomp = self()->comp()->getRecompilationInfo();

#ifdef J9_PROJECT_SPECIFIC
         if (recomp !=NULL && recomp->couldBeCompiledAgain())
            {
            recompFlag = (recomp->useSampling())?METHOD_SAMPLING_RECOMPILATION:METHOD_COUNTING_RECOMPILATION;
            }
#endif
         magicWord |= recompFlag;
         _returnTypeInfoInstruction->setSourceImmediate(magicWord);
         *(uint32_t*)(_returnTypeInfoInstruction->getBinaryEncoding()) = magicWord;
         }

      self()->addToAtlas(cursorInstruction);
      cursorInstruction = cursorInstruction->getNext();
      }

   // Create exception table entries for outlined instructions.
   //
   auto oiIterator = self()->getOutlinedInstructionsList().begin();
   while (oiIterator != self()->getOutlinedInstructionsList().end())
      {
      uint32_t startOffset = (*oiIterator)->getFirstInstruction()->getBinaryEncoding() - self()->getCodeStart();
      uint32_t endOffset   = (*oiIterator)->getAppendInstruction()->getBinaryEncoding() - self()->getCodeStart();

      TR::Block * block = (*oiIterator)->getBlock();
      bool      needsETE = (*oiIterator)->getCallNode() && (*oiIterator)->getCallNode()->getSymbolReference()->canCauseGC();

      if (needsETE && block && !block->getExceptionSuccessors().empty())
         block->addExceptionRangeForSnippet(startOffset, endOffset);

      ++oiIterator;
      }

#ifdef J9_PROJECT_SPECIFIC
   // Place an assumption that gcrPatchPointSymbol reference has been updated
   // with the address of the byte to be patched. Otherwise, fail this compilation
   // and retry without GCR.
   if (self()->comp()->getOption(TR_EnableGCRPatching) &&
       self()->comp()->getRecompilationInfo() &&
       self()->comp()->getRecompilationInfo()->getJittedBodyInfo()->getUsesGCR())
      {
      void * addrToPatch = self()->comp()->getSymRefTab()->findOrCreateGCRPatchPointSymbolRef()->getSymbol()->getStaticSymbol()->getStaticAddress();
      if (!addrToPatch)
         {
         TR_ASSERT(false, "Must have updated gcrPatchPointSymbol with the correct address by now\n");
         self()->comp()->failCompilation<TR::GCRPatchFailure>("Must have updated gcrPatchPointSymbol with the correct address by now");
         }
      }
#endif

   if (self()->comp()->getOption(TR_TraceCG))
      {
      traceMsg(self()->comp(), "</encode>\n");
      }

   }

// different from evaluate in that it returns a clobberable register
TR::Register *OMR::X86::CodeGenerator::gprClobberEvaluate(TR::Node * node, TR_X86OpCodes movRegRegOpCode)
   {
   TR::Register *sourceRegister = self()->evaluate(node);

   bool canClobber = true;
   if (node->getReferenceCount() > 1)
      canClobber = false;
   else if (sourceRegister->needsLazyClobbering())
      canClobber = self()->canClobberNodesRegister(node);

   if (self()->comp()->getOption(TR_TraceCG) && sourceRegister->needsLazyClobbering())
      traceMsg(self()->comp(), "LAZY CLOBBERING: node %s register %s refcount=%d canClobber=%s\n",
         self()->getDebug()->getName(node),
         self()->getDebug()->getName(sourceRegister),
         node->getReferenceCount(),
         canClobber? "true":"false"
         );

   if (canClobber)
      {
      return sourceRegister;
      }
   else
      {
      if (node->getOpCode().isLoadConst())
         {
         if (debug("traceClobberedConstantRegisters") && node->getRegister())
            {
            trfprintf(self()->comp()->getOutFile(),
               "CLOBBERING CONSTANT in %s on " POINTER_PRINTF_FORMAT " in %s\n",
               self()->getDebug()->getName(node->getRegister()), node, self()->comp()->signature());
            trfflush(self()->comp()->getOutFile());
            }
         }

      TR::Register *targetRegister = self()->allocateRegister();
      generateRegRegInstruction(movRegRegOpCode, node, targetRegister, sourceRegister, self());
      //
      // TODO: Should we do this?
      // if (sourceRegister->containsCollectedReference())
      //    targetRegister->setContainsCollectedReference();

      return targetRegister;
      }
   }

TR::Register *OMR::X86::CodeGenerator::intClobberEvaluate(TR::Node * node)
   {
   TR_ASSERT(!node->getOpCode().is8Byte() || node->getOpCode().isRef(), "Non-ref 8bytes must use longClobberEvaluate");
   TR_ASSERT(!node->getOpCode().isRef() || TR::Compiler->target.is32Bit() || self()->comp()->useCompressedPointers(), "64-bit references must use longClobberEvaluate unless under compression");
   return self()->gprClobberEvaluate(node, MOV4RegReg);
   }

TR::Register *OMR::X86::CodeGenerator::shortClobberEvaluate(TR::Node * node)
   {
   TR_ASSERT(!node->getOpCode().is4Byte() && !node->getOpCode().is8Byte(), "Ints/Longs must use int/longClobberEvaluate");
   TR_ASSERT(!(node->getOpCode().isRef() && TR::Compiler->target.is64Bit()), "64-bit references must use longClobberEvaluate");
   return self()->gprClobberEvaluate(node, MOV2RegReg);
   }

TR::Register *OMR::X86::CodeGenerator::floatClobberEvaluate(TR::Node * node)
   {

   if (node->getReferenceCount() > 1)
      {
      TR::Register * temp           = self()->evaluate(node);
      TR::Register * targetRegister = self()->allocateSinglePrecisionRegister(temp->getKind());

      if (temp->needsPrecisionAdjustment())
         TR::TreeEvaluator::insertPrecisionAdjustment(temp, node, self());

      if (temp->mayNeedPrecisionAdjustment())
         targetRegister->setMayNeedPrecisionAdjustment();

      if (temp->getKind() == TR_FPR)
         generateRegRegInstruction(MOVAPSRegReg, node, targetRegister, temp, self());
      else
         generateFPST0STiRegRegInstruction(FLDRegReg, node, targetRegister, temp, self());

      return targetRegister;
      }
   else
      {
      return self()->evaluate(node);
      }
   }

TR::Register *OMR::X86::CodeGenerator::doubleClobberEvaluate(TR::Node * node)
   {

   if (node->getReferenceCount() > 1)
      {
      TR::Register * temp           = self()->evaluate(node);
      TR::Register * targetRegister = self()->allocateRegister(temp->getKind());

      if (temp->needsPrecisionAdjustment())
         TR::TreeEvaluator::insertPrecisionAdjustment(temp, node, self());

      if (temp->mayNeedPrecisionAdjustment())
         targetRegister->setMayNeedPrecisionAdjustment();

      if (temp->getKind() == TR_FPR)
         generateRegRegInstruction(MOVAPDRegReg, node, targetRegister, temp, self());
      else
         generateFPST0STiRegRegInstruction(DLDRegReg, node, targetRegister, temp, self());

      return targetRegister;
      }
   else
      {
      return self()->evaluate(node);
      }
   }

TR_OutlinedInstructions * OMR::X86::CodeGenerator::findOutlinedInstructionsFromLabel(TR::LabelSymbol *label)
   {
   auto oiIterator = self()->getOutlinedInstructionsList().begin();
   while (oiIterator != self()->getOutlinedInstructionsList().end())
      {
      if ((*oiIterator)->getEntryLabel() == label)
         return *oiIterator;
      ++oiIterator;
      }

   return NULL;
   }

TR_OutlinedInstructions * OMR::X86::CodeGenerator::findOutlinedInstructionsFromLabelForShrinkWrapping(TR::LabelSymbol *label)
   {
   auto oiIterator = self()->getOutlinedInstructionsList().begin();
   while (oiIterator != self()->getOutlinedInstructionsList().end())
      {
      if ((*oiIterator)->getEntryLabel() == label)
         return *oiIterator;
      ++oiIterator;
      }

   return NULL;
   }




TR_OutlinedInstructions * OMR::X86::CodeGenerator::findOutlinedInstructionsFromMergeLabel(TR::LabelSymbol *label)
   {
   auto oiIterator = self()->getOutlinedInstructionsList().begin();
   while (oiIterator != self()->getOutlinedInstructionsList().end())
      {
      if ((*oiIterator)->getRestartLabel() == label)
         return (*oiIterator);
      ++oiIterator;
      }

   return NULL;
   }



TR::IA32ConstantDataSnippet * OMR::X86::CodeGenerator::findOrCreateConstant(TR::Node * n, void * c, uint8_t size)
   {

    TR::IA32DataSnippet * cursor;

   // A simple linear search should suffice for now since the number of FP constants
   // produced is typically very small.  Eventually, this should be implemented as an
   // ordered list or a hash table.
   for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
      {
      if ((*iterator)->getKind() == TR::Snippet::IsConstantData &&
          ((TR::IA32ConstantDataSnippet*)(*iterator))->getConstantSize() == size)
         {
         switch (size)
            {
            case 4:
               if ((*iterator)->getDataAs4Bytes() == *((int32_t *)c))
                  {
                  return (TR::IA32ConstantDataSnippet*)(*iterator);
                  }
               break;

            case 8:
               if ((*iterator)->getDataAs8Bytes() == *((int64_t *)c))
                  {
                  return (TR::IA32ConstantDataSnippet*)(*iterator);
                  }
               break;

            case 2:
               if ((*iterator)->getDataAs2Bytes() == *((int16_t *)c))
                  {
                  return (TR::IA32ConstantDataSnippet*)(*iterator);
                  }
               break;

            case 16:
               if (!memcmp((*iterator)->getValue(), c, 16))
                  {
                  return (TR::IA32ConstantDataSnippet*)(*iterator);
                  }
               break;

            default:
               // Currently, only 2, 4, and 8 byte constants can be created.
               //
               TR_ASSERT( 0, "findOrCreateConstant ==> unexpected constant size" );
               break;
            }
         }
      }

   // Constant was not found: create a new snippet for it and add it to the constant list.
   //
   cursor = new (self()->trHeapMemory()) TR::IA32ConstantDataSnippet(self(), n, c, size);
   _dataSnippetList.push_front(cursor);

   return (TR::IA32ConstantDataSnippet*)cursor;
   }

int32_t OMR::X86::CodeGenerator::setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart)
   {
   bool                                     first;
   int32_t                                  size;

   // Assume constants will be emitted in order of decreasing size.  Constants should be aligned
   // according to their size.
   for (int exp=4; exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
         {
         if ((*iterator)->getDataSize() == size)
            {
            if (first)
               {
               first = false;
               estimatedSnippetStart = ((estimatedSnippetStart+size-1)/size) * size;
               }
            (*iterator)->getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart);
            estimatedSnippetStart += (*iterator)->getLength(estimatedSnippetStart);
            }
         }
      }

   return estimatedSnippetStart;
   }

void OMR::X86::CodeGenerator::emitDataSnippets()
   {

   TR::IA32DataSnippet              * cursor;
   uint8_t                                 * codeOffset;
   bool                                     first;
   int32_t                                  size;

   // Emit constants in order of decreasing size.  Constants will be aligned according to
   // their size.
   //
   for (int exp=4; exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
         {
         if ((*iterator)->getDataSize() == size)
            {
            if (first)
               {
               first = false;
               self()->setBinaryBufferCursor( (uint8_t *)(((uintptrj_t)(self()->getBinaryBufferCursor()+size-1)/size) * size) );
               }
            codeOffset = (*iterator)->emitSnippetBody();
            if (codeOffset != NULL)
               self()->setBinaryBufferCursor(codeOffset);
            }
         }
      }
   }

TR::IA32ConstantDataSnippet *OMR::X86::CodeGenerator::findOrCreate2ByteConstant(TR::Node * n, int16_t c)
   {
   return self()->findOrCreateConstant(n, &c, 2);
   }

TR::IA32ConstantDataSnippet *OMR::X86::CodeGenerator::findOrCreate4ByteConstant(TR::Node * n, int32_t c)
   {
   return self()->findOrCreateConstant(n, &c, 4);
   }

TR::IA32ConstantDataSnippet *OMR::X86::CodeGenerator::findOrCreate8ByteConstant(TR::Node * n, int64_t c)
   {
   return self()->findOrCreateConstant(n, &c, 8);
   }

TR::IA32ConstantDataSnippet *OMR::X86::CodeGenerator::findOrCreate16ByteConstant(TR::Node * n, void *c)
   {
   return self()->findOrCreateConstant(n, c, 16);
   }

TR::IA32DataSnippet *OMR::X86::CodeGenerator::create2ByteData(TR::Node *n, int16_t c)
   {
   TR::IA32DataSnippet *cursor = new (self()->trHeapMemory()) TR::IA32DataSnippet(self(), n, &c, 2);
   _dataSnippetList.push_front(cursor);
   return cursor;
   }

TR::IA32DataSnippet *OMR::X86::CodeGenerator::create4ByteData(TR::Node *n, int32_t c)
   {
   TR::IA32DataSnippet *cursor = new (self()->trHeapMemory()) TR::IA32DataSnippet(self(), n, &c, 4);
   _dataSnippetList.push_front(cursor);
   return cursor;
   }

TR::IA32DataSnippet *OMR::X86::CodeGenerator::create8ByteData(TR::Node *n, int64_t c)
   {
   TR::IA32DataSnippet *cursor = new (self()->trHeapMemory()) TR::IA32DataSnippet(self(), n, &c, 8);
   _dataSnippetList.push_front(cursor);
   return cursor;
   }

TR::IA32DataSnippet *OMR::X86::CodeGenerator::create16ByteData(TR::Node *n, void *c)
   {
   TR::IA32DataSnippet *cursor = new (self()->trHeapMemory()) TR::IA32DataSnippet(self(), n, c, 16);
   _dataSnippetList.push_front(cursor);
   return cursor;
   }

static uint32_t registerBitMask(int32_t reg)
   {
   return 1 << (reg-1); // TODO:AMD64: Use the proper mask value
   }


void OMR::X86::CodeGenerator::buildRegisterMapForInstruction(TR_GCStackMap * map)
   {
   TR_InternalPointerMap * internalPtrMap = NULL;
   TR::GCStackAtlas *atlas = self()->getStackAtlas();
   //
   // Build the register map
   //
   for (int i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; ++i)
      {
      TR::RealRegister * reg = self()->machine()->getX86RealRegister((TR::RealRegister::RegNum)i);
      if (reg->getHasBeenAssignedInMethod())
         {
         TR::Register *virtReg = reg->getAssignedRegister();
         if (virtReg)
            {
            if (virtReg->containsInternalPointer())
               {
               if (!internalPtrMap)
                  internalPtrMap = new (self()->trHeapMemory()) TR_InternalPointerMap(self()->trMemory());
               internalPtrMap->addInternalPointerPair(virtReg->getPinningArrayPointer(), i);
               atlas->addPinningArrayPtrForInternalPtrReg(virtReg->getPinningArrayPointer());
               }
            else if (virtReg->containsCollectedReference())
               map->setRegisterBits(registerBitMask(i));
            }
         }
      }

   map->setInternalPointerMap(internalPtrMap);
   }

bool OMR::X86::CodeGenerator::processInstruction(TR::Instruction *instr, TR_BitVector **registerUsageInfo,
                                             int32_t &blockNum,
                                             int32_t &isFence,
                                             bool traceIt)
   {
   TR::Instruction *x86Instr = instr;

   if (x86Instr->getOpCode().isCallOp())
      {
      TR::Node *callNode = x86Instr->getNode();
      if (callNode->getOpCode().hasSymbolReference())//getSymbolReference())
         {
         bool callPreservesRegisters = true;
         TR::SymbolReference *symRef = callNode->getSymbolReference();
         TR::MethodSymbol *m = NULL;

         if (traceIt)
            traceMsg(self()->comp(), "looking at call instr %p\n", x86Instr);

         if (x86Instr->getKind() == TR::Instruction::IsImmSym)
            {
            ///traceMsg(comp(), "looking at call instr %p isImmSym\n", x86Instr);
            symRef = ((TR::X86ImmSymInstruction*)x86Instr)->getSymbolReference();
            TR::Symbol *sym = symRef->getSymbol();
            if (!sym->isLabel())
               m = sym->castToMethodSymbol();

            if (traceIt)
               traceMsg(self()->comp(), "call instr (ImmSym) %p has method symbol %p\n", x86Instr, m);
            }
         else
            {
            ///traceMsg(comp(), "looking at call instr %p isNotImmSym\n", x86Instr);
            m = symRef->getSymbol()->castToMethodSymbol();
            if (traceIt)
               traceMsg(self()->comp(), "call instr %p has method symbol %p\n", x86Instr, m);
            }

         if (m &&
              ((m->isHelper() && !m->preservesAllRegisters()) ||
                m->isVMInternalNative() ||
                m->isJITInternalNative() ||
                m->isJNI() ||
                m->isSystemLinkageDispatch() || (m->getLinkageConvention() == TR_System)))
            {
            callPreservesRegisters = false;
            }

         if (!callPreservesRegisters)
            {
            if (traceIt)
               ////traceMsg(comp(), "call instr [%p] does not preserve regs really [%d]\n", x86Instr, m->preservesAllRegisters());
               traceMsg(self()->comp(), "call instr [%p] does not preserve regs\n", x86Instr);
            registerUsageInfo[blockNum]->setAll(TR::RealRegister::NumRegisters);
            }
         else
            {
            if (traceIt)
               ////traceMsg(comp(), "call instr [%p] preserves regs really [%d]\n", x86Instr, m->preservesAllRegisters());
               traceMsg(self()->comp(), "call instr [%p] preserves regs\n", x86Instr);
            }
         }
      }

   switch (x86Instr->getKind())
      {
      case TR::Instruction::IsFence:
         {
         // process BBEnd nodes (and BBStart)
         // and populate the RUSE info at each block
         //
         TR::Node *fenceNode = x86Instr->getNode();
         if (fenceNode->getOpCodeValue() == TR::BBStart)
            {
            blockNum = fenceNode->getBlock()->getNumber();
            isFence = 1;
            if (traceIt)
               traceMsg(self()->comp(), "Now generating register use information for block_%d\n", blockNum);
            }
         else if (fenceNode->getOpCodeValue() == TR::BBEnd)
            isFence = 2;

         return false;
         }
      case TR::Instruction::IsReg:
      case TR::Instruction::IsRegImm:
      case TR::Instruction::IsRegImmSym:
      case TR::Instruction::IsRegImm64:
      case TR::Instruction::IsRegImm64Sym:
      case TR::Instruction::IsFPReg:
         {
         // just get the targetRegister
         //
         int32_t tgtRegNum = ((TR::RealRegister*)x86Instr->getTargetRegister())->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "instr [%p] USES register [%d]\n", x86Instr, tgtRegNum);
         registerUsageInfo[blockNum]->set(tgtRegNum);
         return true;
         }
      case TR::Instruction::IsRegReg:
      case TR::Instruction::IsRegRegImm:
      case TR::Instruction::IsFPRegReg:
      case TR::Instruction::IsFPST0ST1RegReg:
      case TR::Instruction::IsFPST0STiRegReg:
      case TR::Instruction::IsFPSTiST0RegReg:
      case TR::Instruction::IsFPArithmeticRegReg:
      case TR::Instruction::IsFPCompareRegReg:
      case TR::Instruction::IsFPRemainderRegReg:
         {
         // get targetRegister and sourceRegister
         //
         int32_t tgtRegNum = ((TR::RealRegister*)x86Instr->getTargetRegister())->getRegisterNumber();
         int32_t srcRegNum = ((TR::RealRegister*)x86Instr->getSourceRegister())->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "instr [%p] USES register [%d] ; register [%d]\n", x86Instr, tgtRegNum, srcRegNum);
         registerUsageInfo[blockNum]->set(tgtRegNum);
         ///registerUsageInfo[blockNum]->set(srcRegNum);
         return true;
         }
      case TR::Instruction::IsRegMem:
      case TR::Instruction::IsRegMemImm:
      case TR::Instruction::IsFPRegMem:
         {
         // get targetRegister and memoryReference get baseRegister, indexRegister
         //
         int32_t tgtRegNum = ((TR::RealRegister*)x86Instr->getTargetRegister())->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "instr [%p] USES register [%d]\n", x86Instr, tgtRegNum);
         registerUsageInfo[blockNum]->set(tgtRegNum);
         TR::MemoryReference *mr = x86Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr baseRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr indexRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         if (TR::Compiler->target.is64Bit() && mr->getAddressRegister())
            {
            int32_t addressRegNum = ((TR::RealRegister*)mr->getAddressRegister())->getRegisterNumber();
            if (addressRegNum < TR::RealRegister::NumRegisters)
               {
               if (traceIt)
                  traceMsg(self()->comp(), "instr [%p] USES mr addressRegister [%d]\n", x86Instr, addressRegNum);
               registerUsageInfo[blockNum]->set(addressRegNum);
               }
            }
         else if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr no addressRegister\n", x86Instr);
         return true;
         }
      case TR::Instruction::IsMem:
      case TR::Instruction::IsCallMem:
      case TR::Instruction::IsMemImm:
      case TR::Instruction::IsMemImmSym:
      case TR::Instruction::IsMemImmSnippet:
         {
         // get memoryReference: baseRegister, indexRegister
         //
         TR::MemoryReference *mr = x86Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr baseRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr indexRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         if (TR::Compiler->target.is64Bit() && mr->getAddressRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr addressRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getAddressRegister())->getRegisterNumber());
            registerUsageInfo[blockNum]->set(((TR::RealRegister*)mr->getAddressRegister())->getRegisterNumber());
            }
         else if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr no addressRegister\n", x86Instr);
         return true;
         }
      case TR::Instruction::IsMemReg:
      case TR::Instruction::IsFPMemReg:
      case TR::Instruction::IsMemRegImm:
         {
         // get sourceRegister and memoryReference get baseRegister, indexRegister
         //
         int32_t srcRegNum = ((TR::RealRegister*)x86Instr->getSourceRegister())->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "instr [%p] USES register [%d]\n", x86Instr, srcRegNum);
         TR::MemoryReference *mr = x86Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr baseRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr indexRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         if (TR::Compiler->target.is64Bit() && mr->getAddressRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr addressRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getAddressRegister())->getRegisterNumber());
            registerUsageInfo[blockNum]->set(((TR::RealRegister*)mr->getAddressRegister())->getRegisterNumber());
            }
         else if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr no addressRegister\n", x86Instr);
         return true;
         }
      case TR::Instruction::IsMemRegReg:
         {
         // get sourceRegister, sourceRightRegister and memoryReference: baseRegister, indexRegister
         //
         int32_t srcRegNum = ((TR::RealRegister*)x86Instr->getSourceRegister())->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "instr [%p] USES register [%d]\n", x86Instr, srcRegNum);

         int32_t srcRightRegNum = ((TR::RealRegister*)x86Instr->getSourceRightRegister())->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "instr [%p] USES register [%d]\n", x86Instr, srcRightRegNum);

         TR::MemoryReference *mr = x86Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr baseRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr indexRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            ////registerUsageInfo[blockNum]->set(((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         if (TR::Compiler->target.is64Bit() && mr->getAddressRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr addressRegister [%d]\n", x86Instr, ((TR::RealRegister*)mr->getAddressRegister())->getRegisterNumber());
            registerUsageInfo[blockNum]->set(((TR::RealRegister*)mr->getAddressRegister())->getRegisterNumber());
            }
         else if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES mr no addressRegister\n", x86Instr);

         return true;
         }
      default:
         {
         if (traceIt)
            traceMsg(self()->comp(), "nothing to process at instr [%p]\n", instr);

         // at least make sure we capture the target register if any
         //
         if ((TR::RealRegister*)x86Instr->getTargetRegister())
            {
            int32_t tgtRegNum = ((TR::RealRegister*)x86Instr->getTargetRegister())->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "instr [%p] USES register [%d]\n", x86Instr, tgtRegNum);
            registerUsageInfo[blockNum]->set(tgtRegNum);
            }
         return false;
         }
      }
   }

uint32_t OMR::X86::CodeGenerator::isPreservedRegister(int32_t regIndex)
   {
   // is register preserved in prologue?
   //
   int32_t numPreserved = self()->getProperties().getMaxRegistersPreservedInPrologue();
   for (int32_t pindex = numPreserved-1; pindex >= 0; pindex--)
      {
      if (self()->getProperties().getPreservedRegister((uint32_t)pindex) == (TR::RealRegister::RegNum)regIndex)
         return pindex;
      }
   return -1;
   }

// split a block entry, kind of like adding a new block and
// re-directing its predecessors to the new label
//
TR::Instruction *OMR::X86::CodeGenerator::splitBlockEntry(TR::Instruction *instr)
   {
   TR::LabelSymbol *newLabel = generateLabelSymbol(self());
   return generateLabelInstruction(instr->getPrev(), LABEL, newLabel, self());
   }

TR::Instruction *OMR::X86::CodeGenerator::splitEdge(TR::Instruction *instr,
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
      newLabel = ((TR::X86LabelInstruction *)newSplitLabel)->getLabelSymbol();
      }
   TR::LabelSymbol *targetLabel = NULL;
   TR::Instruction *location = NULL;
   if (isFallThrough)
      {
      location = instr;
      }
   else
      {
      TR::X86LabelInstruction *labelInstr = (TR::X86LabelInstruction *)instr;
      targetLabel = labelInstr->getLabelSymbol();
      labelInstr->setLabelSymbol(newLabel);
      location = targetLabel->getInstruction()->getPrev();
      traceMsg(self()->comp(), "splitEdge fixing branch %p, appending to %p\n", instr, location);
      // now fixup any remaining jmp instrs that jmp to the target
      // so that they now jmp to the new label
      //
      for (auto jmpItr = jmpInstrs->begin(); jmpItr != jmpInstrs->end(); ++jmpItr)
         {
         TR::X86LabelInstruction *l = (TR::X86LabelInstruction *)(*jmpItr);
         if (l->getLabelSymbol() == targetLabel)
            {
            traceMsg(self()->comp(), "splitEdge fixing jmp instr %p\n", (*jmpItr));
            l->setLabelSymbol(newLabel);
            }
         }
      }

   TR::Instruction *cursor = newSplitLabel;
   if (!cursor)
      cursor = generateLabelInstruction(location,
                                        LABEL,
                                        newLabel,
                                        self());

   if (!isFallThrough && needsJump)
      {
      TR::Instruction *jmpLocation = cursor->getPrev();
      TR::LabelSymbol *l = targetLabel;
      TR::Instruction *i = generateLabelInstruction(jmpLocation, JMP4, l, self());
      traceMsg(self()->comp(), "splitEdge jmp instr at [%p]\n", i);
      }
   return cursor;
   }

bool OMR::X86::CodeGenerator::isTargetSnippetOrOutOfLine(TR::Instruction *instr,
                                                     TR::Instruction **start,
                                                     TR::Instruction **end)
   {
   TR::X86LabelInstruction *labelInstr = (TR::X86LabelInstruction *)instr;
   TR::LabelSymbol *targetLabel = labelInstr->getLabelSymbol();
   TR_OutlinedInstructions *oiCursor = self()->findOutlinedInstructionsFromLabelForShrinkWrapping(targetLabel);
   if (oiCursor)
      {
      *start = oiCursor->getFirstInstruction();
      *end = oiCursor->getAppendInstruction();
      return true;
      }
   else
      return false;
   }

void OMR::X86::CodeGenerator::updateSnippetMapWithRSD(TR::Instruction *instr, int32_t rsd)
   {

   // --------------------------------------------------------------------------
   // NOTE: Consider calling TR_ShrinkWrap::updateMapWithRSD in lieu of this
   // codegen-specific function.  They do pretty much the same thing.
   // --------------------------------------------------------------------------

   // at this point, instr is a jmp instruction
   // determine if it branches to a snippet and if so
   // mark the maps in the snippet with the right rsd
   //
   TR::X86LabelInstruction *labelInstr = (TR::X86LabelInstruction *)instr;
   TR::LabelSymbol *targetLabel = labelInstr->getLabelSymbol();
   TR_OutlinedInstructions *oiCursor = self()->findOutlinedInstructionsFromLabel(targetLabel);

   if (oiCursor)
      {
      TR::Instruction *cur = oiCursor->getFirstInstruction();
      TR::Instruction *end = oiCursor->getAppendInstruction();
      while (cur != end)
         {
         if (cur->needsGCMap())
            {
            TR_GCStackMap *map = cur->getGCMap();
            if (map)
               {
               map->setRegisterSaveDescription(rsd);
               }
            }

         if (cur->getSnippetForGC())
            {
            TR::Snippet * snippet = cur->getSnippetForGC();
            if (snippet && snippet->gcMap().isGCSafePoint() && snippet->gcMap().getStackMap())
               {
               TR_GCStackMap *map = snippet->gcMap().getStackMap();
               map->setRegisterSaveDescription(rsd);
               }
            }

         cur = cur->getNext();
         }
      }
   }

int32_t OMR::X86::CodeGenerator::computeRegisterSaveDescription(TR_BitVector *regs, bool populateInfo)
   {
   uint32_t rsd = 0;
   TR_BitVectorIterator regIt(*regs);
   while (regIt.hasMoreElements())
      {
      int32_t regIndex = self()->isPreservedRegister(regIt.getNextElement());
      if (regIndex != -1)
         {
         TR::RealRegister::RegNum idx = self()->getProperties().getPreservedRegister((uint32_t)regIndex);
         ///traceMsg(comp(), "regIndex %d idx %d\n", regIndex, idx);
         TR::RealRegister *reg = self()->machine()->getX86RealRegister((TR::RealRegister::RegNum)idx);
         rsd |= reg->getRealRegisterMask();
         }
      }

   // place the rsd info on the cg. this is used when emitting the
   // metadata for the method
   //
   if (populateInfo)
      self()->comp()->cg()->setLowestSavedRegister(rsd & 0xFFFF);

   return rsd;
   }

void OMR::X86::CodeGenerator::processIncomingParameterUsage(TR_BitVector **registerUsageInfo, int32_t blockNum)
   {
   TR::ResolvedMethodSymbol             *bodySymbol = self()->comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>  paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol               *paramCursor;

   for (paramCursor = paramIterator.getFirst();
       paramCursor != NULL;
       paramCursor = paramIterator.getNext())
      {
      TR::RealRegister::RegNum ai
         = (TR::RealRegister::RegNum)paramCursor->getAllocatedIndex();

      if (self()->comp()->getOption(TR_TraceShrinkWrapping))
         traceMsg(self()->comp(), "found %d used as parm\n", ai);

      if (ai != (TR::RealRegister::RegNum)-1)
         registerUsageInfo[blockNum]->set(ai);
      }
   }



bool OMR::X86::CodeGenerator::allowGlobalRegisterAcrossBranch(TR_RegisterCandidate * rc, TR::Node * node)
   {
   // if a float is being kept alive across a switch make sure it's live across
   // all case destinations
   //
   if (node->getOpCode().isSwitch() &&
       (rc->getDataType() == TR::Float || rc->getDataType() == TR::Double))
      {
      for (int32_t i = node->getCaseIndexUpperBound() - 1; i > 0; --i)
         if (!rc->getBlocksLiveOnEntry().get(node->getChild(i)->getBranchDestination()->getNode()->getBlock()->getNumber()))
             return false;
      }

   return true;
   }

bool OMR::X86::CodeGenerator::supportsInliningOfIsInstance()
   {
   static const char *envp = feGetEnv("TR_NINLINEISINSTANCE");
   return !envp && !self()->comp()->getOption(TR_DisableInlineIsInstance);
   }

uint8_t OMR::X86::CodeGenerator::getSizeOfCombinedBuffer()
   {
   static const char *envp = feGetEnv("TR_NCOMBININGBUF");
   if (envp)
      return 0;
   else
      return 8;
   }

bool OMR::X86::CodeGenerator::supportsComplexAddressing()
   {
   static const char *envp = feGetEnv("TR_NONLYUSERPOINTERS");
   if (envp)
      return false;
   else
      return true;
   }

uint32_t
OMR::X86::CodeGenerator::estimateBinaryLength(TR::MemoryReference *mr)
   {
   return mr->estimateBinaryLength(self());
   }

// movInstruction     MOV4RegImm4 reg,<4 byte imm>
// ...
// startLabel:
// ...
// endLabel:
//
// This relocation encodes the distance (endLabel - startLabel) in the movInstruction's immediate field
// deltaToStartLabel is the byte distance from the start of the instruction following the mov to the startLabel address
void OMR::X86::CodeGenerator::apply32BitLoadLabelRelativeRelocation(TR::Instruction *movInstruction, TR::LabelSymbol *startLabel, TR::LabelSymbol *endLabel, int32_t deltaToStartLabel)
   {
   TR::Instruction *movInstructionX86   = movInstruction;

   TR_ASSERT(movInstructionX86->getOpCodeValue() == MOV4RegImm4,"wrong load instruction used for apply32BitLoadLabelRelativeRelocation\n");

   uint8_t *cursor = movInstructionX86->getBinaryEncoding();
   cursor += movInstructionX86->getOpCode().length(movInstructionX86->rexBits()) + movInstructionX86->rexRepeatCount();

   int32_t distance = int32_t(endLabel->getCodeLocation() - startLabel->getCodeLocation());
   TR_ASSERT(IS_32BIT_SIGNED(distance),
          "relative label does not fit in 4 byte mov immediate instruction\n");
   TR_ASSERT(*(int32_t *)cursor == 0,"offset should be 0 to start\n");

   *(int32_t *)cursor = distance;
   }

void OMR::X86::CodeGenerator::apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   *cursor += ((uintptrj_t)label->getCodeLocation());
   }


// Returns either the disp32 to a helper method from the start of the following
// instruction or the disp32 to a trampoline that can reach the helper.
//
int32_t OMR::X86::CodeGenerator::branchDisplacementToHelperOrTrampoline(
   uint8_t            *nextInstructionAddress,
   TR::SymbolReference *helper)
   {
   intptrj_t helperAddress = (intptrj_t)helper->getMethodAddress();

   if (NEEDS_TRAMPOLINE(helperAddress, nextInstructionAddress, self()))
      {
      helperAddress = self()->fe()->indexedTrampolineLookup(helper->getReferenceNumber(), (void *)(nextInstructionAddress-4));
      TR_ASSERT(IS_32BIT_RIP(helperAddress, nextInstructionAddress), "Local helper trampoline should be reachable directly.\n");
      }

   return (int32_t)(helperAddress - (intptrj_t)(nextInstructionAddress));
   }


// TODO: Revisit this function and make sure it's picking a good register.
// Probably simplify it too.
//
// TODO:AMD64: Is this choosing the right register for AMD64?  This function
// should probably be virtual so it can be different on the two platforms.
//
TR::RealRegister::RegNum OMR::X86::CodeGenerator::pickNOPRegister(TR::Instruction  * successor)
   {
   int32_t j = 1;

   // We don't do ebp or esp (or r11 or r12 for that matter) because their
   // binary encodings are not always idential to those of the other registers.

   TR::RealRegister * ebx = self()->machine()->getX86RealRegister(TR::RealRegister::ebx);
   TR::RealRegister * esi = self()->machine()->getX86RealRegister(TR::RealRegister::esi);
   TR::RealRegister * edi = self()->machine()->getX86RealRegister(TR::RealRegister::edi);

   int8_t ebxLastDef = 0;
   int8_t esiLastDef = 0;
   int8_t ediLastDef = 0;

   if (successor)
      {
      TR::Instruction  * cursor = successor->getPrev();
      static const int32_t WINDOW_SIZE = 5;
      while (j <= WINDOW_SIZE && cursor)
         {
         if (cursor->getOpCodeValue() != FENCE &&
             cursor->getOpCodeValue() != LABEL)
            {
            ++j;

            if (!ebxLastDef && cursor->defsRegister(ebx))
               ebxLastDef = j;
            if (!esiLastDef && cursor->defsRegister(esi))
               esiLastDef = j;
            if (!ediLastDef && cursor->defsRegister(edi))
               ediLastDef = j;
            }

         cursor = cursor->getPrev();
         }
      }

   int32_t min = INT_MAX;
   TR::RealRegister::RegNum reg;

   if (ebxLastDef < min)
      {
      min = ebxLastDef;
      reg = TR::RealRegister::ebx;
      }

   if (esiLastDef < min)
      {
      min = esiLastDef;
      reg = TR::RealRegister::esi;
      }

   if (ediLastDef < min)
      reg = TR::RealRegister::edi;

   return reg;
   }

// Hack markers
//
#define NUM_BIG_BAD_TEMP_REGS (2)

// TODO: Don't duplicate this function all over the place.  Find a good header for it.
inline bool getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::Compiler->target.is64Bit() && node->getSize() > 4;
   }

// TODO: Don't duplicate this function all over the place.  Find a good header for it.
inline intptrj_t integerConstNodeValue(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (getNodeIs64Bit(node, cg))
      {
      return node->getLongInt();
      }
   else
      {
      TR_ASSERT(node->getSize() <= 4, "For efficiency on IA32, only call integerConstNodeValue for 32-bit constants");
      return node->getInt();
      }
   }

bool OMR::X86::CodeGenerator::nodeIsFoldableMemOperand(TR::Node *node, TR::Node *parent, TR_RegisterPressureState *state)
   {
   TR_SimulatedNodeState &nodeState = self()->simulatedNodeState(node, state);
   bool result =
      (node->getOpCode().isLoadVar() || node->getOpCode().isArrayLength()) &&
      !self()->isCandidateLoad(node, state) &&
      !nodeState.hasRegister();

   if (node->getFutureUseCount() > 1)
      {
      result = false;

      // It's ok if we're dealing with an arraylength under a BNDCHK and the
      // only other reference is under a preceeding NULLCHK
      //
      if (parent->getOpCode().isBndCheck() && node->getOpCode().isArrayLength() && node->getFutureUseCount() == 2)
         {
         if (self()->traceSimulateTreeEvaluation() && result)
            traceMsg(self()->comp(), " bndchk/arraylength");
         TR::TreeTop *prevTT = state->_currentTreeTop->getPrevTreeTop();
         if (prevTT)
            {
            TR::Node *nullchk = prevTT->getNode();
            if (nullchk->getOpCode().isNullCheck() && nullchk->getFirstChild() == node)
               result = true;
            }
         }
      }

   if (self()->traceSimulateTreeEvaluation() && result)
      traceMsg(self()->comp(), " %s foldable into %s", self()->getDebug()->getName(node), self()->getDebug()->getName(parent));
   return result;
   }

inline bool lookForMemrefInChild(TR::Node *node)
   {
   return node->getNumChildren() >= 1 && node->getType().isInt64() && node->isHighWordZero();
   }

uint8_t OMR::X86::CodeGenerator::nodeResultGPRCount(TR::Node *node, TR_RegisterPressureState *state)
   {
   TR_ASSERT(!self()->comp()->getOption(TR_DisableRegisterPressureSimulation), "assertion failure");

   // 32-bit integer constants practically never need a register on x86
   //  (includes 64-bit integer constants with high word zero needed to maintain IL correctness)
   //
   if (  node->getOpCode().isLoadConst()
      && (node->getSize() <= 4 || (node->getType().isInt64() && node->isHighWordZero()))
      && (node->getType().isAddress() || node->getType().isIntegral())
      && !(  self()->simulatedNodeState(node, state)._keepLiveUntil != NULL // Check if parent will become a RegStore that keeps this node live
         && state->_currentTreeTop->getNode()->getOpCode().isStoreDirect()
         && state->_currentTreeTop->getNode()->getFirstChild() == node)
      ){
      return 0;
      }
   else
      {
      return OMR::CodeGenerator::nodeResultGPRCount(node, state);
      }

   }

void OMR::X86::CodeGenerator::simulateNodeEvaluation(TR::Node * node, TR_RegisterPressureState * state, TR_RegisterPressureSummary * summary)
   {
   TR_ASSERT(!self()->comp()->getOption(TR_DisableRegisterPressureSimulation), "assertion failure");

   // Memory operand opportunities
   //
   TR::ILOpCode &opCode = node->getOpCode();
   TR::DataType nodeType = node->getType();

   int32_t toFold = -1; // Child numbers are usually unsigned, but then we couldn't use -1

   if ((node->getNumChildren() == 2 || opCode.isBooleanCompare()) && !opCode.isStore() && !opCode.isIndirect() && state->_memrefNestDepth == 0)
      {
      TR::Node *left  = node->getFirstChild();
      TR::Node *right = node->getSecondChild();

      // Operations like isub must clobber their left operand, so that can't
      // be a memref (unless of course it's a direct memory update, which we
      // don't (yet) model here).
      //
      bool clobbersNothing = node->getOpCode().isBooleanCompare() || node->getOpCode().isBndCheck();
      bool tryFoldingLeft = node->getOpCode().isCommutative() || clobbersNothing;

      if (TR::Compiler->target.is32Bit())
         {
         while (lookForMemrefInChild(left))
            left = left->getFirstChild();
         while (lookForMemrefInChild(right))
            right = right->getFirstChild();
         }

      // We try folding left first because it tends to be better for BNDCHKs.
      // TODO: we should really try folding the higher tree first
      //
      if (tryFoldingLeft && (clobbersNothing || right->getFutureUseCount() == 1) && self()->nodeIsFoldableMemOperand(left, node, state))
         toFold = 0;
      else if ((clobbersNothing || left->getFutureUseCount() == 1) && self()->nodeIsFoldableMemOperand(right, node, state))
         toFold = 1;

      }

   if (toFold != -1) // toFold child can be folded into a memory operand
      {
      int32_t i;
      TR_SimulatedMemoryReference memref(self()->trMemory());

      // Evaluate children besides toFold
      for (i=0; i < node->getNumChildren(); i++)
         {
         if (i != toFold)
            self()->simulateTreeEvaluation(node->getChild(i), state, summary);
         }

      // Fold memref child
      self()->simulateMemoryReference(&memref, node->getChild(toFold), state, summary);

      // Decrement refcounts
      for (i=0; i < node->getNumChildren(); i++)
         self()->simulateDecReferenceCount(node->getChild(i), state);
      memref.simulateDecNodeReferenceCounts(state, self());
      self()->simulatedNodeState(node)._childRefcountsHaveBeenDecremented = 1;

      // Go live
      self()->simulateNodeGoingLive(node, state);
      if (self()->traceSimulateTreeEvaluation())
         traceMsg(self()->comp(), " memop");
      }
   else
      {
      // Just call inherited logic
      //
      OMR::CodeGenerator::simulateNodeEvaluation(node, state, summary);
      }


   // Special register usage
   //
   TR::SymbolReference *candidate = state->getCandidateSymRef();
   if ((opCode.isMul() || opCode.isDiv() || opCode.isRem()) && !(opCode.isFloat() || opCode.isDouble()))
      {
      // Integer multiplies will spill edx and possibly eax

      TR::Node *firstChild  = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      bool usesMul = true;
      if (secondChild->getOpCode().isLoadConst() &&
         (TR::Compiler->target.is64Bit() || !nodeType.isInt64()) &&
         populationCount(integerConstNodeValue(secondChild, self())) <= 2)
         {
         // Will probably use shifts/adds/etc instead of multiply
         //
         usesMul = false;
         if (self()->traceSimulateTreeEvaluation())
            traceMsg(self()->comp(), " nomul");
         }

      if (usesMul)
         {
         summary->spill(TR_edxSpill, self());

         bool candidateDiesHere = false;
         if (self()->isCandidateLoad(firstChild, candidate)  && firstChild->getFutureUseCount() == 1)
            candidateDiesHere = true;
         if (self()->isCandidateLoad(secondChild, candidate) && secondChild->getFutureUseCount() == 1)
            candidateDiesHere = true;

         if (candidateDiesHere)
            {
            if (self()->traceSimulateTreeEvaluation())
               traceMsg(self()->comp(), " dieshere");
            }
         else
            {
            summary->spill(TR_eaxSpill, self());
            }

         // Account for the extra result register that mul/div instructions use
         //
         summary->accumulate(state, self(), 1);
         if (self()->traceSimulateTreeEvaluation())
            traceMsg(self()->comp(), " mul:g=%d", summary->_gprPressure);
         }
      }
   else if ((opCode.isLeftShift() || opCode.isRightShift())
      && !node->getSecondChild()->getOpCode().isLoadConst()
      && !self()->isCandidateLoad(node->getSecondChild(), candidate))
      {
      summary->spill(TR_ecxSpill, self());
      }

   if (opCode.isByte())
      summary->spill(TR_eaxSpill, self());
   }

uint8_t OMR::X86::CodeGenerator::old32BitPaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH]=
   {
      {0x90},                                     // 1: xchg eax,eax
      {0x8b, 0xc0},                               // 2: mov eax,eax
      {0x8d, 0x04, 0x20},                         // 3: lea exx, [exx]           (with SIB)
      {0x8d, 0x44, 0x20, 0x00},                   // 4: lea exx, [exx+00]        (with SIB)
      {0x3e, 0x8d, 0x44, 0x20, 0x00},             // 5: lea exx, DS:[exx+00]     (with SIB) (with prefix)
      {0x8d, 0x80, 0x00, 0x00, 0x00, 0x00},       // 6: lea exx, [exx+00000000]
      {0x8d, 0x84, 0x20, 0x00, 0x00, 0x00, 0x00}, // 7: lea exx, [exx+00000000]  (with SIB)
   };

uint8_t OMR::X86::CodeGenerator::intelMultiBytePaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH] =
   {
      {0x90},
      {0x66, 0x90},
      {0x0f, 0x1f, 0x00},
      {0x0f, 0x1f, 0x40, 0x00},
      {0x0f, 0x1f, 0x44, 0x00, 0x00},
      {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00},
      {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00},
      {0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
   };

uint8_t OMR::X86::CodeGenerator::k8PaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH] =
   {
      {0x90},
      {0x66, 0x90},
      {0x66, 0x66, 0x90},
      {0x66, 0x66, 0x66, 0x90},
      {0x66, 0x66, 0x90, 0x66, 0x90},
      {0x66, 0x66, 0x90, 0x66, 0x66, 0x90},
      {0x66, 0x66, 0x66, 0x90, 0x66, 0x66, 0x90},
      {0x66, 0x66, 0x66, 0x90, 0x66, 0x66, 0x66, 0x90},
      {0x66, 0x66, 0x90, 0x66, 0x66, 0x90, 0x66, 0x66, 0x90},
   };

TR_X86PaddingTable OMR::X86::CodeGenerator::_old32BitPaddingTable =
   TR_X86PaddingTable(7, (uint8_t ***)&OMR::X86::CodeGenerator::old32BitPaddingEncoding, TR_X86PaddingTable::registerMatters, 0x8b, 0x20);

TR_X86PaddingTable OMR::X86::CodeGenerator::_intelMultiBytePaddingTable =
   TR_X86PaddingTable(9, (uint8_t ***)&OMR::X86::CodeGenerator::intelMultiBytePaddingEncoding, 0x0, 0x0, 0x0);

TR_X86PaddingTable OMR::X86::CodeGenerator::_K8PaddingTable =
   TR_X86PaddingTable(9, (uint8_t ***)&OMR::X86::CodeGenerator::k8PaddingEncoding, 0x0, 0x0, 0x0);

static const uint8_t *paddingTableEncoding(TR_X86PaddingTable *paddingTable, uint8_t length)
   {
   TR_ASSERT(paddingTable->_biggestEncoding <= PADDING_TABLE_MAX_ENCODING_LENGTH, "Padding table must have _biggestEncoding <= PADDING_TABLE_MAX_ENCODING_LENGTH (%d <= %d)", paddingTable->_biggestEncoding, PADDING_TABLE_MAX_ENCODING_LENGTH);
   TR_ASSERT(length <= paddingTable->_biggestEncoding, "Padding length %d cannot exceed %d", length, paddingTable->_biggestEncoding);
   return (uint8_t *)((uintptrj_t)(paddingTable->_encodings)+(length-1)*PADDING_TABLE_MAX_ENCODING_LENGTH);
   }

uint8_t *OMR::X86::CodeGenerator::generatePadding(uint8_t              *cursor,
                                              intptrj_t             length,
                                              TR::Instruction    *neighborhood,
                                              TR_PaddingProperties  properties,
                                              bool                  recursive)
   {
   const uint8_t *desiredReturnValue = cursor + length;
   const uint8_t sibMask        = 0xb8; // 10111000, where 2^n is set if the NOP with size n uses a SIB byte
   if (length <= _paddingTable->_biggestEncoding)
      {
      // Copy bytes from the appropriate template
      memcpy(cursor, paddingTableEncoding(_paddingTable, length), length);

      if (_paddingTable->_flags.testAny(TR_X86PaddingTable::registerMatters))
         {
         TR::RealRegister::RegNum  regIndex  = self()->pickNOPRegister(neighborhood);
         TR::RealRegister      *reg       = self()->machine()->getX86RealRegister(regIndex);
         int32_t                  prefixLen = (_paddingTable->_prefixMask & (1<<length))? 1 : 0;

         // Target reg
         //
         reg->setRegisterFieldInModRM(cursor+prefixLen+1);

         // Source reg
         //
         if (sibMask & (1 << length))
            {
            reg->setBaseRegisterFieldInSIB(cursor+prefixLen+2);
            }
         else
            {
            reg->setRMRegisterFieldInModRM(cursor+prefixLen+1);
            }
         }
      else
         {
         // Leave the encoding alone, and default to eax
         }

      cursor += length;
      }
   else
      {
      const intptrj_t jmpThreshold = 100; // Beyond this length, a jmp is faster than a sequence of no-op instructions

      if ((properties & TR_AtomicNoOpPadding || length >= jmpThreshold))
         {
         TR_ASSERT(0, "Oops -- we should never get here because using JMPs for no-ops is very slow");
         if (length >= 5)
            {
            length -= 5;
            cursor = TR_X86OpCode(JMP4).binary(cursor);
            *(int32_t*)cursor = length;
            cursor += 4;
            }
         else
            {
            length -= 2;
            cursor = TR_X86OpCode(JMP1).binary(cursor);
            *(int8_t*)cursor = length;
            cursor += 1;
            }
         memset(cursor, length, 0xcc); // Fill the rest with int3s
         cursor += length;
         }
      else
         {
         // Generate a sequence of NOPs
         //
         while (length > _paddingTable->_biggestEncoding)
            {
            cursor = self()->generatePadding(cursor, _paddingTable->_biggestEncoding, neighborhood, properties);
            length -= _paddingTable->_biggestEncoding;
            }

         // Generate an instruction for the residue.
         //
         cursor = self()->generatePadding(cursor, length, neighborhood, properties);
         }
      }
   // Begin -- Static debug counters to track nop generation
   if (!recursive && self()->comp()->getOptions()->enableDebugCounters())
      {
      if (neighborhood)
         {
         int32_t blockFrequency = -1; // used as a proxy for dynamic frequency
         for (TR::Instruction *finst = neighborhood; finst; finst = finst->getPrev())
            {
            if (finst->getNode()->getOpCodeValue() == TR::BBStart)
               {
               blockFrequency = finst->getNode()->getBlock()->getFrequency();
               break;
               }
            }

         TR::Node *guardNode = neighborhood->getNode();
         if (neighborhood->getKind() == TR::Instruction::IsVirtualGuardNOP && guardNode &&
             (guardNode->isTheVirtualGuardForAGuardedInlinedCall() || guardNode->isHCRGuard() || guardNode->isProfiledGuard() || guardNode->isMethodEnterExitGuard()))
            {
            const char *guardKind;
            TR_VirtualGuard *vg = self()->comp()->findVirtualGuardInfo(neighborhood->getNode());
            switch (vg->getKind())
               {
               case TR_NoGuard:
                  guardKind = "NoGuard"; break;
               case TR_ProfiledGuard:
                  guardKind = "ProfiledGuard"; break;
               case TR_InterfaceGuard:
                  guardKind = "InterfaceGuard"; break;
               case TR_AbstractGuard:
                  guardKind = "AbstractGuard"; break;
               case TR_HierarchyGuard:
                  guardKind = "HierarchyGuard"; break;
               case TR_NonoverriddenGuard:
                  guardKind = "NonoverriddenGuard"; break;
               case TR_SideEffectGuard:
                  guardKind = "SideEffectGuard"; break;
               case TR_DummyGuard:
                  guardKind = "DummyGuard"; break;
               case TR_HCRGuard:
                  guardKind = "HCRGuard"; break;
               case TR_MutableCallSiteTargetGuard:
                  guardKind = "MutableCallSiteTargetGuard"; break;
               case TR_MethodEnterExitGuard:
                  guardKind = "MethodEnterExitGuard"; break;
               case TR_DirectMethodGuard:
                  guardKind = "DirectMethodGuard"; break;
               case TR_InnerGuard:
                  guardKind = "InnerGuard"; break;
               case TR_ArrayStoreCheckGuard:
                  guardKind = "ArrayStoreCheckGuard"; break;
               default:
                  guardKind = "Unknown"; break;
               }
            TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "nopCount/%d/%s/%s", blockFrequency, neighborhood->description(), guardKind));
            if (length > 0)
               {
               TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "nopInst/%d/%s/%s", blockFrequency, neighborhood->description(), guardKind));

               for (TR::Instruction *ninst = neighborhood->getNext(); ninst; ninst = ninst->getNext())
                  {
                  if (ninst->isVirtualGuardNOPInstruction())
                     {
                     if (guardNode->isHCRGuard() && ninst->getNode() && ninst->getNode()->isHCRGuard() &&
                         guardNode->getBranchDestination() == ninst->getNode()->getBranchDestination())
                        continue;
                     TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "vgnopNoPatchReason/%d/vgnop", blockFrequency));
                     break;
                     }
                  if (self()->comp()->isPICSite(ninst))
                     {
                     TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "vgnopNoPatchReason/%d/staticPIC", blockFrequency));
                     break;
                     }
                  if (ninst->isPatchBarrier())
                     {
                     if (ninst->getOpCodeValue() != LABEL)
                        TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "vgnopNoPatchReason/%d/patchBarrier", blockFrequency));
                     else
                        TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "vgnopNoPatchReason/%d/controlFlowMerge", blockFrequency));
                     break;
                     }
                  if (ninst->getEstimatedBinaryLength() > 0)
                     {
                     TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "vgnopNoPatchReason/%d/%s_%s", blockFrequency, (guardNode->isHCRGuard() ? "hcr" : "nohcr"), ninst->description()));
                     }
                  }
               }
            }
         else
            {
            TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "nopCount/%d/%s", blockFrequency, neighborhood->description()));
            if (length > 0)
               TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "nopInst/%d/%s", blockFrequency, neighborhood->description()));
            }
         }
      else
         {
         TR::DebugCounter::incStaticDebugCounter(self()->comp(), "nopCount/-1/unknown");
         if (length > 0)
            TR::DebugCounter::incStaticDebugCounter(self()->comp(), "nopInst/-1/unknown");
         }
      }
   // End -- Static debug coutners to track nop generation
   TR_ASSERT(cursor == desiredReturnValue, "Must produce the correct amount of padding");
   return cursor;
   }

intptrj_t
OMR::X86::CodeGenerator::alignment(void *cursor, intptrj_t boundary, intptrj_t margin)
   {
   return self()->alignment((intptrj_t)cursor, boundary, margin);
   }

bool
OMR::X86::CodeGenerator::patchableRangeNeedsAlignment(void *cursor, intptrj_t length, intptrj_t boundary, intptrj_t margin)
   {
   intptrj_t toAlign = self()->alignment(cursor, boundary, margin);
   return (toAlign > 0 && toAlign < length);
   }

TR_X86ScratchRegisterManager *OMR::X86::CodeGenerator::generateScratchRegisterManager(int32_t capacity)
   {
   return new (self()->trHeapMemory()) TR_X86ScratchRegisterManager(capacity, self());
   }

bool
TR_X86ScratchRegisterManager::reclaimAddressRegister(TR::MemoryReference *mr)
   {
   if (TR::Compiler->target.is32Bit())
      return false;

   return reclaimScratchRegister(mr->getAddressRegister());
   }

TR::Instruction *OMR::X86::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond)
   {
   if (delta == 1)
      return generateMemInstruction(cursor,
         INC4Mem,
         generateX86MemoryReference(counter->getBumpCountSymRef(self()->comp()), self()),
         self());
   else
      return generateMemImmInstruction(cursor,
         (-128 <= delta && delta <= 127)? ADD4MemImms : ADD4MemImm4,
         generateX86MemoryReference(counter->getBumpCountSymRef(self()->comp()), self()),
         delta,
         self());
   }

TR::Instruction *OMR::X86::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond)
   {
   if (deltaReg)
      return generateMemRegInstruction(cursor, ADD4MemReg,
         generateX86MemoryReference(counter->getBumpCountSymRef(self()->comp()), self()),
         deltaReg,
         self());
   else
      return cursor;
   }

TR::Instruction *OMR::X86::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm)
   {
   return self()->generateDebugCounterBump(cursor, counter, delta, NULL);
   }

TR::Instruction *OMR::X86::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm)
   {
   return self()->generateDebugCounterBump(cursor, counter, deltaReg, NULL);
   }

void OMR::X86::CodeGenerator::removeUnavailableRegisters(TR_RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters)
   {
   TR_BitVectorIterator loe(rc->getBlocksLiveOnExit());
   TR::SymbolReference*  rcSymRef = rc->getSymbolReference();
   while (loe.hasMoreElements())
      {
      int32_t blockNumber = loe.getNextElement();
      TR::Block * b = blocks[blockNumber];
      TR::Node * lastTreeTopNode = b->getLastRealTreeTop()->getNode();
      switch (lastTreeTopNode->getOpCodeValue())
         {
         case TR::tstart:
            {
            int globalRegNum;
            globalRegNum = self()->machine()->getGlobalReg(TR::RealRegister::eax);
            TR_ASSERT(globalRegNum != -1, "could not find global reg");
            availableRegisters.reset(globalRegNum);
            break;
            }
         default:
            break;
         }
      }
   }

void OMR::X86::CodeGenerator::dumpDataSnippets(TR::FILE *outFile)
   {

   if (outFile == NULL)
      return;

   TR::IA32DataSnippet              * cursor;
   int32_t                                  size;

   for (int exp=4; exp > 0; exp--)
      {
      size = 1 << exp;
      for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
         {
         if ((*iterator)->getDataSize() == size)
            {
            self()->getDebug()->print(outFile, *iterator);
            }
         }
      }
   }

#if defined(DEBUG)
// Dump the instruction before FP register assignment to
// reveal the virtual registers prior to stack register assignment.
//
void OMR::X86::CodeGenerator::dumpPreFPRegisterAssignment(TR::Instruction * instructionCursor)
   {
   if (self()->comp()->getOutFile() == NULL)
      return;

   if (instructionCursor->totalReferencedFPRegisters(self()) > 0)
      {
      trfprintf(self()->comp()->getOutFile(), "\n<< Pre-FPR assignment for instruction: %p", instructionCursor);
      self()->getDebug()->print(self()->comp()->getOutFile(), instructionCursor);
      self()->getDebug()->printReferencedRegisterInfo(self()->comp()->getOutFile(), instructionCursor);

      if (debug("dumpFPRegStatus"))
         {
         self()->machine()->printFPRegisterStatus(self()->fe(), self()->comp()->getOutFile());
         }
      }
   }

// Dump the current instruction with the FP registers assigned and any new
// instructions that may have been added before it (such as FXCH).
//
void OMR::X86::CodeGenerator::dumpPostFPRegisterAssignment(TR::Instruction * instructionCursor,
                                                        TR::Instruction *origPrevInstruction)
   {
   if (self()->comp()->getOutFile() == NULL)
      return;

   if (instructionCursor->totalReferencedFPRegisters(self()) > 0)
      {
      TR::Instruction * prevInstruction = instructionCursor->getPrev();
      trfprintf(self()->comp()->getOutFile(), "\n>> Post-FPR assignment for instruction: %p", instructionCursor);

      if (prevInstruction == origPrevInstruction) prevInstruction = NULL;

      while (prevInstruction && (prevInstruction != origPrevInstruction))
         {
         if (prevInstruction->getPrev() == origPrevInstruction) break;
         prevInstruction = prevInstruction->getPrev();
         }

      while (prevInstruction && (prevInstruction != instructionCursor))
         {
         self()->getDebug()->print(self()->comp()->getOutFile(), prevInstruction);
         prevInstruction = prevInstruction->getNext();
         }

      self()->getDebug()->print(self()->comp()->getOutFile(), instructionCursor);
      self()->getDebug()->printReferencedRegisterInfo(self()->comp()->getOutFile(), instructionCursor);

      if (debug("dumpFPRegStatus"))
         {
         self()->machine()->printFPRegisterStatus(self()->fe(), self()->comp()->getOutFile());
         }
      }
   }

void OMR::X86::CodeGenerator::dumpPreGPRegisterAssignment(TR::Instruction * instructionCursor)
   {
   if (self()->comp()->getOutFile() == NULL)
      return;

   if (instructionCursor->totalReferencedGPRegisters(self()) > 0)
      {
      trfprintf(self()->comp()->getOutFile(), "\n<< Pre-GPR assignment for instruction: %p", instructionCursor);
      self()->getDebug()->print(self()->comp()->getOutFile(), instructionCursor);
      self()->getDebug()->printReferencedRegisterInfo(self()->comp()->getOutFile(), instructionCursor);

      if (debug("dumpGPRegStatus"))
         {
         self()->machine()->printGPRegisterStatus(self()->fe(), self()->machine()->getRegisterFile(), self()->comp()->getOutFile());
         }
      }
   }

// Dump the current instruction with the GP registers assigned and any new
// instructions that may have been added after it.
//
void OMR::X86::CodeGenerator::dumpPostGPRegisterAssignment(TR::Instruction * instructionCursor,
                                                        TR::Instruction *origNextInstruction)
   {
   if (self()->comp()->getOutFile() == NULL)
      return;

   if (instructionCursor->totalReferencedGPRegisters(self()) > 0)
      {
      trfprintf(self()->comp()->getOutFile(), "\n>> Post-GPR assignment for instruction: %p", instructionCursor);

      TR::Instruction * nextInstruction = instructionCursor;

      while (nextInstruction && (nextInstruction != origNextInstruction))
         {
         self()->getDebug()->print(self()->comp()->getOutFile(), nextInstruction);
         nextInstruction = nextInstruction->getNext();
         }

      self()->getDebug()->printReferencedRegisterInfo(self()->comp()->getOutFile(), instructionCursor);

      if (debug("dumpGPRegStatus"))
         {
         self()->machine()->printGPRegisterStatus(self()->fe(), self()->machine()->getRegisterFile(), self()->comp()->getOutFile());
         }
      }
   }
#endif

bool
OMR::X86::CodeGenerator::useSSEFor(TR::DataType type)
   {
   if (type == TR::Float)
      return self()->useSSEForSinglePrecision();
   else if (type == TR::Double)
      return self()->useSSEForDoublePrecision();
   else
      return false;
   }

bool
OMR::X86::CodeGenerator::needToAvoidCommoningInGRA()
   {
   if (!self()->useSSEForSinglePrecision() && !self()->useSSEForDoublePrecision()) return true;
   return false;
   }

int32_t
OMR::X86::CodeGenerator::arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget)
   {
   if (TR::CodeGenerator::useOldArrayTranslateMinimumNumberOfIterations())
      return OMR::CodeGenerator::arrayTranslateMinimumNumberOfElements(isByteSource, isByteTarget);
   return 8;
   }

int32_t
OMR::X86::CodeGenerator::arrayTranslateAndTestMinimumNumberOfIterations()
   {
   if (TR::CodeGenerator::useOldArrayTranslateMinimumNumberOfIterations())
      return OMR::CodeGenerator::arrayTranslateAndTestMinimumNumberOfIterations();
   // Idiom recognition uses this value to determine whether to
   // attempt to transform a loop with the MemCpy pattern. Be more
   // aggressive at scorching, since loops that iterate exactly 8
   // times can appear to iterate slightly less than that.
   return self()->comp()->getOptLevel() >= scorching ? 4 : 8;
   }
