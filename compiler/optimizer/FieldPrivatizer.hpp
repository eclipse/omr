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

#ifndef FIELDPRIV_INCL
#define FIELDPRIV_INCL

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "il/SymbolReference.hpp"
#include "infra/HashTab.hpp"
#include "infra/List.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/IsolatedStoreElimination.hpp"
#include "optimizer/LoopCanonicalizer.hpp"

class TR_BitVector;
class TR_OpaqueClassBlock;
class TR_PostDominators;
class TR_RegisterCandidate;
class TR_Structure;
class TR_ValueNumberInfo;
namespace TR { class Block; }
namespace TR { class Optimization; }
namespace TR { class TreeTop; }

/*
 * Class TR_FieldPrivatizer
 * ========================
 *
 * Field privatizer aims to replace accesses to fields and statics inside loops
 * with accesses of new temps that it creates (allowing the loop to run faster
 * since the temp is an access from the stack, plus the temp gets to be a
 * candidate for global register allocation whereas the original field or static
 * would not be).

 * Field privatizer would attempt to replace loads as well as stores of fields
 * and statics and so if there are calls inside the loop or any other implicit
 * use points for the original field or static (e.g. an exception check,
 * that could fail and reach an exception handler that uses the field or static)
 * then the transformation cannot be done (since omitting stores to the field
 * or static inside the loop may change program behaviour at those use points
 * inside the loop). Assuming field privatizer finds a field or static without
 * any potential use inside the loop and subject to certain other conditions it
 * checks to keep the implementation simple, the transformation is to
 *
 * a) initialize the new temp it creates with the field or static value in
 * the loop preheader
 * b) change every load and store of the field or static inside the loop to
 * use the new temp
 * c) restore the field or static value on all exits out of the loop by using
 * the temp value on exit from the loop
 *
 * This optimization differs from what PRE (a form of load elimination as
 * part of the global commoning it does) may do in the same loop in that
 * field privatizer eliminates stores as well as loads, whereas PRE could
 * only have eliminated the loads of the field or static; PRE would not omit
 * the store to the field or static.
 *
 * As a very simple example...
 *
 * Original loop
 *
 * while (o.f < n)
 * o.f = o.f + 1;
 *
 * Field Privatizer optimized loop
 *
 * t = o.f;
 * while (t < n)
 * t = t + 1;
 * o.f = t;
 *
 * PRE optimized loop
 *
 * t = o.f;
 * while (t < n)
 * {
 * o.f = t + 1;
 * t = t + 1;
 * }
 *
 * So, what PRE would do to the loop is somewhat similar but still less powerful;
 * however PRE can do such optimizations even outside of loops and more importantly
 * can do exactly the same thing even if there were implicit uses of o.f inside
 * the loop. So it's more general in that sense.
 */

class TR_FieldPrivatizer : public TR_LoopTransformer
   {
   class TR_RemovedNode
      {
      public:
      TR_RemovedNode(TR::Node *storeNode, TR::Node *removedSign) :
                    _storeNode(storeNode), _removedSign(removedSign) {}

      TR::Node *_storeNode;
      TR::Node *_removedSign;
      };

   public:

   TR_FieldPrivatizer(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_FieldPrivatizer(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t);
   bool storesBackMustBePlacedInExitBlock(TR::Block *, TR::Block *, TR_BitVector *);
   void placeStoresBackInExits(List<TR::Block> *, List<TR::Block> *);
   void placeStoresBackInExit(TR::Block *, bool);
   void placeInitializersInLoopInvariantBlock(TR::Block *);
   bool subtreeIsInvariantInLoop(TR::Node *);
   bool bothSubtreesMatch(TR::Node *, TR::Node *);
   TR::SymbolReference *getPrivatizedFieldAutoSymRef(TR::Node *);
   void privatizeFields(TR::Node *, bool, vcount_t);
   void privatizeNonEscapingLoop(TR_Structure *, TR::Block *, vcount_t);
   void detectFieldsThatCannotBePrivatized(TR::Node *, vcount_t);
   void detectFieldsThatCannotBePrivatized(TR_Structure *, vcount_t);
   bool canPrivatizeFieldSymRef(TR::Node *);
   bool containsEscapePoints(TR_Structure *, bool &);
   void addPrivatizedRegisterCandidates(TR_Structure *);
   bool isStringPeephole(TR::Node *, TR::TreeTop *);
   void cleanupStringPeephole();
   void placeStringEpiloguesBackInExit(TR::Block *, bool);
   void placeStringEpilogueInExits(List<TR::Block> *, List<TR::Block> *);
   void addStringInitialization(TR::Block *);
   bool isFieldAliasAccessed(TR::SymbolReference * symRef);

   TR_BitVector *_privatizedFields, *_fieldsThatCannotBePrivatized;
   TR_BitVector *_needToStoreBack;
   List<TR::Node> _privatizedFieldNodes;
   TR_HashTabInt              _privatizedFieldSymRefs;
   List<TR_RegisterCandidate> _privatizedRegCandidates;
   TR::Block *_criticalEdgeBlock;
   TR::SymbolReference *_stringSymRef, *_valueOfSymRef, *_tempStringSymRef, *_appendSymRef, *_toStringSymRef, *_initSymRef;
   TR::TreeTop *_stringPeepholeTree;
   TR_OpaqueClassBlock *_stringBufferClass;
   TR_Structure *_currLoopStructure;
   int32_t _counter;
   List<TR::TreeTop> _appendCalls;

   TR_PostDominators *_postDominators;
   };


#endif
