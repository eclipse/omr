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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
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
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"
#include "env/CHTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#endif

class TR_OpaqueClassBlock;
namespace TR { class Register; }

#define PEEK_THRESHOLD 50

#ifdef J9_PROJECT_SPECIFIC
static TR_BitVector * addVeryRefinedCallAliasSets(TR::ResolvedMethodSymbol *, TR_BitVector *, List<void> *);
#endif

OMR::SymbolReference::SymbolReference(TR::SymbolReferenceTable * symRefTab, TR::SymbolReference& sr, intptr_t o, TR::KnownObjectTable::Index knownObjectIndex)
   {
   _referenceNumber = symRefTab->assignSymRefNumber(self());
   _symbol = sr._symbol;
   _offset  = sr._offset + o;
   _owningMethodIndex = sr._owningMethodIndex;
   _cpIndex = sr._cpIndex;
   _unresolvedIndex = sr._unresolvedIndex;
   _extraInfo = 0;
   _flags.set(sr._flags);
   _useDefAliases = NULL;
   _knownObjectIndex = knownObjectIndex;
   self()->copyAliasSets(&sr, symRefTab);
   symRefTab->aliasBuilder.updateSubSets(self());
   }

void
OMR::SymbolReference::copyAliasSets(TR::SymbolReference * fromSymRef, TR::SymbolReferenceTable * symRefTab)
   {
   _useDefAliases = fromSymRef->_useDefAliases;
   }

TR_BitVector *
OMR::SymbolReference::getUseonlyAliasesBV(TR::SymbolReferenceTable * symRefTab)
   {
   int32_t kind = _symbol->getKind();
   switch (kind)
      {
      case TR::Symbol::IsMethod:
         {
         TR::MethodSymbol * methodSymbol = _symbol->castToMethodSymbol();

         // Aliasing for potentialOSRPointHelper
         // A potentialOSRPointHelper call is an exception point that may go to OSR catch block ( see
         // Node API exceptionsRaised), the control flow constraint imposed by the exception edge will
         // apply to all the global optimizations that may move things around. Local optimizations also
         // ask exceptionsRaised to determine if a code motion across certain point is safe. So aliasing
         // is not necessary. However, we'd like to add aliasing here to cause the compiler to be more
         // conservative about reordering this helper with other operations. The aliasing can always be
         // relaxed when necessary.
         //
         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::potentialOSRPointHelperSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }

         // Aliasing for osrFearPointHelper
         // Preventing the reordering of fear point helper w.r.t. OSR points and yield/invalidation points is
         // the minimum requirement of aliasing for OSR fear point helper. These reorderings would in almost
         // all cases be naturally disallowed simply due to the fact that the fear point is represented as a
         // call, which even without aliasing could e.g. perform I/O. Thus the following is a highly conservative
         // aliasing and can be relaxed later when necessary
         //
         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::osrFearPointHelperSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }

         // Aliases for eaEscapeHelper
         // Ensure EA sees the method as causing an escape for all arguments passed
         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::eaEscapeHelperSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }
         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::objectEqualityComparisonSymbol)
             || symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::objectInequalityComparisonSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }
         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::nonNullableArrayNullStoreCheckSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }
         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::loadFlattenableArrayElementNonHelperSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::storeFlattenableArrayElementNonHelperSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }

         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::jitDispatchJ9MethodSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }

         if (!methodSymbol->isHelper())
            {
            return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }


         switch (self()->getReferenceNumber())
            {
            case TR_asyncCheck:
               return 0;

            // helpers that don't throw have no use aliases
            case TR_instanceOf:
            case TR_checkAssignable:
            case TR_monitorEntry:
            case TR_transactionEntry:
            case TR_reportFinalFieldModified:
            case TR_reportMethodEnter:
            case TR_reportStaticMethodEnter:
            case TR_reportMethodExit:
            case TR_acquireVMAccess:
            case TR_throwCurrentException:
            case TR_releaseVMAccess:
            case TR_stackOverflow:
            case TR_writeBarrierStore:
            case TR_writeBarrierStoreGenerational:
            case TR_writeBarrierStoreGenerationalAndConcurrentMark:
            case TR_writeBarrierBatchStore:
            case TR_typeCheckArrayStore:
            case TR_arrayStoreException:
            case TR_arrayBoundsCheck:
            case TR_checkCast:
            case TR_divCheck:
            case TR_overflowCheck:
            case TR_nullCheck:
            case TR_methodTypeCheck:
            case TR_incompatibleReceiver:
            case TR_IncompatibleClassChangeError:
            case TR_aThrow:
            case TR_aNewArray:
            case TR_monitorExit:
            case TR_transactionExit:
            case TR_newObject:
            case TR_newObjectNoZeroInit:
            case TR_acmpeqHelper:
            case TR_acmpneHelper:
            case TR_newValue:
            case TR_newValueNoZeroInit:
            case TR_newArray:
            case TR_multiANewArray:
            case TR_jitLookupDynamicInterfaceMethod:
            case TR_jitLookupDynamicPublicInterfaceMethod:
            default:
               return &symRefTab->aliasBuilder.defaultMethodUseAliases();
            }
         }
      case TR::Symbol::IsResolvedMethod:
         {
#ifdef J9_PROJECT_SPECIFIC
         TR::ResolvedMethodSymbol * resolvedMethodSymbol = _symbol->castToResolvedMethodSymbol();
         if (!TR::comp()->getOption(TR_EnableHCR))
            {
            switch (resolvedMethodSymbol->getRecognizedMethod())
               {
               case TR::java_lang_Double_longBitsToDouble:
               case TR::java_lang_Double_doubleToLongBits:
               case TR::java_lang_Float_intBitsToFloat:
               case TR::java_lang_Float_floatToIntBits:
               case TR::java_lang_Double_doubleToRawLongBits:
               case TR::java_lang_Float_floatToRawIntBits:
               case TR::java_lang_Math_sqrt:
               case TR::java_lang_StrictMath_sqrt:
               case TR::java_lang_Math_sin:
               case TR::java_lang_StrictMath_sin:
               case TR::java_lang_Math_cos:
               case TR::java_lang_StrictMath_cos:
               case TR::java_lang_Math_max_I:
               case TR::java_lang_Math_min_I:
               case TR::java_lang_Math_max_L:
               case TR::java_lang_Math_min_L:
               case TR::java_lang_Math_max_F:
               case TR::java_lang_Math_min_F:
               case TR::java_lang_Math_max_D:
               case TR::java_lang_Math_min_D:
               case TR::java_lang_Math_abs_I:
               case TR::java_lang_Math_abs_L:
               case TR::java_lang_Math_abs_F:
               case TR::java_lang_Math_abs_D:
               case TR::java_lang_Math_pow:
               case TR::java_lang_StrictMath_pow:
               case TR::java_lang_Math_exp:
               case TR::java_lang_StrictMath_exp:
               case TR::java_lang_Math_log:
               case TR::java_lang_StrictMath_log:
               case TR::java_lang_Math_floor:
               case TR::java_lang_Math_ceil:
               case TR::java_lang_Math_copySign_F:
               case TR::java_lang_Math_copySign_D:
               case TR::java_lang_StrictMath_floor:
               case TR::java_lang_StrictMath_ceil:
               case TR::java_lang_StrictMath_copySign_F:
               case TR::java_lang_StrictMath_copySign_D:
                  return NULL;

               default:
               	break;
               }
            }
#endif
         return &symRefTab->aliasBuilder.defaultMethodUseAliases();
         }

      case TR::Symbol::IsAutomatic:
      case TR::Symbol::IsParameter:

         if (symRefTab->aliasBuilder.catchLocalUseSymRefs().isSet(self()->getReferenceNumber()))
            return &symRefTab->aliasBuilder.methodsThatMayThrow();

         return 0;

      default:
         //TR_ASSERT(0, "getUseOnlyAliases: unexpected symbol kind ");
         return 0;
      }
   }

TR_BitVector *
OMR::SymbolReference::getUseDefAliasesBV(bool isDirectCall, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   TR::Region &aliasRegion = comp->aliasRegion();
   int32_t bvInitialSize = comp->getSymRefCount();
   TR_BitVectorGrowable growability = growable;

   // allow more than one shadow for an array type.  Used by LoopAliasRefiner
   const bool supportArrayRefinement=true;

   int32_t kind = _symbol->getKind();
   TR::SymbolReferenceTable * symRefTab = comp->getSymRefTab();

   // !!! NOTE !!!
   // THERE IS A COPY OF THIS LOGIC IN sharesSymbol
   //
   if (!self()->reallySharesSymbol(comp))
      {
      switch (kind)
         {
         case TR::Symbol::IsShadow:
         case TR::Symbol::IsStatic:
            {
            // For unresolved constant dynamic, we need to invoke a Java bootstrap method,
            // which can have arbitrary side effects, so the aliasing should be conservative here.
            // isConstObjectRef now returns true for condy, so we add an explicit condition,
            // more like a short-circuit, to say if we are unresolved and not isConstObjectRef
            // (this is the same as before), or if we are unresolved and condy
            // (this is the extra condition added), we would return conservative aliases.
            if ((self()->isUnresolved() && (_symbol->isConstantDynamic() || !_symbol->isConstObjectRef())) ||
	        _symbol->isVolatile() || self()->isLiteralPoolAddress() ||
                self()->isFromLiteralPool() || _symbol->isUnsafeShadowSymbol() ||
                (_symbol->isArrayShadowSymbol() && comp->getMethodSymbol()->hasVeryRefinedAliasSets()))
               {
               // getUseDefAliases might not return NULL
               }
            else if (!symRefTab->aliasBuilder.mutableGenericIntShadowHasBeenCreated())
               {
               // getUseDefAliases must return NULL
               return NULL;
               }
            else if (kind == TR::Symbol::IsStatic && !symRefTab->aliasBuilder.litPoolGenericIntShadowHasBeenCreated())
               {
               // getUseDefAliases must return NULL
               return NULL;
               }
            break;
            }
         }
      }

   // now do stuff for various kinds of symbols
   //
   switch (kind)
      {
      case TR::Symbol::IsMethod:
         {
         TR::MethodSymbol * methodSymbol = _symbol->castToMethodSymbol();

         if (!methodSymbol->isHelper())
            return symRefTab->aliasBuilder.methodAliases(self());

         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::arraySetSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::osrFearPointHelperSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::potentialOSRPointHelperSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::eaEscapeHelperSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::objectEqualityComparisonSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::objectInequalityComparisonSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::nonNullableArrayNullStoreCheckSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::loadFlattenableArrayElementNonHelperSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::storeFlattenableArrayElementNonHelperSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::jitDispatchJ9MethodSymbol))
            {
            return &symRefTab->aliasBuilder.defaultMethodDefAliases();
            }

         if (symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::arrayCmpSymbol) ||
             symRefTab->isNonHelper(self(), TR::SymbolReferenceTable::arrayCmpLenSymbol))
            return 0;

         switch (self()->getReferenceNumber())
            {
            case TR_methodTypeCheck:
            case TR_nullCheck:
               return &symRefTab->aliasBuilder.defaultMethodDefAliasesWithoutImmutable();

            case TR_arrayBoundsCheck:
            case TR_checkCast:
            case TR_divCheck:
            case TR_typeCheckArrayStore:
            case TR_arrayStoreException:
            case TR_incompatibleReceiver:
            case TR_IncompatibleClassChangeError:
            case TR_reportFinalFieldModified:
            case TR_reportMethodEnter:
            case TR_reportStaticMethodEnter:
            case TR_reportMethodExit:
            case TR_acquireVMAccess:
            case TR_instanceOf:
            case TR_checkAssignable:
            case TR_throwCurrentException:
            case TR_releaseVMAccess:
            case TR_stackOverflow:
            case TR_writeBarrierStore:
            case TR_writeBarrierBatchStore:
            case TR_jitProfileAddress:
            case TR_jitProfileWarmCompilePICAddress:
            case TR_jitProfileValue:
            case TR_jitProfileLongValue:
            case TR_jitProfileBigDecimalValue:
            case TR_jitProfileParseBuffer:
            case TR_jitLookupDynamicInterfaceMethod:
            case TR_jitLookupDynamicPublicInterfaceMethod:

               return 0;

            case TR_asyncCheck:
            case TR_writeBarrierClassStoreRealTimeGC:
            case TR_writeBarrierStoreRealTimeGC:
            case TR_aNewArray:
            case TR_acmpeqHelper:
            case TR_acmpneHelper:
            case TR_newValue:
            case TR_newValueNoZeroInit:
            case TR_newObject:
            case TR_newObjectNoZeroInit:
            case TR_newArray:
            case TR_multiANewArray:
               if ((comp->generateArraylets() || comp->isDLT()) && includeGCSafePoint)
                  return &symRefTab->aliasBuilder.gcSafePointSymRefNumbers();
               else
                  return 0;

            case TR_aThrow:
               return 0;

            // The monitor exit symbol needs to be aliased with all fields in the
            // current class to ensure that all references to fields are evaluated
            // before the monitor exit
            case TR_monitorExit:
            case TR_monitorEntry:
            case TR_transactionExit:
            case TR_transactionEntry:

            default:
               // The following is the place to check for
               // a use of killsAllMethodSymbolRef... However,
               // it looks like the default action is sufficient.
               //if (symRefTab->findKillsAllMethodSymbolRef() == self())
               //   {
               //   }
               return &symRefTab->aliasBuilder.defaultMethodDefAliases();
            }
         }
      case TR::Symbol::IsResolvedMethod:
         {
#ifdef J9_PROJECT_SPECIFIC
         TR::ResolvedMethodSymbol * resolvedMethodSymbol = _symbol->castToResolvedMethodSymbol();

         if (!comp->getOption(TR_EnableHCR))
            {
            switch (resolvedMethodSymbol->getRecognizedMethod())
               {
               case TR::java_lang_System_arraycopy:
                  {
                  TR_BitVector * aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
                  *aliases |= symRefTab->aliasBuilder.arrayElementSymRefs();
                  if (comp->generateArraylets())
                     *aliases |= symRefTab->aliasBuilder.arrayletElementSymRefs();
                  return aliases;
                  }

                  if (resolvedMethodSymbol->isPureFunction())
                      return NULL;

               case TR::java_lang_Double_longBitsToDouble:
               case TR::java_lang_Double_doubleToLongBits:
               case TR::java_lang_Float_intBitsToFloat:
               case TR::java_lang_Float_floatToIntBits:
               case TR::java_lang_Double_doubleToRawLongBits:
               case TR::java_lang_Float_floatToRawIntBits:
               case TR::java_lang_Math_sqrt:
               case TR::java_lang_StrictMath_sqrt:
               case TR::java_lang_Math_sin:
               case TR::java_lang_StrictMath_sin:
               case TR::java_lang_Math_cos:
               case TR::java_lang_StrictMath_cos:
               case TR::java_lang_Math_max_I:
               case TR::java_lang_Math_min_I:
               case TR::java_lang_Math_max_L:
               case TR::java_lang_Math_min_L:
               case TR::java_lang_Math_max_F:
               case TR::java_lang_Math_min_F:
               case TR::java_lang_Math_max_D:
               case TR::java_lang_Math_min_D:
               case TR::java_lang_Math_abs_I:
               case TR::java_lang_Math_abs_L:
               case TR::java_lang_Math_abs_F:
               case TR::java_lang_Math_abs_D:
               case TR::java_lang_Math_pow:
               case TR::java_lang_StrictMath_pow:
               case TR::java_lang_Math_exp:
               case TR::java_lang_StrictMath_exp:
               case TR::java_lang_Math_log:
               case TR::java_lang_StrictMath_log:
               case TR::java_lang_Math_floor:
               case TR::java_lang_Math_ceil:
               case TR::java_lang_Math_copySign_F:
               case TR::java_lang_Math_copySign_D:
               case TR::java_lang_StrictMath_floor:
               case TR::java_lang_StrictMath_ceil:
               case TR::java_lang_StrictMath_copySign_F:
               case TR::java_lang_StrictMath_copySign_D:
               case TR::com_ibm_Compiler_Internal__TR_Prefetch:
               case TR::java_nio_Bits_keepAlive:
                  if ((comp->generateArraylets() || comp->isDLT()) && includeGCSafePoint)
                     return &symRefTab->aliasBuilder.gcSafePointSymRefNumbers();
                  else
                     return 0;

               default:
               	break;
               }
            }
#endif //J9_PROJECT_SPECIFIC

#ifdef J9_PROJECT_SPECIFIC
         TR_ResolvedMethod * method = resolvedMethodSymbol->getResolvedMethod();
         TR_PersistentMethodInfo * methodInfo = comp->getRecompilationInfo() ? TR_PersistentMethodInfo::get(method) : NULL;
         if (methodInfo && (methodInfo->hasRefinedAliasSets() ||
                            comp->getMethodHotness() >= veryHot ||
                            resolvedMethodSymbol->hasVeryRefinedAliasSets()) &&
             (method->isStatic() || method->isFinal() || isDirectCall))
            {
            TR_BitVector * aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            if ((comp->generateArraylets() || comp->isDLT()) && includeGCSafePoint)
               *aliases |= symRefTab->aliasBuilder.gcSafePointSymRefNumbers();

            if (methodInfo->doesntKillAnything() && !comp->getOption(TR_DisableRefinedAliases))
               return aliases;

            if ((resolvedMethodSymbol->hasVeryRefinedAliasSets() || comp->getMethodHotness() >= hot) &&
                !debug("disableVeryRefinedCallAliasSets"))
               {
               TR_BitVector * exactAliases = 0;

               if (resolvedMethodSymbol->hasVeryRefinedAliasSets())
                  exactAliases = symRefTab->aliasBuilder.getVeryRefinedCallAliasSets(resolvedMethodSymbol);
               else
                  {
                  resolvedMethodSymbol->setHasVeryRefinedAliasSets(true);
                  List<void> methodsPeeked(comp->trMemory());
                  exactAliases = addVeryRefinedCallAliasSets(resolvedMethodSymbol, aliases, &methodsPeeked);
                  symRefTab->aliasBuilder.setVeryRefinedCallAliasSets(resolvedMethodSymbol, exactAliases);
                  }
               if (exactAliases)
                  {
                  return exactAliases;
                  }
               }

            // From here on, we're just checking refined alias info.
            // If refined aliases are disabled, return the conservative answer
            // we would have returned had we never attempted to use refined
            // aliases at all.
            //
            if (comp->getOption(TR_DisableRefinedAliases))
               return symRefTab->aliasBuilder.methodAliases(self());

            if (!methodInfo->doesntKillAddressArrayShadows())
               {

               symRefTab->aliasBuilder.addAddressArrayShadows(aliases);

               if (comp->generateArraylets())
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Address));
               }

            if (!methodInfo->doesntKillIntArrayShadows())
               {

               symRefTab->aliasBuilder.addIntArrayShadows(aliases);

               if (comp->generateArraylets())
                  {
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Int32));
                  }
               }

            if (!methodInfo->doesntKillNonIntPrimitiveArrayShadows())
               {

               symRefTab->aliasBuilder.addNonIntPrimitiveArrayShadows(aliases);

               if (comp->generateArraylets())
                  {
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Int8));
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Int16));
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Int32));
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Int64));
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Float));
                  aliases->set(symRefTab->getArrayletShadowIndex(TR::Double));
                  }
               }

            if (!methodInfo->doesntKillAddressFields())
               *aliases |= symRefTab->aliasBuilder.addressShadowSymRefs();

            if (!methodInfo->doesntKillIntFields())
               *aliases |= symRefTab->aliasBuilder.intShadowSymRefs();

            if (!methodInfo->doesntKillNonIntPrimitiveFields())
               *aliases |= symRefTab->aliasBuilder.nonIntPrimitiveShadowSymRefs();

            if (!methodInfo->doesntKillAddressStatics())
               *aliases |= symRefTab->aliasBuilder.addressStaticSymRefs();

            if (!methodInfo->doesntKillIntStatics())
               *aliases |= symRefTab->aliasBuilder.intStaticSymRefs();

            if (!methodInfo->doesntKillNonIntPrimitiveStatics())
               *aliases |= symRefTab->aliasBuilder.nonIntPrimitiveStaticSymRefs();

            TR_BitVector *methodAliases = symRefTab->aliasBuilder.methodAliases(self());
            *aliases &= *methodAliases;
            return aliases;
            }
#endif

         return symRefTab->aliasBuilder.methodAliases(self());
         }
      case TR::Symbol::IsShadow:
         {
         if ((self()->isUnresolved() && !_symbol->isConstObjectRef()) || _symbol->isVolatile() || self()->isLiteralPoolAddress() || self()->isFromLiteralPool() ||
             (_symbol->isUnsafeShadowSymbol() && !self()->reallySharesSymbol()))
            {
            return &comp->getSymRefTab()->aliasBuilder.defaultMethodDefAliasesWithoutImmutable();
            }

         TR_BitVector *aliases = NULL;
         if (_symbol == symRefTab->findGenericIntShadowSymbol())
            {
            aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            *aliases |= symRefTab->aliasBuilder.arrayElementSymRefs();
            if (comp->generateArraylets())
               *aliases |= symRefTab->aliasBuilder.arrayletElementSymRefs();
            *aliases |= symRefTab->aliasBuilder.genericIntShadowSymRefs();
            *aliases |= symRefTab->aliasBuilder.genericIntArrayShadowSymRefs();
            *aliases |= symRefTab->aliasBuilder.genericIntNonArrayShadowSymRefs();
            *aliases |= symRefTab->aliasBuilder.unsafeSymRefNumbers();
#ifdef J9_PROJECT_SPECIFIC
            *aliases |= symRefTab->aliasBuilder.unresolvedShadowSymRefs();
#endif
            if (symRefTab->aliasBuilder.conservativeGenericIntShadowAliasing())
               {
               *aliases |= symRefTab->aliasBuilder.addressShadowSymRefs();
               *aliases |= symRefTab->aliasBuilder.intShadowSymRefs();
               *aliases |= symRefTab->aliasBuilder.nonIntPrimitiveShadowSymRefs();
               }
            aliases->set(self()->getReferenceNumber());
            return aliases;
            }

         if (self()->reallySharesSymbol(comp))
            {
            aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            self()->setSharedShadowAliases(aliases, symRefTab);
            }

         if (symRefTab->findGenericIntShadowSymbol())
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            self()->setLiteralPoolAliases(aliases, symRefTab);

            if (symRefTab->aliasBuilder.conservativeGenericIntShadowAliasing() || self()->isUnresolved())
               {
               *aliases |= symRefTab->aliasBuilder.genericIntShadowSymRefs();
               *aliases |= symRefTab->aliasBuilder.genericIntArrayShadowSymRefs();
               *aliases |= symRefTab->aliasBuilder.genericIntNonArrayShadowSymRefs();
               }
            }

         if (_symbol->isArrayShadowSymbol() &&
             symRefTab->findGenericIntShadowSymbol())
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            *aliases |= symRefTab->aliasBuilder.genericIntShadowSymRefs();
            *aliases |= symRefTab->aliasBuilder.genericIntArrayShadowSymRefs();

            if (supportArrayRefinement && self()->getIndependentSymRefs())
               *aliases -= *self()->getIndependentSymRefs();
            }

#ifdef J9_PROJECT_SPECIFIC
         // make TR::PackedDecimal aliased with TR::Int8(byte)
         if (_symbol->isArrayShadowSymbol() && _symbol->getDataType() == TR::PackedDecimal)
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            aliases->set(symRefTab->getArrayShadowIndex(TR::Int8));
            }
         //the other way around.
         if (_symbol->isArrayShadowSymbol() && _symbol->getDataType() == TR::Int8)
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            aliases->set(symRefTab->getArrayShadowIndex(TR::PackedDecimal));
            }
#endif

         // alias vector arrays shadows  with corresponding scalar array shadows
         if (_symbol->isArrayShadowSymbol() && _symbol->getDataType().isVector())
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            aliases->set(symRefTab->getArrayShadowIndex(_symbol->getDataType().getVectorElementType()));
            }
         // the other way around
         if (_symbol->isArrayShadowSymbol() && _symbol->getDataType().isVectorElement())
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);

            // alias with vectors of all supported lengths
            for (int32_t i = 1; i <= TR::NumVectorLengths; i++)
               {
               aliases->set(symRefTab->getArrayShadowIndex(_symbol->getDataType().scalarToVector((TR::VectorLength)i)));
               }
            }

         if (_symbol->isArrayShadowSymbol() &&
             !symRefTab->aliasBuilder.immutableArrayElementSymRefs().isEmpty())
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);

            TR::DataType type = _symbol->getDataType();
            TR_BitVectorIterator bvi(symRefTab->aliasBuilder.arrayElementSymRefs());
            int32_t symRefNum;
            while (bvi.hasMoreElements())
               {
               symRefNum = bvi.getNextElement();
               if (symRefTab->getSymRef(symRefNum)->getSymbol()->getDataType() == type)
                  aliases->set(symRefNum);
               }
            }

         if (_symbol->isArrayShadowSymbol() &&
             supportArrayRefinement &&
             comp->getMethodSymbol()->hasVeryRefinedAliasSets())
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);

            TR::DataType type = _symbol->getDataType();
            TR_BitVectorIterator bvi(symRefTab->aliasBuilder.arrayElementSymRefs());
            int32_t symRefNum;
            while (bvi.hasMoreElements())
               {
               symRefNum = bvi.getNextElement();
               if (symRefTab->getSymRef(symRefNum)->getSymbol()->getDataType() == type)
                  aliases->set(symRefNum);
               }

            if (self()->getIndependentSymRefs())
               *aliases -= *self()->getIndependentSymRefs();

            return aliases;
            }

         if (aliases)
            aliases->set(self()->getReferenceNumber());

         return aliases;
         }
      case TR::Symbol::IsStatic:
         {
         // For unresolved constant dynamic, we need to invoke a Java bootstrap method,
         // which can have arbitrary side effects, so the aliasing should be conservative here.
         // isConstObjectRef now returns true for condy, so we add an explicit condition,
         // more like a short-circuit, to say if we are unresolved and not isConstObjectRef
         // (this is the same as before), or if we are unresolved and condy
         // (this is the extra condition added), we would return conservative aliases.
         if ((self()->isUnresolved() && (_symbol->isConstantDynamic() || !_symbol->isConstObjectRef())) ||
	     self()->isLiteralPoolAddress() || self()->isFromLiteralPool() || _symbol->isVolatile())
            {
            return &comp->getSymRefTab()->aliasBuilder.defaultMethodDefAliases();
            }

         TR_BitVector *aliases = NULL;
         if (self()->reallySharesSymbol(comp))
            {
            aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            self()->setSharedStaticAliases(aliases, symRefTab);
            }

         if (symRefTab->findGenericIntShadowSymbol())
            {
            if (!aliases)
               aliases = new (aliasRegion) TR_BitVector(bvInitialSize, aliasRegion, growability);
            self()->setLiteralPoolAliases(aliases, symRefTab);
            }

         if (aliases)
            aliases->set(self()->getReferenceNumber());

         return aliases;
         }
      case TR::Symbol::IsMethodMetaData:
         {
         TR_BitVector *aliases = NULL;
         return aliases;
         }
      default:
         //TR_ASSERT(0, "getUseDefAliasing called for non method");
         if (comp->generateArraylets() && comp->getSymRefTab()->aliasBuilder.gcSafePointSymRefNumbers().get(self()->getReferenceNumber()) && includeGCSafePoint)
            return &comp->getSymRefTab()->aliasBuilder.gcSafePointSymRefNumbers();
         else
            return 0;


      }
   }

bool
OMR::SymbolReference::sharesSymbol(bool includingGCSafePoint)
   {
   TR::Compilation * c = TR::comp();
   if (self()->reallySharesSymbol(c))
      return true;

   // At this point, we'd like to call getUseDefAliases(c, false) and return
   // true iff that is non-NULL.  However, doing so caused floatSanity
   // (specifically CompactNullChecks) to consume immense amounts (1GB+) of
   // memory and run for a long, long time (half an hour or more in some
   // cases), so we need to copy some of that logic in here as a short-circuit.
   //
   // !!! NOTE !!!
   // THERE IS A COPY OF THIS LOGIC IN getUseDefAliases
   //
   int32_t kind = _symbol->getKind();
   TR::SymbolReferenceTable * symRefTab = c->getSymRefTab();
   switch (kind)
      {
      case TR::Symbol::IsShadow:
      case TR::Symbol::IsStatic:
         {
         // For unresolved constant dynamic, we need to invoke a Java bootstrap method,
         // which can have arbitrary side effects, so the aliasing should be conservative here.
         // isConstObjectRef now returns true for condy, so we add an explicit condition,
         // more like a short-circuit, to say if we are unresolved and not isConstObjectRef
         // (this is the same as before), or if we are unresolved and condy
         // (this is the extra condition added), we would return conservative aliases.
         if ((self()->isUnresolved() && (_symbol->isConstantDynamic() || !_symbol->isConstObjectRef())) ||
	       _symbol->isVolatile() || self()->isLiteralPoolAddress() ||
               self()->isFromLiteralPool() || _symbol->isUnsafeShadowSymbol() ||
               (_symbol->isArrayShadowSymbol() && c->getMethodSymbol()->hasVeryRefinedAliasSets()))
            {
            // getUseDefAliases might not return NULL
            }
         else if (!symRefTab->aliasBuilder.mutableGenericIntShadowHasBeenCreated())
            {
            // getUseDefAliases must return NULL
            return false;
            }
         else if (kind == TR::Symbol::IsStatic && !symRefTab->aliasBuilder.litPoolGenericIntShadowHasBeenCreated())
            {
            // getUseDefAliases must return NULL
            return false;
            }
         break;
         }
      }

   return !self()->getUseDefAliases(false, includingGCSafePoint).isZero(c);
   }

void
OMR::SymbolReference::setLiteralPoolAliases(TR_BitVector * aliases, TR::SymbolReferenceTable * symRefTab)
   {
   if (!symRefTab->findGenericIntShadowSymbol())
      return;

   TR_SymRefIterator i(symRefTab->aliasBuilder.genericIntShadowSymRefs(), symRefTab);
   TR::SymbolReference * symRef;
   while ((symRef = i.getNext()))
      if (symRef->isLiteralPoolAddress() || symRef->isFromLiteralPool())
         aliases->set(symRef->getReferenceNumber());

   aliases->set(self()->getReferenceNumber());

   *aliases |= symRefTab->aliasBuilder.unsafeSymRefNumbers();
   }

void
OMR::SymbolReference::setSharedShadowAliases(TR_BitVector * aliases, TR::SymbolReferenceTable * symRefTab)
   {
   if (self()->reallySharesSymbol() && !_symbol->isUnsafeShadowSymbol())
      {
      TR::DataType type = self()->getSymbol()->getType();
      TR_SymRefIterator i(type.isAddress() ? symRefTab->aliasBuilder.addressShadowSymRefs()
                                           : (type.isInt32() ? symRefTab->aliasBuilder.intShadowSymRefs()
                                                             : symRefTab->aliasBuilder.nonIntPrimitiveShadowSymRefs()), symRefTab);
      TR::SymbolReference * symRef;
      while ((symRef = i.getNext()))
         if (symRef->getSymbol() == self()->getSymbol())
            aliases->set(symRef->getReferenceNumber());

      // include symbol reference's own shared alias bitvector
      if (symRefTab->getSharedAliases(self()) != NULL)
         *aliases |= *(symRefTab->getSharedAliases(self()));
      }
   else
      aliases->set(self()->getReferenceNumber());

   *aliases |= symRefTab->aliasBuilder.unsafeSymRefNumbers();
   }

void
OMR::SymbolReference::setSharedStaticAliases(TR_BitVector * aliases, TR::SymbolReferenceTable * symRefTab)
   {
   if (self()->reallySharesSymbol())
      {
      TR::DataType type = self()->getSymbol()->getType();
      TR_SymRefIterator i(type.isAddress() ? symRefTab->aliasBuilder.addressStaticSymRefs()
                                           : (type.isInt32() ? symRefTab->aliasBuilder.intStaticSymRefs()
                                                             : symRefTab->aliasBuilder.nonIntPrimitiveStaticSymRefs()), symRefTab);
      TR::SymbolReference * symRef;
      while ((symRef = i.getNext()))
         if (symRef->getSymbol() == self()->getSymbol())
            aliases->set(symRef->getReferenceNumber());
      }
   else
      aliases->set(self()->getReferenceNumber());

   *aliases |= symRefTab->aliasBuilder.unsafeSymRefNumbers();
   }

#ifdef J9_PROJECT_SPECIFIC
TR_BitVector *
addVeryRefinedCallAliasSets(TR::ResolvedMethodSymbol * methodSymbol, TR_BitVector * aliases, List<void> * methodsPeeked)
   {
   TR::Compilation *comp = TR::comp();

   void * methodId = methodSymbol->getResolvedMethod()->getPersistentIdentifier();
   if (methodsPeeked->find(methodId))
      {
      // This can't be allocated into the alias region as it must be accessed across optimizations
      TR_BitVector *heapAliases = new (comp->trHeapMemory()) TR_BitVector(comp->getSymRefCount(), comp->trMemory(), heapAlloc, growable);
      *heapAliases |= *aliases;
      return heapAliases;
      }

   // stop if the peek is getting very deep
   //
   if (methodsPeeked->getSize() >= PEEK_THRESHOLD)
      return 0;

   methodsPeeked->add(methodId);

   dumpOptDetails(comp, "O^O REFINING ALIASES: Peeking into the IL to refine aliases \n");

   if (!methodSymbol->getResolvedMethod()->genMethodILForPeeking(methodSymbol, comp, true))
      return 0;

   TR::SymbolReferenceTable * symRefTab = comp->getSymRefTab();
   for (TR::TreeTop * tt = methodSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
	   TR::Node *node = tt->getNode();
      if (node->getOpCode().isResolveCheck())
         return 0;

      if ((node->getOpCodeValue() == TR::treetop) ||
          (node->getOpCodeValue() == TR::compressedRefs) ||
          node->getOpCode().isCheck())
         node = node->getFirstChild();

      if (node->getOpCode().isStore())
         {
         TR::SymbolReference * symRefInCallee = node->getSymbolReference(), * symRefInCaller;
         TR::Symbol * symInCallee = symRefInCallee->getSymbol();
         TR::DataType type = symInCallee->getDataType();
         if (symInCallee->isShadow())
            {
            if (symInCallee->isArrayShadowSymbol())
               symRefInCaller = symRefTab->getSymRef(symRefTab->getArrayShadowIndex(type));

            else if (symInCallee->isArrayletShadowSymbol())
               symRefInCaller = symRefTab->getSymRef(symRefTab->getArrayletShadowIndex(type));

            else
               symRefInCaller = symRefTab->findShadowSymbol(symRefInCallee->getOwningMethod(comp), symRefInCallee->getCPIndex(), type);

            if (symRefInCaller)
               {
               if (symRefInCaller->reallySharesSymbol(comp))
                  symRefInCaller->setSharedShadowAliases(aliases, symRefTab);

               aliases->set(symRefInCaller->getReferenceNumber());
               }

            }
         else if (symInCallee->isStatic())
            {
            symRefInCaller = symRefTab->findStaticSymbol(symRefInCallee->getOwningMethod(comp), symRefInCallee->getCPIndex(), type);
            if (symRefInCaller)
               {
               if (symRefInCaller->reallySharesSymbol(comp))
                  symRefInCaller->setSharedStaticAliases(aliases, symRefTab);
               else
                  aliases->set(symRefInCaller->getReferenceNumber());
               }
            }
         }
      else if (node->getOpCode().isCall())
         {
         if (node->getOpCode().isCallIndirect())
            return 0;
         TR::ResolvedMethodSymbol * calleeSymbol = node->getSymbol()->getResolvedMethodSymbol();
         if (!calleeSymbol)
            return 0;
         TR_ResolvedMethod * calleeMethod = calleeSymbol->getResolvedMethod();
         if (!calleeMethod->isCompilable(comp->trMemory()) || calleeMethod->isJNINative())
            return 0;

         if (!addVeryRefinedCallAliasSets(calleeSymbol, aliases, methodsPeeked))
            return 0;
         }
      else if (node->getOpCodeValue() == TR::monent)
         return 0;
      }

   // This can't be allocated into the alias region as it must be accessed across optimizations
   TR_BitVector *heapAliases = new (comp->trHeapMemory()) TR_BitVector(comp->getSymRefCount(), comp->trMemory(), heapAlloc, growable);
   *heapAliases |= *aliases;
   return heapAliases;
   }
#endif

bool
OMR::SymbolReference::willUse(TR::SymbolReference * sr2, TR::SymbolReferenceTable* symRefTab)
   {
   if (self()->getSymbol() == sr2->getSymbol())
      return true;

   return self()->getUseonlyAliases().contains(sr2, symRefTab->comp());
   }

bool
OMR::SymbolReference::canKill(TR::SymbolReference * sr2)
   {
   TR::Compilation * comp = TR::comp();
   if (self()->getSymbol() == sr2->getSymbol())
      return true;
   if (!self()->sharesSymbol())
      return false;
   return self()->getUseDefAliases().contains(sr2, comp);
   }

bool
OMR::SymbolReference::storeCanBeRemoved()
   {
   TR::Compilation *comp = TR::comp();
   TR::Symbol * s = self()->getSymbol();

   return !s->isVolatile() &&
     (((s->getDataType() != TR::Double) && (s->getDataType() != TR::Float)) ||
           comp->cg()->getSupportsJavaFloatSemantics() ||
           (self()->isTemporary(comp) && !s->behaveLikeNonTemp()));
   }

const char *
OMR::SymbolReference::getTypeSignature(int32_t & len, TR_AllocationKind allocKind, bool *isFixed)
   {
   TR::Compilation * comp = TR::comp();

   switch (_symbol->getKind())
      {
      case TR::Symbol::IsParameter:
         return _symbol->castToParmSymbol()->getTypeSignature(len);

      case TR::Symbol::IsShadow:
         return (_cpIndex > 0 ? self()->getOwningMethod(comp)->fieldSignatureChars(_cpIndex, len) : 0);

      case TR::Symbol::IsStatic:
         return self()->getOwningMethod(comp)->staticSignatureChars(_cpIndex, len);

      case TR::Symbol::IsMethod:
      case TR::Symbol::IsResolvedMethod:
      case TR::Symbol::IsAutomatic:
      case TR::Symbol::IsMethodMetaData:
      case TR::Symbol::IsLabel:
      default:
         return 0;
      }
   }

/**
 * Copies the input symref reference number into this symref if such a copy is possible
 *
 * \parm  symRef     The source symRef
 * \parm  symRefTab  The Symbol Reference Table pointer.
 *
 * \attention This function doesn't accept null pointers.
 */
void
OMR::SymbolReference::copyRefNumIfPossible(TR::SymbolReference * symRef, TR::SymbolReferenceTable * symRefTab)
   {
   int32_t index = symRef->getReferenceNumber();
   if ((index < symRefTab->getNonhelperIndex(symRefTab->getLastCommonNonhelperSymbol())) ||
       (self()->getSymbol() == symRefTab->findGenericIntShadowSymbol()) ||
       symRef->getSymbol()->isUnsafeShadowSymbol())
      {
      self()->setReferenceNumber(index);
      }
   }

/**
 * make these two array shadows independent of each other, but still aliased to
 * all other array shadows
 */
void
OMR::SymbolReference::makeIndependent(TR::SymbolReferenceTable *symRefTab,
                                      TR::SymbolReference *symRef)
   {
   TR::Compilation *comp = symRefTab->comp();
   TR_ASSERT(self()->getSymbol()->isArrayShadowSymbol(),"symref #%d is not an array shadow\n",self()->getReferenceNumber());
   TR_ASSERT(symRef->getSymbol()->isArrayShadowSymbol(),"symref #%d is not an array shadow\n",symRef->getReferenceNumber());
   if(NULL == self()->getIndependentSymRefs())
      self()->setIndependentSymRefs(new(comp->trHeapMemory()) TR_BitVector(symRefTab->getNumSymRefs(),comp->trMemory(),heapAlloc,growable));

   if(NULL == symRef->getIndependentSymRefs())
      symRef->setIndependentSymRefs(new(comp->trHeapMemory()) TR_BitVector(symRefTab->getNumSymRefs(),comp->trMemory(),heapAlloc,growable));

   self()->getIndependentSymRefs()->set(symRef->getReferenceNumber());
   symRef->getIndependentSymRefs()->set(self()->getReferenceNumber());

   }

#undef PEEK_THRESHOLD

void
OMR::SymbolReference::setAliasedTo(TR_BitVector &bv, TR::SymbolReferenceTable *symRefTab, bool symmetric)
   {
   TR::Compilation *comp = symRefTab->comp();

   TR_ASSERT(_useDefAliases, "this symref doesn't have its own aliasing bitvector");
   if (!symmetric)
      {
      *_useDefAliases |= bv;
      }
   else
      {
      // we must process one by one to ensure symmetric aliasing
      TR_SymRefIterator sit(bv, symRefTab);
      for (TR::SymbolReference *symRef = sit.getNext();
            symRef;
            symRef = sit.getNext())
         {
         self()->setAliasedTo(symRef, true);
         }
      }
   }

/**
 * \pre SymRef must be for an ArrayShadow symbol.
 */
void
OMR::SymbolReference::setIndependentSymRefs(TR_BitVector *bv)
   {
   TR_ASSERT(_symbol->isArrayShadowSymbol(),"bad");
   _independentSymRefs=bv;
   }

/**
 * \pre SymRef must be for an ArrayShadow symbol.
 */
TR_BitVector *
OMR::SymbolReference::getIndependentSymRefs()
   {
   TR_ASSERT(_symbol->isArrayShadowSymbol(),"bad");
   return _independentSymRefs;
   }
