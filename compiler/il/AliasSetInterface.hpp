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

#ifndef ALIASSETINTERFACE_INCL
#define ALIASSETINTERFACE_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"


template <AliasSetInterface _AliasSetInterface>
class TR_AliasSetInterface {
public:
   
   TR_AliasSetInterface(TR::SymbolReference *symRef, bool isDirectCall = false, bool includeGCSafePoint = false) :
      _isDirectCall(isDirectCall),
      _includeGCSafePoint(includeGCSafePoint),
      _symbolReference(symRef),
      _shares_symbol(true) {}

   TR_AliasSetInterface(bool shares_symbol, TR::SymbolReference *symRef, bool isDirectCall = false, bool includeGCSafePoint = false) :
      _isDirectCall(isDirectCall),
      _includeGCSafePoint(includeGCSafePoint),
      _symbolReference(symRef),
      _shares_symbol(shares_symbol) {}


   TR_BitVector *getTRAliases()
      {
      return getTRAliases_impl(_isDirectCall, _includeGCSafePoint);
      }

   template <class BitVector>
   bool
   getAliases(BitVector &aliases)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliases", comp->phaseTimer());
      return getAliasesWithClear(aliases);
      }

   bool
   getAliases(TR_BitVector &aliases)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliases_TR", comp->phaseTimer());
      return getAliasesWithClear(aliases);
      }

   bool
   getAliasesAndUnionWith(TR::SparseBitVector &aliases)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndUnionWith", comp->phaseTimer());
      TR::SparseBitVector tmp(comp->allocator());
      getAliasesWithClear(tmp);
      return aliases.Or(tmp);
      }

   bool
   getAliasesAndUnionWith(TR_BitVector &aliases)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndUnionWith_TR", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         aliases |= *bc_aliases;
      return (!aliases.isEmpty());
      }


   template <class BitVector> bool
   getAliasesAndSubtractFrom(BitVector &v)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndSubtractFrom", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         v.Andc(CS2_TR_BitVector(*bc_aliases));
      return (!v.IsZero());
      }

   template <class BitVector> bool
   getAliasesAndIntersectWith(BitVector &v)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndIntersectWith", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         v.And(CS2_TR_BitVector(*bc_aliases));
      return (!v.IsZero());
      }

   bool
   getAliasesAndSubtractFrom(TR_BitVector &bitvector)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndSubtractFrom_TR", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         bitvector -= *bc_aliases;
      return (!bitvector.isEmpty());
      }

   bool
   contains(TR::SymbolReference *symRef, TR::Compilation *comp)
      {
      return contains(symRef->getReferenceNumber(), comp);
      }

   bool
   contains(uint32_t refNum, TR::Compilation *comp)
      {
      LexicalTimer t("aliasesContains", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         return (bc_aliases->get(refNum) != 0);
      return false;
      }

   template <class BitVector> bool
   containsAny(const BitVector &refs, TR::Compilation *comp)
      {
      LexicalTimer t("aliasesContainsAny", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases == NULL)
         return false;
      return refs.Intersects(CS2_TR_BitVector(*bc_aliases));
      }

   void
   removeAliases(TR::SparseBitVector &aliasesToModify)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("removeAliases", comp->phaseTimer());
      setAliases_impl(aliasesToModify, false,  _isDirectCall, _includeGCSafePoint);
      }

   void
   addAliases(TR::SparseBitVector &aliasesToModify)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("addAliases", comp->phaseTimer());
      setAliases_impl(aliasesToModify, true,  _isDirectCall, _includeGCSafePoint);
      }

   void
   addAlias(TR::SymbolReference *symRef2)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("addAlias", comp->phaseTimer());
      setAlias(symRef2, true);
      }

   void
   removeAlias(TR::SymbolReference *symRef2)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("removeAlias", comp->phaseTimer());
      setAlias(symRef2, false);
      }

   bool containsAny(TR_BitVector& v2, TR::Compilation *comp)
      {
      LexicalTimer t("aliasesContainsAny_TR", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         return bc_aliases->intersects(v2);
      return false;
      }

   bool isZero(TR::Compilation *comp)
      {
      LexicalTimer t("isZero", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         return bc_aliases->isEmpty();
      return true;
      }

   bool hasAliases()
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("hasAliases", comp->phaseTimer());
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         return !bc_aliases->isEmpty() && bc_aliases->hasMoreThanOneElement();
      return false;
      }

   void
   setAlias(TR::SymbolReference *symRef2, bool value) {
      setAlias_impl(symRef2, value, _isDirectCall, _includeGCSafePoint);
   }

   template <class BitVector> bool
   getAliasesWithClear(BitVector &aliases)
      {
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         Assign(aliases, *bc_aliases, true);
      return (!aliases.IsZero());
      }

   bool
   getAliasesWithClear(TR_BitVector &aliases)
      {
      TR_BitVector *bc_aliases = getTRAliases();
      if (bc_aliases)
         aliases = *bc_aliases;
      return (!aliases.isEmpty());
      }

   bool _isDirectCall;
   bool _includeGCSafePoint;

   /*
    * SYMREF-BASED ALIASING
    */
public:
   TR_BitVector *getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint);

   void
   setAlias_impl(TR::SymbolReference *symRef2, bool value, bool isDirectCall, bool includeGCSafePoint);

   void
   setAliases_impl(TR::SparseBitVector &aliasesToModify, bool value, bool isDirectCall, bool includeGCSafePoint)
      {
      TR::Compilation *comp = TR::comp();
      static_assert(_AliasSetInterface == UseDefAliasSet || _AliasSetInterface ==  UseOnlyAliasSet, "failed: can only call setAliases_impl for UseOnly or useDef presently");

         {
         TR::SparseBitVector::Cursor aliasCursor(aliasesToModify);
         for (aliasCursor.SetToFirstOne(); aliasCursor.Valid(); aliasCursor.SetToNextOne())
            {
            uint32_t symRefNum = aliasCursor;
            TR::SymbolReference *symRef2 = comp->getSymRefTab()->getSymRef(aliasCursor);
            setAlias_impl(symRef2, value, isDirectCall, includeGCSafePoint);
            }
         }
      }

   void
   removeAllAliases()
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("removeAllAliases", comp->phaseTimer());
      static_assert(_AliasSetInterface == UseDefAliasSet || _AliasSetInterface ==  UseOnlyAliasSet, "failed: can only call removeAllAliases for UseOnly or useDef presently");

      }
private:
   static void removeSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint);
   static void addSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint);
   static void setSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint, bool value);

   TR::SymbolReference *_symbolReference;
   //shows if the _symbolReference can have alias. Could only be false when called from Node::mayKill() function
   bool _shares_symbol;
};

/*
 * Interface to initialize TR_AliasSetInterface
 */
class TR_UseDefAliasSetInterface : public TR_AliasSetInterface<UseDefAliasSet> {
   public:
  TR_UseDefAliasSetInterface(TR::SymbolReference *symRef,
                             bool isDirectCall = false,
                             bool includeGCSafePoint = false) :
  TR_AliasSetInterface<UseDefAliasSet>
    (symRef, isDirectCall, includeGCSafePoint) {}

   TR_UseDefAliasSetInterface(bool shares_symbol,
                              TR::SymbolReference *symRef,
                              bool isDirectCall = false,
                              bool includeGCSafePoint = false) :
  TR_AliasSetInterface<UseDefAliasSet>
    (shares_symbol, symRef, isDirectCall, includeGCSafePoint) {}
};

class TR_UseOnlyAliasSetInterface: public TR_AliasSetInterface<UseOnlyAliasSet> {
   public:
  TR_UseOnlyAliasSetInterface(TR::SymbolReference *symRef,
                              bool isDirectCall = false,
                              bool includeGCSafePoint = false) :
  TR_AliasSetInterface<UseOnlyAliasSet>
    (symRef, isDirectCall, includeGCSafePoint) {}
};

/*
 * Member function definitions of class TR_AliasSetInterface
 */
template <> inline
TR_BitVector *TR_AliasSetInterface<UseDefAliasSet>::getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint)
   {
   if(_symbolReference)
      {
      if(_shares_symbol)
         return _symbolReference->getUseDefAliasesBV(isDirectCall, includeGCSafePoint);

      else
         {
         //if symbol refereance cannot have alias 
         TR::Compilation *comp = TR::comp();
         TR_BitVector *bv = NULL;

         bv = new (comp->aliasRegion()) TR_BitVector(comp->getSymRefCount(), comp->aliasRegion(), growable);
         bv->set(_symbolReference->getReferenceNumber());

         return bv;
         }
      }
   else
      //if there is no symbol reference associated with the node, we return an empty bit container
      return NULL;
   }

template <> inline
TR_BitVector *TR_AliasSetInterface<UseOnlyAliasSet>::getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint)
   {      
   if(!_symbolReference)
      //if there is no symbol reference, then we return an empty bit container
      return NULL;
      
   return _symbolReference->getUseonlyAliasesBV(TR::comp()->getSymRefTab());
   }
template <AliasSetInterface _AliasSetInterface> inline
void TR_AliasSetInterface<_AliasSetInterface>::setAlias_impl(TR::SymbolReference *symRef2, bool value, bool isDirectCall, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   if (symRef2 == NULL)
      return;

   static_assert(_AliasSetInterface == UseDefAliasSet || _AliasSetInterface ==  UseOnlyAliasSet, "failed: can only call setAlias_impl for UseOnly or useDef presently");

      {
      if (_AliasSetInterface == UseDefAliasSet)
         {
         // carry out symmetrically
         setSymRef1KillsSymRef2Asymmetrically(_symbolReference, symRef2, includeGCSafePoint, value);

         if (!_symbolReference->getSymbol()->isMethod() && !symRef2->getSymbol()->isMethod())
            {
            // symmetric, complete symmetry
            setSymRef1KillsSymRef2Asymmetrically(symRef2, _symbolReference, includeGCSafePoint, value);
            }
         }
      else if (_AliasSetInterface == UseOnlyAliasSet)
         {
         // carry out asymmetrically
         if (_symbolReference->getSymbol()->isMethod() && !symRef2->getSymbol()->isMethod())
            { // foo uses x, same as x kills foo
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         else if (!_symbolReference->getSymbol()->isMethod() && symRef2->getSymbol()->isMethod())
            { // x uses foo, same as foo kills x
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         else if (!_symbolReference->getSymbol()->isMethod() && !symRef2->getSymbol()->isMethod())
            { // x uses y, same as y kills x. Its symmetric
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         else
            { // foo uses bar, bar kills foo
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         }
      }
   }

template <AliasSetInterface _AliasSetInterface> inline
void TR_AliasSetInterface<_AliasSetInterface>::removeSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint)
   {
   TR_BitVector *symRef1_killedAliases = symRef1->getUseDefAliases(includeGCSafePoint).getTRAliases();
   TR_BitVector *symRef2_useAliases = symRef2->getUseonlyAliases().getTRAliases();

   if ((symRef1_killedAliases == NULL) || (symRef2_useAliases == NULL))
      return;

   if (symRef1_killedAliases && symRef1_killedAliases->isSet(symRef2->getReferenceNumber()))
      symRef1_killedAliases->reset(symRef2->getReferenceNumber());

   if (symRef2_useAliases && symRef2_useAliases->isSet(symRef1->getReferenceNumber()))
      symRef2_useAliases->reset(symRef1->getReferenceNumber());
   }

template <AliasSetInterface _AliasSetInterface> inline
void TR_AliasSetInterface<_AliasSetInterface>::addSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   TR_BitVector *symRef1_killedAliases = symRef1->getUseDefAliases(includeGCSafePoint).getTRAliases();

   if (symRef1_killedAliases != NULL)
      {
      if (!symRef1_killedAliases->isSet(symRef2->getReferenceNumber()))
         symRef1_killedAliases->set(symRef2->getReferenceNumber());
      }

   TR_BitVector *symRef2_useAliases = symRef2->getUseonlyAliases().getTRAliases();
   if (symRef2_useAliases != NULL)
      {
      if (!symRef2_useAliases->isSet(symRef1->getReferenceNumber()))
         symRef2_useAliases->set(symRef1->getReferenceNumber());
      }
   }

template <AliasSetInterface _AliasSetInterface> inline
void TR_AliasSetInterface<_AliasSetInterface>::setSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint, bool value)
   {
   TR::Compilation *comp = TR::comp();
   if ((symRef1 == NULL) || (symRef2 == NULL))
      return;

   if (value)
      addSymRef1KillsSymRef2Asymmetrically(symRef1, symRef2, includeGCSafePoint);
   else
      removeSymRef1KillsSymRef2Asymmetrically(symRef1, symRef2, includeGCSafePoint);
   }

template <class T>
void CountUseDefAliases( T& t, const TR::SparseBitVector &syms)
   {
   TR::Compilation *comp = TR::comp();
   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();

      {
      TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());
      TR_BitVector *tr_syms = new (comp->trStackMemory()) TR_BitVector(syms.PopulationCount(), comp->trMemory(), stackAlloc, growable);
      *tr_syms = syms;

      typename T::Cursor c(t);
      for (c.SetToFirst(); c.Valid(); c.SetToNext())
         {
         TR::SymbolReference *symRef = symRefTab->getSymRef(t.KeyAt(c));
         if (symRef && symRef->sharesSymbol())
            {
            TR_BitVector *bv = symRef->getUseDefAliases().getTRAliases();
            if (bv)
               t[c] = bv->commonElementCount(*tr_syms);
            else
               t[c] = 0;
            }
         else
            t[c] = 0;
         }
      }
   }


/*
 * Interface to initialize TR_AliasSetInterface from OMR::SymbolReference
 * (Member function definitions of class OMR::SymbolReference)
 */
inline TR_UseOnlyAliasSetInterface
OMR::SymbolReference::getUseonlyAliases()
   {
   TR_UseOnlyAliasSetInterface aliasSetInterface(self());
   return aliasSetInterface;
   }

inline TR_UseDefAliasSetInterface
OMR::SymbolReference::getUseDefAliases(bool isDirectCall, bool includeGCSafePoint)
   {
   TR_UseDefAliasSetInterface aliasSetInterface(self(), isDirectCall, includeGCSafePoint);
   return aliasSetInterface;
   }

inline TR_UseDefAliasSetInterface
OMR::SymbolReference::getUseDefAliasesIncludingGCSafePoint(bool isDirectCall)
   {
   TR_UseDefAliasSetInterface aliasSetInterface(self(), isDirectCall, true);
   return aliasSetInterface;
   }

#endif
