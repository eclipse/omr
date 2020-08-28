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

#include "optimizer/ValuePropagation.hpp"
#include "optimizer/GlobalValuePropagation.hpp"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/Reachability.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/ClassTableCriticalSection.hpp"
#include "env/CHTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"
#endif


#define OPT_DETAILS "O^O VALUE PROPAGATION: "
#define NEED_WRITE_BARRIER 1
#define NEED_ARRAYSTORE_CHECK 2
#define NEED_ARRAYSTORE_CHECK_AND_WRITE_BARRIER 3
#define STRING_ARRAY_SIZE 100

extern TR::TreeTop *createStoresForArraycopyChildren(TR::Compilation *comp, TR::TreeTop *arrayTree, TR::SymbolReference * &srcObjRef, TR::SymbolReference * &dstObjRef, TR::SymbolReference * &srcRef, TR::SymbolReference * &dstRef, TR::SymbolReference * &lenRef);

void collectArraylengthNodes(TR::Node *node, vcount_t visitCount, List<TR::Node> *arraylengthNodes)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isArrayLength())
      {
      arraylengthNodes->add(node);
      }

   int32_t childNum;
   for (childNum=0;childNum < node->getNumChildren();childNum++)
      collectArraylengthNodes(node->getChild(childNum), visitCount, arraylengthNodes);
   }

OMR::ValuePropagation::Relationship *OMR::ValuePropagation::createRelationship(int32_t relative, TR::VPConstraint *constraint)
   {
   Relationship *rel = _relationshipCache.pop();
   if (!rel)
      rel = new (trStackMemory()) Relationship;
   rel->relative = relative;
   rel->constraint = constraint;
   rel->setNext(NULL);
   return rel;
   }

void OMR::ValuePropagation::freeRelationship(Relationship *rel)
   {
   _relationshipCache.add(rel);
   }

void OMR::ValuePropagation::freeRelationships(TR_LinkHead<Relationship> &list)
   {
   Relationship *cur, *next;
   for (cur = list.getFirst(); cur; cur = next)
      {
      next = cur->getNext();
      freeRelationship(cur);
      }
   list.setFirst(NULL);
   }

OMR::ValuePropagation::StoreRelationship *OMR::ValuePropagation::createStoreRelationship(TR::Symbol *symbol, Relationship *firstRel)
   {
   StoreRelationship *rel = _storeRelationshipCache.pop();
   if (!rel)
      rel = new (trStackMemory()) StoreRelationship;
   rel->symbol = symbol;
   rel->relationships.setFirst(firstRel);
   rel->setNext(NULL);
   return rel;
   }

void OMR::ValuePropagation::freeStoreRelationship(StoreRelationship *rel)
   {
   freeRelationships(rel->relationships);
   _storeRelationshipCache.add(rel);
   }

void OMR::ValuePropagation::freeStoreRelationships(TR_LinkHead<StoreRelationship> &list)
   {
   StoreRelationship *cur, *next;
   for (cur = list.getFirst(); cur; cur = next)
      {
      next = cur->getNext();
      freeStoreRelationship(cur);
      }
   list.setFirst(NULL);
   }

void OMR::ValuePropagation::freeValueConstraints(ValueConstraints &valueConstraints)
   {
   _vcHandler.empty(valueConstraints);
   }

OMR::ValuePropagation::ValueConstraint *OMR::ValuePropagation::copyValueConstraints(ValueConstraints &valueConstraints)
   {
   return _vcHandler.copyAll(valueConstraints);
   }

void OMR::ValuePropagation::addConstraint(TR::VPConstraint *constraint, int32_t hash)
   {
   ConstraintsHashTableEntry *entry = new (trStackMemory()) ConstraintsHashTableEntry;
   entry->constraint = constraint;
   entry->next = _constraintsHashTable[hash];
   _constraintsHashTable[hash] = entry;
   }

void OMR::ValuePropagation::addLoopDef(TR::Node *node)
   {
   // If the loop def entry does not already exist, create it
   //
   int32_t hash = (((uintptr_t)node) >> 2) % VP_HASH_TABLE_SIZE;
   LoopDefsHashTableEntry *entry;
   for (entry = _loopDefsHashTable[hash]; entry; entry = entry->next)
      {
      if (entry->node == node)
         return;
      }
   entry = new (trStackMemory()) LoopDefsHashTableEntry;
   entry->node = node;
   entry->region = NULL;
   entry->next = _loopDefsHashTable[hash];
   _loopDefsHashTable[hash] = entry;
   }

OMR::ValuePropagation::LoopDefsHashTableEntry *OMR::ValuePropagation::findLoopDef(TR::Node *node)
   {
   // Find the loop def entry
   //
   int32_t hash = (((uintptr_t)node) >> 2) % VP_HASH_TABLE_SIZE;
   LoopDefsHashTableEntry *entry;
   for (entry = _loopDefsHashTable[hash]; entry; entry = entry->next)
      {
      if (entry->node == node)
         return entry;
      }
   return NULL;
   }


static bool worthPropagatingConstraints(TR::Compilation *comp, bool isGlobalPropagation)
   {
   if ((!isGlobalPropagation && (comp->getMethodHotness() > cold)) || (comp->getMethodHotness() > warm))
      return true;

   return false;
   }



bool OMR::ValuePropagation::propagateConstraint(TR::Node *node, int32_t valueNumber, Relationship *first, Relationship *rel, ValueConstraints *valueConstraints)
   {
   // Go through the relationships list for this value number and propagate the
   // effects of the given new relationship to the others.
   //
   // Return true if the propagation was successful, false if constraints could
   // not be intersected.
   //
   // Make sure the propagation recursion is not getting too deep
   //
   _propagationDepth++;
   if (_propagationDepth > _maxPropagationDepth)
      {
      _reachedMaxRelationDepth = true;
      if (trace())
         traceMsg(comp(), "===>Reached Max Relational Propagation Depth: %d\n", _propagationDepth);
      ///TR_ASSERT(0, "Reached max relation depth %p\n", _propagationDepth);
      }


   TR::VPConstraint *c;
   Relationship *cur = first;
   for (cur = first; cur && worthPropagatingConstraints(comp(), _isGlobalPropagation); cur = cur->getNext())
      {
      if (cur->relative == rel->relative)
         continue;

      if (rel->relative == AbsoluteConstraint)
         {
         // Propagate new absolute constraint to existing relative constraint
         //
         TR_ASSERT(cur->constraint->asRelation(), "assertion failure");
         c = cur->constraint->asRelation()->propagateAbsoluteConstraint(rel->constraint, cur->relative, this);
         if (c)
            {
            c = addConstraintToList(node, cur->relative, AbsoluteConstraint, c, valueConstraints);
            if (!c)
               return false;
            }
         }
      else if (cur->relative == AbsoluteConstraint)
         {
         // Propagate existing absolute constraint to new relative constraint
         //
         TR_ASSERT(rel->constraint->asRelation(), "assertion failure");
         c = rel->constraint->asRelation()->propagateAbsoluteConstraint(cur->constraint, rel->relative, this);
         if (c)
            {
            c = addConstraintToList(node, rel->relative, AbsoluteConstraint, c, valueConstraints);
            if (!c)
               return false;
            }
         }
      else
         {
         // Propagate relative constraint to others
         //
         TR_ASSERT(cur->constraint->asRelation(), "assertion failure");
         TR_ASSERT(rel->constraint->asRelation(), "assertion failure");
         c = cur->constraint->asRelation()->propagateRelativeConstraint(rel->constraint->asRelation(), cur->relative, rel->relative, this);
         if (c)
            {
            c = addConstraintToList(node, cur->relative, rel->relative, c, valueConstraints);
            if (!c)
               return false;
            }
         }
      }

   // If this is a relative constraint, propagate the same relationship to the
   // relative.
   //
   if (rel->relative != AbsoluteConstraint)
      {
      TR_ASSERT(rel->constraint->asRelation(), "assertion failure");
      if (!addConstraintToList(node, rel->relative, valueNumber, rel->constraint->asRelation()->getComplement(this), valueConstraints))
         return false;
      }

   _propagationDepth--;
   return true;
   }

// Add or modify a relationship for a value number in a value constraint list.
// If the list is null, add the relationship as a global constraint on the value
// number.
// Return the relationship if it is new or changed from the existing one.
//
TR::VPConstraint *OMR::ValuePropagation::addConstraintToList(TR::Node *node, int32_t valueNumber, int32_t relative, TR::VPConstraint *constraint, ValueConstraints *valueConstraints, bool replaceExisting)
   {
   // If we are really adding a global constraint, go do that.
   //
   if (!valueConstraints)
      {
      TR_ASSERT(!replaceExisting, "assertion failure");
      return addGlobalConstraint(node, valueNumber, constraint, relative);
      }

   TR::VPConstraint *c = NULL;
   Relationship    *rel = NULL, *prevRel = NULL;
   bool             newOrChanged = false;
   TR::VPConstraint *existingGlobalConstraint = NULL;

   // Apply global constraints to the new constraint.
   // If the new constraint is the same as an existing global constraint there
   // is nothing to do.
   //
   GlobalConstraint *gc = findGlobalConstraint(valueNumber);
   if (gc)
      {
      for (rel = gc->constraints.getFirst(); rel; rel = rel->getNext())
         {
         if (rel->relative == relative)
            {
            c = constraint->intersect(rel->constraint, this);
            // FIXME:the code below will propagate relative block constraints to
            // the global constraints list; but does not yet work correctly.
            // commented for now.
#if 0
            if (c == NULL)
               return c;
            else if (c == rel->constraint)
               {
               if (relative == AbsoluteConstraint)
                  return c;
               else
                  break;
               }
#else
            if (c == rel->constraint || c == NULL)
               {
               // intersection failed
               if (c == NULL && removeConstraints())
                  {
                  ///setIntersectionFailed(true);
                  ///removeConstraints(valueNumber, NULL);
                  }

               if (c != NULL)
                  {
                  existingGlobalConstraint = c;
                  constraint = c;
                  break;
                  }
               else
                  return c;
               }
#endif
            constraint = c;
            break;
            }
         }
      }

   // Find or insert the value constraint
   //
   ValueConstraint *cur = _vcHandler.find(valueNumber, *valueConstraints);

   // Find the relationship. The list is ordered by relative value number.
   //
   if (!existingGlobalConstraint)
      {
      if (!cur)
         cur = _vcHandler.findOrCreate(valueNumber, *valueConstraints);

      // Find the relationship. The list is ordered by relative value number.
      //
      int32_t numRelatives = 0;
      for (rel = cur->relationships.getFirst(), prevRel = NULL; rel && rel->relative < relative; prevRel = rel, rel = rel->getNext())
         { numRelatives++; }

      // If the relationship does not exist, we must have traversed the whole list
      // Make sure that our lists are not getting too long -
      //
      static const char *p = feGetEnv("TR_VPMaxRelDepth");
      static const int32_t maxRelDepth = p ? atoi(p) : 64;
      if (!rel && numRelatives > maxRelDepth)
         {
         _reachedMaxRelationDepth = true;
         if (trace())
            traceMsg(comp(), "===>Reached Max Relational Propagation Depth: %d\n", numRelatives);
         }

      // If the relationship does not exist, create it.
      //
      if (!rel || rel->relative > relative)
         {
         rel = createRelationship(relative, constraint);
         cur->relationships.insertAfter(prevRel, rel);
         newOrChanged = true;
         c = constraint;
         }

      // If the relationship already exists see if it can be further constrained
      // by the new constraint
      //
      else
         {
         c = replaceExisting ? constraint : constraint->intersect(rel->constraint, this);

         if (!c)
            {
            // intersection failed
            if (removeConstraints())
              {
               ///setIntersectionFailed(true);
               ////removeConstraints(valueNumber, valueConstraints);
              }
            return NULL; // can't intersect the constraints
            }

         if (c != rel->constraint)
            {
            // Replace the constraint with the intersected value
            //
            rel->constraint = c;
            newOrChanged = true;
            }
         }
      }
   // If there are store relationships corresponding to the one being added
   // or changed, apply the new constraint to them too.
   //
   StoreRelationship *store;
   if (cur)
      {
      for (store = cur->storeRelationships.getFirst(); store; store = store->getNext())
         {
         if (isUnreachableStore(store))
            continue;

         TR::VPConstraint *storeConstraint = NULL;
         Relationship *storeRel, *prev;
         TR::Symbol *storeSym = store->symbol;
         // don't update store constraints for symbols
         // that don't match
         //
         bool updateStoreConstraint = true;
         if (node && node->getOpCode().hasSymbolReference() &&
            (node->getSymbol() != storeSym))
         updateStoreConstraint = false;


         if (updateStoreConstraint)
            {
            for (storeRel = store->relationships.getFirst(), prev = NULL; storeRel; prev = storeRel, storeRel = storeRel->getNext())
               {
                if (storeRel->relative > relative)
                  {
                  storeRel = NULL;
                  break;
                  }
                else if (storeRel->relative == relative)
                  {
                  storeConstraint = constraint->intersect(storeRel->constraint, this);
                  break;
                  }
               }
               if (!storeRel)
                  {
                  storeRel = createRelationship(relative, constraint);
                  store->relationships.insertAfter(prev, storeRel);
                  // check for the special class constraint
                  //
                  ///storeConstraint = constraint->intersect(constraint, this);
                  //
                  //FIXME: ugly, uncomment line above and comment this 'if' block
                  if (TR::VPConstraint::isSpecialClass((uintptr_t)constraint->getClass()))
                     {
                     TR_ASSERT(constraint->asClass(), "special class constraint must be VPClass");
                     traceMsg(comp(), "found special class constraint!\n");
                     // remove the type information
                     storeConstraint = TR::VPClass::create(this, NULL, constraint->getClassPresence(), constraint->getPreexistence(), constraint->getArrayInfo(), constraint->getObjectLocation());
                     }
                   else
                     storeConstraint = constraint;
                   }

               TR_ASSERT(!replaceExisting, "assertion failure");
               if (storeConstraint)
                  {
                  if (storeConstraint != storeRel->constraint)
                     {
                     storeRel->constraint = storeConstraint;
                     if (trace() && node)
                        {
                        traceMsg(comp(), "   %s [%p] gets new store constraint:", node->getOpCode().getName(), node);
                        storeRel->print(this, valueNumber, 1);
                        }
                     }
                  }
               else
                  {
                  // This store value is unreachable on this path
                  //
                  //FIXME: this is aggressive and does not handle
                  //stores in loops. this snippet of code is only
                  //correct for acyclic regions.
                  ///if (lastTimeThrough())
                  ///   setUnreachableStore(store);
                  }
               } // updateStoreConstraint
            } // store = cur->storeRelationships.getFirst(); store; store = store->getNext()
         } // if (cur)

   if (existingGlobalConstraint)
      return existingGlobalConstraint;

   if (newOrChanged)
      {
      if (trace() && node)
         {
         traceMsg(comp(), "   n%in %s gets new constraint:", node->getGlobalIndex(), node->getOpCode().getName());
         rel->print(this, valueNumber, 1);
         traceMsg(comp(), "type of constraint - longConstraint: %p, intConstraint: %p, shortConstraint: %p\n", rel->constraint->asLongConstraint(), rel->constraint->asIntConstraint(), rel->constraint->asShortConstraint());
         }

      // Propagate the effects of the constraint to other constraints
      // associated with this value number
      //
      if (valueNumber < _firstUnresolvedSymbolValueNumber)
         {
         TR_ASSERT(!replaceExisting, "should be merging real value numbers");
         bool propFailed = false;
         if (!propagateConstraint(node, valueNumber, cur->relationships.getFirst(), rel, valueConstraints))
            propFailed = true;

         static const bool relPropGlobal = feGetEnv("TR_enableVPRelPropGlobal") != NULL;
         if (relPropGlobal)
            {
            // Propagate according to any global relative constraints as well,
            // but keeping the resulting constraints local in valueConstraints.
            GlobalConstraint *globalConstraint = findGlobalConstraint(valueNumber);
            if (!propFailed && globalConstraint != NULL)
               {
               auto firstReln = globalConstraint->constraints.getFirst();
               if (!propagateConstraint(node, valueNumber, firstReln, rel, valueConstraints))
                  propFailed = true;
               }
            }

         if (propFailed)
            {
            // propagation failed
            if (removeConstraints())
               {
               ////setIntersectionFailed(true);
               ////removeConstraints(valueNumber, valueConstraints);
               }
            // reset the propagationDepth here
            //
            _propagationDepth = 0;
            return NULL;
            }
         }
      }

   return c;
   }

TR::VPConstraint *OMR::ValuePropagation::addGlobalConstraint(TR::Node *node, TR::VPConstraint *constraint, TR::Node *relative)
   {
   // No global constraints for local value propagation
   //
   if (!_isGlobalPropagation)
      return addBlockConstraint(node, constraint, relative);
   int32_t relativeVN  = relative ? getValueNumber(relative) : AbsoluteConstraint;
   return addGlobalConstraint(node, getValueNumber(node), constraint, relativeVN);
   }

TR::VPConstraint *OMR::ValuePropagation::addGlobalConstraint(TR::Node *node, int32_t valueNumber, TR::VPConstraint *constraint, int32_t relative)
   {
   TR_ASSERT(_isGlobalPropagation, "Local VP can't add global constraint");

   // Add the relationship into the relationship list for the value
   //
   GlobalConstraint *gc = findGlobalConstraint(valueNumber);
   if (!gc)
      gc = createGlobalConstraint(valueNumber);

   int32_t numRelatives = 0;
   Relationship *rel, *prev;
   for (rel = gc->constraints.getFirst(), prev = NULL;
        rel && rel->relative < relative;
        prev = rel, rel = rel->getNext())
      { numRelatives++; }

   bool newOrChanged = false;

   static const char *p = feGetEnv("TR_VPMaxRelDepth");
   static const int32_t maxRelDepth = p ? atoi(p) : 64;
   if (!rel && numRelatives > maxRelDepth)
      {
      _reachedMaxRelationDepth = true;
      if (trace())
         traceMsg(comp(), "===>Reached Max Relational Propagation Depth: %d\n", numRelatives);
      }

   if (!rel || rel->relative > relative)
      {
      rel = createRelationship(relative, constraint);
      gc->constraints.insertAfter(prev, rel);
      newOrChanged = true;
      }

   // See if the constraint can be further constrained by the new constraint
   //
   // When we finally fix ValuePropagation to fully support unsigned
   // arithmetic, we can get rid of the c in the assume and following if)
   TR::VPConstraint *c = constraint->intersect(rel->constraint, this);
   // enable this code now since unsigned types are supported in VP
   //TR_ASSERT(c, "Cannot intersect constraints");

   // intersection failed
   if (!c && removeConstraints())
      {
      ////setIntersectionFailed(true);
      ////removeConstraints(valueNumber, NULL);
      return c;
      }

   if (trace() && c == NULL)
      traceMsg(comp(), "Cannot intersect constraints on %s [%p]", node->getOpCode().getName(), node);
   // The assume below is informational in nature and should therefore not be triggered if production builds include assumes (as on WCode or binopt).
   // This condition should rarely show up in WCode or binopt (as Simplifier transformations will homogenize the types) but can happen, for example, when an aggregate symbol is
   // referenced in one place on an oload and another as an iload. MG: Update assert?
   TR_ASSERT(c, "Cannot intersect constraints on %s [%p]", node->getOpCode().getName(), node);

   if (c && c != rel->constraint)
      {
      // Replace the constraint with the intersected value
      //
      rel->constraint = c;
      newOrChanged = true;
      }

   if (newOrChanged)
      {
      if (trace() && node)
         {
         traceMsg(comp(), "   %s [%p] gets new global constraint:", node->getOpCode().getName(), node);
         rel->print(this, valueNumber, 1);
         }

      // Propagate the effects of the constraint to other constraints
      // associated with this value number
      //
      bool propagateOK = propagateConstraint(node, valueNumber, gc->constraints.getFirst(), rel, NULL);

      // propagation of constraints failed
      if (!propagateOK)
         {
         if (removeConstraints())
            {
            ////setIntersectionFailed(true);
            ///removeConstraints(valueNumber, NULL);
            return c;
            }
         // reset the propagationDepth here
         //
         _propagationDepth = 0;
         }

      TR_ASSERT(propagateOK, "Cannot intersect constraints");
      }
   else if (c)
      {
      if (trace() && node)
         {
         traceMsg(comp(), "   %s [%p] found existing global constraint value number %d (%p): ", node->getOpCode().getName(), node, valueNumber, c);
         c->print(comp(), comp()->getOutFile());
         traceMsg(comp(),"\n");
         }
      }

   return c;
   }

TR::VPConstraint *OMR::ValuePropagation::addEdgeConstraint(TR::Node *node, TR::VPConstraint *constraint, EdgeConstraints *constraints, TR::Node *relative)
   {
   if (!_isGlobalPropagation)
      return constraint;

   int32_t valueNumber = getValueNumber(node);
   int32_t relativeVN  = relative ? getValueNumber(relative) : AbsoluteConstraint;
   constraint = addConstraintToList(node, valueNumber, relativeVN, constraint, &constraints->valueConstraints);
   if (!constraint && removeConstraints())
      {
      ///setIntersectionFailed(true);
      removeConstraints(valueNumber, &constraints->valueConstraints, false);
      }
   return constraint;
   }

TR::VPConstraint *OMR::ValuePropagation::addBlockConstraint(TR::Node *node, TR::VPConstraint *constraint, TR::Node *relative, bool mustBeValid)
   {
   if (!constraint)
      return NULL;
   int32_t valueNumber = getValueNumber(node);
   int32_t relativeVN  = relative ? getValueNumber(relative) : AbsoluteConstraint;
   constraint = addConstraintToList(node, valueNumber, relativeVN, constraint, &_curConstraints);

   // intersection failed
   if (!constraint && removeConstraints())
      {
      ///setIntersectionFailed(true);
      removeConstraints(valueNumber, &_curConstraints, true);
      return constraint;
      }

   ///TR_ASSERT(constraint || !mustBeValid, "Cannot intersect constraints");
   return constraint;
   }

TR::VPConstraint *OMR::ValuePropagation::addBlockOrGlobalConstraint(TR::Node *node, TR::VPConstraint *constraint, bool isGlobal, TR::Node *relative)
   {
   if (isGlobal)
      return addGlobalConstraint(node, constraint, relative);

   return addBlockConstraint(node, constraint, relative);
   }

void OMR::ValuePropagation::mergeRelationships(TR_LinkHead<Relationship> &fromList, TR_LinkHead<Relationship> &toList,
      int32_t valueNumber, bool preserveFrom, StoreRelationship *mergingStore, List<TR::Symbol> *storeSymbols,
      bool inBothLists)
   {
   if (trace())
      traceMsg(comp(), "Merging relationships for value number: %i\n", valueNumber);

   // Merge (i.e. logical OR) the "from" list into the "to" list.
   // Both lists are ordered by relation number.

   // A relationship that only exists in one of the lists is assumed
   // to be generalized in the other list.

   // See if there are global constraints for this value number. A
   // merged constraint need not be kept in the "to" list if it is already a
   // global constraint.
   //
   Relationship *global = NULL;
   GlobalConstraint *gc = findGlobalConstraint(valueNumber);
   if (gc)
      global = gc->constraints.getFirst();

   Relationship *from = fromList.getFirst();
   bool isFromEmpty = fromList.isEmpty();
   Relationship *to   = toList.getFirst();
   bool isToEmpty = toList.isEmpty();
   Relationship *prev = NULL;
   Relationship *rel;
   TR::VPConstraint *newConstraint;

   // allow store relationships to survive control flow merges
   //
   bool preserveStoreRelationships = true; //comp()->fe()->aggressiveSmallAppOpts();

   if (!preserveFrom)
      fromList.setFirst(NULL);

   while (from || to)
      {
      if (!to || (from && from->relative < to->relative))
         {
         // The "from" value does not exist in the "to" list.
         // Do not add it to the "to" list
         //
         if (preserveStoreRelationships &&
               mergingStore &&
               (from->relative == AbsoluteConstraint) &&
	      (!inBothLists /* || !isToEmpty */)) // commented out the !isToEmpty because it is not guaranteed to be safe to take an absolute constraint that only exists on one side
            {
            rel = createRelationship(from->relative, from->constraint);
            rel->setNext(to);
            to = rel;
            }
         rel = from->getNext();
         if (!preserveFrom)
            freeRelationship(from);
         from = rel;
         continue;
         }

      if (!from || from->relative > to->relative)
         {
         // The "to" value does not exist int the "from" list.
         // Remove it from the "to" list
         //
         rel = to->getNext();
         if (!preserveStoreRelationships ||
               !mergingStore ||
               (to->relative != AbsoluteConstraint) ||
	     (inBothLists /* && isFromEmpty */)) // but do not remove the storeRel
            {
            toList.removeAfter(prev, to);
            freeRelationship(to);
            }
         to = rel;
         continue;
         }

      if (from->constraint == to->constraint)
         {
         // Constraints are the same - move on
         //
         prev = to;
         to   = to->getNext();
         rel = from->getNext();
         if (!preserveFrom)
            freeRelationship(from);
         from = rel;
         continue;
         }

      if (trace())
         {
         traceMsg(comp(), "Attempting merge from: ");
         from->print(this);
         traceMsg(comp(), "\n           merge To: ");
         to->print(this);
         traceMsg(comp(), "\n");
         }

      newConstraint = to->constraint->merge(from->constraint, this);

      rel = from->getNext();
      if (!preserveFrom)
         freeRelationship(from);
      from = rel;

      if (newConstraint)
         {
         // If the new merged constraint is the same as the corresponding
         // global constraint there is no need to add it as a local constraint
         //
         while (global && global->relative < to->relative)
            global = global->getNext();
         if (global && global->relative == to->relative &&
             global->constraint == newConstraint)
            newConstraint = NULL;
         }

      // If merging the constraints resulted in no constraint at all remove the
      // entry from the "to" list.
      //
      if (!newConstraint)
         {
         rel = to->getNext();
         toList.removeAfter(prev, to);
         freeRelationship(to);
         to = rel;
         }

      // Otherwise replace the existing constraint with the new constraint.
      //
      else
         {
         to->constraint = newConstraint;
         prev = to;
         to   = to->getNext();
         }
      }
   }

void OMR::ValuePropagation::mergeStoreRelationships(ValueConstraint *fromvc, ValueConstraint *tovc, bool preserveFrom)
   {
   // Merge (i.e. logical OR) the store relationships in the "from" list into
   // the "to" list.
   // Both lists are ordered by symbol address.
   // Note that the "from" value constraint may be null

   StoreRelationship *from = fromvc ? fromvc->storeRelationships.getFirst() : NULL;
   StoreRelationship *to   = tovc->storeRelationships.getFirst();
   StoreRelationship *prev = NULL;
   StoreRelationship *store;
   int32_t            valueNumber = tovc->getValueNumber();
   TR_ScratchList<TR::Symbol> storeSymbols(comp()->trMemory());
   storeSymbols.deleteAll();
   bool inBothLists = false;

   if (fromvc && !preserveFrom)
      fromvc->storeRelationships.setFirst(NULL);

   // Find the back-edge constraints just in case they are needed
   //
   ValueConstraint *backEdgeStores = NULL;
   if (_loopInfo && _loopInfo->_backEdgeConstraints)
      {
      backEdgeStores = _vcHandler.find(valueNumber, _loopInfo->_backEdgeConstraints->valueConstraints);
      }

   // If only one list has store constraints for a given symbol, one of two
   // things can happen:
   //    1) If this is the second time through a loop and there is a back-edge
   //       constraint for the symbol, use it as the missing constraint
   //    2) Otherwise make the store constraint generalized.
   //
   // If both lists have store constraints for a symbol, the relationship lists
   // are merged.
   //
   prev = NULL;
   while (from || to)
      {
      TR_LinkHead<Relationship> *fromRel, *toRel;
      TR::Symbol *symbol;
      bool preserve = preserveFrom;
      if (!from || (to && from->symbol > to->symbol))
         {
         if (isUnreachableStore(to))
            {
            // From is missing and to is unreachable - ignore the store
            //
            store = to->getNext();
            tovc->storeRelationships.removeAfter(prev, to);
            freeStoreRelationship(to);
            to = store;
            continue;
            }
         symbol = to->symbol;
         fromRel = NULL;
         if (backEdgeStores)
            {
            store = findStoreRelationship(backEdgeStores->storeRelationships, symbol);
            if (store)
               {
               // this is probably a typo...we should actually
               // be interested in the relationships associated
               // with the store instead
               ///fromRel = &backEdgeStores->relationships;
               fromRel = &store->relationships;
               preserve = true;
               }
            }
         toRel = &to->relationships;
         prev = to;
         to = to->getNext();
         }
      else if (!to || from->symbol < to->symbol)
         {
         if (isUnreachableStore(from))
            {
            // To is missing and from is unreachable - ignore the store
            //
            store = from->getNext();
            if (!preserveFrom)
               freeStoreRelationship(from);
            from = store;
            continue;
            }
         symbol = from->symbol;
         store = createStoreRelationship(symbol, NULL);
         if (preserveFrom)
            store->relationships.setFirst(copyRelationships(from->relationships.getFirst()));
         else
            {
            store->relationships.setFirst(from->relationships.getFirst());
            from->relationships.setFirst(NULL);
            }
         tovc->storeRelationships.insertAfter(prev, store);
         prev = store;
         fromRel = NULL;
         if (backEdgeStores)
            {
            store = findStoreRelationship(backEdgeStores->storeRelationships, symbol);
            if (store)
               {
               fromRel = &backEdgeStores->relationships;
               preserve = true;
               }
            }
         toRel = &prev->relationships;
         store = from->getNext();
         if (!preserveFrom)
            freeStoreRelationship(from);
         from = store;
         }
      else
         {
         // Store constraint exists in both lists
         //
         if (isUnreachableStore(from))
            {
            // Use the existing to constraint
            //
            prev = to;
            to = to->getNext();
            store = from->getNext();
            if (!preserveFrom)
               freeStoreRelationship(from);
            from = store;
            continue;
            }
         if (isUnreachableStore(to))
            {
            // Use the existing from constraint
            //
            store = from->getNext();
            freeRelationships(to->relationships);
            if (preserveFrom)
               to->relationships.setFirst(copyRelationships(from->relationships.getFirst()));
            else
               {
               to->relationships.setFirst(from->relationships.getFirst());
               from->relationships.setFirst(NULL);
               freeStoreRelationship(from);
               }
            from = store;
            prev = to;
            to = to->getNext();
            continue;
            }
         fromRel = &from->relationships;
         toRel = &to->relationships;
         prev = to;
         to = to->getNext();
         store = from->getNext();
         // why did we free the store-rels here?
         // we just stored the store-rels into
         // fromRel above, & by freeing lost all
         // the information even before we merged.
         // commenting out the 2 lines below, because
         // mergeRelationships will remove rels from
         // the 'from' list
         //if (!preserveFrom)
            //freeStoreRelationship(from);
         from = store;
         inBothLists = true;
         }

      if (!fromRel)
         {
         // Generalize the to constraint
         //
         // store constraints should survive control flow merges, this kills them
         // unless they are back edge constraints. This does not seem right; turn
         // off and try to understand why this is reqd if something fails.
         //
         if (!lastTimeThrough())
            freeRelationships(*toRel);
         }
      else
         {
         // Merge the relationship lists
         //
         ///traceMsg(comp(), "before merge inBothLists %d isFromEmpty %d isToEmpty %d\n", inBothLists, fromRel->isEmpty(), toRel->isEmpty());
         mergeRelationships(*fromRel, *toRel, valueNumber, preserve, prev /*mergingStore*/, &storeSymbols, inBothLists);
         }
      }
   }

void OMR::ValuePropagation::mergeValueConstraints(ValueConstraint *fromvc, ValueConstraint *tovc, bool preserveFrom)
   {
   // First merge the non-store relationships.
   //
   mergeRelationships(fromvc->relationships, tovc->relationships, fromvc->getValueNumber(), preserveFrom);

   // Now merge the store relationships.
   //
   mergeStoreRelationships(fromvc, tovc, preserveFrom);
   }

void OMR::ValuePropagation::mergeEdgeConstraints(EdgeConstraints *fromEdge, EdgeConstraints *toEdge)
   {
   // Merge (i.e. logical OR) the "from" list into the "to" list.
   // Both lists are ordered by value number.
   //
   // A non-store constraint that only exists in one of the lists is assumed to
   // be generalized in the other list.
   // A store constraint that only exists in one of the lists is left as-is in
   // the merged list.
   //
   // After the merge, the from list is empty.
   //
   ValueConstraintIterator fromVC;
   ValueConstraintIterator toVC;

   if (fromEdge)
      fromVC.reset(fromEdge->valueConstraints);
   else
      fromVC.reset(_curConstraints);

   if (toEdge)
      toVC.reset(toEdge->valueConstraints);
   else
      toVC.reset(_curConstraints);


   ValueConstraint *from = fromVC.getFirst();
   ValueConstraint *to   = toVC.getFirst();
   ValueConstraint *vc;

   while (from || to)
      {
      if (!to ||
          (from && from->getValueNumber() < to->getValueNumber()))
         {
         // The "from" value does not exist in the "to" list.
         // Only its store constraints will be kept.
         //
         freeRelationships(from->relationships);
         if (!from->storeRelationships.isEmpty())
            {
            vc = _vcHandler.findOrCreate(from->getValueNumber(), *toVC.getBase());
            mergeStoreRelationships(from, vc, false);
            if (vc->storeRelationships.isEmpty())
               _vcHandler.remove(vc->getValueNumber(), *toVC.getBase());
            }
         from = fromVC.getNext();
         continue;
         }

      if (!from || from->getValueNumber() > to->getValueNumber())
         {
         // The "to" value does not exist in the "from" list.
         // Only its store constraints will be kept.
         //
         freeRelationships(to->relationships);
         mergeStoreRelationships(NULL, to, false);
         if (to->storeRelationships.isEmpty())
            _vcHandler.remove(to->getValueNumber(), *toVC.getBase());
         to = toVC.getNext();
         continue;
         }

      mergeValueConstraints(from, to);

      if (to->relationships.isEmpty() && to->storeRelationships.isEmpty())
         {
         _vcHandler.remove(to->getValueNumber(), *toVC.getBase());
         }
      from = fromVC.getNext();
      to = toVC.getNext();
      }

   freeValueConstraints(*fromVC.getBase());
   }

void OMR::ValuePropagation::mergeConstraintIntoEdge(ValueConstraint *constraint, EdgeConstraints *edge)
   {
   // Merge (i.e. logical OR) the value constraint into the list.
   //
   ValueConstraint *cur = _vcHandler.findOrCreate(constraint->getValueNumber(), edge->valueConstraints);
   mergeValueConstraints(constraint, cur, true);

   if (cur->relationships.isEmpty() && cur->storeRelationships.isEmpty())
      {
      _vcHandler.remove(cur->getValueNumber(), edge->valueConstraints);
      }
   }

void OMR::ValuePropagation::mergeBackEdgeConstraints(EdgeConstraints *edge)
   {
   // Merge store constraints into the given exit edge for defs that have
   // been seen on the loop back edges.
   //
   ValueConstraint *vc;
   ValueConstraintIterator iter;
   iter.reset(_loopInfo->_backEdgeConstraints->valueConstraints);
   for (vc = iter.getFirst(); vc; vc = iter.getNext())
      {
      ValueConstraint *edgeVc = _vcHandler.findOrCreate(vc->getValueNumber(), edge->valueConstraints);
      mergeStoreRelationships(vc, edgeVc, true);
      }
   }

// Create a global constraint entry
//
OMR::ValuePropagation::GlobalConstraint *OMR::ValuePropagation::GlobalConstraint::create(TR::Compilation * c, int32_t valueNumber)
   {
   GlobalConstraint *entry;
   entry = new (c->trStackMemory()) GlobalConstraint;
   entry->valueNumber = valueNumber;
   return entry;
   }

// Hash function for the global constraints hash table
//
inline
uint32_t OMR::ValuePropagation::hashGlobalConstraint(int32_t valueNumber)
   {
   return valueNumber & _globalConstraintsHTMaxBucketIndex; // exploiting the fact that size is power of two
   }

// Find the global constraint for the given value number.
//
OMR::ValuePropagation::GlobalConstraint *OMR::ValuePropagation::findGlobalConstraint(int32_t valueNumber)
   {
   if (!_isGlobalPropagation)
      return NULL;

   uint32_t hash = hashGlobalConstraint(valueNumber);
   GlobalConstraint *entry;
   for (entry = _globalConstraintsHashTable[hash]; entry; entry = entry->next)
      {
      if (entry->valueNumber == valueNumber)
         break;
      }
   return entry;
   }

// Create the global constraint for the given value number.
//
OMR::ValuePropagation::GlobalConstraint *OMR::ValuePropagation::createGlobalConstraint(int32_t valueNumber)
   {
   uint32_t hash = hashGlobalConstraint(valueNumber);
   GlobalConstraint *entry = GlobalConstraint::create(comp(), valueNumber);
   entry->next = _globalConstraintsHashTable[hash];
   _globalConstraintsHashTable[hash] = entry;
   return entry;
   }

// Create an edge constraint entry
//
OMR::ValuePropagation::EdgeConstraints *OMR::ValuePropagation::EdgeConstraints::create(TR::Compilation * c, TR::CFGEdge *edge)
   {
   EdgeConstraints *entry;
   entry = new (c->trStackMemory()) EdgeConstraints;
   entry->edge = edge;
   return entry;
   }

// Find the edge constraints for the given edge. Create edge constraints if
// they don't currently exist.
//
OMR::ValuePropagation::EdgeConstraints *OMR::ValuePropagation::getEdgeConstraints(TR::CFGEdge *edge)
   {
   int32_t hash = ((uintptr_t)edge) % VP_HASH_TABLE_SIZE;
   EdgeConstraints *entry;
   for (entry = _edgeConstraintsHashTable[hash]; entry; entry = entry->next)
      {
      if (entry->edge == edge)
         break;
      }
   if (!entry)
      {
      entry = EdgeConstraints::create(comp(), edge);
      entry->next = _edgeConstraintsHashTable[hash];
      _edgeConstraintsHashTable[hash] = entry;
      }
   return entry;
   }

OMR::ValuePropagation::EdgeConstraints *OMR::ValuePropagation::createEdgeConstraints(TR::CFGEdge *edge, bool keepBlockList)
   {
   if (!_isGlobalPropagation)
      return NULL;

   EdgeConstraints *constraints = getEdgeConstraints(edge);
   freeValueConstraints(constraints->valueConstraints);

   // Move or copy the list from the current block constraint list. Keep the
   // list ordered.
   //
   if (keepBlockList)
      {
      _vcHandler.setRoot(constraints->valueConstraints, copyValueConstraints(_curConstraints));
      }
   else
      {
      _vcHandler.setRoot(constraints->valueConstraints, _vcHandler.getRoot(_curConstraints));
      _vcHandler.setRoot(_curConstraints, NULL);
      }

   return constraints;
   }

// Can add more blocks to handle conservatively (no constraints pushed down) to save compile time
// Would need to modify addEdgeConstraints or addExceptionEdgeConstraints
//
static bool reduceCostOfAnalysisByTreatingBlockConservatively(TR::Block *b)
   {
   return b->isOSRCatchBlock();
   }


void OMR::ValuePropagation::createExceptionEdgeConstraints(uint32_t exceptions, ValueConstraint *extraConstraint, TR::Node *reason)
   {
   if (!_isGlobalPropagation)
      return;

   for (auto edge= _curBlock->getExceptionSuccessors().begin(); edge != _curBlock->getExceptionSuccessors().end(); ++edge)
      {
      TR::Block *target = toBlock((*edge)->getTo());
      if (!target->canCatchExceptions(exceptions))
         continue;

      if (trace())
         traceMsg(comp(), "   %s [%p] can throw exception to block_%d\n", reason->getOpCode().getName(), reason, target->getNumber());

      EdgeConstraints *constraints = getEdgeConstraints(*edge);

      // If this is the first propagation along this edge, propagate the current
      // block constraints.
      //
      if (isUnreachablePath(constraints))
         {
         constraints = createEdgeConstraints(*edge, true);
         if (reduceCostOfAnalysisByTreatingBlockConservatively(target))
            _vcHandler.setRoot(constraints->valueConstraints, NULL);
         }
      else if (!reduceCostOfAnalysisByTreatingBlockConservatively(target))
         {
         ValueConstraint *vc;
         ValueConstraintIterator iter;
         iter.reset(_curConstraints);
         for (vc = iter.getFirst(); vc; vc = iter.getNext())
            {
            if (!vc->storeRelationships.isEmpty())
               {
               ValueConstraint *edgeVc = _vcHandler.findOrCreate(vc->getValueNumber(), constraints->valueConstraints);
               mergeStoreRelationships(vc, edgeVc, true);
               }
            }
         }

      // If there is an exception-specific constraint to add, merge it in to the
      // exception path constraint list
      //
      // TODO make sure this is right - the actual logic is
      //   exception edge constraints = current constraints the first time the
      //      edge is found & (extraConstraint1 | extraConstraint2 | ...)
      //
      /////if (extraConstraint)
      /////   mergeConstraintIntoEdge(extraConstraint, constraints);

      printEdgeConstraints(constraints);
      }
   }

void OMR::ValuePropagation::removeConstraint(int32_t valueNumber, ValueConstraints &valueConstraints, int32_t relative)
   {
   // Find the constraint for the given value number in the given value
   // constraint list and remove it.
   //
   ValueConstraint *vc = _vcHandler.find(valueNumber, valueConstraints);
   if (vc)
      {
      Relationship *rel, *prevRel, *next;
      for (rel = vc->relationships.getFirst(), prevRel = NULL; rel; prevRel = rel, rel = next)
         {
         next = rel->getNext();
         if (rel->relative > relative)
            break;
         if (rel->relative == relative)
            {
            vc->relationships.removeAfter(prevRel, rel);
            freeRelationship(rel);
            break;
            }
         }

      if (vc->relationships.isEmpty() && vc->storeRelationships.isEmpty())
         {
         _vcHandler.remove(valueNumber, valueConstraints);
         freeValueConstraint(vc);
         }
      }
   }

OMR::ValuePropagation::StoreRelationship *OMR::ValuePropagation::findStoreRelationship(TR_LinkHead<StoreRelationship> &list, TR::Symbol *symbol)
   {
   StoreRelationship *rel;
   for (rel = list.getFirst(); rel; rel = rel->getNext())
      {
      if (rel->symbol == symbol)
         return rel;
      if (rel->symbol > symbol)
         break;
      }
   return NULL;
   }

OMR::ValuePropagation::Relationship *OMR::ValuePropagation::findConstraintInList(TR_LinkHead<Relationship> &list, int32_t relative)
   {
   // Find the constraint for the given value number in the given relationship
   // list.
   //
   Relationship *rel;
   for (rel = list.getFirst(); rel; rel = rel->getNext())
      {
      if (rel->relative == relative)
         return rel;
      if (rel->relative > relative)
         break;
      }
   return NULL;
   }

OMR::ValuePropagation::Relationship *OMR::ValuePropagation::findValueConstraint(int32_t valueNumber, ValueConstraints &valueConstraints, int32_t relative)
   {
   // Find the constraint for the given value number in the given value
   // constraint list.
   //
   ValueConstraint *vc = _vcHandler.find(valueNumber, valueConstraints);
   if (vc)
      return findConstraintInList(vc->relationships, relative);
   return NULL;
   }

OMR::ValuePropagation::Relationship *OMR::ValuePropagation::findConstraint(int32_t valueNumber, int32_t relative)
   {
   return findValueConstraint(valueNumber, _curConstraints, relative);
   }

OMR::ValuePropagation::Relationship *OMR::ValuePropagation::findEdgeConstraint(int32_t valueNumber, EdgeConstraints *edge, int32_t relative)
   {
   return findValueConstraint(valueNumber, edge->valueConstraints, relative);
   }

OMR::ValuePropagation::StoreRelationship *OMR::ValuePropagation::findStoreValueConstraint(int32_t valueNumber, TR::Symbol *symbol, ValueConstraints &valueConstraints)
   {
   // Find the store constraint for the given value number and symbol in the
   // given value constraint list.
   //
   ValueConstraint *vc = _vcHandler.find(valueNumber, valueConstraints);
   if (vc)
      return findStoreRelationship(vc->storeRelationships, symbol);
   return NULL;
   }

OMR::ValuePropagation::StoreRelationship *OMR::ValuePropagation::findStoreConstraint(int32_t valueNumber, TR::Symbol *symbol)
   {
   return findStoreValueConstraint(valueNumber, symbol, _curConstraints);
   }

OMR::ValuePropagation::StoreRelationship *OMR::ValuePropagation::findStoreEdgeConstraint(int32_t valueNumber, TR::Symbol *symbol, EdgeConstraints *edge)
   {
   return findStoreValueConstraint(valueNumber, symbol, edge->valueConstraints);
   }

OMR::ValuePropagation::Relationship *OMR::ValuePropagation::findGlobalConstraint(int32_t valueNumber, int32_t relative)
   {
   GlobalConstraint *gc = findGlobalConstraint(valueNumber);
   if (!gc)
      return NULL;
   return findConstraintInList(gc->constraints, relative);
   }

TR::VPConstraint *OMR::ValuePropagation::applyGlobalConstraints(TR::Node *node, int32_t valueNumber, TR::VPConstraint *constraint, int32_t relative)
   {
   // See if there are global constraints for this value number that can be
   // applied to further constrain the given local constraint.
   //
   GlobalConstraint *gc = findGlobalConstraint(valueNumber);
   if (!gc)
      return constraint;

   Relationship *rel, *link;
   TR::VPConstraint *c = constraint;
   int32_t limit = std::max(valueNumber, relative);
   for (rel = gc->constraints.getFirst(); rel; rel = rel->getNext())
      {
      if (rel->relative == relative)
         {
         c = c->intersect(rel->constraint, this);

         // intersection failed
         if (!c && removeConstraints())
            {
            ///setIntersectionFailed(true);
            removeConstraints(valueNumber, NULL);
            break;
            }

         TR_ASSERT(c, "Cannot intersect global and local constraints");
         break;
         }
      if (rel->relative == AbsoluteConstraint)
         continue;

      // Look through the global relative for constraints that can be applied.
      //
      gc = findGlobalConstraint(rel->relative);
      Relationship *reverse = NULL, *follow = NULL;
      for (link = gc->constraints.getFirst(); link; link = link->getNext())
         {
         if (link->relative == valueNumber)
            {
            reverse = link;
            if (follow)
               break;
            continue;
            }
         else if (link->relative == relative)
            {
            follow = link;
            if (reverse)
               break;
            continue;
            }
         if (link->relative > limit)
            break;
         }
      if (reverse && follow)
         {
         TR::VPConstraint *linkedConstraint;
         if (relative == AbsoluteConstraint)
            linkedConstraint = reverse->constraint->asRelation()->propagateAbsoluteConstraint(follow->constraint, valueNumber, this);
         else
            linkedConstraint = reverse->constraint->asRelation()->propagateRelativeConstraint(follow->constraint->asRelation(), valueNumber, relative, this);

         if (linkedConstraint)
            {
            c = c->intersect(linkedConstraint, this);
            TR_ASSERT(c, "Cannot intersect global and local constraints");
            }
         }
      }
   return c;
   }

TR::VPConstraint *OMR::ValuePropagation::getStoreConstraint(TR::Node *node, TR::Node *relative)
   {
   // See if there is an existing constraint for this node
   //
   int32_t valueNumber = getValueNumber(node);
   TR::Symbol *sym      = node->getSymbol();
   int32_t relativeVN  = relative ? getValueNumber(relative) : AbsoluteConstraint;
   // Look for a local constraint.
   //
   Relationship *rel = NULL;
   StoreRelationship *store = findStoreConstraint(valueNumber, sym);
   if (store)
      rel = findConstraintInList(store->relationships, relativeVN);

   // If there is no local constraint, see if there is a global constraint
   //
   if (!rel)
      rel = findGlobalConstraint(valueNumber, relativeVN);

   if (rel)
      {
      if (trace())
         {
         traceMsg(comp(), "   %s [%p] has existing store constraint:", node->getOpCode().getName(), node);
         rel->print(this, valueNumber, 1);
         }
      return rel->constraint;
      }

   return NULL;
   }


void OMR::ValuePropagation::createStoreConstraints(TR::Node *node)
   {
   // Only need store constraints for global VP
   //
   if (!_isGlobalPropagation)
      return;

   // Set up store constraints corresponding to each relationship currently in
   // the relationships list for the given node's value number.
   // Always create an absolute store constraint, even if its constraint is null
   // since this is the indicator that the store has been seen.
   //
   int32_t valueNumber = getValueNumber(node);
   //static int32_t counter;

   ValueConstraint *vc = _vcHandler.findOrCreate(valueNumber, _curConstraints);

   TR_ASSERT(node->getOpCode().hasSymbolReference(), "Can't create store constraints if no symbol");
   TR::Symbol *symbol = node->getSymbol();

   // Find or create the store relationship
   //
   StoreRelationship *store, *prev;
   for (store = vc->storeRelationships.getFirst(), prev = NULL; store && store->symbol < symbol; prev = store, store = store->getNext())
      {}
   if (!(store && store->symbol == symbol))
      {
      store = createStoreRelationship(symbol, NULL);
      vc->storeRelationships.insertAfter(prev, store);
      }

   freeRelationships(store->relationships);
   store->relationships.setFirst(copyRelationships(vc->relationships.getFirst()));
   }


bool OMR::ValuePropagation::hasBeenStored(int32_t valueNumber, TR::Symbol *symbol, ValueConstraints &valueConstraints)
   {
   // See if the given value number has a store constraint in the given list
   //
   return (findStoreValueConstraint(valueNumber, symbol, valueConstraints) != NULL);
   }

void OMR::ValuePropagation::setUnreachableStore(StoreRelationship *store)
   {
   // Mark the store constraints in the given list as unreachable
   //
   freeRelationships(store->relationships);
   store->relationships.setFirst(createRelationship(AbsoluteConstraint, TR::VPUnreachablePath::create(this)));
   }

bool OMR::ValuePropagation::isUnreachableStore(StoreRelationship *store)
   {
   Relationship *rel = store->relationships.getFirst();
   return (rel && rel->constraint->asUnreachablePath());
   }


bool OMR::ValuePropagation::isDefInUnreachableBlock(int32_t defIndex)
   {
   TR::TreeTop *defTT = _useDefInfo->getTreeTop(defIndex);
   TR::Node *defNode = defTT->getNode();
   TR::Block *defBlock = _useDefInfo->getTreeTop(defIndex)->getEnclosingBlock();
   TR_StructureSubGraphNode *defSubGraphNode = comp()->getFlowGraph()->getStructure()->findNodeInHierarchy(defBlock->getParentStructureIfExists(comp()->getFlowGraph()), defBlock->getNumber());

   bool reachableInputEdgesFound = false;
   while (defSubGraphNode)
      {
      // Find the first input edge that is reachable
      //
      TR_PredecessorIterator pi(defSubGraphNode);
      TR::CFGEdge *edge;
      for (edge = pi.getFirst(); edge; edge = pi.getNext())
         {
         EdgeConstraints *constraints = getEdgeConstraints(edge);
         if (!isUnreachablePath(constraints))
            {
            reachableInputEdgesFound = true;
            break;
            }
         }

      if (!reachableInputEdgesFound &&
          defSubGraphNode->getStructure()->getParent() &&
          (defSubGraphNode->getStructure()->getParent()->getNumber() == defSubGraphNode->getNumber()))
         defSubGraphNode = comp()->getFlowGraph()->getStructure()->findNodeInHierarchy(defSubGraphNode->getStructure()->getParent()->getParent(), defBlock->getNumber());
      else
         defSubGraphNode = NULL;
      }

  if (!reachableInputEdgesFound)
     return true;

   return false;
   }


TR_YesNoMaybe OMR::ValuePropagation::isCastClassObject(TR::VPClassType *type)
   {
   if (type && type->asResolvedClass())
      {
      TR::VPResolvedClass *rc = type->asResolvedClass();
      TR_OpaqueClassBlock *jlC = fe()->getClassClassPointer(rc->getClass());
      if (jlC)
         {
         if (rc->getClass() == jlC)
            return TR_yes;
         else
            //return TR_no; // this is too strict and does not handle interfaces
                            // implemented by java/lang/Class
            return type->isClassObject();
         }
      }
   return TR_maybe;
   }

void OMR::ValuePropagation::checkTypeRelationship(TR::VPConstraint *lhs, TR::VPConstraint *rhs,
                                                int32_t &value, bool isInstanceOf, bool isCheckCast)
   {
   if (trace())
      traceMsg(comp(), "   checking for relationship between types...\n");

   int32_t result = value;
   TR_OpaqueClassBlock *jlKlass = NULL;

#ifdef J9_PROJECT_SPECIFIC
   jlKlass = comp()->getClassClassPointer();
#endif

   if (lhs->asClass() && rhs->asClass())
      {
      TR::VPConstraint *objectConstraint = lhs;
      TR::VPConstraint *castConstraint = rhs;
      TR::VPClass *objectClass = objectConstraint->asClass();
      TR::VPClass *castClass = castConstraint->asClass();
      TR::VPClassPresence *presence = castClass->getClassPresence();
      TR::VPClassType *type = castClass->getClassType();
      TR::VPClassType *newType;
      if (type && type->asFixedClass())
         newType = TR::VPResolvedClass::create(this, type->getClass());
      else
         newType = type;

      TR_YesNoMaybe castIsClassObject = isCastClassObject(type);

      castClass->typeIntersect(presence, newType, objectClass, this);
      // check if intersection failed
      bool noPresenceResult = (!presence && objectClass->getClassPresence() && castClass->getClassPresence());
      bool noTypeResult = (!newType && objectClass->getClassType() && castClass->getClassType());

      if (jlKlass && noPresenceResult)
         {
         if (trace())
            traceMsg(comp(), "presences are incompatible\n");
         result = 0;
         }
      else if (jlKlass && noTypeResult)
         {
         if (trace())
            traceMsg(comp(), "types are incompatible\n");
         result = 0;

         // if the object is a classobject, then the cast maybe one of the
         // interfaces implemented by Class
         if (isInstanceOf || isCheckCast)
            {
            if ((objectConstraint->isClassObject() == TR_yes) &&
                  (castIsClassObject == TR_maybe))
               {
               if (trace())
                  traceMsg(comp(), "object is a classobject but cast maybe Class\n");
               result = value;
               }
            }
         }
      else if (isInstanceOf || isCheckCast)
         {
         if (!objectClass->getClassType() &&
             (castIsClassObject == TR_no) &&
             (isInstanceOf || objectClass->isNonNullObject()) &&
             (objectClass->isClassObject() == TR_yes))
            {
            if (trace())
               traceMsg(comp(), "object is a classobject but cast is not a Class\n");
            result = 0;
            }
         /*
         else if ((castIsClassObject == TR_no) &&
                  !objectClass->getClassType() &&
                  (objectClass->isClassObject() == TR_no))
            {
            }
         */
         else if ((castIsClassObject == TR_yes) &&
                  !objectClass->getClassType() &&
                  (isInstanceOf || objectClass->isNonNullObject()) &&
                  (objectClass->isClassObject() == TR_no))
            {
            result = 0;
            if (trace())
               traceMsg(comp(), "object is not a classobject but cast is java/lang/Class\n");
            }
         // probably cannot get here
         //
         else if ((castIsClassObject == TR_yes) &&
                  !objectClass->getClassType() &&
                  (objectClass->isNonNullObject() || !isInstanceOf) &&
                  (objectClass->isClassObject() == TR_yes))
             {
             result = 1;
             if (trace())
                traceMsg(comp(), "object is a non-null classobject and cast is java/lang/Class\n");
             }
         }
      }
   else if (lhs->getClassType() && rhs->getClassType())
      {
      //TR::VPConstraint *resultType = lhs->intersect(rhs, this);
      TR::VPConstraint *resultType = lhs->getClassType()->classTypesCompatible(rhs->getClassType(), this);
      if (jlKlass)
         {
         // check if intersection failed
         if (!resultType)
            result = 0;
         // if the intersection failed, and
         // if one of the objects is non-null, then they cannot be related
         if (!result && (lhs->isNonNullObject() || rhs->isNonNullObject()))
            result = 1;
         }
      }

   if (value != result)
      value = result;
   }


void OMR::ValuePropagation::invalidateParmConstraintsIfNeeded(TR::Node *node, TR::VPConstraint *constraint)
   {
   if (_isGlobalPropagation)
      return; //nothing to do

   if (!_parmValues)
      return;

   // invalidate parm values in localVP if there is a store
   // to a parm and the value being stored does not match the
   // type of the parm. since localVP has no notion of value numbers,
   // it cannot merge def constraints to get the right type when a load
   // of the parm is seen
   //
   TR::SymbolReference * symRef = node->getOpCode().hasSymbolReference() ? node->getSymbolReference() : 0;
   if (symRef && symRef->getSymbol()->getParmSymbol())
      {
      int32_t parmNum = symRef->getSymbol()->getParmSymbol()->getOrdinal();
      TR::VPConstraint *parmConstraint = _parmValues[parmNum];
      if (parmConstraint)
         {
         if (trace())
            traceMsg(comp(), "Checking compatibility of store node %p parm %d with value\n", node, parmNum);
         int32_t result = 1;
         checkTypeRelationship(parmConstraint, constraint, result, false, false);
         if (!result) // there is some incompatibility
            {
            if (trace())
               traceMsg(comp(), "   Store node %p to parm %d is not compatible with rhs, invalidating _parms entry %p\n", node, parmNum, _parmValues[parmNum]);
            _parmTypeValid[parmNum] = false; // invalidate the info in the parms array
            }
         }
      }
   }

TR::VPConstraint *OMR::ValuePropagation::mergeDefConstraints(TR::Node *node, int32_t relative, bool &isGlobal, bool forceMerge)
   {
   isGlobal = true; // Will be reset if local constraints found

   if (_defMergedNodes->get(node->getGlobalIndex()))
      {
      if (trace())
         traceMsg(comp(), "Node n%dn has already been processed by mergeDefConstraints - returning NULL\n", node->getGlobalIndex());
      return NULL;
      }

   _defMergedNodes->set(node->getGlobalIndex());

   // If the node is a use node, look at its def points and merge constraints
   // from them.
   //
   if (!_isGlobalPropagation)
      {
      if (relative == AbsoluteConstraint)
         {
         TR::SymbolReference * symRef = node->getOpCode().hasSymbolReference() ? node->getSymbolReference() : 0;
         if (symRef && symRef->getSymbol()->getParmSymbol() && _parmValues)
            {
            int32_t parmNum = symRef->getSymbol()->getParmSymbol()->getOrdinal();
            isGlobal = false;
            // give up if the parm is not invariant. we cannot assume to always have
            // the right constraint without dataflow and usedefs
            //
            if (_parmTypeValid[parmNum] && isParmInvariant(symRef->getSymbol()))
               return _parmValues[parmNum];
            }
         }
      return NULL;
      }

   if (node->getOpCodeValue() == TR::loadaddr)
      {
      // A loadaddr (USE) can be defined by an iistore.  Merging those
      // constraints (the ones on the iistore) in would be dangerous.
      // There's no real point in merging in constraints from other
      // loadaddr defs, so just return NULL.
      return NULL;
      }

   uint16_t useDefIndex = node->getUseDefIndex();
   if (!useDefIndex || !_useDefInfo->isUseIndex(useDefIndex))
      return NULL;

   // If the node has a single defining load any available constraints must
   // have already been computed, so don't do it again.
   //
   TR::Node *defNode = _useDefInfo->getSingleDefiningLoad(node);
   // if a node has a single defining load, then check if it has
   // already been visited. _visitCount is changed when processing a loop
   // so just testing with an equality is insufficient
   //
   if (!forceMerge && defNode)
      {
      if (trace())
         traceMsg(comp(), "   %s [%p] has singleDefiningLoad [%p] ", node->getOpCode().getName(), node, defNode);
      if (defNode->getVisitCount() >= _visitCount)
         {
         if (trace())
            traceMsg(comp(), "  whose constraints already computed\n");
         return NULL;
         }
      if (trace())
         traceMsg(comp(), "  going to merge\n");
      }

   TR_UseDefInfo::BitVector defs(comp()->allocator());
   if (!_useDefInfo->getUseDef(defs, useDefIndex))
      return NULL;

   int32_t baseValueNumber = -1;
   if (node->getOpCode().isIndirect())
      baseValueNumber = getValueNumber(node->getFirstChild());

   TR::VPConstraint   *constraint      = NULL;
   int32_t            defValueNumber;
   StoreRelationship *defValueConstraint;
   TR::VPConstraint   *defConstraint;
   bool               unseenDefsFound = false;
   bool               unconstrained   = false;
   int32_t            defIndex;
   TR::Symbol        *sym;
   Relationship      *rel;

   TR_ScratchList<TR::VPConstraint> bigDecimalConstraints(comp()->trMemory());
   TR_UseDefInfo::BitVector::Cursor cursor(defs);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      defIndex = cursor;
      bool defIsUnseen = false;
      defConstraint = NULL;
      if (defIndex < _useDefInfo->getFirstRealDefIndex())
         {
         if (trace())
            traceMsg(comp(), "   %s [%p] looking at method entry def point: ", node->getOpCode().getName(), node);
         // Remember that method entry is a def point for this value number
         //
         createStoreConstraints(node);

         if (relative == AbsoluteConstraint)
            {
            // Method entry is a def point. If this is a reference to a
            // parameter we may be able to pick up the entry constraint for
            // the parameter.
            //
            TR::SymbolReference *symRef = node->getSymbolReference();
            if (symRef && symRef->getSymbol()->getParmSymbol() && _parmValues)
               {
               int32_t parmNum = symRef->getSymbol()->getParmSymbol()->getOrdinal();
               defConstraint = _parmValues[parmNum];
               isGlobal = false;
               }
            }
         }
      else
         {
         defNode = _useDefInfo->getNode(defIndex);

         // The def node may have been removed. If so, ignore it.
         //
         if (defNode == 0 || defNode->getUseDefIndex() == 0)
            continue;

         defValueNumber = getValueNumber(defNode);
         // before we can look at def nodes to build up a set of contraints we had better know that set of defs
         // dominates the use we are considering - if not we treat the use as an unknown
         if (node->getSymbol()->isAutoOrParm()
             && _curDefinedOnAllPaths
             && !_curDefinedOnAllPaths->get(node->getSymbolReference()->getReferenceNumber()))
            {
            if (trace())
               traceMsg(comp(), "symRef %d is not stored on all paths - treating as unknown\n", defNode->getSymbolReference()->getReferenceNumber());
            unseenDefsFound = true;
            }

         // Only consider def nodes that are stores; other def nodes
         // (e.g. calls) will make this value unconstrained.
         // For an indirect reference, only consider stores whose base has the
         // same value number as this node's base; other stores will make this
         // value unconstrained.
         //
         if (trace())
            traceMsg(comp(), "   %s [%p] looking at def value %d, %s [%p]: ", node->getOpCode().getName(), node, getValueNumber(defNode), defNode->getOpCode().getName(), defNode);

         if (defNode->getOpCode().isStore())
            {
            // TODO : This can be improved to return a value by checking which byte(s) are written to
            // and taking the rhs and applying it properly to get the correct value for (potentially wider) datatype
            // of the use
            //
            if (node->getDataType() != defNode->getDataType())
               return NULL;

            // Check for indirect reference
            //
            bool matchingDef = true;
            if (baseValueNumber >= 0)
               {
               TR_ASSERT(defNode->getOpCode().isIndirect(), "Expected indirect store");
               if (defNode->getOpCode().isIndirect())
                  {
                  matchingDef = (baseValueNumber == getValueNumber(defNode->getFirstChild()));
                  }
               else
                  {
                  matchingDef = false;
                  }
               }

            if (matchingDef)
               {
               // See if there is a valid store constraint for the defining node
               //
               rel = NULL;
               sym = defNode->getSymbol();
               defValueConstraint = findStoreConstraint(defValueNumber, sym);
               if (defValueConstraint)
                  {
                  rel = findConstraintInList(defValueConstraint->relationships, relative);
                  if (rel)
                     {
                     if (rel->constraint->asUnreachablePath())
                        {
                        // This def is unreachable on this path
                        //
                        if (trace())
                           traceMsg(comp(), "def is unreachable (constraint), ignored\n");
                        continue;
                        }
                     isGlobal = false;
                     }
                  else
                     {
                     rel = findGlobalConstraint(defValueNumber, relative);
                     }
                  if (rel)
                     defConstraint = rel->constraint;
                  }

               else
                  {
                  // The node has not yet been seen. It is either coming from
                  // an unreachable block or we are in a loop and it is defined
                  // later in the loop. If it may be the latter case, the first
                  // time through the loop gets an unconstrained value.
                  // The last time through we may be able to take values from
                  // back-edge constraints.
                  //
                  if (!isDefInUnreachableBlock(defIndex))
                     {
                     unseenDefsFound = true;
                     defIsUnseen = true;
                     }
                  else
                     {
                     if (trace())
                        traceMsg(comp(), "def is unreachable (defInUnreachableBlock), ignored\n");
                     continue;
                     }
#if 0
                  bool defIsUnreachable = isDefInUnreachableBlock(defIndex);
                  if (_loopInfo && !defIsUnreachable)
                     {
                     unseenDefsFound = true;
                     defIsUnseen = true;
                     }
                  else
                     {
                     if (defIsUnreachable)
                        {
                        if (trace())
                           traceMsg(comp(), "def is unreachable (defInUnreachableBlock), ignored\n");
                        continue;
                        }
                     else
                        {
                        unseenDefsFound = true;
                        defIsUnseen = true;
                        }
                     }
#endif
                  }
               }
            }
         }

      if (defIsUnseen)
         {
         if (trace())
            traceMsg(comp(), "def has not been seen yet\n");
         }
      else if (!defConstraint)
         {
         if (trace())
            traceMsg(comp(), "no constraint\n");
         constraint = NULL;
         unconstrained = true;
         }
      else
         {
         if (trace())
            {
            defConstraint->print(comp(), comp()->getOutFile());
            traceMsg(comp(), "\n");
            }

         bool mergeConstraint = true;
#ifdef J9_PROJECT_SPECIFIC
         // Handle BigDecimal add, subtract, multiply.
         if (defNode && node)
            {
            if (defNode->getOpCode().isStoreDirect())
               {
               TR::Node *firstChild = defNode->getFirstChild();
               if ((firstChild->getOpCodeValue() == TR::acall) ||
                   (firstChild->getOpCodeValue() == TR::acalli))
                  {
                  TR::SymbolReference * symRef = firstChild->getSymbolReference();
                  if (!symRef->isUnresolved() && symRef->getSymbol()->isResolvedMethod())
                     {
                     TR::ResolvedMethodSymbol *method = symRef->getSymbol()->getResolvedMethodSymbol();

                     if ((method->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
                         (method->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
                         (method->getRecognizedMethod() == TR::java_math_BigDecimal_multiply))
                        {
                        TR::Node *thisChild = firstChild->getFirstChild();
                        if (firstChild->getOpCode().isCallIndirect())
                           thisChild = firstChild->getSecondChild();

                        if ((thisChild == node) ||
                            ((thisChild->getOpCodeValue() == node->getOpCodeValue()) &&
                             (thisChild->getSymbolReference() == node->getSymbolReference())))
                           {
                           bool avoidMerge = false;
                           if (thisChild != node)
                              {
                              uint16_t thisUseDefIndex = thisChild->getUseDefIndex();
                              if (!thisUseDefIndex || !_useDefInfo->isUseIndex(thisUseDefIndex))
                                 avoidMerge = true;

                              TR_UseDefInfo::BitVector thisDefs(comp()->allocator());
                              if (!_useDefInfo->getUseDef(thisDefs, thisUseDefIndex) || (!(thisDefs == defs)))
                                 avoidMerge = true;
                              }

                           if (!avoidMerge)
                              {
                              mergeConstraint = false;
                              bigDecimalConstraints.add(defConstraint);
                              }
                           }
                        }
                     }
                  }
               }
            }
#endif

         if (mergeConstraint)
            {
            if (trace())
               traceMsg(comp(), " merging constraint at def %p\n", defNode);
            if (constraint)
               {
               constraint = constraint->merge(defConstraint, this);
               if (!constraint)
                  unconstrained = true;
               }
            else if (!unconstrained)
               constraint = defConstraint;
            }
         else if (trace())
            traceMsg(comp(), " NOT merging constraint at def %p\n", defNode);
         }
      }

   if (!bigDecimalConstraints.isEmpty())
      {
      bool mergeDeferredConstraints = true;
      if (constraint && constraint->isFixedClass())
         {
         TR_ResolvedMethod *owningMethod = comp()->getCurrentMethod();
         TR_OpaqueClassBlock *classObject = fe()->getClassFromSignature("java/math/BigDecimal", 20, owningMethod);
         if (classObject && (constraint->getClass() == classObject))
            mergeDeferredConstraints = false;
         }

      if (constraint && mergeDeferredConstraints)
         {
         ListIterator<TR::VPConstraint> cIt(&bigDecimalConstraints);
         TR::VPConstraint *c;
         for (c = cIt.getFirst(); c; c = cIt.getNext())
            {
            if (trace())
               traceMsg(comp(), " doing late merging constraint at def %p\n", defNode);
            constraint = constraint->merge(c, this);
            if (!constraint)
               {
               unconstrained = true;
               break;
               }
            }
         }
      bigDecimalConstraints.deleteAll();
      }

   if (!unseenDefsFound)
      return constraint;

   // there are unseen defs and the region being
   // processed is not a loop. so don't return a
   // constraint until all defs are seen
   //
   if (!_loopInfo &&
         lastTimeThrough() &&
         unseenDefsFound)
      return NULL;

   if (!lastTimeThrough())
      {
      // First time through, put the seen defs into the loop defs hash table
      //
      // restart cursor(defs)
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         defIndex = cursor;
         if (defIndex < _useDefInfo->getFirstRealDefIndex())
            continue;

         defNode = _useDefInfo->getNode(defIndex);

         if (defNode)
            {
            defValueNumber = getValueNumber(defNode);
            if (hasBeenStored(defValueNumber, defNode->getSymbol(), _curConstraints))
               addLoopDef(defNode);
            }
         }
      return NULL;
      }

   // Last time through, and there were unseen defs: if we are already unconstrained
   // then there is no point in looking at the backedge defs
   //
   if (unconstrained)
      return constraint;

   // Last time through and there were unseen defs - try looking in the
   // back edge constraints. There is no point in doing this if the value
   // is already unconstrained.
   //
   // Find the most nested loop that contains all the defs that have been
   // seen. We must merge constraints for unseen defs from all loops
   // inside this container loop.
   // If the seen defs do not dominate this use (i.e. method entry is a
   // def of this use) we must look at the back edges of all parent loops
   // since they can all reach this use.
   //
   LoopInfo *container;
   if (defs.ValueAt(0))
      container = NULL; // Method entry is a def
   else
      {
      container = _loopInfo;
      // restart cursor(defs)
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         defIndex = cursor;
         if (defIndex < _useDefInfo->getFirstRealDefIndex())
            continue;

         defNode = _useDefInfo->getNode(defIndex);
         sym = defNode->getSymbol();
         defValueNumber = getValueNumber(defNode);
         if (hasBeenStored(defValueNumber, sym, _curConstraints))
            {
            LoopDefsHashTableEntry *entry = findLoopDef(defNode);
            //
            // Note that entry can be NULL in the following case:
            // if the definition is seen the first time through the loop
            // before the use is seen, then it is NOT added loopDefs.
            // But the second time through the loop, the definition may NOT
            // be seen by the time the use is seen if the basic block containing
            // the def is eliminated by value propagation (unreachable) during
            // the second time through. Since our test to figure out if the def
            // node is unseen because of unreachability is not precise enough
            // (for compile time reasons possibly) if (_loopInfo) is done.
            // if _loopInfo is non null but still the reason for a def being
            // unseen is that the (unreachable) def block has been eliminated,
            // we can get here and entry will be NULL.
            //
            if (entry)
               {
               for ( ; container; container = container->_parent)
                  {
                  if (container->_loop == entry->region)
                     break;
                  }
               }
            }
         }
      }

   if (relative != AbsoluteConstraint)
      {
      if (trace())
         traceMsg(comp(), "we are processing a relative constraint and merging with backedge constraints is not supported\n");
      return NULL;
      }

   // Now go through the defs again and look for back-edge constraints on
   // the unseen def nodes
   //
   unseenDefsFound = false;
   // restart cursor(defs)
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      defIndex = cursor;
      if (defIndex < _useDefInfo->getFirstRealDefIndex())
         continue;

      defNode = _useDefInfo->getNode(defIndex);

      if (defNode->getUseDefIndex() == 0)
         continue;

      sym = defNode->getSymbol();
      defValueNumber = getValueNumber(defNode);
      // if we haven't seen a def on this iteration along all paths we must consider the backedge constraints
      if (node->getSymbol()->isAutoOrParm()
          && !unseenDefsFound
          && !_curDefinedOnAllPaths->get(defNode->getSymbolReference()->getReferenceNumber()))
         {
         if (trace())
            traceMsg(comp(), "symref %d is not defined on all paths - consulting backedge constraints\n", node->getSymbolReference()->getReferenceNumber());

         // this def is unseen
         unseenDefsFound = true;
         LoopInfo *loopInfo;
         for (loopInfo = _loopInfo; loopInfo && loopInfo != container; loopInfo = loopInfo->_parent)
            {
            if (trace())
               traceMsg(comp(), "   %s [%p] looking at back edges in loop %d for def %s [%p]: ", node->getOpCode().getName(), node, loopInfo->_loop->getNumber(), defNode->getOpCode().getName(), defNode);

            // See if the unseen def was seen on a path to this back edge.
            //
            if (!loopInfo ||
                !loopInfo->_backEdgeConstraints)
               {
               if (trace())
                  traceMsg(comp(), "not seen on this back edge, ignored\n");
               continue;
               }

            if (!hasBeenStored(defValueNumber, sym, loopInfo->_backEdgeConstraints->valueConstraints))
               {
               if (trace())
                  traceMsg(comp(), "no backedge constraint, ignored\n");
               continue;
               }

            // this means that the unseen def was seen on some back edge
            unseenDefsFound = false;

            // The def node may have a trivial equality relationship with
            // this use node. If so, ignore it.
            //
            rel = NULL;
            defValueConstraint = findStoreEdgeConstraint(defValueNumber, sym, loopInfo->_backEdgeConstraints);
            //if (defValueConstraint)
            //   rel = findConstraintInList(defValueConstraint->relationships, getValueNumber(node));
            //if (!rel)
            rel = findConstraintInList(defValueConstraint->relationships, relative);

            if (rel)
               {
               isGlobal = false;
               }
            else
               {
               //rel = findGlobalConstraint(defValueNumber,getValueNumber(node));
               //if (!rel)
                  rel = findGlobalConstraint(defValueNumber, relative);
               }
            defConstraint = rel ? rel->constraint : NULL;
            if (defConstraint &&
                defConstraint->asEqual() &&
                defConstraint->asEqual()->increment() == 0)
               {
               if (trace())
                  traceMsg(comp(), "trivially equal, ignored\n");
               continue;
               }

            if (!defConstraint)
               {
               if (trace())
                  traceMsg(comp(), "no constraint\n");
               return NULL;
               }
            if (defConstraint->asUnreachablePath())
               {
               if (trace())
                  traceMsg(comp(), "def is unreachable, ignored\n");
               continue;
               }

            bool mergeConstraint = true;
#ifdef J9_PROJECT_SPECIFIC
            // Handle BigDecimal add, subtract, multiply.
            if (defNode && node)
               {
               if (defNode->getOpCode().isStoreDirect())
                  {
                  TR::Node *firstChild = defNode->getFirstChild();
                  if ((firstChild->getOpCodeValue() == TR::acall) ||
                      (firstChild->getOpCodeValue() == TR::acalli))
                     {
                     TR::SymbolReference * symRef = firstChild->getSymbolReference();
                     if (!symRef->isUnresolved())
                        {
                        TR::ResolvedMethodSymbol *method = symRef->getSymbol()->getResolvedMethodSymbol();

                        if (method &&
                            ((method->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
                             (method->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
                             (method->getRecognizedMethod() == TR::java_math_BigDecimal_multiply)))
                           {
                           TR::Node *thisChild = firstChild->getFirstChild();
                           if (firstChild->getOpCode().isCallIndirect())
                              thisChild = firstChild->getSecondChild();

                           if ((thisChild == node) ||
                               ((thisChild->getOpCodeValue() == node->getOpCodeValue()) &&
                                 (thisChild->getSymbolReference() == node->getSymbolReference())))
                              {
                              bool avoidMerge = false;
                              if (thisChild != node)
                                 {
                                 uint16_t thisUseDefIndex = thisChild->getUseDefIndex();
                                 if (!thisUseDefIndex || !_useDefInfo->isUseIndex(thisUseDefIndex))
                                    avoidMerge = true;

                                 TR_UseDefInfo::BitVector thisDefs(comp()->allocator());
                                 if (!_useDefInfo->getUseDef(thisDefs, thisUseDefIndex) || (!(thisDefs == defs)))
                                    avoidMerge = true;
                                 }

                              if (!avoidMerge)
                                 {
                                 mergeConstraint = false;
                                 bigDecimalConstraints.add(defConstraint);
                                 }
                              }
                           }
                        }
                     }
                  }
               }
#endif

            if (mergeConstraint)
               {
               if (trace())
                  traceMsg(comp(), " mmerging constraint at def %p\n", defNode);
               if (constraint)
                  {
                  //
                  // If its an induction var, check the direction of increment and do not change the bound in the
                  // direction opposite to the increment
                  //
                  InductionVariable *ivInfo;
                  for (ivInfo = loopInfo->_inductionVariables.getFirst(); ivInfo; ivInfo = ivInfo->getNext())
                     {
                     if (ivInfo->_symbol == sym)
                        break;
                     }

                  bool positiveIncrement = false;
                  bool negativeIncrement = false;
                  if (ivInfo && ivInfo->_entryDef)
                     {
                     if (ivInfo->_symbol->getType().isInt32())
                        {
                        if (!defConstraint->isUnsigned())
                           {
                           if (ivInfo->_increment->asIntConst())
                              {
                              if (ivInfo->_increment->getLowInt() > 0)
                                 positiveIncrement = true;
                              else
                                 negativeIncrement = true;
                              }
                           else
                              {
                              if ((ivInfo->_increment->getLowInt() > 0) &&
                                  (ivInfo->_increment->getHighInt() > 0))
                                 positiveIncrement = true;
                              else if ((ivInfo->_increment->getLowInt() < 0) &&
                                       (ivInfo->_increment->getHighInt() < 0))
                                 negativeIncrement = true;
                              }
                           }
                        else
                           positiveIncrement = true;
                        }
                     else if (ivInfo->_symbol->getType().isInt64())
                        {
                        if (!defConstraint->isUnsigned())
                           {
                           if (ivInfo->_increment->asLongConst())
                              {
                              if (ivInfo->_increment->getLowLong() > 0)
                                 positiveIncrement = true;
                              else
                                 negativeIncrement = true;
                              }
                           else
                              {
                              if ((ivInfo->_increment->getLowLong() > 0) &&
                                  (ivInfo->_increment->getHighLong() > 0))
                                 positiveIncrement = true;
                              else if ((ivInfo->_increment->getLowLong() < 0) &&
                                       (ivInfo->_increment->getHighLong() < 0))
                                 negativeIncrement = true;
                              }
                           }
                        else
                           positiveIncrement = true;
                        }
                     else if (ivInfo->_symbol->getType().isInt16())
                       {
                        if (!defConstraint->isUnsigned())
                           {
                           if (ivInfo->_increment->asShortConst())
                              {
                              if (ivInfo->_increment->getLowShort() > 0)
                                 positiveIncrement = true;
                              else
                                 negativeIncrement = true;
                              }
                           else
                              {
                              if ((ivInfo->_increment->getLowShort() > 0) &&
                                  (ivInfo->_increment->getHighShort() > 0))
                                 positiveIncrement = true;
                              else if ((ivInfo->_increment->getLowShort() < 0) &&
                                       (ivInfo->_increment->getHighShort() < 0))
                                 negativeIncrement = true;
                              }
                           }
                        else
                           positiveIncrement = true;

                       }
                     }

                  if (sym->getType().isInt32())
                     {
                     if (positiveIncrement)
                        {
                        if (defConstraint->isUnsigned())
                           {
                           if ((uint32_t) defConstraint->getHighInt() < TR::getMaxUnsigned<TR::Int32>())
                              {
                              if ((uint32_t) defConstraint->getLowInt() < (uint32_t) constraint->getLowInt() &&
                                  (uint32_t) constraint->getLowInt() <= defConstraint->getHighInt() )
                                 {
                                 TR::VPConstraint *newDefConstraint = TR::VPIntRange::create(this, constraint->getLowInt(), defConstraint->getHighInt());
                                 defConstraint = defConstraint->intersect(newDefConstraint, this);
                                 }
                              }
                           }
                        else if (defConstraint->getHighInt() < TR::getMaxSigned<TR::Int32>())
                           {
                           if (defConstraint->getLowInt() < constraint->getLowInt() &&
                               constraint->getLowInt() <= defConstraint->getHighInt())
                              {
                              TR::VPConstraint *newDefConstraint = TR::VPIntRange::create(this, constraint->getLowInt(), defConstraint->getHighInt());
                              defConstraint = defConstraint->intersect(newDefConstraint, this);
                              }
                           }
                        }
                     else if (negativeIncrement)
                        {
                        if (defConstraint->getLowInt() > TR::getMinSigned<TR::Int32>())
                           {
                           if (defConstraint->getLowInt() <= constraint->getHighInt() &&
                               constraint->getHighInt() < defConstraint->getHighInt())
                              {
                              TR::VPConstraint *newDefConstraint = TR::VPIntRange::create(this, defConstraint->getLowInt(), constraint->getHighInt());
                              defConstraint = defConstraint->intersect(newDefConstraint, this);
                              }
                           }
                        }
                     }
                  else if (sym->getType().isInt16())
                    {
                     if (positiveIncrement)
                        {
                        if (defConstraint->isUnsigned())
                           {
                           if ((uint16_t) defConstraint->getHighShort() < TR::getMaxUnsigned<TR::Int16>())
                              {
                              if ((uint16_t) defConstraint->getLowShort() < (uint16_t) constraint->getLowShort() &&
                                  (uint16_t) constraint->getLowShort() <= defConstraint->getHighShort())
                                 {
                                 TR::VPConstraint *newDefConstraint = TR::VPShortRange::create(this, constraint->getLowShort(), defConstraint->getHighShort());
                                 defConstraint = defConstraint->intersect(newDefConstraint, this);
                                 }
                              }
                           }
                        else if (defConstraint->getHighShort() < TR::getMaxSigned<TR::Int16>())
                           {
                           if (defConstraint->getLowShort() < constraint->getLowShort() &&
                               constraint->getLowShort() <= defConstraint->getHighShort())
                              {
                              TR::VPConstraint *newDefConstraint = TR::VPShortRange::create(this, constraint->getLowShort(), defConstraint->getHighShort());
                              defConstraint = defConstraint->intersect(newDefConstraint, this);
                              }
                           }
                        }
                     else if (negativeIncrement)
                        {
                        if (defConstraint->getLowShort() > TR::getMinSigned<TR::Int16>())
                           {
                           if (defConstraint->getLowShort() <= constraint->getHighShort() &&
                               constraint->getHighShort() < defConstraint->getHighShort())
                              {
                              TR::VPConstraint *newDefConstraint = TR::VPShortRange::create(this, defConstraint->getLowShort(), constraint->getHighShort());
                              defConstraint = defConstraint->intersect(newDefConstraint, this);
                              }
                           }
                        }
                     }
                  else
                     {
                     if (positiveIncrement)
                        {
                       if (defConstraint->isUnsigned())
                           {
                           if ((uint64_t) defConstraint->getHighLong() < TR::getMaxUnsigned<TR::Int64>())
                              {
                              if ((uint64_t) defConstraint->getLowLong() < (uint64_t) constraint->getLowLong() &&
                                  (uint64_t) constraint->getLowLong() <= (uint64_t) defConstraint->getHighLong())
                                 {
                                 TR::VPConstraint *newDefConstraint = TR::VPLongRange::create(this, constraint->getLowLong(), defConstraint->getHighLong());
                                 defConstraint = defConstraint->intersect(newDefConstraint, this);
                                 }
                              }
                           }
                        else if (defConstraint->getHighLong() < TR::getMaxSigned<TR::Int64>())
                           {
                           if (defConstraint->getLowLong() < constraint->getLowLong() &&
                               constraint->getLowLong() <= defConstraint->getHighLong())
                              {
                              TR::VPConstraint *newDefConstraint = TR::VPLongRange::create(this, constraint->getLowLong(), defConstraint->getHighLong());
                              defConstraint = defConstraint->intersect(newDefConstraint, this);
                              }
                           }
                        }
                     else if (negativeIncrement)
                        {
                        if (defConstraint->getLowLong() > TR::getMinSigned<TR::Int64>())
                           {
                           if (defConstraint->getLowLong() <= constraint->getHighLong() &&
                               constraint->getHighLong() < defConstraint->getHighLong())
                              {
                              TR::VPConstraint *newDefConstraint = TR::VPLongRange::create(this, defConstraint->getLowLong(), constraint->getHighLong());
                              defConstraint = defConstraint->intersect(newDefConstraint, this);
                              }
                           }
                        }
                     }

                  if (trace())
                     {
                     defConstraint->print(comp(), comp()->getOutFile());
                     traceMsg(comp(), "\n");
                     }

                  constraint = constraint->merge(defConstraint, this);
                  if (!constraint)
                     return NULL;
                  }
               else
                  constraint = defConstraint;
               }
            else if (trace())
               traceMsg(comp(), " NOT mmerging constraint at def %p\n", defNode);
            }
         }

      // check if there are unseen defs even after consulting
      // backedge store constraints. don't return a
      // constraint until all defs are seen
      //
      if (_loopInfo &&
            lastTimeThrough() &&
            unseenDefsFound)
         return NULL;
      }

   if (!bigDecimalConstraints.isEmpty())
      {
      bool mergeDeferredConstraints = true;
      if (constraint && constraint->isFixedClass())
         {
         TR_ResolvedMethod *owningMethod = comp()->getCurrentMethod();
         TR_OpaqueClassBlock *classObject = fe()->getClassFromSignature("java/math/BigDecimal", 20, owningMethod);
         //printf("VP class for BigDecimal %p\n", classObject); fflush(stdout);
         if (classObject && (constraint->getClass() == classObject))
            mergeDeferredConstraints = false;
         }

      if (constraint && mergeDeferredConstraints)
         {
         ListIterator<TR::VPConstraint> cIt(&bigDecimalConstraints);
         TR::VPConstraint *c;
         for (c = cIt.getFirst(); c; c = cIt.getNext())
            {
            if (trace())
               traceMsg(comp(), " late mmerging constraint at def %p\n", defNode);
            constraint = constraint->merge(c, this);
            if (!constraint)
               {
               unconstrained = true;
               break;
               }
            }
         }

      bigDecimalConstraints.deleteAll();
      }

   return constraint;
   }

void OMR::ValuePropagation::replaceByConstant(TR::Node *node, TR::VPConstraint *constraint, bool isGlobal)
   {
   if (isGlobal)
      addGlobalConstraint(node, constraint);
   else
      addBlockConstraint(node, constraint);

   // If this is a local constraint only change the node to a constant if this
   // is the last time through. This is because the constant node would create
   // a global constraint the second time through, which would be wrong.

   if (!isGlobal && !lastTimeThrough())
      return;

   if (!performTransformation(comp(), "%sConstant folding %s [" POINTER_PRINTF_FORMAT "]", OPT_DETAILS, node->getOpCode().getName(), node))
      return;

   // If the node has any children, they must be handled before replacing this
   // node by a constant node.
   //
   removeChildren(node);

   // invalidate its use def info
   int32_t useIndex = node->getUseDefIndex();
   TR_UseDefInfo *info = optimizer()->getUseDefInfo();
   if (info && (info->isDefIndex(useIndex) || info->isUseIndex(useIndex)))
      {
      if (info->getNode(useIndex) == node)
         info->clearNode(useIndex);
      }

   node->setUseDefIndex(0);
   invalidateValueNumberInfo();
   invalidateUseDefInfo();

   TR::DataType type = node->getDataType();
   TR::VPConstraint * shortConstraint = constraint->asShortConstraint();
   switch (type)
      {
      case TR::Int32:
         TR::Node::recreate(node, TR::iconst);
         node->setInt(constraint->asIntConst()->getInt());
         dumpOptDetails(comp(), " to iconst %d\n", node->getInt());
         break;
      case TR::Int8:
         TR::Node::recreate(node, TR::bconst);
         node->setByte(constraint->asIntConst()->getInt());
         dumpOptDetails(comp(), " to bconst %d\n", node->getByte());
         break;
      case TR::Int16:
         TR::Node::recreate(node, TR::sconst);
         (shortConstraint == NULL) ? node->setShortInt(constraint->asIntConst()->getInt()) :
             node->setShortInt(constraint->asShortConst()->getShort());
         dumpOptDetails(comp(), " to sconst %d\n", node->getShortInt());
         break;
      case TR::Int64:
         TR::Node::recreate(node, TR::lconst);
         node->setLongInt(constraint->asLongConst()->getLong());
         dumpOptDetails(comp(), " to lconst " INT64_PRINTF_FORMAT "\n", node->getLongInt());
         break;
      case TR::Float:
         TR::Node::recreate(node, TR::fconst);
         node->setFloat(constraint->asFloatConst()->getFloat());
         dumpOptDetails(comp(), " to fconst [float const]\n");
         break;
      case TR::Double:
         TR::Node::recreate(node, TR::dconst);
         node->setDouble(constraint->asDoubleConst()->getDouble());
         dumpOptDetails(comp(), " to dconst [double const]\n");
         break;
      case TR::Address:
         if (node->getOpCode().isLoadDirect())
            {
            node->setIsDontMoveUnderBranch(false);
            }
         TR::Node::recreate(node, TR::aconst);
         node->setAddress(0);
         TR_ASSERT(constraint->isNullObject(), "Only null address constant supported");
         dumpOptDetails(comp(), " to aconst " UINT64_PRINTF_FORMAT_HEX "\n", (uint64_t)node->getAddress());
         break;
      default:
         TR_ASSERT(0, "Bad type for convert to constant");
         break;
      }

   // Make an effort to set flags as appropriate for the constant
   constrainNewlyFoldedConst(this, node, isGlobal);

   setEnableSimplifier();
   }

void OMR::ValuePropagation::removeRestOfBlock()
   {
   // Remove the rest of the trees in the block.
   //
   TR::TreeTop *treeTop, *next;
   for (treeTop = _curTree->getNextTreeTop();
        treeTop->getNode()->getOpCodeValue() != TR::BBEnd;
        treeTop = next)
      {
      removeNode(treeTop->getNode(), false);
      next = treeTop->getNextTreeTop();
      TR::TransformUtil::removeTree(comp(), treeTop);
      }
   }

void OMR::ValuePropagation::mustTakeException()
   {
   // Make sure this hasn't already been done
   //
   if (_curTree->getNextTreeTop()->getNode()->getOpCodeValue() == TR::Return)
      return;

   /////printf("\nRemoving rest of block in %s\n", comp()->signature());

   if (!performTransformation(comp(), "%sRemoving rest of block after %s [%p]\n", OPT_DETAILS, _curTree->getNode()->getOpCode().getName(), _curTree->getNode()))
      return;

   // Remove the rest of the trees in the block.
   //
   removeRestOfBlock();

   // Terminate the block with a "return" opcode.
   //
   TR::Node *retNode = TR::Node::create(_curTree->getNode(), TR::Return, 0);
   TR::TreeTop::create(comp(), _curTree, retNode);

   // Remove the regular CFG edges out of the block.
   // If there is already an edge to the exit block leave it there. Otherwise
   // create one.
   //
   TR::CFG *cfg = comp()->getFlowGraph();
   for (auto edge = _curBlock->getSuccessors().begin(); edge != _curBlock->getSuccessors().end(); ++edge)
      {
      if ((*edge)->getTo() != cfg->getEnd())
         {
         _edgesToBeRemoved->add(*edge);
         setUnreachablePath(*edge);
         }
      }
   }

bool OMR::ValuePropagation::registerPreXClass(TR::VPConstraint *constraint)
   {
   if (!constraint->isFixedClass())
      return false;
   if (!constraint->isPreexistentObject())
      return false;
   TR_OpaqueClassBlock *clazz = constraint->getClass();
   TR_OpaqueClassBlock *assumptionClazz = constraint->getPreexistence()->getAssumptionClass();
   //TR_ASSERT(!classHasExtended(clazz), "assertion failure");
   if (clazz == assumptionClazz)
      _prexClasses.add(clazz);
   else
      _prexClassesThatShouldNotBeNewlyExtended.add(assumptionClazz);
   return true;
   }

TR::Node *OMR::ValuePropagation::findThrowInBlock(TR::Block *block, TR::TreeTop *&treeTop)
   {
  // if (comp()->getFlowGraph()->getRemovedNodes().find(block))
    if(block->nodeIsRemoved())
      return 0;

   treeTop = block->getLastRealTreeTop();
   TR::Node * node = treeTop->getNode();
   if (node->getOpCodeValue() != TR::athrow)
      {
      if (node->getOpCodeValue() == TR::Return)
         return 0; // mustTakeException has been called on the block that use to contain our throw
      if (node->getNumChildren() != 1)
         return 0; // inlining messed up the trees so that we can't find the throw now
      node = node->getFirstChild();
      }

   if (node->getOpCodeValue() != TR::athrow)
      return 0; // inlining messed up the trees so that we can't find the throw now

   return node;
   }


bool OMR::ValuePropagation::isHighWordZero(TR::Node *node)
   {
   // See if there is a constraint for this value
   //
   bool isGlobal;
   TR::VPConstraint *constraint = getConstraint(node, isGlobal);
   if (constraint)
      {
      TR::VPLongConstraint *longConstraint = constraint->asLongConstraint();
      if (longConstraint &&
          (longConstraint->getLow() >= 0) &&
          ((longConstraint->getHigh() & (((uint64_t)0xffffffff)<<32)) == 0))
         {
           //printf("Found a long which has high word zero (node %x) in %s\n", node, comp()->signature());
         return true;
         }
      }
   return false;
   }

void OMR::ValuePropagation::checkForInductionVariableIncrement(TR::Node *node)
   {
   // If we are the first time through a loop see if this is an increment of a
   // local. If so save the node in the induction variable info for the loop.
   //
   if (!_loopInfo)
      return;

   TR::Symbol *sym = node->getSymbol();
   if (!sym->isAutoOrParm())
      return;

   _loopInfo->_seenDefs->set(getValueNumber(node));

   TR::Node *child = node->getFirstChild();
   if (!(child->getOpCode().isAdd() || child->getOpCode().isSub()))
      return;

   TR::Node *loadNode = child->getFirstChild();
   if (!(loadNode->getOpCode().isLoadVar() && loadNode->getSymbol() == sym))
      return;

   bool isGlobal;
   TR::VPConstraint *increment = getConstraint(child->getSecondChild(), isGlobal);
   if (!increment)
      return;
   if (!(increment->asIntConst() || increment->asLongConst()))
      return;

   if (child->getOpCode().isSub())
      {
      if (increment->asIntConst())
         increment = TR::VPIntConst::create(this, -increment->asIntConst()->getInt());
      else if(increment->asShortConst())
         increment = TR::VPShortConst::create(this, -increment->asShortConst()->getShort());
      else
         increment = TR::VPLongConst::create(this, -increment->asLongConst()->getLong());
      }

   // The node is an increment of a local by a constant. To be an induction
   // variable it must have two defining value numbers, one being the value
   // number of the store of the increment and the other being the value
   // number for the induction variable on entry to the loop.
   //
   bool          isInductionVariable = true;
   uint16_t      useIndex            = loadNode->getUseDefIndex();
   int32_t       incrementVN         = getValueNumber(node);
   TR::Node      *entryDef            = NULL;
   int32_t       entryVN             = -1;

   if (trace())
      traceMsg(comp(), "   %s [%p] may be induction variable [%p]\n", node->getOpCode().getName(), node, sym);

   if (!useIndex || !_useDefInfo->isUseIndex(useIndex))
      isInductionVariable = false;


   bool maybeInductionVariable = false;
   bool invalidEntryInfo = false;
   if (isInductionVariable)
      {
      TR_UseDefInfo::BitVector defs(comp()->allocator());
      if (!_useDefInfo->getUseDef(defs, useIndex))
         isInductionVariable = false;
      else
         {
         TR_UseDefInfo::BitVector::Cursor cursor(defs);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t defIndex = cursor;
            if (defIndex < _useDefInfo->getFirstRealDefIndex())
               {
               // Method entry is a def point.
               //
               isInductionVariable = false;
               if (trace())
                  traceMsg(comp(), "      ivInfo is invalid for sym [%p] as method entry is def point\n", sym);
               break;
               }

            TR::Node *defNode = _useDefInfo->getNode(defIndex);
            int32_t defVN    = getValueNumber(defNode);

            bool notInLoop = false;
            if (defNode->getOpCode().isStore())
               {
               TR::TreeTop *defTree = _useDefInfo->getTreeTop(defIndex);
               if (defTree && comp()->getFlowGraph()->getStructure())
                  {
                  if (defTree->getEnclosingBlock()->getStructureOf()->getContainingLoop() != _loopInfo->_loop)
                     notInLoop = true;
                  }
               }

            if (!notInLoop || _loopInfo->_seenDefs->get(defVN))
               continue;

            entryVN = defVN;
            entryDef = defNode;
            break;
            }


         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t defIndex = cursor;

            if (defIndex < _useDefInfo->getFirstRealDefIndex())
               {
               // Method entry is a def point.
               //
               isInductionVariable = false;
               if (trace())
                  traceMsg(comp(), "      ivInfo is invalid for sym [%p] as method entry is def point\n", sym);
               break;
               }

            TR::Node *defNode = _useDefInfo->getNode(defIndex);
            int32_t defVN    = getValueNumber(defNode);

            bool notInLoop = false;
            if (defNode->getOpCode().isStore())
               {
               TR::TreeTop *defTree = _useDefInfo->getTreeTop(defIndex);
               if (defTree && comp()->getFlowGraph()->getStructure())
                  {
                  if (defTree->getEnclosingBlock()->getStructureOf()->getContainingLoop() != _loopInfo->_loop)
                     notInLoop = true;
                  }
               }

            if ((defVN != incrementVN) && !notInLoop)
               {
               maybeInductionVariable = false;
               isInductionVariable = false;
               if (trace())
                  traceMsg(comp(), "      ivInfo is invalid for sym [%p], def [%d] node %p does not have the same value number as another increment\n", sym, defVN, defNode);
               break;
               }

            if (((defVN != entryVN) && !notInLoop) || ((defVN == entryVN) && notInLoop))
               continue;

            if (entryVN >= 0)
               {
               // Too many entry value numbers
               //
               if (_loopInfo->_seenDefs->get(defVN))
                  {
                  maybeInductionVariable = false;
                  isInductionVariable = false;
                  if (trace())
                     traceMsg(comp(), "      ivInfo is invalid for sym [%p], def [%d] already seen in loop\n", sym, defVN);
                  break;
                  }
               else
                  {
                  if (trace())
                     {
                     traceMsg(comp(), "      Sym [%p] has more than two defs, extra def due to VN [%d]\n", sym, entryVN);
                     traceMsg(comp(), "      Maybe able to guess the increment\n");
                     }
                  maybeInductionVariable = true;
                  invalidEntryInfo = true;
                  }
               }

            if ((defVN != entryVN) && notInLoop)
               {
               maybeInductionVariable = false;
               isInductionVariable = false;
               if (trace())
                  traceMsg(comp(), "      ivInfo is invalid for sym [%p], def [%d] node %p is likely in some inner loop\n", sym, defVN, defNode);
               break;
               }
            }
         }
      }


   // See if we already have induction variable info for this symbol
   //
   InductionVariable *iv;
   for (iv = _loopInfo->_inductionVariables.getFirst(); iv; iv = iv->getNext())
      {
      if (iv->_symbol == sym)
         break;
      }

   if (iv)
      {
      // There is existing info for this symbol. See if the current increment
      // is compatible with it
      //
      if (iv->_onlyIncrValid)
         invalidEntryInfo = false;
      iv->_invalidEntryInfo = invalidEntryInfo;
      if (iv->_entryDef)
         {
         if (/* entryDef != iv->_entryDef || */ incrementVN != iv->_incrementVN)
            {
            // Not compatible, mark the induction variable as invalid
            //
            iv->_entryDef = NULL;
            }
         }
      }

   else
      {
      // This is a new induction variable. Set it up.
      //
      iv = new (trStackMemory()) InductionVariable(sym, entryDef, incrementVN, increment, this);
      _loopInfo->_inductionVariables.add(iv);
      // induction variable has more than two defs, but
      // it may be possible to guess the increment at least
      if (maybeInductionVariable)
         {
         if (checkLoopTestBlock(sym))
            {
            invalidEntryInfo = false;
            iv->_onlyIncrValid = true;
            iv->_entryDef = NULL;
            if (trace())
               traceMsg(comp(), "      Guessed increment of the iv for sym [%p]\n", sym);
            }
         else
            if (trace())
               traceMsg(comp(), "      Could not guess the increment for sym [%p], not marked as induction variable\n", sym);
         }

      iv->_invalidEntryInfo = invalidEntryInfo;
      // Not valid to discover it second time round
      //
      if (lastTimeThrough())
         iv->_entryDef = NULL;
      }

   // If the induction variable is not valid, mark it
   //
   if (!isInductionVariable)
      iv->_entryDef = NULL;

   // if the entry is invalid, mark it so
   if (iv->_invalidEntryInfo)
      iv->_entryDef = NULL;

   // If the induction variable is valid, inject a constraint showing a use of
   // the induction variable has been found
   //
   if (iv->_entryDef)
      {
      // select the right type of constraint for induction variables to prevent merging different types of constraints together
      if (node->getOpCode().isLong())
         addConstraintToList(node, iv->_valueNumber, AbsoluteConstraint, TR::VPLongConst::create(this, incrementVN), &_curConstraints, true);
      else if (node->getOpCode().isShort())
         addConstraintToList(node, iv->_valueNumber, AbsoluteConstraint, TR::VPShortConst::create(this, incrementVN), &_curConstraints, true);
      else
         addConstraintToList(node, iv->_valueNumber, AbsoluteConstraint, TR::VPIntConst::create(this, incrementVN), &_curConstraints, true);
      }
   }

void OMR::ValuePropagation::checkForInductionVariableLoad(TR::Node *node)
   {
   // If we are the last time through a loop see if this is a load of an
   // induction variable.
   //
   if (!_loopInfo || !lastTimeThrough())
      return;

   if (!node->getOpCode().isLoadVar())
      return;

   TR::Symbol *sym = node->getSymbol();
   if (!sym->isAutoOrParm())
      return;

   // See if we have induction variable info for this symbol
   //
   InductionVariable *iv;
   for (iv = _loopInfo->_inductionVariables.getFirst(); iv; iv = iv->getNext())
      {
      if (iv->_symbol == sym)
         break;
      }

   // If the induction variable is valid, inject a constraint showing a use of
   // the induction variable has been found
   //
   if (iv && iv->_entryDef)
      {
      int32_t useValueNumber = getValueNumber(node);

      // select the right type of constraint for induction variables to prevent merging different types of constraints together
      if (node->getOpCode().isLong())
         addConstraintToList(node, iv->_valueNumber, AbsoluteConstraint, TR::VPLongConst::create(this, useValueNumber), &_curConstraints, true);
      else if (node->getOpCode().isShort())
         addConstraintToList(node, iv->_valueNumber, AbsoluteConstraint, TR::VPShortConst::create(this, useValueNumber), &_curConstraints, true);
      else
         addConstraintToList(node, iv->_valueNumber, AbsoluteConstraint, TR::VPIntConst::create(this, useValueNumber), &_curConstraints, true);
      }
   }

void OMR::ValuePropagation::collectInductionVariableEntryConstraints()
   {
   // If this is the last time through the loop, induction variables have
   // already been identified. Make sure that the entry values for each one are
   // actually known on entry, and save the entry constraints.
   // At the same time get rid of invalid induction variables from the list.
   //
   if (!_loopInfo)
      return;

   InductionVariable *iv, *next;
   for (iv = _loopInfo->_inductionVariables.getFirst(); iv; iv = next)
      {
      next = iv->getNext();
      if (iv->_entryDef && !iv->_invalidEntryInfo && !iv->_onlyIncrValid)
         {
         int32_t entryVN = getValueNumber(iv->_entryDef);
         if (hasBeenStored(entryVN, iv->_entryDef->getSymbol(), _curConstraints))
            {
            iv->_entryConstraint = getStoreConstraint(iv->_entryDef);
            if (iv->_entryConstraint && iv->_entryConstraint->asUnreachablePath())
               {
               // Entry def is unreachable at the loop entry, mark it as
               // invalid.
               //
               iv->_entryDef = NULL;
               }
            }
         else
            {
            // Entry value number not known on entry, induction variable is
            // invalid.
            //
            iv->_entryDef = NULL;
            }
         }

      // Remove invalid induction variables from the list
      //
      if (!iv->_entryDef)
         _loopInfo->_inductionVariables.remove(iv);
      }
   }

bool OMR::ValuePropagation::checkLoopTestBlock(TR::Symbol *sym)
   {
   // the symbol could be an induction variable even though it
   // has more than two defs. find the loop test block and check
   // if this is the sym used in the loop test. if it is indeed,
   // it might be useful to at least guess the increment.
   if (!_loopInfo)
      return false;
   TR_RegionStructure *region = _loopInfo->_loop;
   if (!region->isNaturalLoop())
      return false;

   // check for a while-do or a do-while loop
   //
   bool isWellFormedLoop = false;

   TR_BlockStructure *entry = _loopInfo->_entryBlock ? _loopInfo->_entryBlock->getStructureOf() : NULL;
   TR_StructureSubGraphNode *entryNode = region->getEntry();
   if (entry)
      {
      for (auto edge = entryNode->getSuccessors().begin(); edge != entryNode->getSuccessors().end(); ++edge)
         {
         if (region->isExitEdge(*edge))
            isWellFormedLoop = true;
         }
      }

   if (!isWellFormedLoop)
      {
      TR_StructureSubGraphNode *branchNode = NULL;
      TR_RegionStructure::Cursor snIt(*region);
      for (TR_StructureSubGraphNode *sgNode = snIt.getCurrent();
           sgNode && (branchNode == NULL);
           sgNode = snIt.getNext())
         {
         bool hasBackEdge = false;
         bool hasExitEdge = false;
         for (auto edge = sgNode->getSuccessors().begin(); edge != sgNode->getSuccessors().end(); ++edge)
            {
            TR_StructureSubGraphNode *dest = toStructureSubGraphNode((*edge)->getTo());
            if (region->isExitEdge(*edge))
               hasExitEdge = true;
            if (dest == region->getEntry())
               {
               hasBackEdge = true;
               }
            if (hasBackEdge && hasExitEdge)
               isWellFormedLoop = true;
            }
         }
      }

   TR_BlockStructure *loopTestBlock = NULL;
   if (isWellFormedLoop)
      {
      ListIterator<TR::CFGEdge> cfgIt(&region->getExitEdges());
      for (TR::CFGEdge *e = cfgIt.getFirst(); e; e = cfgIt.getNext())
         {
         loopTestBlock = toStructureSubGraphNode(e->getFrom())->getStructure()->asBlock();
         if (loopTestBlock)
            {
            TR::Node *branchNode = loopTestBlock->getBlock()->getLastRealTreeTop()->getNode();
            if (branchNode->getOpCode().isBranch() &&
                  branchNode->getOpCode().isBooleanCompare())
               {
               TR::Node *firstChild = branchNode->getFirstChild();
               TR::Node *secondChild = branchNode->getSecondChild();
               while (firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub())
                  {
                  if (firstChild->getSecondChild()->getOpCode().isLoadConst())
                     firstChild = firstChild->getFirstChild();
                  else
                     return false;
                  }
               if ((firstChild->getOpCode().isLoadVarDirect() &&
                     (firstChild->getSymbolReference()->getSymbol() == sym)) ||
                   (secondChild->getOpCode().isLoadVarDirect() &&
                     (secondChild->getSymbolReference()->getSymbol() == sym)))
                  {
                  return true;
                  }
               }
            }
         }
      return false;
      }
   else
      return false;
   }


void OMR::ValuePropagation::checkBackEdgeCoverage()
   {
   // For each valid induction variable there should be a constraint on the
   // back edge to show that increments to the induction variable have
   // reached the back edge on all paths.
   //
   InductionVariable *iv, *next;
   for (iv = _loopInfo->_inductionVariables.getFirst(); iv; iv = next)
      {
      next = iv->getNext();
      if (iv->_entryDef)
         {
         Relationship *rel = findEdgeConstraint(iv->_valueNumber, _loopInfo->_backEdgeConstraints);
         if (rel)
            {
            TR_ASSERT(((rel->constraint->asIntConst() && rel->constraint->asIntConst()->getInt() == iv->_incrementVN) ||
                      (rel->constraint->asLongConst() && rel->constraint->asLongConst()->getLong() == iv->_incrementVN) ||
                      (rel->constraint->asShortConst() && rel->constraint->asShortConst()->getShort() == iv->_incrementVN))
                      , "getLow not equal to incrementVN");
            removeConstraint(iv->_valueNumber, _loopInfo->_backEdgeConstraints->valueConstraints);
            }
         else
            {
            iv->_entryDef = NULL;
            }
         }

      // If the induction variable is no longer valid, remove it
      //
      if (!iv->_entryDef)
         _loopInfo->_inductionVariables.remove(iv);
      }
   }

void OMR::ValuePropagation::setUpInductionVariables(TR_StructureSubGraphNode *node)
   {
   TR_RegionStructure *region = node->getStructure()->asRegion();
   region->clearInductionVariables();

   if (_loopInfo->_inductionVariables.isEmpty())
      return;

   InductionVariable *ivInfo;

   while ((ivInfo = _loopInfo->_inductionVariables.pop()))
      {
      // Set up the exit value by merging constraints from all exit edges.
      // Exit edge constraints have already been merged into the parent
      // subgraph so we must examine the successors of the region's node in the
      // parent subgraph.
      //
      TR::VPConstraint *exitConstraint      = NULL;
      bool             exitConstraintFound = false;
      TR_SuccessorIterator exits(node);
      for (TR::CFGEdge *edge = exits.getFirst(); edge; edge = exits.getNext())
         {
         if (ivInfo->_onlyIncrValid)
            continue;

         EdgeConstraints *constraints = getEdgeConstraints(edge);

         // Ignore unreachable exit paths
         //
         if (isUnreachablePath(constraints))
            continue;

         // Merge in the constraints for all uses of the symbol on this edge.
         //
         Relationship *uses = findEdgeConstraint(ivInfo->_valueNumber, constraints);
         if (!uses)
            {
            exitConstraintFound = true;
            exitConstraint = NULL;
            continue;
            }

         TR::VPConstraint *constraint = uses->constraint;

         // Remove the "uses" constraint - it does not apply to the parent
         //
         removeConstraint(ivInfo->_valueNumber, constraints->valueConstraints);

         // If this is not a valid induction variable or if the exit constraint
         // is already generalized don't bother examining the uses constraint
         //
         if (!ivInfo->_entryDef)
            continue;
         if (exitConstraintFound && exitConstraint == NULL)
            continue;

         // The "uses" constraint is a set of int values that are the value
         // numbers which can hold the induction variable along this edge.
         // Merge the constraints for all these value numbers to get the
         // constraint for the induction variable along this edge
         //
         ListElement<TR::VPConstraint> *next;
         if (constraint->asMergedConstraints())
            {
            next = constraint->asMergedConstraints()->getList()->getListHead();
            constraint = next->getData();
            next = next->getNextElement();
            }
         else
            {
            next = NULL;
            }

         while (1)
            {
            int32_t high = 0;
            int32_t low = 1;
            if (constraint->asIntConstraint())
               {
               high = constraint->getHighInt();
               low = constraint->getLowInt();
               }
            else if (constraint->asLongConstraint())
               {
               high = constraint->getHighLong();
               low = constraint->getLowLong();
               }

            for (int32_t i = low; i <= high; i++)
               {
               Relationship *rel = findEdgeConstraint(i, constraints);

               if (!rel)
                  {
                  exitConstraint = NULL;
                  next = NULL;
                  break;
                  }
               if (exitConstraint)
                  {
                  exitConstraint = exitConstraint->merge(rel->constraint, this);
                  if (!exitConstraint)
                     {
                     next = NULL;
                     break;
                     }
                  }
               else
                  exitConstraint = rel->constraint;
               }

            if (!next)
               break;
            constraint = next->getData();
            next = next->getNextElement();
            }

         exitConstraintFound = true;
         }

      if (!ivInfo->_entryDef && !ivInfo->_onlyIncrValid)
         continue;

      //if (ivInfo->_invalidEntryInfo)
      //   continue;

      TR::VPConstraint *entry, *exit, *incr;
      if (ivInfo->_symbol->getType().isInt32())
         {
         if (ivInfo->_entryConstraint &&
             !ivInfo->_invalidEntryInfo)
            {
            if (ivInfo->_entryConstraint->asIntConst())
               entry = new (trHeapMemory()) TR::VPIntConst(ivInfo->_entryConstraint->getLowInt());
            else
               entry = new (trHeapMemory()) TR::VPIntRange(ivInfo->_entryConstraint->getLowInt(), ivInfo->_entryConstraint->getHighInt());
            }
         else
            entry = NULL;
         if (ivInfo->_increment->asIntConst())
            incr = new (trHeapMemory()) TR::VPIntConst(ivInfo->_increment->getLowInt());
         else
            incr = new (trHeapMemory()) TR::VPIntRange(ivInfo->_increment->getLowInt(), ivInfo->_increment->getHighInt());
         if (exitConstraint)
            {
            if (exitConstraint->asIntConst())
               exit = new (trHeapMemory()) TR::VPIntConst(exitConstraint->getLowInt());
            else
               exit = new (trHeapMemory()) TR::VPIntRange(exitConstraint->getLowInt(), exitConstraint->getHighInt());
            }
         else
            exit = NULL;
         }
      else if (ivInfo->_symbol->getType().isInt16())
         {
         if (ivInfo->_entryConstraint &&
             !ivInfo->_invalidEntryInfo)
            {
            if (ivInfo->_entryConstraint->asShortConst())
               entry = new (trHeapMemory()) TR::VPShortConst(ivInfo->_entryConstraint->getLowShort());
            else
               entry = new (trHeapMemory()) TR::VPShortRange(ivInfo->_entryConstraint->getLowShort(), ivInfo->_entryConstraint->getHighShort());
            }
         else
            entry = NULL;
         if (ivInfo->_increment->asShortConst())
            incr = new (trHeapMemory()) TR::VPShortConst(ivInfo->_increment->getLowShort());
         else
            incr = new (trHeapMemory()) TR::VPShortRange(ivInfo->_increment->getLowShort(), ivInfo->_increment->getHighShort());
         if (exitConstraint)
            {
            if (exitConstraint->asShortConst())
               exit = new (trHeapMemory()) TR::VPShortConst(exitConstraint->getLowShort());
            else
               exit = new (trHeapMemory()) TR::VPShortRange(exitConstraint->getLowShort(), exitConstraint->getHighShort());
            }
         else
            exit = NULL;

         }
      else
         {
         if (ivInfo->_entryConstraint &&
             !ivInfo->_invalidEntryInfo)
            {
            if (ivInfo->_entryConstraint->asLongConst())
               entry = new (trHeapMemory()) TR::VPLongConst(ivInfo->_entryConstraint->getLowLong());
            else
               entry = new (trHeapMemory()) TR::VPLongRange(ivInfo->_entryConstraint->getLowLong(), ivInfo->_entryConstraint->getHighLong());
            }
         else
            entry = NULL;
         if (ivInfo->_increment->asLongConst())
            incr = new (trHeapMemory()) TR::VPLongConst(ivInfo->_increment->getLowLong());
         else
            incr = new (trHeapMemory()) TR::VPLongRange(ivInfo->_increment->getLowLong(), ivInfo->_increment->getHighLong());
         if (exitConstraint)
            {
            if (exitConstraint->asLongConst())
               exit = new (trHeapMemory()) TR::VPLongConst(exitConstraint->getLowLong());
            else
               exit = new (trHeapMemory()) TR::VPLongRange(exitConstraint->getLowLong(), exitConstraint->getHighLong());
            }
         else
            exit = NULL;
         }

      TR_YesNoMaybe isSigned = TR_maybe;
      if (exitConstraint)
         {
         if (exitConstraint->isUnsigned())
            isSigned = TR_no;
         else
            isSigned = TR_yes;
         }

      TR_InductionVariable *iv = new (trHeapMemory()) TR_InductionVariable(ivInfo->_symbol->castToRegisterMappedSymbol(), entry, exit, incr, isSigned);

      if (trace())
         {
         /////printf("\nFound induction variable in %s",comp()->signature());
         traceMsg(comp(), "\nFound induction variable %d [%p]", ivInfo->_valueNumber-_firstInductionVariableValueNumber, ivInfo->_symbol);
         if (ivInfo->_entryConstraint &&
             ivInfo->_entryDef)
            {
            traceMsg(comp(), "\n   Entry constraint : ");
            ivInfo->_entryConstraint->print(comp(), comp()->getOutFile());
            }
         traceMsg(comp(), "\n   Increment constraint : ");
         ivInfo->_increment->print(comp(), comp()->getOutFile());
         if (exitConstraint)
            {
            traceMsg(comp(), "\n   Exit constraint : ");
            exit->print(comp(), comp()->getOutFile());
            //exitConstraint->print(comp(), comp()->getOutFile());
            }
         traceMsg(comp(), "\n");
         }

      region->addInductionVariable(iv);
      }
   }

void OMR::ValuePropagation::collectBackEdgeConstraints()
   {
   // Merge the constraint lists from the back edges of a natural loop.
   // Only those constraints that apply to value numbers for def nodes and
   // for induction variables are kept in the back edge constraints, since
   // these are the only constraints that can validly be used.
   //
   TR_StructureSubGraphNode *entry = _loopInfo->_loop->getEntry();
   TR_PredecessorIterator    pi(entry);
   TR::CFGEdge               *edge;
   EdgeConstraints          *constraints;

   if (!_loopInfo->_backEdgeConstraints)
      _loopInfo->_backEdgeConstraints = EdgeConstraints::create(comp(), NULL);
   else
      freeValueConstraints(_loopInfo->_backEdgeConstraints->valueConstraints);

   for (edge = pi.getFirst(); edge; edge = pi.getNext())
      {
      constraints = getEdgeConstraints(edge);

      // Ignore unreachable edges
      //
      if (isUnreachablePath(constraints))
         continue;

      // Remove constraint info for any value numbers that do not represent
      // def nodes.
      //
      ValueConstraint *cur, *next;
      ValueConstraintIterator iter(constraints->valueConstraints);
      for (cur = iter.getFirst(); cur; cur = next)
         {
         next = iter.getNext();
         if (cur->getValueNumber() < _firstInductionVariableValueNumber)
            {
            // Remove any non-store constraints and see what's left
            //
            freeRelationships(cur->relationships);
            if (cur->storeRelationships.isEmpty())
               {
               _vcHandler.remove(cur->getValueNumber(), constraints->valueConstraints);
               freeValueConstraint(cur);
               }
            }
         }

      if (_loopInfo->_backEdgeConstraints->valueConstraints.isEmpty())
         {
         // This is the first back edge. Copy its constraint list.
         //
         _vcHandler.setRoot(_loopInfo->_backEdgeConstraints->valueConstraints, _vcHandler.getRoot(constraints->valueConstraints));
         _vcHandler.setRoot(constraints->valueConstraints, NULL);
         }
      else
         {
         // Merge into the back edge constraint list. We don't want to look into
         // the back edge constraints for missing store constraints while
         // merging, so temporarily zap the loop info
         //
         LoopInfo *temp = _loopInfo;
         _loopInfo = NULL;
         mergeEdgeConstraints(constraints, temp->_backEdgeConstraints);
         _loopInfo = temp;
         }
      }
   }


TR::GlobalValuePropagation::GlobalValuePropagation(TR::OptimizationManager *manager)
   : TR::ValuePropagation(manager), _blocksToProcess(NULL)
   {
   _isGlobalPropagation = true;
   }


int32_t TR::GlobalValuePropagation::perform()
   {

   TR::CFG *cfg = comp()->getFlowGraph();

   // Can't do this analysis if there is no CFG or no use/def info or no
   // value numbers
   //
   if (!cfg)
      {
      dumpOptDetails(comp(), "Can't do Global Value Propagation - there is no CFG\n");
      return 0;
      }
   if (!optimizer()->getUseDefInfo())
      {
      dumpOptDetails(comp(), "Can't do Global Value Propagation - no use/def info for %s\n", comp()->signature());
      return 0;
      }
   _useDefInfo = optimizer()->getUseDefInfo();
   if (!optimizer()->getValueNumberInfo())
      {
      dumpOptDetails(comp(), "Can't do Global Value Propagation - no value numbers for %s\n", comp()->signature());
      return 0;
      }
   _valueNumberInfo = optimizer()->getValueNumberInfo();

   if (trace())
      {
      comp()->dumpMethodTrees("Trees before Global Value Propagation");
      }

   // From here, down, stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   initialize();

   if ((_firstUnresolvedSymbolValueNumber - 1) <= comp()->getNodeCount())
      {
      dumpOptDetails(comp(), "Can't do Global Value Propagation - too many nodes\n");
      return 0;
      }

   // TODO: Use TR_CanReachGivenBlocks from TR_CanBeReachedWithoutExceptionEdges

   static char *skipBlocksThatCannotReachNonColdBlocks = feGetEnv("TR_skipBlocksThatCannotReachNonColdBlocks");
   if (skipBlocksThatCannotReachNonColdBlocks)
      {
      _blocksToProcess = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNumberOfNodes(), comp()->trMemory(), stackAlloc, notGrowable, TR_MemoryBase::ValuePropagation);
      TR_CanReachNonColdBlocks(comp()).perform(_blocksToProcess);
      }

   static char *skipBlocksThatCannotReachNormalPaths = feGetEnv("TR_skipBlocksThatCannotReachNormalPaths");
   if (skipBlocksThatCannotReachNormalPaths)
      {
      _blocksToProcess = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNumberOfNodes(), comp()->trMemory(), stackAlloc, notGrowable, TR_MemoryBase::ValuePropagation);
      TR_CanBeReachedWithoutExceptionEdges(comp()).perform(_blocksToProcess);
      TR_CanReachGivenBlocks(comp(), _blocksToProcess).perform(_blocksToProcess); // Bitvector is changed in-place
      }


   _bestRun = true; //getLastRun(); getLastRun doesn't work well yet for VP

   // initialize the bit which indicates
   // if intersection of constraints
   // failed at any time. this is used
   // only on prod-builds
   setIntersectionFailed(false);

   getParmValues();
   determineConstraints();

   // If there are deep chains of value numbers related to each other
   // disable future passes of value propagation
   //
   if (_reachedMaxRelationDepth)
      {
      requestOpt(OMR::globalValuePropagation, false);
      requestOpt(OMR::localValuePropagation,  false);
      }

   // Enable other optimization passes which may now be useful
   //
   if (enableSimplifier())
      {
      requestOpt(OMR::treeSimplification);
      requestOpt(OMR::basicBlockExtension);
      }

   // Disable looping back to this optimization. This will be turned on again
   // if late inlining succeeds.  Note, this must be done before
   // doDelayedTrasformations is called as it calls the inliner which may
   // re-enable value propagation.
   //
   requestOpt(OMR::eachExpensiveGlobalValuePropagationGroup, false);

   if (checksWereRemoved())
      requestOpt(OMR::catchBlockRemoval);

   // Perform transformations that were delayed until the end of the analysis
   //
   doDelayedTransformations();

   if (_enableVersionBlocks)
      {
      // since block versioner invalidates structure, run
      // another pass of GVP just to populate induction variable information
      if (!_blocksToBeVersioned->isEmpty())
         {
         requestOpt(OMR::veryCheapGlobalValuePropagationGroup, true);
         }
      versionBlocks();
      }

   if (trace())
      comp()->dumpMethodTrees("Trees after Global Value Propagation");

   // Invalidate usedef and value number information if necessary
   //
   if (_useDefInfo && useDefInfoInvalid())
      optimizer()->setUseDefInfo(NULL);
   if (_valueNumberInfo && valueNumberInfoInvalid())
      optimizer()->setValueNumberInfo(NULL);

   return 3;
   }




void OMR::ValuePropagation::getParmValues()
   {
   return;
   }




bool OMR::ValuePropagation::isParmInvariant(TR::Symbol *sym)
   {
   int32_t index = sym->getParmSymbol()->getOrdinal();
   return (_parmInfo[index] ? false : true);
   }


bool TR::GlobalValuePropagation::buildInputConstraints(TR::CFGNode *node)
   {
   // Build the input constraints for the given CFG node into _curConstraints.
   // Return true if the node is reachable, false if not.
   //
   EdgeConstraints *constraints;

   bool unreachableInputEdgesFound = false;
   bool reachableInputEdgesFound   = false;

   freeValueConstraints(_curConstraints);

   // Find the first input edge that is reachable
   //
   TR_PredecessorIterator pi(node);
   TR::CFGEdge *edge;
   for (edge = pi.getFirst(); edge; edge = pi.getNext())
      {
      constraints = getEdgeConstraints(edge);
      if (isUnreachablePath(constraints))
         unreachableInputEdgesFound = true;
      else
         break;
      }

   if (edge)
      {
      reachableInputEdgesFound = true;

      // Re-use the constraints from the first reachable edge.
      //
      _vcHandler.setRoot(_curConstraints, _vcHandler.getRoot(constraints->valueConstraints));
      _vcHandler.setRoot(constraints->valueConstraints, NULL);
      for (edge = pi.getNext(); edge; edge = pi.getNext())
         {
         constraints = getEdgeConstraints(edge);

         // Ignore unreachable edges
         //
         if (isUnreachablePath(constraints))
            {
            unreachableInputEdgesFound = true;
            continue;
            }

         //dumpOptDetails(comp(), "Calling from 2 before\n");
         //printValueConstraints(_curConstraints);
         mergeEdgeConstraints(constraints, NULL);
         //dumpOptDetails(comp(), "Calling from 2 after\n");
         //printValueConstraints(_curConstraints);
         }
      }

   // If the node was not unreachable we are done.
   //
   if (reachableInputEdgesFound || !unreachableInputEdgesFound)
      return true;

   // If the only edges coming into this node are unreachable ones, the
   // node is unreachable.
   // In this case don't bother processing the node, and propagate the
   // unreachability to its output edges
   //
   if (trace())
      {
      traceMsg(comp(), "\n\nIgnoring unreachable CFG node %d\n", node->getNumber());
      }

   setUnreachablePath();
   TR_SuccessorIterator si(node);
   for (edge = si.getFirst(); edge; edge = si.getNext())
      {
      }

   return false;
   }


void TR::GlobalValuePropagation::propagateOutputConstraints(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool isNaturalLoop, List<TR::CFGEdge> &outEdges1, List<TR::CFGEdge> *outEdges2)
   {
   // Propagate constraints on the output edges to the parent region
   // There may be two sets of edges (for normal and exception edges)
   //
   TR_Structure *parent = node->getStructure()->getParent();
   if (!parent)
      return;

   // First mark all the out edges in the parent CFG as unreachable
   //
   setUnreachablePath();
   TR_SuccessorIterator si(node);
   TR::CFGEdge *edge;
   for (edge = si.getFirst(); edge; edge = si.getNext())
      {
      createEdgeConstraints(edge, true);
      }

   // Now go through each exit edge, find the corresponding edge in the
   // parent CFG and merge the constraints.
   //
   List<TR::CFGEdge> dummyList(comp()->trMemory());
   if (!outEdges2)
      outEdges2 = &dummyList;

   TR::CFGEdge *parentEdge;
   for (parentEdge = si.getFirst(); parentEdge; parentEdge = si.getNext())
      {
      EdgeConstraints *parentConstraints = getEdgeConstraints(parentEdge);
      int32_t parentTo = parentEdge->getTo()->getNumber();
      TR_TwoListIterator<TR::CFGEdge> exits(outEdges1, *outEdges2);
      bool parentEdgeIsUnreachable = true;
      for (edge = exits.getFirst(); edge; edge = exits.getNext())
         {
         if (edge->getTo()->getNumber() == parentTo)
            {
            EdgeConstraints *constraints = getEdgeConstraints(edge);

            // reset the bit here as parent region
            // might have multiple exit edges; we
            // need to detect the case when at least
            // one of them is reachable so backedge constraints
            // can be propagated
            //

            // Ignore unreachable exit paths
            //
            if (isUnreachablePath(constraints))
               {
               if (parentEdgeIsUnreachable)
                  parentEdgeIsUnreachable = true;
               continue;
               }
            else
               parentEdgeIsUnreachable = false;

            if (isUnreachablePath(parentConstraints))
               {
               // This is the first set of constraints for the parent edge. Use the
               // constraint list from the exit edge.
               //
              freeValueConstraints(parentConstraints->valueConstraints);
              _vcHandler.setRoot(parentConstraints->valueConstraints, _vcHandler.getRoot(constraints->valueConstraints));
              _vcHandler.setRoot(constraints->valueConstraints, NULL);
              }
           else
              {
              // Merge the constraints from the exit edge into the parent edge
              //
              mergeEdgeConstraints(constraints, parentConstraints);
              }
            }
         }

      // If the region is a natural loop, we must take account of the back edges
      // of the loop. If there are constraints on the back edges for value
      // numbers that have not been seen on the exit edge, these constraints
      // must be injected into the parent's edge.
      //
      if (isNaturalLoop && !parentEdgeIsUnreachable)
         {
         mergeBackEdgeConstraints(parentConstraints);
         }
      }
   }

void TR::GlobalValuePropagation::determineConstraints()
   {
   // Go through the regions finding constraints.
   // We will use 2 visit counts, one for the first time through a region node
   // or block and one for the second (and last) time through.
   //
   comp()->incVisitCount();
   _visitCount = comp()->incVisitCount();
   _vcHandler.setRoot(_curConstraints, NULL);
   TR_StructureSubGraphNode rootNode(comp()->getFlowGraph()->getStructure());

   processStructure(&rootNode, true, false);
   }

void TR::GlobalValuePropagation::processStructure(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop)
   {
   static uint32_t numIter = 0;
   if (comp()->getOptions()->realTimeGC() &&
       ((++numIter) & 0xf) == 0 &&
       comp()->compilationShouldBeInterrupted(BEFORE_PROCESS_STRUCTURE_CONTEXT))
      {
      comp()->failCompilation<TR::CompilationInterrupted>("interrupted when starting processStructure()");
      }
   TR_RegionStructure *region = node->getStructure()->asRegion();
   if (region)
      {
      _defMergedNodes->empty();
      if (region->isAcyclic())
         {
         processAcyclicRegion(node, lastTimeThrough, insideLoop);
         }
      else if (region->isNaturalLoop())
         {
         processNaturalLoop(node, lastTimeThrough, insideLoop);
         }
      else
         {
         processImproperLoop(node, lastTimeThrough, insideLoop);
         }
      }
   else
      {
      processBlock(node, lastTimeThrough, insideLoop);
      }
   }

void TR::GlobalValuePropagation::processAcyclicRegion(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop)
   {
   if (trace())
      printStructureInfo(node->getStructure(), true, lastTimeThrough);

   processRegionSubgraph(node, lastTimeThrough, insideLoop, false);

   if (trace())
      printStructureInfo(node->getStructure(), false, lastTimeThrough);
   }

void TR::GlobalValuePropagation::processNaturalLoop(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop)
   {
   TR_RegionStructure *region = node->getStructure()->asRegion();

   LoopInfo *parentLoopInfo = _loopInfo;

   if (!insideLoop)
      {
      // This is an outermost loop. Process all the nodes inside the loop once
      // to get back edge constraints and then again for real.
      //
      TR_ASSERT(lastTimeThrough, "Not last time through outer loop");
      TR_ASSERT(!_loopInfo, "Not expecting loop info");

      _loopInfo = new (trStackMemory()) LoopInfo(region, NULL);
      _loopInfo->_seenDefs = new (trStackMemory()) TR_BitVector(_numValueNumbers, trMemory(), stackAlloc);
      // Save the input constraints and bitvector of value numbers seen. They
      // must be reset for the second time through the loop.
      //
      ValueConstraint *inputConstraints;
      inputConstraints = copyValueConstraints(_curConstraints);

      // Process the loop first time through
      //
      if (trace())
         printStructureInfo(region, true, false);

      _visitCount--;
      processRegionSubgraph(node, false, true, true);

      // having processed the loop the first time we want to make sure to wipe out any
      // seenOnAllPaths information on the back edges - we use this notion only for the current iteration
      for (auto itr = region->getEntry()->getPredecessors().begin(), end = region->getEntry()->getPredecessors().end(); itr != end; ++itr)
         {
         (*_definedOnAllPaths)[*itr] = NULL;
         }

      if (_reachedMaxRelationDepth)
        {
        _loopInfo = parentLoopInfo;
        _visitCount++;
        return;
        }

      if (trace())
         printStructureInfo(region, false, false);

      // Reset saved values and process the loop again.
      //
      freeValueConstraints(_curConstraints);
      _vcHandler.setRoot(_curConstraints, inputConstraints);
      _visitCount++;
      }

   else
      {
      TR_ASSERT(parentLoopInfo, "Expected parent loop info");

      if (!lastTimeThrough)
         {
         // First time through a nested loop. Create loop info for this loop
         // and add it to the parent's loop info. We can then find it the next
         // time through this loop.
         //
         _loopInfo = new (trStackMemory()) LoopInfo(region, parentLoopInfo);
         _loopInfo->_seenDefs = new (trStackMemory()) TR_BitVector(_numValueNumbers, trMemory(), stackAlloc);
         parentLoopInfo->_subLoops.add(_loopInfo);
         }
      else
         {
         // Last time through a nested loop. Find this region's loop info
         // from its parent's info
         //
         for (_loopInfo = parentLoopInfo->_subLoops.getFirst();
              _loopInfo && _loopInfo->_loop != region;
              _loopInfo = _loopInfo->getNext())
            {}
         TR_ASSERT(_loopInfo, "Expected loop info");
         }
      }

   if (trace())
      printStructureInfo(region, true, lastTimeThrough);

   // If this is the last time through the loop, collect info for induction
   // variable entry constraints.
   //
   if (lastTimeThrough)
      collectInductionVariableEntryConstraints();

   processRegionSubgraph(node, lastTimeThrough, true, true);

   // having processed the loop again we again clear out any seenOnAllPaths information on the back edges
   for (auto itr = region->getEntry()->getPredecessors().begin(), end = region->getEntry()->getPredecessors().end(); itr != end; ++itr)
      {
      (*_definedOnAllPaths)[*itr] = NULL;
      }

   if (_reachedMaxRelationDepth)
      {
      _loopInfo = parentLoopInfo;
      return;
      }

   // we now compute the definedOnAllPaths information for the exit edges
   // the exit edge is a union of the exit edge seenOnAllPaths and the intersection of the entry
   // seen on all paths
   TR_BitVector *inboundDefinedOnAllPaths = mergeDefinedOnAllPaths(node);

   // we treat exceptions conservatively for now - we know nothing
   // this could be improved in the future
   if (!node->getExceptionPredecessors().empty())
      inboundDefinedOnAllPaths->empty();

   if (trace())
      {
      traceMsg(comp(), "   defined on all paths for entry of loop %d", region->getNumber());
      inboundDefinedOnAllPaths->print(comp());
      traceMsg(comp(), "\n");
      }

   ListIterator<TR::CFGEdge> it(&region->getExitEdges());
   for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
      {
      if (trace())
         traceMsg(comp(), "   defined on all paths for exit %d->%d:", edge->getFrom()->getNumber(), edge->getTo()->getNumber());

      if ((*_definedOnAllPaths)[edge])
         {
         if (trace())
            {
            ((*_definedOnAllPaths)[edge])->print(comp());
            traceMsg(comp(), "\n");
            }
         (*(*_definedOnAllPaths)[edge]) |= *inboundDefinedOnAllPaths;
         }
      else
         {
         if (trace())
            traceMsg(comp(), " NULL\n");
         (*_definedOnAllPaths)[edge] = inboundDefinedOnAllPaths;
         }

      // the exit edges in the natural loop region do not connect to the parent's nodes
      // so we now find the matching exit edge in the parent region and copy the defined on all paths
      // information onto those edges
      for (auto itr = node->getSuccessors().begin(), end = node->getSuccessors().end(); itr != end; ++itr)
         {
         if ((*itr)->getTo()->getNumber() == edge->getTo()->getNumber())
            {
            TR_BitVector *parentEdgeDefinedOnAllPaths = (*_definedOnAllPaths)[*itr];
            if (parentEdgeDefinedOnAllPaths != NULL)
               {
               *parentEdgeDefinedOnAllPaths &= (*(*_definedOnAllPaths)[edge]);
               }
            else
               {
               parentEdgeDefinedOnAllPaths = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);
               (*_definedOnAllPaths)[*itr] = parentEdgeDefinedOnAllPaths;
               *parentEdgeDefinedOnAllPaths = (*(*_definedOnAllPaths)[edge]);
               }
            break;
            }
         }
      }

   // Back edge constraints have now been accumulated into a separate list of
   // constraints.
   // This list is used for 2 purposes:
   //    1) To get constraints for definitions that have not been seen in the
   //       normal forward analysis. These definitions must appear later in
   //       this loop than their use.
   //    2) To populate exit edges with constraints for definitions that are
   //       seen only after the exit edge in the loop.

   if (!lastTimeThrough)
      {
      // First time through the loop, check the back edges to make
      // sure they are all covered by induction variable increments
      //
      checkBackEdgeCoverage();
      }

   else
      {
      // Last time through the loop, put induction variable info
      // into the region itself
      //
      setUpInductionVariables(node);
      }

   if (trace())
      printStructureInfo(node->getStructure(), false, lastTimeThrough);

   _loopInfo = parentLoopInfo;
   }

void TR::GlobalValuePropagation::processImproperLoop(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop)
   {
   if (trace())
      printStructureInfo(node->getStructure(), true, lastTimeThrough);

   TR_RegionStructure *region = node->getStructure()->asRegion();

   // For each node in the improper region the input is the set of generalized
   // store constraints that are either input to the region or for stores that
   // are done in the region. This set is calculated here.
   //
   ValueConstraints stores;
   generalizeStores(stores, &_curConstraints);

   // Get the store constraints from stores in the region itself
   //
   TR_RegionStructure::Cursor si(*region);
   TR_StructureSubGraphNode *subNode;
   for (subNode = si.getFirst(); subNode; subNode = si.getNext())
      {
      getImproperRegionStores(subNode, stores);
      }


   // Note : this code is commented out (we seem to never analyze an improper
   // region when doing GVP (they are apparently created later on in the optimizer).
   // Since handling improper regions correctly is complicated, we simply skip
   // them; however if we wanted to restart analyzin inside them, we must use code
   // under comment after the below loop.
   //

   /*
   // Process the region nodes.
   //
   for (subNode = si.getFirst(); subNode; subNode = si.getNext())
      {
      freeValueConstraints(_curConstraints);
      _vcHandler.setRoot(_curConstraints, _vcHandler.copyAll(stores));
      processStructure(subNode, lastTimeThrough, insideLoop);
      }
   */

   //printf("Found and analyzing improper region in %s\n", comp()->signature());
   //
   // If we do decide to analyze code inside improper regions, then we
   // need to analyze in proper order (rather than random order as shown above)
   // as there are cases when a conservative constraint for a value may mean
   // we do not fold a branch that would have been folded (if blocks were analyzed
   // in proper order) resulting in analysis of unreachable code which might
   // cause problems when intersecting constraints, for example.
   //
   //freeValueConstraints(_curConstraints);
   //processRegionSubgraph(node, true, true, false);


   // The set of generalized store constrains is added to all exit edges
   // from the region.
   //
   freeValueConstraints(_curConstraints);
   _vcHandler.setRoot(_curConstraints, _vcHandler.copyAll(stores));
   ListIterator<TR::CFGEdge> ei(&region->getExitEdges());
   for (TR::CFGEdge * edge = ei.getFirst(); edge; edge = ei.getNext())
      {
      createEdgeConstraints(edge, true);
      }

   // Now propagate the exit edge constraints to the parent region.
   //
   propagateOutputConstraints(node, lastTimeThrough, false, region->getExitEdges(), NULL);

   freeValueConstraints(_curConstraints);
   freeValueConstraints(stores);

   if (trace())
      printStructureInfo(node->getStructure(), false, lastTimeThrough);
   }

void OMR::ValuePropagation::generalizeStores(ValueConstraints &stores, ValueConstraints *vC)
   {
   ValueConstraint *vc, *newVc;

   // Get the store constraints from the input constraint list
   //
   ValueConstraintIterator iter(*vC);
   for (vc = iter.getFirst(); vc; vc = iter.getNext())
      {
      if (vc->storeRelationships.isEmpty())
         continue;

      newVc = _vcHandler.findOrCreate(vc->getValueNumber(), stores);
      StoreRelationship *rel, *newRel, *prev = NULL;
      for (rel = vc->storeRelationships.getFirst(); rel; rel = rel->getNext())
         {
         newRel = createStoreRelationship(rel->symbol, NULL);
         newVc->storeRelationships.insertAfter(prev, newRel);
         prev = newRel;
         }
      }
   }

void OMR::ValuePropagation::findStoresInBlock(TR::Block *block, ValueConstraints &stores)
   {
   // Scan this block for stores that are to be put into the containing
   // improper region's list of generalized store constraints.
   //
   TR::TreeTop        *treeTop;
   TR::Node           *node;

   for (treeTop = block->getEntry(); treeTop && treeTop != block->getExit(); treeTop = treeTop->getNextTreeTop())
      {
      node = treeTop->getNode();
      if (!node->getOpCode().isStore() && node->getNumChildren() > 0)
         node = node->getFirstChild();
      if (node->getOpCode().isStore())
         {
         ValueConstraint *vc = _vcHandler.findOrCreate(getValueNumber(node), stores);
         StoreRelationship *rel, *prev;
         for (rel = vc->storeRelationships.getFirst(), prev = NULL; rel && rel->symbol < node->getSymbol(); prev = rel, rel = rel->getNext())
            {}
         if (!(rel && rel->symbol == node->getSymbol()))
            {
            rel = createStoreRelationship(node->getSymbol(), NULL);
            vc->storeRelationships.insertAfter(prev, rel);
            }
         }
      }
   }

void TR::GlobalValuePropagation::getImproperRegionStores(TR_StructureSubGraphNode *node, ValueConstraints &stores)
   {
   TR_RegionStructure *region = node->getStructure()->asRegion();
   if (region)
      {
      // Get the store constraints from stores in this region
      //
      TR_RegionStructure::Cursor si(*region);
      TR_StructureSubGraphNode *subNode;
      for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
         {
         getImproperRegionStores(subNode, stores);
         }
      }
   else
      {
      findStoresInBlock(node->getStructure()->asBlock()->getBlock(), stores);
      }
   }

void TR::GlobalValuePropagation::processRegionSubgraph(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop, bool isNaturalLoop)
   {
   // Process the nodes in a region in flow order (not completely possible for
   // an improper region but this method is not called for an improper region).
   //
   // Ignore internal predecessors of the entry node (the back edges). They
   // have been handled before we reach here.
   // Current constraints have already been set up to be the input constraints.
   //
   TR_RegionStructure *region = node->getStructure()->asRegion();
   TR_StructureSubGraphNode *entry = region->getEntry();

   // Process the entry node.
   //
   entry->setVisitCount(_visitCount);
   processStructure(entry, lastTimeThrough, insideLoop);

   // Process the others
   //
   TR_RegionStructure::Cursor si(*region);
   TR_StructureSubGraphNode *subNode;
   for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
      {
      if (subNode != entry && subNode->getVisitCount() != _visitCount)
         {
         processRegionNode(subNode, lastTimeThrough, insideLoop);
         if (_reachedMaxRelationDepth)
            return;
         }
      }

   // If this is a natural loop, collect back edge constraints and then
   // propagate exit edge constraints to the parent region. This propagation
   // uses the accumulated back edge constraint information.
   //
   // Otherwise just propagate the exit edge constraints to the parent region.
   //
   if (isNaturalLoop)
      collectBackEdgeConstraints();
   propagateOutputConstraints(node, lastTimeThrough, isNaturalLoop, region->getExitEdges(), NULL);
   }


void TR::GlobalValuePropagation::processRegionNode(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop)
   {
   node->setVisitCount(_visitCount);

   TR::CFGEdge *edge;

   // Make sure all the node's predecessors have been processed.
   //
   TR_PredecessorIterator pi(node);
   for (edge = pi.getFirst(); edge; edge = pi.getNext())
      {
      TR_StructureSubGraphNode *pred = toStructureSubGraphNode(edge->getFrom());
      if (pred->getVisitCount() != _visitCount)
         processRegionNode(pred, lastTimeThrough, insideLoop);
      }

   // Build up the input constraints for this node
   //
   bool nodeIsReachable = buildInputConstraints(node);

   // If the only edges coming into this node are unreachable ones it is
   // unreachable.
   // In this case don't bother processing the node, and propagate the
   // unreachability to its output edges
   //
   if (!nodeIsReachable)
      {
      if (trace())
         {
         traceMsg(comp(), "\n\nIgnoring unreachable node %d\n", node->getNumber());
         }

      // The current constraint list is already primed with an "UnreachablePath"
      // constraint.
      //
      TR_SuccessorIterator si(node);
      for (edge = si.getFirst(); edge; edge = si.getNext())
         {
         printEdgeConstraints(createEdgeConstraints(edge, true));
         }

      // If the node represents a block, add it to the list of unreachable
      // blocks
      //
      /* if (lastTimeThrough && node->getStructure()->asBlock())
         {
         _blocksToBeRemoved->add(node->getStructure()->asBlock()->getBlock());
         } */

      if (lastTimeThrough)
         {
         if (node->getStructure()->asBlock())
            {
            _blocksToBeRemoved->add(node->getStructure()->asBlock()->getBlock());
            }
         else if (node->getStructure()->asRegion())
            {
            _blocksToBeRemoved->add(node->getStructure()->asRegion()->getEntryBlock());
            }
         }

      return;
      }

   // Process this node
   //
   processStructure(node, lastTimeThrough, insideLoop);
   }

TR_BitVector *TR::GlobalValuePropagation::mergeDefinedOnAllPaths(TR_StructureSubGraphNode *node)
   {
   TR_BitVector *mergeResult = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);

   if (!node->getExceptionPredecessors().empty())
      return mergeResult;

   bool first = true;
   for (auto itr = node->getPredecessors().begin(), end = node->getPredecessors().end(); itr != end; ++itr)
      {
      TR_BitVector *predDefinedOnAllPaths = (*_definedOnAllPaths)[*itr];
      if (trace())
         {
         traceMsg(comp(), "   inbound seenOnAllpaths for edge %d->%d", (*itr)->getFrom()->getNumber(), (*itr)->getTo()->getNumber());
         if (predDefinedOnAllPaths)
            predDefinedOnAllPaths->print(comp());
         else
            traceMsg(comp(), "NULL");
         traceMsg(comp(), "\n");
         }

      if (predDefinedOnAllPaths == NULL)
         mergeResult->empty();
      else if (first)
         (*mergeResult) = *predDefinedOnAllPaths;
      else
         (*mergeResult) &= *predDefinedOnAllPaths;

      first = false;
      }
   return mergeResult;
   }

void TR::GlobalValuePropagation::processBlock(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop)
   {
   TR::CFGEdge *edge;

   TR_BlockStructure *block = node->getStructure()->asBlock();
   _curBlock = block->getBlock();

   TR::TreeTop *startTree = _curBlock->getEntry();
   if (!startTree)
      return;

   if (_blocksToProcess && !_blocksToProcess->isSet(block->getNumber()))
      {
      if (performTransformation(comp(), "%sSkip block_%d deemed unimportant\n", OPT_DETAILS, block->getNumber()))
         return;
      }

   if (trace())
      {
      traceMsg(comp(), "GVP: Processing block_%i\n", _curBlock->getNumber());
      }

   // inside of a loop we track the defined on all paths bitvector so we know when a merge of def constraints
   // may need to consider the backedge - here we compute the block's inbound seen on all paths set by intersecting
   // the sets on the inbound edges
   if (insideLoop)
      {
      _curDefinedOnAllPaths = mergeDefinedOnAllPaths(node);
      }

   if (insideLoop && trace())
      {
      traceMsg(comp(), "   defined on all paths for entry of block %d", block->getNumber());
      _curDefinedOnAllPaths->print(comp());
      traceMsg(comp(), "\n");
      }

#if DEBUG
   static int32_t stopAtBlock = -1;
   if (_curBlock->getNumber() == stopAtBlock)
      {
      dumpOptDetails(comp(), "Stop here\n");
      }
#endif

   // Remember if this is the entry block for a loop
   //
   if (_loopInfo && _loopInfo->_loop->getNumber() == _curBlock->getNumber())
      _loopInfo->_entryBlock = _curBlock;

   // Prime the exception out edges as "unreachable". For those that are reached
   // the "unreachable" constraint will be replaced by the actual constraints
   // for the exception path.
   //
   if (!_curBlock->getExceptionSuccessors().empty())
      {
      // Save the real contents of the block constraints and create the
      // "unreachable" constraint that is to be propagated to the exception out
      // edges
      //
      ValueConstraint *saveConstraints = _vcHandler.getRoot(_curConstraints);
      _vcHandler.setRoot(_curConstraints, NULL);
      setUnreachablePath();
      for (auto edge= _curBlock->getExceptionSuccessors().begin(); edge != _curBlock->getExceptionSuccessors().end(); ++edge)
         {
         createEdgeConstraints(*edge, true);
         }

      freeValueConstraints(_curConstraints);

      _vcHandler.setRoot(_curConstraints, saveConstraints);
      }

   if (comp()->getStartBlock()->getNumber() == _curBlock->getNumber())
      {
      addConstraintToList(NULL, _syncValueNumber, AbsoluteConstraint, TR::VPSync::create(this, TR_no), &_curConstraints);
      }

   if (trace())
      printStructureInfo(node->getStructure(), true, lastTimeThrough);

   bool hasIncomingStoreRelationships = false;
   ValueConstraintIterator iter(_curConstraints);
   for (ValueConstraint *vc = iter.getFirst(); vc; vc = iter.getNext())
      {
      if (!vc->storeRelationships.isEmpty())
         {
         hasIncomingStoreRelationships = true;
         break;
         }
      }

   // Go through the trees finding constraints
   //
   _lastTimeThrough = lastTimeThrough;
   _booleanNegationInfo.setFirst(NULL);

   TR::TreeTop *endTree = _curBlock->getExit();
   processTrees(startTree, endTree);
   if (_reachedMaxRelationDepth)
      return;

   // inside a loop we will now propagate the seen on all paths definitions computed through the block onto
   // the exit edges
   if (insideLoop)
      {
      for (auto itr = node->getSuccessors().begin(), end = node->getSuccessors().end(); itr != end; ++itr)
         {
         if (trace())
            {
            traceMsg(comp(), "   outbound seenOnAllpaths for edge %d->%d", (*itr)->getFrom()->getNumber(), (*itr)->getTo()->getNumber());
            _curDefinedOnAllPaths->print(comp());
            traceMsg(comp(), "\n");
            }
         (*_definedOnAllPaths)[*itr] = _curDefinedOnAllPaths;
         }
      }

   if (!isUnreachablePath(_curConstraints))
      {
      TR_ASSERT(endTree->getNextTreeTop(), "Fall-through last block");
      TR::Block *nextBlock = endTree->getNextTreeTop()->getNode()->getBlock();

      // Find the fall-through edge
      //
      if (trace())
         traceMsg(comp(),"Calling findOutEdge using _curBlock %d and nextBlock %d\n",_curBlock->getNumber(),nextBlock->getNumber());

      edge = findOutEdge(_curBlock->getSuccessors(), nextBlock);
      if (trace())
         traceMsg(comp(), "Processing %s [%p]\n   Fall-through to next block\n", endTree->getNode()->getOpCode().getName(), endTree->getNode());
      printEdgeConstraints(createEdgeConstraints(edge, false));
      }

   if (trace())
      printStructureInfo(node->getStructure(), false, lastTimeThrough);

   // Check to make sure we are not dropping store constraints.
   // If we are, it is probably a bug.
   if (hasIncomingStoreRelationships)
      {
      TR::Block *nextBlock = endTree->getNextTreeTop() ? endTree->getNextTreeTop()->getNode()->getBlock() : 0;
      for (auto edge = _curBlock->getSuccessors().begin(); edge != _curBlock->getSuccessors().end(); ++edge)
         {
         if (getEdgeConstraints(*edge)->valueConstraints.isEmpty())
            {
            TR_ASSERT((*edge)->getTo()->getNumber() == 1, "Empty set of constraints on edge to block %d\n", (*edge)->getTo()->getNumber());
            }
         }
      }

   // Propagate constraints on the output edges to the parent region, if any
   // Create temporary List structures to pass to this function. Will be eliminated once getExitEdges has migrated to STL
   List<TR::CFGEdge> List1(comp()->trMemory());
   List<TR::CFGEdge> List2(comp()->trMemory());
   for (auto iter = _curBlock->getSuccessors().begin(); iter != _curBlock->getSuccessors().end(); ++iter)
      List1.add(*iter);
   for (auto iter = _curBlock->getExceptionSuccessors().begin(); iter != _curBlock->getExceptionSuccessors().end(); ++iter)
      List2.add(*iter);
   propagateOutputConstraints(node, lastTimeThrough, false, List1, &List2);
   }

const char *
TR::GlobalValuePropagation::optDetailString() const throw()
   {
   return "O^O GLOBAL VALUE PROPAGATION: ";
   }

TR::CFGEdge *OMR::ValuePropagation::findOutEdge(TR::CFGEdgeList &edges, TR::CFGNode *target)
   {
   // Find the output edge in the list of edges to the given node
   //
   auto edge = edges.begin();
   for (; (edge != edges.end()) && ((*edge)->getTo() != target); ++edge)
      {}
   TR_ASSERT(edge != edges.end(), "Missing edge in the CFG");
   return *edge;
   }

bool OMR::ValuePropagation::isUnreachablePath(ValueConstraints &valueConstraints)
   {
   return !valueConstraints.isEmpty() &&
          !_vcHandler.getRoot(valueConstraints)->relationships.isEmpty() &&
          _vcHandler.getRoot(valueConstraints)->relationships.getFirst()->constraint->asUnreachablePath();
   }

bool OMR::ValuePropagation::isUnreachablePath(EdgeConstraints *constraints)
   {
   return isUnreachablePath(constraints->valueConstraints);
   }

void OMR::ValuePropagation::setUnreachablePath()
   {
#if 0
   freeValueConstraints(_curConstraints);
   addConstraintToList(NULL, 0, AbsoluteConstraint, TR::VPUnreachablePath::create(this), &_curConstraints);
#else
   setUnreachablePath(_curConstraints);
#endif
   }

void OMR::ValuePropagation::setUnreachablePath(ValueConstraints &vc)
   {
   freeValueConstraints(vc);
   addConstraintToList(NULL, 0, AbsoluteConstraint, TR::VPUnreachablePath::create(this), &vc);
   }

void OMR::ValuePropagation::setUnreachablePath(TR::CFGEdge *edge)
   {
   if (!_isGlobalPropagation)
      return;

   EdgeConstraints *constraints = getEdgeConstraints(edge);
#if 0
   freeValueConstraints(constraints->valueConstraints);
   addConstraintToList(NULL, 0, AbsoluteConstraint, TR::VPUnreachablePath::create(this), &constraints->valueConstraints);
#else
   setUnreachablePath(constraints->valueConstraints);
#endif
   }


void OMR::ValuePropagation::printStructureInfo(TR_Structure *s, bool starting, bool lastTimeThrough)
   {
   traceMsg(comp(), "\n%s ", starting ? "Starting " : "Stopping ");
   char *type;
   bool isLoop = false;
   if (s->asRegion())
      {
      TR_RegionStructure *region = s->asRegion();
      if (region->isAcyclic())
         type = "acyclic region";
      else if (region->isNaturalLoop())
         {
         type = "natural loop";
         isLoop = true;
         }
      else
         type = "improper region";
      }
   else
      type = "block";
   traceMsg(comp(), "%s ",type);

   printParentStructure(s);
   traceMsg(comp(), "%d", s->getNumber());
   if (lastTimeThrough)
      traceMsg(comp(), " last time through\n");
   else
      traceMsg(comp(), " first time through\n");

   if (starting)
      {
      printGlobalConstraints();
      traceMsg(comp(), "   Starting edge constraints:\n");
      if (_curConstraints.isEmpty())
         {
         traceMsg(comp(), "      NONE\n");
         }
      else
         {
         printValueConstraints(_curConstraints);
         }

      if (isLoop && lastTimeThrough)
         {
         traceMsg(comp(), "   Back edge constraints:\n");
         if (!_loopInfo->_backEdgeConstraints ||
             _loopInfo->_backEdgeConstraints->valueConstraints.isEmpty())
            {
            traceMsg(comp(), "      NONE\n");
            }
         else
            {
            printValueConstraints(_loopInfo->_backEdgeConstraints->valueConstraints);
            }
         }
      }
   }

void OMR::ValuePropagation::printParentStructure(TR_Structure *s)
   {
   if (s->getParent())
      {
      printParentStructure(s->getParent());
      traceMsg(comp(), "%d->",s->getParent()->getNumber());
      }
   }


void OMR::ValuePropagation::printValueConstraints(ValueConstraints &valueConstraints)
   {
   ValueConstraintIterator iter(valueConstraints);
   ValueConstraint *vc;
   for (vc = iter.getFirst(); vc; vc = iter.getNext())
      {
      vc->print(this, 6);
      }
   }

void OMR::ValuePropagation::printGlobalConstraints()
   {
   traceMsg(comp(), "   Global constraints:\n");
   for (int32_t i = 0; i <= _globalConstraintsHTMaxBucketIndex; i++)
      {
      GlobalConstraint *entry;
      for (entry = _globalConstraintsHashTable[i]; entry; entry = entry->next)
         {
         for (Relationship *rel = entry->constraints.getFirst(); rel; rel = rel->getNext())
            {
            traceMsg(comp(), "      global");
            rel->print(this, entry->valueNumber, 1);
            }
         }
      }
   }

void OMR::ValuePropagation::printEdgeConstraints(EdgeConstraints *constraints)
   {
   if (!_isGlobalPropagation || comp()->getOutFile() == NULL)
      return;

   if (trace())
      {
      TR::CFGNode *from = constraints->edge->getFrom();
      TR::CFGNode *to   = constraints->edge->getTo();
      traceMsg(comp(), "   Edge %d->%d", from->getNumber(), to->getNumber());
      if (isUnreachablePath(constraints))
         {
         traceMsg(comp(), " is unreachable\n");
         }
      else if (constraints->valueConstraints.isEmpty())
         {
         traceMsg(comp(), " has no constraints\n");
         }
      else
         {
         traceMsg(comp(), " constraints:\n");
         printValueConstraints(constraints->valueConstraints);
         }
      }
   }

void OMR::ValuePropagation::Relationship::print(OMR::ValuePropagation *vp)
   {
   if (vp->comp()->getOutFile() == NULL)
      return;
   if (relative == AbsoluteConstraint)
      {
      // An absolute store constraint can have a null constraint value.
      //
      if (constraint)
         constraint->print(vp->comp(), vp->comp()->getOutFile());
      else
         traceMsg(vp->comp(), "generalized");
      }
   else
      constraint->print(vp->comp(), vp->comp()->getOutFile(), relative);
   }

void OMR::ValuePropagation::Relationship::print(OMR::ValuePropagation *vp, int32_t valueNumber, int32_t indent)
   {
   if (vp->comp()->getOutFile() == NULL)
      return;
   TR_FrontEnd *fe = vp->fe();
   if (valueNumber < vp->_firstUnresolvedSymbolValueNumber)
      {
      trfprintf(vp->comp()->getOutFile(), "%*.svalue %d is ", indent, " ",valueNumber);
      print(vp);
      }
   else if (valueNumber < vp->_firstInductionVariableValueNumber)
      {
      // Symbol resolution constraint
      //
      trfprintf(vp->comp()->getOutFile(), "%*.ssymbol %d is resolved", indent, " ", valueNumber-vp->_firstUnresolvedSymbolValueNumber);
      }
   else
      {
      // Induction variable use constraint
      //
      OMR::ValuePropagation::InductionVariable *iv;
      for (iv = vp->_loopInfo->_inductionVariables.getFirst(); iv; iv = iv->getNext())
         {
         if (iv->_valueNumber == valueNumber)
            break;
         }
      if (iv)
         trfprintf(vp->comp()->getOutFile(), "%*.sinduction variable %d [%p]", indent, " ", valueNumber-vp->_firstInductionVariableValueNumber, iv->_symbol);
      else
         trfprintf(vp->comp()->getOutFile(), "%*.sparent induction variable %d", indent, " ", valueNumber-vp->_firstInductionVariableValueNumber);
      trfprintf(vp->comp()->getOutFile(), " used by value number(s) ");
      constraint->print(vp->comp(), vp->comp()->getOutFile());
      }
   trfprintf(vp->comp()->getOutFile(), "\n");
   }

void OMR::ValuePropagation::StoreRelationship::print(OMR::ValuePropagation *vp, int32_t valueNumber, int32_t indent)
   {
   if (vp->comp()->getOutFile() == NULL)
      return;

   TR_FrontEnd *fe = vp->fe();
   if (relationships.getFirst())
      for (Relationship *rel = relationships.getFirst(); rel; rel = rel->getNext())
         {
         trfprintf(vp->comp()->getOutFile(), "%*.ssymbol %p store", indent, " ", symbol);
         rel->print(vp, valueNumber, 1);
         }
   else
      trfprintf(vp->comp()->getOutFile(), "%*.sptr %p symbol %p has no relationships\n", indent, " ", this, symbol);
   }

void OMR::ValuePropagation::ValueConstraint::print(OMR::ValuePropagation *vp, int32_t indent)
   {
   if (vp->comp()->getOutFile() == NULL)
      return;
   for (Relationship *rel = relationships.getFirst(); rel; rel = rel->getNext())
      rel->print(vp, getValueNumber(), indent);
   for (StoreRelationship *storeRel = storeRelationships.getFirst(); storeRel; storeRel = storeRel->getNext())
      storeRel->print(vp, getValueNumber(), indent);
   }


// routines to support removal of
// constraints if intersection fails
// for a particular value number
//
bool OMR::ValuePropagation::removeConstraints()
   {
   static char *p = feGetEnv("TR_FixIntersect");
   if (!p)
      return false;
   else
      return true;
   }

bool OMR::ValuePropagation::removeConstraints(int32_t valueNumber, ValueConstraints *vc, bool findStores)
   {
   if (trace())
      {
      traceMsg(comp(), "   Cannot intersect constraints!\n");
      traceMsg(comp(), "   Intersection of constraints failed for valueNumber [%d], removing constraints\n", valueNumber);
      }

   if (!vc)
      vc = &_curConstraints;

   ValueConstraints stores;
   generalizeStores(stores, vc);

   if (findStores)
      findStoresInBlock(_curBlock, stores);

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   if (_isGlobalPropagation)
      removeConstraints(valueNumber);
   if (trace())
      traceMsg(comp(), "   Setting fall-through as unreachable\n");
   // mark current X as unreachable
   // X = block
   if (findStores)
      {
      if (trace())
         traceMsg(comp(), "   block following block_%d is unreachable\n", _curBlock->getNumber());
      setUnreachablePath();
      }
   // there are no edgeConstraints for localVP
   // X = edge
   else if (_isGlobalPropagation)
      setUnreachablePath(*vc);
#else
   freeValueConstraints(*vc);
   _vcHandler.setRoot(*vc, _vcHandler.copyAll(stores));
#endif
   return true;
   }



// this routine removes both valueconstraints
// and store relationships collected on the vn so far
//
bool OMR::ValuePropagation::removeConstraints(int32_t valueNumber, ValueConstraints *valueConstraints)
   {
   if (trace())
      traceMsg(comp(), "   Intersection of constraints failed for valueNumber [%d], removing constraints\n", valueNumber);

   // remove globalConstraints on this valueNumber
   //
   if (!valueConstraints)
      return removeConstraints(valueNumber);


   ValueConstraint *vc = _vcHandler.find(valueNumber, *valueConstraints);
   if (vc)
      {
      Relationship *rel, *next;
      for (rel = vc->relationships.getFirst(); rel; rel = next)
         {
         next = rel->getNext();
         if (rel->relative != AbsoluteConstraint)
            {
            // this is a relationship, remove all
            // relationships to this valueNumber from the
            // constraint list in the relative
            //
            removeConstraint(rel->relative, *valueConstraints, valueNumber);
            }

         if (trace())
            {
            traceMsg(comp(), "   removing absoulte constraint:\n");
            rel->print(this, valueNumber, 6);
            }
         vc->relationships.remove(rel);
         freeRelationship(rel);
         }
      // remove storerelationships associated with this valueNumber
      //
      StoreRelationship *curStore, *nextStore;
      for (curStore = vc->storeRelationships.getFirst(); curStore; curStore = nextStore)
          {
          nextStore = curStore->getNext();
          Relationship *rel;
          for (rel = curStore->relationships.getFirst(); rel; rel = rel->getNext())
              {
              if (rel->relative != AbsoluteConstraint)
                 removeStoreConstraints(valueConstraints, rel->relative, valueNumber);
              if (trace())
                 {
                 traceMsg(comp(), "   removing absolute store constraint:\n");
                 rel->print(this, valueNumber, 6);
                 }
              }
          vc->storeRelationships.remove(curStore);
          freeStoreRelationship(curStore);
          }
      return true;
      }
   return false;
   }


bool OMR::ValuePropagation::removeStoreConstraints(ValueConstraints *valueConstraints, int32_t valueNumber, int32_t relative)
   {
   ValueConstraint *vc = _vcHandler.find(valueNumber, *valueConstraints);
   if (vc)
      {
      StoreRelationship *curStore;
      for (curStore = vc->storeRelationships.getFirst(); curStore; curStore = curStore->getNext())
          {
          Relationship *rel, *next;
          for (rel = curStore->relationships.getFirst(); rel; rel = next)
              {
              next = rel->getNext();

              if (rel->relative == relative)
                 {
                 if (trace())
                    {
                    traceMsg(comp(), "   removing store relationship:\n");
                    rel->print(this, rel->relative, 6);
                    }
                 curStore->relationships.remove(rel);
                 freeRelationship(rel);
                 break;
                 }
              }
          }
      return true;
      }
   return false;
   }

bool OMR::ValuePropagation::removeConstraints(int32_t valueNumber)
   {
   if (trace())
      traceMsg(comp(), "   Intersection failed for value number [%d], removing global constraints\n", valueNumber);

   GlobalConstraint *gc = findGlobalConstraint(valueNumber);
   if (!gc)
      return false;
   Relationship *rel, *next;
   for (rel = gc->constraints.getFirst(); rel; rel = next)
      {
      next = rel->getNext();
      if (rel->relative != AbsoluteConstraint)
         {
         // this is a relative constraint;
         // remove all constraints associated
         // with this valueNumber on the relatives
         //
         GlobalConstraint *relGC = findGlobalConstraint(rel->relative);
         if (relGC)
            {
            Relationship *cur, *nextCur;
            bool foundRel = false;
            for (cur = relGC->constraints.getFirst(); cur; cur = nextCur)
               {
               nextCur = cur->getNext();
               if (cur->relative == valueNumber)
                  {
                  foundRel = true;
                  break;
                  }
               }
            if (foundRel)
               {
               if (trace())
                  {
                  traceMsg(comp(), "   removing global relationship:\n");
                  rel->print(this, cur->relative, 6);
                  }
               gc->constraints.remove(cur);
               freeRelationship(cur);
               }
            }
         }
      // remove this relationship
      if (trace())
         {
         traceMsg(comp(), "   removing global absolute constraint:\n");
         rel->print(this, valueNumber, 6);
         }
      gc->constraints.remove(rel);
      freeRelationship(rel);
      }
   return true;
   }

TR::TreeTop* TR::ArraycopyTransformation::createArrayNode(TR::TreeTop* tree, TR::TreeTop* newTree, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef, TR::SymbolReference* lenRef, TR::SymbolReference* srcObjRef, TR::SymbolReference* dstObjRef, bool isForward)
   {
   TR::Node* root = tree->getNode()->getFirstChild();
   TR::Node* len = TR::Node::createLoad(root, lenRef);

   return createArrayNode(tree, newTree, srcRef, dstRef, len, srcObjRef, dstObjRef, isForward);
   }

TR::TreeTop* TR::ArraycopyTransformation::createArrayNode(TR::TreeTop* tree, TR::TreeTop* newTree, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef, TR::Node* len, TR::SymbolReference* srcObjRef, TR::SymbolReference* dstObjRef, bool isForward)
   {
   TR::Node* root = tree->getNode()->getFirstChild();
   TR::Node* src = NULL;
   TR::Node* dst = NULL;
   TR::Node* srcObj = NULL;
   TR::Node* dstObj = NULL;
   TR::Node *node = NULL;

   if (!root->isReferenceArrayCopy())
      {
      //src = TR::Node::createLoad(root, srcRef);
      if (srcRef)
         src = TR::Node::createLoad(root, srcRef);
      else
         {
         if (root->getNumChildren() == 3)
            src = root->getFirstChild()->duplicateTree();
         else
            src = root->getChild(2)->duplicateTree();
         }

      //dst = TR::Node::createLoad(root, dstRef);
      if (dstRef)
         dst = TR::Node::createLoad(root, dstRef);
      else
         {
         if (root->getNumChildren() == 3)
            dst = root->getSecondChild()->duplicateTree();
         else
            dst = root->getChild(3)->duplicateTree();
         }
      node = TR::Node::createArraycopy(src, dst, len);
      node->setNumChildren(3);
      ///node->setReferenceArrayCopy(false);
      if (trace())
         traceMsg(comp(), "Created 3-child arraycopy %s from root node %s, ", comp()->getDebug()->getName(node), comp()->getDebug()->getName(root));
      }
   else
      {
      //src = TR::Node::createLoad(root, srcRef);
      if (srcRef)
         src = TR::Node::createLoad(root, srcRef);
      else
         {
         if (root->getNumChildren() == 3)
            src = root->getFirstChild()->duplicateTree();
         else
            src = root->getChild(2)->duplicateTree();
         }

      //dst = TR::Node::createLoad(root, dstRef);
      if (dstRef)
         dst = TR::Node::createLoad(root, dstRef);
      else
         {
         if (root->getNumChildren() == 3)
            dst = root->getSecondChild()->duplicateTree();
         else
            dst = root->getChild(3)->duplicateTree();
         }

      srcObj = TR::Node::createLoad(root, srcObjRef);
      dstObj = TR::Node::createLoad(root, dstObjRef);
      node = TR::Node::createArraycopy(srcObj, dstObj, src, dst, len);
      node->setNumChildren(5);
      ////node->setReferenceArrayCopy(true);
      node->setNoArrayStoreCheckArrayCopy(root->isNoArrayStoreCheckArrayCopy());

      if (root->isHalfWordElementArrayCopy())
         node->setHalfWordElementArrayCopy(root->isHalfWordElementArrayCopy());
      else if (root->isWordElementArrayCopy())
         node->setWordElementArrayCopy(root->isWordElementArrayCopy());
      if (trace())
         traceMsg(comp(), "Created 5-child arraycopy %s from root node %s, ", comp()->getDebug()->getName(node), comp()->getDebug()->getName(root));
      }

   node->setArrayCopyElementType(root->getArrayCopyElementType());
   node->setSymbolReference(root->getSymbolReference());

   //node->setNonDegenerateArrayCopy(root->isNonDegenerateArrayCopy());
   node->setForwardArrayCopy(isForward);
   node->setBackwardArrayCopy(!isForward);

   if (trace())
      traceMsg(comp(), "type = %s, isForward = %d\n", TR::DataType::getName(node->getArrayCopyElementType()), isForward);

   // duplicate the tree just to copy either the ResolveCHK or the tree-top
   TR::Node* treeNode = tree->getNode()->duplicateTree();

   treeNode->setAndIncChild(0, node);

   newTree->setNode(treeNode);

   if (!isForward)
      {
      tree->getEnclosingBlock()->setIsCold();
      tree->getEnclosingBlock()->setFrequency(REVERSE_ARRAYCOPY_COLD_BLOCK_COUNT);
      }

   return newTree;
   }

int64_t TR::ArraycopyTransformation::arraycopyHighFrequencySpecificLength(TR::Node* arrayCopyNode)
   {
#ifdef J9_PROJECT_SPECIFIC
   const float MIN_ARRAYCOPY_FREQ_FOR_SPECIALIZATION = 0.7f;
   if (comp()->getRecompilationInfo())
      {
      if (comp()->target().is64Bit())
         {
         TR_LongValueInfo *valueInfo = static_cast<TR_LongValueInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(arrayCopyNode, comp(), LongValueInfo));
         if (valueInfo && valueInfo->getTopProbability() > MIN_ARRAYCOPY_FREQ_FOR_SPECIALIZATION)
            {
            return ((int64_t) valueInfo->getTopValue());
            }
         }
      else
         {
         TR_ValueInfo *valueInfo = static_cast<TR_ValueInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(arrayCopyNode, comp(), ValueInfo));
         if (valueInfo && valueInfo->getTopProbability() > MIN_ARRAYCOPY_FREQ_FOR_SPECIALIZATION)
            {
            return ((int64_t) valueInfo->getTopValue());
            }
         }
      }
#endif

   return -1;
   }

TR::TreeTop* TR::ArraycopyTransformation::createPointerCompareNode(TR::Node* node, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef)
   {
   bool is64Bit = comp()->target().is64Bit();
   TR::Node* cmp;
   TR::Node* src; // = TR::Node::createLoad(node, srcRef);
   if (srcRef)
      src = TR::Node::createLoad(node, srcRef);
   else
      {
      if (node->getNumChildren() == 3)
         src = node->getFirstChild()->duplicateTree();
      else
         src = node->getChild(2)->duplicateTree();
      }

   TR::Node* dst; // = TR::Node::createLoad(node, dstRef);
   if (dstRef)
      dst = TR::Node::createLoad(node, dstRef);
   else
      {
      if (node->getNumChildren() == 3)
         dst = node->getSecondChild()->duplicateTree();
      else
         dst = node->getChild(3)->duplicateTree();
      }

   cmp = TR::Node::createif(TR::ifacmpge, src, dst, NULL);

   TR::TreeTop *cmpTree = TR::TreeTop::create(comp(), cmp);
   return cmpTree;
   }

TR::TreeTop* TR::ArraycopyTransformation::createRangeCompareNode(TR::Node* node, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef, TR::SymbolReference* lenRef)
   {
   bool is64Bit = comp()->target().is64Bit();
   TR::Node* cmp;
   TR::Node* src; // = TR::Node::createLoad(node, srcRef);
   if (srcRef)
      src = TR::Node::createLoad(node, srcRef);
   else
      {
      if (node->getNumChildren() == 3)
         src = node->getFirstChild()->duplicateTree();
      else
         src = node->getChild(2)->duplicateTree();
      }

   TR::Node* dst; // = TR::Node::createLoad(node, dstRef);
   if (dstRef)
      dst = TR::Node::createLoad(node, dstRef);
   else
      {
      if (node->getNumChildren() == 3)
         dst = node->getSecondChild()->duplicateTree();
      else
         dst = node->getChild(3)->duplicateTree();
      }


   TR::Node* len = TR::Node::createLoad(node, lenRef);

   TR::Node *srcEndNode;
   if (is64Bit)
      {
      if (len->getType().isInt32())
         srcEndNode = TR::Node::create(TR::aladd, 2, src,
                      TR::Node::create(TR::i2l, 1, len));
      else
         srcEndNode = TR::Node::create(TR::aladd, 2, src, len);
      }
   else
      {
      srcEndNode = TR::Node::create(TR::aiadd, 2, src, len);
      }

   cmp = TR::Node::createif(TR::ifacmpgt, srcEndNode, dst, NULL);

   TR::TreeTop *cmpTree = TR::TreeTop::create(comp(), cmp);

   return cmpTree;
   }


TR::TreeTop* TR::ArraycopyTransformation::createMultipleArrayNodes(TR::TreeTop* arrayTree, TR::Node* node)
   {
     if (/* node->isReferenceArrayCopy() || */ node->isRarePathForwardArrayCopy() || node->isBackwardArrayCopy())
      {
      return arrayTree;
      }
   bool alreadyForwardArrayCopy = node->isForwardArrayCopy();

   TR::CFG *cfg = comp()->getFlowGraph();

   TR::Block * arraycopyBlock  = arrayTree->getEnclosingBlock();
   TR::TreeTop* outerArraycopyTree = NULL;

   TR::Node* src = node->getChild(0);
   TR::Node* dst = node->getChild(1);
   TR::Node* len = node->getChild(2);

   // if a specific length of copy is known to occur frequently, special case
   // it since we can do a well-known length copy more efficiently than a
   // variable length copy.
   int64_t specificLength = arraycopyHighFrequencySpecificLength(node);

   // if this is already a forward arraycopy and there is no specific length known, no
   // further processing required.
   if (alreadyForwardArrayCopy && (specificLength < 0 || len->getOpCode().isLoadConst()))
      {
      return arrayTree;
      }

   setChangedTrees(true);

   TR::SymbolReference *srcObjRef = NULL;
   TR::SymbolReference *dstObjRef = NULL;
   TR::SymbolReference *srcRef = NULL;
   TR::SymbolReference *dstRef = NULL;
   TR::SymbolReference *lenRef = NULL;

   TR::TreeTop *firstInsertedTree = createStoresForArraycopyChildren(comp(), arrayTree, srcObjRef, dstObjRef, srcRef, dstRef, lenRef);

   TR::Block * followOnBlock = NULL;
   if (!alreadyForwardArrayCopy)
      {
      TR::TreeTop* arraycopyForward = TR::TreeTop::create(comp());
      TR::TreeTop* arraycopyBackward = TR::TreeTop::create(comp());

      TR::TreeTop* compareTree = createPointerCompareNode(node, srcRef, dstRef);
      TR::TreeTop* outerElseTree = createRangeCompareNode(node, srcRef, dstRef, lenRef);

      createArrayNode(arrayTree, arraycopyForward, srcRef, dstRef, lenRef, srcObjRef, dstObjRef, true);
      createArrayNode(arrayTree, arraycopyBackward, srcRef, dstRef, lenRef, srcObjRef, dstObjRef, false);

      followOnBlock = arraycopyBlock->createConditionalBlocksBeforeTree(arrayTree, compareTree, arraycopyBackward, outerElseTree, cfg);

      TR::Block *forwardArrayCopyBlock = TR::Block::createEmptyBlock(node, comp(), arraycopyBlock->getFrequency(), arraycopyBlock);
      forwardArrayCopyBlock->setIsExtensionOfPreviousBlock(false);

      TR::TreeTop *forwardArrayCopyEntry = forwardArrayCopyBlock->getEntry();
      TR::TreeTop *forwardArrayCopyExit  = forwardArrayCopyBlock->getExit();

      TR::Block * outerElseBlock   = outerElseTree->getEnclosingBlock();
      TR::TreeTop* outerElseExit  = outerElseBlock->getExit();
      TR::TreeTop* outerElseEntry = outerElseBlock->getEntry();
      outerElseExit->join(forwardArrayCopyEntry);
      forwardArrayCopyEntry->join(arraycopyForward);
      arraycopyForward->join(forwardArrayCopyExit);
      forwardArrayCopyExit->join(followOnBlock->getEntry());

      TR::Block * ifBlock = arraycopyBackward->getEnclosingBlock();
      TR::TreeTop* ifEntry = ifBlock->getEntry();
      compareTree->getNode()->setBranchDestination(forwardArrayCopyEntry);
      outerElseTree->getNode()->setBranchDestination(ifEntry);

      cfg->addNode(forwardArrayCopyBlock);

      cfg->addEdge(TR::CFGEdge::createEdge(outerElseBlock,  ifBlock, trMemory()));
      cfg->addEdge(TR::CFGEdge::createEdge(outerElseBlock,  forwardArrayCopyBlock, trMemory()));
      cfg->addEdge(TR::CFGEdge::createEdge(arraycopyBlock,  forwardArrayCopyBlock, trMemory()));
      cfg->addEdge(TR::CFGEdge::createEdge(forwardArrayCopyBlock,  followOnBlock, trMemory()));
      cfg->copyExceptionSuccessors(ifBlock, forwardArrayCopyBlock);

      cfg->removeEdge(outerElseBlock->getSuccessors(), outerElseBlock->getNumber(), followOnBlock->getNumber());
      cfg->removeEdge(arraycopyBlock->getSuccessors(), arraycopyBlock->getNumber(), ifBlock->getNumber());

      outerArraycopyTree = arraycopyForward;
      }
   else
      {
      outerArraycopyTree = arrayTree; // if already forward, just use the original arraycopy
      }

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after forward/backward arraycopy transformation");
      cfg->comp()->getDebug()->print(cfg->comp()->getOutFile(), cfg);
      }

   arraycopyBlock = outerArraycopyTree->getEnclosingBlock();
   if (specificLength >= 0)
      specializeForLength(outerArraycopyTree, node, (uintptr_t) specificLength, srcRef, dstRef, lenRef, srcObjRef, dstObjRef);

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after arraycopy frequency specialization");
      cfg->comp()->getDebug()->print(cfg->comp()->getOutFile(), cfg);
      }

   return firstInsertedTree;
   }

TR::TreeTop* TR::ArraycopyTransformation::tryToSpecializeForLength(TR::TreeTop *tt, TR::Node *arraycopyNode)
   {
   TR::Node *lengthChild = arraycopyNode->getChild(arraycopyNode->getNumChildren()-1); // last child is length
   if (arraycopyNode->isRarePathForwardArrayCopy() || lengthChild->getOpCode().isLoadConst())
      return tt; // already specialized

   if (!arraycopyNode->isForwardArrayCopy())
      return tt; // can only specialize forward arraycopies because the rare path flag is "isRarePathForwardArrayCopy"

   int64_t specificLength = arraycopyHighFrequencySpecificLength(arraycopyNode); // TODO: What about 64-bit lengths on 64-bit platforms?
   if (specificLength >= 0
      && performTransformation(comp(), "%sSpecializing arraycopy %s for length of %d bytes\n", OPT_DETAILS, comp()->getDebug()->getName(arraycopyNode), specificLength))
      {
      TR::SymbolReference *srcObjRef = NULL;
      TR::SymbolReference *dstObjRef = NULL;
      TR::SymbolReference *srcRef = NULL;
      TR::SymbolReference *dstRef = NULL;
      TR::SymbolReference *lenRef = NULL;
      TR::TreeTop *firstInsertedTree = createStoresForArraycopyChildren(comp(), tt, srcObjRef, dstObjRef, srcRef, dstRef, lenRef);
      specializeForLength(tt, arraycopyNode, (uintptr_t) specificLength, srcRef, dstRef, lenRef, srcObjRef, dstObjRef);
      return firstInsertedTree;
      }
   else
      {
      return tt;
      }
   }

static TR::Node *addressSizedConst(TR::Compilation *comp, TR::Node *n, intptr_t val)
   {
   TR::Node *node = comp->target().is64Bit()? TR::Node::lconst(n, val) : TR::Node::iconst(n, val);
   if (node->getOpCodeValue() == TR::lconst)
      node->setLongInt(val);
   return node;
   }

TR::TreeTop* TR::ArraycopyTransformation::specializeForLength(TR::TreeTop *tt, TR::Node *arraycopyNode, uintptr_t lengthInBytes,
      TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, TR::SymbolReference *lenRef, TR::SymbolReference *srcObjRef, TR::SymbolReference *dstObjRef)
   {
   TR::TreeTop* arraycopyOriginal = TR::TreeTop::create(comp());
   TR::TreeTop* arraycopySpecific = TR::TreeTop::create(comp());

   createArrayNode(tt, arraycopyOriginal, srcRef, dstRef, lenRef, srcObjRef, dstObjRef, true);
   arraycopyOriginal->getNode()->getFirstChild()->setRarePathForwardArrayCopy(true);

   TR::TreeTop *specializedTree = createArrayNode(tt, arraycopySpecific, srcRef, dstRef, addressSizedConst(comp(), arraycopyNode, lengthInBytes), srcObjRef, dstObjRef, true);
   if (trace())
      dumpOptDetails(comp(), "%s Specialized arraycopy is %s\n", OPT_DETAILS, getDebug()->getName(specializedTree->getNode()->getFirstChild()));

   TR::Node* lengthNode = TR::Node::createLoad(arraycopyNode, lenRef);

   TR::TreeTop *specificIfCheck = TR::TreeTop::create(comp(),
      TR::Node::createif(lengthNode->getType().isInt32()? TR::ificmpne : TR::iflcmpne,
         lengthNode, addressSizedConst(comp(), arraycopyNode, lengthInBytes)));

   TR::Block *followOnBlock = tt->getEnclosingBlock()->createConditionalBlocksBeforeTree(
      tt, specificIfCheck, arraycopyOriginal, arraycopySpecific, comp()->getFlowGraph());

   specificIfCheck->getNode()->setBranchDestination(arraycopyOriginal->getEnclosingBlock()->getEntry());

   // Scale the frequency of the specialized path
   TR::Block * specificPathBlock = arraycopySpecific->getEnclosingBlock();
   int32_t specializedBlockFrequency = TR::Block::getScaledSpecializedFrequency(specificPathBlock->getFrequency());
   if (specificPathBlock->getFrequency() <= MAX_COLD_BLOCK_COUNT)
      specializedBlockFrequency = specificPathBlock->getFrequency();
   else if (specializedBlockFrequency <= MAX_COLD_BLOCK_COUNT)
      specializedBlockFrequency = MAX_COLD_BLOCK_COUNT+1;
   arraycopyOriginal->getEnclosingBlock()->setFrequency(specializedBlockFrequency);
   arraycopyOriginal->getEnclosingBlock()->setIsCold(false);
   requestOpt(OMR::basicBlockOrdering); // Block frequency is reset, so run order blocks optimization to reorder this path appropriately.

   return specificIfCheck; // First inserted tree
   }

TR::ArraycopyTransformation::ArraycopyTransformation(TR::OptimizationManager *manager)
   : TR::Optimization(manager), _changedTrees(false)
   {}

int32_t TR::ArraycopyTransformation::perform()
   {
   // this is not an optional transformation - it must be run if arraycopy transformation
   // is done since the code generator can not handle the direct output from
   // ValuePropagation arraycopy creation.
   // it is intended for platforms that want to generate inline code for both forward
   // and backward arraycopy but don't want to do control flow in the platform code
   bool mustSpecializeForDirection = comp()->cg()->getSupportsPostProcessArrayCopy();

   TR::CFG *cfg = comp()->getFlowGraph();
   TR::TreeTop* lastTreeTop  = cfg->findLastTreeTop();
   TR::TreeTop* firstTreeTop = comp()->getMethodSymbol()->getFirstTreeTop();
   for (TR::TreeTop* tt = lastTreeTop; tt != firstTreeTop; tt = tt->getPrevTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::treetop || node->getOpCode().isResolveOrNullCheck())
         {
         TR::Node *arraycopyNode = node->getFirstChild();
         if (arraycopyNode->getOpCodeValue() == TR::arraycopy)
            {
            if (mustSpecializeForDirection)
               tt = createMultipleArrayNodes(tt, arraycopyNode);
            else if (!tt->getEnclosingBlock()->isCold())
               tt = tryToSpecializeForLength(tt, arraycopyNode);
            }
         }
      }

   if (!hasChangedTrees() || !performTransformation(comp(), "%s Arraycopy Transformation for primitive and reference arrays\n", OPT_DETAILS))
      return 0;

   return 1;
   }

const char *
TR::ArraycopyTransformation::optDetailString() const throw()
   {
   return "O^O ARRAY COPY TRANSFORMATION: ";
   }

void OMR::ValuePropagation::createNewBlockInfoForVersioning(TR::Block *block)
   {

   if (!block->isCatchBlock() && !_bndChecks->isEmpty() && !_bndChecks->isSingleton())
      {
      TR_LinkHead<ArrayLengthToVersion> arrayLengthsLocal;

      if (prepareForBlockVersion(&arrayLengthsLocal))
         {
         TR_LinkHead<ArrayLengthToVersion> *arrayLengths = new (trStackMemory())TR_LinkHead<ArrayLengthToVersion>();
         arrayLengths->setFirst(arrayLengthsLocal.getFirst());

         BlockVersionInfo *b = new (trStackMemory()) BlockVersionInfo;
         b->_block = block;
         b->_arrayLengths = arrayLengths;
         _blocksToBeVersioned->add(b);
         }
      }
   _bndChecks->deleteAll();
   _seenDefinedSymbolReferences->empty();
   _firstLoads->setFirst(NULL);
   }


void OMR::ValuePropagation::versionBlocks()
   {

   TR::CFG *_cfg = comp()->getFlowGraph();

   TR::TreeTop *endTree = comp()->getMethodSymbol()->getLastTreeTop();
   BlockVersionInfo *cur;

   for (cur = _blocksToBeVersioned->getFirst(); cur; cur = cur->getNext())
      {
      //there is at least one array to version
      TR::Block *block = cur->_block;
      bool blockIsInCFG = false;

      for (TR::CFGNode *node = _cfg->getFirstNode(); node; node = node->getNext())
         {
         if (node->getNumber() == block->getNumber())
            {
            blockIsInCFG = true;
            break;
            }
         }
      if (!blockIsInCFG)
         continue;


      //check if all bndchecks are in the block

      TR_ScratchList<TR::Node> temp(trMemory());
      for (ArrayLengthToVersion *arrayLength = cur->_arrayLengths->getFirst(); arrayLength; arrayLength = arrayLength->getNext())
         {
         for (ArrayIndexInfo *arrayIndex = arrayLength->_arrayIndicesInfo->getFirst(); arrayIndex; arrayIndex = arrayIndex->getNext())
            {
            ListIterator<TR::Node> iter(arrayIndex->_bndChecks);
            for (TR::Node *node = iter.getFirst(); node; node = iter.getNext())
               temp.add(node);
            }
         }

      for (TR::TreeTop *tt = block->getFirstRealTreeTop(); tt; tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();
         if (node->getOpCodeValue() == TR::BBStart &&
             !node->getBlock()->isExtensionOfPreviousBlock())
            break;
         if (node->getOpCodeValue() == TR::BNDCHK)
            {
            if (temp.find(node))
               temp.remove(node);
            }
         }

      if (!temp.isEmpty())
         continue;

      if (!performTransformation(comp(), "%s Versioning block_%d \n", OPT_DETAILS, block->getNumber()))
         continue;


      //  Construct the tests to replace boundsChecks
      //
      TR_ScratchList<TR::Node> comparisonNodes(trMemory());
      buildBoundCheckComparisonNodes(cur, &comparisonNodes);

      if (comparisonNodes.isEmpty())
         continue;


      _cfg->setStructure(NULL);
      invalidateUseDefInfo();
      invalidateValueNumberInfo();


      TR::Block *tempBlock = block;
      TR::TreeTop *lastTree, *firstTree;
      while(1)
         {
         lastTree = tempBlock->getExit();
         firstTree = lastTree->getNextTreeTop();
         if (!firstTree)
            break;
         TR::Node *node = firstTree->getNode();
         TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "assertion failure");
         if (!node->getBlock()->isExtensionOfPreviousBlock())
            break;
         tempBlock = node->getBlock();

         }
      TR::Block *lastBlock = tempBlock;


      //if (lastBlock == block)
      //   printf("versioning block_%d in %s \n",block->getNumber(),  comp()->signature());
      //else
      //   printf("versioning extended block_%d in %s \n",block->getNumber(),  comp()->signature());



      //clone block
      TR_BlockCloner cloner(_cfg, true, false);       //clone branches exactly
      TR::Block *clonedBlock = cloner.cloneBlocks(block, lastBlock);

      tempBlock = clonedBlock;
      while(1)
         {
         tempBlock->setIsCold();
         tempBlock->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
         TR::TreeTop * fromTT = tempBlock->getEntry();

         for (fromTT = fromTT->getNextTreeTop(); fromTT != tempBlock->getExit(); fromTT = fromTT->getNextTreeTop())
            {

            if (fromTT->getNode()->getOpCode().isBranch())
               fromTT->getNode()->setBranchDestination(cloner.getToBlock(fromTT->getNode()->getBranchDestination()->getNode()->getBlock())->getEntry());

            }

         if (!((tempBlock->getExit()->getNextTreeTop()) && (tempBlock->getExit()->getNextTreeTop()->getNode()->getBlock()->isExtensionOfPreviousBlock())))
            break;

         tempBlock = tempBlock->getExit()->getNextTreeTop()->getNode()->getBlock();
         }

      TR::Block *lastClonedBlock = cloner.getLastClonedBlock();

      //append to the end of method
      //
      TR::TreeTop *newEntryTree = clonedBlock->getEntry();
      TR::TreeTop *newExitTree = lastClonedBlock->getExit();
      endTree->join(newEntryTree);
      newExitTree->setNextTreeTop(NULL);
      endTree = newExitTree;

      //adjusting the CFG;
      //

      for (auto edge = lastBlock->getSuccessors().begin(); edge != lastBlock->getSuccessors().end(); ++edge)
         {
         TR::Block *succ = toBlock((*edge)->getTo());

         // branches explicitly to its successor, then we can simply
         // add an edge from the cloned block to the successor; else
         // if it was fall through then we have to add a new goto block
         // which would branch explicitly to the successor.
         //
         TR::Node *lastNode = lastClonedBlock->getLastRealTreeTop()->getNode();
         if (lastNode->getNumChildren() > 0)
            {
            if (lastNode->getFirstChild()->getOpCodeValue() == TR::athrow)
               lastNode = lastNode->getFirstChild();
            }

         if(lastNode->getOpCodeValue() == TR::treetop)
             lastNode = lastNode->getFirstChild();

         TR::ILOpCode &lastOpCode = lastNode->getOpCode();
         bool fallsThrough = false;
         if (!(lastOpCode.isBranch() || lastOpCode.isJumpWithMultipleTargets() || lastOpCode.isReturn() || (lastOpCode.getOpCodeValue() == TR::athrow)))
            fallsThrough = true;
         else if (lastOpCode.isBranch())
            {
            if (!((lastNode->getBranchDestination() == succ->getEntry()) ||
                  (succ->getEntry() && (lastNode->getBranchDestination() == cloner.getToBlock(succ->getEntry()->getNode()->getBlock())->getEntry()))))
               fallsThrough = true;
            }

         if (!fallsThrough)
            {
            if (succ->getEntry() && (!lastOpCode.isBranch() || (lastNode->getBranchDestination() != cloner.getToBlock(succ->getEntry()->getNode()->getBlock())->getEntry())))
               _cfg->addEdge(TR::CFGEdge::createEdge(lastClonedBlock,  succ, trMemory()));
            else
               _cfg->addEdge(TR::CFGEdge::createEdge(lastClonedBlock, cloner.getToBlock(succ), trMemory()));
            }
         else
            {
            TR::Block *newGotoBlock = TR::Block::createEmptyBlock(lastNode, comp(), succ->getFrequency(), succ);
            if (trace())
               traceMsg(comp(), "Creating new goto block : %d\n", newGotoBlock->getNumber());

            _cfg->addNode(newGotoBlock);
            TR::TreeTop *gotoBlockEntryTree = newGotoBlock->getEntry();
            TR::TreeTop *gotoBlockExitTree = newGotoBlock->getExit();
            TR::Node *gotoNode =  TR::Node::create(lastNode, TR::Goto);
            TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
            TR_ASSERT(succ->getEntry(), "Entry tree of succ is NULL\n");
            gotoNode->setBranchDestination(succ->getEntry());
            gotoBlockEntryTree->join(gotoTree);
            gotoTree->join(gotoBlockExitTree);
            TR::TreeTop *clonedExit = lastClonedBlock->getExit();
            clonedExit->join(gotoBlockEntryTree);
            gotoBlockExitTree->setNextTreeTop(NULL);
            endTree = gotoBlockExitTree;

            _cfg->addEdge(TR::CFGEdge::createEdge(lastClonedBlock,  newGotoBlock, trMemory()));
            _cfg->addEdge(TR::CFGEdge::createEdge(newGotoBlock,  succ, trMemory()));

            }
         }

      // Adjust exception edges for the new cloned blocks
      //
      for (auto edge = lastBlock->getExceptionSuccessors().begin(); edge != lastBlock->getExceptionSuccessors().end(); ++edge)
         {
         TR::Block *succ = toBlock((*edge)->getTo());
         _cfg->addEdge(TR::CFGEdge::createExceptionEdge(lastClonedBlock, succ,trMemory()));
         }




      // Add each test accumalated earlier into a block of its own
      //
      TR::Block *chooserBlock = NULL;
      ListElement<TR::Node> *comparisonNode = comparisonNodes.getListHead();
      TR_ScratchList<TR::Block> comparisonBlocks(trMemory());
      TR::TreeTop *insertionPoint = block->getEntry();
      TR::TreeTop *treeBeforeInsertionPoint = insertionPoint->getPrevTreeTop();

      while (comparisonNode)
         {
         TR::Node *actualComparisonNode = comparisonNode->getData();
         TR::TreeTop *comparisonTree = TR::TreeTop::create(comp(), comparisonNode->getData(), NULL, NULL);
         TR::Block *comparisonBlock = TR::Block::createEmptyBlock(insertionPoint->getNode(), comp(), block->getFrequency(), block);
         if(optimizer()->getLastRun(OMR::basicBlockExtension))
            comparisonBlock->setIsExtensionOfPreviousBlock(true);
         actualComparisonNode->setBranchDestination(clonedBlock->getEntry());
         TR::TreeTop *comparisonEntryTree = comparisonBlock->getEntry();
         TR::TreeTop *comparisonExitTree = comparisonBlock->getExit();
         comparisonEntryTree->join(comparisonTree);
         comparisonTree->join(comparisonExitTree);
         comparisonExitTree->join(insertionPoint);

         if (comp()->useCompressedPointers())
            {
            for (int32_t i = 0; i < actualComparisonNode->getNumChildren(); i++)
               {
               TR::Node *objectRef = actualComparisonNode->getChild(i);
               bool shouldBeCompressed = false;
               if (objectRef->getOpCode().isLoadIndirect() &&
                      objectRef->getDataType() == TR::Address &&
                      TR::TransformUtil::fieldShouldBeCompressed(objectRef, comp()))
                  {
                  shouldBeCompressed = true;
                  }
               else if (objectRef->getOpCode().isArrayLength() &&
                        objectRef->getFirstChild()->getOpCode().isLoadIndirect() &&
                        TR::TransformUtil::fieldShouldBeCompressed(objectRef->getFirstChild(), comp()))
                  {
                  objectRef = objectRef->getFirstChild();
                  shouldBeCompressed = true;
                  }
               if (shouldBeCompressed)
                  {
                  TR::TreeTop *translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(objectRef), NULL, NULL);
                  comparisonBlock->prepend(translateTT);
                  }
               }
            }

         if (treeBeforeInsertionPoint)
            treeBeforeInsertionPoint->join(comparisonEntryTree);
         else
            comp()->setStartTree(comparisonEntryTree);

         chooserBlock = comparisonBlock;
         comparisonBlocks.add(comparisonBlock);
         insertionPoint = comparisonEntryTree;
         comparisonNode = comparisonNode->getNextElement();
         }

      if (chooserBlock && optimizer()->getLastRun(OMR::basicBlockExtension))
         {
         chooserBlock->setIsExtensionOfPreviousBlock(false);
         block->setIsExtensionOfPreviousBlock(true);
         requestOpt(OMR::localCSE , true, chooserBlock);
         }

      // Adjust the predecessors of the original block
      // so that they now branch to the first block containing the
      // (first) versioning test.
      //
      while (!block->getPredecessors().empty())
         {
         TR::CFGEdge * const nextPred = block->getPredecessors().front();
         block->getPredecessors().pop_front();
         nextPred->setTo(chooserBlock);
         TR::Block * const nextPredBlock = toBlock(nextPred->getFrom());

         if (nextPredBlock != _cfg->getStart())
            {
            TR::TreeTop *lastTreeInPred = nextPredBlock->getLastRealTreeTop();

            if (lastTreeInPred)
               lastTreeInPred->adjustBranchOrSwitchTreeTop(comp(), block->getEntry(), chooserBlock->getEntry());
            }
         }

      //Update the cfgs with newly created comparison test blocks
      //
      ListElement<TR::Block> *currComparisonBlock = comparisonBlocks.getListHead();
      while (currComparisonBlock)
         {
         TR::Block *currentBlock = currComparisonBlock->getData();
         _cfg->addNode(currentBlock);
         ListElement<TR::Block> *nextComparisonBlock = currComparisonBlock->getNextElement();
         if (nextComparisonBlock)
            {
            _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  nextComparisonBlock->getData(), trMemory()));
            _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  clonedBlock, trMemory()));
            }
         else
            {
            _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  block, trMemory()));
            _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  clonedBlock, trMemory()));
            }

         currComparisonBlock = nextComparisonBlock;
         }

      // Done with trees and CFG changes

      /***** Arrange the fast version *****/
      //remove bndchecks from the fast version
      removeBndChecksFromFastVersion(cur);
      }
   }

//walk the bndchks collected for this block and add to appropriate bucket based on arrayLenght.
//returns True if to version block and false if not.
bool OMR::ValuePropagation::prepareForBlockVersion(TR_LinkHead<ArrayLengthToVersion> *arrayLengths)
   {
   bool isGlobal;
   TR_BitVector invariantVariables(comp()->getSymRefCount(), trMemory(), stackAlloc);
   invariantVariables.setAll(comp()->getSymRefCount());
   invariantVariables -= *_seenDefinedSymbolReferences;

   ListIterator<TR::Node> iter(_bndChecks);
   for (TR::Node *bndchkNode = iter.getFirst(); bndchkNode; bndchkNode = iter.getNext())
      {
      TR::Node *arrayLen = bndchkNode->getFirstChild();
      ArrayLengthToVersion *array = NULL;
      if (!(arrayLen->getOpCode().isLoadConst() ||
           (arrayLen->getOpCode().isArrayLength() &&
           (((arrayLen->getFirstChild()->getOpCode().getOpCodeValue() == TR::aloadi) &&
             ((arrayLen->getFirstChild()->getFirstChild()->getOpCode().getOpCodeValue() == TR::aload) &&
              (arrayLen->getFirstChild()->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm() &&
               !arrayLen->getFirstChild()->getFirstChild()->getSymbol()->isInternalPointerAuto()))) ||
            ((arrayLen->getFirstChild()->getOpCode().getOpCodeValue() == TR::aload) &&
             arrayLen->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm())))))
         {
         continue;
         }

    TR_OpaqueClassBlock *instanceOfClass = NULL;
    TR_OpaqueClassBlock *indexSourceObjectClass = NULL;

      //check if aloads are invariants
    if (!arrayLen->getOpCode().isLoadDirect())
       {
       TR::SymbolReference * symbolRef;
       if (arrayLen->getFirstChild()->getOpCode().getOpCodeValue() == TR::aload)
            symbolRef = arrayLen->getFirstChild()->getOpCode().hasSymbolReference() ? arrayLen->getFirstChild()->getSymbolReference() : 0;
       else
              symbolRef = arrayLen->getFirstChild()->getFirstChild()->getOpCode().hasSymbolReference() ? arrayLen->getFirstChild()->getFirstChild()->getSymbolReference() : 0;

       if (!symbolRef || !invariantVariables.isSet(symbolRef->getReferenceNumber()))
              continue;

       if (arrayLen->getFirstChild()->hasUnresolvedSymbolReference())
          continue;
       if ((arrayLen->getFirstChild()->getOpCode().getOpCodeValue() == TR::aloadi) && (arrayLen->getFirstChild()->getFirstChild()->hasUnresolvedSymbolReference()))
          continue;

       if (arrayLen->getFirstChild()->getOpCode().getOpCodeValue() == TR::aloadi)
          {
          TR::Node *nodeField= arrayLen->getFirstChild();
          TR::Node *nodeBase = nodeField->getFirstChild();
          TR::SymbolReference *symRefField = nodeField->getSymbolReference();
          int32_t len;
          char *sig = symRefField->getOwningMethod(comp())->classNameOfFieldOrStatic(symRefField->getCPIndex(), len);
          TR_OpaqueClassBlock *classOfField = NULL;
          TR_OpaqueClassBlock *classOfBase = NULL;
          TR::VPConstraint *vpcBase = getConstraint(nodeBase, isGlobal);
          TR::VPResolvedClass *rcBase = NULL;

          if (sig)
             classOfField = fe()->getClassFromSignature(sig, len,symRefField->getOwningMethod(comp()));

          if (!classOfField)
             continue;

          instanceOfClass = classOfField;
          }
      }

      for (ArrayLengthToVersion *arrayFound = arrayLengths->getFirst(); arrayFound; arrayFound = arrayFound->getNext())
         {
         if (arrayFound->_arrayLen == arrayLen)
            {
            array = arrayFound;
            break;
            }
         }

      TR::Node *baseNode = NULL;
      int32_t c;
      bool shouldAnalyze = false;

      //check if bndchk is relevant
      if (bndchkNode->getSecondChild()->getOpCode().isLoadConst())
         {
         c = bndchkNode->getSecondChild()->getInt();
         shouldAnalyze = true;
         }
      else
         {
         //not a const
         static const char * disableExtendedBoundCheckRemoval = feGetEnv("TR_DisableExtendedBoundCheckRemoval");

         TR::Node *index;
         if (!disableExtendedBoundCheckRemoval)
            {
            index = findVarOfSimpleForm(bndchkNode->getSecondChild());
            }
         else
            index = findVarOfSimpleFormOld(bndchkNode->getSecondChild());
         if (index)
            {
            //check if invariant
            TR::SymbolReference * symRef = index->getOpCode().hasSymbolReference() ? index->getSymbolReference() : 0;
            if (symRef)
               {
               if (invariantVariables.isSet(symRef->getReferenceNumber()))
                  baseNode = index;
               else
                  {
                  int32_t indexSymRefNum = symRef->getReferenceNumber();
                  //check if trackableNonInvariant
                  for (FirstLoadOfNonInvariant *firstLoad =_firstLoads->getFirst() ;firstLoad && !baseNode ; firstLoad = firstLoad->getNext())
                     {
                     if (firstLoad->_node!= NULL && (firstLoad->_symRefNum == indexSymRefNum))
                        {
                        baseNode = firstLoad->_node;
                        break;
                        }
                     }
                  }
               if (baseNode) //invarinat or trackableNonInvariant
                  {
                  shouldAnalyze = true;
                  if (bndchkNode->getSecondChild() == baseNode)
                     c = 0;
                  else
                     {
                     //find relation: we should have a constraint
                     if (getConstraint(bndchkNode->getSecondChild(),isGlobal,baseNode))
                        c = getConstraint(bndchkNode->getSecondChild(),isGlobal,baseNode)->asEqual()->increment();
                     else
                        shouldAnalyze = false;
                     }
                  }

               }
            else
               {
               if (!disableExtendedBoundCheckRemoval)
                  {
                  baseNode = index;
                  shouldAnalyze = true;
                  if (bndchkNode->getSecondChild() == baseNode)
                        c = 0;
                  else
                     {
                     if (getConstraint(bndchkNode->getSecondChild(),isGlobal,baseNode))
                           c = getConstraint(bndchkNode->getSecondChild(),isGlobal,baseNode)->asEqual()->increment();
                        else
                           {
                           shouldAnalyze = false;
                           }
                     }
                  }
               }
            }//if index
         }
      if (!shouldAnalyze)
         continue;

      if(baseNode && baseNode->getOpCode().getOpCodeValue() == TR::iloadi)
         {
         // Get the class of the field
         int32_t len;
         TR::SymbolReference *symRefField = baseNode->getSymbolReference();
         char *sig = symRefField->getOwningMethod(comp())->classNameOfFieldOrStatic(symRefField->getCPIndex(), len);
         TR_OpaqueClassBlock *classOfField = (sig)?fe()->getClassFromSignature(sig, len,symRefField->getOwningMethod(comp())):NULL;
         if (!classOfField)
            continue;
         indexSourceObjectClass = classOfField;
         }

      if (!array )
         {
         createNewBucketForArrayIndex(array, arrayLengths, c, baseNode, bndchkNode, instanceOfClass, indexSourceObjectClass);
         continue;
         }

      //find bucket if exists
      TR_ASSERT(array,"we should have an array in hand at this point\n");

      bool removeBndchkFromFastBlock = false;
      bool updateMax = false;
      bool updateMin = false;
      ArrayIndexInfo *arrayIndexInfoFound = NULL;

      for (ArrayIndexInfo *arrayIndexInfo = array->_arrayIndicesInfo->getFirst(); arrayIndexInfo; arrayIndexInfo = arrayIndexInfo->getNext())
         {
         if (arrayIndexInfo->_notToVersionBucket)
            continue;

         //cannot be the bucket if different types
         if (((arrayIndexInfo->_baseNode == NULL) && (baseNode !=NULL)) ||
             ((arrayIndexInfo->_baseNode != NULL) && (baseNode == NULL)))
            continue; //the bucket search

         //found same type of buckets: check if this is the one
         //For consts: nothing else to check
         //For variable: check if related.
         if (baseNode)
            {

            TR::VPConstraint *rel = getConstraint(bndchkNode->getSecondChild(),isGlobal,arrayIndexInfo->_baseNode);
            if (!(rel && rel->asEqual()) && (bndchkNode->getSecondChild() != arrayIndexInfo->_baseNode ))
               continue;
            }


         //this is the bucket
         arrayIndexInfoFound = arrayIndexInfo;
         break; //buckets
         } //for buckets

      if (arrayIndexInfoFound)
         {

         int deltaMax = c-arrayIndexInfoFound->_max;
         int deltaMin = c-arrayIndexInfoFound->_min;
         if (baseNode)
            {
            if (arrayIndexInfoFound->_max == arrayIndexInfoFound->_min)
               {
               //Adding second point
               //In this case deltaMax == deltaMin,
               //because max==min (only one access so far)
               //we compare distance with baseNode=0 as reference.
               //The following tests, using c and max (distance from 0) guarantees delta positive.
               if (deltaMax>0 && c>arrayIndexInfoFound->_max)
                  {
                  updateMax = true;
                  arrayIndexInfoFound->_delta = c-arrayIndexInfoFound->_max;
                  }
               else if (deltaMax<0 && c<arrayIndexInfoFound->_max)
                  {
                  updateMin = true;
                  arrayIndexInfoFound->_delta = arrayIndexInfoFound->_max-c;
                  }
               else if (arrayIndexInfoFound->_max == c)
                  removeBndchkFromFastBlock = true;
               }
            else
               {
               //adding third+ point
               if (deltaMax>0 && deltaMin>0)
                  {
                  updateMax = true;
                  arrayIndexInfoFound->_delta += deltaMax;
                  }
               else if (deltaMax<0 && deltaMin<0)
                  {
                  updateMin = true;
                  arrayIndexInfoFound->_delta -= deltaMin;
                  }
               else if ((deltaMax<0 && deltaMin>0) ||
                        (deltaMax == 0 || deltaMin == 0))
                  removeBndchkFromFastBlock = true;

               }
            }
         else
            {

            //if delta is larger than maxInt, one of the accesses will throw an exception.
            //So just don't version this bucket.
            //we need to check that max-c>0 to make sure that delta is positive (less than maxInt)
            if ((c<arrayIndexInfoFound->_min) && (arrayIndexInfoFound->_max-c>0))
               {
               if (c >= 0)
                  {
                  updateMin = true;
                  arrayIndexInfoFound->_delta += (arrayIndexInfoFound->_min-c);
                  }
               }
            else if ((c>arrayIndexInfoFound->_max) && (c-arrayIndexInfoFound->_min>0))
               {
               updateMax = true;
               arrayIndexInfoFound->_delta +=(c-arrayIndexInfoFound->_max);
               }
            else if ((arrayIndexInfoFound->_min<c && c>arrayIndexInfoFound->_max) ||
                     (c==arrayIndexInfoFound->_min ||c==arrayIndexInfoFound->_max))
               removeBndchkFromFastBlock = true;
            }

         }//if arrayIndexInfoFound
      else
         {
         createNewBucketForArrayIndex(array, arrayLengths, c, baseNode, bndchkNode, instanceOfClass, indexSourceObjectClass);
         continue; //next bndchk
         }

      //Found bucket
      if (updateMax)
         {
         arrayIndexInfoFound->_max = c;
         removeBndchkFromFastBlock = true;
         }
      else if (updateMin)
         {
         arrayIndexInfoFound->_min = c;
         removeBndchkFromFastBlock = true;
         }

      if (removeBndchkFromFastBlock)
         {
         arrayIndexInfoFound->_bndChecks->add(bndchkNode);
         arrayIndexInfoFound->_versionBucket = true;
         }
      else
         {
         //delta was neg - don't version this bucket
         arrayIndexInfoFound->_notToVersionBucket=true;
         arrayIndexInfoFound->_versionBucket=false;
         }

      }

   for (ArrayLengthToVersion *array = arrayLengths->getFirst(); array; array = array->getNext())
      {
      for (ArrayIndexInfo *arrayIndex = array->_arrayIndicesInfo->getFirst(); arrayIndex; arrayIndex = arrayIndex->getNext())
         {
         if (arrayIndex->_versionBucket)
            return true;
         }
      }

   return false;

   }


void OMR::ValuePropagation::buildBoundCheckComparisonNodes(BlockVersionInfo *blockInfo, List<TR::Node> *comparisonNodes)
   {
   // Walk arrayLength and for each one walk all buckets - create tests for min, max, null-tests and instanceof.
   // Also performs some conservative calculations to decide if the required tests might be to costly.
   int numBoundChecksRemoved=0, numInstanceOfChecksAdded=0, numOfNullTestsAdded=0;
   int32_t len;
   const char *sig;
   TR_OpaqueClassBlock *clazz;
   TR::Node *nextComparisonNode;

   for (ArrayLengthToVersion *arrayLength = blockInfo->_arrayLengths->getFirst(); arrayLength; arrayLength = arrayLength->getNext())
      {
      bool arrayLengthVersioned=false;
      TR_ScratchList<TR::Node> temp(trMemory());
      for (ArrayIndexInfo *arrayIndex = arrayLength->_arrayIndicesInfo->getFirst(); arrayIndex; arrayIndex = arrayIndex->getNext())
         {
         if (!arrayIndex->_versionBucket)
            continue;

         numBoundChecksRemoved += arrayIndex->_bndChecks->getSize();

         if (performTransformation(comp(), "%s Creating tests outside block_%d for versioning arraylenth %p \n", OPT_DETAILS, blockInfo->_block->getNumber(), arrayLength->_arrayLen))
            {
            arrayLengthVersioned=true;

            TR::Node *maxIndex;
            TR::Node *minIndex;


            if (arrayIndex->_baseNode)
               {
               maxIndex = TR::Node::create(TR::iadd,2, arrayIndex->_baseNode->duplicateTree(), TR::Node::create(arrayLength->_arrayLen,TR::iconst, 0, arrayIndex->_max));
               minIndex = TR::Node::create(TR::iadd,2, arrayIndex->_baseNode->duplicateTree(), TR::Node::create(arrayLength->_arrayLen,TR::iconst, 0, arrayIndex->_min));
               }
            else
               {
               maxIndex = TR::Node::create(arrayLength->_arrayLen,TR::iconst, 0, arrayIndex->_max);
               minIndex = TR::Node::create(arrayLength->_arrayLen,TR::iconst, 0, arrayIndex->_min);
               }

            if (arrayIndex->_baseNode)
               {
               nextComparisonNode = TR::Node::createif(TR::ificmplt, minIndex, TR::Node::create(arrayLength->_arrayLen , TR::iconst, 0, 0));

               if (trace())
                  traceMsg(comp(), "First Test - Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

               temp.add(nextComparisonNode);
               }

            nextComparisonNode = TR::Node::createif(TR::ifiucmpge, maxIndex, arrayLength->_arrayLen->duplicateTree());

            if (trace())
               traceMsg(comp(), "Second test - Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

            temp.add(nextComparisonNode);

            if (arrayIndex->_baseNode && arrayIndex->_instanceOfClass && 
                arrayIndex->_baseNode->getOpCode().getOpCodeValue() == TR::iloadi)
               {
               // InstanceOf check for the object we load the array index from
               TR::Node *indexObjectRefChild = arrayIndex->_baseNode->getFirstChild();
               if (indexObjectRefChild)
                  {
                  sig = indexObjectRefChild->getTypeSignature(len);
                  clazz = sig ? fe()->getClassFromSignature(sig,len,indexObjectRefChild->getSymbolReference()->getOwningMethod(comp())) : NULL;
                  TR_YesNoMaybe result = clazz ? comp()->fe()->isInstanceOf(arrayIndex->_instanceOfClass,clazz,false) : TR_no;
                  if (result != TR_yes)
                     {
                     if (comp()->compileRelocatableCode())
                        {
                        dumpOptDetails(comp(), "%s Abandoned versioning of block_%d because an instanceOf check is needed and this is a relocatable compile.\n", OPT_DETAILS, blockInfo->_block->getNumber());
                        comparisonNodes->deleteAll();
                        return;
                        }
                     dumpOptDetails(comp(), "%s Creating test for instanceof of %p outside block_%d for versioning array index %p \n", OPT_DETAILS, indexObjectRefChild, blockInfo->_block->getNumber(), arrayIndex->_baseNode);
                     TR::Node *duplicateClassPtr = TR::Node::createWithSymRef(indexObjectRefChild, TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateClassSymbol(arrayIndex->_baseNode->getSymbolReference()->getOwningMethodSymbol(comp()), -1, arrayIndex->_instanceOfClass, false));
                     TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, indexObjectRefChild->duplicateTree(),  duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
                     TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(arrayIndex->_baseNode, TR::iconst, 0, 0));
                     temp.add(ificmpeqNode);
                     numInstanceOfChecksAdded++;
                     }
                  else
                     {
                     dumpOptDetails(comp(), "%s ctInstanceOf test passed, skipping runtime instanceof of %p outside block_%d for versioning array index %p\n", OPT_DETAILS, indexObjectRefChild, blockInfo->_block->getNumber(), arrayIndex->_baseNode);
                     }
                  }
               }
            }
         }
      if (arrayLengthVersioned)
         {
         if (arrayLength->_arrayLen->getOpCode().isArrayLength())
            {
            TR::Node *objectRef = arrayLength->_arrayLen->getFirstChild();
            TR::Node *objectRefChild = NULL;

            if (objectRef->getOpCode().getOpCodeValue() == TR::aloadi)
               objectRefChild = objectRef->getFirstChild();

            if (objectRefChild && !objectRefChild->isNonNull())
               {
               dumpOptDetails(comp(), "%s Creating test for nullCheck of %p outside block_%d for versioning arraylenth %p \n", OPT_DETAILS, objectRefChild, blockInfo->_block->getNumber(), arrayLength->_arrayLen);
               TR::Node *nullTestNode = TR::Node::createif(TR::ifacmpeq, objectRefChild->duplicateTree(), TR::Node::aconst(objectRefChild, 0));
               comparisonNodes->add(nullTestNode);
               numOfNullTestsAdded++;
               }

            if (arrayLength->_instanceOfClass && objectRefChild)
               {
               sig = objectRefChild->getTypeSignature(len);
               clazz = sig ? fe()->getClassFromSignature(sig,len,objectRefChild->getSymbolReference()->getOwningMethod(comp())) : NULL;
               TR_YesNoMaybe result = clazz ? comp()->fe()->isInstanceOf(arrayLength->_instanceOfClass,clazz,false) : TR_no;
               ArrayIndexInfo *arrayIndex = NULL;

               if (result != TR_yes)
                  {
                  // Have we already generated an instanceOf for this object/class when handling array indexes?
                  for (arrayIndex = arrayLength->_arrayIndicesInfo->getFirst(); result != TR_yes && arrayIndex; arrayIndex = arrayIndex->getNext())
                     {
                     if (arrayIndex->_versionBucket && arrayIndex->_baseNode && arrayIndex->_baseNode->getOpCode().getOpCodeValue() == TR::iloadi &&
                         objectRefChild == arrayIndex->_baseNode->getFirstChild() && arrayLength->_instanceOfClass == arrayIndex->_instanceOfClass)
                        {
                        result = TR_yes; // We already generated an instanceOf check for this object&class combination
                        dumpOptDetails(comp(), "%s Skipping redundant runtime instanceof of %p outside block_%d for versioning arraylenth %p\n", OPT_DETAILS, objectRefChild, blockInfo->_block->getNumber(), arrayLength->_arrayLen);
                        }
                     }
                  }

               if (result != TR_yes)
                  {
                  if (comp()->compileRelocatableCode())
                     {
                     dumpOptDetails(comp(), "%s Abandoned versioning of block_%d because an instanceOf check is needed and this is a relocatable compile.\n", OPT_DETAILS, blockInfo->_block->getNumber());
                     comparisonNodes->deleteAll();
                     return;
                     }
                  dumpOptDetails(comp(), "%s Creating test for instanceof of %p outside block_%d for versioning arraylenth %p \n", OPT_DETAILS, objectRefChild, blockInfo->_block->getNumber(), arrayLength->_arrayLen);
                  TR::Node *duplicateClassPtr = TR::Node::createWithSymRef(objectRefChild, TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateClassSymbol(objectRef->getSymbolReference()->getOwningMethodSymbol(comp()), -1, arrayLength->_instanceOfClass, false));
                  TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, objectRefChild->duplicateTree(),  duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
                  TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(objectRef, TR::iconst, 0, 0));
                  comparisonNodes->add(ificmpeqNode);
                  numInstanceOfChecksAdded++;
                  }
               else if (arrayIndex)
                  {
                  dumpOptDetails(comp(), "%s ctInstanceOf test passed, skipping runtime instanceof of %p outside block_%d for versioning arraylenth %p\n", OPT_DETAILS, objectRefChild, blockInfo->_block->getNumber(), arrayLength->_arrayLen);
                  }
               }

            if (!objectRef->isNonNull())
               {
               dumpOptDetails(comp(), "%s Creating test for nullCheck of %p outside block_%d for versioning arraylenth %p \n", OPT_DETAILS, objectRef, blockInfo->_block->getNumber(), arrayLength->_arrayLen);
               TR::Node *nullTestNode = TR::Node::createif(TR::ifacmpeq, objectRef->duplicateTree(), TR::Node::aconst(objectRef, 0));
               comparisonNodes->add(nullTestNode);
               numOfNullTestsAdded++;
               }
            }
         comparisonNodes->add(temp);
         }
      }
   // Is this transformation worthwhile? Since we are adding a min & max test it's only a win
   // if we are removing 3 bound-checks. We estimate a instanceOf is as costly as 3 bound-checks
   // and null-tests is about as costly as one bound-check
   if (numBoundChecksRemoved <= 3+(numInstanceOfChecksAdded*3)+numOfNullTestsAdded)
      {
      dumpOptDetails(comp(), "%s Abandoned versioning of block_%d, it would require %d instanceOfChecks and %d null-tests while only saving %d boundChecks\n", OPT_DETAILS, blockInfo->_block->getNumber(), numInstanceOfChecksAdded, numOfNullTestsAdded, numBoundChecksRemoved);
      comparisonNodes->deleteAll();
      }
   else
      {
      dumpOptDetails(comp(), "%s Versioning of block_%d requires %d instanceOfChecks and %d null-tests while saving %d boundChecks\n", OPT_DETAILS, blockInfo->_block->getNumber(), numInstanceOfChecksAdded, numOfNullTestsAdded, numBoundChecksRemoved);
      if (numInstanceOfChecksAdded>0)
         {
         // We had added an instanceOf, so lets run opts to see if they can be removed
         requestOpt(OMR::localCSE,  true);
         requestOpt(OMR::localValuePropagation,  true);
         }
      }
   }



void OMR::ValuePropagation::removeBndChecksFromFastVersion(BlockVersionInfo *blockInfo)
   {

   for (ArrayLengthToVersion *array = blockInfo->_arrayLengths->getFirst(); array; array = array->getNext())
      {
      for (ArrayIndexInfo *index = array->_arrayIndicesInfo->getFirst(); index; index = index->getNext())
         {
         if (!index->_bndChecks->isEmpty() && !index->_bndChecks->isSingleton())
            {
            ListIterator<TR::Node> iter(index->_bndChecks);
            for (TR::Node *bndchk = iter.getFirst(); bndchk; bndchk = iter.getNext())
               {
               dumpOptDetails(comp(), "blockVersioner: removing bndchk %p\n", bndchk);
               TR::Node::recreate(bndchk, TR::treetop);
               removeNode(bndchk->getFirstChild(),false);
               bndchk->setChild(0, bndchk->getSecondChild());
               bndchk->setChild(1, NULL);
               bndchk->setNumChildren(1);
               if (trace())
                  traceMsg(comp(), "Block versioner: Remove bndchk %p \n", bndchk );


               setChecksRemoved();
               }
            }
         }
      }

   }

// deals only with constants. (i+5), (i+2-3)
TR::Node *OMR::ValuePropagation::findVarOfSimpleFormOld(TR::Node *node) //ArrayIndexNodeFromOffset(TR::Node *node, int32_t stride)
   {
   if (node->getOpCode().hasSymbolReference() &&
       !node->hasUnresolvedSymbolReference() &&
       ((node->getOpCode().getOpCodeValue() == TR::iload) ||
        ((node->getOpCode().getOpCodeValue() == TR::iloadi) &&
         ((node->getFirstChild()->getOpCode().getOpCodeValue() == TR::aload) &&
           !((node->getFirstChild())->hasUnresolvedSymbolReference()) &&
           !(_seenDefinedSymbolReferences->isSet(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))))))
      return node;
   else
      {
      while (node->getOpCode().isAdd() || node->getOpCode().isSub())
         {
         if (node->getSecondChild()->getOpCode().isLoadConst())
            node = node->getFirstChild();
         else
            break;
         }

      if (node &&
          node->getOpCode().hasSymbolReference() &&
           !node->hasUnresolvedSymbolReference() &&
          ((node->getOpCode().getOpCodeValue() == TR::iload) ||
           ((node->getOpCode().getOpCodeValue() == TR::iloadi) &&
            ((node->getFirstChild()->getOpCode().getOpCodeValue() == TR::aload) &&
              !((node->getFirstChild())->hasUnresolvedSymbolReference()) &&
              !(_seenDefinedSymbolReferences->isSet(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))))))

         return node;

      return NULL;
      }

   }

// deals with constants of the form (i+5), (i+2-3), but now i doesn't have to be an invariant;
// i has to be any subtree, but its children are walked through, and if they have a symbol reference
// they have to be either a param or an auto (we are trying to avoid object fields)
// and of course they have to be not defined before then; see design 1839
TR::Node *OMR::ValuePropagation::findVarOfSimpleForm(TR::Node *node) //ArrayIndexNodeFromOffset(TR::Node *node, int32_t stride)
   {
   if (node->getOpCode().hasSymbolReference() &&
       !node->hasUnresolvedSymbolReference() &&
       ((node->getOpCode().getOpCodeValue() == TR::iload) ||
        ((node->getOpCode().getOpCodeValue() == TR::iloadi) &&
         ((node->getFirstChild()->getOpCode().getOpCodeValue() == TR::aload) &&
           !((node->getFirstChild())->hasUnresolvedSymbolReference()) &&
           !(_seenDefinedSymbolReferences->isSet(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))))))
         return node;

   while (node->getOpCode().isAdd() || node->getOpCode().isSub())
      {
      if (node->getSecondChild()->getOpCode().isLoadConst())
         node = node->getFirstChild();
      else
         break;
      }

   TR::ILOpCode &opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      if (!symReference->getSymbol()->isAutoOrParm() ||
            symReference->isUnresolved())
         {
         if (node->getOpCode().hasSymbolReference() &&
               !node->hasUnresolvedSymbolReference() &&
               ((node->getOpCode().getOpCodeValue() == TR::iload) ||
               ((node->getOpCode().getOpCodeValue() == TR::iloadi) &&
               ((node->getFirstChild()->getOpCode().getOpCodeValue() == TR::aload) &&
               !((node->getFirstChild())->hasUnresolvedSymbolReference()) &&
               !(_seenDefinedSymbolReferences->isSet(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))))))
            return node;
         return NULL;
         }
      else
         {
         // check for non-invariants. ignore all tracked non-invariants
         //
         bool trackedInvariant = false;
         for (FirstLoadOfNonInvariant *firstLoad = _firstLoads->getFirst(); firstLoad; firstLoad = firstLoad->getNext())
            {
            if (firstLoad->_symRefNum == symReference->getReferenceNumber())
               {
               trackedInvariant = true;
               break;
               }
            }

         if (!trackedInvariant &&
               _seenDefinedSymbolReferences->get(symReference->getReferenceNumber()))
            return NULL;
         }
      }

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      if (!findVarOfSimpleForm(node->getChild(i)))
         return NULL;
      }

   return node;
   }


void OMR::ValuePropagation::createNewBucketForArrayIndex(ArrayLengthToVersion *array, TR_LinkHead<ArrayLengthToVersion> *arrayLengths, int32_t c, TR::Node *baseNode, TR::Node *bndchkNode, TR_OpaqueClassBlock *instanceOfClass, TR_OpaqueClassBlock *indexSourceObjectClass)
   {
   if (!array )
      {         //create a new arrayLengthToVersion
      array = new (trStackMemory()) ArrayLengthToVersion;
      array->_arrayLen = bndchkNode->getFirstChild();
      array->_arrayIndicesInfo = new (trStackMemory())TR_LinkHead<ArrayIndexInfo>();
      array->_instanceOfClass = instanceOfClass;
      addToSortedList(arrayLengths,array);
      }
   //create a new bucket
   ArrayIndexInfo *arrayIndex = new (trStackMemory()) ArrayIndexInfo;
   arrayIndex->_min = c;
   arrayIndex->_max = c;
   arrayIndex->_delta = 0;
   arrayIndex->_baseNode = baseNode;
   arrayIndex->_instanceOfClass = indexSourceObjectClass;
   arrayIndex->_bndChecks = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
   arrayIndex->_bndChecks->add(bndchkNode);
   arrayIndex->_versionBucket= false;
   if (!baseNode && c<0)
      arrayIndex->_notToVersionBucket=true;
   else
      arrayIndex->_notToVersionBucket=false;
   array->_arrayIndicesInfo->add(arrayIndex);
   }

void OMR::ValuePropagation::collectDefSymRefs(TR::Node *node, TR::Node *parent)
   {
   if (!node)
      return;
   TR::ILOpCode &opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      if (node->mightHaveVolatileSymbolReference())
         {
         if (symReference->sharesSymbol())
            symReference->getUseDefAliases().getAliasesAndUnionWith(*_seenDefinedSymbolReferences);
         else
            _seenDefinedSymbolReferences->set(symReference->getReferenceNumber());
         }

      if (opCode.isResolveCheck())
         {
         TR::SymbolReference *childSymRef = node->getFirstChild()->getSymbolReference();
         bool isCallDirect = false;
         if (node->getFirstChild()->getOpCode().isCallDirect())
            isCallDirect = true;

         _seenDefinedSymbolReferences->set(childSymRef->getReferenceNumber());
         childSymRef->getUseDefAliases(isCallDirect).getAliasesAndUnionWith(*_seenDefinedSymbolReferences);
         }

      if (!opCode.isLoadVar() && (opCode.getOpCodeValue() != TR::loadaddr))
         {
         bool isCallDirect = false;
         if (opCode.isCallDirect())
            isCallDirect = true;

         if (!opCode.isCheck() && !opCode.isStore() /* &&
             !node->isTheVirtualCallNodeForAGuardedInlinedCall() */)
            {
            symReference->getUseDefAliases(isCallDirect).getAliasesAndUnionWith(*_seenDefinedSymbolReferences);
            }

         if (opCode.isStore())
            {
            if (symReference->sharesSymbol())
               symReference->getUseDefAliases().getAliasesAndUnionWith(*_seenDefinedSymbolReferences);
            else
               _seenDefinedSymbolReferences->set(symReference->getReferenceNumber());


            //check if of the form i=i+1
            if (opCode.getOpCodeValue() == TR::istore)
               {
               TR::Node *load = findVarOfSimpleFormOld(node->getFirstChild());
               if (load && symReference->getReferenceNumber() == load->getSymbolReference()->getReferenceNumber())
                  {//check if this is the first time
                  bool firstLoadWasSeen = false;
                  for (FirstLoadOfNonInvariant *firstLoad = _firstLoads->getFirst() ;firstLoad  ; firstLoad = firstLoad->getNext())
                     {
                     if (firstLoad->_symRefNum == load->getSymbolReference()->getReferenceNumber())
                        {
                        firstLoadWasSeen = true;
                        break;
                        }

                     }
                  if (!firstLoadWasSeen)
                     {
                     FirstLoadOfNonInvariant *newFirstLoad = new (trStackMemory())FirstLoadOfNonInvariant;
                     newFirstLoad->_symRefNum = load->getSymbolReference()->getReferenceNumber();
                     newFirstLoad->_node = load;
                     _firstLoads->add(newFirstLoad);

                     }
                  }
               }
            }
         }
      }
#if 0
   if (!(parent && parent->getOpCode().isNullCheck()))
      {
      if ((opCode.isCall()) || (opCodeValue == TR::New) || (opCodeValue == TR::newarray) || (opCodeValue == TR::anewarray) || (opCodeValue == TR::multianewarray))
         _containsCalls = true;

      if (node->hasUnresolvedSymbolReference())
         _containsCalls = true;
      }
#endif
   }

//add in decresing order when constants
//non-constants arraylen will be at the head of list.
void  OMR::ValuePropagation::addToSortedList(TR_LinkHead<ArrayLengthToVersion> *arrayLengths, ArrayLengthToVersion *newArrayLen)
   {
   if (!newArrayLen->_arrayLen->getOpCode().isLoadConst())
      {
      arrayLengths->add(newArrayLen);
      return;
      }

   //is const
   ArrayLengthToVersion *prev = NULL;
   for (ArrayLengthToVersion *array = arrayLengths->getFirst(); array; array = array->getNext())
      {
      if (array->_arrayLen->getOpCode().isLoadConst() &&
          (newArrayLen->_arrayLen->getInt()>array->_arrayLen->getInt()))
         {
         arrayLengths->insertAfter(prev, newArrayLen);
         return;
         }

      prev = array;
      }

   arrayLengths->insertAfter(prev,newArrayLen);
   return;
   }


TR::TreeTop* OMR::ValuePropagation::createReferenceArrayNodeWithoutFlags(TR::TreeTop* tree, TR::TreeTop* newTree, TR::SymbolReference* srcObjectRef, TR::SymbolReference* dstObjectRef, TR::SymbolReference* lenRef, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, bool useFlagsOnOriginalArraycopy)
   {
   TR::Node* root = tree->getNode()->getFirstChild();
   TR::Node* len = TR::Node::createLoad(root, lenRef);

   TR::Node* src;
   if (srcRef)
      src = TR::Node::createLoad(root, srcRef);
   else
      {
      if (root->getNumChildren() == 3)
         src = root->getFirstChild()->duplicateTree();
      else
         src = root->getChild(2)->duplicateTree();
      }

   TR::Node* dst;
   if (dstRef)
      dst = TR::Node::createLoad(root, dstRef);
   else
      {
     if (root->getNumChildren() == 3)
         dst = root->getSecondChild()->duplicateTree();
      else
         dst = root->getChild(3)->duplicateTree();
      }

   TR::Node* srcObject = TR::Node::createLoad(root, srcObjectRef);
   TR::Node* dstObject = TR::Node::createLoad(root, dstObjectRef);
   TR::Node* node = TR::Node::createArraycopy(srcObject, dstObject, src, dst, len);
   node->setNumChildren(5);

   //node->setArrayCopyElementType(TR::Address);
   node->setSymbolReference(root->getSymbolReference());

   if (useFlagsOnOriginalArraycopy)
      {
      //node->setNonDegenerateArrayCopy(root->isNonDegenerateArrayCopy());
      node->setForwardArrayCopy(root->isForwardArrayCopy());
      node->setBackwardArrayCopy(root->isBackwardArrayCopy());

      if (root->isHalfWordElementArrayCopy())
         node->setHalfWordElementArrayCopy(true);
      else if (root->isWordElementArrayCopy())
         node->setWordElementArrayCopy(true);
      }
   ///node->setReferenceArrayCopy(true);

   // duplicate the tree just to copy either the ResolveCHK or the tree-top
   TR::Node* treeNode = tree->getNode()->duplicateTree();
   treeNode->setAndIncChild(0, node);

   newTree->setNode(treeNode);
   return newTree;
   }


void OMR::ValuePropagation::transformReferenceArrayCopyWithoutCreatingStoreTrees(TR_TreeTopWrtBarFlag *arrayTree, TR::SymbolReference *srcObjRef, TR::SymbolReference *dstObjRef, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, TR::SymbolReference *lenRef)
   {
   TR::Node *node = arrayTree->_treetop->getNode();
   if (node->getOpCodeValue() != TR::arraycopy)
      node = node->getFirstChild();

   TR::CFG *cfg = comp()->getFlowGraph();
   TR::Block *arraycopyBlock = arrayTree->_treetop->getEnclosingBlock();

   TR::TreeTop* arraycopyArrayStoreCheck  = TR::TreeTop::create(comp());
   TR::TreeTop* arraycopyNoArrayStoreCheck = TR::TreeTop::create(comp());

   //TR_ASSERT(srcRef, "No source address ref\n");
   //TR_ASSERT(dstRef, "No destination address ref\n");
   createReferenceArrayNodeWithoutFlags(arrayTree->_treetop, arraycopyArrayStoreCheck, srcObjRef, dstObjRef, lenRef, srcRef, dstRef, true);

   TR_ASSERT(srcObjRef, "No source object ref\n");
   TR_ASSERT(dstObjRef, "No destination object ref\n");
   if ((arrayTree->_flag & NEED_WRITE_BARRIER) == 0)
      createPrimitiveArrayNodeWithoutFlags(arrayTree->_treetop, arraycopyNoArrayStoreCheck, srcRef, dstRef, lenRef, true, true);
   else
      {
      createReferenceArrayNodeWithoutFlags(arrayTree->_treetop, arraycopyNoArrayStoreCheck, srcObjRef, dstObjRef, lenRef, srcRef, dstRef, true);
      arraycopyNoArrayStoreCheck->getNode()->getFirstChild()->setNoArrayStoreCheckArrayCopy(true);
      }

   TR::Node* srcObject = node->getChild(0);
   TR::Node* dstObject = node->getChild(1);
   TR::TreeTop *ifTree = createArrayStoreCompareNode(srcObject, dstObject);

   arraycopyBlock->createConditionalBlocksBeforeTree(arrayTree->_treetop, ifTree, arraycopyArrayStoreCheck, arraycopyNoArrayStoreCheck, cfg, false);
   ifTree->getNode()->setBranchDestination(arraycopyArrayStoreCheck->getEnclosingBlock()->getEntry());
   if (!arraycopyBlock->isCold())
      {
      TR::Block *arraycopyArrayStoreBlock = arraycopyArrayStoreCheck->getEnclosingBlock();
      arraycopyArrayStoreBlock->setIsCold(false);
      arraycopyArrayStoreBlock->setFrequency(arraycopyBlock->getFrequency()/3);

      TR::Block *arraycopyNoArrayStoreBlock = arraycopyNoArrayStoreCheck->getEnclosingBlock();
      arraycopyNoArrayStoreBlock->setIsCold(false);
      arraycopyNoArrayStoreBlock->setFrequency((2*arraycopyBlock->getFrequency())/3);

      arraycopyArrayStoreBlock->getSuccessors().front()->setFrequency(arraycopyBlock->getFrequency()/3);
      arraycopyArrayStoreBlock->getPredecessors().front()->setFrequency(arraycopyBlock->getFrequency()/3);

      arraycopyNoArrayStoreBlock->getSuccessors().front()->setFrequency((2*arraycopyBlock->getFrequency())/3);
      arraycopyNoArrayStoreBlock->getPredecessors().front()->setFrequency((2*arraycopyBlock->getFrequency())/3);
      }

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after arraycopy array store check specialization");
      cfg->comp()->getDebug()->print(cfg->comp()->getOutFile(), cfg);
      }
   }


void OMR::ValuePropagation::transformReferenceArrayCopy(TR_TreeTopWrtBarFlag *arrayTree)
   {
   TR::Node *node = arrayTree->_treetop->getNode();
   if (node->getOpCodeValue() != TR::arraycopy)
      node = node->getFirstChild();

   TR::SymbolReference *srcObjRef = NULL;
   TR::SymbolReference *dstObjRef = NULL;
   TR::SymbolReference *srcRef = NULL;
   TR::SymbolReference *dstRef = NULL;
   TR::SymbolReference *lenRef = NULL;

   createStoresForArraycopyChildren(comp(), arrayTree->_treetop, srcObjRef, dstObjRef, srcRef, dstRef, lenRef);

   transformReferenceArrayCopyWithoutCreatingStoreTrees(arrayTree, srcObjRef, dstObjRef, srcRef, dstRef, lenRef);
   }

void OMR::ValuePropagation::transformUnknownTypeArrayCopy(TR_TreeTopWrtBarFlag *arrayTree)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::Node *node = arrayTree->_treetop->getNode();
   if (node->getOpCodeValue() != TR::arraycopy)
      node = node->getFirstChild();

   TR::CFG *cfg = comp()->getFlowGraph();
   TR::SymbolReference *srcObjRef = NULL;
   TR::SymbolReference *dstObjRef = NULL;
   TR::SymbolReference *srcRef = NULL;
   TR::SymbolReference *dstRef = NULL;
   TR::SymbolReference *lenRef = NULL;
   createStoresForArraycopyChildren(comp(), arrayTree->_treetop, srcObjRef, dstObjRef, srcRef, dstRef, lenRef);

   TR::Block *arraycopyBlock = arrayTree->_treetop->getEnclosingBlock();

   TR::TreeTop* arraycopyPrimitive  = TR::TreeTop::create(comp());
   TR::TreeTop* arraycopyReference = TR::TreeTop::create(comp());

   //TR_ASSERT(srcRef, "No source address ref\n");
   //TR_ASSERT(dstRef, "No destination address ref\n");
   createPrimitiveArrayNodeWithoutFlags(arrayTree->_treetop, arraycopyPrimitive, srcRef, dstRef, lenRef, true, false);
   //arraycopyForward->getNode()->getFirstChild()->setRarePathForwardArrayCopy(true, comp());

   TR_ASSERT(srcObjRef, "No source object ref\n");
   TR_ASSERT(dstObjRef, "No destination object ref\n");
   createReferenceArrayNodeWithoutFlags(arrayTree->_treetop, arraycopyReference, srcObjRef, dstObjRef, lenRef, srcRef, dstRef, true);

   TR::Node* srcObject = node->getChild(0);
   TR::TreeTop *ifTree = createPrimitiveOrReferenceCompareNode(srcObject);

   arraycopyBlock->createConditionalBlocksBeforeTree(arrayTree->_treetop, ifTree, arraycopyReference, arraycopyPrimitive, cfg, false);
   ifTree->getNode()->setBranchDestination(arraycopyReference->getEnclosingBlock()->getEntry());
   if (!arraycopyBlock->isCold())
      {
      TR::Block *arraycopyReferenceBlock = arraycopyReference->getEnclosingBlock();
      arraycopyReferenceBlock->setIsCold(false);
      arraycopyReferenceBlock->setFrequency(arraycopyBlock->getFrequency()/3);

      TR::Block *arraycopyPrimitiveBlock = arraycopyPrimitive->getEnclosingBlock();
      arraycopyReferenceBlock->setIsCold(false);
      arraycopyPrimitiveBlock->setFrequency((2*arraycopyBlock->getFrequency())/3);

      arraycopyReferenceBlock->getSuccessors().front()->setFrequency(arraycopyBlock->getFrequency()/3);
      arraycopyReferenceBlock->getPredecessors().front()->setFrequency(arraycopyBlock->getFrequency()/3);

      arraycopyPrimitiveBlock->getSuccessors().front()->setFrequency((2*arraycopyBlock->getFrequency())/3);
      arraycopyPrimitiveBlock->getPredecessors().front()->setFrequency((2*arraycopyBlock->getFrequency())/3);
      }

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after arraycopy reference/primitive specialization");
#if DEBUG
      cfg->comp()->getDebug()->print(cfg->comp()->getOutFile(), cfg);
#endif
      }

   if ((arrayTree->_flag & NEED_ARRAYSTORE_CHECK) == 0)
      arraycopyReference->getNode()->getFirstChild()->setNoArrayStoreCheckArrayCopy(true);
   else
      transformReferenceArrayCopyWithoutCreatingStoreTrees(new (trStackMemory())TR_TreeTopWrtBarFlag(arraycopyReference, arrayTree->_flag), srcObjRef, dstObjRef, srcRef, dstRef, lenRef);
#endif
   }


static void changeBranchToGoto(OMR::ValuePropagation *vp, TR::Node *guardNode, TR::Block *guard)
   {
   // change the if to goto
   TR::Node::recreate(guardNode, TR::Goto);
   guardNode->getFirstChild()->recursivelyDecReferenceCount();
   guardNode->getSecondChild()->recursivelyDecReferenceCount();
   guardNode->setNumChildren(0);

   // remove the fallthrough edge
   TR::Block *fallThroughBlock = guard->getNextBlock();

   if (fallThroughBlock)
      {
      for (auto e = guard->getSuccessors().begin(); e != guard->getSuccessors().end(); ++e)
         {
         if ((*e)->getTo() == fallThroughBlock)
            {
            vp->comp()->getFlowGraph()->removeEdge(*e);
            break;
            }
         }
      }
   }


/** \brief
 *     Extension point for language specific optimizations on recognized methods
 *
 * \parm node
 *     The call node to be constrained
 */
void OMR::ValuePropagation::constrainRecognizedMethod(TR::Node *node)
   {
   }


/** \brief
 *     Extension point for language specific optimizations on direct load
 *
 * \parm node
 *     The direct load node to be constrained
 *
 * \return
 *     True if node is transformed, false otherwise
 */
bool OMR::ValuePropagation::transformDirectLoad(TR::Node *node)
   {
   return TR::TransformUtil::transformDirectLoad(comp(), node);
   }

bool OMR::ValuePropagation::checkAllUnsafeReferences(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return true;

   node->setVisitCount(visitCount);

   if (node->getOpCode().hasSymbolReference() &&
       node->getSymbol()->isUnsafeShadowSymbol())
      {
      if (_unsafeArrayAccessNodes->get(node->getGlobalIndex()))
         {
         comp()->getSymRefTab()->aliasBuilder.unsafeArrayElementSymRefs().set(node->getSymbolReference()->getReferenceNumber());
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Node is unsafe but not an array access %p \n", node);
         return false;
         }
      }

   int32_t childNum;
   for (childNum=0; childNum < node->getNumChildren(); childNum++)
      {
      if (!checkAllUnsafeReferences(node->getChild(childNum), visitCount))
         return false;
      }

   return true;
   }



void OMR::ValuePropagation::doDelayedTransformations()
   {
   ListIterator<TR_TreeTopNodePair> treesIt1(&_scalarizedArrayCopies);
   TR_TreeTopNodePair *scalarizedArrayCopy;
   for (scalarizedArrayCopy = treesIt1.getFirst();
        scalarizedArrayCopy; scalarizedArrayCopy = treesIt1.getNext())
      {
      bool didTransformArrayCopy = false;
      TR::TransformUtil::scalarizeArrayCopy(comp(), scalarizedArrayCopy->_node, scalarizedArrayCopy->_treetop, true, didTransformArrayCopy);

      if (didTransformArrayCopy)
         {
         removeArrayCopyNode(scalarizedArrayCopy->_treetop);
         }
      }
   _scalarizedArrayCopies.deleteAll();

   // NOTE: the array copy spine checks are processed before the reference, realtime, and
   //       unknown arraycopies because it refactors the CFG at a higher (outer) level
   //       than the others.  To simplify the CFG it should be run first.
   //
   if (comp()->requiresSpineChecks())
      {
      ListIterator<TR_ArrayCopySpineCheck> acscIt(&_arrayCopySpineCheck);
      TR_ArrayCopySpineCheck *arrayCopySpineCheck;
      for (arrayCopySpineCheck = acscIt.getFirst();
           arrayCopySpineCheck;
           arrayCopySpineCheck = acscIt.getNext())
         {
         transformArrayCopySpineCheck(arrayCopySpineCheck);
         }
      _arrayCopySpineCheck.deleteAll();
      }

#ifdef J9_PROJECT_SPECIFIC
   ListIterator<TR::TreeTop> convIt(&_converterCalls);
   TR::TreeTop *converterCallTree;
   for (converterCallTree = convIt.getFirst();
		   converterCallTree; converterCallTree = convIt.getNext())
      {

      transformConverterCall(converterCallTree);
      }
   _converterCalls.deleteAll();

   ListIterator<TR::TreeTop> objCloneIt(&_objectCloneCalls);
   ListIterator<ObjCloneInfo> objCloneTypeIt(&_objectCloneTypes);
      {
      TR::TreeTop *callTree = objCloneIt.getFirst();
      ObjCloneInfo *cloneInfo = objCloneTypeIt.getFirst();
      while (callTree && cloneInfo)
         {
         transformObjectCloneCall(callTree, cloneInfo);
         callTree = objCloneIt.getNext();
         cloneInfo = objCloneTypeIt.getNext();
         }
      }
   _objectCloneCalls.deleteAll();
   _objectCloneTypes.deleteAll();

   ListIterator<TR::TreeTop> arrayCloneIt(&_arrayCloneCalls);
   ListIterator<ArrayCloneInfo> arrayCloneTypeIt(&_arrayCloneTypes);
      {
      TR::TreeTop *callTree = arrayCloneIt.getFirst();
      ArrayCloneInfo *cloneInfo = arrayCloneTypeIt.getFirst();
      while (callTree && cloneInfo)
         {
         transformArrayCloneCall(callTree, cloneInfo);
         callTree = arrayCloneIt.getNext();
         cloneInfo = arrayCloneTypeIt.getNext();
         }
      }
   _arrayCloneCalls.deleteAll();
   _arrayCloneTypes.deleteAll();
#endif

   ListIterator<TR_TreeTopWrtBarFlag> treesIt(&_unknownTypeArrayCopyTrees);
   TR_TreeTopWrtBarFlag *unknownTypeArrayCopyTree;
   for (unknownTypeArrayCopyTree = treesIt.getFirst();
        unknownTypeArrayCopyTree; unknownTypeArrayCopyTree = treesIt.getNext())
      {
      transformUnknownTypeArrayCopy(unknownTypeArrayCopyTree);
      }
   _unknownTypeArrayCopyTrees.deleteAll();

   treesIt.set(&_referenceArrayCopyTrees);
   TR_TreeTopWrtBarFlag *referenceArrayCopyTree;
   for (referenceArrayCopyTree = treesIt.getFirst();
        referenceArrayCopyTree; referenceArrayCopyTree = treesIt.getNext())
      {
      transformReferenceArrayCopy(referenceArrayCopyTree);
      }
   _referenceArrayCopyTrees.deleteAll();

#ifdef J9_PROJECT_SPECIFIC
   if (comp()->generateArraylets())
      {
      ListIterator<TR_RealTimeArrayCopy> tt(&_needRunTimeCheckArrayCopy);
      TR_RealTimeArrayCopy *rtArrayCopyTree;
      for (rtArrayCopyTree = tt.getFirst(); rtArrayCopyTree; rtArrayCopyTree = tt.getNext())
         {
         transformRealTimeArrayCopy(rtArrayCopyTree);
         }
      _needRunTimeCheckArrayCopy.deleteAll();

      tt.set(&_needMultiLeafArrayCopy);
      for (rtArrayCopyTree = tt.getFirst(); rtArrayCopyTree; rtArrayCopyTree = tt.getNext())
         {
         transformRTMultiLeafArrayCopy(rtArrayCopyTree);
         }
      _needMultiLeafArrayCopy.deleteAll();
      }
#endif

   int32_t i;
   TR::CFG *cfg = comp()->getFlowGraph();
   TR::CFGNode *node = NULL;
   if (_blocksToBeRemoved)
      {
      for (i = _blocksToBeRemoved->size()-1; i >= 0; --i)
         {
         node = _blocksToBeRemoved->element(i);
         if (performTransformation(comp(), "%sRemoving unreachable block_%d at [%p]\n", OPT_DETAILS, node->getNumber(), node))
            {
            while (!node->getPredecessors().empty())
               cfg->removeEdge(node->getPredecessors().front());
            while (!node->getExceptionPredecessors().empty())
               cfg->removeEdge(node->getExceptionPredecessors().front());
            invalidateUseDefInfo();
            invalidateValueNumberInfo();
            }
         }
      }

   // If there were unreachable edges that still exist, remove them
   //
   TR::Region &stackRegion = comp()->trMemory()->currentStackRegion();
   TR::list<TR::Block*, TR::Region&> removedEdgeSources(stackRegion);

   if (_edgesToBeRemoved)
      {
      for (i = _edgesToBeRemoved->size()-1; i >= 0; --i)
         {
         TR::CFGEdge *edge = _edgesToBeRemoved->element(i);

         // NB: this following transformation is not conditional - it must be done otherwise the CFG could
         // be incorrect (for example, if a conditional branch was converted to a goto, you have to remove
         // the extra edge in the CFG or else madness will ensue.
         removedEdgeSources.push_back(toBlock(edge->getFrom()));
         if (std::find(edge->getTo()->getPredecessors().begin(), edge->getTo()->getPredecessors().end(), edge) != edge->getTo()->getPredecessors().end())
            {
            if (trace())
               traceMsg(comp(), "Removing unreachable edge from %d to %d\n", edge->getFrom()->getNumber(), edge->getTo()->getNumber());
            if (cfg->removeEdge(edge))
               {
               invalidateUseDefInfo();
               invalidateValueNumberInfo();
               }
            }

         // If the "from" block is still in the cfg but has no successors,
         // add an edge to the method's exit block.
         //
         node = edge->getFrom();
         if (node->getSuccessors().empty())
            {
           // if (!cfg->getRemovedNodes().find(node))
              if (!node->nodeIsRemoved())
               cfg->addEdge(TR::CFGEdge::createEdge(node, cfg->getEnd(), trMemory()));
            }
         }
      }

   TR_RegionStructure::extractUnconditionalExits(comp(), removedEdgeSources);

#ifdef J9_PROJECT_SPECIFIC
   if (!_multiLeafCallsToInline.isEmpty())
      {
      ListIterator<TR::TreeTop> iter(&_multiLeafCallsToInline);
      TR_InlineCall multiLeafInlineCall(optimizer(),this);
      TR::TreeTop *multiLeafCall;
      for (multiLeafCall = iter.getCurrent(); multiLeafCall != NULL; multiLeafCall = iter.getNext())
         {
         TR::Node *vcallNode = multiLeafCall->getNode()->getFirstChild();
         //maybe the call node was removed.
         if (vcallNode->getReferenceCount() < 1)
            continue;

         if (!multiLeafInlineCall.inlineCall(multiLeafCall))
            performTransformation(comp(),"%s WARNING: Inlining of %p failed\n", OPT_DETAILS,
                  multiLeafCall->getNode());
         }
      _multiLeafCallsToInline.deleteAll();
      }

   // process calls to unsafe methods for Hybrid.
   if (_unsafeCallsToInline.getFirst() != NULL)
      {
      TR_InlineCall unsafeInlineCall(optimizer(),this);
      for (CallInfo *uci = _unsafeCallsToInline.getFirst(); uci; uci = uci->getNext())
         {
         if(uci->_block->nodeIsRemoved())
            continue;

         if (!unsafeInlineCall.inlineCall(uci->_tt))
            {
            performTransformation(comp(),"%s WARNING: Inlining of %p failed\n", OPT_DETAILS,uci->_tt->getNode());
            }
         }

      _unsafeCallsToInline.setFirst(0);
      }
#endif

   // process calls that were devirtualized. See if they can be inlined or if
   // they need special JNI processing.
   //
   for (CallInfo *ci = _devirtualizedCalls.getFirst(); ci; ci = ci->getNext())
      {
      //if (comp()->getFlowGraph()->getRemovedNodes().find(ci->_block))
        if(ci->_block->nodeIsRemoved())
         continue;

      TR::Node *callNode;
      TR::Node *parent = ci->_tt->getNode();
      if (parent->getNumChildren() && (callNode = parent->getFirstChild())->getOpCode().isCall())
         {
         bool callUnreachable = true;
         bool callGuarded = callNode->isTheVirtualCallNodeForAGuardedInlinedCall();
         if (callGuarded)
            {
            if (callNode->getOpCode().isCallIndirect())
               {
               // TODO : see if we can be smarter about the type of the guard being used
               }
            else
               {
               bool guardForDifferentCall = false;
               TR::TreeTop *cursorTree = ci->_tt->getPrevTreeTop();
               TR::Node *cursorNode = cursorTree->getNode();
               while (cursorNode->getOpCodeValue() != TR::BBStart)
                  {
                  if (cursorNode->getNumChildren() > 0)
                     cursorNode = cursorNode->getFirstChild();

                  if (cursorNode->getOpCode().isCall() &&
                      cursorNode->isTheVirtualCallNodeForAGuardedInlinedCall())
                     {
                     guardForDifferentCall = true;
                     break;
                     }
                  cursorTree = cursorTree->getPrevTreeTop();
                  cursorNode = cursorTree->getNode();
                  }

               if (!guardForDifferentCall)
                  {
                  TR::Block * guard = ci->_block->findVirtualGuardBlock(cfg);

                  if (!guard)
                     {
                     TR::Block * foldedGuard = NULL;
                     if (ci->_block->getPredecessors().size() == 1)
                        foldedGuard = ci->_block->getPredecessors().front()->getFrom()->asBlock();

                     if (foldedGuard && (foldedGuard != cfg->getStart()) && (foldedGuard->getSuccessors().size() == 1) &&
                         (foldedGuard->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto) &&
                         (foldedGuard->getLastRealTreeTop()->getNode()->getBranchDestination() == ci->_block->getEntry()))
                        callUnreachable = false;
                     }

                  if (guard && (guard->getSuccessors().size() == 2) && guard->getExit())
                     {
                     TR::Node *guardNode = guard->getLastRealTreeTop()->getNode();
                     // check if the virtual guard has any inner
                     // assumptions; guard should not be removed in
                     // this case as inner assumptions could be violated
                     //
                     TR_VirtualGuard *virtualGuard;
                     bool canBeRemoved;
                     if (guardNode)
                        {
                        virtualGuard = comp()->findVirtualGuardInfo(guardNode);
                        canBeRemoved = virtualGuard->canBeRemoved() && callNode->getOpCode().isCallDirect(); //do not remove guards for indirect calls. If the ifacmpne handler
                        																			          //didn't do it, the chances are it is illegal to remove the guard
                        }
                     else
                        canBeRemoved = true;

                     if (guardNode &&
                         guardNode->isProfiledGuard())
                        {
                        TR::Node *classTypeNode = guardNode->getSecondChild();

                        int32_t guardType = -1; // unknown
                        if (guardNode->getOpCodeValue() == TR::ifacmpne)
                           {
                           if (trace())
                              traceMsg(comp(), "Got guard [%p] as ifacmpne\n", guardNode);
                           guardType = 1; // change if to goto
                           }
                        else if (guardNode->getOpCodeValue() == TR::ifacmpeq)
                           {
                           if (trace())
                              traceMsg(comp(), "Got guard [%p] as ifacmpeq\n", guardNode);
                           guardType = 0; // remove branch
                           }

                        TR_YesNoMaybe typeCompatibleStatus = TR_maybe; // unknown
                        if (classTypeNode->getOpCodeValue() == TR::aconst &&
                            classTypeNode->isClassPointerConstant())
                           {
                           TR_OpaqueClassBlock *typeClass = (TR_OpaqueClassBlock *)classTypeNode->getAddress();
                           TR_OpaqueClassBlock *receiverClass = callNode->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->containingClass();
                           TR_YesNoMaybe result = comp()->fe()->isInstanceOf(typeClass,receiverClass,true);
                           TR_YesNoMaybe isVPClassDerivedFromGuardClass = TR_no;
                           TR_YesNoMaybe isGuardClassDerivedFromVPClass = TR_yes;
                           if (ci->_thisType && (ci->_thisType != typeClass))
                              {
                              isVPClassDerivedFromGuardClass = comp()->fe()->isInstanceOf(ci->_thisType, typeClass, true);
                              if (isVPClassDerivedFromGuardClass == TR_no)
                                 isGuardClassDerivedFromVPClass = comp()->fe()->isInstanceOf(typeClass, ci->_thisType, true);
                              }

                           if ((result == TR_no) || (isVPClassDerivedFromGuardClass == TR_yes) || (isGuardClassDerivedFromVPClass == TR_no))
                              {
                              typeCompatibleStatus = TR_no; // types incompatible
                              }
                           else if ((result == TR_yes) &&
                                    (ci->_thisType == typeClass) &&
                                    canBeRemoved)
                              {
                              typeCompatibleStatus = TR_yes; // types compatible
                              }
                           else
                              callUnreachable = false;

                           if (trace())
                              traceMsg(comp(), "typeCompatibleStatus [%p] %d\n", guardNode, typeCompatibleStatus);
                           }
                        else
                           {
                           TR_ASSERT((classTypeNode->getOpCodeValue() == TR::aconst &&
                                   classTypeNode->isMethodPointerConstant()), "Devirtualization of guarded profiled calls only works currently for vft-guards or method compare guards");
                           void *profiledMethod = (void *)classTypeNode->getAddress();
                           void *devirtualizedMethod = callNode->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier();

                           if (profiledMethod != devirtualizedMethod)
                              typeCompatibleStatus = TR_no; // incompatible
                           else if (canBeRemoved)
                              typeCompatibleStatus = TR_yes; // compatible
                           else
                              callUnreachable = false;
                           }

                        // take appropriate action
                        //
                        if (typeCompatibleStatus == TR_no)
                           {
                           // ifacmpne
                           if ((guardType == 1) &&
                                 performTransformation(comp(), "%sChanging branch (guard) %p in block_%d to goto due to devirtualization\n", OPT_DETAILS, guardNode, guard->getNumber()))
                              {
                              // change the if to goto
                              changeBranchToGoto(this, guardNode, guard);
                              callUnreachable = false;
                              }
                           else if ((guardType == 0) &&
                                       performTransformation(comp(), "%sRemoving branch %p in block_%d due to devirtualization\n", OPT_DETAILS, guardNode, guard->getNumber()))
                              {
                              // remove branch
                              guard->removeBranch(comp());
                              callUnreachable = false;
                              }
                           }
                        else if (typeCompatibleStatus == TR_yes)
                           {
                           // ifacmpne
                           if ((guardType == 1) &&
                                 performTransformation(comp(), "%sRemoving branch %p in block_%d due to devirtualization\n", OPT_DETAILS, guardNode, guard->getNumber()))
                              {
                              // remove branch
                              guard->removeBranch(comp());
                              //callUnreachable true;
                              }
                           else if ((guardType == 0) &&
                                 performTransformation(comp(), "%sChanging branch (guard) %p in block_%d to goto due to devirtualization\n", OPT_DETAILS, guardNode, guard->getNumber()))
                              {
                              // change the if to goto
                              changeBranchToGoto(this, guardNode, guard);
                              //callUnreachable  true;
                              }
                           }
                        }
                     }
                  }
               }
            }

         //else
         //traceMsg(comp(), "Reached 2 for call %p\n", callNode);
         //traceMsg(comp(), "unreachable %d guarded %d\n", callUnreachable, callGuarded);
         if (!callGuarded || !callUnreachable)
            {
            TR::ResolvedMethodSymbol *methodSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
#ifdef J9_PROJECT_SPECIFIC
            if (methodSymbol->isJNI())
               callNode->processJNICall(ci->_tt, comp()->getMethodSymbol());
            else
#endif
               {
               if (comp()->getMethodHotness() <= warm && comp()->getOption(TR_DisableInliningDuringVPAtWarm))
                  {
                  if (trace()) traceMsg(comp(), "\tDo not inline call at [%p]\n", callNode);
                  }
               else
                  {
#ifdef J9_PROJECT_SPECIFIC
                  TR_InlineCall newInlineCall(optimizer(), this);

                  // restrict the amount of inlining in warm/cold bodies
                  int32_t initialMaxSize = 0;
                  if (comp()->getMethodHotness() <= warm)
                     {
                     initialMaxSize = comp()->getOptions()->getMaxSzForVPInliningWarm();
                     newInlineCall.setSizeThreshold(initialMaxSize);
                     }

                  if (!newInlineCall.inlineCall(ci->_tt, ci->_thisType, true, ci->_argInfo, initialMaxSize))
                     {
                     // If inlining failed, try to issue a direct call
                     if (!callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isInterpreted() ||
                         callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isJITInternalNative())
                        {
                        if (callNode->getOpCode().isCallIndirect())
                           {
                           //printf("XXX Added devirtualized call info in %s for %x\n", comp()->signature(), callNode);
                           comp()->findOrCreateDevirtualizedCall(callNode, ci->_thisType);
                           }
                        }
                     }
#endif
                  }
               }
            }
         }
      }
   _devirtualizedCalls.setFirst(0);


   for (VirtualGuardInfo *cvg = _convertedGuards.getFirst(); cvg; cvg = cvg->getNext())
      {
      if(cvg->_block->nodeIsRemoved())
         continue;

      //IL part
      TR::Node* oldNode = cvg->_currentTree->getNode();

      // !oldNode means that the branch was already removed
      if (!oldNode || !performTransformation(comp(), "%sReplacing the old guard %p with the shiny new overridden guard %p at treetop %p\n", oldNode, cvg->_newGuardNode, cvg->_currentTree))
         {
         continue;
         }

      oldNode->recursivelyDecReferenceCount();
      cvg->_currentTree->setNode(cvg->_newGuardNode);
      //Guards clean up
      comp()->removeVirtualGuard(comp()->findVirtualGuardInfo(oldNode));
      comp()->addVirtualGuard(cvg->_newVirtualGuard);
      //
      }
   _convertedGuards.setFirst(0);

#ifdef J9_PROJECT_SPECIFIC
   ListIterator<TR::Node> nodesIt(&_javaLangClassGetComponentTypeCalls);
   TR::Node *getComponentCallNode;
   for (getComponentCallNode = nodesIt.getFirst();
        getComponentCallNode; getComponentCallNode = nodesIt.getNext())
      {
      TR::Node::recreate(getComponentCallNode, TR::aloadi);
      getComponentCallNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateArrayComponentTypeSymbolRef());
      if (getComponentCallNode->getNumChildren() > 1)
         {
         TR_ASSERT((getComponentCallNode->getNumChildren() == 2), "Only expect two children for call\n");
         TR::Node *vftLoadChild = getComponentCallNode->getFirstChild();
         getComponentCallNode->setChild(0, getComponentCallNode->getSecondChild());
         vftLoadChild->recursivelyDecReferenceCount();
         getComponentCallNode->setNumChildren(1);
         }
      }
   _javaLangClassGetComponentTypeCalls.deleteAll();
#endif

   // Process throws that can be turned into gotos
   //
   ListIterator<TR_Pair<TR::Node, TR::Block> > throwsIt(&_predictedThrows);
   TR_Pair<TR::Node, TR::Block> *predictedThrow;
   for (predictedThrow = throwsIt.getFirst(); predictedThrow; predictedThrow = throwsIt.getNext())
      {
      TR::Node *predictedNode = predictedThrow->getKey();
      TR::Block *blockContainingThrow = predictedThrow->getValue();

      TR::TreeTop *treeTop;
      TR::Node    *node = findThrowInBlock(blockContainingThrow, treeTop);
      if (!node)
         continue;

      if (node != predictedNode)
         continue;

      // temporary set the numOfChildren to two to avoid assume in getSecondChild
      //
      node->setNumChildren(2);
      TR::Block *predictedCatchBlock = (TR::Block *)node->getSecondChild();
      node->setNumChildren(1);

      TR::TreeTop * firstTT = predictedCatchBlock->getFirstRealTreeTop();

      // Find the first real tree in catch block; ignoring the
      // profiling trees possibly inserted by catch block profiler
      //
      while (firstTT &&
             firstTT->getNode()->isProfilingCode())
         firstTT = firstTT->getNextRealTreeTop();

      if (predictedCatchBlock->specializedDesyncCatchBlock())
         dumpOptDetails(comp(), "%sChanging a throw [%p] to a goto for specializedDesyncCatchBlock\n", OPT_DETAILS, node);
      else if (!performTransformation(comp(), "%sChanging a throw [%p] to a goto\n", OPT_DETAILS, node))
         continue;

      optimizer()->setAliasSetsAreValid(false); // the catch auto must be aliased to the catch block
      TR::SymbolReference * excpSymRef = comp()->getSymRefTab()->findOrCreateExcpSymbolRef();
      if (treeTop->getNode()->getOpCodeValue() == TR::NULLCHK)
         {
         TR::Node::recreate(node, TR::PassThrough);
         treeTop = TR::TreeTop::create(comp(), treeTop, TR::Node::createWithSymRef(TR::astore, 1, 1, node->getFirstChild(), excpSymRef));
         }
      else
         {
         TR::Node::recreate(node, TR::astore);
         node->setSymbolReference(excpSymRef);
         }

      invalidateUseDefInfo();
      invalidateValueNumberInfo();

      if (debug("traceThrowToGoto"))
         printf("\nthrow converted to goto in %s ", comp()->signature());
      TR::Block * gotoDestination = predictedCatchBlock->split(firstTT, cfg);

      List<TR::SymbolReference> l1(trMemory()), l2(trMemory()), l3(trMemory());
      TR::ResolvedMethodSymbol * currentSymbol = comp()->getJittedMethodSymbol();
      TR_HandleInjectedBasicBlock hibb(comp(), NULL, currentSymbol, l1, l2, l3, 0);
      hibb.findAndReplaceReferences(predictedCatchBlock->getEntry(), gotoDestination, 0);
      ListIterator<TR::SymbolReference> newTemps(&l2);
      for (TR::SymbolReference * newTemp = newTemps.getFirst(); newTemp; newTemp = newTemps.getNext())
         currentSymbol->addAutomatic(newTemp->getSymbol()->castToAutoSymbol());

      ListElement<TR_Pair<TR::Node, TR::Block> > *e = throwsIt.getCurrentElement()->getNextElement();
      for (; e; e= e->getNextElement())
         {
         TR_Pair<TR::Node, TR::Block> *predictedThrow = e->getData();
         if (predictedThrow->getValue() == predictedCatchBlock)
            predictedThrow->setValue(gotoDestination);
         }

      TR::TreeTop::create(comp(), treeTop, TR::Node::create(node, TR::Goto, 0, gotoDestination->getEntry()));
      cfg->addEdge(blockContainingThrow, gotoDestination);
      cfg->removeEdge(blockContainingThrow, cfg->getEnd());

      if (predictedCatchBlock->specializedDesyncCatchBlock())
         cfg->removeEdge(blockContainingThrow, predictedCatchBlock);
      for (auto edge = predictedCatchBlock->getExceptionSuccessors().begin(); edge != predictedCatchBlock->getExceptionSuccessors().end();)
          cfg->removeEdge(*(edge++));
      }

   _predictedThrows.init();

#ifdef J9_PROJECT_SPECIFIC
   // Commit any preexistence based assumptions
   //
   ListIterator<TR_OpaqueClassBlock> cit(&_prexClasses);
   for (TR_OpaqueClassBlock *clazz = cit.getCurrent(); clazz; clazz = cit.getNext())
      {
      //printf("---secs--- class assumption in %s\n", comp()->signature());
      comp()->getCHTable()->recompileOnClassExtend(comp(), clazz);
      }

   _prexClasses.init();

   ListIterator<TR_ResolvedMethod> mit(&_prexMethods);
   for (TR_ResolvedMethod *method = mit.getCurrent(); method; method = mit.getNext())
      {
      //printf("---secs--- method assumption in %s\n", comp()->signature());
      comp()->getCHTable()->recompileOnMethodOverride(comp(), method);
      }

   _prexMethods.init();

   // Commit any preexistence based assumptions
   //
   ListIterator<TR_OpaqueClassBlock> prexCit(&_prexClassesThatShouldNotBeNewlyExtended);
   for (TR_OpaqueClassBlock *prexClazz = prexCit.getCurrent(); prexClazz; prexClazz = prexCit.getNext())
      {
      //printf("---secs--- class assumption in %s\n", comp()->signature());
      comp()->getCHTable()->recompileOnNewClassExtend(comp(), prexClazz);

      TR_PersistentClassInfo *classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(prexClazz, comp());
      if (classInfo)
         {
         TR_ScratchList<TR_PersistentClassInfo> subClasses(trMemory());
         TR_ClassQueries::collectAllSubClasses(classInfo, &subClasses, comp());
         ListIterator<TR_PersistentClassInfo> subClassesIt(&subClasses);
         for (TR_PersistentClassInfo *subClassInfo = subClassesIt.getFirst(); subClassInfo; subClassInfo = subClassesIt.getNext())
            {
            TR_OpaqueClassBlock *subClass = (TR_OpaqueClassBlock *) subClassInfo->getClassId();
            comp()->getCHTable()->recompileOnNewClassExtend(comp(), subClass);
            }
         }
      }

   if (0 && usePreexistence() && _prexClassesThatShouldNotBeNewlyExtended.isEmpty())
      {
      if (chTableWasValid())
         comp()->setFailCHTableCommit(false);
      }
   ListIterator<TR_PersistentClassInfo> resetCit(&_resetClassesThatShouldNotBeNewlyExtended);
   for (TR_PersistentClassInfo *resetClazz = resetCit.getCurrent(); resetClazz; resetClazz = resetCit.getNext())
      {
      if (!_prexClassesThatShouldNotBeNewlyExtended.find(resetClazz->getClassId()))
         {
         resetClazz->resetShouldNotBeNewlyExtended(comp()->getCompThreadID());
         }
      }

   _prexClassesThatShouldNotBeNewlyExtended.init();


#endif

      // Change here
   for (ClassInitInfo *cii = _classesToCheckInit.getFirst(); cii; cii = cii->getNext())
      {
      if (cii->_block->nodeIsRemoved())
         continue;

      //printf("Doing new vp trace transformation in %s\n", comp()->signature()); fflush(stdout);

      TR::Block *succToSplit = NULL;
      TR::TreeTop *ifTree = cii->_tt;
      TR::TreeTop *prevTree = ifTree->getPrevTreeTop();
      TR::Node *ifNode = cii->_tt->getNode();
      TR::Block *block = cii->_tt->getEnclosingBlock();
      TR::Block *nextBlock = block->getNextBlock();
      TR::Block *origDest = ifNode->getBranchDestination()->getNode()->getBlock();
      TR::Block *origNext = nextBlock;
      if (ifNode->getOpCodeValue() == TR::ifacmpne)
         {
         succToSplit = origDest;
         origNext = origDest;
         origDest = nextBlock;
         }
      else
         {
         TR::Block *newBlock = block->breakFallThrough(comp(), block, nextBlock);
         TR::Node::recreate(ifNode, TR::ifacmpne);

         newBlock->getLastRealTreeTop()->getNode()->setBranchDestination(ifNode->getBranchDestination());

         comp()->getFlowGraph()->addEdge(newBlock, origDest);
         comp()->getFlowGraph()->addEdge(block, nextBlock);
         comp()->getFlowGraph()->removeEdge(newBlock, nextBlock);
         comp()->getFlowGraph()->removeEdge(block, origDest);

         ifNode->setBranchDestination(nextBlock->getEntry());
         succToSplit = nextBlock;
         nextBlock = newBlock;
    }


      TR::Block *splitBlock = block->splitEdge(block, succToSplit, comp());
      if (splitBlock->getLastRealTreeTop()->getNode()->getOpCodeValue() != TR::Goto)
         {
         TR::Node *gotoNode = TR::Node::create(ifNode, TR::Goto, 0, origDest->getEntry());
         TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
         splitBlock->append(gotoTree);
         }
      else
         {
         splitBlock->getLastRealTreeTop()->getNode()->setBranchDestination(origDest->getEntry());
         }

      comp()->getFlowGraph()->addEdge(splitBlock, origDest);


      TR::Block *splitBlock2 = block->splitEdge(block, splitBlock, comp());
      ifNode->setBranchDestination(origNext->getEntry());

      TR::Node *origFirst = ifNode->getFirstChild();
      TR::Node *origSecond = ifNode->getSecondChild();
      bool recognizedStatic = false;

      if ((origFirst->getOpCodeValue() == TR::aload) &&
          origFirst->getSymbol()->isStaticField())
         {
         int32_t staticNameLen = -1;
         char *staticName = NULL;
         staticName = origFirst->getSymbolReference()->getOwningMethod(comp())->staticName(origFirst->getSymbolReference()->getCPIndex(), staticNameLen, comp()->trMemory());

         if ((staticName && (staticNameLen > 0) &&
             (!strncmp(staticName, "com/ibm/websphere/ras/TraceComponent.fineTracingEnabled", 55) ||
              !strncmp(staticName, "com/ibm/ejs/ras/TraceComponent.anyTracingEnabled", 48) ||
              !strncmp(staticName, "java/lang/String.compressionFlag", 32))) ||
             ((cii->_len == 41) && !strncmp(cii->_sig, "Lcom/ibm/websphere/ras/TraceEnabledToken;", cii->_len)) ||
             ((cii->_len == 35) && !strncmp(cii->_sig, "Lcom/ibm/ejs/ras/TraceEnabledToken;", cii->_len)) ||
             ((cii->_len == 40) && !strncmp(cii->_sig, "Ljava/lang/String$StringCompressionFlag;", cii->_len)))
            {
            recognizedStatic = true;
            ifNode->setAndIncChild(0, origFirst->duplicateTree());
            ifNode->setAndIncChild(1, TR::Node::create(origSecond, TR::aconst, 0, 0));
            }
         }

     if (!recognizedStatic)
        {
        TR::DataType dataType = origFirst->getDataType();
        TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType, false, 0);

        TR::Node *astoreNode = TR::Node::createWithSymRef(TR::astore, 1, 1, ifNode->getFirstChild(), newSymbolReference);
        TR::TreeTop *astoreTree = TR::TreeTop::create(comp(), astoreNode, NULL, NULL);

        prevTree->join(astoreTree);
        astoreTree->join(ifTree);
        prevTree = astoreTree;

        ifNode->setAndIncChild(0, TR::Node::createWithSymRef(TR::aload, 0, newSymbolReference));
        ifNode->setAndIncChild(1, TR::Node::create(origSecond, TR::aconst, 0, 0));
        }

      prevTree->join(block->getExit());

      origFirst->recursivelyDecReferenceCount();
      origSecond->recursivelyDecReferenceCount();

      TR::Node *guard = comp()->createSideEffectGuard(comp(), block->getEntry()->getNode(), splitBlock2->getEntry());

      TR::TreeTop *lastTree = splitBlock2->getLastRealTreeTop();
      if (lastTree->getNode()->getOpCodeValue() == TR::Goto)
	 {
	 TR::TreeTop *treeBeforeLast = lastTree->getPrevTreeTop();
         treeBeforeLast->join(lastTree->getNextTreeTop());
	 }

      TR::TreeTop *guardTree = TR::TreeTop::create(comp(), guard, NULL, NULL);
      block->append(guardTree);

      splitBlock2->append(ifTree);
      splitBlock2->setIsCold();
      splitBlock2->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
      comp()->getFlowGraph()->addEdge(splitBlock2, origNext);
      comp()->getFlowGraph()->removeEdge(splitBlock, succToSplit);

      if (splitBlock2->getNextBlock() != splitBlock)
         {
         TR::Block *nextToSplitBlock2 = splitBlock2->getNextBlock();
         TR::Block *prevToSplitBlock2 = splitBlock2->getPrevBlock();
         if (nextToSplitBlock2)
            prevToSplitBlock2->getExit()->join(nextToSplitBlock2->getEntry());
         else
            prevToSplitBlock2->getExit()->setNextTreeTop(NULL);

         TR::Block *prevToSplitBlock = splitBlock->getPrevBlock();
         prevToSplitBlock->getExit()->join(splitBlock2->getEntry());
         splitBlock2->getExit()->join(splitBlock->getEntry());
         }

      bool found = false;
      for (TR_ClassLoadCheck * clc = comp()->getClassesThatShouldNotBeLoaded()->getFirst(); clc; clc = clc->getNext())
         {
         if (clc->_length == cii->_len && !strncmp(clc->_name, cii->_sig, cii->_len))
            {
            found = true;
            break;
            }
         }

      if (!found)
         comp()->getClassesThatShouldNotBeLoaded()->add(new (trHeapMemory()) TR_ClassLoadCheck(cii->_sig, cii->_len));
      }

   _classesToCheckInit.setFirst(0);

   comp()->getSymRefTab()->aliasBuilder.unsafeArrayElementSymRefs().empty();

   if (!_unsafeArrayAccessNodes->isEmpty())
      {
      vcount_t visitCount = comp()->incVisitCount();
      TR::TreeTop *curTree = comp()->getStartTree();
      while (curTree)
         {
         if (curTree->getNode() &&
            !checkAllUnsafeReferences(curTree->getNode(), visitCount))
            {
            comp()->getSymRefTab()->aliasBuilder.unsafeArrayElementSymRefs().empty();
            break;
            }
         curTree = curTree->getNextTreeTop();
         }
      }

   if (trace())
      {
      traceMsg(comp(), "Unsafe references that are only used to access array elements: ");
      comp()->getSymRefTab()->aliasBuilder.unsafeArrayElementSymRefs().print(comp());
      traceMsg(comp(), "\n");
      }
   }

void constrainRangeByPrecision(const int64_t low, const int64_t high, const int32_t precision, int64_t &lowResult, int64_t &highResult, bool isNonNegative)
   {
   lowResult = low;
   highResult = high;

   if (precision >= 1)
      {
      int64_t highFromP = getMaxAbsValueForPrecision(precision);
      int64_t lowFromP = highFromP * -1;

      if (highFromP != TR::getMaxSigned<TR::Int64>())
         {
         lowResult = std::max(low, lowFromP);
         highResult = std::min(high, highFromP);
         }
      }

   if (isNonNegative)
      lowResult = 0;
   }

#if USE_TREES
void OMR::ValuePropagation::ValueConstraintHandler::setVP(OMR::ValuePropagation * vp)
   {
   _vp = vp;
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::allocate(int32_t key)
   {
   return _vp->createValueConstraint(key, NULL, NULL);
   }

void OMR::ValuePropagation::ValueConstraintHandler::free(ValueConstraint * vc)
   {
   _vp->freeValueConstraint(vc);
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::copy(ValueConstraint * vc)
   {
   Relationship *rel = _vp->copyRelationships(vc->relationships.getFirst());
   StoreRelationship *storeRel = _vp->copyStoreRelationships(vc->storeRelationships.getFirst());
   ValueConstraint *newvc = _vp->createValueConstraint(vc->getValueNumber(), rel, storeRel);
   return newvc;
   }

TR::Compilation * OMR::ValuePropagation::ValueConstraintHandler::comp()
   {
   return _vp->comp();
   }

#else
void OMR::ValuePropagation::ValueConstraintHandler::setVP(OMR::ValuePropagation * vp)
   {
   _vp = vp;
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::allocate(int32_t key)
   {
   return _vp->createValueConstraint(key, NULL, NULL);
   }

void OMR::ValuePropagation::ValueConstraintHandler::free(ValueConstraint * vc)
   {
   _vp->freeValueConstraint(vc);
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::copy(ValueConstraint * vc)
   {
   Relationship *rel = _vp->copyRelationships(vc->relationships.getFirst());
   StoreRelationship *storeRel = _vp->copyStoreRelationships(vc->storeRelationships.getFirst());
   ValueConstraint *newvc = _vp->createValueConstraint(vc->getValueNumber(), rel, storeRel);
   return newvc;
   }

void OMR::ValuePropagation::ValueConstraintHandler::empty(ValueConstraints & valueConstraints)
   {
   ValueConstraint *vc;
   while (vc = valueConstraints.pop())
      free(vc);
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::copyAll(ValueConstraints & valueConstraints)
   {
   TR_LinkHeadAndTail<ValueConstraint> newList;
   ValueConstraint *vc, *newVc;
   for (vc = valueConstraints.getFirst(); vc; vc = vc->getNext())
      {
      newList.append(copy(vc));
      }
   return newList.getFirst();
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::getRoot(ValueConstraints & list)
   {
   return list.getFirst();
   }

void OMR::ValuePropagation::ValueConstraintHandler::setRoot(ValueConstraints & list, ValueConstraint * vc)
   {
   list.setFirst(vc);
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::find(int32_t key, ValueConstraints & list)
   {
   ValueConstraint *prev, *cur;
   for (cur = list.getFirst(), prev = NULL; cur; prev = cur, cur = cur->getNext())
      {
      if (cur->getValueNumber() < key)
         continue;
      if (cur->getValueNumber() > key)
         break;
      return cur;
      }
   return NULL;
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::findOrCreate(int32_t key, ValueConstraints & list)
   {
   ValueConstraint *prev, *cur;
   for (cur = list.getFirst(), prev = NULL; cur; prev = cur, cur = cur->getNext())
      {
      if (cur->getValueNumber() < key)
         continue;
      if (cur->getValueNumber() > key)
         break;
      return cur;
      }
   ValueConstraint *result = allocate(key);
   result->setNext(cur);
   if (prev) prev->setNext(result);
   else list.setFirst(result);
   return result;
   }

OMR::ValuePropagation::ValueConstraint * OMR::ValuePropagation::ValueConstraintHandler::remove(int32_t key, ValueConstraints & list)
   {
   ValueConstraint *prev, *cur;
   for (cur = list.getFirst(), prev = NULL; cur; prev = cur, cur = cur->getNext())
      {
      if (cur->getValueNumber() < key)
         continue;
      if (cur->getValueNumber() > key)
         break;
      if (prev) prev->setNext(cur->getNext());
      else list.setFirst(cur->getNext());
      return cur;
      }
   return NULL;
   }
#endif

OMR::ValuePropagation::InductionVariable::InductionVariable(TR::Symbol * sym, TR::Node * entryDef, int32_t incrVN, TR::VPConstraint * incr, OMR::ValuePropagation * vp)
   : _symbol(sym), _entryDef(entryDef), _entryConstraint(0), _incrementVN(incrVN), _increment(incr)
   {
   _valueNumber = vp->_numValueNumbers++;
   _onlyIncrValid = false;
   }

OMR::ValuePropagation::CallInfo::CallInfo(OMR::ValuePropagation * vp, TR_OpaqueClassBlock * thisType, TR_PrexArgInfo * argInfo)
   : _tt(vp->_curTree), _block(vp->_curBlock), _thisType(thisType), _argInfo(argInfo)
   {}

OMR::ValuePropagation::VirtualGuardInfo::VirtualGuardInfo(OMR::ValuePropagation * vp, TR_VirtualGuard * vgOld, TR_VirtualGuard * vgNew, TR::Node * newGNode, TR::Node * cn)
   : _currentTree(vp->_curTree), _block(vp->_curBlock), _oldVirtualGuard(vgOld), _newVirtualGuard(vgNew), _newGuardNode(newGNode), _callNode(cn)
   {}

OMR::ValuePropagation::ClassInitInfo::ClassInitInfo(OMR::ValuePropagation * vp, char * sig, int32_t len)
   : _tt(vp->_curTree), _block(vp->_curBlock), _sig(sig), _len(len)
   {}
