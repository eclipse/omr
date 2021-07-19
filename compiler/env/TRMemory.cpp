/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include <algorithm>
#include <new>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

namespace TR { class Monitor; }

extern size_t mem_round(size_t);

const char * objectName[] =
   {
   "Array",
   "Instruction",
   "ListElement",
   "Node",
   "BitVector",
   "Register",
   "MemoryReference",
   "CFGNode",
   "Symbol",
   "SymbolReference",
   "SymRefUnion",
   "BackingStore",
   "StorageReference",
   "PseudoRegister",
   "Link",
   "Machine",
   "CFGEdge",
   "TreeTop",
   "GCStackMap",
   "IlGenerator",
   "RegisterDependencyConditions",
   "Snippet",
   "Relocation",
   "VirtualGuard",
   "RegisterDependencyGroup",
   "RegisterDependency",
   "FrontEnd",
   "VMField",
   "VMFieldsInfo",
   "Method",
   "Pair",
   "HashTab",
   "HashTable",
   "HashTabEntry",
   "DebugCounter",
   "JSR292",
   "Memory",
   "CS2",
   "UnknownType",

   // Optimizer Types
   "AVLTree",
   "CFGChecker",
   "CFGSimplifier",
   "CatchBlockRemover",
   "CompactLocals",
   "CopyPropagation",
   "DataFlowAnalysis",
   "DominatorVerifier",
   "Dominators",
   "DominatorsChk",
   "EscapeAnalysis",
   "EstimateCodeSize",
   "ExpressionsSimplification",
   "GlobalRegisterAllocator",
   "GlobalRegisterCandidates",
   "HedgeTree",
   "Inliner",
   "InnerPreexistence",
   "IsolatedStoreElimination",
   "LocalLiveVariablesForGC",
   "LocalAnalysis",
   "LocalCSE",
   "LocalDeadStoreElimination",
   "LocalOpts",
   "LocalReordering",
   "LocalLiveRangeReduction",
   "LoopAliasRefiner",
   "LoopTransformer",
   "MonitorElimination",
   "NewInitialization",
   "Optimization",
   "OSR",
   "PartialRedundancy",
   "RedundantAsycCheckRemoval",
   "RegionAnalysis",
   "RightNumberOfNames",
   "Structure",
   "SwitchAnalyzer",
   "UseDefInfo",
   "ValueNumberInfo",
   "ValuePropagation",
   "VirtualGuardTailSplitter",
   "InterProceduralAnalyzer",
   "InductionVariableAnalysis",
   "CoarseningInterProceduralAnalyzer",

   "AheadOfTimeCompile",
   "AOTAdapter",
   "AOTMethodHeader",
   "HWProfile",
   "TR_HWProfileCallSiteElem",
   "TR_HWProfileCallSiteList",

   // ARM types
   "ARMRelocation",
   "ARMMemoryArgument",
   "ARMOperand",
   "ARMFloatConstant",
   "ARMRegisterDependencyGroup",
   "ARMRegisterDependencyConditions",
   "ARMConstant",
   "ARMConstantDataSnippet",

   // ARM64 types
   "ARM64Relocation",
   "ARM64MemoryArgument",

   "BlockCloner",
   "BlockFrequencyInfo",
   "ByteCodeIterator",
   "CallSiteInfo",
   "CatchBlockExtension",
   "CatchBlockProfileInfo",
   "CFG",
   "CHTable",
   "ClassLookahead",
   "CodeGenerator",
   "Compilation",
   "ExceptionTableEntry",
   "ExceptionTableEntryIterator",
   "ExtendedBlockSuccessorIterator",
   "ExtraInfoForNew",
   "GlobalRegister",
   "GCRegisterMap",
   "GCStackAtlas",
   "GRABlockInfo",
   "IdAddrNodes",

   // IA32 codegen
   "BetterSpillPlacement",
   "ClobberingInstruction",
   "OutlinedCode",
   "IA32ProcessorInfo",
   "X86AllocPrefetchGeometry",
   "X86TOCHashTable",

   "IGBase",
   "IGNode",
   "InternalFunctionsBase",
   "InternalPointerMap",
   "InternalPointerPair",
   "LinkHead",
   "Linkage",
   "List",
   "ListAppender",
   "ListIterator",
   "LiveReference",
   "LiveRegisterInfo",
   "LiveRegisters",
   "NodeMappings",
   "Optimizer",

   "Options",
   "OptionSet",
   "OrderedExceptionHandlerIterator",
   "PCMapEntry",
   "PersistentCHTable",
   "PersistentInfo",
   "PersistentMethodInfo",
   "PersistentJittedBodyInfo",
   "PersistentProfileInfo",

   // PPC objects
   "PPCFloatConstant",
   "PPCConstant",
   "PPCConstantDataSnippet",
   "PPCCounter",
   "PPCLoadLabelItem",
   "PPCRelocation",
   "PPCMemoryArgument",

   "SnippetHashtable",
   "RandomGenerator",
   "Recompilation",
   "RegisterCandidates",
   "RematerializationInfo",

   // S390 types
   "S390Relocation",
   "S390ProcessorInfo",
   "S390MemoryArgument",
   "S390EncodingRelocation",
   "BranchPreloadCallData",

   "ScratchRegisterManager",
   "SimpleRegex",
   "SimpleRegex::Component",
   "SimpleRegex::Regex",
   "SimpleRegex::Simple",
   "SingleTimer",
   "SymbolReferenceTable",
   "TableOfConstants",
   "Timer",
   "TwoListIterator",
   "ValueProfileInfo",
   "ValueProfileInfoManager",
   "MethodValueProfileInfo",
   "MethodBranchProfileInfo",
   "BranchProfileInfoManager",
   "VirtualGuardSiteInfo",
   "WCode",
   "CallGraph",
   "CPOController",
   "RegisterIterator",
   "TRStats",
   "TR_OptimizationPlan",
   "TR_ClassHolder",
   "SmartBuffer",
   "TR_ListingInfo",
   "methodInfoType",

   "IProfiler",
   "TR_IPBytecodeHashTableEntry",
   "TR_IPBCDataFourBytes",
   "TR_IPBCDataAllocation",
   "TR_IPBCDataEightWords",
   "TR_IPBCDataPointer",
   "TR_IPBCDataCallGraph",
   "TR_ICallingContext",

   "TR_IPMethodTable",
   "TR_IPCCNode",
   "IPHashedCallSite",
   "Assumptions",
   "PatchSites",

   "TR_ZHWProfiler",
   "TR_PPCHWProfiler",
   "TR_ZGuardedStorage",

   "CatchTable",

   "DecimalPrivatization",
   "DecimalReduction",

   "TR_PreviousNodeConversion",
   "TR::RelocationDebugInfo",
   "TR::AOTClassInfo",
   "TR_SharedCache",
   "SharedCacheRegion",
   "SharedCacheLayout",
   "SharedCacheConfig",

   "TR::RegisterPair",
   "TR::Instruction",

   "TR_ArtifactManager",
   "CompilationInfo",
   "CompilationInfoPerThreadBase",
   "TR_DebuggingCounters",
   "TR_Pattern",
   "TR_UseNodeInfo",
   "PPSEntry",
   "GPUHelpers",
   "TR_ColdVariableOutliner",

   "CodeMetaData",
   "CodeMetaDataAVL",

   "RegionAllocation",

   "Debug",

   // JITServer types
   "ClientSessionData",
   "ROMClass",
   "JITServerAOTCache",

   "SymbolValidationManager",

   "ObjectFormat",
   "FunctionCallData"
   };


// **************************************************************************
//
// deprecated uses of the global deprecatedTRMemory (not thread safe)
//

extern TR_PersistentMemory * trPersistentMemory;

// **************************************************************************
//
//
//

TR_Memory::TR_Memory(TR_PersistentMemory &persistentMemory, TR::Region & heapMemoryRegion) :
   TR_MemoryBase(persistentMemory),
   _trPersistentMemory(&persistentMemory),
   _comp(NULL),
   _heapMemoryRegion(heapMemoryRegion),
   _stackMemoryRegion(TR::ref(heapMemoryRegion))
   {
   TR_ASSERT(sizeof(objectName)/sizeof(char*) == NumObjectTypes, "Mismatch between NumObjectTypes and object names");
   }

// **************************************************************************
//
// Heap allocation
//

void *
TR_Memory::allocateHeapMemory(size_t requestedSize, ObjectType ot)
   {
   void *alloc = _heapMemoryRegion.allocate(requestedSize);
   TR::AllocatedMemoryMeter::update_allocated(requestedSize, heapAlloc);
   return alloc;
   }
void *
TR_Memory::allocateStackMemory(size_t requestedSize, ObjectType ot)
   {
   void * alloc = _stackMemoryRegion.get().allocate(requestedSize);
   TR::AllocatedMemoryMeter::update_allocated(requestedSize, stackAlloc);
   return alloc;
   }

TR::Region &
TR_Memory::currentStackRegion()
   {
   return _stackMemoryRegion.get();
   }

TR::Region &
TR_Memory::registerStackRegion(TR::Region &stackMemoryRegion)
   {
   TR::Region &currentRegion = _stackMemoryRegion;
   _stackMemoryRegion = TR::ref(stackMemoryRegion);
   return currentRegion;
   }

void
TR_Memory::unregisterStackRegion(TR::Region &current, TR::Region &previous) throw()
   {
   TR_ASSERT(current == _stackMemoryRegion.get(), "Mismatched stack regions");
   _stackMemoryRegion = TR::ref(previous);
   }

void *
TR_Memory::allocateMemory(size_t size, TR_AllocationKind kind, ObjectType ot)
   {
   switch(kind)
      {
      case heapAlloc:       return allocateHeapMemory(size, ot);
      case stackAlloc:      return allocateStackMemory(size, ot);
      case persistentAlloc:
         {
         void * alloc = _trPersistentMemory->allocatePersistentMemory(size, ot);
         if (!alloc) throw std::bad_alloc();
         return alloc;
         }
      default:              return allocateHeapMemory(size, ot);
      }
   }

void
TR_Memory::freeMemory(void *p, TR_AllocationKind kind, ObjectType ot)
   {
   switch(kind)
      {
      case persistentAlloc:
         _trPersistentMemory->freePersistentMemory(p);
         break;
      default:
         (void) 0;
         break;
      }
   }

// **************************************************************************
//
// allocation helpers
//

namespace TR
   {
   AllocatedMemoryMeter::Metric AllocatedMemoryMeter::_currentMemUsage = 0;
   // profile everything until read options
   uint8_t AllocatedMemoryMeter::_enabled = heapAlloc | stackAlloc | persistentAlloc;
   }
