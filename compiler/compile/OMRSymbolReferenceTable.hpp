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

#include "il/symbol/ResolvedMethodSymbol.hpp"

#include <map>
#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "codegen/FrontEnd.hpp"
#include "env/KnownObjectTable.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/AliasBuilder.hpp"
#include "compile/Method.hpp"
#include "cs2/hashtab.h"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
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
      initializeStatusFromClassSymbol,
      isClassFlagsSymbol,
      vftSymbol,
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

      /** \brief
       *
       *  A call with this symbol marks a place in the jitted code where OSR transition to the VM interpreter is supported.
       *  The transition target bytecode is the bytecode index on the call plus an induction offset which is stored on the
       *  call node.
       *
       *  \code
       *    call <potentialOSRPointHelperSymbol>
       *  \endcode
       *
       *  \note
       *   The call is not to be codegen evaluated, it should be cleaned up before codegen.
       */
      potentialOSRPointHelperSymbol,
      /** \brief
       *
       *  A call with this symbol marks a place that has been optimized with runtime assumptions. Such place needs protection of OSR
       *  points. When the assumption becomes wrong, the execution of jitted code with the assumption has to be transition to the VM
       *  interpreter before running the invalid code.
       *
       *  \code
       *    call <osrFearPointHelperSymbol>
       *  \endcode
       *
       *  \note
       *   The call is not to be codegen evaluated, it should be cleaned up before codegen.
       */
      osrFearPointHelperSymbol,
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
      j9methodConstantPoolSymbol,
      startPCLinkageInfoSymbol,
      instanceShapeFromROMClassSymbol,

      /** \brief
       *
       *  This symbol is used by the code generator to recognize and inline a call which emulates the following
       *  tree sequence:
       *
       *  \code
       *  monent
       *    object
       *  ...
       *  aloadi / iloadi / lloadi
       *    ==>object
       *  ...
       *  monexitfence
       *  monexit
       *    ==>object
       *  \endcode
       *
       *  Where <c>object</c> is a valid object reference. The sequence represents a field load within a
       *  synchronized region. Since a full monent / monexit operation for a single load is expensive, some code
       *  generators can emit an optimized instruction sequence to load the field atomically while conforming
       *  to the monent / monexit semantics.
       */
      synchronizedFieldLoadSymbol,

      /** \brief
       *
       *  This symbol represents an intrinsic call of the following format:
       *
       *  \code
       *    icall/lcall <atomicAddSymbol>
       *      <address>
       *      <value>
       *  \endcode
       *
       *  Which performs the following operation atomically:
       *
       *  \code
       *    [address] = [address] + <value>
       *    return <value>
       *  \endcode
       *
       *  The data type of \c <value> indicates the width of the operation.
       */
      atomicAddSymbol,

      /** \brief
       *
       *  This symbol represents an intrinsic call of the following format:
       *
       *  \code
       *    icall/lcall <atomicFetchAndAddSymbol>
       *      <address>
       *      <value>
       *  \endcode
       *
       *  Which performs the following operation atomically:
       *
       *  \code
       *    temp = [address]
       *    [address] = [address] + <value>
       *    return temp
       *  \endcode
       *
       *  The data type of \c <value> indicates the width of the operation.
       */
      atomicFetchAndAddSymbol,
      atomicFetchAndAdd32BitSymbol,
      atomicFetchAndAdd64BitSymbol,

      /** \brief
       *
       *  This symbol represents an intrinsic call of the following format:
       *
       *  \code
       *    icall/lcall <atomicSwapSymbol>
       *      <address>
       *      <value>
       *  \endcode
       *
       *  Which performs the following operation atomically:
       *
       *  \code
       *    temp = [address]
       *    [address] = <value>
       *    return temp
       *  \endcode
       *
       *  The data type of \c <value> indicates the width of the operation.
       */
      atomicSwapSymbol,
      atomicSwap32BitSymbol,
      atomicSwap64BitSymbol,

      /** \brief
       *
       *  This symbol represents an intrinsic call of the following format:
       *
       *  \code
       *    icall <atomicCompareAndSwapReturnStatusSymbol>
       *      <address>
       *      <old value>
       *      <new value>
       *  \endcode
       *
       *  Which performs the following operation atomically:
       *
       *  \code
       *    temp = [address]
       *    if (temp == <old value>)
       *      [address] = <new value>
       *      return true
       *    else
       *      return false
       *  \endcode
       *
       *  The data type of \c <old value> indicates the width of the operation.
       */
      atomicCompareAndSwapReturnStatusSymbol,

      /** \brief
       *
       *  This symbol represents an intrinsic call of the following format:
       *
       *  \code
       *    icall/lcall <atomicCompareAndSwapReturnValueSymbol>
       *      <address>
       *      <old value>
       *      <new value>
       *  \endcode
       *
       *  Which performs the following operation atomically:
       *
       *  \code
       *    temp = [address]
       *    if (temp == <old value>)
       *      [address] = <new value>
       *    return temp
       *  \endcode
       *
       *  The data type of \c <old value> indicates the width of the operation.
       */
      atomicCompareAndSwapReturnValueSymbol,

      firstPerCodeCacheHelperSymbol,
      lastPerCodeCacheHelperSymbol = firstPerCodeCacheHelperSymbol + TR_numCCPreLoadedCode - 1,

      lastCommonNonhelperSymbol
      };

  CommonNonhelperSymbol getLastCommonNonhelperSymbol()
      {
      return lastCommonNonhelperSymbol;
      }

   /**
    * Check whether the given symbol reference is the specified
    * "non-helper" symbol reference
    * @param[in] symRef the symbol reference to check
    * @param[in] nonHelper the non-helper symbol to check
    * @returns `true` if symRef is the specified non-helper symbol;
    * `false` otherwise
    */
   bool isNonHelper(TR::SymbolReference *symRef, CommonNonhelperSymbol nonHelper);

   /**
    * Check whether the given reference number is the specified
    * "non-helper" symbol.
    * @param[in] ref the reference number to check
    * @param[in] nonHelper the non-helper symbol to check
    * @returns `true` if ref is the specified non-helper symbol;
    * `false` otherwise
    */
   bool isNonHelper(int32_t ref, CommonNonhelperSymbol nonHelper);

   /**
    * Check whether the given symbol reference is a "non-helper" symbol.
    * @param[in] symRef the symbol reference to check
    * @returns `true` if symRef is a non-helper reference;
    * `false` otherwise
    */
   bool isNonHelper(TR::SymbolReference *symRef);

   /**
    * Check whether the given reference number is a "non-helper" symbol.
    * @param[in] ref the reference number to check
    * @returns `true` if ref is a non-helper reference;
    * `false` otherwise
    */
   bool isNonHelper(int32_t ref);

   /**
    * Retrieve the @ref CommonNonhelperSymbol for this symbol reference.
    * @param[in] symRef the symbol reference to check
    * @returns the @ref CommonNonhelperSymbol that this symbol reference
    * refers to or the value of getLastCommonNonhelperSymbol() if
    * the symbol reference does not refer to a non-helper
    */
   CommonNonhelperSymbol getNonHelperSymbol(TR::SymbolReference *symRef);

   /**
    * Retrieve the `CommonNonhelperSymbol` for this reference number.
    * @param[in] ref the reference number to check
    * @returns the @ref CommonNonhelperSymbol that this symbol reference
    * refers to or the value of getLastCommonNonhelperSymbol() if
    * the symbol reference does not refer to a non-helper
    */
   CommonNonhelperSymbol getNonHelperSymbol(int32_t ref);


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
   int32_t getArrayletShadowIndex(TR::DataType t) { return _numHelperSymbols + firstArrayletShadowSymbol + t; }

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
   TR::SymbolReference * findOrCreatePotentialOSRPointHelperSymbolRef();
   TR::SymbolReference * findOrCreateOSRFearPointHelperSymbolRef();
   TR::SymbolReference * findOrCreateInduceOSRSymbolRef(TR_RuntimeHelper induceOSRHelper);

   TR::ParameterSymbol * createParameterSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType, TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN);
   TR::SymbolReference * findOrCreateAutoSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType, bool isReference = true,
         bool isInternalPointer = false, bool reuseAuto = true, bool isAdjunct = false, size_t size = 0);
   TR::SymbolReference * createTemporary(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::DataType, bool isInternalPointer = false, size_t size = 0);
   TR::SymbolReference * createCoDependentTemporary(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::DataType, bool isInternalPointer, size_t size,
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
   TR::SymbolReference * createKnownStaticReferenceSymbolRef(void *address, TR::KnownObjectTable::Index knownObjectIndex=TR::KnownObjectTable::UNKNOWN);

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
   TR::SymbolReference * findOrCreateSymRefWithKnownObject(TR::SymbolReference *original, uintptrj_t *referenceLocation, bool isArrayWithConstantElements);
   TR::SymbolReference * findOrCreateSymRefWithKnownObject(TR::SymbolReference *original, TR::KnownObjectTable::Index objectIndex);
   /*
    * The public API that should be used when the caller needs a temp to hold a known object
    *
    * \note If there is a temp with the same known object already use the existing one. Otherwise, create a new temp.
    */
   TR::SymbolReference * findOrCreateTemporaryWithKnowObjectIndex(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::KnownObjectTable::Index knownObjectIndex);
   TR::SymbolReference * findOrCreateThisRangeExtensionSymRef(TR::ResolvedMethodSymbol *owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateContiguousArraySizeSymbolRef();
   TR::SymbolReference * findOrCreateNewArrayNoZeroInitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateNewObjectSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateNewObjectNoZeroInitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateArrayStoreExceptionSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateArrayShadowSymbolRef(TR::DataType, TR::Node * baseAddress = 0);
   TR::SymbolReference * findOrCreateImmutableArrayShadowSymbolRef(TR::DataType);
   TR::SymbolReference * createImmutableArrayShadowSymbolRef(TR::DataType, TR::Symbol *sym);
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
   bool                 isImmutableArrayShadow(TR::SymbolReference *symRef);

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
   /** \brief
    *    This function creates the symbol reference given a temp symbol and the known object index
    *
    *  \param symbol
    *    the temp symbol needed for creating the symbol reference
    *
    *  \note
    *    This function should only be called from functions inside symbol reference table when creating new autos or temps.
    *    Code outside symbol reference table should use the public API findOrCreateTemporaryWithKnowObjectIndex.
    */
   TR::SymbolReference * createTempSymRefWithKnownObject(TR::Symbol *symbol, mcount_t owningMethodIndex, int32_t slot, TR::KnownObjectTable::Index knownObjectIndex);

   /**\brief
    *
    * This is the lowest level of function to find the symbol reference of any type with known object index
    *
    * \param symbol
    *       For temp symbol reference, \p symbol can be NULL.
    *       For symbol reference type other than temp, an original symbol is needed to find its corresponding symbol reference.
    *       Take a static field with known object index for example, \p symbol is the original static field symbol.
    *
    * \param knownObjectIndex
    *
    */
   TR::SymbolReference * findSymRefWithKnownObject(TR::Symbol *symbol, TR::KnownObjectTable::Index knownObjectIndex);
   /*
    * For finding symbol reference with known object index for a temp
    */
   TR::SymbolReference * findTempSymRefWithKnownObject(TR::KnownObjectTable::Index knownObjectIndex);
   TR::SymbolReference * findOrCreateCPSymbol(TR::ResolvedMethodSymbol *, int32_t, TR::DataType, bool, void *, TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN);
   TR::SymbolReference * findOrCreateAutoSymbolImpl(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType, bool isReference = true,
         bool isInternalPointer = false, bool reuseAuto = true, bool isAdjunct = false, size_t size = 0, TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN);

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

   uint32_t                            _nextRegShadowIndex;
   int32_t                             _numUnresolvedSymbols;
   uint32_t                            _numHelperSymbols;
   uint32_t                            _numPredefinedSymbols;
   int32_t                             _numInternalPointers;
   bool                                _hasImmutable;
   bool                                _hasUserField;

   size_t                              _size_hint;

   typedef CS2::CompoundHashKey<mcount_t, const char *> OwningMethodAndString;
   typedef CS2::HashTable<OwningMethodAndString, TR::SymbolReference *, TR::Allocator> SymrefsByOwningMethodAndString;

   SymrefsByOwningMethodAndString      _methodsBySignature;

   // Aliasmap is keyed by a symbol reference's reference number
   typedef TR::typed_allocator<std::pair<int32_t const, TR_BitVector * >, TR::Allocator> AliasMapAllocator;
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
