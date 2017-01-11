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

#ifndef OMR_NODE_INCL
#define OMR_NODE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_NODE_CONNECTOR
#define OMR_NODE_CONNECTOR
namespace OMR { class Node; }
namespace OMR { typedef OMR::Node NodeConnector; }
#endif

#include <limits.h>                       // for UINT_MAX, USHRT_MAX
#include <stddef.h>                       // for size_t
#include <stdint.h>                       // for uint16_t, int32_t, int64_t
#include <string.h>                       // for memset
#include "codegen/RegisterConstants.hpp"  // for TR_GlobalRegisterNumber
#include "cs2/hashtab.h"                  // for HashTable
#include "env/TRMemory.hpp"               // for TR_ArenaAllocator
#include "il/DataTypes.hpp"               // for DataTypes, etc
#include "il/ILOpCodes.hpp"               // for ILOpCodes
#include "il/ILOps.hpp"                   // for ILOpCode
#include "il/NodeUnions.hpp"              // for UnionedWithChildren
#include "infra/Annotations.hpp"          // for OMR_EXTENSIBLE
#include "infra/Assert.hpp"               // for TR_ASSERT
#include "infra/Flags.hpp"                // for flags32_t

class TR_BitVector;
class TR_Debug;
class TR_DebugExt;
class TR_NodeKillAliasSetInterface;
class TR_NodeUseAliasSetInterface;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_ResolvedMethod;
namespace TR { class AutomaticSymbol; }
namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class NodePool; }
namespace TR { class Register; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
namespace TR { class NodeExtension; }
namespace TR { template <class T> class list; }
template <class T> class List;

#define NUM_DEFAULT_CHILDREN    2

/// Node counts
///
typedef uint32_t    ncount_t;
#define MAX_NODE_COUNT   UINT_MAX

/// Node reference counts
///
typedef uint32_t    rcount_t;
#define MAX_RCOUNT  UINT_MAX

/// Node local indexes
///
typedef uint32_t    scount_t;
#define MAX_SCOUNT  UINT_MAX
#define SCOUNT_HIGH_BIT          0x80000000
#define NULL_USEDEF_SYMBOL_INDEX 0xFFFF  //TODO: should be 0xFFFF until we change the date type of _localIndex in Symbol.hpp

/// Visit counts
///
/// \def MAX_VCOUNT:       max # visit counts that can be consumed before
///                        risking incorrect behaviour (ie. hard limit)
/// \def HIGH_VISIT_COUNT: recommended max # visit counts between reset operations (ie. soft limit)
/// \def VCOUNT_HEADROOM:  max # visit counts that can be safely consumed
///                        between HIGH_VISIT_COUNT checks
///
#define MAX_VCOUNT      (USHRT_MAX)
#define VCOUNT_HEADROOM (48000)
#define HIGH_VISIT_COUNT (MAX_VCOUNT-VCOUNT_HEADROOM)
typedef uint16_t   vcount_t;

#define TR_MAX_CHARS_FOR_HASH 32
#define TR_DECIMAL_HASH 7

typedef enum
   {
   NoPrefetch = 0,
   PrefetchLoad = 1,
   PrefetchLoadL1 = 2,
   PrefetchLoadL2 = 3,
   PrefetchLoadL3 = 4,
   PrefetchLoadNonTemporal = 5,
   PrefetchStore = 101,
   PrefetchStoreConditional = 102,
   PrefetchStoreNonTemporal = 103,
   ReleaseStore = 501,  ///< Retain for Load
   ReleaseAll = 502     ///< Release from cache
} PrefetchType;

namespace OMR
{

class OMR_EXTENSIBLE Node
   {
// Forward declarations
public:
   class ChildIterator;
protected:
   struct OptAttributes;

private:

   /// This operator is declared private and not actually defined;
   /// this is a C++ convention to avoid accidental assignment between instances of this class
   /// Once we have universal C++11 support, should be changed to use "= delete"
   Node & operator=(const TR::Node &);

/**
 * Protected constructors and helpers
 */
protected:

   Node(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, OptAttributes * optAttributes = NULL);

   Node(TR::Node *from, uint16_t numChildren = 0);

   static void copyValidProperties(TR::Node *fromNode, TR::Node *toNode);

   static TR::Node *createInternal(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *originalNode = 0);

/**
 * Public constructors and helpers
 */
public:

   inline TR::Node * self();

   Node();
   ~Node();

   void * operator new(size_t s, TR::NodePool & nodePool, ncount_t poolIndex = 0);

   static TR::Node *copy(TR::Node *);
   static TR::Node *copy(TR::Node *, int32_t numChildren);

   static TR::Node *recreate(TR::Node *originalNode, TR::ILOpCodes op);
   static TR::Node *recreateWithSymRef(TR::Node *originalNode, TR::ILOpCodes op, TR::SymbolReference *newSymRef);

   // create methods from everywhere other than the ilGenerator need
   // to pass in a TR::Node pointer from which the byte code information will be copied
   //
   static TR::Node *create(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren = 0);
   static TR::Node *create(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node* first);
   static TR::Node *create(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node* first, TR::Node* second);
   static TR::Node *create(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::TreeTop * dest);
   static TR::Node *create(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, int32_t intValue, TR::TreeTop * dest = 0);
   static TR::Node *createWithSymRef(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::SymbolReference * symRef);
   static TR::Node *createWithSymRef(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node* first, TR::SymbolReference * symRef);
   static TR::Node *createOnStack(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren = 0);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren = 0, TR::SymbolReference * symRef = 0);

   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren = 0);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::TreeTop * dest);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, int32_t intValue, TR::TreeTop * dest = 0);
   static TR::Node *createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, uintptr_t extraChildren, TR::SymbolReference *symRef);
   static TR::Node *createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, uintptr_t extraChildren, TR::SymbolReference *symRef);
   static TR::Node *createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::SymbolReference * symRef);
   static TR::Node *createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::SymbolReference * symRef, uintptr_t extraChildrenForFixup);

#if defined(_MSC_VER) || defined(LINUXPPC)
private:
   static TR::Node *recreateWithoutSymRef_va_args(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, va_list &args);
   static TR::Node *createWithoutSymRef(TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, ...);
   static TR::Node *recreateWithoutSymRef(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, ...);
   static TR::Node *recreateWithSymRefWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, ...);

public:
   // only this variadic method is part of the external interface
   static TR::Node *createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, ...);

#else // XLC (but not on LINUX PPC) and GCC support the C++11 feature, variadic templates
private:
   uint16_t addChildrenAndSymRef(uint16_t lastIndex, TR::SymbolReference *symRef);
   uint16_t addChildrenAndSymRef(uint16_t childIndex, TR::Node *child);

   template <class...ChildrenAndSymRefType>
   uint16_t addChildrenAndSymRef(uint16_t childIndex, TR::Node *child, ChildrenAndSymRefType... childrenAndSymRef);

   template <class...ChildrenAndSymRefType>
   static TR::Node *recreateWithSymRefWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, TR::Node *first, ChildrenAndSymRefType... childrenAndSymRef);

   template <class...Children>
   static TR::Node *recreateWithoutSymRef(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, TR::Node *first, Children... children);

   template <class...ChildrenAndSymRefType>
   static TR::Node *createWithSymRefInternal(TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, TR::Node *first, ChildrenAndSymRefType... childrenAndSymRef);

   template <class...Children>
   static TR::Node *createWithoutSymRef(TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, TR::Node *first, Children... children);
public:

   // only this variadic method is part of the external interface
   template <class...ChildrenAndSymRefType>
   static TR::Node *createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, TR::Node *first, ChildrenAndSymRefType... childrenAndSymRef);
#endif

   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh);
   static TR::Node *create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::Node *eighth);

   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::Node *eighth);

   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::SymbolReference * symRef);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::SymbolReference * symRef);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::SymbolReference * symRef);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::SymbolReference * symRef);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::SymbolReference * symRef);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::SymbolReference * symRef);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::SymbolReference * symRef);
   static TR::Node *recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::Node *eighth, TR::SymbolReference * symRef);

   static TR::Node *createWithRoomForOneMore(TR::ILOpCodes op, uint16_t numChildren, void * symbolRefOrBranchTarget = 0, TR::Node *first = 0, TR::Node *second = 0, TR::Node *third = 0, TR::Node *fourth = 0, TR::Node *fifth = 0);
   static TR::Node *createWithRoomForThree(TR::ILOpCodes op, TR::Node *first, TR::Node *second, void * symbolRefOrBranchTarget = 0);
   static TR::Node *createWithRoomForFive(TR::ILOpCodes op, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, void * symbolRefOrBranchTarget = 0);

   static TR::Node *createif(TR::ILOpCodes op, TR::Node *first, TR::Node *second, TR::TreeTop * branchTarget = 0);
   static TR::Node *createbranch(TR::ILOpCodes op, TR::Node * first, TR::TreeTop * branchTarget = 0);
   static TR::Node *createCase(TR::Node *originatingByteCodeNode, TR::TreeTop *, CASECONST_TYPE = 0);

   static TR::Node *createArrayOperation(TR::ILOpCodes arrayOp, TR::Node *first, TR::Node *second, TR::Node * third, TR::Node *fourth = NULL, TR::Node *fifth = NULL);
   static TR::Node *createArraycopy();
   static TR::Node *createArraycopy(TR::Node *first, TR::Node *second, TR::Node *third);
   static TR::Node *createArraycopy(TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth);

   static TR::Node *createLoad(TR::SymbolReference * symRef);
   static TR::Node *createLoad(TR::Node *originatingByteCodeNode, TR::SymbolReference *);

   static TR::Node *createStore(TR::SymbolReference * symRef, TR::Node * value);
   static TR::Node *createStore(TR::SymbolReference * symRef, TR::Node * value, TR::ILOpCodes op);
   static TR::Node *createStore(TR::SymbolReference * symRef, TR::Node * value, TR::ILOpCodes op, size_t size);
   static TR::Node *createStore(TR::Node *originatingByteCodeNode, TR::SymbolReference * symRef, TR::Node * value);

   static TR::Node *createRelative32BitFenceNode(void * relocationAddress);
   static TR::Node *createRelative32BitFenceNode(TR::Node *originatingByteCodeNode, void *);

   static TR::Node *createAddressNode(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uintptrj_t address);
   static TR::Node *createAddressNode(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uintptrj_t address, uint8_t precision);

   static TR::Node *createAllocationFence(TR::Node *originatingByteCodeNode, TR::Node *fenceNode);

   static TR::Node *bconst(TR::Node *originatingByteCodeNode, int8_t val);
   static TR::Node *bconst(int8_t val);

   static TR::Node *buconst(TR::Node *originatingByteCodeNode, uint8_t val);
   static TR::Node *buconst(uint8_t val);

   static TR::Node *sconst(TR::Node *originatingByteCodeNode, int16_t val);
   static TR::Node *sconst(int16_t val);

   static TR::Node *cconst(TR::Node *originatingByteCodeNode, uint16_t val);
   static TR::Node *cconst(uint16_t val);

   static TR::Node *iconst(TR::Node *originatingByteCodeNode, int32_t val);
   static TR::Node *iconst(int32_t val);

   static TR::Node *iuconst(TR::Node *originatingByteCodeNode, uint32_t val);
   static TR::Node *iuconst(uint32_t val);

   static TR::Node *lconst(TR::Node *originatingByteCodeNode, int64_t val);
   static TR::Node *lconst(int64_t val);

   static TR::Node *luconst(TR::Node *originatingByteCodeNode, uint64_t val);
   static TR::Node *luconst(uint64_t val);

   static TR::Node *aconst(TR::Node *originatingByteCodeNode, uintptrj_t val);
   static TR::Node *aconst(TR::Node *originatingByteCodeNode, uintptrj_t val, uint8_t precision);
   static TR::Node *aconst(uintptrj_t val);

   static TR::Node *createConstZeroValue(TR::Node *originatingByteCodeNode, TR::DataType dt);
   static TR::Node *createConstOne(TR::Node *originatingByteCodeNode, TR::DataType dt);
   static TR::Node *createConstDead(TR::Node *originatingByteCodeNode, TR::DataType dt, intptrj_t extraData=0);

   static TR::Node *createCompressedRefsAnchor(TR::Node *firstChild);

   static TR::Node *createAddConstantToAddress(TR::Node * addr, intptr_t value, TR::Node * parent = NULL);
   static TR::Node *createLiteralPoolAddress(TR::Node *node, size_t offset);

   static TR::Node *createVectorConst(TR::Node *originatingByteCodeNode, TR::DataType dt);
   static TR::Node *createVectorConversion(TR::Node *src, TR::DataType trgType);

/**
 * Private constructor helpers
 */
private:

   void copyChildren(TR::Node *from, uint16_t numChildren = 0, bool forNodeExtensionOnly = false);

   static TR::Node *recreateAndCopyValidPropertiesImpl(TR::Node *originalNode, TR::ILOpCodes op, TR::SymbolReference *newSymRef);

   static bool isLegalCallToCreate(TR::ILOpCodes opvalue)
      {
      TR::ILOpCode opcode; opcode.setOpCodeValue(opvalue);
      TR_ASSERT(!opcode.isIf(), "use createif or createbranch on this node\n");
      TR_ASSERT(opvalue != TR::arraycopy, "use createArraycopy to create this node");
      TR_ASSERT(opvalue != TR::v2v, "use createVectorConversion to create node: %s", opcode.getName());
      TR_ASSERT(opvalue != TR::vconst, "use createVectorConst to create node: %s", opcode.getName());
      return true;
      }


/**
 * Public functions
 */
public:

   /** @return true if n is one of the children of this node, return false otherwise */
   bool                   hasChild(TR::Node *searchNode);

   void                   addChildren(TR::Node ** extraChildren, uint16_t num);

   TR::Node *             setValueChild(TR::Node *child);
   TR::Node *             setAndIncChild(int32_t c, TR::Node * p);
   TR::Node *             setAndIncValueChild(TR::Node *child);
   TR::Node *             getValueChild();
   TR::Node *             getAndDecChild(int32_t c);

   TR::Node *             duplicateTree(bool duplicateChildren = true);
   TR::Node *             duplicateTreeWithCommoning(TR::Allocator allocator);
   TR::Node *             duplicateTree_DEPRECATED(bool duplicateChildren = true);
   bool                   isUnsafeToDuplicateAndExecuteAgain(int32_t *nodeVisitBudget);

   /// uncommonChild changes
   ///     parent
   ///        =>this
   /// to:
   ///     parent
   ///        clone(this)
   ///           ...
   ///
   void                   uncommonChild(int32_t childIndex);

   /// creates clone, and adjusts reference counts of clone, node and its children
   /// as though the node had been uncommoned from its parent.
   ///
   /// returns the uncommoned clone.
   ///
   TR::Node *             uncommon();

   bool                   containsNode(TR::Node *searchNode, vcount_t visitCount); // Careful how you use this: it doesn't account for aliasing
   // these call containsNode on each node found in the list or bitvector set of Nodes
   bool                   containsAnyNode(List<TR::Node *> &nodeList, vcount_t visitCount);
   bool                   containsAnyNode(TR::SparseBitVector &nodeSet, vcount_t visitCount, TR::Compilation *comp);

   /// Does this node have an unresolved symbol reference?
   bool                   hasUnresolvedSymbolReference();

   /// Does this node have a volatile symbol reference?
   bool                   mightHaveVolatileSymbolReference();

   /// Is this node the 'this' pointer?
   bool                   isThisPointer();

   /// Whether this node is high part of a "dual", in DAG representation a dual
   /// is a composite operator, made from a high order part and its adjunct operator
   /// which is its third child. It returns true if the node has the form:
   ///
   ///     highOp
   ///       firstChild
   ///       secondChild
   ///       adjunctOp
   ///         pairFirstChild
   ///         pairSecondChild
   ///
   /// and the opcodes for highOp/adjunctOp are lumulh/lmul, luaddh/luadd, or lusubh/lusub.
   ///
   bool                   isDualHigh();

   /// Whether this node is high part of a ternary subtract or addition, like a dual this
   /// is a composite operator, made from a high order part and its adjunct operator
   /// which is the first child of its third child. It returns true if the node has the form:
   ///
   ///     highOp
   ///       firstChild
   ///       secondChild
   ///       computeCC
   ///         adjunctOp
   ///           pairFirstChild
   ///           pairSecondChild
   ///
   /// and the opcodes for highOp/adjunctOp are luaddc/luadd, or lusubb/lusub.
   ///
   bool                   isTernaryHigh();

   /// Whether this node is the high or low part of a "dual", in cyclic representation.
   /// ie it represents a composite operator, together with its pair.
   /// The node and its pair have each other as its third child, completing the cycle.
   /// It returns true if the node has the form:
   ///
   ///     node
   ///       firstChild
   ///       secondChild
   ///       pair
   ///         pairFirstChild
   ///         pairSecondChild
   ///         ==> node
   ///
   bool                   isDualCyclic();

   bool                   isConstZeroBytes();
   bool                   isConstZeroValue();

   bool                   safeToDoRecursiveDecrement();

   /// Is this node a GC safe point
   bool                   canGCandReturn();
   bool                   canGCandExcept();
   bool                   canCauseGC();
   bool                   isGCSafePointWithSymRef();

   bool                   dontEliminateStores(bool isForLocalDeadStore = false);

   bool                   isNotCollected();

   bool                   addressPointsAtObject();

   /// Decide whether it is safe to replace the next reference to this node with
   /// a copy of the node, i.e. make sure it is not killed between the first
   /// reference and the next reference.
   /// The treetop containing the first reference is provided by the caller.
   bool                   isSafeToReplaceNode(TR::TreeTop *curTreeTop);

   bool                   isl2aForCompressedArrayletLeafLoad();

   /// Returns true if the node kills the symbol reference
   bool                   mayModifyValue(TR::SymbolReference *);

   bool                   performsVolatileAccess(vcount_t visitCount);

   bool                   uses64BitGPRs();

   bool                   isRematerializable(TR::Node *parent, bool onlyConsiderOpCode);

   bool                   canEvaluate();

   bool                   isDoNotPropagateNode();
   bool                   containsDoNotPropagateNode(vcount_t vc);

   bool                   anchorConstChildren();

   bool                   isFloatToFixedConversion();

   bool                   isZeroExtension();

   bool                   isPureCall();

   bool                   isClassUnloadingConst();

   // A common query used by the optimizer
   inline bool            isSingleRef();

   // A common query used by the code generators
   inline bool            isSingleRefUnevaluated();

   TR_YesNoMaybe          hasBeenRun();

   /// Given a monenter node, return the persistent class identifer that's being synchronized
   TR_OpaqueClassBlock *  getMonitorClass(TR_ResolvedMethod *);
   TR_OpaqueClassBlock *  getMonitorClassInNode();
   void                   setMonitorClassInNode(TR_OpaqueClassBlock *);

   // Given that this is a NULLCHK node, find the reference that is being checked.
   TR::Node *             getNullCheckReference();
   void                   setNullCheckReference(TR::Node *refNode);

   /// getFirstArgumentIndex returns the child index where the arguments start for a call node.
   /// For an indirect call the first child may be the vft.
   int32_t                getFirstArgumentIndex();
   inline int32_t         getNumArguments();
   inline TR::Node *      getArgument(int32_t index);
   inline TR::Node *      getFirstArgument();

   TR::Node *             getReturnCode(bool isReason=false);
   TR::Node *             getReturnReason();

   uint32_t               getSize();
   uint32_t               getRoundedSize();

   uint32_t               getNumberOfSlots();

   int32_t                getMaxIntegerPrecision();

   uint16_t               getCaseIndexUpperBound();

   /// Find the store node (if any) represented by this node or its child
   TR::Node *             getStoreNode();

   TR::TreeTop *          getVirtualCallTreeForGuard();
   TR::Node *             getVirtualCallNodeForGuard();

   TR_OpaqueMethodBlock * getOwningMethod();

   inline TR::DataType    getType();

   // TODO: Sink into J9. Depends on OMR::Compilation::getMethodFromNode
   void *                 getAOTMethod();

   /**
    * @return the signature of the node's type if applicable.
    * @note the signature's storage may have been created on the stack!
    */
   const char *           getTypeSignature(int32_t &, TR_AllocationKind = stackAlloc);

   // 'Value' can change as a result of modification
   void                   notifyChangeToValueOfNode();

   void                   reverseBranch(TR::TreeTop *newDest);

   void                   devirtualizeCall(TR::TreeTop *treeTop);

   bool                   nodeMightKillCondCode();
   void                   gatherAllNodesWhichMightKillCondCode(vcount_t vc, TR::list<TR::Node *> &nodesWhichKillCondCode);

   TR::TreeTop *          extractTheNullCheck(TR::TreeTop *);

   int32_t                countNumberOfNodesInSubtree(vcount_t);

   /// Which exceptions can this node cause to be thrown?
   uint32_t               exceptionsRaised();

   /// Has the expression represented by this node ever been executed?  Could be
   /// determined from such evidence as profile-directed feedback for a static
   /// compiler, or runtime instrumentation for a dynamic compiler.
   ///
   TR::Node *             skipConversions();

   TR::Node *             createLongIfNeeded();

   TR::TreeTop *          createStoresForVar(TR::SymbolReference * &nodeRef, TR::TreeTop *insertBefore, bool simpleRef = false);

   void                   printFullSubtree();

   const char *           getName(TR_Debug *);

   bool                   isReferenceArrayCopy();
   bool                   chkReferenceArrayCopy();
   const char *           printIsReferenceArrayCopy();

   // CS2 Alias Interface:
   // implemented in base/AliasSetInterface.hpp right now.
   inline                 TR_NodeUseAliasSetInterface mayUse();
   inline                 TR_NodeKillAliasSetInterface mayKill(bool gcSafe = false);

   enum signExtensionFlagNames
      {
                           ///< isUnneededConversion,
      unsignedLoad,        ///< = isUnneededConversion,
      force64BitReg,       ///< Flag used by 390 loads to place result into 64 bit register
      couldSkipExtension,  ///< Flag used by 390 loads, when top bits might be ignored. i.e. load<24>(ICM)+a2b
      numSignExtensionFlags
      };

   bool couldIgnoreExtend();
   void setCouldIgnoreExtend(bool b);
   const char * printCouldIgnoreExtend();

   bool force64BitLoad();
   void setForce64BitLoad(bool b);
   const char * printForce64BitLoad();

   bool isUnsignedLoad();
   void setIsUnsignedLoad(bool b);
   const char * printIsUnsignedLoad();

   /**
    * Node field functions
    */

   TR::ILOpCode&        getOpCode()      { return _opCode; }
   TR::ILOpCodes        getOpCodeValue() { return _opCode.getOpCodeValue(); }

   uint16_t             getNumChildren()             { return _numChildren; }
   uint16_t             setNumChildren(uint16_t num) { return ( _numChildren = num); }

   ncount_t             getGlobalIndex()           { return _globalIndex; }
   void                 setGlobalIndex(ncount_t i) { _globalIndex = i; }

   flags32_t            getFlags() { return _flags; }
   void                 setFlags(flags32_t f);

   TR_ByteCodeInfo&     getByteCodeInfo() { return _byteCodeInfo; }
   void                 setByteCodeInfo(const TR_ByteCodeInfo &bcInfo);
   void                 copyByteCodeInfo(TR::Node * from);

   uint32_t             getByteCodeIndex();
   void                 setByteCodeIndex(uint32_t);

   int16_t              getInlinedSiteIndex();
   void                 setInlinedSiteIndex(int16_t);

   TR::Node*            setChild(int32_t c, TR::Node * p);
   inline TR::Node *    getChild(int32_t c);

   TR::Node*            setFirst(TR::Node * p);
   TR::Node*            setSecond(TR::Node * p);
   inline TR::Node *    getFirstChild();
   inline TR::Node *    getSecondChild();
   inline TR::Node *    getThirdChild();
   inline TR::Node *    getLastChild();

   void                 swapChildren();
   TR::Node *           removeChild(int32_t);
   TR::Node *           removeLastChild();
   void                 removeAllChildren();
   void                 rotateChildren(int32_t first, int32_t last); ///< @note when finsihed, last child ends up where first child used to be

   TR::Node *           findChild(TR::ILOpCodes opcode, bool isReversed = false);
   int32_t              findChildIndex(TR::Node * child);
   int32_t              countChildren(TR::ILOpCodes opcode);

   /// Return an iterator for the children of this node.
   inline ChildIterator childIterator(int32_t startIndex=0);

   /**
    * Node field functions end
    */

   /**
    * OptAttributes functions
    */

   bool            hasOptAttributes()  { return _optAttributes != NULL; }

   inline vcount_t getVisitCount();
   inline vcount_t setVisitCount(vcount_t vc);
   inline vcount_t incVisitCount();

   void            resetVisitCounts(vcount_t t); ///< reset visit counts on this node and all its children

   inline rcount_t getReferenceCount();
   inline rcount_t setReferenceCount(rcount_t rc);
   inline rcount_t incReferenceCount();
   inline rcount_t decReferenceCount();

   // Decrement the reference count and if it reaches zero decrement the counts of all the children
   rcount_t        recursivelyDecReferenceCount();
   void            recursivelyDecReferenceCountFromCodeGen();

   inline scount_t getLocalIndex();
   inline scount_t setLocalIndex(scount_t li);
   inline scount_t incLocalIndex();
   inline scount_t decLocalIndex();

   inline scount_t getFutureUseCount();
   inline scount_t setFutureUseCount(scount_t li);
   inline scount_t incFutureUseCount();
   inline scount_t decFutureUseCount();

   void            initializeFutureUseCounts(vcount_t visitCount);

   void            setIsNotRematerializeable();
   bool            isRematerializeable();

   inline uint16_t getUseDefIndex();
   inline uint16_t setUseDefIndex(uint16_t udi);

   TR::Register *  getRegister();
   TR::Register *  setRegister(TR::Register *reg);
   void *          unsetRegister();

   int32_t         getEvaluationPriority(TR::CodeGenerator *codeGen);
   int32_t         setEvaluationPriority(int32_t p);

   inline ncount_t getNodePoolIndex();
   inline void     setNodePoolIndex(ncount_t i);

   /**
    * OptAttributes functions end
    */

   /**
    * UnionBase functions
    */

   // These three methods should be used only if you're sure you can't use one of the other ones.
   inline int64_t          getConstValue();
   inline uint64_t         getUnsignedConstValue();
   inline void             setConstValue(int64_t val);

   inline int64_t          getLongInt();
   inline int64_t          setLongInt(int64_t li);
   inline int32_t          getLongIntLow();
   inline int32_t          getLongIntHigh();

   inline uint64_t         getUnsignedLongInt();
   inline uint64_t         setUnsignedLongInt(uint64_t uli);
   inline uint32_t         getUnsignedLongIntLow();
   inline uint32_t         getUnsignedLongIntHigh();

   inline int32_t          getInt();
   inline int32_t          setInt(int32_t i);

   inline uint32_t         getUnsignedInt();
   inline uint32_t         setUnsignedInt(uint32_t ui);

   inline int16_t          getShortInt();
   inline int16_t          setShortInt(int16_t si);

   inline uint16_t         getUnsignedShortInt();
   inline uint16_t         setUnsignedShortInt(uint16_t c);

   inline int8_t           getByte();
   inline int8_t           setByte(int8_t b);

   inline uint8_t          getUnsignedByte();
   inline uint8_t          setUnsignedByte(uint8_t b);

   inline float            getFloat();
   inline float            setFloat(float f);

   inline uint32_t         setFloatBits(uint32_t f);
   inline uint32_t         getFloatBits();

   inline double           getDouble();
   inline double           setDouble(double d);

   inline uint64_t         getDoubleBits();
   inline uint64_t         setDoubleBits(uint64_t d);

   inline uint64_t         getAddress();
   inline uint64_t         setAddress(uint64_t a);

   template <typename T> inline T getConst();
   template <typename T> inline T setConst(T t);

   template <class T> inline T getIntegerNodeValue();

   bool                    canGet32bitIntegralValue();
   int32_t                 get32bitIntegralValue();

   bool                    canGet64bitIntegralValue();
   int64_t                 get64bitIntegralValue();
   void                    set64bitIntegralValue(int64_t);
   uint64_t                get64bitIntegralValueAsUnsigned();

   TR::Node *              getAllocation();
   TR::Node *              setAllocation(TR::Node * p);

   /*
    * This function is public to support the TR::UnionedWithChildren::*
    * functions that use it extensively. At the moment however, the extension
    * interface is limited to Node subclasses, so this function ends up
    * being public.
    */
   void                    freeExtensionIfExists();

   TR::DataType           getArrayCopyElementType();
   void                    setArrayCopyElementType(TR::DataType type);

   TR_OpaqueClassBlock *   getArrayStoreClassInNode();
   void                    setArrayStoreClassInNode(TR_OpaqueClassBlock *o);

   TR_OpaqueClassBlock *   getArrayComponentClassInNode();
   void                    setArrayComponentClassInNode(TR_OpaqueClassBlock *c);

   struct ArrayStoreCheckInfo
     {
     TR_OpaqueClassBlock *objectClass;
     TR_OpaqueClassBlock *arrayComponentClass;
     };

   bool                    hasArrayStoreCheckInfo();
   void                    createArrayStoreCheckInfo();

   TR_OpaqueMethodBlock *  getMethod();
   void                    setMethod(TR_OpaqueMethodBlock *method);

   // Get/set the label symbol associated with a BBStart node
   TR::LabelSymbol *       getLabel();
   TR::LabelSymbol *       setLabel(TR::LabelSymbol *lab);

   TR_GlobalRegisterNumber getGlobalRegisterNumber();
   TR_GlobalRegisterNumber setGlobalRegisterNumber(TR_GlobalRegisterNumber i);

   TR_GlobalRegisterNumber getLowGlobalRegisterNumber();
   TR_GlobalRegisterNumber setLowGlobalRegisterNumber(TR_GlobalRegisterNumber i);

   TR_GlobalRegisterNumber getHighGlobalRegisterNumber();
   TR_GlobalRegisterNumber setHighGlobalRegisterNumber(TR_GlobalRegisterNumber i);

   size_t                  getLiteralPoolOffset();
   size_t                  setLiteralPoolOffset(size_t offset, size_t size);

   void *                  getRelocationDestination(uint32_t n);
   void *                  setRelocationDestination(uint32_t n, void * p);

   uint32_t                getRelocationType();
   uint32_t                setRelocationType(uint32_t r);

   uint32_t                getNumRelocations();
   uint32_t                setNumRelocations(uint32_t n);

   CASECONST_TYPE          getCaseConstant();
   CASECONST_TYPE          setCaseConstant(CASECONST_TYPE c);

   void *                  getMonitorInfo();
   void *                  setMonitorInfo(void *info);

   TR::ILOpCodes           getOverflowCheckOperation();
   TR::ILOpCodes           setOverflowCheckOperation(TR::ILOpCodes op);
   /**
    * UnionBase functions end
    */

   /**
    * UnionPropertyA functions
    */

   TR::SymbolReference *  getSymbolReference();
   TR::SymbolReference *  setSymbolReference(TR::SymbolReference * p);
   TR::SymbolReference *  getRegLoadStoreSymbolReference();
   TR::SymbolReference *  setRegLoadStoreSymbolReference(TR::SymbolReference * p);
   TR::SymbolReference *  getSymbolReferenceOfAnyType();
   TR::Symbol *           getSymbol();

   inline TR::TreeTop *   getBranchDestination();
   inline TR::TreeTop *   setBranchDestination(TR::TreeTop * p);

   TR::Block *            getBlock(bool ignored = false);
   TR::Block *            setBlock(TR::Block * p);

   int32_t                getArrayStride();
   int32_t                setArrayStride(int32_t s);

   TR::AutomaticSymbol * getPinningArrayPointer();
   TR::AutomaticSymbol * setPinningArrayPointer(TR::AutomaticSymbol *s);

   inline TR::DataType   getDataType();
   inline TR::DataType   setDataType(TR::DataType dt);
   TR::DataType          computeDataType();

   /**
    * UnionPropertyA functions end
    */

   /**
    * Node flag functions
    */

   bool isZero();
   void setIsZero(bool v);
   const char * printIsZero();

   bool isNonZero();
   void setIsNonZero(bool v);
   const char * printIsNonZero();

   bool isNull();
   void setIsNull(bool v);

   bool isNonNull();
   void setIsNonNull(bool v);

   bool pointsToNull();
   void setPointsToNull(bool v);
   bool chkPointsToNull();
   const char * printPointsToNull();

   bool pointsToNonNull();
   void setPointsToNonNull(bool v);
   bool chkPointsToNonNull();
   const char * printPointsToNonNull();

   // Only used during local analysis
   bool containsCall();
   void setContainsCall(bool v);

   // Value is in a global register which cannot be used on an 8 bit instruction
   bool isInvalid8BitGlobalRegister();
   void setIsInvalid8BitGlobalRegister(bool v);
   const char * printIsInvalid8BitGlobalRegister();

   // 390 zGryphon Highword register GRA
   bool getIsHPREligible() { return _flags.testAny(isHPREligible); }
   void setIsHPREligible() { _flags.set(isHPREligible); }
   void resetIsHPREligible() { _flags.reset(isHPREligible); }
   const char * printIsHPREligible();
   bool isEligibleForHighWordOpcode();

   // Result of this node is being stored into the same location as its left child
   bool isDirectMemoryUpdate();
   void setDirectMemoryUpdate(bool v);
   const char * printIsDirectMemoryUpdate();

   // Used prior to codegen phase
   bool isProfilingCode();
   void setIsProfilingCode();
   const char * printIsProfilingCode();

   // Used only during codegen phase
   bool hasBeenVisitedForHints();
   void setHasBeenVisitedForHints(bool v=true);

   bool isNonNegative();
   void setIsNonNegative(bool v);
   const char * printIsNonNegative();

   bool isNonPositive();
   void setIsNonPositive(bool v);
   const char * printIsNonPositive();

   inline bool isNegative();
   inline bool isPositive();
   bool divisionCannotOverflow();
   bool isNonDegenerateArrayCopy();

   // Should not be counted for metrics
   bool isDebug();
   void setIsDebug(bool v);

   // Flag used by arithmetic int/long operations
   bool cannotOverflow();
   void setCannotOverflow(bool v);
   bool chkCannotOverflow();
   const char * printCannotOverflow();

   // Flag used by TR::table nodes
   bool isSafeToSkipTableBoundCheck();
   void setIsSafeToSkipTableBoundCheck(bool b);
   bool chkSafeToSkipTableBoundCheck();
   const char * printIsSafeToSkipTableBoundCheck();

   // Flag used by TR::lstore and TR::lstorei nodes
   bool isNOPLongStore();
   void setNOPLongStore(bool v);
   bool chkNOPLongStore();
   const char * printIsNOPLongStore();

   // Flag for store nodes storing to stack slots
   bool storedValueIsIrrelevant();
   void setStoredValueIsIrrelevant(bool v);
   bool chkStoredValueIsIrrelevant();
   const char * printIsStoredValueIsIrrelevant();

   // Flags used by TR::astore nodes
   bool isHeapificationStore();
   void setHeapificationStore(bool v);
   bool chkHeapificationStore();
   const char * printIsHeapificationStore();

   bool isHeapificationAlloc();
   void setHeapificationAlloc(bool v);
   bool chkHeapificationAlloc();
   const char * printIsHeapificationAlloc();

   bool isLiveMonitorInitStore();
   void setLiveMonitorInitStore(bool v);
   bool chkLiveMonitorInitStore();
   const char * printIsLiveMonitorInitStore();

   // Flag used by TR::iload nodes
   bool useSignExtensionMode();
   void setUseSignExtensionMode(bool b);
   const char * printSetUseSignExtensionMode();

   // Flag used by TR::BNDCHK nodes
   bool hasFoldedImplicitNULLCHK();
   void setHasFoldedImplicitNULLCHK(bool v);
   bool chkFoldedImplicitNULLCHK();
   const char * printHasFoldedImplicitNULLCHK();

   // Flag used by TR::athrow nodes
   bool throwInsertedByOSR();
   void setThrowInsertedByOSR(bool v);
   bool chkThrowInsertedByOSR();
   const char * printIsThrowInsertedByOSR();

   // Flags used by call nodes
   bool isTheVirtualCallNodeForAGuardedInlinedCall();
   void setIsTheVirtualCallNodeForAGuardedInlinedCall();
   void resetIsTheVirtualCallNodeForAGuardedInlinedCall();
   bool chkTheVirtualCallNodeForAGuardedInlinedCall();
   const char * printIsTheVirtualCallNodeForAGuardedInlinedCall();

   bool isArrayCopyCall();
   bool isDontTransformArrayCopyCall();
   void setDontTransformArrayCopyCall();
   bool chkDontTransformArrayCopyCall();
   const char * printIsDontTransformArrayCopyCall();

   bool isNodeRecognizedArrayCopyCall();
   void setNodeIsRecognizedArrayCopyCall(bool v);
   bool chkNodeRecognizedArrayCopyCall();
   const char * printIsNodeRecognizedArrayCopyCall();

   bool canDesynchronizeCall();
   void setDesynchronizeCall(bool v);
   bool chkDesynchronizeCall();
   const char * printCanDesynchronizeCall();

   bool isPreparedForDirectJNI();
   void setPreparedForDirectJNI();

   bool isSafeForCGToFastPathUnsafeCall();
   void setIsSafeForCGToFastPathUnsafeCall(bool v);

   // Flag used by TR::ladd and TR::lsub or by TR::lshl and TR::lshr for compressedPointers
   bool containsCompressionSequence();
   void setContainsCompressionSequence(bool v);
   bool chkCompressionSequence();
   const char * printContainsCompressionSequence();

   // Flag used by TR::aiadd and TR::aladd
   bool isInternalPointer();
   void setIsInternalPointer(bool v);
   const char * printIsInternalPointer();

   // Flags used by TR::arraytranslate and TR::arraytranslateAndTest
   bool isArrayTRT();
   void setArrayTRT(bool v);
   bool chkArrayTRT();
   const char * printArrayTRT();

   bool isCharArrayTRT();
   void setCharArrayTRT(bool v);
   bool chkCharArrayTRT();
   const char * printCharArrayTRT();

   bool isSourceByteArrayTranslate();
   void setSourceIsByteArrayTranslate(bool v);
   bool chkSourceByteArrayTranslate();
   const char * printSetSourceIsByteArrayTranslate();

   bool isTargetByteArrayTranslate();
   void setTargetIsByteArrayTranslate(bool v);
   bool chkTargetByteArrayTranslate();
   const char * printSetTargetIsByteArrayTranslate();

   bool isByteToByteTranslate();
   bool isByteToCharTranslate();
   bool isCharToByteTranslate();
   bool isCharToCharTranslate();
   bool chkByteToByteTranslate();
   bool chkByteToCharTranslate();
   bool chkCharToByteTranslate();
   bool chkCharToCharTranslate();
   const char * printIsByteToByteTranslate();
   const char * printIsByteToCharTranslate();
   const char * printIsCharToByteTranslate();
   const char * printIsCharToCharTranslate();

   bool getTermCharNodeIsHint();
   void setTermCharNodeIsHint(bool v);
   bool chkTermCharNodeIsHint();
   const char * printSetTermCharNodeIsHint();

   bool getSourceCellIsTermChar();
   void setSourceCellIsTermChar(bool v);
   bool chkSourceCellIsTermChar();
   const char * printSourceCellIsTermChar();

   bool getTableBackedByRawStorage();
   void setTableBackedByRawStorage(bool v);
   bool chkTableBackedByRawStorage();
   const char * printSetTableBackedByRawStorage();

   // Flags used by TR::arraycmp
   bool isArrayCmpLen();
   void setArrayCmpLen(bool v);
   bool chkArrayCmpLen();
   const char * printArrayCmpLen();

   bool isArrayCmpSign();
   void setArrayCmpSign(bool v);
   bool chkArrayCmpSign();
   const char * printArrayCmpSign();

   // Flags used by TR::arraycopy
   bool isHalfWordElementArrayCopy();
   void setHalfWordElementArrayCopy(bool v);
   bool chkHalfWordElementArrayCopy();
   const char * printIsHalfWordElementArrayCopy();

   bool isWordElementArrayCopy();
   void setWordElementArrayCopy(bool v);
   bool chkWordElementArrayCopy();
   const char * printIsWordElementArrayCopy();

   bool isForwardArrayCopy();
   void setForwardArrayCopy(bool v);
   bool chkForwardArrayCopy();
   const char * printIsForwardArrayCopy();

   bool isBackwardArrayCopy();
   void setBackwardArrayCopy(bool v);
   bool chkBackwardArrayCopy();
   const char * printIsBackwardArrayCopy();

   bool isRarePathForwardArrayCopy();
   void setRarePathForwardArrayCopy(bool v);
   bool chkRarePathForwardArrayCopy();
   const char * printIsRarePathForwardArrayCopy();

   bool isNoArrayStoreCheckArrayCopy();
   void setNoArrayStoreCheckArrayCopy(bool v);
   bool chkNoArrayStoreCheckArrayCopy();
   const char * printIsNoArrayStoreCheckArrayCopy();

   bool isArraysetLengthMultipleOfPointerSize();
   void setArraysetLengthMultipleOfPointerSize(bool v);

   // Flags used by TR::bitOpMem
   bool isXorBitOpMem();
   void setXorBitOpMem(bool v);
   bool chkXorBitOpMem();
   const char * printXorBitOpMem();

   bool isOrBitOpMem();
   void setOrBitOpMem(bool v);
   bool chkOrBitOpMem();
   const char * printOrBitOpMem();

   bool isAndBitOpMem();
   void setAndBitOpMem(bool v);
   bool chkAndBitOpMem();
   const char * printAndBitOpMem();

   // Flags used by TR::ArrayCHK
   bool isArrayChkPrimitiveArray1();
   void setArrayChkPrimitiveArray1(bool v);
   bool chkArrayChkPrimitiveArray1();
   const char * printIsArrayChkPrimitiveArray1();

   bool isArrayChkReferenceArray1();
   void setArrayChkReferenceArray1(bool v);
   bool chkArrayChkReferenceArray1();
   const char * printIsArrayChkReferenceArray1();

   bool isArrayChkPrimitiveArray2();
   void setArrayChkPrimitiveArray2(bool v);
   bool chkArrayChkPrimitiveArray2();
   const char * printIsArrayChkPrimitiveArray2();

   bool isArrayChkReferenceArray2();
   void setArrayChkReferenceArray2(bool v);
   bool chkArrayChkReferenceArray2();
   const char * printIsArrayChkReferenceArray2();

   // Flags used by TR::wrtbar and TR::iwrtbar
   bool skipWrtBar();
   void setSkipWrtBar(bool v);
   bool chkSkipWrtBar();
   const char * printIsSkipWrtBar();

   bool isLikelyStackWrtBar();
   void setLikelyStackWrtBar(bool v);

   bool isHeapObjectWrtBar();
   void setIsHeapObjectWrtBar(bool v);
   bool chkHeapObjectWrtBar();
   const char * printIsHeapObjectWrtBar();

   bool isNonHeapObjectWrtBar();
   void setIsNonHeapObjectWrtBar(bool v);
   bool chkNonHeapObjectWrtBar();
   const char * printIsNonHeapObjectWrtBar();

   bool isUnsafeStaticWrtBar();
   void setIsUnsafeStaticWrtBar(bool v);

   // Flag used by TR::istore/TR::iRegStore
   bool needsSignExtension();
   void setNeedsSignExtension(bool b);
   const char * printNeedsSignExtension();

   // Flag used by TR::iload/TR::iRegLoad
   bool skipSignExtension();
   void setSkipSignExtension(bool b);
   const char * printSkipSignExtension();

   // Flag used by FP regLoad/regStore
   bool needsPrecisionAdjustment();
   void setNeedsPrecisionAdjustment(bool v);
   bool chkNeedsPrecisionAdjustment();
   const char * printNeedsPrecisionAdjustment();

   // Flag used by TR_if or TR::istore/TR::iRegStore or TR::iadd, TR::ladd, TR::isub, TR::lsub
   bool isUseBranchOnCount();
   void setIsUseBranchOnCount(bool v);

   // Flag used by TR::xload/TR::xRegLoad
   bool isDontMoveUnderBranch();
   void setIsDontMoveUnderBranch(bool v);
   bool chkDontMoveUnderBranch();
   const char * printIsDontMoveUnderBranch();

   // Flag used by TR_xload
   bool canChkNodeCreatedByPRE();
   bool isNodeCreatedByPRE();
   void setIsNodeCreatedByPRE();
   void resetIsNodeCreatedByPRE();
   bool chkNodeCreatedByPRE();
   const char * printIsNodeCreatedByPRE();

   // Flag used by TR::xstore
   bool isPrivatizedInlinerArg();
   void setIsPrivatizedInlinerArg(bool v);
   bool chkIsPrivatizedInlinerArg();
   const char *printIsPrivatizedInlinerArg();

   // Flags used by TR_if
   bool isMaxLoopIterationGuard();
   void setIsMaxLoopIterationGuard(bool v);
   const char * printIsMaxLoopIterationGuard();

   bool isProfiledGuard();
   void setIsProfiledGuard();
   const char * printIsProfiledGuard();

   bool isInterfaceGuard();
   void setIsInterfaceGuard();
   const char * printIsInterfaceGuard();

   bool isAbstractGuard();
   void setIsAbstractGuard();
   const char * printIsAbstractGuard();

   bool isHierarchyGuard();
   void setIsHierarchyGuard();
   const char * printIsHierarchyGuard();

   bool isNonoverriddenGuard();
   void setIsNonoverriddenGuard();
   const char * printIsNonoverriddenGuard();

   bool isSideEffectGuard();
   void setIsSideEffectGuard();
   const char * printIsSideEffectGuard();

   bool isDummyGuard();
   void setIsDummyGuard();
   const char * printIsDummyGuard();

   bool isHCRGuard();
   void setIsHCRGuard();
   const char * printIsHCRGuard();

   bool isTheVirtualGuardForAGuardedInlinedCall();
   void resetIsTheVirtualGuardForAGuardedInlinedCall();
   bool isNopableInlineGuard();

   bool isMutableCallSiteTargetGuard();
   void setIsMutableCallSiteTargetGuard();
   const char * printIsMutableCallSiteTargetGuard();

   bool isMethodEnterExitGuard();
   void setIsMethodEnterExitGuard(bool b = true);
   const char * printIsMethodEnterExitGuard();

   bool isDirectMethodGuard();
   void setIsDirectMethodGuard(bool b = true);
   const char * printIsDirectMethodGuard();

   bool isOSRGuard();
   void setIsOSRGuard();
   const char * printIsOSRGuard();

   bool childrenWereSwapped();
   void setSwappedChildren(bool v);

   bool isVersionableIfWithMaxExpr();
   void setIsVersionableIfWithMaxExpr(TR::Compilation * c);

   bool isVersionableIfWithMinExpr();
   void setIsVersionableIfWithMinExpr(TR::Compilation * c);

   // Can be set on int loads and arithmetic operations
   bool isPowerOfTwo()          { return _flags.testAny(IsPowerOfTwo); }
   void setIsPowerOfTwo(bool b) { _flags.set(IsPowerOfTwo, b); }

   // Flags used by indirect stores and wrtbars for references
   bool isStoreAlreadyEvaluated();
   void setStoreAlreadyEvaluated(bool b);
   bool chkStoreAlreadyEvaluated();
   const char * printStoreAlreadyEvaluated();

   // Flags used by regLoad
   bool isSeenRealReference();
   void setSeenRealReference(bool b);
   bool chkSeenRealReference();
   const char * printIsSeenRealReference();

   // Flags used by TR::fbits2i and TR::dbits2l
   bool normalizeNanValues();
   void setNormalizeNanValues(bool v);
   bool chkNormalizeNanValues();
   const char * printNormalizeNanValues();

   // Flag used by node of datatype TR_[US]Int64
   bool isHighWordZero();
   void setIsHighWordZero(bool b);
   bool chkHighWordZero();
   const char * printIsHighWordZero();

   // Flag used by node of datatype which isn't TR_[US]Int64
   bool isUnsigned();
   void setUnsigned(bool b);
   bool chkUnsigned();
   const char * printIsUnsigned();

   // Flags used by aconst and iaload (on s390 iaload can be used after dynamic lit pool)
   bool isClassPointerConstant();
   void setIsClassPointerConstant(bool b);
   bool chkClassPointerConstant();
   const char * printIsClassPointerConstant();

   bool isMethodPointerConstant();
   void setIsMethodPointerConstant(bool b);
   bool chkMethodPointerConstant();
   const char * printIsMethodPointerConstant();

   bool isUnneededIALoad();
   void setUnneededIALoad(bool v);

   // Flags used by TR::monent, TR::monexit, ad TR::tstart
   bool canSkipSync();
   void setSkipSync(bool v);
   bool chkSkipSync();
   const char * printIsSkipSync();

   bool isStaticMonitor();
   void setStaticMonitor(bool v);
   bool chkStaticMonitor();
   const char * printIsStaticMonitor();

   bool isSyncMethodMonitor();
   void setSyncMethodMonitor(bool v);
   bool chkSyncMethodMonitor();
   const char * printIsSyncMethodMonitor();

   bool isReadMonitor();
   void setReadMonitor(bool v);
   bool chkReadMonitor();
   const char * printIsReadMonitor();

   bool isLocalObjectMonitor();
   void setLocalObjectMonitor(bool v);
   bool chkLocalObjectMonitor();
   const char * printIsLocalObjectMonitor();

   bool isPrimitiveLockedRegion();
   void setPrimitiveLockedRegion(bool v = true);
   bool chkPrimitiveLockedRegion();
   const char * printIsPrimitiveLockedRegion();

   bool hasMonitorClassInNode();
   void setHasMonitorClassInNode(bool v);

   // Flags used by TR::newarray and TR::anewarray
   bool markedAllocationCanBeRemoved();
   void setAllocationCanBeRemoved(bool v);
   bool chkAllocationCanBeRemoved();
   const char * printAllocationCanBeRemoved();

   bool canSkipZeroInitialization();
   void setCanSkipZeroInitialization(bool v);
   bool chkSkipZeroInitialization();
   const char * printCanSkipZeroInitialization();

   // Flag used by TR::imul
   bool isAdjunct();
   void setIsAdjunct(bool v);

   // Flags used by TR::loadaddr
   bool cannotTrackLocalUses();
   void setCannotTrackLocalUses(bool v);
   bool chkCannotTrackLocalUses();
   const char * printCannotTrackLocalUses();

   bool escapesInColdBlock();
   void setEscapesInColdBlock(bool v);
   bool chkEscapesInColdBlock();
   const char * printEscapesInColdBlock();

   bool cannotTrackLocalStringUses();
   void setCannotTrackLocalStringUses(bool v);
   bool chkCannotTrackLocalStringUses();
   const char * printCannotTrackLocalStringUses();

   // Flag used by TR::allocationFence
   bool canOmitSync();
   void setOmitSync(bool v);
   bool chkOmitSync();
   const char * printIsOmitSync();

   // Flag used by div and rem
   bool isSimpleDivCheck();
   void setSimpleDivCheck(bool v);
   bool chkSimpleDivCheck();
   const char * printIsSimpleDivCheck();

   // Flag used by shl and shr
   bool isNormalizedShift();
   void setNormalizedShift(bool v);
   bool chkNormalizedShift();
   const char * printIsNormalizedShift();

   // Flag used by operator nodes
   bool isFPStrictCompliant();
   void setIsFPStrictCompliant(bool v);
   const char * printIsFPStrictCompliant();

   // Flags used by conversion nodes
   bool isUnneededConversion();
   void setUnneededConversion(bool v);
   const char * printIsUnneededConversion();

   bool parentSupportsLazyClobber();
   void setParentSupportsLazyClobber(bool v);
   const char * printParentSupportsLazyClobber();
   /// Helpful function that's simpler to call than setParentSupportsLazyClobber.
   /// A tree evaluator that supports lazy clobbering (meaning checks the
   /// register's node count before clobbering a child node's register) can
   /// call this before evaluating its child, regardless of what that child is.
   ///
   void oneParentSupportsLazyClobber(TR::Compilation *comp);

   // Flag used by float to fixed conversion nodes
   bool useCallForFloatToFixedConversion();
   void setUseCallForFloatToFixedConversion(bool v = true);
   bool chkUseCallForFloatToFixedConversion();
   const char * printUseCallForFloatToFixedConversion();

   // Flags for TR::fence
   bool isLoadFence();
   void setIsLoadFence();

   bool isStoreFence();
   void setIsStoreFence();

   // Flags used by TR::checkcast, TR::checkCastForArrayStore, and TR::instanceof
   inline bool          canCheckReferenceIsNonNull();
   inline bool          isReferenceNonNull();
   inline void          setReferenceIsNonNull(bool v = true);
   inline bool          chkIsReferenceNonNull();
   inline const char *  printIsReferenceNonNull();

   // Flag used by TR::Return
   bool isReturnDummy();
   void setReturnIsDummy();
   bool chkReturnIsDummy();
   const char * printReturnIsDummy();

   // Flag used by ilload nodes for DFP
   bool isBigDecimalLoad();
   void setIsBigDecimalLoad();

   // Flag used by iload, iiload, lload, ilload, aload, iaload
   bool isLoadAndTest()          { return _flags.testAny(loadAndTest); }
   void setIsLoadAndTest(bool v) { _flags.set(loadAndTest, v); }

   // Flag for TR::New
   bool shouldAlignTLHAlloc();
   void setAlignTLHAlloc(bool v);

   // Flag used by TR_PassThrough
   bool isCopyToNewVirtualRegister();
   void setCopyToNewVirtualRegister(bool v = true);
   const char * printCopyToNewVirtualRegister();

   // zEmulator only?
   bool nodeRequiresConditionCodes();
   void setNodeRequiresConditionCodes(bool v);
   bool chkOpsNodeRequiresConditionCodes();
   const char *printRequiresConditionCodes();

   // Clear out relevant flags set on the node.
   void resetFlagsForCodeMotion();

   /**
    * Node flags functions end
    */


// Public inner classes and struct definitions.
public:
   class ChildIterator
      {
      TR::Node *_node;
      int32_t   _index;

      public:

      ChildIterator(TR::Node *node, int32_t index):_node(node),_index(index){}

      int32_t   currentChildIndex(){ return _index; }
      TR::Node *currentChild();

      void stepForward();
      bool isAt(const ChildIterator &other){ return _node == other._node && _index == other._index; }

      public: // operators

      void      operator ++() { stepForward(); }
      TR::Node *operator ->() { return currentChild(); } // Make a ChildIterator act like a Node pointer
      TR::Node *operator  *() { return currentChild(); } // Normal C++ style would have this returning a Node&, but Node* is way handier

      bool      operator ==(const ChildIterator &other) { return  this->isAt(other); }
      bool      operator !=(const ChildIterator &other) { return !this->isAt(other); }
      };


// Protected functions
protected:

   /** setOpCodeValue is now private, and deprecated. Use recreate instead. */
   TR::ILOpCodes setOpCodeValue(TR::ILOpCodes op);

   // Misc helpers
   TR::Node * duplicateTreeWithCommoningImpl(CS2::HashTable<TR::Node *, TR::Node *, TR::Allocator> &nodeMapping);

   bool collectSymbolReferencesInNode(TR_BitVector &symbolReferencesInNode, vcount_t visitCount);

   // For OptAttributes
   void setOptAttributes(OptAttributes * oa) { _optAttributes = oa; }
   OptAttributes * getOptAttributes() { TR_ASSERT(_optAttributes != NULL, "_optAttributes is NULL!"); return _optAttributes; }

   // For NodeExtension
   inline bool  hasNodeExtension();
   inline void  setHasNodeExtension(bool v);
   size_t       sizeOfExtension();
   void         addExtensionElements(uint16_t num);
   void         createNodeExtension(uint16_t numElems);
   void         copyNodeExtension(TR::NodeExtension * other, uint16_t numElems, size_t size);

   // For UnionPropertyA
   bool hasSymbolReference();
   bool hasRegLoadStoreSymbolReference();
   bool hasSymbolReferenceOfAnyType();
   bool hasBranchDestinationNode();
   bool hasBlock();
   bool hasArrayStride();
   bool hasPinningArrayPointer();
   bool hasDataType();

// Protected inner classes and structs.
protected:

   /**
    * Discarding optimization phase specific TR::Node data after node evaluation
    *
    * The OptAttribute class is meant contain fields of TR::Node which are relevant
    * only before instruction selection has ended.
    *
    * A side table of OptAttributes is kept in TR::NodePool and will be freed
    * when instruction selection is complete.  A Node's pointer to its
    * OptAttributes will be NULLed as well, triggering an assertion on any
    * attempt to access these fields post-instruction selection.
    */
   struct OptAttributes {
      friend class TR::NodePool;

      OptAttributes();
      void * operator new (size_t s, TR::NodePool & nodes);

      vcount_t _visitCount;
      rcount_t _referenceCount;
      scount_t _localIndex;        ///< general index that has the lifetime of a single optimization pass

      union
         {
         uint16_t _useDefIndex;    ///< used by optimizations

         /**
          * The _register field is only used during codegen. It is a
          * tagged pointer though. The low bit set means that the field
          * is actually an evaluation priority.
          * 0   - not evaluated, priority unknown
          * odd - not evaluated, rest of the fields is evaluationPriority
          * even- evaluated into a register
          */
         TR::Register * _register;
         } _unionA;

      ncount_t _poolIndex;         ///< index to retrieve node from nodepool, this can be shared amongst nodes with non-intersecting lifetimes
   };

   struct NodeExtensionStore
      {
      void setExtensionPtr(TR::NodeExtension * ptr) { _data = ptr; }
      TR::NodeExtension * getExtensionPtr()         { return _data; }

      void setNumElems(uint16_t numElems) { _numElems = numElems; }
      uint16_t getNumElems()              { return  _numElems; }

   private:
      TR::NodeExtension * _data;
      uint16_t _flags;   // for future use
      uint16_t _numElems;
      };

   // Union of elements exclusive with children
   union UnionBase
      {
      //Const Values -- exclusive since const have no children
      int64_t   _constValue;
      float     _fpConstValue;
      double    _dpConstValue;

      //intToFloat returns a bag of bits in a uint32_t
      int32_t   _fpConstValueBits;


      Node::NodeExtensionStore    _extension;
      TR::Node*                   _children[NUM_DEFAULT_CHILDREN];

      UnionedWithChildren         _unionedWithChildren;

      UnionBase()
         {
         memset(this, 0, sizeof(UnionBase)); // in C++11 notation: _unionBase = {0};
         }
      };

   union UnionPropertyA
      {
      // these first two TR::SymbolReference fields are aliases, and are not disjoint
      TR::SymbolReference   *_symbolReference;              ///< hasSymbolReference()
      TR::SymbolReference   *_regLoadStoreSymbolReference;  ///< hasRegLoadStoreSymbolReference()

      // the rest are disjoint
      TR::TreeTop           *_branchDestinationNode;        ///< hasBranchDestinationNode()
      TR::Block             *_block;                        ///< hasBlock()
      int32_t                _arrayStride;                  ///< hasArrayStride()
      TR::AutomaticSymbol  *_pinningArrayPointer;          ///< hasPinningArrayPointer()
      TR::DataTypes         _dataType;                     ///< hasDataType()  TODO: Change to TR::DataType once all target compilers support it

      UnionPropertyA()
         {
         memset(this, 0, sizeof(UnionPropertyA)); ///< in C++11 notation: _unionPropertyA = {0};
         }
      };

   // Union Property verification helpers
   enum UnionPropertyA_Type
      {
      HasNoUnionPropertyA = 0,
      HasSymbolReference,              ///< hasSymbolReference()
      HasRegLoadStoreSymbolReference,  ///< hasRegLoadStoreSymbolReference()
      HasBranchDestinationNode,        ///< hasBranchDestinationNode()
      HasBlock,                        ///< hasBlock()
      HasArrayStride,                  ///< hasArrayStride()
      HasPinningArrayPointer,          ///< hasPinningArrayPointer()
      HasDataType                      ///< hasDataType()
      };

   UnionPropertyA_Type getUnionPropertyA_Type();


// Protected fields
protected:

   /// Operation this node represents.
   ///
   /// \note Should be the first field, as it makes debugging
   ///       easier.
   TR::ILOpCode           _opCode;

   /// Number of children this node has.
   uint16_t               _numChildren;

   /// Unique index for every single node created - no other node
   /// will have the same index within a compilation.
   ncount_t               _globalIndex;

   /// Flags for the node.
   flags32_t              _flags;

   /// Info about the byte code associated with this node.
   TR_ByteCodeInfo        _byteCodeInfo;

   /// Contains fields of TR::Node which are relevant
   /// only before instruction selection has ended.
   OptAttributes *        _optAttributes;

   /// Elements unioned with children.
   UnionBase              _unionBase;

   /// Misc properties of nodes.
   UnionPropertyA         _unionPropertyA;


// Private functionality.
private:
   TR::Node * getExtendedChild(int32_t c);

   friend class ::TR_DebugExt;
   friend class TR::NodePool;


// flag bits
protected:
   enum
      {
      // Here's map of how the flag bits have been allocated
      //
      // 0x00000000 - 0x00000400 are general node flags (6 bits)
      // 0x00000400 - 0x00008000 are node specific flags (10 bits)
      // 0x00001000 - 0x10000000 are unallocated (13 bits)
      // 0x20000000 - 0x80000000 are frontend specific general node flags (3 bits)
      //

      //---------------------------------------- general flags ---------------------------------------

      nodeIsZero                            = 0x00000002,
      nodeIsNonZero                         = 0x00000004,
      nodeIsNull                            = 0x00000002,
      nodeIsNonNull                         = 0x00000004,
      nodePointsToNull                      = 0x00000002,
      nodePointsToNonNull                   = 0x00000004,
      nodeContainsCall                      = 0x00000008, ///< Only used during local analysis
      invalid8BitGlobalRegister             = 0x00000010, ///< value is in a global register which cannot be used on an 8 bit instruction
      isHPREligible                         = 0x00000010, ///< 390 zGryphon Highword register GRA
      nodeHasExtension                      = 0x00000020,
      directMemoryUpdate                    = 0x00000040, ///< Result of this node is being stored into the same
                                                          ///< location as its left child
      profilingCode                         = 0x00000080, ///< Used prior to codegen phase
      visitedForHints                       = 0x00000080, ///< Used only during codegen phase
      nodeIsNonNegative                     = 0x00000100,
      nodeIsNonPositive                     = 0x00000200,
      nodeIsDebug                           = 0x00000400, ///< Should not be counted for metrics

      //---------------------------------------- node specific flags---------------------------------------

      // Flag used by arithmetic int/long operations
      cannotOverFlow                        = 0x00001000,

      // Flag used by TR::table nodes
      canSkipTableBoundCheck                = 0x00008000,

      // Flag used by TR::lstore and TR::lstorei nodes
      NOPLongStore                          = 0x00008000,

      /// Flag used by stores to stack variables where the RHS value being
      /// stored is basically irrelevant since the stack slot is dead anyway.
      /// The main purpose of such stores is to "naturally" express the fact
      /// that the slots are no longer live along a certain control flow path
      /// even though there is a "use" that appears later along the control flow
      /// path.  This can occur when there is a join in the control flow and the
      /// slot was actually in use along some other control flow path that
      /// merged in, but not on the control flow path where we emitted a store
      /// with this flag set.  The case that has motivated the addition of this
      /// flag is OSR (on stack replacement) in Java with a shared OSR catch
      /// block (helper call that has uses of the slots as children). We want to
      /// emit stores with this flag set on some side exits that could lead to
      /// the OSR catch block where we know that the slot is not live as per the
      /// bytecodes.
      ///
      StoredValueIsIrrelevant               = 0x00020000,

      // Flags used by TR::astore node
      liveMonitorInitStore                  = 0x00002000,
      HeapificationStore                    = 0x00008000,

      // Flag used by TR::iload nodes
      SignExtensionMode                     = 0x00000800,

      // Flags used by TR::BNDCHK nodes
      foldedImplicitNULLCHK                 = 0x00008000,

      // Flag used by TR::athrow nodes
      ThrowInsertedByOSR                    = 0x00004000,

      // Flags used by call nodes
      virtualCallNodeForAGuardedInlinedCall = 0x00000800, ///< virtual calls
      dontTransformArrayCopyCall            = 0x00000800, ///< arraycopy call - static
      nodeIsRecognizedArrayCopyCall         = 0x00010000,
      desynchronizeCall                     = 0x00020000,
      preparedForDirectToJNI                = 0x00040000, // TODO: make J9_PROJECT_SPECIFIC
      unsafeFastPathCall                    = 0x00080000, // TODO: make J9_PROJECT_SPECIFIC

      // Flag used by TR::ladd and TR::lsub or by TR::lshl and TR::lshr for compressedPointers
      isCompressionSequence                 = 0x00000800,

      // Flag used by TR::aiadd and TR::aladd
      internalPointer                       = 0x00008000,

      // Flags used by TR::arraytranslate and TR::arraytranslateAndTest
      arrayTRT                              = 0x00008000,  ///< used by arraytranslateAndTest
      charArrayTRT                          = 0x00004000,  ///< used by arraytranslateAndTest
      sourceIsByteArrayTranslate            = 0x00001000,
      targetIsByteArrayTranslate            = 0x00002000,
      termCharNodeIsHint                    = 0x00004000,
      sourceCellIsTermChar                  = 0x00000800,
      tableBackedByRawStorage               = 0x00008000,

      // Flags used by TR::arraycmp
      arrayCmpLen                           = 0x00008000,
      arrayCmpSign                          = 0x00004000,

      // Flags used by TR::arraycopy
      arraycopyElementSizeMask              = 0x00001800,
      arraycopyElementSizeUnknown           = 0x00000000,
      arraycopyElementSize2                 = 0x00000800,
      arraycopyElementSize4                 = 0x00001000,
      arraycopyDirectionMask                = 0x00006000,
      arraycopyDirectionUnknown             = 0x00000000,
      arraycopyDirectionBackward            = 0x00002000,
      arraycopyDirectionForward             = 0x00004000,
      arraycopyDirectionForwardRarePath     = 0x00006000,
      noArrayStoreCheckArrayCopy            = 0x00008000,
      arraysetLengthMultipleOfPointerSize   = 0x00020000, ///< flag used by TR::arrayset to guarantee that checking for lengths smaller than pointer size is not needed (leading to more efficient code being generated, e.g. REP STOSQ on X86

      // Flags used by TR::bitOpMem
      bitOpMemOPMASK                        = 0x00003000, ///< currently, XOR, AND, OR operations are suppoerted
      bitOpMemXOR                           = 0x00001000,
      bitOpMemAND                           = 0x00002000,
      bitOpMemOR                            = 0x00003000,

      // Flags used by TR::ArrayCHK
      arrayChkPrimitiveArray1               = 0x00001000,
      arrayChkReferenceArray1               = 0x00002000,
      arrayChkPrimitiveArray2               = 0x00004000,
      arrayChkReferenceArray2               = 0x00008000,

      // Flags used by wrtbar and iwrtbar
      SkipWrtBar                            = 0x00000800,
      likelyStackWrtBar                     = 0x00002000,
      heapObjectWrtBar                      = 0x00004000,
      nonHeapObjectWrtBar                   = 0x00008000,
      unsafeStaticWrtBar                    = 0x00010000,

      NeedsSignExtension                    = 0x00004000, ///< Flag used by TR::istore/TR::iRegStore
      SkipSignExtension                     = 0x00004000, ///< Flag used by TR::iload/TR::iRegLoad
      precisionAdjustment                   = 0x00004000, ///< Flag used by FP regLoad/regStore

      nodeUsedForBranchOnCount              = 0x00000400, ///< Flag used by TR_if or TR::istore/TR::iRegStore or TR::iadd,
                                                          ///< TR::ladd, TR::isub, TR::lsub

      dontMoveUnderBranch                   = 0x00002000, ///< Flag used by TR::xload/TR::xRegLoad
      nodeCreatedByPRE                      = 0x00040000, ///< Flag used by TR_xload

      // Flag used by TR::xstore
      privatizedInlinerArg                  = 0x00002000,

      // Flags used by TR_if
      maxLoopIterationGuard                 = 0x00000800,  ///< allows redundant async check removal to remove ac's
      inlineGuardMask                       = 0x0000F000,
      inlineProfiledGuard                   = 0x00001000,
      inlineInterfaceGuard                  = 0x00002000,
      inlineAbstractGuard                   = 0x00003000,
      inlineHierarchyGuard                  = 0x00004000,
      inlineNonoverriddenGuard              = 0x00005000,
      sideEffectGuard                       = 0x00006000,
      dummyGuard                            = 0x00007000,
      inlineHCRGuard                        = 0x00008000,
      mutableCallSiteTargetGuard            = 0x00009000,
      methodEnterExitGuard                  = 0x0000A000,
      directMethodGuard                     = 0x0000B000,
      osrGuard                              = 0x0000C000,
      swappedChildren                       = 0x00020000,
      versionIfWithMaxExpr                  = 0x00010000,
      versionIfWithMinExpr                  = 0x00040000,

      // Can be set on int loads and arithmetic operations
      IsPowerOfTwo                          = 0x10000000,

      // Flags used by indirect stores & wrtbars for references
      storeAlreadyEvaluated                 = 0x00001000,

      // Flags used by regLoad
      SeenRealReference                     = 0x00008000,

      // Flags used by TR::fbits2i and TR::dbits2l
      mustNormalizeNanValues                = 0x00008000,

      // Flag used by node of datatype TR_[US]Int64; used to denote
      // some extra range information computed by value propagation
      //
      highWordZero                          = 0x00004000,

      // Flag used by node of datatype which isn't TR_[US]Int64; used to denote
      // that the operation should be executed by unsigned instruction in the codegen
      // currently used for ldiv folding to idiv, when both children are with highWordZero
      Unsigned                              = 0x00004000,

      // Flags used by TR::aconst and TR::iaload (on s390 iaload can be used after dynamic lit pool)
      //
      classPointerConstant                  = 0x00010000,
      methodPointerConstant                 = 0x00002000,
      unneededIALoad                        = 0x00001000,

      // Flags used by TR::monent, TR::monexit, and TR::tstart
      //
      skipSync                              = 0x00000800,
      staticMonitor                         = 0x00001000,
      syncMethodMonitor                     = 0x00002000,
      readMonitor                           = 0x00004000,
      localObjectMonitor                    = 0x00008000,
      primitiveLockedRegion                 = 0x00000400,
      monitorClassInNode                    = 0x00010000,

      // Flags only used for isNew() opcodes
      //
      HeapificationAlloc                    = 0x00001000,

      // Flag used by TR::newarray and TR::anewarray
      //
      allocationCanBeRemoved                = 0x00004000,
      skipZeroInit                          = 0x00008000,

      // Flag used by TR::lmul (possibly TR::luadd, TR::lusub)
      // Whether this node is the adjunct node of a dual high node
      adjunct                               = 0x00010000,

      // Flag used by TR::loadaddr
      //
      cantTrackLocalUses                    = 0x00008000,
      coldBlockEscape                       = 0x00001000,
      cantTrackLocalStringUses              = 0x00002000,

      // Flag used by TR::allocationFence
      //
      omitSync                              = 0x00008000,

      // Flag used by div/rem
      //
      simpleDivCheck                        = 0x00008000,

      // Flag used by shl and shr
      //
      normalizedShift                       = 0x00008000,

      // Flag used by operator nodes (like TR_mul)
      // result of operator is bit-wise identical whether
      // float/double or double/extended double used
      resultFPStrictCompliant               = 0x00002000,

      // Flags used by conversion nodes e.g. i2b
      //
      unneededConv                          = 0x00008000,
      ParentSupportsLazyClobber             = 0x00002000, ///< Tactical x86 codegen flag.  Only when refcount <= 1.  Indicates that parent will consult the register's node count before clobbering it (not just the node's refcount).

      // Flag used by float to fixed conversion nodes e.g. f2i/f2pd/d2i/df2i/f2l/d2l/f2s/d2pd etc
      callForFloatToFixedConversion         = 0x00400000,

      // Flags used by TR::fence
      fenceLoad                             = 0x00000400,
      fenceStore                            = 0x00000800,

      // Flag used by TR::checkcast, TR::checkCastForArrayStore, and TR::instanceof
      referenceIsNonNull                    = 0x00008000, ///< Sometimes we can't mark the child as non-null (because it's not provably non-null everywhere) but we know it's non-null by the time we run this parent

      // Flag used by TR::Return
      returnIsDummy                         = 0x00001000,

      // Flag used by ilload nodes for BigDecimal long field (DFP)
      // Active when disableDFP JIT option not specifiec, and running
      // on hardware Power or zSeries hardware that supports DFP
      bigDecimal_load                       = 0x00000002, // TODO: make J9_PROJECT_SPECIFIC

      // Flag used by iload, iiload, lload, ilload, aload, iaload
      loadAndTest                           = 0x00008000,

      // Flag used by TR::New when the codegen supports
      // alignment on the TLH when allocating the object
      //
      alignTLHAlloc                         = 0x00002000,

      // Flag used by TR_PassThrough
      copyToNewVirtualRegister              = 0x00001000,

      // zEmulator only?
      requiresConditionCodes                = 0x80000000,
      };

   };

typedef TR_ArenaAllocator NodeExtAllocator;

}

#endif
