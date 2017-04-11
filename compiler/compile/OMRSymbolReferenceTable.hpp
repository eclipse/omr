/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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
 ******************************************************************************/

#ifndef OMR_SYMBOLREFERENCETABLE_INCL
#define OMR_SYMBOLREFERENCETABLE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SYMBOLREFERENCETABLE_CONNECTOR
#define OMR_SYMBOLREFERENCETABLE_CONNECTOR
namespace OMR { class SymbolReferenceTable; }
namespace OMR { typedef OMR::SymbolReferenceTable SymbolReferenceTableConnector; }
#endif

#include "infra/HashTab.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"

#include <map>                                 // for std::map
#include <stddef.h>                            // for NULL, size_t
#include <stdint.h>                            // for int32_t, etc
#include "env/TRMemory.hpp"                    // for Allocator, etc
#include "codegen/FrontEnd.hpp"                // for TR_FrontEnd
#include "env/KnownObjectTable.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/AliasBuilder.hpp"            // for AliasBuilder
#include "compile/Method.hpp"                  // for mcount_t
#include "cs2/hashtab.h"                       // for HashTable, etc
#include "env/jittypes.h"                      // for intptrj_t, uintptrj_t
#include "il/DataTypes.hpp"                    // for DataTypes, etc
#include "il/Symbol.hpp"                       // for Symbol
#include "il/symbol/MethodSymbol.hpp"          // for MethodSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"  // for RegsiterMappedSymbol
#include "infra/Array.hpp"                     // for TR_Array
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/BitVector.hpp"                 // for TR_BitVector
#include "infra/Flags.hpp"                     // for flags8_t
#include "infra/Link.hpp"                      // for TR_Link, etc
#include "infra/List.hpp"                      // for List, etc
#include "runtime/Runtime.hpp"


namespace OMR
{

class SymbolReferenceTable
   {

   public:

   SymbolReferenceTable(size_t size, TR::Compilation *comp);

   TR_ALLOC(TR_Memory::SymbolReferenceTable)

   TR::SymbolReferenceTable *self();

   TR::Compilation *comp() { return _compilation; }
   TR_FrontEnd *fe() { return _fe; }
   TR_Memory *trMemory() { return _trMemory; }
   TR_StackMemory trStackMemory() { return _trMemory; }
   TR_HeapMemory trHeapMemory() { return _trMemory; }

   TR_Array<TR::SymbolReference *> baseArray;

   TR::AliasBuilder aliasBuilder;

   enum CommonNonhelperSymbol
      {
      // Note: when using these enumerations to index into the symbol reference table
      //       you must add _numHelperSymbols to the value

      arraySetSymbol,
      arrayCopySymbol,
      arrayCmpSymbol,
      prefetchSymbol,

      killsAllMethodSymbol,                           // A dummy method whose alias set includes all
      usesAllMethodSymbol,                            // A dummy method whose use only alias set includes all

      firstArrayShadowSymbol,

      lastWCodeNonhelperSymbol = firstArrayShadowSymbol + TR::NumTypes,
      lastWCodeNonhelperSymbolX,

      firstArrayletShadowSymbol = firstArrayShadowSymbol + TR::NumTypes,

      firstCommonNonhelperNonArrayShadowSymbol = firstArrayletShadowSymbol + TR::NumTypes,

      contiguousArraySizeSymbol = firstCommonNonhelperNonArrayShadowSymbol,
      discontiguousArraySizeSymbol,
      arrayClassRomPtrSymbol,
      classRomPtrSymbol,
      javaLangClassFromClassSymbol,
      classFromJavaLangClassSymbol,
      addressOfClassOfMethodSymbol,
      ramStaticsFromClassSymbol,
      componentClassSymbol,
      componentClassAsPrimitiveSymbol,
      isArraySymbol,
      isClassAndDepthFlagsSymbol,
      isClassFlagsSymbol,
      vftSymbol,
      bcdCheckSymbol,
      currentThreadSymbol,
      recompilationCounterSymbol,
      excpSymbol,
      indexableSizeSymbol,
      resolveCheckSymbol,
      arrayTranslateSymbol,
      arrayTranslateAndTestSymbol,
      long2StringSymbol,
      bitOpMemSymbol,
      reverseLoadSymbol,
      reverseStoreSymbol,
      currentTimeMaxPrecisionSymbol,
      headerFlagsSymbol,
      singlePrecisionSQRTSymbol,
      threadPrivateFlagsSymbol,  // private flags slot on j9vmthread
      arrayletSpineFirstElementSymbol,// address of contiguous arraylet spine first element
      dltBlockSymbol,            // DLT Block field in j9vmthread
      countForRecompileSymbol,   // global boolean indicating when methods should start counting for guarded recompilations
      gcrPatchPointSymbol,       // address where we need to patch GCR guard instruction
      counterAddressSymbol,      // address of invokation counter for indirect loads and stores
      startPCSymbol,             // startPC of the compiled method
      compiledMethodSymbol,      // J9Method of the compiled method
      thisRangeExtensionSymbol,
      profilingBufferCursorSymbol,// profilingBufferCursor slot on j9vmthread
      profilingBufferEndSymbol,  // profilingBufferEnd slot on j9vmthread
      profilingBufferSymbol,     // profilingBuffer on j9vmthread
      osrBufferSymbol,     // osrBuffer slot on j9vmthread
      osrScratchBufferSymbol,    //osrScratchBuffer slot on  j9vmthread
      osrFrameIndexSymbol,       // osrFrameIndex slot on j9vmthread
      osrReturnAddressSymbol,       // osrFrameIndex slot on j9vmthread
      lowTenureAddressSymbol,    // on j9vmthread
      highTenureAddressSymbol,   // on j9vmthread
      fragmentParentSymbol,
      globalFragmentSymbol,
      instanceShapeSymbol,
      instanceDescriptionSymbol,
      descriptionWordFromPtrSymbol,
      classFromJavaLangClassAsPrimitiveSymbol,
      javaVMSymbol,
      heapBaseSymbol,
      heapTopSymbol,
      j9methodExtraFieldSymbol,
      startPCLinkageInfoSymbol,
      instanceShapeFromROMClassSymbol,

      /** \brief
       *
       *  This symbol is used by the code generator to recognize and inline a call which emulates the following
       *  tree sequence:
       *
       *  <code>
       *  monent
       *    object
       *  aloadi / iloadi
       *    ==>object
       *  monexit
       *  monexitfence
       *    ==>object
       *  </code>
       *
       *  Where <c>object</c> is a valid object reference. The sequence represents a field load within a
       *  synchronized region. Since a full monent / monexit operation for a single load is expensive, some code
       *  generators can emit an optimized instruction sequence to load the field atomically while conforming
       *  to the monent / monexit semantics.
       */
      synchronizedFieldLoadSymbol,

      // common atomic primitives
      atomicAdd32BitSymbol,
      atomicAdd64BitSymbol,
      atomicFetchAndAdd32BitSymbol,
      atomicFetchAndAdd64BitSymbol,
      atomicSwap32BitSymbol,
      atomicSwap64BitSymbol,
      atomicCompareAndSwap32BitSymbol,
      atomicCompareAndSwap64BitSymbol,

      // python symbols start here
      pythonFrameCodeObjectSymbol,   // code object from the frame object
      pythonFrameFastLocalsSymbol,   // fastlocals array base from the frame object
      pythonFrameGlobalsSymbol,      // globals dict object from the frame object
      pythonFrameBuiltinsSymbol,     // builtins dict object from the frame object
      pythonCodeConstantsSymbol,     // the code constants tuple object
      pythonCodeNumLocalsSymbol,     // number of local variables from the code object
      pythonCodeNamesSymbol,         // names tuple object from the code object
      pythonObjectTypeSymbol,        // type pointer from a python object
      pythonTypeIteratorMethodSymbol,// used for the iterator method slot from a python type
      pythonObjectRefCountSymbol,

      firstPerCodeCacheHelperSymbol,
      lastPerCodeCacheHelperSymbol = firstPerCodeCacheHelperSymbol + TR_numCCPreLoadedCode - 1,

      lastCommonNonhelperSymbol
      };

  CommonNonhelperSymbol getLastCommonNonhelperSymbol()
      {
      return lastCommonNonhelperSymbol;
      }

   // Check whether the given symbol reference (or reference number) is the
   // known "non-helper" symbol reference.
   //
   bool isNonHelper(TR::SymbolReference *, CommonNonhelperSymbol);
   bool isNonHelper(int32_t, CommonNonhelperSymbol);

   // Total number of symbols (known and dynamic) in the SRT
   //
   int32_t getNumSymRefs()                            { return baseArray.size(); }
   void    setNumSymRefs(int32_t n)                   { baseArray.setSize(n); }

   // Number of known symbols with space reserved in the SRT
   //
   uint32_t getNumPredefinedSymbols() { return _numPredefinedSymbols; }

   int32_t getIndexOfFirstSymRef()                    { return 0; }

   // Add a symbol reference to the SRT and return its reference number
   //
   int32_t assignSymRefNumber(TR::SymbolReference *sf) { return baseArray.add(sf); }

   int32_t getNumUnresolvedSymbols()                  { return _numUnresolvedSymbols; }
   int32_t getNonhelperIndex(CommonNonhelperSymbol s);
   int32_t getNumHelperSymbols()                      { return _numHelperSymbols; }
   int32_t getArrayShadowIndex(TR::DataType t)        { return _numHelperSymbols + firstArrayShadowSymbol + t; }
   int32_t getArrayletShadowIndex(TR::DataType t) { return _numHelperSymbols + firstArrayShadowSymbol + TR::NumTypes + t; }

   template <class BitVector>
   void getAllSymRefs(BitVector &allSymRefs)
      {
      for (int32_t symRefNumber = getIndexOfFirstSymRef(); symRefNumber < getNumSymRefs(); symRefNumber++)
         {
         TR::SymbolReference *symRef = getSymRef(symRefNumber);
         if (symRef)
            allSymRefs[symRefNumber] = true;
         }
      }

   // --------------------------------------------------------------------------

   TR::SymbolReference * & element(TR_RuntimeHelper s);
   TR::SymbolReference * & element(CommonNonhelperSymbol s);

   TR::SymbolReference * getSymRef(CommonNonhelperSymbol i);
   TR::SymbolReference * getSymRef(int32_t i) { return baseArray.element(i); }

   TR::SymbolReference * createRuntimeHelper(TR_RuntimeHelper index, bool canGCandReturn, bool canGCandExcept, bool preservesAllRegisters);
   TR::SymbolReference * findOrCreateRuntimeHelper(TR_RuntimeHelper index, bool canGCandReturn, bool canGCandExcept, bool preservesAllRegisters);

   TR::SymbolReference * findOrCreateCodeGenInlinedHelper(CommonNonhelperSymbol index);

   TR::ParameterSymbol * createParameterSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType, bool isUnsigned);
   TR::SymbolReference * findOrCreateAutoSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType, bool isReference = true,
         bool isInternalPointer = false, bool reuseAuto = true, bool isAdjunct = false, size_t size = 0);
   TR::SymbolReference * createTemporary(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::DataType, bool isInternalPointer = false, size_t size = 0);
   TR::SymbolReference * createCoDependententTemporary(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::DataType, bool isInternalPointer, size_t size,
         TR::Symbol *coDependent, int32_t offset);
   TR::SymbolReference * findStaticSymbol(TR_ResolvedMethod * owningMethod, int32_t cpIndex, TR::DataType);

   // --------------------------------------------------------------------------
   // OMR
   // --------------------------------------------------------------------------

   TR::SymbolReference * findOrCreatePendingPushTemporary(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t stackDepth, TR::DataType, size_t size = 0);
   TR::SymbolReference * createLocalPrimArray(int32_t objectSize, TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t arrayType);
   TR::Symbol           * findOrCreateGenericIntShadowSymbol();
   TR::Symbol           * findGenericIntShadowSymbol() { return _genericIntShadowSymbol; }

   int32_t getNumInternalPointers() { return _numInternalPointers; }

   TR::SymbolReference * methodSymRefFromName(TR::ResolvedMethodSymbol *owningMethodSymbol, char *className, char *methodName, char *signature, TR::MethodSymbol::Kinds kind, int32_t cpIndex=-1);

   TR::SymbolReference *createSymbolReference(TR::Symbol *sym, intptrj_t o = 0);

   TR::Symbol * findOrCreateConstantAreaSymbol();
   TR::SymbolReference * findOrCreateConstantAreaSymbolReference();
   bool  isConstantAreaSymbolReference(TR::SymbolReference *symRef) { return _constantAreaSymbolReference && _constantAreaSymbolReference == symRef; }
   bool  isConstantAreaSymbol(TR::Symbol *sym) { return _constantAreaSymbol && _constantAreaSymbol == sym; }

   bool isVtableEntrySymbolRef(TR::SymbolReference * s) { return _vtableEntrySymbolRefs.find(s); }

   TR::SymbolReference * createKnownStaticDataSymbolRef(void *counterAddress, TR::DataType type);
   TR::SymbolReference * createKnownStaticDataSymbolRef(void *counterAddress, TR::DataType type, TR::KnownObjectTable::Index knownObjectIndex);
   TR::SymbolReference * findOrCreateTransactionEntrySymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateTransactionExitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateTransactionAbortSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreatePrefetchSymbol();
   TR::SymbolReference * findPrefetchSymbol() { return element(prefetchSymbol); }
   TR::SymbolReference * findOrCreateStartPCSymbolRef();
   TR::SymbolReference * findOrCreateAThrowSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateCheckCastSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateDivCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateOverflowCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateInstanceOfSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);

   // symbol
   TR::SymbolReference * findOrCreateOSRReturnAddressSymbolRef();

   // base recompilation
   TR::SymbolReference * findOrCreateRecompilationCounterSymbolRef(void *counterAddress);

   TR::SymbolReference * findOrCreateMethodSymbol(mcount_t owningMethodIndex, int32_t cpIndex, TR_ResolvedMethod *, TR::MethodSymbol::Kinds, bool = false);
   TR::SymbolReference * findOrCreateComputedStaticMethodSymbol(mcount_t owningMethodIndex, int32_t cpIndex, TR_ResolvedMethod * resolvedMethod);
   TR::SymbolReference * findOrCreateStaticMethodSymbol(mcount_t owningMethodIndex, int32_t cpIndex, TR_ResolvedMethod * resolvedMethod);
   TR::SymbolReference * findJavaLangClassFromClassSymbolRef();

   // FE, CG, optimizer
   TR::SymbolReference * findOrCreateNullCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findVftSymbolRef();
   TR::SymbolReference * findOrCreateVftSymbolRef();
   TR::SymbolReference * findOrCreateArrayBoundsCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);

   // optimizer (EA)
   TR::SymbolReference * createLocalObject(int32_t objectSize, TR::ResolvedMethodSymbol * owningMethodSymbol, TR::SymbolReference *classSymRef);
   TR::SymbolReference * createLocalAddrArray(int32_t objectSize, TR::ResolvedMethodSymbol * owningMethodSymbol, TR::SymbolReference *classSymRef);

   // optimizer
   TR::SymbolReference * findInstanceShapeSymbolRef();
   TR::SymbolReference * findInstanceDescriptionSymbolRef();
   TR::SymbolReference * findDescriptionWordFromPtrSymbolRef();
   TR::SymbolReference * findClassFlagsSymbolRef();
   TR::SymbolReference * findClassAndDepthFlagsSymbolRef();
   TR::SymbolReference * findArrayComponentTypeSymbolRef();
   TR::SymbolReference * findClassIsArraySymbolRef();
   TR::SymbolReference * findHeaderFlagsSymbolRef() { return element(headerFlagsSymbol); }
   TR::SymbolReference * createKnownStaticRefereneceSymbolRef(void *address, TR::KnownObjectTable::Index knownObjectIndex=TR::KnownObjectTable::UNKNOWN);

   TR::SymbolReference * findOrCreateArrayTranslateSymbol();
   TR::SymbolReference * findOrCreateSinglePrecisionSQRTSymbol();
   TR::SymbolReference * findOrCreateCurrentTimeMaxPrecisionSymbol();
   TR::SymbolReference * findOrCreateArrayTranslateAndTestSymbol();
   TR::SymbolReference * findOrCreatelong2StringSymbol();
   TR::SymbolReference * findOrCreatebitOpMemSymbol();

   // compilation, optimizer
   TR::SymbolReference * findArrayClassRomPtrSymbolRef();
   TR::SymbolReference * findClassRomPtrSymbolRef();
   TR::SymbolReference * findClassFromJavaLangClassSymbolRef();

   // CG, optimizer
   TR::SymbolReference * findThisRangeExtensionSymRef(TR::ResolvedMethodSymbol *owningMethodSymbol = 0);

   TR::SymbolReference * findOrCreateSymRefWithKnownObject(TR::SymbolReference *original, uintptrj_t *referenceLocation);
   TR::SymbolReference * findOrCreateSymRefWithKnownObject(TR::SymbolReference *original, TR::KnownObjectTable::Index objectIndex);
   TR::SymbolReference * findOrCreateThisRangeExtensionSymRef(TR::ResolvedMethodSymbol *owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateContiguousArraySizeSymbolRef();
   TR::SymbolReference * findOrCreateNewArrayNoZeroInitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateNewObjectSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateNewObjectNoZeroInitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateArrayStoreExceptionSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateArrayShadowSymbolRef(TR::DataType, TR::Node * baseAddress = 0);
   TR::SymbolReference * findOrCreateArrayletShadowSymbolRef(TR::DataType type);
   TR::SymbolReference * findOrCreateAsyncCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateExcpSymbolRef();
   TR::SymbolReference * findOrCreateANewArrayNoZeroInitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);

   // cg linkage, optimizer, base chtable
   TR::SymbolReference * findObjectNewInstanceImplSymbol() { return _ObjectNewInstanceImplSymRef; }

   // fe, cg
   TR::SymbolReference * findOrCreateNewArraySymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateReportMethodEnterSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);

   TR::SymbolReference * findOrCreateIndexableSizeSymbolRef();
   TR::SymbolReference * findOrCreateGCRPatchPointSymbolRef();

   // compilation
   TR::SymbolReference * findAddressOfClassOfMethodSymbolRef();
   TR::SymbolReference * findOrCreateVtableEntrySymbolRef(TR::ResolvedMethodSymbol * calleeSymbol, int32_t vtableSlot);
   TR::SymbolReference * createIsOverriddenSymbolRef(TR::ResolvedMethodSymbol * calleeSymbol); /* never reuse is overridden symrefs */

   // fe, base node
   TR::SymbolReference * findOrCreateResolveCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);

   TR::SymbolReference * findOrCreateVolatileReadLongSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateVolatileWriteLongSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateVolatileReadDoubleSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateVolatileWriteDoubleSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateMonitorEntrySymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateMonitorExitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);

   // Z
   TR::SymbolReference * findDLPStaticSymbolReference(TR::SymbolReference * staticSymbolReference);
   TR::SymbolReference * findOrCreateDLPStaticSymbolReference(TR::SymbolReference * staticSymbolReference);

   TR::SymbolReference * findOrCreateGenericIntShadowSymbolReference(intptrj_t offset, bool allocateUseDefBitVector = false);
   TR::SymbolReference * createGenericIntShadowSymbolReference(intptrj_t offset, bool allocateUseDefBitVector = false);
   TR::SymbolReference * findOrCreateGenericIntArrayShadowSymbolReference(intptrj_t offset);
   TR::SymbolReference * findOrCreateGenericIntNonArrayShadowSymbolReference(intptrj_t offset);

   TR::SymbolReference * findOrCreateArrayCopySymbol();
   TR::SymbolReference * findOrCreateArraySetSymbol();
   TR::SymbolReference * findOrCreateArrayCmpSymbol();

   TR::SymbolReference * findOrCreateClassSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, void * classObject, bool cpIndexOfStatic = false);
   TR::SymbolReference * findOrCreateArrayShadowSymbolRef(TR::DataType type, TR::Node * baseArrayAddress, int32_t size, TR_FrontEnd * fe);

   TR::SymbolReference * findOrCreateCounterAddressSymbolRef();
   TR::SymbolReference * findOrCreateCounterSymRef(char *name, TR::DataType d, void *address);
   TR::SymbolReference * createRefinedArrayShadowSymbolRef(TR::DataType);
   TR::SymbolReference * createRefinedArrayShadowSymbolRef(TR::DataType, TR::Symbol *); // TODO: to be changed to a special sym ref
   bool                 isRefinedArrayShadow(TR::SymbolReference *symRef);


   // A RegisterSymbol is a pseudo memory location that represents some machine
   // register or a value we would like the register allocator to map to a register.
   // Such a RegisterSymbol belongs to a method as its conceptual life span is the
   // life of a procedure. A set of hardware registers is pre-created for each method.
   //
   void initRegisterSymbols(TR::ResolvedMethodSymbol *);
   TR::SymbolReference * createRegisterSymbol(TR::ResolvedMethodSymbol *, TR_RegisterKinds, TR::DataType, TR_GlobalRegisterNumber grn);
   TR::SymbolReference * getRegisterSymbol(TR_GlobalRegisterNumber grn);

   /*
    * --------------------------------------------------------------------------
    */

   void makeAutoAvailableForIlGen(TR::SymbolReference * a);
   TR::SymbolReference * findAvailableAuto(TR::DataType dataType, bool behavesLikeTemp, bool isAdjunct = false);
   TR::SymbolReference * findAvailableAuto(List<TR::SymbolReference> &, TR::DataType dataType, bool behavesLikeTemp, bool isAdjunct = false);
   void clearAvailableAutos() { _availableAutos.init(); }

   /*
    * For union type support
    */

   // Mark two symbol references as shared aliases of each other 
   void makeSharedAliases(TR::SymbolReference *sr1, TR::SymbolReference *sr2);
   // Retrieve shared aliases bitvector for a given symbol reference
   TR_BitVector *getSharedAliases(TR::SymbolReference *sr);

   protected:

   TR::SymbolReference * findOrCreateCPSymbol(TR::ResolvedMethodSymbol *, int32_t, TR::DataType, bool, void *);

   bool shouldMarkBlockAsCold(TR_ResolvedMethod * owningMethod, bool isUnresolvedInCP);
   void markBlockAsCold();

   char *strdup(const char *arg);

   TR::Symbol *                       _genericIntShadowSymbol;

   TR::Symbol *                         _constantAreaSymbol;
   TR::SymbolReference *                _constantAreaSymbolReference;

   TR::SymbolReference *                _ObjectNewInstanceImplSymRef;

   TR_Array<TR_BitVector *>            _knownObjectSymrefsByObjectIndex;

   TR_Array<TR::SymbolReference *> *    _unsafeSymRefs;
   TR_Array<TR::SymbolReference *> *    _unsafeVolatileSymRefs;

   List<TR::SymbolReference>            _availableAutos;
   List<TR::SymbolReference>            _vtableEntrySymbolRefs;
   List<TR::SymbolReference>            _classLoaderSymbolRefs;
   List<TR::SymbolReference>            _classStaticsSymbolRefs;
   List<TR::SymbolReference>            _classDLPStaticsSymbolRefs;
   List<TR::SymbolReference>            _debugCounterSymbolRefs;
   TR::Symbol                           *_currentThreadDebugEventDataSymbol;
   List<TR::SymbolReference>            _currentThreadDebugEventDataSymbolRefs;

   uint32_t                            _nextRegShadowIndex;
   TR_Array<TR::SymbolReference *>      *_registerSymbolRefs;

   CS2::HashTable<uint32_t, TR::Symbol *, TR::Allocator> _aggregateShadowSymbolMap;
   CS2::HashTable<TR::SparseBitVector *, TR::SymbolReference *, TR::Allocator> _aggregateShadowSymbolReferenceMap;
   int32_t                             _numUnresolvedSymbols;
   uint32_t                            _numHelperSymbols;
   uint32_t                            _numPredefinedSymbols;
   int32_t                             _numInternalPointers;
   bool                                _hasImmutable;
   bool                                _hasUserField;

   size_t                              _size_hint;

   TR_HashTab *_genericCallHash;

   typedef CS2::CompoundHashKey<mcount_t, const char *> OwningMethodAndString;
   typedef CS2::HashTable<OwningMethodAndString, TR::SymbolReference *, TR::Allocator> SymrefsByOwningMethodAndString;

   SymrefsByOwningMethodAndString      _methodsBySignature;

   // Aliasmap is keyed by a symbol reference's reference number
   typedef TR::typed_allocator<std::pair<int32_t, TR_BitVector *>, TR::Allocator> AliasMapAllocator;
   typedef std::map<int32_t, TR_BitVector *, std::less<int32_t>, AliasMapAllocator> AliasMap;
   AliasMap                            *_sharedAliasMap;

   TR_FrontEnd *_fe;
   TR::Compilation *_compilation;
   TR_Memory *_trMemory;

   // J9
#define _numNonUserFieldClasses 4

   };

}

#endif
