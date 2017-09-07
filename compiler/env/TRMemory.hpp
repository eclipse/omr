/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

/**
 * \page JITMemoryAllocation JIT Memory Allocation
 * JIT memory allocation.
 *
 * The heap exists for the duration of a single compilation. Memory can
 * be allocated on the heap but not freed.
 * The heap is supported by a single method:
 *    jitMalloc       - allocate heap memory
 *
 * The stack exists for the duration of a single compilation. Memory can
 * be allocated on the stack and there is a mark/release mechanism to
 * free all memory allocated since a given stack mark.
 * The stack is supported by 3 methods:
 *    jitStackAlloc   - allocate stack memory
 *    jitStackMark    - mark a stack position
 *    jitStackRelease - release stack memory back to a stack mark
 *
 *
 * The persistent heap exists for ever. Memory can be allocated and freed
 * randomly on the persistent heap.
 * The persistent heap is supported by 2 methods:
 *    jitPersistentAlloc - allocate persistent heap memory
 *    jitPersistentFree  - free persistent heap memory
 *
 * There are 4 variants of the "new" operator for JIT memory allocation:
 *    new C()                  - allocates a C object on the heap
 *    new (STACK_NEW) C()      - allocates a C object on the stack
 *    new (heapOrStackAlloc) C() - allocates a C object on the heap or stack
 *    new (PERSISTENT_NEW) C() - allocates a C object on the persistent
 *                               heap
 *
 */
#ifndef jitmemory_h
#define jitmemory_h

#include <new>                          // for bad_alloc
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, int32_t, uint64_t, etc
#include <stdio.h>                      // for sprintf, NULL, printf
#include <string.h>                     // for memset, memcpy, strcpy, etc
#include "cs2/allocator.h"              // for shared_allocator, etc
#include "cs2/bitvectr.h"               // for ABitVector
#include "cs2/sparsrbit.h"              // for ASparseBitVector
#include "cs2/timer.h"                  // for LexicalBlockProfiler, etc
#include "env/FilePointerDecl.hpp"      // for FILE
#include "env/PersistentAllocator.hpp"  // for PersistentAllocator
#include "env/PersistentInfo.hpp"       // for PersistentInfo
#include "env/defines.h"                // for TR_HOST_64BIT
#include "infra/Assert.hpp"             // for TR_ASSERT
#include "infra/ReferenceWrapper.hpp"   // for reference_wrapper
#include "env/Region.hpp"               // for Region

#include <stdlib.h>
#include "cs2/bitmanip.h"
#include "cs2/hashtab.h"
#include "cs2/llistof.h"
#include "env/jittypes.h"

#include "env/TypedAllocator.hpp"

enum TR_AllocationKind { heapAlloc = 1, stackAlloc = 2, persistentAlloc = 4};
class TR_PersistentMemory;
class TR_Memory;

namespace TR
   {

   namespace Internal
      {
      /**
       * This class exists to allow a different operator new to be called for
       * placement new calls of the form
       *
       * \verbatim
       * new (PERSISTENT_NEW) Object();
       * \endverbatim
       *
       */
      class PersistentNewType { };

      /**
       * This object is a dummy to allow existing new (PERSISTENT_NEW) code to
       * work.
       */
      extern PersistentNewType persistent_new_object;

      }
   }

/**
 * The macro to access the dummy global object.
 */
#define PERSISTENT_NEW         TR::Internal::persistent_new_object

/**
 * Allows a user to define their own operator new such that you can do
 * something like
 *
 * \verbatim
 * new (PERSISTENT_NEW, moreData) Foo();
 * \endverbatim
 */
#define PERSISTENT_NEW_DECLARE TR::Internal::PersistentNewType

// Round the requested size
//
inline size_t mem_round(size_t size)
   {
#if defined(TR_HOST_64BIT) || defined(FIXUP_UNALIGNED)
   return (size+7) & (~7);
#else
   return (size+3) & (~3);
#endif
   }

namespace TR {

   template <size_t alignment, typename T>
   T round_t(T currentValue, size_t remainder, bool roundDown)
      {
      if (roundDown) { return currentValue - remainder; }
      else { return currentValue + (alignment - remainder); }
      }

   template <size_t alignment>
   size_t alignmentRemainder(uintptr_t allocationSize)
      {
      static_assert(alignment && !(alignment & (alignment - 1) ), "Must align to a power of 2" );
      return allocationSize & (alignment - 1);
      }

   template <size_t alignment>
   size_t alignmentRemainder(void* allocation)
      {
      return alignmentRemainder<alignment>( reinterpret_cast<uintptr_t>( allocation ) );
      }

   template <size_t alignment>
   size_t alignAllocationSize(size_t allocationSize, bool alignDown = false)
      {
      return round_t<alignment, decltype(allocationSize) >(allocationSize, alignmentRemainder<alignment>(allocationSize), alignDown );
      }

   template <size_t alignment>
   void * alignAllocation(void * p, bool alignDown = false)
      {
      uint8_t * cursor = static_cast<uint8_t *>(p);
      return static_cast<void *>( round_t<alignment, decltype(cursor) >( cursor, alignmentRemainder<alignment>(cursor), alignDown ) );
      }

}

class TR_MemoryBase
   {
protected:

   TR_MemoryBase(void * jitConfig) :
      _jitConfig(jitConfig)
      {}

   TR_MemoryBase(const TR_MemoryBase &prototype) :
      _jitConfig(prototype._jitConfig)
      {}

public:
   /**
    * If adding new object types below, add the corresponding names
    * to objectName[] array defined in TRMemory.cpp
    */
   enum ObjectType
      {
      // big storage hogs first
      Array,
      Instruction,
      LLListElement,
      Node,
      BitVector,
      Register,
      MemoryReference,
      CFGNode,
      Symbol,
      SymbolReference,
      SymRefUnion,
      BackingStore,
      StorageReference,
      PseudoRegister,
      LLLink,
      Machine,
      CFGEdge,
      TreeTop,
      GCStackMap,
      IlGenerator,
      RegisterDependencyConditions,
      Snippet,
      Relocation,
      VirtualGuard,
      RegisterDependencyGroup,
      RegisterDependency,
      FrontEnd,
      VMField,
      VMFieldsInfo,
      Method,
      Pair,
      HashTab,
      HashTable,
      HashTableEntry,
      DebugCounter,
      JSR292,
      Memory,
      CS2,
      UnknownType,

      // Optimizer Types
      AVLTree,
      CFGChecker,
      CFGSimplifier,
      CatchBlockRemover,
      CompactLocals,
      CopyPropagation,
      DataFlowAnalysis,
      DominatorVerifier,
      Dominators,
      DominatorsChk,
      EscapeAnalysis,
      EstimateCodeSize,
      ExpressionsSimplification,
      GlobalRegisterAllocator,
      GlobalRegisterCandidates,
      HedgeTree,
      Inliner,
      InnerPreexistence,
      IsolatedStoreElimination,
      LocalLiveVariablesForGC,
      LocalAnalysis,
      LocalCSE,
      LocalDeadStoreElimination,
      LocalOpts,
      LocalReordering,
      LocalLiveRangeReduction,
      LongRegAllocation,
      LoopAliasRefiner,
      LoopTransformer,
      MonitorElimination,
      NewInitialization,
      Optimization,
      OSR,
      PartialRedundancy,
      RedundantAsycCheckRemoval,
      RegionAnalysis,
      RightNumberOfNames,
      Structure,
      SwitchAnalyzer,
      UseDefInfo,
      ValueNumberInfo,
      ValuePropagation,
      VirtualGuardTailSplitter,
      InterProceduralAnalyzer,
      InductionVariableAnalysis,
      CoarseningInterProceduralAnalyzer,
      ShrinkWrapping,

      AheadOfTimeCompile,

      HWProfile,
      TR_HWProfileCallSiteElem,
      TR_HWProfileCallSiteList,

      // ARM types
      ARMRelocation,
      ARMMemoryArgument,
      ARMOperand,
      ARMFloatConstant,
      ARMRegisterDependencyGroup,
      ARMRegisterDependencyConditions,
      ARMConstant,
      ARMConstantDataSnippet,

      BlockCloner,
      BlockFrequencyInfo,
      ByteCodeIterator,
      CallSiteInfo,
      CatchBlockExtension,
      CatchBlockProfileInfo,
      CFG,
      CHTable,
      ClassLookahead,
      CodeGenerator,
      Compilation,
      ExceptionTableEntry,
      ExceptionTableEntryIterator,
      ExtendedBlockSuccessorIterator,
      ExtraInfoForNew,
      GlobalRegister,
      GCRegisterMap,
      GCStackAtlas,
      GRABlockInfo,
      IdAddrNodes,

      // IA32 codegen
      BetterSpillPlacement,
      ClobberingInstruction,
      OutlinedCode,
      IA32ProcessorInfo,
      X86AllocPrefetchGeometry,
      X86TOCHashTable,

      IGBase,
      IGNode,
      InternalFunctionsBase,
      InternalPointerMap,
      InternalPointerPair,
      LLLinkHead,
      Linkage,
      LLList,
      LLListAppender,
      LLListIterator,
      LiveReference,
      LiveRegisterInfo,
      LiveRegisters,
      NodeMappings,
      Optimizer,

      Options,
      OptionSet,
      OrderedExceptionHandlerIterator,
      PCMapEntry,
      PersistentCHTable,
      PersistentInfo,
      PersistentMethodInfo,
      PersistentJittedBodyInfo,
      PersistentProfileInfo,

      // PPC objects
      PPCFloatConstant,
      PPCConstant,
      PPCConstantDataSnippet,
      PPCCounter,
      PPCLoadLabelItem,
      PPCRelocation,
      PPCMemoryArgument,

      SnippetHashtable,
      RandomGenerator,
      Recompilation,
      RegisterCandidates,
      RematerializationInfo,

      // S390 types
      S390Relocation,
      S390ProcessorInfo,
      S390MemoryArgument,
      S390EncodingRelocation,
      BranchPreloadCallData,

      ScratchRegisterManager,
      SimpleRegex,
      SimpleRegexComponent,
      SimpleRegexRegex,
      SimpleRegexSimple,
      SingleTimer,
      SymbolReferenceTable,
      TableOfConstants,
      Timer,
      TwoListIterator,
      ValueProfileInfo,
      ValueProfileInfoManager,
      MethodValueProfileInfo,
      MethodBranchProfileInfo,
      BranchProfileInfoManager,
      VirtualGuardSiteInfo,
      WCode,
      CallGraph,
      CPOController,
      RegisterIterator,
      TRStats,
      OptimizationPlan,
      ClassHolder,
      SmartBuffer,
      TR_ListingInfo,
      methodInfoType,

      IProfiler,
      IPBytecodeHashTableEntry,
      IPBCDataFourBytes,
      IPBCDataAllocation,
      IPBCDataEightWords,
      IPBCDataPointer,
      IPBCDataCallGraph,
      IPCallingContext,
      IPMethodTable,
      IPCCNode,
      IPHashedCallSite,
      Assumption,

      ZHWProfiler,
      PPCHWProfiler,
      ZGuardedStorage,
      PPCLMGuardedStorage,

      CatchTable,

      DecimalPrivatization,
      DecimalReduction,

      PreviousNodeConversion,
      RelocationDebugInfo,
      AOTClassInfo,
      SharedCache,

      RegisterPair,
      S390Instruction,

      ArtifactManager,
      CompilationInfo,
      CompilationInfoPerThreadBase,
      TR_DebuggingCounters,
      TR_Pattern,
      TR_UseNodeInfo,
      PPSEntry,
      GPUHelpers,
      ColdVariableOutliner,

      CodeMetaData,
      CodeMetaDataAVL,

      RegionAllocation,

      Debug,

      NumObjectTypes,
      // If adding new object types above, add the corresponding names
      // to objectName[] array defined in TRMemory.cpp
      //
      };

   static void *  jitPersistentAlloc(size_t size, ObjectType = UnknownType);
   static void    jitPersistentFree(void *mem);

   protected:

   void *    _jitConfig;

   };

class TR_PersistentMemory : public TR_MemoryBase
   {
public:
   static const uintptr_t MEMINFO_SIGNATURE = 0x1CEDD1CE;

   TR_PersistentMemory (
      void * jitConfig,
      TR::PersistentAllocator &persistentAllocator
      );

   void * allocatePersistentMemory(size_t const size, ObjectType const ot = UnknownType) throw()
      {
      _totalPersistentAllocations[ot] += size;
      void * persistentMemory = _persistentAllocator.get().allocate(size, std::nothrow);
      return persistentMemory;
      }

   void freePersistentMemory(void *mem) throw()
      {
      _persistentAllocator.get().deallocate(mem);
      }

   TR::PersistentInfo * getPersistentInfo() { return &_persistentInfo; }

   uintptr_t _signature;        // eyecatcher

   friend class TR_Memory;
   friend class TR_DebugExt;

   /** Used by TR_PPCTableOfCnstants */
   static TR::PersistentInfo * getNonThreadSafePersistentInfo();

   TR::PersistentInfo _persistentInfo;
   TR::reference_wrapper<TR::PersistentAllocator> _persistentAllocator;
   size_t _totalPersistentAllocations[TR_MemoryBase::NumObjectTypes];
   };

extern TR_PersistentMemory * trPersistentMemory;

class TR_MemoryAllocationType
   {
protected:
   TR_MemoryAllocationType(TR_Memory &m) :
      _trMemory(m)
      {}
   TR_Memory & trMemory() { return _trMemory; }
   TR_Memory & _trMemory;
   };

class TR_StackMemory : public TR_MemoryAllocationType
   {
public:
   TR_StackMemory(TR_Memory * m) : TR_MemoryAllocationType(*m) { }
   inline void *allocate(size_t, TR_MemoryBase::ObjectType = TR_MemoryBase::UnknownType);
   void deallocate(void *p) {}
   friend bool operator ==(const TR_StackMemory &left, const TR_StackMemory &right) { return &left._trMemory == &right._trMemory; }
   friend bool operator !=(const TR_StackMemory &left, const TR_StackMemory &right) { return !( left == right); }
   };

class TR_HeapMemory : public TR_MemoryAllocationType
   {
public:
   TR_HeapMemory(TR_Memory * m) : TR_MemoryAllocationType(*m) { }
   inline void *allocate(size_t, TR_MemoryBase::ObjectType = TR_MemoryBase::UnknownType);
   void deallocate(void *p) {}
   friend bool operator ==(const TR_HeapMemory &left, const TR_HeapMemory &right) { return &left._trMemory == &right._trMemory; }
   friend bool operator !=(const TR_HeapMemory &left, const TR_HeapMemory &right) { return !( left == right); }
   };

namespace TR {
class Region;
class StackMemoryRegion;
}

class TR_DebugExt;

class TR_Memory : public TR_MemoryBase
   {
   friend class TR_DebugExt;
public:
   TR_HeapMemory  trHeapMemory()  { return this; }
   TR_StackMemory trStackMemory() { return this; }
   TR_PersistentMemory * trPersistentMemory() { return _trPersistentMemory; }

   TR_Memory(
      TR_PersistentMemory &,
      TR::Region &heapMemoryRegion
   );

   void setCompilation(TR::Compilation *compilation) { _comp = compilation; }

   void * allocateHeapMemory(size_t requestedSize, ObjectType = UnknownType);
   void * allocateStackMemory(size_t, ObjectType = UnknownType);
   void * allocateMemory(size_t size, TR_AllocationKind kind, ObjectType ot = UnknownType);
   void freeMemory(void *p, TR_AllocationKind kind, ObjectType ot = UnknownType);
   TR::PersistentInfo * getPersistentInfo() { return _trPersistentMemory->getPersistentInfo(); }
   TR::Region& currentStackRegion();
   TR::Region& heapMemoryRegion() { return _heapMemoryRegion; }

private:

   friend class TR::StackMemoryRegion;
   friend class TR::Region;

   /* These are intended to be used exclusively by TR::StackMemoryRegion. */
   TR::Region& registerStackRegion(TR::Region &stackRegion);
   void unregisterStackRegion(TR::Region &current, TR::Region &previous) throw();

   TR_PersistentMemory * _trPersistentMemory;
   TR::Compilation *         _comp;

   TR::Region &_heapMemoryRegion;
   TR::reference_wrapper<TR::Region> _stackMemoryRegion;
   };

/*
 * Added the following operator overloads for the dynamic allocation of
 * objects such as a stl list
*/
inline void
operator delete(void* p, TR_HeapMemory heapMemory) { }

inline void
operator delete(void* p, TR_StackMemory heapMemory) { }

inline void
operator delete(void* p, PERSISTENT_NEW_DECLARE) throw() {  }

inline void
operator delete[](void* p, TR_HeapMemory heapMemory) { }

inline void
operator delete[](void* p, TR_StackMemory heapMemory) { }

inline void
operator delete[](void* p, PERSISTENT_NEW_DECLARE) { }

inline void *
operator new(size_t size, PERSISTENT_NEW_DECLARE) throw() { return TR_Memory::jitPersistentAlloc(size, TR_MemoryBase::UnknownType); }

inline void *
operator new(size_t size, TR_HeapMemory heapMemory) { return heapMemory.allocate(size); }

inline void *
operator new(size_t size, TR_StackMemory stackMemory) { return stackMemory.allocate(size); }

inline void *
operator new[](size_t size, TR_HeapMemory heapMemory) { return heapMemory.allocate(size); }

inline void *
operator new[](size_t size, TR_StackMemory stackMemory) { return stackMemory.allocate(size); }

inline void *
operator new[](size_t size, PERSISTENT_NEW_DECLARE) { return TR_Memory::jitPersistentAlloc(size, TR_MemoryBase::UnknownType); }




/*
 * TR_ByteCodeInfo exists in an awkward situation where it seems TR_ALLOC()
 * would be inappropriate for the structure, despite things needing to allocate them,
 * including in array form.  Array forms of placement new are irredeemably broken, needing an
 * implementation defined prefix size for which there is no portable way to discover.
 * Thus theese operators. :(
 */
inline void *
operator new(size_t size, TR_Memory * m, TR_AllocationKind allocKind, TR_MemoryBase::ObjectType ot)
   {
   return m->allocateMemory(size, allocKind, ot);
   }

inline void
operator delete(void *mem, TR_Memory * m, TR_AllocationKind allocKind, TR_MemoryBase::ObjectType ot)
   {
   return m->freeMemory(mem, allocKind, ot);
   }

inline void *
operator new[](size_t size, TR_Memory * m, TR_AllocationKind allocKind, TR_MemoryBase::ObjectType ot)
   {
   return m->allocateMemory(size, allocKind, ot);
   }

inline void
operator delete[](void *mem, TR_Memory * m, TR_AllocationKind allocKind, TR_MemoryBase::ObjectType ot)
   {
   m->freeMemory(mem, allocKind, ot);
   }

inline void *
TR_StackMemory::allocate(size_t size, TR_MemoryBase::ObjectType ot)
   {
   return trMemory().allocateStackMemory(size, ot);
   }

inline void *
TR_HeapMemory::allocate(size_t size, TR_MemoryBase::ObjectType ot)
   {
   return trMemory().allocateHeapMemory(size, ot);
   }

//On global gc end if not in compilation, and heap memory is available, return all to vm if total is larger than SIZE_OF_HEAP_SEGMENTS_TO_FLUSH_BY_GC. Now set to 4M. For 64bit platform 8M as they have more memory.
#if defined(TR_HOST_64BIT)
#define SIZE_OF_HEAP_SEGMENTS_TO_FLUSH_BY_GC 8388608 //8M
#else
#define SIZE_OF_HEAP_SEGMENTS_TO_FLUSH_BY_GC 4194304 //4M
#endif

#define DEFAULT_LIST_SIZE_LIMIT 10
#define SEGMENT_FRAGMENTATION_SIZE 32
#define MAX_SEGMENT_SIZE_MULTIPLIER 64

#define TR_PERSISTENT_ALLOC_WITHOUT_NEW(a) \
   static void * jitPersistentAlloc(size_t s)                                {return TR_Memory::jitPersistentAlloc(s, a); } \
   static void   jitPersistentFree(void *mem)                                 {TR_Memory::jitPersistentFree(mem); }

#define TR_PERSISTENT_NEW(a) \
   void * operator new   (size_t s, PERSISTENT_NEW_DECLARE)   throw()   {return TR_Memory::jitPersistentAlloc(s, a);} \
   void * operator new[] (size_t s, PERSISTENT_NEW_DECLARE)   throw()   {return TR_Memory::jitPersistentAlloc(s, a);} \
   void * operator new   (size_t s, TR_PersistentMemory * m)  throw()   {return m->allocatePersistentMemory(s, a);} \
   void * operator new[] (size_t s, TR_PersistentMemory * m)  throw()   {return m->allocatePersistentMemory(s, a);} \
   void operator delete  (void *p, size_t s) throw() { TR_ASSERT(false, "Invalid use of operator delete"); }

#define TR_ALLOC_WITHOUT_NEW(a) \
   TR_PERSISTENT_ALLOC_WITHOUT_NEW(a) \

#define TR_PERSISTENT_ALLOC_THROW(a) \
   TR_PERSISTENT_ALLOC_WITHOUT_NEW(a) \
   void * operator new   (size_t s, PERSISTENT_NEW_DECLARE) { void * alloc = TR_Memory::jitPersistentAlloc(s, a); if(!alloc) throw std::bad_alloc(); return alloc; } \
   void operator delete (void *p, PERSISTENT_NEW_DECLARE)   { TR_Memory::jitPersistentFree(p); } \
   void * operator new[] (size_t s, PERSISTENT_NEW_DECLARE m) { return operator new(s, m); } \
   void operator delete[] (void *p, PERSISTENT_NEW_DECLARE m) { operator delete (p, m); } \
   void * operator new (size_t s, TR_PersistentMemory * m) { void * alloc = m->allocatePersistentMemory(s, a); if(!alloc) throw std::bad_alloc(); return alloc; } \
   void operator delete (void *p, TR_PersistentMemory * m) { m->freePersistentMemory(p); } \
   void * operator new[] (size_t s, TR_PersistentMemory * m) { return operator new(s, m); } \
   void operator delete[] (void *p, TR_PersistentMemory * m) { operator delete(p, m); }

#define TR_PERSISTENT_ALLOC(a) \
   TR_PERSISTENT_ALLOC_WITHOUT_NEW(a) \
   TR_PERSISTENT_NEW(a)

#define TR_ALLOC(a) \
   TR_ALLOC_WITHOUT_NEW(a) \
   TR_PERSISTENT_NEW(a) \
   void * operator new (size_t s, TR_ArenaAllocator *m)                  {return m->allocate(s);} \
   void * operator new (size_t s, TR_HeapMemory m, TR_MemoryBase::ObjectType ot = a) { return m.allocate(s,ot); } \
   void operator delete(void *p, TR_HeapMemory m, TR_MemoryBase::ObjectType ot) { return m.deallocate(p); } \
   void * operator new[] (size_t s, TR_HeapMemory m, TR_MemoryBase::ObjectType ot = a) { return m.allocate(s,ot); } \
   void operator delete[] (void *p, TR_HeapMemory m, TR_MemoryBase::ObjectType ot) { return m.deallocate(p); } \
   void * operator new (size_t s, TR_StackMemory m, TR_MemoryBase::ObjectType ot = a) { return m.allocate(s,ot); } \
   void operator delete (void *p, TR_StackMemory m, TR_MemoryBase::ObjectType ot) { return m.deallocate(p); } \
   void * operator new[] (size_t s, TR_StackMemory m, TR_MemoryBase::ObjectType ot = a) { return m.allocate(s,ot); } \
   void operator delete[] (void *p, TR_StackMemory m, TR_MemoryBase::ObjectType ot) { return m.deallocate(p); } \
   void * operator new (size_t s, TR_Memory * m, TR_AllocationKind k) \
      { void *alloc = m->allocateMemory(s, k, a); return alloc; } \
   void operator delete (void *p, TR_Memory * m, TR_AllocationKind k) { m->freeMemory(p, k, a); } \
   void * operator new[] (size_t s, TR_Memory * m, TR_AllocationKind k) \
      {void *alloc = m->allocateMemory(s, k, a); return alloc; } \
   void operator delete[] (void *p, TR_Memory * m, TR_AllocationKind k) { m->freeMemory(p, k, a); } \
   void * operator new(size_t size, TR::Region &region) { return region.allocate(size); } \
   void operator delete(void * p, TR::Region &region) { region.deallocate(p); } \
   void * operator new[](size_t size, TR::Region &region) { return region.allocate(size); } \
   void operator delete[](void * p, TR::Region &region) { region.deallocate(p); } \


class TRPersistentMemoryAllocator
   {
   TR_PersistentMemory *memory;
public:
   TRPersistentMemoryAllocator(TR_Memory *m) : memory(m->trPersistentMemory()) { }
   TRPersistentMemoryAllocator(TR_PersistentMemory *m) : memory(m) { }

   TRPersistentMemoryAllocator(const TRPersistentMemoryAllocator &a) : memory(a.memory) { }

   void *allocate(size_t size, const char *name=NULL, int ignore=0)
      {
      return memory->allocatePersistentMemory(size, TR_Memory::CS2);
      }

   void deallocate(void *pointer, size_t size, const char *name=NULL, int ignore=0)
      {
      memory->freePersistentMemory(pointer);
      }

   void *reallocate(size_t newsize, void *ptr, size_t size, const char *name=NULL, int ignore=0)
      {
      if (newsize == size) return ptr;

      void *ret = allocate(newsize, name);
      memcpy(ret, ptr, size<newsize?size:newsize);
      deallocate(ptr, size, name);
      return ret;
      }

   template <class ostr, class allocator> ostr& stats(ostr &o, allocator &a)
      {
      o << "TRPersistentMemoryAllocator stats:\n";
      o << "not implemented\n";
      return o;
      }
   };

template <TR_AllocationKind kind, uint32_t minbits, uint32_t maxbits = sizeof(void *)*4>
class TRMemoryAllocator {
  TR_Memory *memory;
  bool  scavenge;
  void *(freelist[maxbits - minbits]);
  size_t stats_largeAlloc;
  size_t statsArray[maxbits - minbits];

  static uint32_t bucket(size_t size) {
    uint32_t i=minbits;

    while (i<maxbits && bucketsize(i) < size) i+=1;
    return i;
  }

  static size_t bucketsize(uint32_t b) {
    return size_t(1)<<b;
  }

public:
  TRMemoryAllocator(const TRMemoryAllocator<kind,minbits,maxbits> &a) : memory(a.memory), stats_largeAlloc(0), scavenge(a.scavenge) {
    memset(&freelist, 0, sizeof(freelist));
    memset(&statsArray, 0, sizeof(freelist));
  }

  TRMemoryAllocator<kind,minbits,maxbits> &operator= (const TRMemoryAllocator &a) {
    stats_largeAlloc = 0;
    scavenge = a.scavenge;
    memset(&freelist, 0, sizeof(freelist));
    memset(&statsArray, 0, sizeof(freelist));
  }

  TRMemoryAllocator(TR_Memory *m) : memory(m), stats_largeAlloc(0), scavenge(true) {
    memset(&freelist, 0, sizeof(freelist));
    memset(&statsArray, 0, sizeof(freelist));

  }

  template <uint32_t mi, uint32_t ma>
  TRMemoryAllocator(const TRMemoryAllocator<kind, mi, ma> &a) : memory(a.memory),
            stats_largeAlloc(0), scavenge(true) {
    memset(&freelist, 0, sizeof(freelist));
    memset(&statsArray, 0, sizeof(freelist));
  }

  static TRMemoryAllocator &instance()
     {
     TR_ASSERT(0, "no default constructor for TRMemoryAllocator");
     TRMemoryAllocator *instance = NULL;
     return *instance;
     }

  void set_scavange()   { scavenge=true; }
  void unset_scavange() { scavenge=false;}

  void *allocate(size_t size, const char *name=NULL, int ignore=0) {
    uint32_t b = bucket(size);

    if (b>=maxbits) {
      stats_largeAlloc += size;
      return  memory->allocateMemory(size, kind, TR_Memory::CS2);
    }

    if (freelist[b-minbits]) {
      void *ret = freelist[b-minbits];
      memcpy(&freelist[b-minbits],freelist[b-minbits], sizeof(void *));

      return ret;
    }

    // See if we have segments in the freelists of the larger segments.
    // Get a free segment from the fist available larger size and
    // add it to the freelist of the current bucket.

    if (scavenge) {
      for (uint32_t i=b+1; i < maxbits; i++) {
        uint32_t chunksize=0; uint32_t elements=0;
        if (freelist[i-minbits]) {
          chunksize = bucketsize(i);
          elements = (1 << (i-b)); // (2^i / 2^b)

          // remove this segment from its freelist
          char *freechunk = (char *) freelist[i-minbits];
          memcpy(&freelist[i-minbits],freelist[i-minbits], sizeof(void *));

          // set the link on the last element to NULL
          memset(freechunk+bucketsize(b)*(elements-1), 0, sizeof(void *));

          // Set the head of the new freelist
          freelist[b-minbits] = freechunk+bucketsize(b);

          // Now link up remaining elements to form the freelist
          for (int i=elements-2; i > 0; i--) {
            void *target = freechunk+bucketsize(b)*i;
            void *source = freechunk+(bucketsize(b)*(i+1));
            memcpy(target, &source, sizeof(void *));
          }

          return ((void *)freechunk);
        }
      }
    }

    statsArray[b-minbits] += bucketsize(b);
    return memory->allocateMemory(bucketsize(b), kind, TR_Memory::CS2);
  }

  void deallocate(void *pointer, size_t size, const char *name=NULL, int ignore=0) {
    uint32_t b = bucket(size);

    if (b<maxbits) {
      memcpy(pointer,&freelist[b-minbits], sizeof(void *));
      freelist[b-minbits]=pointer;
    }
  }

  void *reallocate(size_t newsize, void *ptr, size_t size, const char *name=NULL, int ignore=0) {
    uint32_t b1 = bucket(newsize);
    uint32_t b2 = bucket(size);

    if (b1==b2 && b1!=maxbits) return ptr;

    void *ret = allocate(newsize, name);
    memcpy(ret, ptr, size<newsize?size:newsize);
    deallocate(ptr, size, name);
    return ret;
  }

  template <class ostr, class allocator> ostr& stats(ostr &o, allocator &a) {
    o << "TRMemoryAllocator stats:\n";
    for (int i=minbits; i < maxbits; i++) {
      if (statsArray[i-minbits]) {
        int32_t sz=0;
        void *p = freelist[i-minbits];
        while (p) {
          sz += bucketsize(i);
          p = *(void **)p;
        }
        o <<  "bucket[" << i-minbits << "] (" << bucketsize(i) << ") : allocated = "
          << statsArray[i-minbits] <<  " bytes, freelist size = " << sz << "\n" ;
      }
    }

    return o;
  }
};

typedef TRMemoryAllocator<heapAlloc, 12, 28> TRCS2MemoryAllocator;

namespace TR
   {
   typedef CS2::heap_allocator< 65536, 12, TRCS2MemoryAllocator > ThreadLocalAllocator;
   typedef CS2::shared_allocator < ThreadLocalAllocator > Allocator;

   typedef TRPersistentMemoryAllocator CS2PersistentAllocator;

   typedef CS2::stat_allocator    < CS2PersistentAllocator > GlobalBaseAllocator;

   class GlobalSingletonAllocator: public GlobalBaseAllocator
      {
   public:
      static GlobalSingletonAllocator &instance()
         {
         if (_instance == NULL)
            createInstance();

         return *_instance;
         }

   private:
      GlobalSingletonAllocator(const GlobalBaseAllocator &a) : GlobalBaseAllocator(a)
         {
         TR_ASSERT(!_instance, "GlobalSingletonAllocator must be initialized only once");
         _instance = this;
         }

      static void createInstance();
      static GlobalSingletonAllocator *_instance;
      };

   typedef CS2::shared_allocator < GlobalSingletonAllocator > GlobalAllocator;

   static GlobalAllocator globalAllocator(const char *name = NULL)
      {
      return GlobalAllocator(GlobalSingletonAllocator::instance());
      }

   /*
    * some common CS2 datatypes
    */
   typedef CS2::ASparseBitVector<TR::Allocator> SparseBitVector;
   typedef CS2::ABitVector<TR::Allocator>       BitVector;

   typedef CS2::ASparseBitVector<TR::GlobalAllocator> GlobalSparseBitVector;
   typedef CS2::ABitVector<TR::GlobalAllocator>       GlobalBitVector;

   class AllocatedMemoryMeter
      {
      private:
      static const bool isWithFreed = true;
      static const bool isWithHWM = true;

      public:
      class Metric {
         private:
         uint64_t _allocated;           // allocated bytes
         uint64_t _freed;               // freed bytes
         uint64_t _maxLive;             // high water mark for live bytes

         public:
         uint64_t live() { return _allocated - _freed; }

         void update_allocated(uint64_t allocated)
            {
            _allocated += allocated;
            if (live() > _maxLive)
               _maxLive = live();
            }

         void update_freed(uint64_t freed)
            {
            _freed += freed;
            }

         public:


         Metric() {} // default constructor of Metric is to be uninitialized

         // only allow conversion construction Metric m = 0; ie v must be zero
         Metric(const uint64_t v) : _allocated(v), _freed(v), _maxLive(v)
            {
            TR_ASSERT(v == 0, "conversion constructor is valid only when converting from zero");
            }

         Metric operator- (const Metric operand) const
            {
            Metric result = *this;
            result -= operand;
            return result;
            }

         // this is used for accumulated differences only
         Metric & operator+= (const Metric &increment)
            {
            _allocated += increment._allocated;
            _freed += increment._freed;
            _maxLive += increment._maxLive;

            return *this;
            }

         // this is used for differences only
         Metric & operator-= (const Metric &increment)
            {
            _allocated -= increment._allocated;
            _freed -= increment._freed;
            _maxLive -= increment._maxLive;

            return *this;
            }

         // basic printf
         void _printf()
            {
            printf("%11llu %11llu %11llu %11llu", (long long unsigned int)_allocated, (long long unsigned int)_freed, (long long unsigned int)_maxLive, (long long unsigned int)live());
            }


         friend class AllocatedMemoryMeter;
      };
      public:

      // TODO: make private once removing tracing code
      static Metric _currentMemUsage;          // current statistics across all alloc types and memory, persistent, stack, heap and CS2 allocators
      static uint8_t _enabled;                 // which memory types will be profiled from TR_AllocationKind

      public:
      static void update_allocated(uint64_t allocated, TR_AllocationKind allocationKind)
         {
         if (_enabled & allocationKind)
            _currentMemUsage.update_allocated(allocated);
         }
      static void update_freed(uint64_t freed, TR_AllocationKind allocationKind)
         {
         if (_enabled & allocationKind)
            _currentMemUsage.update_freed(freed);
         }
      static void check(TR_Memory *trMemory, const char * file, int line);

      /**
       * \brief Starts the meter from the currently allocated bytes.
       */
      void Start(void)
         {
         _reading = _currentMemUsage;
         // _reading._printf(); printf("\n");
         }

      /**
       * \brief Stops the meter. The allocated bytes counted so far is kept and resuming the
       * meter with Start() will resume counting from this point. Call Reset() to
       * reset the count to 0.
       */
      void Stop(void)
         {
         Metric current = _currentMemUsage;
         if ((current._allocated >= _reading._allocated) && (current._freed >= _reading._freed))
            _reading = current - _reading;
         else
            _reading = 0;
         // _currentMemUsage._printf();  printf("\n");
         // printf("%40.40s ", "diff = "); _reading._printf(); printf("\n");
         }

      /**
       * \brief Resets the meter to a count of 0. Stops the meter if it was running.
       */
      void Reset(void)
         {
         _reading = 0;
         }

      /**
       * \brief Returns the metric of this meter
       */
      Metric Read(void) const { return _reading; }

      static char* Name(bool csv = false)
         {
         if (csv)
            return "Allocs";
         else
            return "Memory Usage (bytes)";
         }

      static char *UnitsText(bool alternativeFormat = false /* ignored */)
         {
            return "allocated (% total)  freed (% total)  maxLive (% total)";
         }

      static uint32_t sprintf_part(char *line, uint64_t bytes, uint64_t total)
         {
         uint32_t offset = 0;

         float ratio = total?(float(bytes)/float(total))*100:0;

         offset += sprintf(line+offset, "%12llu ", (long long unsigned int)bytes);
         offset += sprintf(line+offset, " (%5.1f%%)", ratio);
         return offset;
         }

      static uint32_t sprintfMetric(char *line, Metric value, Metric total, bool alternativeFormat = false /* ignored */, bool csv = false)
         {
         uint32_t offset = 0;
         if (csv)
            {
            offset += sprintf(line, "\"%llu", (long long unsigned int)value._allocated);
            if (isWithFreed) offset += sprintf(line+offset, "%llu", (long long unsigned int)value._freed);
            if (isWithHWM) offset += sprintf(line+offset, "%llu", (long long unsigned int)value._maxLive);
            offset += sprintf(line+offset, "\"");
            }
         else
            {
            offset += sprintf_part(line+offset, value._allocated, total._allocated);
            if (isWithFreed)
               {
               offset += sprintf(line+offset, " ");
               offset += sprintf_part(line+offset, value._freed, total._freed);
               }
            if (isWithHWM)
               {
               offset += sprintf(line+offset, " ");
               offset += sprintf_part(line+offset, value._maxLive, total._maxLive);
               }
            }
         return offset;
         }
   private:
      Metric _reading;     // meter reading while metering, uninitialised on construction
   };

   typedef CS2::RunnableMeter<AllocatedMemoryMeter> MemoryMeter;
   typedef CS2::LexicalBlockProfiler<MemoryMeter, Allocator>  LexicalMemProfiler;
   typedef CS2::PhaseMeasuringSummary<MemoryMeter, Allocator> PhaseMemSummary;
   }
typedef CS2::arena_allocator  <65536, TR::Allocator> TR_ArenaAllocator;

/*
 * Added the following static function to
 * reduce the verbosity involved in declaring a typed_allocator
 * everytime we need an stl object.
 */

template <typename T>
static inline TR::typed_allocator<T, TR::Allocator> getTypedAllocator(TR::Allocator al)
{
	TR::typed_allocator<T, TR::Allocator> ta(al);
	return ta;
}

#endif
