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
 *******************************************************************************/

#ifndef VALUEPROP_INCL
#define VALUEPROP_INCL

#include <stddef.h>                           // for NULL
#include <stdint.h>                           // for int32_t, int64_t, etc
#include "compile/Compilation.hpp"            // for Compilation
#include "cs2/hashtab.h"                      // for HashTable
#include "env/TRMemory.hpp"                   // for TR_Memory, etc
#include "env/jittypes.h"                     // for uintptrj_t
#include "il/DataTypes.hpp"                   // for DataTypes, TR_YesNoMaybe
#include "il/ILOpCodes.hpp"                   // for ILOpCodes::DIVCHK
#include "il/Node.hpp"                        // for Node, vcount_t
#include "infra/Link.hpp"                     // for TR_LinkHead, TR_Link, etc
#include "infra/List.hpp"                     // for List, TR_ScratchList
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

#define USE_TREES   1
#define HEDGE_TREES 1

#if USE_TREES
   #if HEDGE_TREES
      #define TREE_CLASS TR_HedgeTree
      #define TREE_NODE TR_HedgeNode
      #define TREE_ITERATOR TR_HedgeTreeIterator
      #define TREE_HANDLER TR_HedgeTreeHandler
      #include "infra/HedgeTree.hpp"
   #else
      #define TREE_CLASS TR_AVLTree
      #define TREE_NODE TR_AVLNode
      #define TREE_ITERATOR TR_AVLTreeIterator
      #define TREE_HANDLER TR_AVLTreeHandler
      #include "infra/AVLTree.hpp"
   #endif
#endif

#define VP_HASH_TABLE_SIZE 251
#define VP_SPECIALKLASS -1

class TR_BitVector;
class TR_OpaqueClassBlock;
class TR_PersistentClassInfo;
class TR_PrexArgInfo;
class TR_RegionStructure;
class TR_ResolvedMethod;
class TR_Structure;
class TR_StructureSubGraphNode;
class TR_UseDefInfo;
class TR_VPClassType;
class TR_VPConstraint;
class TR_VPIntConst;
class TR_VPNonNullObject;
class TR_VPNullObject;
class TR_VPPreexistentObject;
class TR_VPUnreachablePath;
class TR_VP_BCDSign;
class TR_ValueNumberInfo;
class TR_ValuePropagation;
class TR_VirtualGuard;
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class StaticSymbol; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;
template <class T> class TR_Stack;

typedef TR::Node* (* ValuePropagationPtr)(TR_ValuePropagation *, TR::Node *);
extern const ValuePropagationPtr constraintHandlers[];

class TR_ArraycopyTransformation : public TR::Optimization
   {
   public:
   TR_ArraycopyTransformation(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_ArraycopyTransformation(manager);
      }

   virtual int32_t perform();

   private:
   TR::TreeTop* createArrayNode(TR::TreeTop* tree, TR::TreeTop* newTree, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef, TR::SymbolReference* lenRef, TR::SymbolReference* srcObjRef, TR::SymbolReference* dstObjRef, bool isForward);
   TR::TreeTop* createArrayNode(TR::TreeTop* tree, TR::TreeTop* newTree, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef, TR::Node* lenRef, TR::SymbolReference* srcObjRef, TR::SymbolReference* dstObjRef, bool isForward);


   int64_t arraycopyHighFrequencySpecificLength(TR::Node* byteLenNode);
   TR::TreeTop* createPointerCompareNode(TR::Node* node, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef);
   TR::TreeTop* createRangeCompareNode(TR::Node* node, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef, TR::SymbolReference* lenRef);
//   int shiftAmount(TR::DataTypes type, TR::Node* node);

   TR::TreeTop* createMultipleArrayNodes(TR::TreeTop* arrayTreeTop, TR::Node* node);
   TR::TreeTop* tryToSpecializeForLength(TR::TreeTop *tt, TR::Node *arraycopyNode);
   TR::TreeTop* specializeForLength(TR::TreeTop *tt, TR::Node *arraycopyNode, uintptrj_t lengthInBytes,
         TR::SymbolReference *srcRef,
         TR::SymbolReference *dstRef,
         TR::SymbolReference *lenRef,
         TR::SymbolReference *srcObjRef,
         TR::SymbolReference *dstObjRef);

   bool hasChangedTrees()
      {
      return _changedTrees;
      }
   void setChangedTrees(bool state)
      {
      _changedTrees = state;
      }
   bool _changedTrees;
   };

class TR_ValuePropagation : public TR::Optimization
   {
   public:
   int32_t _syncValueNumber;

   TR_ValuePropagation(TR::OptimizationManager *manager);

   void initialize();

   // Constraints hash table.
   // Constraints are immutable, so they can be shared. This hash table is the
   // means by which constraints are found in order to be shared.
   //
   struct ConstraintsHashTableEntry
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      ConstraintsHashTableEntry *next;
      TR_VPConstraint           *constraint;
      };
   void addConstraint(TR_VPConstraint *constraint, int32_t hash);


   // Relationship. This is a property of a value number and represents either
   //    1) a relationship with another value number, when the "relative" field
   //       gives the other value number, or
   //    2) a special constraint, when the "relative" field defines the special
   //       constraint, as follows:
   //          (-1) an absolute constraint
   //
   enum SpecialRelationships
      {
      AbsoluteConstraint = -1
      };
   struct Relationship : public TR_Link<Relationship>
      {
      int32_t          relative;
      TR_VPConstraint *constraint;
      void print(TR_ValuePropagation *vp);
      void print(TR_ValuePropagation *vp, int32_t valueNumber, int32_t indent);
      };
   Relationship *createRelationship(int32_t relative, TR_VPConstraint *constraint);
   void          freeRelationship(Relationship *rel);
   void          freeRelationships(TR_LinkHead<Relationship> &list);
   Relationship *copyRelationships(Relationship *first);

   TR_LinkHead<Relationship> _relationshipCache;

   // Store relationship. This represents constraints on a value that has been
   // stored into a variable, and is identified by a symbol for the stored
   // variable and a list of relationships.
   // Store relationships are treated specially at merge points and are used to
   // get values of def nodes to compose constraints for use nodes.
   //
   struct StoreRelationship : public TR_Link<StoreRelationship>
      {
      TR::Symbol        *symbol;
      TR_LinkHead<Relationship> relationships;
      void print(TR_ValuePropagation *vp, int32_t valueNumber, int32_t indent);
      };
   StoreRelationship *createStoreRelationship(TR::Symbol *symbol, Relationship *firstRel);
   void          freeStoreRelationship(StoreRelationship *rel);
   void          freeStoreRelationships(TR_LinkHead<StoreRelationship> &list);
   StoreRelationship *copyStoreRelationships(StoreRelationship *first);

   TR_LinkHead<StoreRelationship> _storeRelationshipCache;

   void addUnsafeArrayAccessNode(ncount_t index) { _unsafeArrayAccessNodes->set(index); }

   // Value constraint. This represents constraints applied to a particular
   // value number, and is represented by a linked list of relationships.
   //
#if USE_TREES
   class ValueConstraint : public TREE_NODE<ValueConstraint>
      {
      public:
      ValueConstraint(int32_t valueNumber)
         : TREE_NODE<ValueConstraint>(valueNumber) {}

      void initialize(int32_t valueNumber, Relationship *rel, StoreRelationship *storeRel)
         {
         TREE_NODE<ValueConstraint>::initialize(valueNumber);
         relationships.setFirst(rel);
         storeRelationships.setFirst(storeRel);
         }

      int32_t getValueNumber() {return getKey();}

      TR_LinkHead<Relationship> relationships;
      TR_LinkHead<StoreRelationship> storeRelationships;
      void print(TR_ValuePropagation *vp, int32_t indent);
      };

   typedef TREE_CLASS<ValueConstraint> ValueConstraints;
   typedef TREE_ITERATOR<ValueConstraint> ValueConstraintIterator;

#else
   class ValueConstraint : public TR_Link<ValueConstraint>
      {
      public:
      ValueConstraint(int32_t valueNumber)
         : _valueNumber(valueNumber) {}

      void initialize(int32_t valueNumber, Relationship *rel, StoreRelationship *storeRel)
         {
         _valueNumber = valueNumber;
         relationships.setFirst(rel);
         storeRelationships.setFirst(storeRel);
         setNext(NULL);
         }

      int32_t getValueNumber() {return _valueNumber;}

      TR_LinkHead<Relationship> relationships;
      TR_LinkHead<StoreRelationship> storeRelationships;
      void print(TR_ValuePropagation *vp, int32_t indent);

      private:
      int32_t _valueNumber;

      };

   typedef TR_LinkHead<ValueConstraint> ValueConstraints;

   class ValueConstraintIterator
      {
      public:
      ValueConstraintIterator() : _list(NULL) {}
      ValueConstraintIterator(ValueConstraints &list) {reset(list);}
      void reset(ValueConstraints &list)
         {
         _list = &list;
         _next = list.getFirst();
         }
      ValueConstraint *getFirst() {_next = _list->getFirst(); return getNext();}
      ValueConstraint *getNext() {ValueConstraint *vc = _next; if (_next) _next = _next->getNext(); return vc;}
      ValueConstraints *getBase() {return _list;}
      private:
      ValueConstraints *_list;
      ValueConstraint  *_next;
      };

#endif

   ValueConstraint *createValueConstraint(int32_t valueNumber, Relationship *relationships, StoreRelationship *storeRelationships);
   void             freeValueConstraint(ValueConstraint *vc);
   void             freeValueConstraints(ValueConstraints &valueConstraints);
   ValueConstraint *copyValueConstraints(ValueConstraints &valueConstraints);

#if USE_TREES
   class ValueConstraintHandler : public TREE_HANDLER<ValueConstraint>
      {
      public:
      void setVP(TR_ValuePropagation *vp) {_vp = vp;}

      virtual ValueConstraint *allocate(int32_t key)
         {
         return _vp->createValueConstraint(key, NULL, NULL);
         }
      virtual void free(ValueConstraint *vc)
         {
         _vp->freeValueConstraint(vc);
         }
      virtual ValueConstraint *copy(ValueConstraint *vc)
         {
         Relationship *rel = _vp->copyRelationships(vc->relationships.getFirst());
         StoreRelationship *storeRel = _vp->copyStoreRelationships(vc->storeRelationships.getFirst());
         ValueConstraint *newvc = _vp->createValueConstraint(vc->getValueNumber(), rel, storeRel);
         return newvc;
         }

      virtual TR::Compilation *comp() {return _vp->comp();}

      private:
      TR_ValuePropagation *_vp;
      };
#else
   class ValueConstraintHandler
      {
      public:
      void setVP(TR_ValuePropagation *vp) {_vp = vp;}

      ValueConstraint *allocate(int32_t key)
         {
         return _vp->createValueConstraint(key, NULL, NULL);
         }
      void free(ValueConstraint *vc)
         {
         _vp->freeValueConstraint(vc);
         }
      ValueConstraint *copy(ValueConstraint *vc)
         {
         Relationship *rel = _vp->copyRelationships(vc->relationships.getFirst());
         StoreRelationship *storeRel = _vp->copyStoreRelationships(vc->storeRelationships.getFirst());
         ValueConstraint *newvc = _vp->createValueConstraint(vc->getValueNumber(), rel, storeRel);
         return newvc;
         }

      void empty(ValueConstraints &valueConstraints)
         {
         ValueConstraint *vc;
         while (vc = valueConstraints.pop())
            free(vc);
         }
      ValueConstraint *copyAll(ValueConstraints &valueConstraints)
         {
         TR_LinkHeadAndTail<ValueConstraint> newList;
         ValueConstraint *vc, *newVc;
         for (vc = valueConstraints.getFirst(); vc; vc = vc->getNext())
            {
            newList.append(copy(vc));
            }
         return newList.getFirst();
         }
      ValueConstraint *getRoot(ValueConstraints &list) {return list.getFirst();}
      void setRoot(ValueConstraints &list, ValueConstraint *vc) {list.setFirst(vc);}

      ValueConstraint *find(int32_t key, ValueConstraints &list)
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

      ValueConstraint *findOrCreate(int32_t key, ValueConstraints &list)
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
         if (prev) prev->setNext(result); else list.setFirst(result);
         return result;
         }

      ValueConstraint *remove(int32_t key, ValueConstraints &list)
         {
         ValueConstraint *prev, *cur;
         for (cur = list.getFirst(), prev = NULL; cur; prev = cur, cur = cur->getNext())
            {
            if (cur->getValueNumber() < key)
               continue;
            if (cur->getValueNumber() > key)
               break;
            if (prev) prev->setNext(cur->getNext()); else list.setFirst(cur->getNext());
            return cur;
            }
         return NULL;
         }
      private:
      TR_ValuePropagation *_vp;
      };
#endif

   TR_Stack<ValueConstraint*> *_valueConstraintCache;



   // Global constraints hash table.
   // This table contains the value constraints that apply across the whole method.
   // It is keyed on the value number, and each entry contains a list of
   // relationships.
   //
   struct GlobalConstraint
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      static GlobalConstraint *create(TR::Compilation *, int32_t valueNumber);

      GlobalConstraint         *next;
      TR_LinkHead<Relationship> constraints;
      int32_t                   valueNumber;
      };
   //GlobalConstraint *createGlobalConstraint(TR::CFGEdge *edge, bool keepBlockList);
   GlobalConstraint *findGlobalConstraint(int32_t valueNumber);
   Relationship     *findGlobalConstraint(int32_t valueNumber, int32_t relative);
   GlobalConstraint *createGlobalConstraint(int32_t valueNumber);
   uint32_t hashGlobalConstraint(int32_t valueNumber);



   // Edge constraints hash table.
   // This table contains the value constraints that apply to edges in the CFG.
   // It is keyed on the edge, and each entry contains a list of value
   // constraints that apply to that particular edge.
   // Edge constraints are also kept for edges in the various region subgraphs.
   //
   struct EdgeConstraints
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      static EdgeConstraints *create(TR::Compilation *, TR::CFGEdge *edge);

      EdgeConstraints               *next;
      TR::CFGEdge                    *edge;
      ValueConstraints               valueConstraints;
      };
   EdgeConstraints *createEdgeConstraints(TR::CFGEdge *edge, bool keepBlockList);
   void createExceptionEdgeConstraints(uint32_t exceptions, ValueConstraint *extraConstraint, TR::Node *reason);
   EdgeConstraints *getEdgeConstraints(TR::CFGEdge *edge);



   // Loop defs hash table.
   // This table contains def nodes that were used to resolve uses that also
   // had as-yet-unseen def nodes. The table is filled the first time through
   // loops and the second time through each node is associated with the loop
   // region that contains it. This allows resolution of the use nodes to know
   // which loops' back edges to look in for the as-yet-unseen def nodes.
   //
   struct LoopDefsHashTableEntry
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      LoopDefsHashTableEntry *next;
      TR::Node                *node;
      TR_RegionStructure     *region;
      };
   void addLoopDef(TR::Node *node);
   LoopDefsHashTableEntry *findLoopDef(TR::Node *node);



   TR_VPConstraint *addGlobalConstraint(TR::Node *node, TR_VPConstraint *constraint, TR::Node *relative = NULL);
   TR_VPConstraint *addGlobalConstraint(TR::Node *node, int32_t valueNumber, TR_VPConstraint *constraint, int32_t relative);
   TR_VPConstraint *addBlockConstraint(TR::Node *node, TR_VPConstraint *constraint, TR::Node *relative = NULL, bool mustBeValid = true);
   TR_VPConstraint *addEdgeConstraint(TR::Node *node, TR_VPConstraint *constraint, EdgeConstraints *contraints, TR::Node *relative = NULL);
   TR_VPConstraint *addConstraintToList(TR::Node *node, int32_t valueNumber, int32_t relative, TR_VPConstraint *constraint, ValueConstraints *valueConstraints, bool replaceExisting = false);
   TR_VPConstraint *addBlockOrGlobalConstraint(TR::Node *node, TR_VPConstraint *constraint, bool isGlobal, TR::Node *relative=NULL);
   void mergeRelationships(TR_LinkHead<Relationship> &fromList, TR_LinkHead<Relationship> &toList, int32_t valueNumber, bool preserveFrom = false, StoreRelationship *mergingStore = NULL, List<TR::Symbol> *storeSymbols = NULL, bool inBothLists = false);
   void mergeStoreRelationships(ValueConstraint *fromvc, ValueConstraint *tovc, bool preserveFrom = false);
   void mergeValueConstraints(ValueConstraint *fromvc, ValueConstraint *tovc, bool preserveFrom = false);
   void mergeEdgeConstraints(EdgeConstraints *fromEdge, EdgeConstraints *toEdge);
   void mergeConstraintIntoEdge(ValueConstraint *constraint, EdgeConstraints *edge);
   void removeConstraint(int32_t valueNumber, ValueConstraints &valueConstraints, int32_t relative = AbsoluteConstraint);
   void collectBackEdgeConstraints();
   void mergeBackEdgeConstraints(EdgeConstraints *edge);

   // Find the constraint for a value number in the current block
   //
   StoreRelationship *findStoreRelationship(TR_LinkHead<StoreRelationship> &list, TR::Symbol *symbol);
   Relationship *findConstraintInList(TR_LinkHead<Relationship> &list, int32_t relative);
   Relationship *findConstraint(int32_t valueNumber, int32_t relative = AbsoluteConstraint);
   Relationship *findValueConstraint(int32_t valueNumber, ValueConstraints &valueConstraints, int32_t relative = AbsoluteConstraint);
   Relationship *findEdgeConstraint(int32_t valueNumber, EdgeConstraints *edge, int32_t relative = AbsoluteConstraint);

   StoreRelationship *findStoreConstraint(int32_t valueNumber, TR::Symbol *symbol);
   StoreRelationship *findStoreValueConstraint(int32_t valueNumber, TR::Symbol *symbol, ValueConstraints &valueConstraints);
   StoreRelationship *findStoreEdgeConstraint(int32_t valueNumber, TR::Symbol *symbol, EdgeConstraints *edge);

   // Get the constraint for a node in the current block, or create one from
   // the node's def points.
   //
   TR_VPConstraint *getConstraint(TR::Node *node, bool &isGlobal, TR::Node *relative = NULL);
   TR_VPConstraint *mergeDefConstraints(TR::Node *node, int32_t relative, bool &isGlobal, bool forceMerge = false);
   TR_VPConstraint *applyGlobalConstraints(TR::Node *node, int32_t valueNumber, TR_VPConstraint *constraint, int32_t relative);
   void invalidateParmConstraintsIfNeeded(TR::Node *node, TR_VPConstraint *constraint);
   void checkTypeRelationship(TR_VPConstraint *lhs, TR_VPConstraint *rhs, int32_t &value, bool isInstanceOf, bool isCheckCast);
   TR_YesNoMaybe isCastClassObject(TR_VPClassType *type);
   TR_VPConstraint *getStoreConstraint(TR::Node *node, TR::Node *relative = NULL);
   void createStoreConstraints(TR::Node *node);
   void setUnreachableStore(StoreRelationship *store);
   bool isUnreachableStore(StoreRelationship *store);
   bool isDefInUnreachableBlock(int32_t defIndex);
   bool hasBeenStored(int32_t valueNumber, TR::Symbol *symbol, ValueConstraints &valueConstraints);

   void generalizeStores(ValueConstraints &stores, ValueConstraints *edgeConstraints);
   void findStoresInBlock(TR::Block *block, ValueConstraints &stores);

   bool propagateConstraint(TR::Node *node, int32_t valueNumber, Relationship *first, Relationship *rel, ValueConstraints *valueConstraints);

   // Methods to help in tree transformations
   //
   void removeNode(TR::Node *node, bool anchorIt = true);
   void removeChildren(TR::Node *node, bool anchorIt = true);
   void replaceByConstant(TR::Node *node, TR_VPConstraint *constraint, bool isGlobal);
   void removeRestOfBlock();
   void mustTakeException();
   void processTrees(TR::TreeTop *startTree, TR::TreeTop *endTree);
   void transformArrayCopyCall(TR::Node *node);
   bool canTransformArrayCopyCallForSmall(TR::Node *node, int32_t &srcLength, int32_t &dstLength, int32_t &stride, TR::DataTypes &type );
   int32_t getPrimitiveArrayType(char primitiveArrayChar);

   bool canRunTransformToArrayCopy();
   bool transformUnsafeCopyMemoryCall(TR::Node *arraycopyNode);

   static TR::CFGEdge *findOutEdge(TR::list<TR::CFGEdge*> &edges, TR::CFGNode *target);
   bool isUnreachablePath(ValueConstraints &valueConstraints);
   bool isUnreachablePath(EdgeConstraints *constraints);
   void setUnreachablePath();
   void setUnreachablePath(TR::CFGEdge *edge);
   void setUnreachablePath(ValueConstraints &vc);

   void launchNode(TR::Node *node, TR::Node *parent, int32_t whichChild);

   bool checkAllUnsafeReferences(TR::Node *node, vcount_t visitCount);
   void doDelayedTransformations();

   void replacePackedArrayLoad(TR::Node *loadNode, TR::Node *packedNode, TR::Node *curNode, vcount_t visitCount);

   struct TR_TreeTopNodePair
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      TR_TreeTopNodePair(TR::TreeTop *treetop, TR::Node *n)
         : _treetop(treetop), _node(n)
         {}

      TR::TreeTop                    *_treetop;
      TR::Node                       *_node;
      };

   struct TR_VPTreeTopPair
   {
   TR_ALLOC(TR_Memory::ValuePropagation)

      TR_VPTreeTopPair(TR::TreeTop *treetop1,TR::TreeTop *treetop2): _treetop1(treetop1),_treetop2(treetop2){}

      TR::TreeTop                    *_treetop1;
      TR::TreeTop                    *_treetop2;

   };


   struct TR_VPStringCached
      {

      TR_ALLOC(TR_Memory::ValuePropagation)

      TR_VPStringCached(TR::TreeTop *append1, TR::TreeTop *append2, TR::Node *appendString1, TR::Node *appendString2, TR::TreeTop *newTree, TR::TreeTop *toStringTree)
         : _treetop1(append1), _treetop2(append2), _appendedString1(appendString1), _appendedString2(appendString2), _newTree(newTree), _toStringTree(toStringTree)
         {}
      TR::TreeTop                    *_treetop1;
      TR::TreeTop                    *_treetop2;
      TR::Node						 *_appendedString1;
      TR::Node						 *_appendedString2;
      TR::TreeTop					 *_newTree;
      TR::TreeTop					 *_toStringTree;

      };

   struct TR_TreeTopWrtBarFlag
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      TR_TreeTopWrtBarFlag(TR::TreeTop *treetop, uint8_t b)
         : _treetop(treetop), _flag(b)
         {}

      TR::TreeTop                    *_treetop;
      uint8_t                          _flag;
      };


   struct TR_RealTimeArrayCopy
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      TR_RealTimeArrayCopy(TR::TreeTop *vcall, TR::DataTypes type, uint8_t b)
         : _treetop(vcall), _flag(b), _type(type)
         {}

      TR::TreeTop  *_treetop;
      uint8_t      _flag;
      TR::DataTypes _type;
      };

   struct TR_ArrayCopySpineCheck
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      TR_ArrayCopySpineCheck(
         TR::TreeTop *arraycopyTree,
         TR::SymbolReference *arraycopySymRef,
         TR::Node    *srcObjNode,
         TR::Node    *srcOffNode,
         TR::Node    *dstObjNode,
         TR::Node    *dstOffNode,
         TR::Node    *copyLenNode) :
            _arraycopyTree(arraycopyTree),
            _arraycopySymRef(arraycopySymRef),
            _srcObjNode(srcObjNode),
            _srcOffNode(srcOffNode),
            _dstObjNode(dstObjNode),
            _dstOffNode(dstOffNode),
            _copyLenNode(copyLenNode),
            _srcObjRef(NULL),
            _srcOffRef(NULL),
            _dstObjRef(NULL),
            _dstOffRef(NULL),
            _copyLenRef(NULL) {}

      TR::TreeTop *_arraycopyTree;

      TR::Node *_srcObjNode;
      TR::Node *_srcOffNode;
      TR::Node *_dstObjNode;
      TR::Node *_dstOffNode;
      TR::Node *_copyLenNode;

      TR::SymbolReference *_srcObjRef;
      TR::SymbolReference *_srcOffRef;
      TR::SymbolReference *_dstObjRef;
      TR::SymbolReference *_dstOffRef;
      TR::SymbolReference *_copyLenRef;

      TR::SymbolReference *_arraycopySymRef;
      };

   struct TR_PackedArrayLoad
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      TR_PackedArrayLoad(
         TR::TreeTop *loadTT,
         TR::Node    *loadNode,
         TR::Node    *clazzPointer,
         TR::Node    *packedDS) :
            _loadTT(loadTT),
            _loadNode(loadNode),
            _clazzPointer(clazzPointer),
            _packedDS(packedDS) {}

      TR::TreeTop *_loadTT;
      TR::Node *_loadNode;
      TR::Node *_clazzPointer;
      TR::Node *_packedDS;
      };

   TR::TreeTop *createPrimitiveOrReferenceCompareNode(TR::Node *);
   TR::TreeTop *createArrayStoreCompareNode(TR::Node *, TR::Node *);

   TR::TreeTop *createSpineCheckNode(TR::Node *node, TR::SymbolReference *objSymRef);
   TR::TreeTop *createAndInsertStoresForArrayCopySpineCheck(TR_ArrayCopySpineCheck *checkInfo);
   TR::TreeTop *createArrayCopyCallForSpineCheck(TR_ArrayCopySpineCheck *checkInfo);
   void transformArrayCopySpineCheck(TR_ArrayCopySpineCheck *checkInfo);


   void removeArrayCopyNode(TR::TreeTop *);
   TR::SymbolReference * getStringCacheRef();
   void transformStringConcats(TR_VPStringCached *vcall);
   void transformStringCtors(TR_VPTreeTopPair *vcall2);
   void transformUnknownTypeArrayCopy(TR_TreeTopWrtBarFlag *);
   void transformReferenceArrayCopy(TR_TreeTopWrtBarFlag *);
   void transformReferenceArrayCopyWithoutCreatingStoreTrees(TR_TreeTopWrtBarFlag *arrayTree, TR::SymbolReference *srcObjRef, TR::SymbolReference *dstObjRef, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, TR::SymbolReference *lenRef);
#ifdef J9_PROJECT_SPECIFIC
   void transformConverterCall(TR::TreeTop *);
#endif

   struct ObjCloneInfo {
      TR_ALLOC(TR_Memory::ValuePropagation)

      TR_OpaqueClassBlock *_clazz;
      bool _isFixed;
      ObjCloneInfo(TR_OpaqueClassBlock *clazz, bool isFixed)
         : _clazz(clazz), _isFixed(isFixed)  { }
   };

   void transformObjectCloneCall(TR::TreeTop *, ObjCloneInfo *cloneInfo);
   void transformArrayCloneCall(TR::TreeTop *, TR_OpaqueClassBlock *j9class);


   TR::TreeTop *createPrimitiveArrayNodeWithoutFlags(TR::TreeTop* tree, TR::TreeTop* newTree, TR::SymbolReference* srcRef, TR::SymbolReference* dstRef, TR::SymbolReference * lenRef, bool useFlagsOnOriginalArraycopy, bool isOptimizedReferenceArraycopy);
   TR::TreeTop *createReferenceArrayNodeWithoutFlags(TR::TreeTop* tree, TR::TreeTop* newTree, TR::SymbolReference* srcObjectRef, TR::SymbolReference* dstObjectRef, TR::SymbolReference* lenRef, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, bool useFlagsOnOriginalArraycopy);
#ifdef J9_PROJECT_SPECIFIC
   void generateArrayTranslateNode(TR::TreeTop *vcallTree,TR::TreeTop *arrayTranslateTree, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, TR::SymbolReference *srcOffRef, TR::SymbolReference *dstOffRef, TR::SymbolReference *lenRef,TR::SymbolReference *tableRef,bool hasTable);
#endif
   TR::TreeTop* createConverterCallNodeAfterStores(TR::TreeTop *tree,TR::TreeTop *origTree, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, TR::SymbolReference *lenRef, TR::SymbolReference *srcOffRef, TR::SymbolReference *dstOffRef, TR::SymbolReference *thisRef, TR::SymbolReference *tableRef);
   //realtime support
   void transformRealTimeArrayCopy(TR_RealTimeArrayCopy *rtArrayCopyTree);

#ifdef J9_PROJECT_SPECIFIC
   void transformRTMultiLeafArrayCopy(TR_RealTimeArrayCopy *rtArrayCopyTree);
#endif

   TR::TreeTop *buildSameLeafTest(TR::Node *offset,TR::Node *len,TR::Node *spineShiftNode);
   TR::TreeTop *createArrayCopyVCallNodeAfterStores(TR::TreeTop* tree, TR::SymbolReference* srcObjectRef, TR::SymbolReference* dstObjectRef, TR::SymbolReference* lenRef, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef);
   TR::TreeTop* createSameLeafTestAfterStores(TR::TreeTop *tree, TR::SymbolReference* child1Ref,TR::SymbolReference* child2Ref);
   void generateRTArrayNodeWithoutFlags(TR_RealTimeArrayCopy *rtArrayCopyTree, TR::TreeTop *dupArraycopyTree, TR::SymbolReference *srcRef, TR::SymbolReference *dstRef, TR::SymbolReference *srcOffRef, TR::SymbolReference *dstOffRef, TR::SymbolReference *lenRef, bool primitive);

   int32_t getValueNumber(TR::Node *node);


   void printStructureInfo(TR_Structure *structure, bool starting, bool lastTimeThrough);
   void printParentStructure(TR_Structure *structure);
   void printValueConstraints(ValueConstraints &valueConstraints);
   void printEdgeConstraints(EdgeConstraints *constraints);
   void printGlobalConstraints();

   // routines for removal of constraints
   // when intersection fails for a vn
   // only rip out constraints in a prod build
   bool removeConstraints(int32_t valueNumber, ValueConstraints *vc, bool findStores);
   bool removeConstraints(int32_t valueNumber, ValueConstraints *valueConstraints);
   bool removeConstraints(int32_t valueNumber);
   bool removeStoreConstraints(ValueConstraints *vc, int32_t valueNumber, int32_t relative);
   bool removeConstraints();

   bool intersectionFailed() {return _intersectionFailed;}
   void setIntersectionFailed(bool b) {_intersectionFailed = b;}
   bool                           _intersectionFailed;

   TR_VPConstraint                        **_parmValues;
   ConstraintsHashTableEntry              **_constraintsHashTable;
   GlobalConstraint                       **_globalConstraintsHashTable;
   uint32_t                               _globalConstraintsHTMaxBucketIndex;
   EdgeConstraints                        **_edgeConstraintsHashTable;
   LoopDefsHashTableEntry                 **_loopDefsHashTable;

   TR::Block                       *_curBlock;
   TR::TreeTop                     *_curTree;
   TR::Node                        *_parentNode;
   int32_t                         _numValueNumbers;
   int32_t                         _firstUnresolvedSymbolValueNumber;
   int32_t                         _firstInductionVariableValueNumber;

   ValueConstraints                _curConstraints;
   ValueConstraintHandler          _vcHandler;

   vcount_t                        _visitCount;

   // Induction variable identification
   //
   struct InductionVariable : public TR_Link<InductionVariable>
      {
      InductionVariable(TR::Symbol *sym, TR::Node *entryDef, int32_t incrVN, TR_VPConstraint *incr, TR_ValuePropagation *vp)
         : _symbol(sym), _entryDef(entryDef), _entryConstraint(0), _incrementVN(incrVN),
           _increment(incr)
         {
         _valueNumber = vp->_numValueNumbers++;
         _onlyIncrValid = false;
         }
      TR::Symbol                *_symbol;
      TR::Node                 *_entryDef;
      TR_VPConstraint         *_entryConstraint;
      TR_VPConstraint         *_increment;
      int32_t                  _valueNumber;
      int32_t                  _incrementVN;
      bool                     _invalidEntryInfo;
      bool                     _onlyIncrValid;
      };
   struct LoopInfo : public TR_Link<LoopInfo>
      {
      LoopInfo(TR_RegionStructure *region, LoopInfo *parent)
         : _loop(region), _parent(parent), _entryBlock(NULL),
           _backEdgeConstraints(NULL)
         {}
      TR_RegionStructure             *_loop;
      TR::Block                       *_entryBlock;
      EdgeConstraints                *_backEdgeConstraints;
      TR_LinkHead<InductionVariable>  _inductionVariables;
      TR_BitVector                   *_seenDefs;
      LoopInfo                       *_parent;
      TR_LinkHead<LoopInfo>           _subLoops;
      };


   bool isHighWordZero(TR::Node *node);
   void checkForInductionVariableIncrement(TR::Node *node);
   void checkForInductionVariableLoad(TR::Node *node);
   void collectInductionVariableEntryConstraints();
   void checkBackEdgeCoverage();
   bool checkLoopTestBlock(TR::Symbol *sym);
   void setUpInductionVariables(TR_StructureSubGraphNode *node);
   LoopInfo *_loopInfo;

   // Predicting throw targets and converting throws into gotos
   //
   TR::Node *findThrowInBlock(TR::Block *block, TR::TreeTop *&treeTop);
   TR_ScratchList<TR_Pair<TR::Node, TR::Block> > _predictedThrows;

   List<TR::Node> *getArraylengthNodes() {return &_arraylengthNodes;}

   // Pre-existence handling
   //
   void enablePreexistence() {_usePreexistence = true;}
   bool usePreexistence()    {return _usePreexistence;}
   bool registerPreXClass(TR_VPConstraint *constraint);
   TR_ScratchList<TR_OpaqueClassBlock> _prexClasses;
   TR_ScratchList<TR_ResolvedMethod> _prexMethods;
   TR_ScratchList<TR_OpaqueClassBlock> _prexClassesThatShouldNotBeNewlyExtended;
   TR_ScratchList<TR_PersistentClassInfo> _resetClassesThatShouldNotBeNewlyExtended;

   // Calls that have been devirtualized
   //
   struct CallInfo : public TR_Link<CallInfo>
      {
      CallInfo(TR_ValuePropagation *vp, TR_OpaqueClassBlock *thisType, TR_PrexArgInfo *argInfo)
         : _tt(vp->_curTree), _block(vp->_curBlock), _thisType(thisType), _argInfo(argInfo) { }
      TR::TreeTop          *_tt;
      TR::Block            *_block;
      TR_OpaqueClassBlock *_thisType;
      TR_PrexArgInfo      *_argInfo;
      };



    struct VirtualGuardInfo  : public TR_Link<VirtualGuardInfo>
        {
        VirtualGuardInfo(TR_ValuePropagation *vp, TR_VirtualGuard* vgOld, TR_VirtualGuard* vgNew, TR::Node* newGNode, TR::Node* cn)
        : _currentTree(vp->_curTree), _block(vp->_curBlock), _oldVirtualGuard (vgOld), _newVirtualGuard(vgNew), _newGuardNode(newGNode), _callNode(cn) { }
        TR_VirtualGuard     *_oldVirtualGuard;
        TR_VirtualGuard     *_newVirtualGuard;
        TR::Node             *_newGuardNode;
        TR::Node             *_callNode;
        TR::TreeTop          *_currentTree;
        TR::Block            *_block;
        };

    TR_LinkHead<VirtualGuardInfo> _convertedGuards;
    TR_LinkHead<CallInfo> _devirtualizedCalls;
    TR_LinkHead<CallInfo> _unsafeCallsToInline;

   struct ClassInitInfo : public TR_Link<ClassInitInfo>
      {
      ClassInitInfo(TR_ValuePropagation *vp, char *sig, int32_t len)
         : _tt(vp->_curTree), _block(vp->_curBlock), _sig(sig), _len(len) { }
      TR::TreeTop          *_tt;
      TR::Block            *_block;
      char                *_sig;
      int32_t             _len;
      };
   TR_LinkHead<ClassInitInfo> _classesToCheckInit;


   // Handling of duplicate boolean negations in the same block,
   // i.e.  x = !x; ... x = !x;
   //
   struct BooleanNegationInfo : public TR_Link<BooleanNegationInfo>
      {
      int32_t  _storeValueNumber;
      TR::Node *_loadNode;
      };

   TR_LinkHead<BooleanNegationInfo>   _booleanNegationInfo;

   //Block Versioner
   struct ArrayIndexInfo : public TR_Link<ArrayIndexInfo>
      {
      TR::Node *_baseNode;
      int32_t _min;
      int32_t _max;
      int32_t _delta;
      bool _versionBucket;
      bool _notToVersionBucket;
      TR_ScratchList<TR::Node> *_bndChecks;
      };
   struct ArrayLengthToVersion : public TR_Link<ArrayLengthToVersion>
      {
      TR::Node *_arrayLen;
      TR_OpaqueClassBlock *_instanceOfClass;
      TR_LinkHead<ArrayIndexInfo> *_arrayIndicesInfo;
      };

   struct BlockVersionInfo : public TR_Link<BlockVersionInfo>
      {
      TR::Block *_block;
      TR_LinkHead<ArrayLengthToVersion> *_arrayLengths;
      };

   struct FirstLoadOfNonInvariant : public TR_Link<FirstLoadOfNonInvariant>
      {
      int32_t _symRefNum;
      TR::Node *_node;
      };

   void createNewBlockInfoForVersioning(TR::Block *);
   void versionBlocks();
   void buildBoundCheckComparisonNodes(BlockVersionInfo *, List<TR::Node> *);
   void removeBndChecksFromFastVersion(BlockVersionInfo *);
   TR::Node * findVarOfSimpleForm(TR::Node *);
   TR::Node * findVarOfSimpleFormOld(TR::Node *);
   void createNewBucketForArrayIndex(ArrayLengthToVersion *,TR_LinkHead<ArrayLengthToVersion> *, int32_t , TR::Node *, TR::Node *, TR_OpaqueClassBlock *);
   void collectDefSymRefs(TR::Node *,TR::Node *);
   bool prepareForBlockVersion(TR_LinkHead<ArrayLengthToVersion> *);
   void addToSortedList(TR_LinkHead<ArrayLengthToVersion> *,ArrayLengthToVersion *);
   TR_LinkHead<BlockVersionInfo> *_blocksToBeVersioned;
   TR_LinkHead<FirstLoadOfNonInvariant> *_firstLoads;
   TR_BitVector *_seenDefinedSymbolReferences;
   TR_ScratchList<TR::Node> *_bndChecks;
   bool _enableVersionBlocks;
   bool _disableVersionBlockForThisBlock;
   TR::Block *_startEBB;

   TR_BitVector *_unsafeArrayAccessNodes;

   // Blocks that are unreachable and can be removed.
   //
   TR_Array<TR::CFGNode*> *_blocksToBeRemoved;

   // Edges that are to be removed.
   //
   TR_Array<TR::CFGEdge *> *_edgesToBeRemoved;

   // Cached constraints
   //
   TR_VPNullObject        *_nullObjectConstraint;
   TR_VPNonNullObject     *_nonNullObjectConstraint;
   TR_VPPreexistentObject *_preexistentObjectConstraint;
   TR_VPIntConst          *_constantZeroConstraint;
   TR_VPUnreachablePath   *_unreachablePathConstraint;

 #if (defined(LINUX) && ( defined(TR_TARGET_X86) || defined(TR_TARGET_S390)))
  #if __GNUC__ > 4 || \
   (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
   TR_VP_BCDSign **getBCDSignConstraints(TR::DataTypes dt) __attribute__((optimize(1)));
  #else
   TR_VP_BCDSign **getBCDSignConstraints(TR::DataTypes dt);
  #endif
 #else
   TR_VP_BCDSign **getBCDSignConstraints(TR::DataTypes dt);
 #endif
   TR_UseDefInfo      *_useDefInfo;      // Cached use/def info
   TR_ValueNumberInfo *_valueNumberInfo; // Cached value number info
   CS2::HashTable<uint64_t, TR::list<TR::Node *>*, TR::Allocator> _constNodeInfo;

   // Flags
   //
   bool lastTimeThrough()        {return _lastTimeThrough;}
   bool checksWereRemoved()      {return _checksRemoved;}
   bool enableSimplifier()       {return _enableSimplifier;}
   bool useDefInfoInvalid()      {return _invalidateUseDefInfo;}
   bool valueNumberInfoInvalid() {return _invalidateValueNumberInfo;}
   bool getBestRun()             {return _bestRun;}
   TR::Node *getCurrentParent()   {return _currentParent;}
   bool chTableWasValid()        {return _chTableWasValid;}
   bool chTableValidityChecked() {return _chTableValidityChecked;}
   //

   void setChecksRemoved()           {_checksRemoved = true;}
   void setEnableSimplifier()        {_enableSimplifier = true;}
   void setChTableWasValid(bool b)   {_chTableWasValid = b;}
   void setChTableValidityChecked(bool b)   {_chTableValidityChecked = b;}
   void invalidateUseDefInfo()       {_invalidateUseDefInfo = true;}
   void invalidateValueNumberInfo()  {_invalidateValueNumberInfo = true;}
   void setCurrentParent(TR::Node *n) {_currentParent = n;}
   void getParmValues();
   bool isParmInvariant(TR::Symbol *sym);
   bool computeDivRangeWhenDivisorCanBeZero(TR::Node *node)
      {
      if (getCurrentParent()->getOpCodeValue() == TR::DIVCHK)
         return true;
      return false;
      }


   bool      _isGlobalPropagation;
   bool      _lastTimeThrough;
   bool      _invalidateUseDefInfo;
   bool      _invalidateValueNumberInfo;
   bool      _enableSimplifier;
   bool      _checksRemoved;
   bool      _chTableWasValid;
   bool      _chTableValidityChecked;
   bool      _usePreexistence;
   bool      _reachedMaxRelationDepth;
   bool      _bestRun;
   TR_YesNoMaybe      _changedThis;
   int32_t   _propagationDepth;
   int32_t   _maxPropagationDepth;


   TR::Node    *_currentParent;
   List<TR::TreeTop> _objectCloneCallTreeTops;
   List<TR::Node> _objectCloneCallNodes;
   List<TR_OpaqueClassBlock> _objectCloneInstanceClasses;
   List<TR::Node> _arraylengthNodes;
   List<TR::Node> _javaLangClassGetComponentTypeCalls;
   List<TR_TreeTopWrtBarFlag> _unknownTypeArrayCopyTrees;
   List<TR_TreeTopWrtBarFlag> _referenceArrayCopyTrees;
   List<TR_RealTimeArrayCopy> _needRunTimeCheckArrayCopy;
   List<TR_RealTimeArrayCopy> _needMultiLeafArrayCopy;
   List<TR_ArrayCopySpineCheck> _arrayCopySpineCheck;
   List<TR::TreeTop> _multiLeafCallsToInline;
   List<TR_TreeTopNodePair> _scalarizedArrayCopies;
   List<TR_VPStringCached> _cachedStringBufferVcalls;
   List<TR_VPTreeTopPair> _cachedStringPeepHolesVcalls;
   List<TR::TreeTop> _converterCalls;
   List<TR::TreeTop> _objectCloneCalls;
   List<TR::TreeTop> _arrayCloneCalls;
   List<ObjCloneInfo> _objectCloneTypes;
   List<TR_OpaqueClassBlock> _arrayCloneTypes;

   int32_t    *_parmInfo;
   bool       *_parmTypeValid;

   private:
   TR_VP_BCDSign **_bcdSignConstraints;
   };

class TR_GlobalValuePropagation : public TR_ValuePropagation
   {
   public:

   TR_GlobalValuePropagation(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_GlobalValuePropagation(manager);
      }

   virtual int32_t perform();

   private:

   void determineConstraints();
   void processStructure(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processAcyclicRegion(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processNaturalLoop(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processImproperLoop(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processRegionSubgraph(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop, bool isNaturalLoop);
   void processRegionNode(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processBlock(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void getImproperRegionStores(TR_StructureSubGraphNode *node, ValueConstraints &stores);
   bool buildInputConstraints(TR::CFGNode *node);
   void propagateOutputConstraints(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool isNaturalLoop, List<TR::CFGEdge> &outEdges1, List<TR::CFGEdge> *outEdges2);
   // Blocks not included in this set will be skipped for speed.
   // NULL bitvector means the info is unavailable, and all blocks should be processed.
   //
   TR_BitVector *_blocksToProcess;

   };

/*
 * Class TR_LocalValuePropagation
 * ==============================
 *
 * Constant, type and relational propagation within a block. Propagates constants 
 * and types based on assignments and performs removal of subsumed checks and 
 * compares. Also performs devirtualization of calls, checkcast and array store 
 * check elimination.  Does not do value numbering, instead works on the 
 * assumption that every expression has a unique value number (improved 
 * effectiveness if expressions having same value are commoned by a prior 
 * local CSE pass).  
 */

class TR_LocalValuePropagation : public TR_ValuePropagation
   {
   public:

   TR_LocalValuePropagation(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LocalValuePropagation(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual void postPerformOnBlocks();

   private:

   TR::TreeTop *processBlock(TR::TreeTop *start);
   };

TR::Node *generateArrayletAddressTree(TR::Compilation* comp, TR::Node *vcallNode, TR::DataTypes type, TR::Node *off,TR::Node *obj, TR::Node *spineShiftNode,TR::Node *shiftNode,TR::Node *strideShiftNode, TR::Node *hdrSize);
TR::Node *generateArrayAddressTree(TR::Compilation* comp, TR::Node *node, int32_t offHigh, TR::Node *offNode, TR::Node *objNode, int32_t elementSize, TR::Node * &stride, TR::Node *hdrSize);
TR::Node * createHdrSizeNode(TR::Compilation *comp, TR::Node *n);

void constrainRangeByPrecision(const int64_t low, const int64_t high, const int32_t precision, int64_t &lowResult, int64_t &highResult, bool isNonNegative = false);


#endif
