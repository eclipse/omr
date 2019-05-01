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

#ifndef ALIASSETINTERFACE_INCL
#define ALIASSETINTERFACE_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "cs2/sparsrbit.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"


template <class AliasSetInterface>
class TR_AliasSetInterface {
public:

   TR_AliasSetInterface(bool isDirectCall = false, bool includeGCSafePoint = false) :
      _isDirectCall(isDirectCall),
      _includeGCSafePoint(includeGCSafePoint)
      {}

   TR_BitVector *getTRAliases()
      {
      return static_cast<AliasSetInterface*>(this)->getTRAliases_impl(_isDirectCall, _includeGCSafePoint);
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
      static_cast<AliasSetInterface*>(this)->setAliases_impl(aliasesToModify, false,  _isDirectCall, _includeGCSafePoint);
      }

   void
   addAliases(TR::SparseBitVector &aliasesToModify)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("addAliases", comp->phaseTimer());
      static_cast<AliasSetInterface*>(this)->setAliases_impl(aliasesToModify, true,  _isDirectCall, _includeGCSafePoint);
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
     static_cast<AliasSetInterface*>(this)->setAlias_impl(symRef2, value, _isDirectCall, _includeGCSafePoint);
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
   };

////////////////// SYMREF-BASED ALIASING

typedef enum {
   useDefAliasSet,
   UseOnlyAliasSet
} TR_AliasSetType;

template <uint32_t _aliasSetType>
class TR_SymAliasSetInterface : public TR_AliasSetInterface<TR_SymAliasSetInterface<_aliasSetType> > {
public:
  TR_SymAliasSetInterface(TR::SymbolReference *symRef, bool isDirectCall = false, bool includeGCSafePoint = false) :
    TR_AliasSetInterface<TR_SymAliasSetInterface<_aliasSetType> >(isDirectCall, includeGCSafePoint),
    _symbolReference(symRef) {}

   TR_BitVector *getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint);

   void
   setAlias_impl(TR::SymbolReference *symRef2, bool value, bool isDirectCall, bool includeGCSafePoint);

   void
   setAliases_impl(TR::SparseBitVector &aliasesToModify, bool value, bool isDirectCall, bool includeGCSafePoint)
      {
      TR::Compilation *comp = TR::comp();
      TR_ASSERT(_aliasSetType == useDefAliasSet || _aliasSetType ==  UseOnlyAliasSet, "failed: can only call setAliases_impl for UseOnly or useDef presently");

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
      TR_ASSERT(_aliasSetType == useDefAliasSet || _aliasSetType ==  UseOnlyAliasSet, "failed: can only call removeAllAliases for UseOnly or useDef presently");

      }

private:
    static void removeSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint);

    static void addSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint);

    static void setSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint, bool value);

  TR::SymbolReference *_symbolReference;
};

struct TR_UseDefAliasSetInterface : public TR_SymAliasSetInterface<useDefAliasSet> {
  TR_UseDefAliasSetInterface(TR::SymbolReference *symRef,
                             bool isDirectCall = false,
                             bool includeGCSafePoint = false) :
  TR_SymAliasSetInterface<useDefAliasSet>
    (symRef, isDirectCall, includeGCSafePoint) {}
};

struct TR_UseOnlyAliasSetInterface: public TR_SymAliasSetInterface<UseOnlyAliasSet> {
  TR_UseOnlyAliasSetInterface(TR::SymbolReference *symRef,
                              bool isDirectCall = false,
                              bool includeGCSafePoint = false) :
  TR_SymAliasSetInterface<UseOnlyAliasSet>
    (symRef, isDirectCall, includeGCSafePoint) {}
};

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::setAlias_impl(TR::SymbolReference *symRef2, bool value, bool isDirectCall, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   if (symRef2 == NULL)
      return;

   TR_ASSERT(_aliasSetType == useDefAliasSet || _aliasSetType ==  UseOnlyAliasSet, "failed: can only call setAlias_impl for UseOnly or useDef presently");

      {
      if (_aliasSetType == useDefAliasSet)
         {
         // carry out symmetrically
         setSymRef1KillsSymRef2Asymmetrically(_symbolReference, symRef2, includeGCSafePoint, value);

         if (!_symbolReference->getSymbol()->isMethod() && !symRef2->getSymbol()->isMethod())
            {
            // symmetric, complete symmetry
            setSymRef1KillsSymRef2Asymmetrically(symRef2, _symbolReference, includeGCSafePoint, value);
            }
         }
      else if (_aliasSetType == UseOnlyAliasSet)
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

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::removeSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint)
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

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::addSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint)
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

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::setSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint, bool value)
   {
   TR::Compilation *comp = TR::comp();
   if ((symRef1 == NULL) || (symRef2 == NULL))
      return;

   if (value)
      addSymRef1KillsSymRef2Asymmetrically(symRef1, symRef2, includeGCSafePoint);
   else
      removeSymRef1KillsSymRef2Asymmetrically(symRef1, symRef2, includeGCSafePoint);
   }

template <> inline
TR_BitVector *TR_SymAliasSetInterface<useDefAliasSet>::getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint)
   {
   return _symbolReference->getUseDefAliasesBV(isDirectCall, includeGCSafePoint);
   }

template <> inline
TR_BitVector *TR_SymAliasSetInterface<UseOnlyAliasSet>::getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint)
   {
   return _symbolReference->getUseonlyAliasesBV(TR::comp()->getSymRefTab());
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


////NODE ALIASING

typedef enum {
   mayUseAliasSet,
   mayKillAliasSet
} TR_NodeAliasSetType;

template <uint32_t _aliasSetType>
class TR_NodeAliasSetInterface : public TR_AliasSetInterface<TR_NodeAliasSetInterface<_aliasSetType> > {
public:
   TR_NodeAliasSetInterface(TR::Node *node, bool isDirectCall = false, bool includeGCSafePoint = false) :
      TR_AliasSetInterface<TR_NodeAliasSetInterface<_aliasSetType> >(isDirectCall, includeGCSafePoint),
        _node(node) {}

   TR_BitVector *getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint);

private:
  TR::Node            *_node;
};

template<> inline
TR_BitVector *TR_NodeAliasSetInterface<mayUseAliasSet>::getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   TR_BitVector *bv = NULL;

   if (_node->getOpCode().isLikeUse() && _node->getOpCode().hasSymbolReference())
      bv = _node->getSymbolReference()->getUseonlyAliasesBV(comp->getSymRefTab());
   // else there is no symbol reference associated with the node, we return an empty bit container

   return bv;
   }

template<> inline
TR_BitVector *TR_NodeAliasSetInterface<mayKillAliasSet>::getTRAliases_impl(bool isDirectCall, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   TR_BitVector *bv = NULL;

   if (_node->getOpCode().hasSymbolReference() && (_node->getOpCode().isLikeDef() || _node->mightHaveVolatileSymbolReference())) //we want the old behavior in these cases
      {
      if (_node->getSymbolReference()->sharesSymbol(includeGCSafePoint))
         bv = _node->getSymbolReference()->getUseDefAliasesBV(isDirectCall, includeGCSafePoint);
      else
         {
         bv = new (comp->aliasRegion()) TR_BitVector(comp->getSymRefCount(), comp->aliasRegion(), growable);
         bv->set(_node->getSymbolReference()->getReferenceNumber());
         }
      }

   // else there is no symbol reference associated with the node, we return an empty bit container
   return bv;
   }

struct TR_NodeUseAliasSetInterface: public TR_NodeAliasSetInterface<mayUseAliasSet> {
  TR_NodeUseAliasSetInterface(TR::Node *node,
                              bool isDirectCall = false,
                              bool includeGCSafePoint = false) :
  TR_NodeAliasSetInterface<mayUseAliasSet>
    (node, isDirectCall, includeGCSafePoint) {}
};

struct TR_NodeKillAliasSetInterface: public TR_NodeAliasSetInterface<mayKillAliasSet> {
  TR_NodeKillAliasSetInterface(TR::Node *node,
                               bool isDirectCall = false,
                               bool includeGCSafePoint = false) :
    TR_NodeAliasSetInterface<mayKillAliasSet>
     (node, isDirectCall, includeGCSafePoint) {}
};


///////////////////////////////////////


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
