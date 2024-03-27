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

#include "compile/SymbolReferenceTable.hpp"
#include "compile/Compilation.hpp"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "omrformatconsts.h"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "env/KnownObjectTable.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "cs2/hashtab.h"
#include "env/ClassEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGen.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "runtime/RuntimeAssumptions.hpp"
#include "env/CHTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#endif

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;


OMR::SymbolReferenceTable::SymbolReferenceTable(size_t sizeHint, TR::Compilation *comp) :
     baseArray(comp->trMemory(), static_cast<uint32_t>(sizeHint + TR_numRuntimeHelpers)),
     aliasBuilder(self(), sizeHint, comp),
     _trMemory(comp->trMemory()),
     _fe(comp->fe()),
     _compilation(comp),
     _genericIntShadowSymbol(0),
     _constantAreaSymbol(0),
     _constantAreaSymbolReference(0),
     _numUnresolvedSymbols(0),
     _numInternalPointers(0),
     _ObjectNewInstanceImplSymRef(0),
     _knownObjectSymrefsByObjectIndex(comp->trMemory()),
     _unsafeSymRefs(0),
     _unsafeVolatileSymRefs(0),
     _availableAutos(comp->trMemory()),
     _vtableEntrySymbolRefs(comp->trMemory()),
     _classLoaderSymbolRefs(comp->trMemory()),
     _classStaticsSymbolRefs(comp->trMemory()),
     _debugCounterSymbolRefs(comp->trMemory()),
     _methodsBySignature(8, comp->allocator("SymRefTab")), // TODO: Determine a suitable default size
     _hasImmutable(false),
     _hasUserField(false),
     _sharedAliasMap(NULL),
     _originalUnimprovedSymRefs(std::less<int32_t>(), comp->allocator())
   {
   _numHelperSymbols = TR_numRuntimeHelpers + 1;;

   _numPredefinedSymbols = _numHelperSymbols + getLastCommonNonhelperSymbol();

   baseArray.setSize(_numPredefinedSymbols);

   for (uint32_t i = 0; i < _numPredefinedSymbols; i++)
      baseArray.element(i) = 0;
   _size_hint = sizeHint;

   }


TR::SymbolReferenceTable *
OMR::SymbolReferenceTable::self()
   {
   return static_cast<TR::SymbolReferenceTable *>(this);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateContiguousArraySizeSymbolRef()
   {
   if (!element(contiguousArraySizeSymbol))
      {
      TR::Symbol * sym;

      // Size field is 32-bits on ALL header shapes.
      //
      sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(contiguousArraySizeSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), contiguousArraySizeSymbol, sym);
      element(contiguousArraySizeSymbol)->setOffset(fe()->getOffsetOfContiguousArraySizeField());
      }
   return element(contiguousArraySizeSymbol);
   }

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateContiguousArrayDataAddrFieldShadowSymRef()
   {
   if (!element(contiguousArrayDataAddrFieldSymbol))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      sym->setContiguousArrayDataAddrFieldSymbol();
      element(contiguousArrayDataAddrFieldSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), contiguousArrayDataAddrFieldSymbol, sym);
      element(contiguousArrayDataAddrFieldSymbol)->setOffset(TR::Compiler->om.offsetOfContiguousDataAddrField());
      }
   return element(contiguousArrayDataAddrFieldSymbol);
   }
#endif // defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)

TR::SymbolReference *
OMR::SymbolReferenceTable::findContiguousArrayDataAddrFieldShadowSymRef()
   {
   return element(contiguousArrayDataAddrFieldSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateVftSymbolRef()
   {
   if (!element(vftSymbol))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      sym->setClassObject();
      if (!TR::Compiler->cls.classObjectsMayBeCollected())
         sym->setNotCollected();

      element(vftSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), vftSymbol, sym);
      element(vftSymbol)->setOffset(TR::Compiler->om.offsetOfObjectVftField());
      }
   return element(vftSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findVftSymbolRef()
   {
   return element(vftSymbol);
   }


// Denotes the address of the ramMethod in the vtable.
//
TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateVtableEntrySymbolRef(TR::ResolvedMethodSymbol *calleeSymbol, int32_t vtableSlot)
   {
   mcount_t index = calleeSymbol->getResolvedMethodIndex();
   ListIterator<TR::SymbolReference> i(&_vtableEntrySymbolRefs);
   TR::SymbolReference * symRef;
   for (symRef = i.getFirst(); symRef; symRef = i.getNext())
      if ((symRef->getOffset() == vtableSlot))
         return symRef;

   TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);

   if (!TR::Compiler->cls.classObjectsMayBeCollected())
      sym->setNotCollected();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, vtableSlot);
   symRef->setOwningMethodIndex(index);
   _vtableEntrySymbolRefs.add(symRef);

   return symRef;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateIndexableSizeSymbolRef()
   {
   if (!element(indexableSizeSymbol))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(indexableSizeSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), indexableSizeSymbol, sym);
      element(indexableSizeSymbol)->setOffset(fe()->getOffsetOfIndexableSizeField());
      }
   return element(indexableSizeSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findArrayClassRomPtrSymbolRef()
   {
   return element(arrayClassRomPtrSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findClassRomPtrSymbolRef()
   {
   return element(classRomPtrSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateThisRangeExtensionSymRef(TR::ResolvedMethodSymbol *owningMethodSymbol)
   {
   if (!element(thisRangeExtensionSymbol))
      {
      TR::SymbolReference *symRef = createKnownStaticDataSymbolRef(0, TR::Address);
      element(thisRangeExtensionSymbol) = symRef;
      }
   return element(thisRangeExtensionSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findThisRangeExtensionSymRef(TR::ResolvedMethodSymbol *owningMethodSymbol)
   {
   return element(thisRangeExtensionSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findInstanceShapeSymbolRef()
{
   return element(instanceShapeSymbol);
}


TR::SymbolReference *
OMR::SymbolReferenceTable::findInstanceDescriptionSymbolRef()
{
   return element(instanceDescriptionSymbol);
}


TR::SymbolReference *
OMR::SymbolReferenceTable::findDescriptionWordFromPtrSymbolRef()
{
   return element(descriptionWordFromPtrSymbol);
}


TR::SymbolReference *
OMR::SymbolReferenceTable::findJavaLangClassFromClassSymbolRef()
   {
   return element(javaLangClassFromClassSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findClassFromJavaLangClassSymbolRef()
   {
   return element(classFromJavaLangClassSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findAddressOfClassOfMethodSymbolRef()
   {
   return element(addressOfClassOfMethodSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findClassIsArraySymbolRef()
   {
   return element(isArraySymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findClassAndDepthFlagsSymbolRef()
   {
   return element(isClassAndDepthFlagsSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findClassFlagsSymbolRef()
   {
   return element(isClassFlagsSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findArrayComponentTypeSymbolRef()
   {
   return element(componentClassSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateExcpSymbolRef()
   {
   if (!element(excpSymbol))
      {
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "ExceptionMeta");
      sym->setDataType(TR::Address);
      element(excpSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), excpSymbol, sym);
      element(excpSymbol)->setOffset(TR::Compiler->vm.thisThreadGetPendingExceptionOffset());

      // We can't let the load/store of the exception symbol swing down
      //
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(excpSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(excpSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateOSRReturnAddressSymbolRef()
   {
   if (!element(osrReturnAddressSymbol))
      {
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "osrReturnAddress");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(osrReturnAddressSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), osrReturnAddressSymbol, sym);
      element(osrReturnAddressSymbol)->setOffset(TR::Compiler->vm.thisThreadGetOSRReturnAddressOffset(comp()));

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(osrReturnAddressSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(osrReturnAddressSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateInduceOSRSymbolRef(TR_RuntimeHelper induceOSRHelper)
   {
   TR_ASSERT(induceOSRHelper == TR_induceOSRAtCurrentPC || induceOSRHelper == TR_induceOSRAtCurrentPCAndRecompile, "unexpected helper index when creating induce OSR helper symbol reference");
   TR::SymbolReference *induceOSRSymRef = findOrCreateRuntimeHelper(induceOSRHelper,
                                                                    true /* canGCandReturn */,
                                                                    true /* canGCandExcept */,
                                                                    true /* preservesAllRegisters */);
   // treat jitInduceOSR like an interpreted call so that each platform's codegen generates a snippet for it
   induceOSRSymRef->getSymbol()->getMethodSymbol()->setInterpreted();
   return induceOSRSymRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayletShadowSymbolRef(TR::DataType type)
   {
   int32_t index = getArrayletShadowIndex(TR::Address);
   if (!baseArray.element(index))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),TR::Address);
      sym->setArrayletShadowSymbol();
      sym->setNotCollected();
      TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(self(), index, sym);
      baseArray.element(index) = symRef;
      symRef->setCanGCandReturn();
      baseArray.element(index)->setCPIndex(-1);
      aliasBuilder.arrayletElementSymRefs().set(index);
      aliasBuilder.gcSafePointSymRefNumbers().set(index);
      }

   return baseArray.element(index);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayShadowSymbolRef(TR::DataType type, TR::Node * baseArrayAddress)
   {
   int32_t index = getArrayShadowIndex(type);
   if (!baseArray.element(index))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),type);
      sym->setArrayShadowSymbol();
      baseArray.element(index) = new (trHeapMemory()) TR::SymbolReference(self(), index, sym);
      baseArray.element(index)->setCPIndex(-1);
      aliasBuilder.arrayElementSymRefs().set(index);
      }

   return baseArray.element(index);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayShadowSymbolRef(TR::DataType type, TR::Node * baseArrayAddress, int32_t size, TR_FrontEnd * fe)
   {

   int32_t index = getArrayShadowIndex(type);
   if (!baseArray.element(index))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),type, size);
      sym->setArrayShadowSymbol();
      baseArray.element(index) = new (trHeapMemory()) TR::SymbolReference(self(), index, sym);
      baseArray.element(index)->setCPIndex(-1);
      baseArray.element(index)->setReallySharesSymbol();
      aliasBuilder.arrayElementSymRefs().set(index);
      }

   return baseArray.element(index);

   }

bool OMR::SymbolReferenceTable::isImmutableArrayShadow(TR::SymbolReference *symRef)
   {
   int32_t index = symRef->getReferenceNumber();
   return aliasBuilder.immutableArrayElementSymRefs().isSet(index);
   }

/**
 * \brief
 *    Find or create a SymbolReference for access to array with given element type
 * \parm
 *    elementType The element type of the array
 *
 * \return
 *    A symbol reference for the ImmutableArrayShadow
 */
TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateImmutableArrayShadowSymbolRef(TR::DataType elementType)
   {
   TR_BitVectorIterator bvi(aliasBuilder.immutableArrayElementSymRefs());
   while (bvi.hasMoreElements())
      {
      TR::SymbolReference *symRef = getSymRef(bvi.getNextElement());
      // A immutable array shadow with known object index is for array shadow from a known array,
      // and thus cannot be shared with other array shadows
      if (symRef->getSymbol()->getDataType() == elementType && !symRef->hasKnownObjectIndex())
         return symRef;
      }

   // Instead of creating a regular array shadow and waiting for VP to improve it to ImmutableArrayShadow,
   // there are occassions we want to create one directly, e.g. when we're generating array shadow for a
   // constant array with constant element. In this case, a regular array shadow might not exist
   TR::SymbolReference * symRef = findOrCreateArrayShadowSymbolRef(elementType);
   symRef->setReallySharesSymbol();

   TR::SymbolReference *newRef = new (trHeapMemory()) TR::SymbolReference(self(), (TR::Symbol *) symRef->getSymbol());
   newRef->setReallySharesSymbol();
   newRef->setCPIndex(-1);

   int32_t index = newRef->getReferenceNumber();
   aliasBuilder.arrayElementSymRefs().set(index); // this ensures the newly created shadow can still be aliased to all other array shadows
   aliasBuilder.immutableArrayElementSymRefs().set(index);

   return newRef;
   }

bool OMR::SymbolReferenceTable::isRefinedArrayShadow(TR::SymbolReference *symRef)
   {
   int32_t index = symRef->getReferenceNumber();
   return aliasBuilder.refinedNonIntPrimitiveArrayShadows().isSet(index) ||
          aliasBuilder.refinedIntArrayShadows().isSet(index) ||
          aliasBuilder.refinedAddressArrayShadows().isSet(index);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::createRefinedArrayShadowSymbolRef(TR::DataType type)
   {
   TR::SymbolReference * symRef = getSymRef(getArrayShadowIndex(type));
   symRef->setReallySharesSymbol();

   return createRefinedArrayShadowSymbolRef(type, (TR::Symbol *) symRef->getSymbol(), symRef);
   }



//TODO: to be changed to a special sym ref to be used by intrinsics
TR::SymbolReference *
OMR::SymbolReferenceTable::createRefinedArrayShadowSymbolRef(
   TR::DataType type,
   TR::Symbol *sym,
   TR::SymbolReference *original)
   {
   const bool trace=false;
   sym->setArrayShadowSymbol();

   TR::SymbolReference *newRef = new (trHeapMemory()) TR::SymbolReference(self(), sym);
   newRef->setReallySharesSymbol();

   int32_t index = newRef->getReferenceNumber();
   newRef->setCPIndex(-1);
   aliasBuilder.arrayElementSymRefs().set(index); // this ensures the newly created shadow can still be aliased to all other array shadows

   comp()->getMethodSymbol()->setHasVeryRefinedAliasSets(true);
   switch(type)
      {
      case TR::Address:
         aliasBuilder.refinedAddressArrayShadows().set(index);
         aliasBuilder.addressShadowSymRefs().set(index);
         break;
      case TR::Int32:
         aliasBuilder.refinedIntArrayShadows().set(index);
         aliasBuilder.intShadowSymRefs().set(index);
         break;
      default:
         aliasBuilder.refinedNonIntPrimitiveArrayShadows().set(index);
         aliasBuilder.nonIntPrimitiveShadowSymRefs().set(index);
         break;
      }
   if(trace)
      {
      traceMsg(comp(),"Created new array shadow %d\nRefinedAddress shadows:",index);
      aliasBuilder.refinedAddressArrayShadows().print(comp());
      traceMsg(comp(),"\nRefined Int Array shadows:");
      aliasBuilder.refinedIntArrayShadows().print(comp());
      traceMsg(comp(),"\nRefined non int shadows:");
      aliasBuilder.refinedNonIntPrimitiveArrayShadows().print(comp());
      traceMsg(comp(),"\n");
      }

   rememberOriginalUnimprovedSymRef(newRef, original);
   return newRef;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateRecompilationCounterSymbolRef(void *counterAddress)
   {
   if (!element(recompilationCounterSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Int32);
      sym->setStaticAddress(counterAddress);
      sym->setRecompilationCounter();
      sym->setNotDataAddress();
      element(recompilationCounterSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), recompilationCounterSymbol, sym);
      }
   return element(recompilationCounterSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayCopySymbol()
   {
   if (!element(arrayCopySymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(arrayCopySymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arrayCopySymbol, sym);
      }
   return element(arrayCopySymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArraySetSymbol()
   {
   if (!element(arraySetSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(arraySetSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arraySetSymbol, sym);
      }
   return element(arraySetSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreatePrefetchSymbol()
   {
   if (!element(prefetchSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(prefetchSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), prefetchSymbol, sym);
      }
   return element(prefetchSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayTranslateSymbol()
   {
   if (!element(arrayTranslateSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(arrayTranslateSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arrayTranslateSymbol, sym);
      }
   return element(arrayTranslateSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayTranslateAndTestSymbol()
   {
   if (!element(arrayTranslateAndTestSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(arrayTranslateAndTestSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arrayTranslateAndTestSymbol, sym);
      }
   return element(arrayTranslateAndTestSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreatelong2StringSymbol()
   {
   if (!element(long2StringSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(long2StringSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), long2StringSymbol, sym);
      }
   return element(long2StringSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreatebitOpMemSymbol()
   {
   if (!element(bitOpMemSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(bitOpMemSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), bitOpMemSymbol, sym);
      }
   return element(bitOpMemSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateSinglePrecisionSQRTSymbol()
   {
   if (!element(singlePrecisionSQRTSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(singlePrecisionSQRTSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), singlePrecisionSQRTSymbol, sym);
      }
   return element(singlePrecisionSQRTSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayCmpSymbol()
   {
   if (!element(arrayCmpSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(arrayCmpSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arrayCmpSymbol, sym);
      }
   return element(arrayCmpSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayCmpLenSymbol()
   {
   if (!element(arrayCmpLenSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(arrayCmpLenSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arrayCmpLenSymbol, sym);
      }
   return element(arrayCmpLenSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateCurrentTimeMaxPrecisionSymbol()
   {
   if (!element(currentTimeMaxPrecisionSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_Helper);
      sym->setHelper();

      element(currentTimeMaxPrecisionSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), currentTimeMaxPrecisionSymbol, sym);
      }
   return element(currentTimeMaxPrecisionSymbol);
   }



TR::SymbolReference *
OMR::SymbolReferenceTable::createKnownStaticDataSymbolRef(void *dataAddress, TR::DataType type)
   {
   TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),type);
   sym->setStaticAddress(dataAddress);
   sym->setNotCollected();
   return new (trHeapMemory()) TR::SymbolReference(self(), sym);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createKnownStaticDataSymbolRef(void *dataAddress, TR::DataType type, TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),type);
   sym->setStaticAddress(dataAddress);
   sym->setNotCollected();
   return TR::SymbolReference::create(self(), sym, knownObjectIndex);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createKnownStaticReferenceSymbolRef(void *dataAddress, TR::KnownObjectTable::Index knownObjectIndex)
   {
   const char *name = "<known-static-reference>";
   if (knownObjectIndex != TR::KnownObjectTable::UNKNOWN)
      {
      char *nameBuffer = (char *)trMemory()->allocateMemory(25, heapAlloc);
      sprintf(nameBuffer, "<known-obj%d>", knownObjectIndex);
      name = nameBuffer;
      }
   TR::StaticSymbol * sym = TR::StaticSymbol::createNamed(trHeapMemory(), TR::Address, dataAddress,name);
   return TR::SymbolReference::create(self(), sym, knownObjectIndex);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::createIsOverriddenSymbolRef(TR::ResolvedMethodSymbol * calleeSymbol)
   {
   mcount_t index = calleeSymbol->getResolvedMethodIndex();
   TR::SymbolReference * symRef;

   TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
   sym->setStaticAddress(calleeSymbol->getResolvedMethod()->addressContainingIsOverriddenBit());
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, index, -1);

   aliasBuilder.addressStaticSymRefs().set(symRef->getReferenceNumber()); // add the symRef to the statics list to get correct aliasing info
   symRef->setOverriddenBitAddress();
   return symRef;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::createRuntimeHelper(TR_RuntimeHelper index,
                                               bool             canGCandReturn,
                                               bool             canGCandExcept,
                                               bool             preservesAllRegisters)
   {
   TR::MethodSymbol * methodSymbol = TR::MethodSymbol::create(trHeapMemory(),runtimeHelperLinkage(index));
   methodSymbol->setHelper();
   methodSymbol->setMethodAddress(runtimeHelperValue(index));

   if (preservesAllRegisters && !debug("noPreserveHelperRegisters"))
      methodSymbol->setPreservesAllRegisters();

   TR::SymbolReference * sr = baseArray.element(index) = new (trHeapMemory()) TR::SymbolReference(self(), index, methodSymbol);

   if (canGCandReturn)
      sr->setCanGCandReturn();
   if (canGCandExcept)
      sr->setCanGCandExcept();

   return sr;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateRuntimeHelper(TR_RuntimeHelper index,
                                                     bool             canGCandReturn,
                                                     bool             canGCandExcept,
                                                     bool             preservesAllRegisters)
   {

   TR::SymbolReference * symRef = baseArray.element(index);
   if (!symRef)
      symRef = createRuntimeHelper(index, canGCandReturn, canGCandExcept, preservesAllRegisters);
   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateCodeGenInlinedHelper(CommonNonhelperSymbol index)
   {
   if (!element(index))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_None);
      sym->setHelper();
      sym->setIsInlinedByCG();
      element(index) = new (trHeapMemory()) TR::SymbolReference(self(), index, sym);
      }
   return element(index);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateNewObjectSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_newObject, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateNewObjectNoZeroInitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_newObjectNoZeroInit, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateNewValueSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_newValue, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateNewValueNoZeroInitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_newValueNoZeroInit, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateNewArraySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_newArray, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateNewArrayNoZeroInitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_newArrayNoZeroInit, true, true, true);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateANewArrayNoZeroInitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_aNewArrayNoZeroInit, true, true, true);
   }



TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateInstanceOfSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_instanceOf, false, false, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateVolatileReadLongSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_volatileReadLong, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateVolatileWriteLongSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_volatileWriteLong, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateVolatileReadDoubleSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_volatileReadDouble, true, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateVolatileWriteDoubleSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_volatileWriteDouble, true, true, true);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateCheckCastSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_checkCast, false, true, true);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateAThrowSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_aThrow, false, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateNullCheckSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_nullCheck, false, true, true);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateResolveCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   if (!element(resolveCheckSymbol))
      {
      // Resolve check symbol will just re-use the helper method for null check
      // It is never used to call a helper, but we need a separate symbol
      // reference for aliasing purposes.
      //
      TR::SymbolReference *symRef = findOrCreateNullCheckSymbolRef(owningMethodSymbol);
      element(resolveCheckSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), resolveCheckSymbol, symRef->getSymbol());
      }
   return element(resolveCheckSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayBoundsCheckSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_arrayBoundsCheck, false, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateDivCheckSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_divCheck, false, true, true);
   }

/*
 * The overflowCheck symbol reference is for use only aliasing set in OMR::SymbolReference::getUseonlyAliasesBV.
 * we want to make sure defs are not moved across the overflowCHK in case an exception is thrown and the
 * catch block might get stale values.
 */
TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateOverflowCheckSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_overflowCheck, false, true, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateArrayStoreExceptionSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_arrayStoreException, false, true, true);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateMonitorEntrySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_monitorEntry, true, false, true);
   }

/**
 * Finds or creates a symbol reference of a method given its name.
 *
 * @param owningMethodSymbol The symbol of the owning method.
 * @param className The enclosing class name of the method.
 * @param methodName The name of the method.
 * @param methodSignature The signature of the method.
 * @param kind The kind of method.
 * @param cpIndex The constant pool index of the method.
 *
 * @return The symbol reference of the method.
 */

TR::SymbolReference *
OMR::SymbolReferenceTable::methodSymRefFromName(TR::ResolvedMethodSymbol * owningMethodSymbol, const char *className, const char *methodName, const char *methodSignature, TR::MethodSymbol::Kinds kind, int32_t cpIndex)
   {
   // Check _methodsBySignature to see if we've already created a symref for this one
   //
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   auto fullSignatureLength = strlen(className) + 1 + strlen(methodName) + strlen(methodSignature);
   char *fullSignature = (char*)trMemory()->allocateMemory(1 + fullSignatureLength, stackAlloc);
   sprintf(fullSignature, "%s.%s%s", className, methodName, methodSignature);
   TR_ASSERT(strlen(fullSignature) == fullSignatureLength, "Computed fullSignatureLength must match actual length of fullSignature");
   CS2::HashIndex hashIndex = 0;
   static char *ignoreMBSCache = feGetEnv("TR_ignoreMBSCache");
   OwningMethodAndString key(owningMethodSymbol->getResolvedMethodIndex(), fullSignature);
   if (_methodsBySignature.Locate(key, hashIndex) && !ignoreMBSCache)
      {
      TR::SymbolReference *result = _methodsBySignature[hashIndex];
      if (comp()->getOption(TR_TraceMethodIndex))
         traceMsg(comp(), "-- MBS cache hit (1): M%p\n", result->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod());
      return result;
      }
   else
      {
      // fullSignature will be kept as a key by _methodsBySignature, so it needs heapAlloc
      //
      key = OwningMethodAndString(owningMethodSymbol->getResolvedMethodIndex(), self()->strdup(fullSignature));
      if (comp()->getOption(TR_TraceMethodIndex))
         traceMsg(comp(), "-- MBS cache miss (1) owning method #%d, signature %s\n", owningMethodSymbol->getResolvedMethodIndex().value(), fullSignature);
      }

   //
   // No existing symref.  Create a new one.
   //

   TR_OpaqueMethodBlock *method = fe()->getMethodFromName(className, methodName, methodSignature);

   // It is possible for getMethodFromName to return NULL in relocatable compilations
   if (!method && comp()->compileRelocatableCode())
      comp()->failCompilation<TR::CompilationException>("Failed to get method %s.%s%s from name", className, methodName, methodSignature);

   TR_ASSERT_FATAL(method, "methodSymRefFromName: method must exist: %s.%s%s", className, methodName, methodSignature);
   TR_ASSERT(kind != TR::MethodSymbol::Virtual, "methodSymRefFromName doesn't support virtual methods"); // Until we're able to look up vtable index

   // Note: we use cpIndex=-1 here so we don't end up getting back the original symref (which will not have the signature we want)
   TR::SymbolReference *result = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), -1,
      comp()->fe()->createResolvedMethod(comp()->trMemory(), method, owningMethodSymbol->getResolvedMethod()), kind);

   result->setCPIndex(cpIndex);
   _methodsBySignature.Add(key, result);
   TR_ASSERT(_methodsBySignature.Locate(key), "After _methodsBySignature.Add, _methodsBySignature.Locate must be true");

   return result;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateSymRefWithKnownObject(TR::SymbolReference *original, uintptr_t *referenceLocation)
   {
   return findOrCreateSymRefWithKnownObject(original, referenceLocation, false);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateSymRefWithKnownObject(TR::SymbolReference *original, uintptr_t *referenceLocation, bool isArrayWithConstantElements)
   {
   TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
   if (!knot)
      return original;

   TR::KnownObjectTable::Index objectIndex = knot->getOrCreateIndexAt(referenceLocation, isArrayWithConstantElements);
   return findOrCreateSymRefWithKnownObject(original, objectIndex);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findTempSymRefWithKnownObject(TR::KnownObjectTable::Index knownObjectIndex)
   {
   return findSymRefWithKnownObject(NULL, knownObjectIndex);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findSymRefWithKnownObject(TR::Symbol *symbol, TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR_BitVector *bucket = _knownObjectSymrefsByObjectIndex[knownObjectIndex];
   if (!bucket)
      return NULL;

   TR_BitVectorIterator bvi(*bucket);
   while (bvi.hasMoreElements())
      {
      TR::SymbolReference *symRef = getSymRef(bvi.getNextElement());
      if (symbol && symRef->getSymbol() == symbol)
         return symRef;
      if (!symbol && symRef->getSymbol()->isAuto()) //For temp, symbol can be NULL. Find any temp with the given knownObjectIndex.
         return symRef;
      }
   return NULL;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateSymRefWithKnownObject(TR::SymbolReference *originalSymRef, TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR::SymbolReference *symRef = findSymRefWithKnownObject(originalSymRef->getSymbol(), knownObjectIndex);
   if (symRef)
      return symRef;

   // Need a new one
   TR_BitVector *bucket = _knownObjectSymrefsByObjectIndex[knownObjectIndex];
   if (!bucket)
      {
      bucket = new (trHeapMemory()) TR_BitVector(baseArray.size(), trMemory(), heapAlloc, growable, TR_MemoryBase::SymbolReference);
      _knownObjectSymrefsByObjectIndex[knownObjectIndex] = bucket;
      }
   TR::SymbolReference *result = new (trHeapMemory()) TR::SymbolReference(self(), *originalSymRef, 0, knownObjectIndex);
   bucket->set(result->getReferenceNumber());

   // If the known object symref is created for an immutable array shadow, it should be aliased to other array shadows as well
   // Put the symRef in arrayElementSymRefs so that its alias set can be built correctly
   if (isImmutableArrayShadow(originalSymRef))
      {
      result->setReallySharesSymbol();
      int32_t index = result->getReferenceNumber();
      aliasBuilder.arrayElementSymRefs().set(index);
      aliasBuilder.immutableArrayElementSymRefs().set(index);
      }

   rememberOriginalUnimprovedSymRef(result, originalSymRef);
   return result;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createTempSymRefWithKnownObject(TR::Symbol *symbol, mcount_t owningMethodIndex, int32_t slot, TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR_ASSERT_FATAL(symbol->isAutoOrParm(), "createTempSymRefWithKnownObject can only be called for temp symbol %p", symbol);
   TR_BitVector *bucket = _knownObjectSymrefsByObjectIndex[knownObjectIndex];
   if (!bucket)
      {
      bucket = new (trHeapMemory()) TR_BitVector(baseArray.size(), trMemory(), heapAlloc, growable, TR_MemoryBase::SymbolReference);
      _knownObjectSymrefsByObjectIndex[knownObjectIndex] = bucket;
      }
   TR::SymbolReference * symRef = new (trHeapMemory()) TR::SymbolReference(self(), symbol, owningMethodIndex, slot, 0 /*unresolvedIndex*/, knownObjectIndex);
   bucket->set(symRef->getReferenceNumber());
   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateMonitorExitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_monitorExit, true /* canGCandReturn */, true /* canGCandExcept */, true /* preservesAllRegisters */);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateTransactionEntrySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_transactionEntry, true, false, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateTransactionAbortSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_transactionAbort, false, false, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateTransactionExitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_transactionExit, true, false, true);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateAsyncCheckSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_asyncCheck, true, false, true);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateReportMethodEnterSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_reportMethodEnter, true, false, true);
   }

bool OMR::SymbolReferenceTable::shouldMarkBlockAsCold(TR_ResolvedMethod * owningMethod, bool isUnresolvedInCP)
   {
   return false;
   }

void OMR::SymbolReferenceTable::markBlockAsCold()
   {
   comp()->getCurrentIlGenerator()->getCurrentBlock()->setIsCold();
   comp()->getCurrentIlGenerator()->getCurrentBlock()->setFrequency(UNRESOLVED_COLD_BLOCK_COUNT);
   }

char *
OMR::SymbolReferenceTable::strdup(const char *arg)
   {
   char *result = (char*)trMemory()->allocateMemory(1+strlen(arg), heapAlloc, TR_MemoryBase::SymbolReferenceTable);
   strcpy(result, arg);
   return result;
   }

TR::Symbol *
OMR::SymbolReferenceTable::findOrCreateGenericIntShadowSymbol()
   {
   if (!_genericIntShadowSymbol)
      _genericIntShadowSymbol = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);

   return _genericIntShadowSymbol;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateGenericIntShadowSymbolReference(intptr_t offset, bool allocateUseDefBitVector)
   {
   //FIXME: we should attempt to find it
   return createGenericIntShadowSymbolReference(offset, allocateUseDefBitVector);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createGenericIntShadowSymbolReference(intptr_t offset, bool allocateUseDefBitVector)
   {
   TR::SymbolReference * symRef = new (trHeapMemory()) TR::SymbolReference(self(), findOrCreateGenericIntShadowSymbol(), comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
   symRef->setOffset(offset);
   symRef->setReallySharesSymbol();
   aliasBuilder.intShadowSymRefs().set(symRef->getReferenceNumber());
   aliasBuilder.genericIntShadowSymRefs().set(symRef->getReferenceNumber());
   aliasBuilder.setMutableGenericIntShadowHasBeenCreated(true);

   if (allocateUseDefBitVector)
      symRef->setEmptyUseDefAliases(self());

   return symRef;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateGenericIntArrayShadowSymbolReference(intptr_t offset)
   {
   TR::SymbolReference * symRef = new (trHeapMemory()) TR::SymbolReference(self(), findOrCreateGenericIntShadowSymbol(), comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
   symRef->setOffset(offset);
   symRef->setReallySharesSymbol();
   aliasBuilder.intShadowSymRefs().set(symRef->getReferenceNumber());
   aliasBuilder.genericIntArrayShadowSymRefs().set(symRef->getReferenceNumber());
   aliasBuilder.setMutableGenericIntShadowHasBeenCreated(true);
   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateGenericIntNonArrayShadowSymbolReference(intptr_t offset)
   {
   TR::SymbolReference * symRef = new (trHeapMemory()) TR::SymbolReference(self(), findOrCreateGenericIntShadowSymbol(), comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
   symRef->setOffset(offset);
   symRef->setReallySharesSymbol();
   aliasBuilder.intShadowSymRefs().set(symRef->getReferenceNumber());
   aliasBuilder.genericIntNonArrayShadowSymRefs().set(symRef->getReferenceNumber());
   aliasBuilder.setMutableGenericIntShadowHasBeenCreated(true);
   return symRef;
   }


TR::Symbol *
OMR::SymbolReferenceTable::findOrCreateConstantAreaSymbol()
   {
   if (!_constantAreaSymbol)
      {
      char * symName = (char *)TR_MemoryBase::jitPersistentAlloc(strlen("CONSTANT_AREA") + 1);
      sprintf(symName, "CONSTANT_AREA");
      _constantAreaSymbol = TR::StaticSymbol::createNamed(comp()->trHeapMemory(), TR::NoType, symName);
      }
   return _constantAreaSymbol;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateConstantAreaSymbolReference()
   {
   if (!_constantAreaSymbolReference)
      {
      _constantAreaSymbolReference = new (comp()->trHeapMemory()) TR::SymbolReference(self(), findOrCreateConstantAreaSymbol());
      _constantAreaSymbolReference->setFromLiteralPool();
      }
   return _constantAreaSymbolReference;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateGCRPatchPointSymbolRef()
   {
   if (!element(gcrPatchPointSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Int8); // Is this 32 bit?
      sym->setStaticAddress(0);
      sym->setGCRPatchPoint(); // set the flag
      sym->setNotDataAddress();
      sym->setStaticAddressWithinMethodBounds();
      element(gcrPatchPointSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), gcrPatchPointSymbol, sym);
      }
   return element(gcrPatchPointSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateStartPCSymbolRef()
   {
   if (!element(startPCSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Int32);
      sym->setStaticAddress(0);
      sym->setStartPC();
      sym->setNotDataAddress();
      sym->setStaticAddressWithinMethodBounds();
      element(startPCSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), startPCSymbol, sym);
      }
   return element(startPCSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateCounterAddressSymbolRef()
   {
   if (!element(counterAddressSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);

#ifdef J9_PROJECT_SPECIFIC
#ifdef TR_HOST_S390
      sym->setStaticAddress(comp()->getRecompilationInfo()->getCounterAddress());
#else
      sym->setStaticAddress(0);
#endif
#else
      sym->setStaticAddress(0);
#endif
      sym->setNotCollected();
      sym->setRecompilationCounter();
      sym->setNotDataAddress();
      element(counterAddressSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), counterAddressSymbol, sym);
      }
   return element(counterAddressSymbol);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findStaticSymbol(TR_ResolvedMethod * owningMethod, int32_t cpIndex, TR::DataType type)
   {
   TR::SymbolReference * symRef;
   TR_SymRefIterator i(type == TR::Address ? aliasBuilder.addressStaticSymRefs() :
                                            (type == TR::Int32 ? aliasBuilder.intStaticSymRefs() : aliasBuilder.nonIntPrimitiveStaticSymRefs()), self());
   while ((symRef = i.getNext()))
      {
      if (symRef->getSymbol()->getDataType() == type &&
          symRef->getCPIndex() != -1 &&
            TR::Compiler->cls.jitStaticsAreSame(comp(), owningMethod, cpIndex,
               symRef->getOwningMethod(comp()), symRef->getCPIndex()))
         return symRef;
      }
   return 0;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateClassSymbol(
   TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, void * classObject, bool cpIndexOfStatic)
   {
   // Class symbols are created for explicit references to classes, they are also created for stores to statics.
   // In the latter case a load is created for the class the static is defined in to be used by the write barrier code.
   // For explicit references to classes the cpIndex passed into this routine refers to a class.  For the implicit
   // reference resulting from a store to a static the cpIndex passed into this routine is for the static.
   //
   // If the cpindex stored in the symref correspond to a static then we set the 'addressIsCPIndexOfStatic' bit.
   // This bit is used for unresolved references to call a specific resolve helper that understand that the cpIndex
   // corresponds to a static.   The bit is also used in getClassNameChars for resolved and unresolved symrefs.
   //
   // findOrCreateCPSymbol will reuse an existing resolved symbol reference if a previously created symbol references contains
   // the same resolved address.  As a result we can end up sharing symbol references between an explicit reference to the class
   // and an implicit reference used on a write barrier node.   In such a case we make sure that the shared symref uses the
   // cpindex for the explicit reference and has the 'addressIsCPIndexOfStatic' set to false.
   //
   TR::SymbolReference * symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, classObject != 0, classObject);
   TR::StaticSymbol * sym = symRef->getSymbol()->castToStaticSymbol();
   sym->setClassObject();

#ifdef J9_PROJECT_SPECIFIC
   if (cpIndex == -1 && comp()->compileRelocatableCode() && !comp()->getOption(TR_UseSymbolValidationManager))
      {
      // Restricting TR_ArbitraryClassAddress to classes loaded by the
      // bootstrap loader ensures that there is no ambiguity when correlating
      // the class on AOT load.
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
      void *loader = fej9->getClassLoader((TR_OpaqueClassBlock*)classObject);
      void *bootstrapLoader = fej9->getSystemClassLoader();
      TR_ASSERT_FATAL(
         loader == bootstrapLoader,
         "class symref cpIndex=-1 in AOT not loaded by bootstrap loader\n");
      }
#endif

   if (cpIndexOfStatic)
      {
      if (symRef->getCPIndex() == cpIndex && symRef->getOwningMethodIndex() == owningMethodSymbol->getResolvedMethodIndex())
         sym->setAddressIsCPIndexOfStatic(true);
      }
   else if (sym->addressIsCPIndexOfStatic())
      {
      TR_ASSERT(symRef->getCPIndex() != cpIndex,
             "how can the cpIndices be the same if one symbol represents the class of a static and the other doesnt");
      symRef->setCPIndex(cpIndex);
      symRef->setOwningMethodIndex(owningMethodSymbol->getResolvedMethodIndex());
      sym->setAddressIsCPIndexOfStatic(false);
      }

   // do not mark classObjects as being notCollected, unless it is classesOnHeap.
   if (!TR::Compiler->cls.classObjectsMayBeCollected() && TR::Compiler->cls.classesOnHeap())
      sym->setNotCollected();

   return symRef;
   }



TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateCPSymbol(
   TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, TR::DataType dataType, bool resolved, void * dataAddress, TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR::StaticSymbol *sym;
   TR_SymRefIterator i(aliasBuilder.cpConstantSymRefs(), self());
   TR::SymbolReference * symRef = i.getNext();
   for (; symRef; symRef = i.getNext())
      {
      sym = (TR::StaticSymbol *) symRef->getSymbol();

      // TODO: Shouldn't it use staticsAreSame?

      if (owningMethodSymbol->getResolvedMethodIndex() == symRef->getOwningMethodIndex())
         {
         if (resolved)
            {
            if (!symRef->isUnresolved() && sym->getStaticAddress() == dataAddress)
               {
               // If this call gives us a cpIndex and we didn't have one before,
               // apply it to the symbol reference.
               //
               if (cpIndex > 0 && symRef->getCPIndex() <= 0)
                  symRef->setCPIndex(cpIndex);
               return symRef;
               }
            }
         else if (symRef->isUnresolved() && cpIndex == symRef->getCPIndex())
            return symRef;
         }
      }

   sym = TR::StaticSymbol::create(trHeapMemory(),dataType);
   int32_t unresolvedIndex = resolved ? 0 : _numUnresolvedSymbols++;

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), cpIndex, unresolvedIndex, knownObjectIndex);

   if (resolved)
      sym->setStaticAddress(dataAddress);
   else
      {
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   aliasBuilder.cpConstantSymRefs().set(symRef->getReferenceNumber());
   aliasBuilder.cpSymRefs().set(symRef->getReferenceNumber());

   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateComputedStaticMethodSymbol(
   mcount_t owningMethodIndex, int32_t cpIndex, TR_ResolvedMethod * resolvedMethod)
   {
   return findOrCreateMethodSymbol(owningMethodIndex, cpIndex, resolvedMethod, TR::MethodSymbol::ComputedStatic, false /*isUnresolvedInCP*/);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateStaticMethodSymbol(
   mcount_t owningMethodIndex, int32_t cpIndex, TR_ResolvedMethod * resolvedMethod)
   {
   return findOrCreateMethodSymbol(owningMethodIndex, cpIndex, resolvedMethod, TR::MethodSymbol::Static, false /*isUnresolvedInCP*/);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateMethodSymbol(
   mcount_t owningMethodIndex, int32_t cpIndex, TR_ResolvedMethod * resolvedMethod, TR::MethodSymbol::Kinds callKind, bool isUnresolvedInCP)
   {
   TR::SymbolReference * symRef;
   // We check comp()->getHasMethodHandleInvoke() here only because owningMethodDoesntMatter() could be expensive, and currently only JSR292 uses this facility
   if (comp()->getHasMethodHandleInvoke() && resolvedMethod && resolvedMethod->owningMethodDoesntMatter()) // Any symbol with the same TR_OpaqueMethodBlock will do
      {
      TR_SymRefIterator i(aliasBuilder.methodSymRefs(), self());
      for (symRef = i.getNext(); symRef; symRef = i.getNext())
         {
         TR::ResolvedMethodSymbol *existingMethod = symRef->getSymbol()->getResolvedMethodSymbol();
         if (existingMethod && (existingMethod->getMethodKind() == callKind) && resolvedMethod->canAlwaysShareSymbolDespiteOwningMethod(existingMethod->getResolvedMethod()))
            {
            if (performTransformation(comp(), "O^O SYMBOL SHARING: Reusing #%d M%p for M%p\n", symRef->getReferenceNumber(), existingMethod->getResolvedMethod(), resolvedMethod))
               {
               TR_ResolvedMethod * owningMethod = comp()->getOwningMethodSymbol(owningMethodIndex)->getResolvedMethod();
               symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP ? TR_no: TR_maybe);
               if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
                  markBlockAsCold();
               return symRef;
               }
            }
         }
      }
   else if (cpIndex != -1) // -1 is a indication that the symbol reference shouldn't be shared
      {
      TR_SymRefIterator i(aliasBuilder.methodSymRefs(), self());
      for (symRef = i.getNext(); symRef; symRef = i.getNext())
        {
        if  ( cpIndex == symRef->getCPIndex()
              && owningMethodIndex == symRef->getOwningMethodIndex()
              && cpIndex != -1
              && symRef->getSymbol()->getMethodSymbol()->getMethodKind() == callKind
              && !(resolvedMethod && (symRef->isUnresolved() || symRef->getSymbol()->isDummyResolvedMethod()))
            )
          return symRef;
        }
      }

   TR_ResolvedMethod * owningMethod = comp()->getOwningMethodSymbol(owningMethodIndex)->getResolvedMethod();

   TR::MethodSymbol * sym;
   int32_t unresolvedIndex = 0;
   bool canGCandReturn = true;
   bool canGCandExcept = true;
   if (resolvedMethod)
      {
      TR::ResolvedMethodSymbol *resSym = TR::ResolvedMethodSymbol::create(trHeapMemory(),resolvedMethod,comp());
      sym = resSym;
#ifdef J9_PROJECT_SPECIFIC
      canGCandReturn = !(resSym->canDirectNativeCall() || resSym->getRecognizedMethod()==TR::java_lang_System_arraycopy);
      canGCandExcept = !resSym->canDirectNativeCall();
#endif
      }
   else
      {
#ifdef J9_PROJECT_SPECIFIC
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      unresolvedIndex = _numUnresolvedSymbols++;
      sym = TR::MethodSymbol::create(trHeapMemory(),TR_Private,fej9->createMethod(trMemory(),owningMethod->containingClass(),cpIndex));
#else
      TR_ASSERT(0, "expecting a resolved method");
#endif
      }
   sym->setMethodKind(callKind);

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodIndex, cpIndex, unresolvedIndex);
   if (canGCandReturn)
      symRef->setCanGCandReturn();
   if (canGCandExcept)
      symRef->setCanGCandExcept();

   if (!resolvedMethod)
      symRef->setUnresolved();
   else if (callKind == TR::MethodSymbol::Virtual && cpIndex != -1)
      symRef->setOffset(resolvedMethod->virtualCallSelector(cpIndex));

   aliasBuilder.methodSymRefs().set(symRef->getReferenceNumber());

   symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP? TR_no : TR_maybe);

   if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
      markBlockAsCold();
   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateAutoSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType type, bool isReference, bool isInternalPointer, bool reuseAuto, bool isAdjunct, size_t size)
   {
   return findOrCreateAutoSymbolImpl(owningMethodSymbol, slot, type, isReference, isInternalPointer, reuseAuto, isAdjunct, size, TR::KnownObjectTable::UNKNOWN);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateAutoSymbolImpl(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType type, bool isReference, bool isInternalPointer, bool reuseAuto, bool isAdjunct, size_t size, TR::KnownObjectTable::Index knownObjectIndex)
   {
   mcount_t owningMethodIndex = owningMethodSymbol->getResolvedMethodIndex();
   int32_t numberOfParms = owningMethodSymbol->getNumParameterSlots();
   List<TR::SymbolReference> & list = slot < 0 ? owningMethodSymbol->getPendingPushSymRefs(-slot - 1) :
                                                owningMethodSymbol->getAutoSymRefs(slot);
   bool slotSharedbyRefAndNonRef = false;
   ListIterator<TR::SymbolReference> i(&list);
   TR::SymbolReference * symRef = i.getFirst();
   for (; symRef; symRef = i.getNext())
      if ((type == symRef->getSymbol()->getDataType() && (symRef->isAdjunct() == isAdjunct) &&
          (size == 0 || size == symRef->getSymbol()->getSize()))
              // standard match
          || (type == TR::Address && symRef->isDual() && symRef->isAdjunct() == isAdjunct))
              // dual symref match, when original type is an address, and needs to be coerced
         {
         if (slot < numberOfParms && isReference && symRef->getSymbol()->getParmSymbol())
            {
            symRef->getSymbol()->castToParmSymbol()->setReferencedParameter();
            }
         return symRef;
         }
      else if (comp()->getOption(TR_MimicInterpreterFrameShape) &&
               (type == TR::Address || symRef->getSymbol()->getDataType() == TR::Address))
         {
         if (symRef->getSymbol()->getParmSymbol() || comp()->getOption(TR_DontJitIfSlotsSharedByRefAndNonRef))
            {
            comp()->failCompilation<TR::CompilationException>("stack mapping can't handle a parameter slot that is shared between refs and nonrefs 0");
            }

         slotSharedbyRefAndNonRef = true;
         symRef->getSymbol()->setSlotSharedByRefAndNonRef(true); // An address slot is being shared with an int slot.
         comp()->setSlotsSharedByRefAndNonRef(true);
         }

      if (TR::Symbol::convertTypeToNumberOfSlots(type) == 2)
         {
         List<TR::SymbolReference> & list2 = slot < 0 ? owningMethodSymbol->getPendingPushSymRefs(-(slot-1) - 1) :
                                                              owningMethodSymbol->getAutoSymRefs(slot+1);

         ListIterator<TR::SymbolReference> i2(&list2);
         TR::SymbolReference * symRef2 = i2.getFirst();
         for (; symRef2; symRef2 = i2.getNext())
            {
            if (comp()->getOption(TR_MimicInterpreterFrameShape) &&
                (symRef2->getSymbol()->getDataType() == TR::Address))
               {
               if (symRef2->getSymbol()->getParmSymbol() || comp()->getOption(TR_DontJitIfSlotsSharedByRefAndNonRef))
                  {
                  comp()->failCompilation<TR::CompilationException>("stack mapping can't handle a parameter slot that is shared between refs and nonrefs 1");
                  }
               slotSharedbyRefAndNonRef = true;
               symRef2->getSymbol()->setSlotSharedByRefAndNonRef(true); // An address slot is being shared with an int slot.
               comp()->setSlotsSharedByRefAndNonRef(true);
               }
            }
         }

      if ((type == TR::Address) && (slot != -1) && (slot != 0))
         {
         List<TR::SymbolReference> & list2 = slot < 0 ? owningMethodSymbol->getPendingPushSymRefs(-(slot+1) - 1) :
                                                          owningMethodSymbol->getAutoSymRefs(slot-1);

         ListIterator<TR::SymbolReference> i2(&list2);
         TR::SymbolReference * symRef2 = i2.getFirst();
         for (; symRef2; symRef2 = i2.getNext())
            {
            if (comp()->getOption(TR_MimicInterpreterFrameShape) &&
                (TR::Symbol::convertTypeToNumberOfSlots(symRef2->getSymbol()->getDataType()) == 2))
               {
               if (symRef2->getSymbol()->getParmSymbol() || comp()->getOption(TR_DontJitIfSlotsSharedByRefAndNonRef))
                  {
                  comp()->failCompilation<TR::CompilationException>("stack mapping can't handle a parameter slot that is shared between refs and nonrefs 3");
                  }
               slotSharedbyRefAndNonRef = true;
               symRef2->getSymbol()->setSlotSharedByRefAndNonRef(true); // An address slot is being shared with an int slot.
               comp()->setSlotsSharedByRefAndNonRef(true);
               }
            }
         }

   // We need to return a symref whose slot number matches what was requested in the bytecodes (Walker code)
   // This routine does not consider slot number and so we are disabling it to avoid issues when we need to
   // recreate the stack accurately for the interpreter
   //
   if (comp()->getOption(TR_MimicInterpreterFrameShape) || comp()->getOption(TR_EnableOSR))
      reuseAuto = false;

   if (reuseAuto && !isInternalPointer)
      symRef = findAvailableAuto(type, true, isAdjunct);

   if (!symRef)
      {
      TR::AutomaticSymbol * sym = NULL;

      if (isInternalPointer)
         {
         sym = size ? TR::AutomaticSymbol::createInternalPointer(trHeapMemory(), type, static_cast<uint32_t>(size), comp()->fe()) :
                      TR::AutomaticSymbol::createInternalPointer(trHeapMemory(), type);
         _numInternalPointers++;
         if (_numInternalPointers > comp()->maxInternalPointers())
            {
            comp()->failCompilation<TR::ExcessiveComplexity>("Excessive number of internal pointers");
            }
         }
      else
         {
         sym = size ? TR::AutomaticSymbol::create(trHeapMemory(),type,static_cast<uint32_t>(size)) :
                      TR::AutomaticSymbol::create(trHeapMemory(),type);
         }

      sym->setSlotSharedByRefAndNonRef(slotSharedbyRefAndNonRef);
      if (comp()->getOption(TR_MimicInterpreterFrameShape)) // && comp()->getOption(TR_FSDInlining))
         {
         // negative implies PPS save temp. which we want to map on the end of the autos
         // in order that at decompilation time the stackwalker can copy the
         // auto and the saved PPS as one contiguous region.
         // The slot number of the PPS temp refers to the PPS slot it saves
         // whereas its GC index will be its place in the frame.
         //
         if(comp()->isOutermostMethod())        //perform as usual if this is the method we're compiling
            {
            if (slot < 0)
               sym->setGCMapIndex(-slot + owningMethodSymbol->getFirstJitTempIndex() - 1);
            else if (slot < owningMethodSymbol->getFirstJitTempIndex())
               sym->setGCMapIndex(slot);
            }
         else                                   //else we don't want to actually map symrefs to gc slots in fsd inlined methods
            sym->setGCMapIndex(-1);
         }

      if (knownObjectIndex == TR::KnownObjectTable::UNKNOWN)
         symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodIndex, slot);
      else
         symRef = createTempSymRefWithKnownObject(sym, owningMethodIndex, slot, knownObjectIndex);

      if (isAdjunct)
         symRef->setIsAdjunct();

      owningMethodSymbol->addAutomatic(sym);
      }

   // Get the list again, as it might have grown in size and been re-allocated.
   List<TR::SymbolReference> &list3 = slot < 0 ? owningMethodSymbol->getPendingPushSymRefs(-slot - 1) :
                                                owningMethodSymbol->getAutoSymRefs(slot);
   list3.add(symRef);
   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateShadowSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool isStore)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrFabricateShadowSymbol(TR_OpaqueClassBlock *containingClass, TR::DataType type, uint32_t offset, bool isVolatile, bool isPrivate, bool isFinal, const char * name, const char * signature)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findAvailableAuto(TR::DataType type, bool behavesLikeTemp, bool isAdjunct)
   {
   return findAvailableAuto(_availableAutos, type, behavesLikeTemp, isAdjunct);
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findAvailableAuto(List<TR::SymbolReference> & availableAutos,
                                           TR::DataType type, bool behavesLikeTemp, bool isAdjunct)
   {
   // Disable sharing of autos in IL gen now as autos compaction based on
   // liveness information has been implemented as a pass before codegen is
   // done at some higher hotness levels; however at lower opt levels we still
   // to compact early as there will be too many locals at lower opt levels.
   //
   if (!(comp()->cg()->getSupportsCompactedLocals() && comp()->getMethodHotness() >=hot))
      {
      static const char *notSharing = feGetEnv("TR_noShare");
      TR::SymbolReference * a = 0;
      ListElement<TR::SymbolReference> * prev;
      ListIterator<TR::SymbolReference> autos(&availableAutos);
      for (prev = 0, a = autos.getFirst(); a; prev = autos.getCurrentElement(), a = autos.getNext())
         if (type == a->getSymbol()->getDataType() &&
             !notSharing &&
             !a->getSymbol()->holdsMonitoredObject() &&
             !a->hasKnownObjectIndex() &&
             (a->isAdjunct() == isAdjunct) &&
             (comp()->cg()->getSupportsJavaFloatSemantics() ||
              (type != TR::Float && type != TR::Double) ||
              (a->isTemporary(comp()) && behavesLikeTemp == !a->getSymbol()->behaveLikeNonTemp())))
            {
            availableAutos.removeNext(prev);
            return a;
            }
      }

   return 0;
   }


void
OMR::SymbolReferenceTable::makeAutoAvailableForIlGen(TR::SymbolReference * a)
   {
   if (!a->getSymbol()->isNotCollected() &&
       !_availableAutos.find(a))
      _availableAutos.add(a);
   }


TR::ParameterSymbol *
OMR::SymbolReferenceTable::createParameterSymbol(
   TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType type, TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR::ParameterSymbol * sym = TR::ParameterSymbol::create(trHeapMemory(),type,slot);

   TR::SymbolReference *symRef = NULL;
   if (knownObjectIndex == TR::KnownObjectTable::UNKNOWN)
      symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), slot);
   else
      symRef = createTempSymRefWithKnownObject(sym, owningMethodSymbol->getResolvedMethodIndex(), slot, knownObjectIndex);

   owningMethodSymbol->setParmSymRef(slot, symRef);
   owningMethodSymbol->getAutoSymRefs(slot).add(symRef);

   return sym;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::createTemporary(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::DataType type, bool isInternalPointer, size_t size)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR_ASSERT(!type.isBCD() || size,"binary coded decimal types must provide a size\n");
#endif

   return findOrCreateAutoSymbol(owningMethodSymbol, owningMethodSymbol->incTempIndex(fe()), type, true, isInternalPointer, false, false, size);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateTemporaryWithKnowObjectIndex(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR::SymbolReference *symRef = findTempSymRefWithKnownObject(knownObjectIndex);
   if (symRef)
      return symRef;
   return findOrCreateAutoSymbolImpl(owningMethodSymbol,
                                 owningMethodSymbol->incTempIndex(fe()),
                                 TR::Address,
                                 true /* isReference */,
                                 false /* isInternalPointer*/,
                                 false /* reuseAuto */,
                                 false /* isAdjunct */,
                                 0 /*size*/,
                                 knownObjectIndex);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreatePendingPushTemporary(
   TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType type, size_t size)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR_ASSERT(!type.isBCD() || size,"binary coded decimal types must provide a size\n");
   TR_ASSERT_FATAL(!owningMethodSymbol->comp()->getOption(TR_EnableOSR) || (slot + TR::Symbol::convertTypeToNumberOfSlots(type) - 1) < owningMethodSymbol->getNumPPSlots(),
      "cannot create a pending push temporary that exceeds the number of slots for this method\n");
#endif
   TR::SymbolReference *tempSymRef = findOrCreateAutoSymbol(owningMethodSymbol, -(slot + 1), type, true, false, false, false, size);
   tempSymRef->getSymbol()->setIsPendingPush();
   return tempSymRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createNamedStatic(TR::ResolvedMethodSymbol *owningMethodSymbol, TR::DataType type, const char *name)
   {
   TR::StaticSymbol * sym = TR::StaticSymbol::createNamed(trHeapMemory(),type, name);
   return new (trHeapMemory()) TR::SymbolReference(self(),sym,owningMethodSymbol->getResolvedMethodIndex(),owningMethodSymbol->incTempIndex(fe()));
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createLocalObject(int32_t objectSize, TR::ResolvedMethodSymbol * owningMethodSymbol, TR::SymbolReference *classSymRef)
   {
   int32_t             slot              = owningMethodSymbol->incTempIndex(fe());
   TR_ResolvedMethod * method            = owningMethodSymbol->getResolvedMethod();
   mcount_t            owningMethodIndex = owningMethodSymbol->getResolvedMethodIndex();

   TR::AutomaticSymbol  * sym             = TR::AutomaticSymbol::createLocalObject(trHeapMemory(), TR::New, classSymRef, TR::Aggregate, objectSize, fe());
   sym->setBehaveLikeNonTemp();
   owningMethodSymbol->addAutomatic(sym);
   TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodIndex, slot);
   owningMethodSymbol->getAutoSymRefs(slot).add(symRef);
   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createLocalAddrArray(int32_t objectSize, TR::ResolvedMethodSymbol * owningMethodSymbol, TR::SymbolReference *classSymRef)
   {
   int32_t             slot              = owningMethodSymbol->incTempIndex(fe());
   TR_ResolvedMethod * method            = owningMethodSymbol->getResolvedMethod();
   mcount_t            owningMethodIndex = owningMethodSymbol->getResolvedMethodIndex();

   TR::AutomaticSymbol  * sym             = TR::AutomaticSymbol::createLocalObject(trHeapMemory(),TR::anewarray, classSymRef, TR::Aggregate, objectSize, fe());
   sym->setBehaveLikeNonTemp();
   owningMethodSymbol->addAutomatic(sym);
   TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodIndex, slot);
   owningMethodSymbol->getAutoSymRefs(slot).add(symRef);
   return symRef;
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::createLocalPrimArray(int32_t objectSize, TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t arrayType)
   {
   int32_t             slot              = owningMethodSymbol->incTempIndex(fe());
   TR_ResolvedMethod * method            = owningMethodSymbol->getResolvedMethod();
   mcount_t            owningMethodIndex = owningMethodSymbol->getResolvedMethodIndex();

   TR::AutomaticSymbol  * sym             = TR::AutomaticSymbol::createLocalObject(trHeapMemory(), arrayType, TR::Aggregate, objectSize, fe());
   sym->setBehaveLikeNonTemp();
   sym->setNotCollected();
   owningMethodSymbol->addAutomatic(sym);
   TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodIndex, slot);
   owningMethodSymbol->getAutoSymRefs(slot).add(symRef);
   return symRef;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateCounterSymRef(char *name, TR::DataType d, void *address)
   {
   ListIterator<TR::SymbolReference> i(&_debugCounterSymbolRefs);
   TR::SymbolReference *result;
   for (result = i.getFirst(); result; result = i.getNext())
      {
      TR::StaticSymbol *named = (TR::StaticSymbol*)result->getSymbol();
      if (!strcmp(named->getName(), name))
         return result;
      }

   // No match
   //
   TR::StaticSymbol * sym = TR::StaticSymbol::createNamed(trHeapMemory(), d,address,name);
   sym->setNotDataAddress();
   result = new (trHeapMemory()) TR::SymbolReference(self(),sym);
   _debugCounterSymbolRefs.add(result);
   return result;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::createSymbolReference(TR::Symbol *sym, intptr_t offset)
   {
   TR::SymbolReference *ref = new (trHeapMemory()) TR::SymbolReference(self(), sym, offset);
   ref->setSymbol(sym);
   return ref;
   }


TR::SymbolReference * OMR::SymbolReferenceTable::getSymRef(CommonNonhelperSymbol i)
   {
   if (i < getLastCommonNonhelperSymbol())
      {
      return baseArray.element(getNonhelperIndex(i));
      }
   else
      {
      return NULL;
      }
   }


bool OMR::SymbolReferenceTable::isNonHelper(TR::SymbolReference *symRef, CommonNonhelperSymbol s)
   {
   return isNonHelper(symRef->getReferenceNumber(), s);
   }

bool OMR::SymbolReferenceTable::isNonHelper(int32_t ref, CommonNonhelperSymbol s)
   {
   if (unsigned(ref) >= _numHelperSymbols && unsigned(ref) < _numHelperSymbols + getLastCommonNonhelperSymbol() && s < getLastCommonNonhelperSymbol())
      {
      return ref == getNonhelperIndex(s);
      }
   else
      {
      return false;
      }
   }

bool OMR::SymbolReferenceTable::isNonHelper(TR::SymbolReference *symRef)
   {
   return isNonHelper(symRef->getReferenceNumber());
   }

bool OMR::SymbolReferenceTable::isNonHelper(int32_t ref)
   {
   return (unsigned(ref) >= _numHelperSymbols && unsigned(ref) < _numHelperSymbols + getLastCommonNonhelperSymbol());
   }

OMR::SymbolReferenceTable::CommonNonhelperSymbol OMR::SymbolReferenceTable::getNonHelperSymbol(TR::SymbolReference *symRef)
   {
   return getNonHelperSymbol(symRef->getReferenceNumber());
   }

OMR::SymbolReferenceTable::CommonNonhelperSymbol OMR::SymbolReferenceTable::getNonHelperSymbol(int32_t ref)
   {
   return isNonHelper(ref) ? (CommonNonhelperSymbol) (ref - _numHelperSymbols)
                           : getLastCommonNonhelperSymbol();
   }

int32_t OMR::SymbolReferenceTable::getNonhelperIndex(CommonNonhelperSymbol s)
   {
   TR_ASSERT(s <= getLastCommonNonhelperSymbol(), "assertion failure");
   return _numHelperSymbols + s;
   }


TR::SymbolReference *nullSymRef = NULL;
TR::SymbolReference * & OMR::SymbolReferenceTable::element(TR_RuntimeHelper s)
   {
   return baseArray.element(s);
   }


TR::SymbolReference * & OMR::SymbolReferenceTable::element(CommonNonhelperSymbol s)
   {
   if (s < getLastCommonNonhelperSymbol())
      {
      return baseArray.element(getNonhelperIndex(s));
      }
   else
      {
      TR_ASSERT(nullSymRef == NULL, "assertion failure");
      return nullSymRef;
      }
   }

void OMR::SymbolReferenceTable::makeSharedAliases(TR::SymbolReference *sr1, TR::SymbolReference *sr2)
   {
   int32_t symRefNum1 = sr1->getReferenceNumber();
   int32_t symRefNum2 = sr2->getReferenceNumber();
   TR_BitVector *aliases1 = NULL;
   TR_BitVector *aliases2 = NULL;

   TR_ASSERT(sr1->reallySharesSymbol(), "SymRef1 should have its ReallySharesSymbol flag set.\n");
   TR_ASSERT(sr2->reallySharesSymbol(), "SymRef2 should have its ReallySharesSymbol flag set.\n");

   if (_sharedAliasMap == NULL)
      {
      _sharedAliasMap = new (comp()->trHeapMemory()) AliasMap(std::less<int32_t>(), comp()->allocator());
      }
    else
       {
       AliasMap::iterator iter1 = _sharedAliasMap->find(symRefNum1);
       if (iter1 != _sharedAliasMap->end())
          aliases1 = iter1->second;
       AliasMap::iterator iter2 = _sharedAliasMap->find(symRefNum2);
       if (iter2 != _sharedAliasMap->end())
          aliases2 = iter2->second;
       }

    if (aliases1 == NULL)
       {
       aliases1 = new (comp()->trHeapMemory()) TR_BitVector(self()->getNumSymRefs(), comp()->trMemory(), heapAlloc);
       aliases1->empty();
       _sharedAliasMap->insert(std::make_pair(symRefNum1, aliases1));
       }

    if (aliases2 == NULL)
       {
       aliases2 = new (comp()->trHeapMemory()) TR_BitVector(self()->getNumSymRefs(), comp()->trMemory(), heapAlloc);
       aliases2->empty();
       _sharedAliasMap->insert(std::make_pair(symRefNum2, aliases2));
       }

   aliases1->set(sr2->getReferenceNumber());
   aliases2->set(sr1->getReferenceNumber());
   }

TR_BitVector *OMR::SymbolReferenceTable::getSharedAliases(TR::SymbolReference *sr)
   {
   if (_sharedAliasMap == NULL)
      return NULL;

   AliasMap::iterator match = _sharedAliasMap->find(sr->getReferenceNumber());
   if (match != _sharedAliasMap->end())
      {
      return match->second;
      }

   return NULL;
   }


TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateJProfileValuePlaceHolderSymbolRef()
   {
   if (!element(jProfileValueSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_None);
      sym->setHelper();
      element(jProfileValueSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), jProfileValueSymbol, sym);
      }
   return element(jProfileValueSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateJProfileValuePlaceHolderWithNullCHKSymbolRef()
   {
   if (!element(jProfileValueWithNullCHKSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(),TR_None);
      sym->setHelper();
      element(jProfileValueWithNullCHKSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), jProfileValueWithNullCHKSymbol, sym);
      }
   return element(jProfileValueWithNullCHKSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreatePotentialOSRPointHelperSymbolRef()
   {
   if (!element(potentialOSRPointHelperSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(), TR_None);
      sym->setHelper();
      TR::SymbolReference* symRef = new (trHeapMemory()) TR::SymbolReference(self(), potentialOSRPointHelperSymbol, sym);
      element(potentialOSRPointHelperSymbol) = symRef;
      }
   return element(potentialOSRPointHelperSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateOSRFearPointHelperSymbolRef()
   {
   if (!element(osrFearPointHelperSymbol))
      {
      TR::MethodSymbol * sym = TR::MethodSymbol::create(trHeapMemory(), TR_None);
      sym->setHelper();
      TR::SymbolReference* symRef = new (trHeapMemory()) TR::SymbolReference(self(), osrFearPointHelperSymbol, sym);
      element(osrFearPointHelperSymbol) = symRef;
      }
   return element(osrFearPointHelperSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::findOrCreateEAEscapeHelperSymbolRef()
   {
   if (!element(eaEscapeHelperSymbol))
      {
      TR::MethodSymbol* sym = TR::MethodSymbol::create(trHeapMemory(), TR_None);
      sym->setHelper();
      TR::SymbolReference* symRef = new (trHeapMemory()) TR::SymbolReference(self(), eaEscapeHelperSymbol, sym);
      element(eaEscapeHelperSymbol) = symRef;
      }
   return element(eaEscapeHelperSymbol);
   }

TR::SymbolReference *
OMR::SymbolReferenceTable::getOriginalUnimprovedSymRef(TR::SymbolReference *symRef)
   {
   auto entry = _originalUnimprovedSymRefs.find(symRef->getReferenceNumber());
   if (entry == _originalUnimprovedSymRefs.end())
      return symRef;
   else
      return getSymRef(entry->second);
   }

void
OMR::SymbolReferenceTable::rememberOriginalUnimprovedSymRef(
   TR::SymbolReference *improved,
   TR::SymbolReference *original)
   {
   original = getOriginalUnimprovedSymRef(original);
   auto insertResult = _originalUnimprovedSymRefs.insert(
      std::make_pair(improved->getReferenceNumber(), original->getReferenceNumber()));

   auto entryPreventingInsertion = insertResult.first;
   bool insertionSucceeded = insertResult.second;
   TR_ASSERT_FATAL(
      insertionSucceeded,
      "original unimproved symref collision for #%d: originals are #%d and #%d",
      improved->getReferenceNumber(),
      entryPreventingInsertion->second,
      original->getReferenceNumber());
   }


const char *OMR::SymbolReferenceTable::_commonNonHelperSymbolNames[] =
   {
   "<contiguousArraySize>",
   "<discontiguousArraySize>",
   "<arrayClassRomPtr>",
   "<javaLangClassFromClass>",
   "<classFromJavaLangClass>",
   "<addressOfClassOfMethod>",
   "<componentClass>",
   "<isArray>",
   "<isClassAndDepthFlags>",
   "<isClassFlags>",
   "<vft>",
   "<currentThread>",
   "<recompilationCounter>",
   "<excp>",
   "<indexableSize>",
   "<resolveCheck>",
   "<arrayTranslate>",
   "<arrayTranslateAndTest>",
   "<long2String>",
   "<bitOpMem>",
   "<reverseLoad>",
   "<reverseStore>",
   "<currentTimeMaxPrecision>",
   "<headerFlags>",
   "<singlePrecisionSQRT>",
   "<countForRecompile>",
   "<gcrPatchPoint>",
   "<counterAddress>",
   "<startPC>",
   "<compiledMethod>",
   "<thisRangeExtension>",
   "<profilingBufferCursor>",
   "<profilingBufferEnd>",
   "<profilingBuffer>",
   "<osrBuffer>",
   "<osrScratchBuffer>",
   "<osrFrameIndex>",
   "<osrReturnAddress>",
   "<contiguousArrayDataAddrField>",
   "<potentialOSRPointHelper>",
   "<osrFearPointHelper>",
   "<eaEscapeHelper>",
   "<instanceShape>",
   "<instanceDescription>",
   "<descriptionWordFromPtr>",
   "<objectEqualityComparison>",
   "<objectInequalityComparison>",
   "<nonNullableArrayNullStoreCheck>",
   "<loadFlattenableArrayElementNonHelper>",
   "<storeFlattenableArrayElementNonHelper>",
   "<synchronizedFieldLoad>",
   "<atomicAdd>",
   "<atomicFetchAndAdd>",
   "<atomicFetchAndAdd32Bit>",
   "<atomicFetchAndAdd64Bit>",
   "<atomicSwap>",
   "<atomicSwap32Bit>",
   "<atomicSwap64Bit>",
   "<atomicCompareAndSwapReturnStatus>",
   "<atomicCompareAndSwapReturnValue>",
   "<jProfileValueSymbol>",
   "<jProfileValueWithNullCHKSymbol>"
   };


const char *
OMR::SymbolReferenceTable::getNonHelperSymbolName(CommonNonhelperSymbol nonHelper)
   {
   TR_ASSERT_FATAL(nonHelper >= OMRfirstPrintableCommonNonhelperSymbol &&
                   nonHelper <= OMRlastPrintableCommonNonhelperSymbol,
                   "unknown non helper %" OMR_PRId32, static_cast<int32_t>(nonHelper));

#if 0
   static_assert(sizeof(_commonNonHelperSymbolNames)/sizeof(_commonNonHelperSymbolNames[0]) ==
                 static_cast<int32_t>(OMRlastPrintableCommonNonhelperSymbol - OMRfirstPrintableCommonNonhelperSymbol + 1),
                 "_commonNonHelperSymbolNames array must match CommonNonHelperSymbol enumeration");
#endif

   return _commonNonHelperSymbolNames[static_cast<int32_t>(nonHelper - OMRfirstPrintableCommonNonhelperSymbol)];
   }
