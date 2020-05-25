/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "il/SymbolReference.hpp"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/DataTypes.hpp"
#include "il/MethodSymbol.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"

class TR_OpaqueClassBlock;

TR::SymbolReference*
OMR::SymbolReference::self()
   {
   return static_cast<TR::SymbolReference*>(this);
   }

/**
 * Initialization method for SymbolReferences.
 *
 * This will likely go away in the longer term, once we can use delegating
 * constructors.
 */
void
OMR::SymbolReference::init(TR::SymbolReferenceTable * symRefTab,
          uint32_t      refNumber,
          TR::Symbol * sym,
          intptr_t     offset,
          mcount_t      owningMethodIndex,
          int32_t       cpIndex,
          int32_t       unresolvedIndex,
          bool          checkNoNamedShadow)
   {
   _referenceNumber = refNumber;
   _symbol = sym;
   _offset = offset;
   _owningMethodIndex = owningMethodIndex;
   _cpIndex = cpIndex;
   _useDefAliases = NULL;
   _unresolvedIndex = unresolvedIndex;
   _extraInfo = NULL;
   _knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
   symRefTab->aliasBuilder.updateSubSets(self());
   self()->setHasBeenAccessedAtRuntime(TR_maybe);
   }

OMR::SymbolReference::SymbolReference(TR::SymbolReferenceTable * symRefTab)
   {
   self()->init(symRefTab, symRefTab->assignSymRefNumber(self()));
   }

OMR::SymbolReference::SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   TR::Symbol * symbol,
                   intptr_t offset)
   {
   self()->init(symRefTab, symRefTab->assignSymRefNumber(self()), symbol, offset);
   }

OMR::SymbolReference::SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   int32_t refNumber,
                   TR::Symbol *ps,
                   intptr_t offset)
   {
   self()->init(symRefTab, refNumber, ps, offset);
   }

OMR::SymbolReference::SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::SymbolReferenceTable::CommonNonhelperSymbol number,
                   TR::Symbol *ps,
                   intptr_t offset)
   {
   self()->init(symRefTab, symRefTab->getNonhelperIndex(number), ps, offset);
   }


/**
 * Create a symbol reference, however, don't check named shadows
 */
OMR::SymbolReference::SymbolReference(TR::SymbolReferenceTable * symRefTab, TR::Symbol * symbol, intptr_t offset, const char *name)
   {
   self()->init(symRefTab,
        symRefTab->assignSymRefNumber(self()),
        symbol,
        offset,
        JITTED_METHOD_INDEX,  // Default owning method index
        -1,                   // Default cpIndex
        0,                    // Default unresolved Index
        false                 // Check no named shadows
       );
   }


OMR::SymbolReference::SymbolReference(
      TR::SymbolReferenceTable *symRefTab,
      TR::Symbol *sym,
      mcount_t owningMethodIndex,
      int32_t cpIndex,
      int32_t unresolvedIndex,
      TR::KnownObjectTable::Index knownObjectIndex)
   {
   self()->init(symRefTab,
        symRefTab->assignSymRefNumber(self()),
        sym,
        0,  // Offset 0
        owningMethodIndex,
        cpIndex,
        unresolvedIndex);

   _knownObjectIndex = knownObjectIndex;

   if (sym->isResolvedMethod())
      symRefTab->comp()->registerResolvedMethodSymbolReference(self());

   }


/**
 * Returns the method address stored in the underlying method symbol.
 *
 * This method only works if the underlying symbol is a MethodSymbol.
 */
void *
OMR::SymbolReference::getMethodAddress()
   {
   return (void *)self()->getSymbol()->castToMethodSymbol()->getMethodAddress();
   }

/**
 * Returns the method symbol associated with the owning method.
 */
TR::ResolvedMethodSymbol *
OMR::SymbolReference::getOwningMethodSymbol(TR::Compilation *c)
   {
   return c->getOwningMethodSymbol(self()->getOwningMethodIndex());
   }

/**
 * Returns the Owning Method
 */
TR_ResolvedMethod *
OMR::SymbolReference::getOwningMethod(TR::Compilation * c)
   {
   return self()->getOwningMethodSymbol(c)->getResolvedMethod();
   }

bool
OMR::SymbolReference::maybeVolatile()
   {
   if (_symbol->isVolatile())
      return true;

   if (self()->isUnresolved()
       && !_symbol->isConstObjectRef()
       && (_symbol->isShadow() || _symbol->isStatic()))
      return true;

   return false;
   }

bool
OMR::SymbolReference::isUnresolvedFieldInCP(TR::Compilation *c)
   {
   TR_ASSERT(c->compileRelocatableCode(),"isUnresolvedFieldInCP only callable in AOT compiles");

   if (!self()->isUnresolved())
      return false;

   if (c->getOption(TR_DisablePeekAOTResolutions))
      return true;

   return self()->getOwningMethod(c)->getUnresolvedFieldInCP(self()->getCPIndex());
   }

bool
OMR::SymbolReference::isUnresolvedMethodInCP(TR::Compilation *c)
   {
   TR_ASSERT(c->compileRelocatableCode() && self()->getSymbol()->isMethod(), "isUnresolvedMethodInCP only callable in AOT compiles on method symbols");

   if (!self()->isUnresolved())
      return false;

   if (c->getOption(TR_DisablePeekAOTResolutions))
      return true;

   TR::MethodSymbol *sym = self()->getSymbol()->getMethodSymbol();
   if (sym->isStatic())
      return self()->getOwningMethod(c)->getUnresolvedStaticMethodInCP(self()->getCPIndex());
   else if (sym->isSpecial())
      return self()->getOwningMethod(c)->getUnresolvedSpecialMethodInCP(self()->getCPIndex());
   else if (sym->isVirtual())
      return self()->getOwningMethod(c)->getUnresolvedVirtualMethodInCP(self()->getCPIndex());
   else
      return true;
   }

bool
OMR::SymbolReference::reallySharesSymbol(TR::Compilation * c)
   {
   return (_flags.testAny(ReallySharesSymbol)
           || (c->hasUnsafeSymbol()
               && (_symbol->isStatic() || _symbol->isShadow())));
   }

/**
 * Copy flags from another SymbolReference
 *
 * Used mostly in MemoryReference code.
 *
 * \todo: Investigate possible deprecation.
 */
void
OMR::SymbolReference::copyFlags(TR::SymbolReference * sr)
   {
   _flags.set(sr->_flags);
   }


bool
OMR::SymbolReference::isThisPointer()
   {
   TR::Compilation *c = TR::comp();
   TR::ParameterSymbol* p = _symbol->getParmSymbol();
   return p && p->getSlot() == 0
      && !self()->getOwningMethod(c)->isStatic();
   }

bool
OMR::SymbolReference::isTemporary(TR::Compilation *c)
   {
   return self()->getSymbol()->isAuto()
      && (self()->getCPIndex() >= self()->getOwningMethodSymbol(c)->getFirstJitTempIndex()
          || self()->getCPIndex() < 0);
   }

const char *
OMR::SymbolReference::getName(TR_Debug *debug)
   {
   if (debug)
      return debug->getName(self());
   else
      return "<unknown symref>";
   }

TR::SymbolReference *
OMR::SymbolReference::create(TR::SymbolReferenceTable *symRefTab, TR::Symbol *sym, TR::KnownObjectTable::Index koi)
   {
   TR::SymbolReference *result = new (symRefTab->trHeapMemory()) TR::SymbolReference(symRefTab, sym);
   result->_knownObjectIndex = koi;
   return result;
   }

TR::KnownObjectTable::Index
OMR::SymbolReference::getKnownObjectIndex()
   {
   if (self()->getSymbol())
      {
      TR::ParameterSymbol *parm = self()->getSymbol()->getParmSymbol();
      if (parm && parm->hasKnownObjectIndex())
         {
         if (_knownObjectIndex != TR::KnownObjectTable::UNKNOWN)
            TR_ASSERT(self()->getKnownObjectIndex() == parm->getKnownObjectIndex(), "Parm symbol and symref known-object indexes must match (%d != %d)", self()->getKnownObjectIndex(), parm->getKnownObjectIndex());
         return parm->getKnownObjectIndex();
         }
      }
   return _knownObjectIndex;
   }

bool
OMR::SymbolReference::hasKnownObjectIndex()
   {
   return self()->getKnownObjectIndex() != TR::KnownObjectTable::UNKNOWN;
   }

uintptr_t*
OMR::SymbolReference::getKnownObjectReferenceLocation(TR::Compilation *comp)
   {
   return self()->hasKnownObjectIndex() ?
      comp->getKnownObjectTable()->getPointerLocation(self()->getKnownObjectIndex()) :
      NULL;
   }

void
OMR::SymbolReference::setAliasedTo(TR::SymbolReference *other, bool symmetric)
   {
      {
      TR_ASSERT(_useDefAliases, "this symref doesn't have its own aliasing bitvector");
      _useDefAliases->set(other->getReferenceNumber());
      if (symmetric) other->setAliasedTo(self(), false);
      }
   }

void
OMR::SymbolReference::setEmptyUseDefAliases(TR::SymbolReferenceTable *symRefTab)
   {
   TR::Compilation *comp = symRefTab->comp();
   _useDefAliases = new (comp->trHeapMemory()) TR_BitVector(comp->getSymRefTab()->getNumSymRefs(), comp->trMemory(), heapAlloc, growable);
   }

void
OMR::SymbolReference::setUseDefAliases(TR_BitVector * bv)
   {
   _useDefAliases = bv;
   TR_ASSERT((!_symbol || !_symbol->isArrayShadowSymbol()) && !self()->isTempVariableSizeSymRef(),
             "should never call with an array shadow or a tempMemSlot");
   }

bool
OMR::SymbolReference::canCauseGC()
   {
   return self()->canGCandReturn() || self()->canGCandExcept();
   }

bool
OMR::SymbolReference::isLitPoolReference()
   {
   return self()->isLiteralPoolAddress() || self()->isFromLiteralPool();
   }

void
OMR::SymbolReference::setHasBeenAccessedAtRuntime(TR_YesNoMaybe ynm)
   {
   _flags.setValue(AccessedAtRuntimeMask, ynm * AccessedAtRuntimeBase);
   }

TR_YesNoMaybe
OMR::SymbolReference::hasBeenAccessedAtRuntime()
   {
   return (TR_YesNoMaybe)(_flags.getValue(AccessedAtRuntimeMask) / AccessedAtRuntimeBase);
   }

bool
OMR::SymbolReference::isOSRInductionHelper()
   {
   return _referenceNumber == TR_induceOSRAtCurrentPCAndRecompile || _referenceNumber == TR_induceOSRAtCurrentPC;
   }
