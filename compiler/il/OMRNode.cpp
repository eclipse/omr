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

#include "il/Node.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "cs2/hashtab.h"
#include "env/ClassEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Environment.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/VMEnv.hpp"
#include "env/defines.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/IL.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodeExtension.hpp"
#include "il/NodePool.hpp"
#include "il/NodeUtils.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGen.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "infra/TRlist.hpp"
#include "infra/Checklist.hpp"
#include "optimizer/LoadExtensions.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "ras/Debug.hpp"

#ifdef J9_PROJECT_SPECIFIC
#ifdef TR_TARGET_S390
#include "z/codegen/S390Register.hpp"
#endif
#endif
/**
 * Node constructors and create functions
 */

void *
OMR::Node::operator new(size_t s, TR::NodePool& nodes)
   {
   return (void *) nodes.allocate();
   }

void
OMR::Node::operator delete(void *node, TR::NodePool& nodes)
   {
   nodes.deallocate((TR::Node *) node);
   }

void *
OMR::Node::operator new(size_t s, void *ptr) throw()
   {
   return ::operator new(s, ptr);
   }

void
OMR::Node::operator delete(void *node, void *ptr) throw()
   {
   ::operator delete(node, ptr);
   }

OMR::Node::Node()
   : _opCode(TR::BadILOp),
     _numChildren(0),
     //_globalIndex(0),
     _flags(0),
     _visitCount(0),
     _localIndex(0),
     _referenceCount(0),
     _knownObjectIndex(TR::KnownObjectTable::UNKNOWN),
     _byteCodeInfo(),
     _unionBase(),
     _unionPropertyA()
   {
   }

OMR::Node::~Node()
   {
   _unionPropertyA = UnionPropertyA();
   _opCode.setOpCodeValue(TR::BadILOp);
   self()->freeExtensionIfExists();
   }

OMR::Node::Node(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren)
   : _opCode(op),
     _numChildren(numChildren),
     //_globalIndex(0),
     _flags(0),
     _visitCount(0),
     _localIndex(0),
     _referenceCount(0),
     _knownObjectIndex(TR::KnownObjectTable::UNKNOWN),
     _byteCodeInfo(),
     _unionBase(),
     _unionPropertyA()
   {
   TR::Compilation * comp = TR::comp();

   if (!comp->isPeekingMethod() && self()->uses64BitGPRs())
      comp->getJittedMethodSymbol()->setMayHaveLongOps(true);

   uint16_t numElems = numChildren;
   if (numElems > NUM_DEFAULT_CHILDREN)
      {
      self()->createNodeExtension(numElems);
      }

   if (op == TR::BBStart)
      {
      self()->setChild(0, NULL);
      self()->setLabel(NULL);
      }
   else
      {
      self()->setChild(0, NULL);
      self()->setChild(1, NULL);
      }

   self()->setReferenceCount(0);
   self()->setVisitCount(0);
   self()->setLocalIndex(0);
   self()->setKnownObjectIndex(TR::KnownObjectTable::UNKNOWN),
   memset( &(_unionA), 0, sizeof( _unionA ) );
   if (self()->getGlobalIndex() == MAX_NODE_COUNT)
      {
      TR_ASSERT(0, "getGlobalIndex() == MAX_NODE_COUNT");
      comp->failCompilation<TR::ExcessiveComplexity>("Global index equal to max node count");
      }

   _byteCodeInfo.setInvalidCallerIndex();
   _byteCodeInfo.setIsSameReceiver(0);
   TR_IlGenerator * ilGen = comp->getCurrentIlGenerator();
   if (ilGen)
      {
      int32_t i = ilGen->currentByteCodeIndex();
      _byteCodeInfo.setByteCodeIndex(i >= 0 ? i : 0);
      _byteCodeInfo.setCallerIndex(comp->getCurrentInlinedSiteIndex());

      if (_byteCodeInfo.getCallerIndex() < 0)
         _byteCodeInfo.setCallerIndex(ilGen->currentCallSiteIndex());
      TR_ASSERT(_byteCodeInfo.getCallerIndex() < (SHRT_MAX/4), "Caller index too high; cannot set high order bit\n");
      _byteCodeInfo.setDoNotProfile(0);
      }
   else if (originatingByteCodeNode)
      {
      _byteCodeInfo = originatingByteCodeNode->getByteCodeInfo();
      _byteCodeInfo.setDoNotProfile(1);
      }
   else
      {
      // Commented out because dummy nodes are created in
      // estimate code size to be able to propagate frequencies
      //else
      //   TR_ASSERT(0, "no byte code info");

      _byteCodeInfo.setDoNotProfile(1);
      }
      if(comp->getDebug())
        comp->getDebug()->newNode(self());

   // check that _unionPropertyA union is disjoint
   TR_ASSERT(
         self()->hasSymbolReference()
       + self()->hasRegLoadStoreSymbolReference()
       + self()->hasBranchDestinationNode()
       + self()->hasBlock()
       + self()->hasArrayStride()
       + self()->hasPinningArrayPointer()
       + self()->hasDataType() <= 1,
         "_unionPropertyA union is not disjoint for this node %s (%p):\n"
         "  has({SymbolReference, ...}, ..., DataType) = ({%1d,%1d},%1d,%1d,%1d,%1d,%1d)\n",
         self()->getOpCode().getName(), this,
         self()->hasSymbolReference(),
         self()->hasRegLoadStoreSymbolReference(),
         self()->hasBranchDestinationNode(),
         self()->hasBlock(),
         self()->hasArrayStride(),
         self()->hasPinningArrayPointer(),
         self()->hasDataType());
   }

/**
 * Copy constructor, must allow for copying more than 2 children.
 */
OMR::Node::Node(TR::Node * from, uint16_t numChildren)
   : _opCode(TR::BadILOp),
     _numChildren(0),
     _globalIndex(0),
     _flags(0),
     _visitCount(0),
     _localIndex(0),
     _referenceCount(0),
     _knownObjectIndex(TR::KnownObjectTable::UNKNOWN),
     _byteCodeInfo(),
     _unionBase(),
     _unionPropertyA()
   {
   TR::Compilation * comp = TR::comp();
   memcpy(self(), from, sizeof(TR::Node));
   if (self()->hasDataType())
      self()->setDataType(TR::NoType);

   self()->copyChildren(from, numChildren, true /* forNodeExtensionOnly */);

   if (from->getOpCode().getOpCodeValue() == TR::allocationFence)
      self()->setAllocation(NULL);

   self()->setGlobalIndex(comp->getNodePool().getLastGlobalIndex());
   // a memcpy is used above to copy fields from the argument "node" to "this", but
   // opt attributes are separate, and need separate initialization.
   self()->setReferenceCount(from->getReferenceCount());
   self()->setVisitCount(from->getVisitCount());
   self()->setLocalIndex(from->getLocalIndex());
   self()->setKnownObjectIndex(from->getKnownObjectIndex());
   _unionA = from->_unionA;

   if (self()->getGlobalIndex() == MAX_NODE_COUNT)
      {
      TR_ASSERT(0, "getGlobalIndex() == MAX_NODE_COUNT");
      comp->failCompilation<TR::ExcessiveComplexity>("Global index equal to max node count");
      }

   if(comp->getDebug())
      comp->getDebug()->newNode(self());

   TR_IlGenerator * ilGen = comp->getCurrentIlGenerator();
   if (ilGen)
      {
      _byteCodeInfo.setDoNotProfile(0);
      }
   else
      {
      _byteCodeInfo.setDoNotProfile(1);
      }

   if (from->getOpCode().isBranch() || from->getOpCode().isSwitch())
      _byteCodeInfo.setDoNotProfile(1);

   if (from->getOpCode().isStoreReg() || from->getOpCode().isLoadReg())
      {
      if (from->requiresRegisterPair(comp))
         {
         self()->setLowGlobalRegisterNumber(from->getLowGlobalRegisterNumber());
         self()->setHighGlobalRegisterNumber(from->getHighGlobalRegisterNumber());
         }
      else
         {
         self()->setGlobalRegisterNumber(from->getGlobalRegisterNumber());
         }
      }

   if (from->hasDataType())
      {
      self()->setDataType(from->getDataType());
      }
   }



TR::Node *
OMR::Node::copy(TR::Node * from)
   {
   TR::Compilation *comp = TR::comp();
   int32_t numChildren = from->getNumChildren();
   TR::ILOpCode opCode = from->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

   if (opCode.isIf() ||
      opCodeValue == TR::anewarray ||
      opCodeValue == TR::newarray ||
      opCodeValue == TR::arraycopy ||
      opCodeValue == TR::tstart ||
      opCodeValue == TR::arrayset)
      ++numChildren;

   // note arraystorechk node has 1 or 2 children but it allocates spaces for 3 children
   // we need to make sure we have room for 3
   if (opCodeValue == TR::ArrayStoreCHK)
      numChildren = 3;

   TR::Node * n = new (comp->getNodePool()) TR::Node(from);
   from->copyVirtualGuardInfoTo(n, comp);
   return n;
   }

TR::Node *
OMR::Node::copy(TR::Node * from, int32_t numChildren)
   {
   TR::Compilation *comp = TR::comp();
   TR_ASSERT(numChildren >= from->getNumChildren(), "copy must have at least as many children as original");

   TR::Node *clone = new (comp->getNodePool()) TR::Node(from, numChildren);
   from->copyVirtualGuardInfoTo(clone, comp);

   return clone;
   }



/**
 * Copy the children of from to this node, extending if necessary
 *
 * If !forNodeExtensionOnly then clone the default children as well, including the _numChildren[Room] fields
 */
void
OMR::Node::copyChildren(TR::Node *from, uint16_t numChildren /* ignored if !forNodeExtensionOnly */, bool forNodeExtensionOnly)
   {
   if (!forNodeExtensionOnly)
      {
      _numChildren = numChildren = from->getNumChildren();
      }

   if (from->hasNodeExtension())
      {
      // if original node had a node extension, need to copy over those elements
      if (numChildren > from->_unionBase._extension.getNumElems())
         {
         self()->createNodeExtension(numChildren);
         for(uint16_t childNum = 0; childNum < from->_unionBase._extension.getNumElems(); childNum++)
            self()->setChild(childNum, from->getChild(childNum));
         }
      else
         {
         size_t size = from->sizeOfExtension();
         self()->copyNodeExtension(from->_unionBase._extension.getExtensionPtr(), from->_unionBase._extension.getNumElems(), size);
         }
      }
   else
      {
      if (numChildren > NUM_DEFAULT_CHILDREN)
         self()->createNodeExtension(numChildren);
      if ((numChildren > NUM_DEFAULT_CHILDREN) || !forNodeExtensionOnly)
         for(uint16_t childNum = 0 ; childNum < from->getNumChildren(); childNum++)
            self()->setChild(childNum, from->getChild(childNum));
      }
   }

void
OMR::Node::copyValidProperties(TR::Node *fromNode, TR::Node *toNode)
   {
   // IMPORTANT: Copy UnionPropertyA first
   // Some parts of the code rely on getDataType, which can assert if
   // toNode's opcode before copying has the HasNoDataType property and
   // fromNode's opcode does not have the HasNoDataType property
   // Also, copy all children in case the node does not have a data type
   // and it has to be computed from its children
   UnionPropertyA_Type fromUnionPropertyA_Type = fromNode->getUnionPropertyA_Type();
   UnionPropertyA_Type toUnionPropertyA_Type = toNode->getUnionPropertyA_Type();

   toNode->copyChildren(fromNode);

   if (fromUnionPropertyA_Type == toUnionPropertyA_Type)
      {
      switch (fromUnionPropertyA_Type)
         {
         case HasSymbolReference:
#if !defined(ENABLE_RECREATE_WITH_COPY_VALID_PROPERTIES_COMPLETE)
            toNode->_unionPropertyA._symbolReference = fromNode->_unionPropertyA._symbolReference;
#else
            toNode->setSymbolReference(fromNode->getSymbolReference());
#endif
            break;
         case HasRegLoadStoreSymbolReference:
#if !defined(ENABLE_RECREATE_WITH_COPY_VALID_PROPERTIES_COMPLETE)
            toNode->_unionPropertyA._regLoadStoreSymbolReference = fromNode->_unionPropertyA._regLoadStoreSymbolReference;
#else
            toNode->setRegLoadStoreSymbolReference(fromNode->getRegLoadStoreSymbolReference());
#endif
            break;
         case HasBranchDestinationNode:
            toNode->setBranchDestination(fromNode->getBranchDestination());
            break;
         case HasBlock:
            toNode->setBlock(fromNode->getBlock());
            break;
         case HasArrayStride:
            toNode->setArrayStride(fromNode->getArrayStride());
            break;
         case HasPinningArrayPointer:
            toNode->setPinningArrayPointer(fromNode->getPinningArrayPointer());
            break;
         case HasDataType:
            toNode->setDataType(fromNode->getDataType());
            break;
         default:
            /* HasNoUnionPropertyA */
            break;
         }
      }

   TR::ILOpCodes fromOpCode = fromNode->getOpCodeValue();
   TR::ILOpCodes toOpCode = toNode->getOpCodeValue();

#if defined(ENABLE_RECREATE_WITH_COPY_VALID_PROPERTIES_COMPLETE)
   //  _unionBase._unionBase._constValue - presently incomplete
   bool fromConst = fromNode->getOpCode().isLoadConst() && (fromNode->getType().isIntegral() || fromNode->getType().isAddress());
   bool toConst = toNode->getOpCode().isLoadConst() && (toNode->getType().isIntegral() || toNode->getType().isAddress());

   if (fromConst && toConst)
      toNode->set64bitIntegralValue(fromNode->get64bitIntegralValue());

   // TODO: other properties that need to be completed - see below
#else
   // _unionBase
   if (toNode->_numChildren == 0 || !toNode->hasNodeExtension())
      {
      /* if (fromNode->getOpCode().getOpCodeValue() == TR::allocationFence)
         self()->setAllocation(NULL); */
         {
         // may be using other property that is unioned with _unionBase._children but do not know which one
         // so have to copy it forward irregardless of whether it is valid or not
         toNode->_unionBase = fromNode->_unionBase;
         }
      }

   // nameSymRef & callNodeData, if set on formNode, is already set for toNode (its looked up via globalIndex which is unchanged)
   // TODO: check whether properties are valid for the replacement node and remove if invalid

   if (toNode->getOpCode().isBranch() || toNode->getOpCode().isSwitch())
      toNode->_byteCodeInfo.setDoNotProfile(1);

   // _flags
   toNode->setFlags(fromNode->getFlags());  // do not clear hasNodeExtension

   // do not copy cannotOverflow
   if (toNode->_flags.testAny(cannotOverFlow) && toNode->getOpCode().isArithmetic())
      toNode->_flags.set(cannotOverFlow, false);
#endif

   // DONE:
   // OMR::Base
   //   _opCode
   //   _numChildren         - init list
   //   _globalIndex
   // OMR::Node
   //   _byteCodeInfo
   //   _unionPropertyB      - init list
   //   _globalRegisterInfo  - non-J9
   // _optAttributes       - init list

   // TODO:
   // OMR::Base
   // // this union still needs to be completed on consts, and _unionBase._unionBase._unionedWithChildren
   //  _unionBase:
   //    _children[NUM_DEFAULT_CHILDREN]  - cstr body
   //    _constValue        - init list, need to fix users of recreate to NOT set const, THEN do recreate
   //    _fpConstValue
   //    _dpConstValue
   //    _fpConstValueBits
   //    _extension         - done
   //       _data
   //       _flags   // for future use
   //       _numElems
   //    _unionedWithChildren
   //       union:
   //          _constData
   //          _globalRegisterInfo
   //          _offset
   //          _relocationInfo
   //          _caseInfo
   //          _monitorInfo
   //
   // CallNodeData
   //
   // OMR::Node
   //   _unionPropertyA      - needs to be completed for DAA
   //   _flags
   }

TR::Node *
OMR::Node::recreate(TR::Node *originalNode, TR::ILOpCodes op)
   {
   return TR::Node::recreateAndCopyValidPropertiesImpl(originalNode, op, NULL);
   }

TR::Node *
OMR::Node::recreateWithSymRef(TR::Node *originalNode, TR::ILOpCodes op, TR::SymbolReference *newSymRef)
   {
   return TR::Node::recreateAndCopyValidPropertiesImpl(originalNode, op, newSymRef);
   }

/**
 * OMR::Node::recreate
 *
 * creates a new node with a new op, based on the originalNode, reusing the memory occupied by originalNode and returning the
 * same address as it.
 *
 * It will have the same globalIndex, poolIndex, byteCodeInfo, and optAttributes as the originalNode, but properties from
 * the originalNode that are valid for the new node only will be copied. All other properties will be reset to their default values.
 */
TR::Node *
OMR::Node::recreateAndCopyValidPropertiesImpl(TR::Node *originalNode, TR::ILOpCodes op, TR::SymbolReference *newSymRef)
   {
   TR_ASSERT(originalNode != NULL, "trying to recreate node from a NULL originalNode.");
   if (originalNode->getOpCodeValue() == op)
      {
      if (!originalNode->hasSymbolReference() || newSymRef != originalNode->getSymbolReference())
         originalNode->_byteCodeInfo.setDoNotProfile(1);

      // need to at least set the new symbol reference on the node before returning
      if (newSymRef)
         originalNode->setSymbolReference(newSymRef);

      return originalNode;
      }

   TR::Compilation * comp = TR::comp();
//   // one may consider copy forwarding as a transformation of the old way of doing things to the new way
//   // "untransforming" means reverting to setOpCodeValue only. However reverting may in future be a
//   // bad thing to do, so disabling this.
//   if (!performTransformation(comp, "O^O RECREATE: recreating node %s %p; original opcode %s\n",
//         TR::ILOpCode(op).getName(), originalNode, originalNode->getOpCode().getName()))
//      {
//      originalNode->setOpCodeValue(op);
//      return originalNode;
//      }

   // need to copy the original node so the properties are available later to set on the recreated node
   uint16_t numChildren = originalNode->getNumChildren();
   TR::Node *originalNodeCopy = TR::Node::copy(originalNode, numChildren);
#if !defined(DISABLE_RECREATE_WITH_TRUE_CLEARING_OF_PROPERTIES)
   // we can't delete the node, otherwise we will potentially lose the poolIndex (and other properties), but we do need to deallocate
   // any objects it has created. This is one of them. It will be recreated if necessary on createInternal.
   // other properties such as nameSymRef & callNodeData, are associated with the _globalIndex
   // and are not freed and realloced.
   originalNode->freeExtensionIfExists();

   // this clears most properties, so copyValidProperties must be complete
   TR::Node *node = TR::Node::createInternal(0, op, originalNode->getNumChildren(), originalNode);
#else
   // this does not clear properties, but some properties may be invalid for the replacement node,
   // but it does not matter if copyValidProperties is complete, though it must be accurate
   TR::Node *node = originalNode;
   node->setOpCodeValue(op);
#endif

   if (newSymRef)
      {
      if (originalNodeCopy->hasSymbolReference() || originalNodeCopy->hasRegLoadStoreSymbolReference())
         originalNodeCopy->setSymbolReference(newSymRef);
      else if (node->hasSymbolReference() || node->hasRegLoadStoreSymbolReference())
         node->setSymbolReference(newSymRef);
      }

   // TODO: copyValidProperties is incomplete
   TR::Node::copyValidProperties(originalNodeCopy, node);
   originalNode->_byteCodeInfo.setDoNotProfile(1);

   // add originalNodeCopy back to the node pool
   comp->getNodePool().deallocate(originalNodeCopy);
   return node;
   }

TR::Node *
OMR::Node::createInternal(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *originalNode)
   {
   if (!originalNode)
      return new (TR::comp()->getNodePool()) TR::Node(originatingByteCodeNode, op, numChildren);
   else
      {
      // Recreate node from originalNode, ignore originatingByteCodeNode
      ncount_t globalIndex = originalNode->getGlobalIndex();
      vcount_t visitCount = originalNode->getVisitCount();
      scount_t localIndex = originalNode->getLocalIndex();
      rcount_t referenceCount = originalNode->getReferenceCount();
      TR::KnownObjectTable::Index knownObjectIndex = originalNode->getKnownObjectIndex();
      UnionA unionA = originalNode->_unionA;
      const TR_ByteCodeInfo byteCodeInfo = originalNode->getByteCodeInfo();  // copy bytecode info into temporary variable
      //TR::Node * node = new (TR::comp()->getNodePool(), poolIndex) TR::Node(0, op, numChildren);
      TR::Node *node = new ((void*)originalNode) TR::Node(0, op, numChildren);
      node->setGlobalIndex(globalIndex);
      node->setByteCodeInfo(byteCodeInfo);

      node->setVisitCount(visitCount);
      node->setLocalIndex(localIndex);
      node->setReferenceCount(referenceCount);
      node->setKnownObjectIndex(knownObjectIndex);
      node->_unionA = unionA;

      return node;
      }
   }



TR::Node *
OMR::Node::create(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   TR::Node * node = TR::Node::createInternal(originatingByteCodeNode, op, numChildren);
   return node;
   }

TR::Node *
OMR::Node::create(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::TreeTop * dest)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   TR::Node *node = TR::Node::createInternal(originatingByteCodeNode, op, numChildren);
   if (dest)
      node->setBranchDestination(dest);
   return node;
   }

TR::Node *
OMR::Node::create(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, int32_t intValue, TR::TreeTop * dest)
   {
   TR_ASSERT(op != TR::bconst && op != TR::sconst, "Invalid constructor for 8/16-bit constants");
   TR::Node * node = TR::Node::create(originatingByteCodeNode, op, numChildren, dest);
   if (op == TR::lconst)
      node->setLongInt(intValue);
   else
      node->setInt(intValue);

   return node;
   }

TR::Node *
OMR::Node::create(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node* first)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   TR::Node * node = TR::Node::createInternal(originatingByteCodeNode, op, numChildren);
   node->setAndIncChild(0, first);
   return node;
   }

TR::Node *
OMR::Node::create(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node* first, TR::Node *second)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   TR::Node * node = TR::Node::createInternal(originatingByteCodeNode, op, numChildren);
   node->setAndIncChild(0, first);
   node->setAndIncChild(1, second);
   return node;
   }

TR::Node *
OMR::Node::createWithSymRef(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::SymbolReference * symRef)
   {
   TR::Node * node = TR::Node::create(originatingByteCodeNode, op, numChildren);
   node->setSymbolReference(symRef);
   return node;
   }

TR::Node *
OMR::Node::createWithSymRef(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node* first, TR::SymbolReference * symRef)
   {
   TR::Node * node = TR::Node::create(originatingByteCodeNode, op, numChildren, first);
   node->setSymbolReference(symRef);
   return node;
   }

TR::Node *
OMR::Node::createOnStack(TR::Node * originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren)
   {
   return TR::Node::createInternal(originatingByteCodeNode, op, numChildren);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   TR::Node *node = TR::Node::createInternal(0, op, numChildren, originalNode);
   if (symRef != NULL || node->hasSymbolReference() || node->hasRegLoadStoreSymbolReference())
      {
      // don't attempt to set this if the symRef is NULL and the node isn't a symRef type; otherwise attempt to do so
      node->setSymbolReference(symRef);
      }
   return node;
   }



TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren)
   {
   return TR::Node::create(0, op, numChildren);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::TreeTop * dest)
   {
   return TR::Node::create(0, op, numChildren, dest);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, int32_t intValue, TR::TreeTop * dest)
   {
   return TR::Node::create(0, op, numChildren, intValue, dest);
   }

TR::Node *
OMR::Node::createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, uintptr_t extraChildren, TR::SymbolReference *symRef)
   {
   uint16_t numChildrenRoom = numChildren + (uint16_t) extraChildren;  // explictly cast to avoid warning
   TR::Node *node = TR::Node::createWithSymRef(op, numChildrenRoom, 1, first, symRef);
   node->_numChildren = numChildren;
   return node;
   }

TR::Node *
OMR::Node::createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, uintptr_t extraChildren, TR::SymbolReference *symRef)
   {
   TR::Node *node = TR::Node::createWithSymRef(op, numChildren, first, extraChildren, symRef); // explictly cast to avoid warning
   node->setChild(1, second);
   second->incReferenceCount();
   return node;
   }

TR::Node *
OMR::Node::createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::SymbolReference * symRef)
   {
   return TR::Node::createWithSymRef(0, op, numChildren, symRef);
   }

TR::Node *
OMR::Node::createWithSymRef(TR::ILOpCodes op, uint16_t numChildren, TR::SymbolReference * symRef, uintptr_t extraChildrenForFixup)
   {
   uint16_t numChildrenRoom = numChildren + (uint16_t) extraChildrenForFixup; // explictly cast to avoid warning
   TR::Node * r = TR::Node::createWithSymRef(0, op, numChildrenRoom, symRef);
   r->_numChildren = numChildren;
   return r;
   }



/**
 * These are defined for compilers that do not support variadic templates
 * Otherwise the definitions are in il/OMRNode_inlines.hpp
 */
#if defined(_MSC_VER) || defined(LINUXPPC)
TR::Node *
OMR::Node::recreateWithoutSymRef_va_args(TR::Node *originalNode, TR::ILOpCodes op,
                                              uint16_t numChildren, uint16_t numChildArgs,
                                              va_list &args)
   {
   TR_ASSERT(numChildArgs > 0, "must be called with at least one child, but numChildArgs = %d", numChildArgs);
   TR::Node *first = va_arg(args, TR::Node *);
   TR_ASSERT(first != NULL, "child %d must be non NULL", 0);
   TR::Node *node = TR::Node::createInternal(first, op, numChildren, originalNode);
   node->setAndIncChild(0, first);
   for (uint16_t i = 1; i < numChildArgs; i++)
      {
      TR::Node *child = va_arg(args, TR::Node *);
      TR_ASSERT(child != NULL, "child %d must be non NULL", i);
      node->setAndIncChild(i, child);
      }
   return node;
   }

TR::Node *
OMR::Node::createWithoutSymRef(TR::ILOpCodes op, uint16_t numChildren,
                                    uint16_t numChildArgs, ...)
   {
   va_list args;
   va_start(args, numChildArgs);
   TR::Node *node = TR::Node::recreateWithoutSymRef_va_args(0, op, numChildren, numChildArgs, args);
   va_end(args);
   return node;
   }

TR::Node *
OMR::Node::recreateWithoutSymRef(TR::Node *originalNode, TR::ILOpCodes op,
                                      uint16_t numChildren, uint16_t numChildArgs,
                                      ...)
   {
   va_list args;
   va_start(args, numChildArgs);
   TR::Node *node = TR::Node::recreateWithoutSymRef_va_args(originalNode, op, numChildren, numChildArgs, args);
   va_end(args);
   return node;
   }

TR::Node *
OMR::Node::recreateWithSymRefWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op,
                                   uint16_t numChildren, uint16_t numChildArgs,
                                   ...)
   {
   va_list args;
   va_start(args, numChildArgs);
   TR::Node *node = TR::Node::recreateWithoutSymRef_va_args(originalNode, op, numChildren, numChildArgs, args);
   TR::SymbolReference *symRef = va_arg(args, TR::SymbolReference *);
   node->setSymbolReference(symRef);
   va_end(args);
   return node;
   }


// only this variadic method is part of the external interface
TR::Node *
OMR::Node::createWithSymRef(TR::ILOpCodes op, uint16_t numChildren,
                                 uint16_t numChildArgs, ...)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   va_list args;
   va_start(args, numChildArgs);
   TR::Node *node = TR::Node::recreateWithoutSymRef_va_args(0, op, numChildren, numChildArgs, args);
   TR::SymbolReference *symRef = va_arg(args, TR::SymbolReference *);
   node->setSymbolReference(symRef);
   va_end(args);
   return node;
   }

#else

uint16_t
OMR::Node::addChildrenAndSymRef(uint16_t lastIndex, TR::SymbolReference *symRef)
   {
   self()->setSymbolReference(symRef);
   uint16_t numChildArgs = lastIndex;
   return numChildArgs;
   }

uint16_t
OMR::Node::addChildrenAndSymRef(uint16_t childIndex, TR::Node *child)
   {
   TR_ASSERT(child != NULL, "child[%d] must be non NULL", childIndex);
   self()->setAndIncChild(childIndex, child);
   uint16_t numChildArgs = childIndex + 1;
   return numChildArgs;
   }

#endif



TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 1, first);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 2, first, second);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 3, first, second, third);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 4, first, second, third, fourth);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 5, first, second, third, fourth, fifth);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 6, first, second, third, fourth, fifth, sixth);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 7, first, second, third, fourth, fifth, sixth, seventh);
   }

TR::Node *
OMR::Node::create(TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::Node *eighth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::createWithoutSymRef(op, numChildren, 8, first, second, third, fourth, fifth, sixth, seventh, eighth);
   }


TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 1, first);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 2, first, second);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 3, first, second, third);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 4, first, second, third, fourth);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 5, first, second, third, fourth, fifth);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 6, first, second, third, fourth, fifth, sixth);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 7, first, second, third, fourth, fifth, sixth, seventh);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::Node *eighth)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithoutSymRef(originalNode, op, numChildren, 8, first, second, third, fourth, fifth, sixth, seventh, eighth);
   }



TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 1, first, symRef);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 2, first, second, symRef);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 3, first, second, third, symRef);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 4, first, second, third, fourth, symRef);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 5, first, second, third, fourth, fifth, symRef);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 6, first, second, third, fourth, fifth, sixth, symRef);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 7, first, second, third, fourth, fifth, sixth, seventh, symRef);
   }

TR::Node *
OMR::Node::recreateWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op, uint16_t numChildren, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth, TR::Node *sixth, TR::Node *seventh, TR::Node *eighth, TR::SymbolReference * symRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return TR::Node::recreateWithSymRefWithoutProperties(originalNode, op, numChildren, 8, first, second, third, fourth, fifth, sixth, seventh, eighth, symRef);
   }



TR::Node *
OMR::Node::createWithRoomForOneMore(TR::ILOpCodes op, uint16_t numChildren, void * symbolRefOrBranchTarget, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, TR::Node *fifth)
   {
   TR::Node * node = TR::Node::createWithoutSymRef(op, numChildren, 2, first, second);
   node->addExtensionElements(1);
   if (symbolRefOrBranchTarget != NULL || node->hasSymbolReference() || node->hasBranchDestinationNode())
      {
      // don't attempt to set this if the symbolRefOrBranchTarget is NULL and the node isn't one of the valid types
      if (node->hasSymbolReference())
         node->setSymbolReference((TR::SymbolReference *) symbolRefOrBranchTarget);
      else if (node->hasBranchDestinationNode())
         node->setBranchDestination((TR::TreeTop *) symbolRefOrBranchTarget);
      else
         TR_ASSERT(false, "attempting to access _branchDestinationNode or _symbolReference field for node %s %p that does not have one of them", node->getOpCode().getName(), node);
      }

   if (third)
      node->setAndIncChild(2, third);
   if (fourth)
      node->setAndIncChild(3, fourth);
   if (fifth)
      node->setAndIncChild(4, fifth);
   node->setChild(numChildren, 0); // clear (numChildren+1)th child
   return node;
   }

TR::Node *
OMR::Node::createWithRoomForThree(TR::ILOpCodes op, TR::Node *first, TR::Node *second, void * symbolRefOrBranchTarget)
   {
   // So far extra 3rd child could be added to the following nodes: if, variableNewArray, arraystorechk or a node with a padding address
   // these nodes are created with room for three children
   TR::Node * node;
   if (op != TR::ArrayStoreCHK)
      {
      node = TR::Node::createWithoutSymRef(op, 2, 2, first, second);
      node->addExtensionElements(1);
      }
   else
      {
      if (second)
         {
         node = TR::Node::createWithoutSymRef(op, 2, 2, first, second);
         node->addExtensionElements(1);
         }
      else
         {
         node = TR::Node::createWithoutSymRef(op, 1, 1, first);
         node->addExtensionElements(2);
         }
      }

   if (symbolRefOrBranchTarget != NULL || node->hasSymbolReference() || node->hasBranchDestinationNode())
      {
      // don't attempt to set this if the symbolRefOrBranchTarget is NULL and the node isn't one of the valid types
      if (node->hasSymbolReference())
         node->setSymbolReference((TR::SymbolReference *) symbolRefOrBranchTarget);
      else if (node->hasBranchDestinationNode())
         node->setBranchDestination((TR::TreeTop *) symbolRefOrBranchTarget);
      else
         TR_ASSERT(false, "attempting to access _branchDestinationNode or _symbolReference field for node %s %p that does not have one of them", node->getOpCode().getName(), node);
      }
#ifdef J9_PROJECT_SPECIFIC
   TR_ASSERT(node->getOpCode().isIf() || (node->getOpCodeValue()==TR::variableNewArray)
          || node->getOpCode().canHavePaddingAddress() || node->getOpCodeValue()==TR::ArrayStoreCHK,
         "createWithRoomForThree should only be used to create an ifcmp, arraystorechk, variableNewArray node or a node with a padding address" );
#else
   TR_ASSERT(node->getOpCode().isIf() || (node->getOpCodeValue()==TR::variableNewArray),
         "createWithRoomForThree should only be used to create an ifcmp, arraystorechk, variableNewArray node or a node with a padding address" );
#endif
   return node;
   }

TR::Node *
OMR::Node::createWithRoomForFive(TR::ILOpCodes op, TR::Node *first, TR::Node *second, TR::Node *third, TR::Node *fourth, void *symbolRefOrBranchTarget)
   {
   TR_ASSERT((first != NULL) && (second != NULL) && (third != NULL) && (fourth != NULL), "must supply all four children");
   return TR::Node::createWithRoomForOneMore(op, 4, symbolRefOrBranchTarget, first, second, third, fourth);
   }



// An 'if' node is created with room for 3 children.
TR::Node *
OMR::Node::createif(TR::ILOpCodes op, TR::Node *first, TR::Node *second, TR::TreeTop * branchTarget)
   {
   // An 'if' node is created with room for 3 children.
   // The third child may be set to point at a GlRegDep node
   //
   return TR::Node::createWithRoomForThree(op, first, second, (void *) branchTarget);
   }

TR::Node *
OMR::Node::createbranch(TR::ILOpCodes op, TR::Node *first, TR::TreeTop * branchTarget)
   {
   // So far extra 3rd child could be added to the following nodes: if, newarray, anewarray
   // these nodes are created with room for three children

   TR::Node *node;
   if (first)
      {
      node = TR::Node::createWithoutSymRef(op, 1, 1, first);
      node->addExtensionElements(1);
      }
   else
      {
      node = TR::Node::createInternal(0, op, 0);
      node->addExtensionElements(2);
      }
   node->setBranchDestination(branchTarget);
   return node;
   }

TR::Node *
OMR::Node::createCase(TR::Node * originatingByteCodeNode, TR::TreeTop * destination, CASECONST_TYPE caseConstant)
   {
   TR::Node *caseNode = TR::Node::createInternal(originatingByteCodeNode, TR::Case, 0);
   caseNode->setBranchDestination(destination);
   caseNode->setCaseConstant(caseConstant);
   return caseNode;
   }



/**
 * May be called with first and/or second NULL, but if 3rd and higher children are specified they must be non-NULL
 */
TR::Node *
OMR::Node::createArrayOperation(TR::ILOpCodes arrayOp, TR::Node *first, TR::Node *second, TR::Node * third, TR::Node *fourth, TR::Node *fifth)
   {
   uint16_t numChildren = 3;
   if (fourth)
      numChildren = 4;
   if (fifth)
      numChildren = 5;

   return TR::Node::createWithRoomForOneMore(arrayOp, numChildren, 0, first, second, third, fourth, fifth);
   }

TR::Node *
OMR::Node::createArraycopy()
   {
   TR::Node * node = TR::Node::createInternal(0, TR::arraycopy, 3);
   node->addExtensionElements(1);
   node->setArrayCopyElementType(TR::Int8); // can be reset by consumer but don't leave this uninitialized
   node->setSymbolReference(TR::comp()->getSymRefTab()->findOrCreateArrayCopySymbol());

   return node;
   }

TR::Node *
OMR::Node::createArraycopy(TR::Node *first, TR::Node *second, TR::Node * third)
   {
   TR::Node * node = TR::Node::createArrayOperation(TR::arraycopy, first, second, third);
   node->setArrayCopyElementType(TR::Int8); // can be reset by consumer but don't leave this uninitialized
   node->setSymbolReference(TR::comp()->getSymRefTab()->findOrCreateArrayCopySymbol());
   return node;
   }

TR::Node *
OMR::Node::createArraycopy(TR::Node *first, TR::Node *second, TR::Node * third, TR::Node *fourth, TR::Node *fifth)
   {
   TR::Node * node = TR::Node::createArrayOperation(TR::arraycopy, first, second, third, fourth, fifth);
   node->setSymbolReference(TR::comp()->getSymRefTab()->findOrCreateArrayCopySymbol());
   return node;
   }


TR::Node *
OMR::Node::createPotentialOSRPointHelperCallInILGen(TR::Node* originatingByteCodeNode, int32_t osrInductionOffset)
   {
   TR::Compilation* comp = TR::comp();
   // The following are assertions to prevent misuses of this helper
   //
   TR_ASSERT(comp->getCurrentIlGenerator(), "This API must be called during ILGen");
   TR_ASSERT(!comp->isPeekingMethod(), "Can not generate the helper call during peeking");
   TR_ASSERT(comp->supportsInduceOSR(), "Can not create the helper without OSR support");

   TR::Node* callNode = TR::Node::createWithSymRef(originatingByteCodeNode, TR::call, 0, TR::comp()->getSymRefTab()->findOrCreatePotentialOSRPointHelperSymbolRef());
   callNode->setOSRInductionOffset(osrInductionOffset);

   // Node created outside of ilgen will have doNotProfile set, this results in OSR infrastructure believing it
   // can not OSR at this node. Reset this flag since a potentialOSRPointHelper is a safe OSR transition point
   //
   callNode->getByteCodeInfo().setDoNotProfile(0);
   return callNode;
   }

TR::Node *
OMR::Node::createOSRFearPointHelperCall(TR::Node* originatingByteCodeNode)
   {
   TR::Compilation* comp = TR::comp();

   TR_ASSERT(!comp->isPeekingMethod(), "Can not generate the helper call during peeking");
   TR_ASSERT(comp->supportsInduceOSR(), "Can not create the helper without OSR support");

   TR::Node* callNode = TR::Node::createWithSymRef(originatingByteCodeNode, TR::call, 0, TR::comp()->getSymRefTab()->findOrCreateOSRFearPointHelperSymbolRef());
   return callNode;
   }

TR::Node *
OMR::Node::createEAEscapeHelperCall(TR::Node* originatingByteCodeNode, int32_t numChildren)
   {
   TR::Compilation* comp = TR::comp();

   TR_ASSERT(!comp->isPeekingMethod(), "Can not generate the helper call during peeking");

   TR::Node* callNode = TR::Node::createWithSymRef(originatingByteCodeNode, TR::call, numChildren, TR::comp()->getSymRefTab()->findOrCreateEAEscapeHelperSymbolRef());
   return callNode;
   }

TR::Node *
OMR::Node::createLoad(TR::SymbolReference * symRef)
   {
   return TR::Node::createLoad(0, symRef);
   }

TR::Node *
OMR::Node::createLoad(TR::Node * originatingByteCodeNode, TR::SymbolReference * symRef)
   {
   TR::Node *load = TR::Node::createWithSymRef(originatingByteCodeNode, TR::comp()->il.opCodeForDirectLoad(symRef->getSymbol()->getDataType()), 0, symRef);
   if (symRef->getSymbol()->isParm())
      symRef->getSymbol()->getParmSymbol()->setReferencedParameter();
   return load;
   }



TR::Node *

OMR::Node::createStore(TR::SymbolReference * symRef, TR::Node * value)
   {
   return TR::Node::createStore(symRef, value,
                      TR::comp()->il.opCodeForDirectStore(symRef->getSymbol()->getDataType()),
                      0);
   }

TR::Node *
OMR::Node::createStore(TR::SymbolReference *symRef, TR::Node * value, TR::ILOpCodes op)
   {
   return TR::Node::createStore(symRef, value, op, 0);
   }

TR::Node *
OMR::Node::createStore(TR::SymbolReference * symRef, TR::Node * value, TR::ILOpCodes op, size_t size)
   {
   TR::Node *store = TR::Node::createWithSymRef(op, 1, 1, value, symRef);
   return store;
   }

TR::Node *
OMR::Node::createStore(TR::Node *originatingByteCodeNode, TR::SymbolReference * symRef, TR::Node * value)
   {
   TR::DataType type = symRef->getSymbol()->getDataType();
   TR::ILOpCodes op = TR::comp()->il.opCodeForDirectStore(type);
   return TR::Node::createWithSymRef(originatingByteCodeNode, op, 1, value, symRef);
   }

TR::Node *
OMR::Node::createRelative32BitFenceNode(void * relocationAddress)
   {
   return TR::Node::createRelative32BitFenceNode(0, relocationAddress);
   }

TR::Node *
OMR::Node::createRelative32BitFenceNode(TR::Node * originatingByteCodeNode, void * relocationAddress)
   {
   TR::Node * node = TR::Node::createInternal(originatingByteCodeNode, TR::exceptionRangeFence, 0);
   node->setRelocationType(TR_EntryRelative32Bit);
   node->setNumRelocations(1);
   node->setRelocationDestination(0, relocationAddress);
   return node;
   }



TR::Node *
OMR::Node::createAddressNode(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uintptr_t address, uint8_t precision)
   {
   TR::Node *node = TR::Node::create(originatingByteCodeNode, op, 0, 0);
   node->setAddress(address);
   return node;
   }

TR::Node *
OMR::Node::createAddressNode(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uintptr_t address)
   {
   TR::Node *node = TR::Node::create(originatingByteCodeNode, op, 0, 0);
   node->setAddress(address);
   return node;
   }



TR::Node *
OMR::Node::createAllocationFence(TR::Node *originatingByteCodeNode, TR::Node *fenceNode)
   {
   TR::Node *node = TR::Node::create(originatingByteCodeNode, TR::allocationFence, 0, 0);
   node->setAllocation(fenceNode);
   return node;
   }



TR::Node *
OMR::Node::bconst(TR::Node *originatingByteCodeNode, int8_t val)
   {
   TR::Node *r = TR::Node::create(originatingByteCodeNode, TR::bconst);
   r->setByte(val);
   return r;
   }

TR::Node *
OMR::Node::bconst(int8_t val)
   {
   return TR::Node::bconst(0, val);
   }




TR::Node *
OMR::Node::sconst(TR::Node *originatingByteCodeNode, int16_t val)
   {
   TR::Node *r = TR::Node::create(originatingByteCodeNode, TR::sconst);
   r->setShortInt(val);
   return r;
   }

TR::Node *
OMR::Node::sconst(int16_t val)
   {
   return TR::Node::sconst(0, val);
   }

TR::Node *
OMR::Node::iconst(TR::Node *originatingByteCodeNode, int32_t val)
   {
   return TR::Node::create(originatingByteCodeNode, TR::iconst, 0, val);
   }

TR::Node *
OMR::Node::iconst(int32_t val)
   {
   return TR::Node::iconst(0, val);
   }

TR::Node *
OMR::Node::lconst(TR::Node *originatingByteCodeNode, int64_t val)
   {
   TR::Node *r = TR::Node::create(originatingByteCodeNode, TR::lconst);
   r->setLongInt(val);
   return r;
   }

TR::Node *
OMR::Node::lconst(int64_t val)
   {
   return TR::Node::lconst(0, val);
   }



TR::Node *
OMR::Node::aconst(TR::Node *originatingByteCodeNode, uintptr_t val)
   {
   return TR::Node::createAddressNode(originatingByteCodeNode, TR::aconst, val);
   }

TR::Node *
OMR::Node::aconst(TR::Node *originatingByteCodeNode, uintptr_t val, uint8_t precision)
   {
   return TR::Node::createAddressNode(originatingByteCodeNode, TR::aconst, val, precision);
   }

TR::Node *
OMR::Node::aconst(uintptr_t val)
   {
   return TR::Node::aconst(0, val);
   }



TR::Node *
OMR::Node::createConstZeroValue(TR::Node *originatingByteCodeNode, TR::DataType dt)
   {
   TR::Node *constZero = NULL;
   switch (dt)
      {
      case TR::Int8:
         constZero = TR::Node::bconst(originatingByteCodeNode, 0);
         break;
      case TR::Int16:
         constZero = TR::Node::sconst(originatingByteCodeNode, 0);
         break;
      case TR::Int32:
         constZero = TR::Node::iconst(originatingByteCodeNode, 0);
         break;
      case TR::Int64:
         constZero = TR::Node::lconst(originatingByteCodeNode, 0);
         break;
      case TR::Address:
         constZero = TR::Node::aconst(originatingByteCodeNode, 0);
         break;
      case TR::Float:
         {
         constZero = TR::Node::create(originatingByteCodeNode, TR::fconst, 0);
         constZero->setFloatBits(FLOAT_POS_ZERO);
         break;
         }
      case TR::Double:
         {
         constZero = TR::Node::create(originatingByteCodeNode, TR::dconst, 0);
         constZero->setUnsignedLongInt(DOUBLE_POS_ZERO);
         break;
         }
      default:
         TR_ASSERT_SAFE_FATAL(false, "datatype %s not supported for createConstZeroValue\n", dt.toString());
      }
   return constZero;
   }

TR::Node *
OMR::Node::createConstOne(TR::Node *originatingByteCodeNode, TR::DataType dt)
   {
   TR::Node *constOne = NULL;

   switch (dt)
      {
      case TR::Int8:
         constOne = TR::Node::bconst(originatingByteCodeNode, 1);
         break;
      case TR::Int16:
         constOne = TR::Node::sconst(originatingByteCodeNode, 1);
         break;
      case TR::Int32:
         constOne = TR::Node::iconst(originatingByteCodeNode, 1);
         break;
      case TR::Int64:
         constOne = TR::Node::lconst(originatingByteCodeNode, 1);
         break;
      case TR::Address:
         constOne = TR::Node::aconst(originatingByteCodeNode, 1);
         break;
      case TR::Float:
         constOne = TR::Node::create(originatingByteCodeNode, TR::fconst, 0);
         constOne->setFloatBits(FLOAT_ONE);
         break;
      case TR::Double:
         constOne = TR::Node::create(originatingByteCodeNode, TR::dconst, 0);
         constOne->setUnsignedLongInt(DOUBLE_ONE);
         break;
      default:
         TR_ASSERT_SAFE_FATAL(false, "datatype %s not supported for createConstOne\n", dt.toString());
      }
   return constOne;
   }

TR::Node *
OMR::Node::createConstDead(TR::Node *originatingByteCodeNode, TR::DataType dt, intptr_t extraData)
   {
   TR::Node *result = NULL;
   const int8_t dead8 = (int8_t)((extraData << 4) | 0xD);
   const int16_t dead16 = (int16_t)((extraData << 8) | 0xDD);
   const int32_t dead32 = static_cast<const int32_t>((extraData << 16) | 0xdead);
   const int64_t dead64 = dead32;
   char buf[20];
   memset(buf, 0, 20);
   switch (dt)
      {
      case TR::Int8:
         result = TR::Node::bconst(originatingByteCodeNode, dead8);
         break;
      case TR::Int16:
         result = TR::Node::sconst(originatingByteCodeNode, dead16);
         break;
      case TR::Int32:
         result = TR::Node::iconst(originatingByteCodeNode, dead32);
         break;
      case TR::Int64:
         result = TR::Node::lconst(originatingByteCodeNode, dead64);
         break;
      case TR::Address:
         // Just because a reference is dead doesn't mean we can fill it with garbage
         result = TR::Node::aconst(originatingByteCodeNode, 0);
         break;
      case TR::Float:
         result = TR::Node::create(originatingByteCodeNode, TR::fconst, 0);
         result->setFloat(*(float*)(&dead32));
         break;
      case TR::Double:
         result = TR::Node::create(originatingByteCodeNode, TR::dconst, 0);
         result->setDouble(*(double*)(&dead64));
         break;
      default:
         TR_ASSERT_SAFE_FATAL(false, "datatype %s not supported for createConstDead\n", dt.toString());
      }
   return result;
   }



TR::Node*
OMR::Node::createCompressedRefsAnchor(TR::Node *firstChild)
   {
   TR::Node *heapBaseKonst = TR::Node::create(firstChild, TR::lconst, 0, 0);
   return TR::Node::create(TR::compressedRefs, 2, firstChild, heapBaseKonst);
   }



TR::Node *
OMR::Node::createAddConstantToAddress(TR::Node * addr, intptr_t value, TR::Node * parent)
   {
   TR::Node * ret = NULL;
   TR::Node *parentOfNewNode = parent ? parent : addr;

   if (value == 0) return addr;

   if (TR::comp()->target().is64Bit())
      {
      TR::Node *lconst = TR::Node::lconst(parentOfNewNode, (int64_t)value);
      ret = TR::Node::create(parentOfNewNode, TR::aladd, 2);
      ret->setAndIncChild(0, addr);
      ret->setAndIncChild(1, lconst);
      }
   else
      {
      ret = TR::Node::create(parentOfNewNode, TR::aiadd, 2);
      ret->setAndIncChild(0, addr);
      ret->setAndIncChild(1, TR::Node::create(parentOfNewNode, TR::iconst, 0, static_cast<int32_t>(value)));
      }

   ret->setIsInternalPointer(true);
   return ret;
   }

TR::Node *
OMR::Node::createLiteralPoolAddress(TR::Node *node, size_t offset)
   {
   TR::Node *address = NULL;
   return TR::Node::createAddConstantToAddress(NULL, offset);
   }


/**
 * Node constructors and create functions end
 */



/**
 * Misc public functions
 */

/**
 * @return true if searchNode is one of the children of this node, return false otherwise
 */
bool
OMR::Node::hasChild(TR::Node *searchNode)
   {
   if(searchNode == NULL)
      return false;

   for(uint32_t i = 0; i < self()->getNumChildren(); i++)
      {
      if(searchNode == self()->getChild(i))
         return true;
      }
      return false;
   }



void
OMR::Node::addChildren(TR::Node ** extraChildren, uint16_t num)
   {
   uint16_t oldNumChildren = self()->getNumChildren();
   uint16_t numElems;

   if (self()->hasNodeExtension())
      {
      numElems = _unionBase._extension.getNumElems() + num;
      self()->copyNodeExtension(_unionBase._extension.getExtensionPtr(), numElems, self()->sizeOfExtension());
      }
   else
      {
      numElems = NUM_DEFAULT_CHILDREN + num;
      self()->createNodeExtension(numElems);
      }

   self()->setNumChildren(oldNumChildren + num);
   for(uint16_t i = 0 ; i < num; i++)
      {
      self()->setAndIncChild(i+oldNumChildren, *(extraChildren + i));
      }
   }



TR::Node *
OMR::Node::setValueChild(TR::Node *child)
   {
   if (self()->getOpCode().isStoreIndirect())
      return self()->setChild(1, child);
   else
      return self()->setChild(0, child);
   }

TR::Node *
OMR::Node::setAndIncChild(int32_t c, TR::Node * p)
   {
   TR_ASSERT(self()->getOpCodeValue() != TR::BBStart || c != 1, "Can't use second child of TR::BBStart %p", this);
   if (p) p->incReferenceCount();
   return self()->setChild(c, p);
   }

TR::Node *
OMR::Node::setAndIncValueChild(TR::Node *child)
   {
   if (self()->getOpCode().isStoreIndirect())
      return self()->setAndIncChild(1, child);
   else
      return self()->setAndIncChild(0, child);
   }

TR::Node *
OMR::Node::getValueChild()
   {
   if (self()->getOpCode().isStoreIndirect())
      return self()->getSecondChild();
   else
      return self()->getFirstChild();
   }

TR::Node *
OMR::Node::getAndDecChild(int32_t c)
   {
   TR::Node * p = self()->getChild(c);
   p->decReferenceCount();
   return p;
   }



TR::Node *
OMR::Node::duplicateTreeWithCommoningImpl(CS2::HashTable<TR::Node*, TR::Node*, TR::Allocator> &nodeMapping)
   {
   CS2::HashIndex hashIndex = 0;

   if(nodeMapping.Locate(self(), hashIndex))
      return nodeMapping.DataAt(hashIndex);

   TR::Compilation *comp = TR::comp();

   int32_t numChildren = self()->getNumChildren();

   TR::ILOpCode opCode = self()->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

   TR::Node * to = new (comp->getNodePool()) TR::Node(self());

   to->setReferenceCount(0);

   nodeMapping.Add(self(),to);

   for (int32_t i = 0; i < self()->getNumChildren(); ++i)
      {
      to->setAndIncChild(i, self()->getChild(i)->duplicateTreeWithCommoningImpl(nodeMapping));
      }

   return to;
   }

TR::Node *
OMR::Node::duplicateTree(bool duplicateChildren)
   {
   TR::Node *unsafeNode = self()->duplicateTree_DEPRECATED(duplicateChildren);

   //Would like to enable this, but some optimizations (e,g. Field Privatization) seem to rely on the old behaviour of duplicateTree
   //TR::Node *safeNode = safeDuplicateTree(duplicateChildren);

   //traceMsg(TR::comp(), "--------------\n duplicateTree duplicating %p unsafeNode %p safeNode %p duplicateChildren = %d\n safeDuplicateTree:\n",this,unsafeNode,safeNode,duplicateChildren);
   //TR::comp()->getDebug()->print(TR::comp()->getOutFile(),safeNode);
   //traceMsg(TR::comp(), "original:\n");
   //TR::comp()->getDebug()->print(TR::comp()->getOutFile(),unsafeNode);
   //traceMsg(TR::comp(), "--------------\n");

   return unsafeNode;
   }

TR::Node *
OMR::Node::duplicateTreeForCodeMotion()
   {
   TR::Node *result = self()->duplicateTree(true);
   result->resetFlagsAndPropertiesForCodeMotion();
   return result;
   }

/**
 * Method to duplicate an entire subtree. Tries to do so 'safely' by maintaining a mapping of visited (and duplicated) nodes
 */
TR::Node *
OMR::Node::duplicateTreeWithCommoning(TR::Allocator allocator)
   {
   CS2::HashTable<TR::Node*, TR::Node*, TR::Allocator> nodeMapping(allocator);
   return self()->duplicateTreeWithCommoningImpl(nodeMapping);
   }

/**
 * Method to duplicate an entire subtree.  Resulting subtree has no reference
 * counts > 1 (so if the input tree was internally multiply connected, the
 * output tree will no longer be).
 * @deprecated
 */
TR::Node *
OMR::Node::duplicateTree_DEPRECATED(bool duplicateChildren)
   {
   TR::Compilation *comp = TR::comp();
   int32_t numChildren = _numChildren;

   TR::ILOpCode opCode = self()->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

   if (opCode.isIf() ||
      opCodeValue == TR::anewarray ||
      opCodeValue == TR::newarray ||
      opCodeValue == TR::arraycopy ||
      opCodeValue == TR::tstart ||
      opCodeValue == TR::arrayset)
      ++numChildren;

   TR::Node * newRoot = new (comp->getNodePool()) TR::Node(self());

   newRoot->setReferenceCount(0);

   if (newRoot->getOpCode().isStoreReg() || newRoot->getOpCode().isLoadReg())
      {
      if (newRoot->requiresRegisterPair(comp))
         {
         newRoot->setLowGlobalRegisterNumber(self()->getLowGlobalRegisterNumber());
         newRoot->setHighGlobalRegisterNumber(self()->getHighGlobalRegisterNumber());
         }
      else
         {
         newRoot->setGlobalRegisterNumber(self()->getGlobalRegisterNumber());
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   if (newRoot->getOpCode().isConversionWithFraction())
      {
      newRoot->setDecimalFraction(self()->getDecimalFraction());
      }
#endif

   for (int32_t i = 0; i < self()->getNumChildren(); i++)
      {
      TR::Node* child = self()->getChild(i);
      if (child)
         {
         TR::Node * newChild = duplicateChildren ? child->duplicateTree_DEPRECATED() : child;
         newRoot->setAndIncChild(i, newChild);
         }
      }

   self()->copyVirtualGuardInfoTo(newRoot, comp);

   return newRoot;
   }

/**
 * This is a way to mitigate the dangers of the duplicateTree API.
 * However, a "false" doesn't necessarily prove that duplication is the
 * right thing to do.  duplicateTree is an inherently risky API, and you
 * should know exactly what kind of tree you're duplicating and why.
 *
 * @note that executing a tree twice might not produce the same result.
 * This function is only indicating whether executing a tree twice is SAFE,
 * not whether it is correct.  That depends on the context, and it's the
 * caller's responsibility to ensure.
 */
bool
OMR::Node::isUnsafeToDuplicateAndExecuteAgain(int32_t *nodeVisitBudget)
   {
   TR::Compilation *comp = TR::comp();

   if ((*nodeVisitBudget) <= 0)
      return true; // Conservative answer because we can't affort to check all children

   (*nodeVisitBudget)--;

   if (self()->getOpCode().hasSymbolReference())
      {
      if (self()->getSymbolReference()->isUnresolved())
         {
         // Unresolved symrefs need to be evaluated in their original location,
         // under the ResolveCHK.
         //
         return true;
         }
      else if (self()->getOpCodeValue() == TR::loadaddr)
         {
         // Despite the symref, loadaddr is really like a constant, and is safe to re-execute.
         }
      else if (self()->getOpCode().isLoadVarDirect())
         {
         // Multiple loads of the same variable can return different values, which is particularly
         // problematic if the variable is an address to be dereferenced by the containing tree.
         // We'll aggressively call this "safe" here, but the caller must ensure that loads within
         // the duplicated tree can't return something unsafe.
         }
      else if (self()->getOpCode().isLoadIndirect())
         {
         switch (self()->getSymbolReference()->getReferenceNumber() - comp->getSymRefTab()->getNumHelperSymbols())
            {
            case TR::SymbolReferenceTable::vftSymbol:
               // We trust these
               break;
            default:
               // Generally can't rematerialize a field load
               return true;
            }
         }
      else
         {
         // Things with symrefs generally can have side-effects, so it is not safe to run them twice.
         //
         return true;
         }
      }
   else
      {
      // Things without symrefs generally have no side-effects, so it is safe to run them twice.
      }

   // If we've got this far, we only need to worry about children.
   //
   bool unsafeChildFound = false;
   for (int32_t i = 0; !unsafeChildFound && i < self()->getNumChildren(); i++)
      unsafeChildFound = self()->getChild(i)->isUnsafeToDuplicateAndExecuteAgain(nodeVisitBudget);
   return unsafeChildFound;
   }



/**
 * uncommonChild changes
 *     parent
 *        =>this
 *     to:
 *     parent
 *        clone(this)
 *           ...
 */
void
OMR::Node::uncommonChild(int32_t childIndex)
   {
   TR::Node *child = self()->getChild(childIndex);
   TR::Node *clone = child->uncommon();
   self()->setChild(childIndex, clone);
   }

/**
 * Creates clone, and adjusts reference counts of clone, node and its children
 * as though the node had been uncommoned from its parent.
 * @return the uncommoned clone.
 */
TR::Node *
OMR::Node::uncommon()
   {
   TR::Node *clone = TR::Node::copy(self());
   clone->setReferenceCount(1);
   self()->decReferenceCount();
   for (int32_t i = self()->getNumChildren() - 1; i >= 0; --i)
      {
      self()->getChild(i)->incReferenceCount();
      }
   return clone;
   }



/**
 * See if this node contains the given searchNode in its subtree.
 * We must be sensitive to visit counts in two ways:
 *     1) If the visit count is the same as the given visit count, we must not look
 *        into the subtree.
 *     2) We must maintain the visit counts of the nodes visited; to do this we use
 *        the next available visit count, i.e. (visitCount+1)
 */
bool
OMR::Node::containsNode(TR::Node *searchNode, vcount_t visitCount)
   {
   if (self()== searchNode)
      return true;

   vcount_t oldVisitCount = self()->getVisitCount();
   if (oldVisitCount == visitCount)
      return false;
   self()->setVisitCount(visitCount);

   for (int i = 0; i < self()->getNumChildren(); i++)
      {
      if (self()->getChild(i)->containsNode(searchNode, visitCount))
         return true;
      }

   return false;
   }

/**
 * Does this node have an unresolved symbol reference?
 */
bool
OMR::Node::hasUnresolvedSymbolReference()
   {
   return self()->getOpCode().hasSymbolReference() && self()->getSymbolReference()->isUnresolved();
   }

/**
 * Does this node have a volatile symbol reference by any chance?
 */
bool
OMR::Node::mightHaveVolatileSymbolReference()
   {
   if (self()->getOpCode().hasSymbolReference())
      return self()->getSymbolReference()->maybeVolatile();

   return false;
   }



/**
 * Is this node the 'this' pointer
 */
bool
OMR::Node::isThisPointer()
   {
   return self()->getOpCode().hasSymbolReference() && self()->getSymbolReference()->isThisPointer();
   }



/**
 * Whether this node is high part of a "dual", in DAG representation a dual
 * is a composite operator, made from a high order part and its adjunct operator
 * which is its third child. It returns true if the node has the form:
 *
 *    highOp
 *      firstChild
 *      secondChild
 *      adjunctOp
 *        pairFirstChild
 *        pairSecondChild
 *
 * and the opcodes for highOp/adjunctOp are lumulh/lmul, luaddh/ladd, or lusubh/lsub.
 */
bool
OMR::Node::isDualHigh()
   {
   if ((self()->getNumChildren() == 3) && self()->getChild(2))
      {
      TR::ILOpCodes pairOpValue = self()->getChild(2)->getOpCodeValue();
      if (((self()->getOpCodeValue() == TR::lumulh) && (pairOpValue == TR::lmul))
          || ((self()->getOpCodeValue() == TR::luaddh) && (pairOpValue == TR::ladd))
          || ((self()->getOpCodeValue() == TR::lusubh) && (pairOpValue == TR::lsub)))
         return true;
      }
   return false;
   }

/**
 * Whether this node is high part of a select subtract or addition, like a dual this
 * is a composite operator, made from a high order part and its adjunct operator
 * which is the first child of its third child. It returns true if the node has the form:
 *
 *    highOp
 *      firstChild
 *      secondChild
 *      computeCC
 *        adjunctOp
 *          pairFirstChild
 *          pairSecondChild
 *
 * and the opcodes for highOp/adjunctOp are luaddc/ladd, or lsubb/lsub.
 */
bool
OMR::Node::isSelectHigh()
   {
   if (((self()->getOpCodeValue() == TR::luaddc) || (self()->getOpCodeValue() == TR::lusubb))
       && (self()->getNumChildren() == 3) && self()->getChild(2)
       && (self()->getChild(2)->getNumChildren() == 1) && self()->getChild(2)->getFirstChild())
      {
      TR::ILOpCodes ccOpValue = self()->getChild(2)->getOpCodeValue();
      TR::ILOpCodes pairOpValue = self()->getChild(2)->getFirstChild()->getOpCodeValue();
      if ((ccOpValue == TR::computeCC) &&
          (((self()->getOpCodeValue() == TR::luaddc) && (pairOpValue == TR::ladd))
           || ((self()->getOpCodeValue() == TR::luaddc) && (pairOpValue == TR::ladd))
           || ((self()->getOpCodeValue() == TR::lusubb) && (pairOpValue == TR::lsub))))
         return true;
      }
   return false;
   }

/**
 * isTernaryHigh is now deprecated. Use isSelectHigh instead.
 */
bool
OMR::Node::isTernaryHigh()
   {
   return self()->isSelectHigh();
   }

/**
 * Whether this node is the high or low part of a "dual", in cyclic representation.
 * ie it represents a composite operator, together with its pair.
 * The node and its pair have each other as its third child, completing the cycle.
 * It returns true if the node has the form:
 *
 *    node
 *      firstChild
 *      secondChild
 *      pair
 *        pairFirstChild
 *        pairSecondChild
 *        ==> node
 */
bool
OMR::Node::isDualCyclic()
   {
   if (self()->getNumChildren() == 3)
      {
      TR::Node *pair = self()->getChild(2);
      if (pair && (pair->getNumChildren() == 3) && (pair->getChild(2) == self()))
         return true;
      }
   return false;
   }



bool
OMR::Node::isConstZeroBytes()
   {
   if (!self()->getOpCode().isLoadConst())
      return false;

   switch (self()->getDataType())
      {
      case TR::Int8:
         return self()->getByte() == 0;
      case TR::Int16:
         return self()->getShortInt() == 0;
      case TR::Int32:
         return self()->getInt() == 0;
      case TR::Int64:
         return self()->getLongInt() == 0;
      case TR::Address:
         return self()->getAddress() == 0;
      case TR::Float:
         return self()->getFloatBits() == 0;
      case TR::Double:
         return self()->getDoubleBits() == 0;
      default:
         TR_ASSERT(false, "Unrecognized const node %s can't be checked for zero bytes");
         return false;
      }
   }


bool
OMR::Node::isConstZeroValue()
   {
   if (!self()->getOpCode().isLoadConst())
      return false;

   switch (self()->getDataType())
      {
      case TR::Int8:
         return self()->getByte() == 0;
      case TR::Int16:
         return self()->getShortInt() == 0;
      case TR::Int32:
         return self()->getInt() == 0;
      case TR::Int64:
         return self()->getLongInt() == 0;
      case TR::Address:
         return self()->getAddress() == 0;
      case TR::Float:
         {
         TR::Compilation *comp = TR::comp();
         return self()->getFloatBits() == FLOAT_POS_ZERO;
         }
      case TR::Double:
         {
         TR::Compilation *comp = TR::comp();
         return self()->getDoubleBits() == DOUBLE_POS_ZERO;
         }
      default:
         TR_ASSERT(false, "Unrecognized constant node can't be checked for zero");
         return false;
      }
   }



static bool
refCanBeKilled(TR::Node *node, TR::Compilation *comp)
   {
   if (node->getOpCodeValue() == TR::loadaddr)
      {
      return false;
      }

   if (node->getOpCode().isLoadConst() && !node->anchorConstChildren())
      {
      return false;
      }

   if (node->getOpCode().isLoadReg())
      {
      return false;
      }

   return true;
   }

/**
 * The typical use is to allow a simple recursive decrement to be done vs a full evaluation (or anchoring)
 * of an address tree that turns out not to be required at tree evaluation (or tree simplifiation) time
 *
 * @return true if the tree contains no variable loads (as loads
 * could be potentially killed by intervening stores)
 */
bool
OMR::Node::safeToDoRecursiveDecrement()
   {
   TR::Compilation *comp = TR::comp();
   if (!refCanBeKilled(self(), comp))
      {
      return true;
      }
   else if (self()->getOpCode().isAdd() &&
            !refCanBeKilled(self()->getFirstChild(), comp) &&
            !refCanBeKilled(self()->getSecondChild(), comp))
      {
      return true;
      }
   else if (self()->getOpCode().isAdd() &&
            self()->getFirstChild()->getOpCode().isAdd() &&
            !refCanBeKilled(self()->getFirstChild()->getFirstChild(), comp) &&
            !refCanBeKilled(self()->getFirstChild()->getSecondChild(), comp) &&
            !refCanBeKilled(self()->getSecondChild(), comp))
      {
      // this pattern is created by localOffsetBucketing
      // aiadd
      //    aiadd
      //       aload cached_static
      //       iconst
      //    iconst
      return true;
      }
   else
      {
      return false;
      }
   }



bool
OMR::Node::canGCandReturn()
   {
   TR::Node * node;

   if (self()->getOpCode().isResolveCheck())
      {
      // A field load or store cannot GC and return, since GC for a field load or
      // store can only happen at run-time if the underlying object is null
      // (otherwise creation of the object will have caused the class to be
      // loaded and no GC will happen on this reference).
      // Since the underlying object must be null for GC to occur, an exception
      // will always be thrown.
      //
      node = self()->getFirstChild();
      if (node->getOpCode().isIndirect() && node->getOpCode().isLoadVarOrStore() &&
          node->getSymbolReference()->getSymbol()->isShadow() &&
          !node->getSymbolReference()->isFromLiteralPool())
         return false;
      if (node->getOpCodeValue() == TR::arraycopy)
         return false;
      return true;
      }

   // For a treetop or nullchk look at the child.
   //
   if (self()->getOpCodeValue() == TR::treetop || self()->getOpCode().isNullCheck())
      {
      node = self()->getFirstChild();
      if (node->getOpCode().isLoadVarOrStore())
         {
         // Loads and stores can only GC if resolution is going to occur, and
         // that is covered by TR_ResolveCheck nodes.
         //
         return false;
         }
      if (node->getOpCodeValue() == TR::arraycopy)
         return false;
      }
   else
      node = self();

   return node->getOpCode().hasSymbolReference() &&
          node->getSymbolReference()->canGCandReturn();
   }

bool
OMR::Node::canGCandExcept()
   {
   // Can't GC and go to an exception handler if there aren't any
   //
   // TODO - We must create a map anyway for these cases, since we must not merge
   // maps across them.
   // Find a better way of handling this.
   //
   /////if (compilation->getMethodSymbol()->isEHAwareMethod())
   /////   return false;

   // For a treetop look at the child.
   //
   TR::Node * node = (self()->getOpCodeValue() == TR::treetop) ? self()->getFirstChild() : self();

   return (node->getOpCode().isCheck() ||
           (node->getOpCode().hasSymbolReference() &&
            node->getSymbolReference()->canGCandExcept()));
   }

bool
OMR::Node::canCauseGC()
   {
   return self()->canGCandReturn() || self()->canGCandExcept();
   }

bool
OMR::Node::isGCSafePointWithSymRef()
   {
   return (self()->canGCandReturn() && self()->getOpCode().hasSymbolReference());
   }



bool
OMR::Node::dontEliminateStores(bool isForLocalDeadStore)
   {
   TR::Compilation * comp = TR::comp();
   if (self()->getSymbolReference()->getSymbol()->dontEliminateStores(comp, isForLocalDeadStore) ||
       (self()->getSymbol()->isAutoOrParm() &&
        self()->storedValueIsIrrelevant()))
      return true;

   return false;
   }



bool
OMR::Node::isNotCollected()
   {
   return !self()->computeIsCollectedReference();
   }


/**
 * \brief
 *    Check if the node is an internal pointer.
 *
 * \return
 *    Return true if the node is an internal pointer, false otherwise.
 *
 * \note
 *    This query currently only works on opcodes that can have a pinning array pointer,
 *    i.e. aiadd, aladd. Suppport for other opcodes can be added if necessary.
 */
bool
OMR::Node::computeIsInternalPointer()
   {
   TR_ASSERT(self()->getOpCode().hasPinningArrayPointer(), "Opcode %s is not supported, node is " POINTER_PRINTF_FORMAT, self()->getOpCode().getName(), self());
   return self()->computeIsCollectedReference();
   }

/**
 * \brief
 *    Check if the node is collected.
 *
 * \return
 *    Return true if the node is collected, false otherwise.
 *
 * \note
 *    This query traverses down the children of the node and try to find if the node originates
 *    from something collected, i.e. anything that is collected reference or internal pointer.
 */
bool
OMR::Node::computeIsCollectedReference()
   {
   TR::Compilation * comp = TR::comp();
   TR::NodeChecklist processedNodesCollected(comp);
   TR::NodeChecklist processedNodesNotCollected(comp);
   return (self()->computeIsCollectedReferenceImpl(processedNodesCollected, processedNodesNotCollected) != TR_no);
   }

static TR_YesNoMaybe
recordProcessedNodeResult(TR::Node *node, TR_YesNoMaybe collectedness, TR::NodeChecklist &processedNodesCollected, TR::NodeChecklist &processedNodesNotCollected)
   {
   switch (collectedness)
      {
      case TR_yes:
         processedNodesCollected.add(node);
         break;
      case TR_no:
         processedNodesNotCollected.add(node);
         break;
      case TR_maybe:
         processedNodesCollected.add(node);
         processedNodesNotCollected.add(node);
         break;
      default:
         TR_ASSERT_FATAL(false, "Invalid collectedness result for Node %p\n", node);
         break;
      }
   return collectedness;
   }

TR_YesNoMaybe
OMR::Node::computeIsCollectedReferenceImpl(TR::NodeChecklist &processedNodesCollected, TR::NodeChecklist &processedNodesNotCollected)
   {
   TR::Node *curNode = self();
   TR::Node *receiverNode = curNode;
   if (curNode->getOpCode().isTreeTop())
      return TR_no;

   // In order to prevent from walking the same node repeatedly when
   // one is referenced multiple times in a very deep tree structure
   // (e.g. select whose child is a select and so on),
   // Use 2 checklists to record following states:
   // -- The node is not contained in either collected or uncollected checklists - first time encounter, process the node
   //    and add it to the appropriate checklist(s) based on the result.
   // -- Node is seen in both checklitsts - previously processed with result of TR_maybe
   // -- Node is seen in processedNodesCollected - previously processed with result of TR_yes
   // -- Node is seen in processedNodesNotCollected - previously processed with result of TR_no
   bool selectSeenCollected = processedNodesCollected.contains(receiverNode);
   bool selectSeenNotCollected = processedNodesNotCollected.contains(receiverNode);
   if (selectSeenCollected && selectSeenNotCollected)
      return TR_maybe;
   else if (selectSeenCollected)
      return TR_yes;
   else if (selectSeenNotCollected)
      return TR_no;

   while (curNode)
      {
      if (curNode->isInternalPointer())
         return recordProcessedNodeResult(receiverNode, TR_yes, processedNodesCollected, processedNodesNotCollected);

      TR::ILOpCode op = curNode->getOpCode();
      TR::ILOpCodes opValue = curNode->getOpCodeValue();

      // If a language can handle a collected reference going via a non-address type,
      // then the logic would need to be augmented to handle that
      if (op.isConversion())
         return recordProcessedNodeResult(receiverNode, TR_no, processedNodesCollected, processedNodesNotCollected);

      if (op.getDataType() != TR::Address)
         return recordProcessedNodeResult(receiverNode, TR_no, processedNodesCollected, processedNodesNotCollected);
      // The following are all opcodes that are address type, non-TreeTop and non-conversion

      if (op.isAdd())
         {
         curNode = curNode->getFirstChild();
         continue;
         }

      if (op.isSelect())
         {
         TR_YesNoMaybe secondChildResult = curNode->getSecondChild()->computeIsCollectedReferenceImpl(processedNodesCollected, processedNodesNotCollected);
         if (TR_maybe == secondChildResult)
            {
            TR_YesNoMaybe thirdChildResult = curNode->getThirdChild()->computeIsCollectedReferenceImpl(processedNodesCollected, processedNodesNotCollected);
            return recordProcessedNodeResult(receiverNode, thirdChildResult, processedNodesCollected, processedNodesNotCollected);
            }
         else
            return recordProcessedNodeResult(receiverNode, secondChildResult, processedNodesCollected, processedNodesNotCollected);
         }

      // opcodes associated with a symref, we should
      // be able to tell its collectedness from its symref.
      if (op.isLoadVar() || op.isLoadAddr() || op.isLoadReg())
         {
         TR_ASSERT(curNode->hasSymbolReference() || curNode->hasRegLoadStoreSymbolReference(), "node " POINTER_PRINTF_FORMAT "(%s) should have symbol reference", curNode, op.getName());
         TR::Symbol *symbol = curNode->getSymbolReference()->getSymbol();
         // isCollectedReference() responds false to generic int shadows because their type
         // is int. However, address type generic int shadows refer to collected slots.
         if (opValue == TR::aloadi && symbol == TR::comp()->getSymRefTab()->findGenericIntShadowSymbol())
            return recordProcessedNodeResult(receiverNode, TR_yes, processedNodesCollected, processedNodesNotCollected);
         else
            return recordProcessedNodeResult(receiverNode, (symbol->isCollectedReference() ? TR_yes : TR_no),
                  processedNodesCollected, processedNodesNotCollected);
         }

      // Symbols for calls and news does not contain collectedness information.
      // Current implementation treats all object references collectable, and also
      // assumes that the return of an acall* is an object reference.
      if (op.isNew() || op.isCall() || opValue == TR::variableNew || opValue == TR::variableNewArray)
         return recordProcessedNodeResult(receiverNode, TR_yes, processedNodesCollected, processedNodesNotCollected);

      switch (opValue)
         {
         case TR::aconst:
            // aconst null can either be collected or uncollected. The collectedness of aconst
            // null can be determined by usedef analysis, which is too expensive for a very
            // basic query. Treating aconst null as a collected reference is always safe. The
            // only problem is: we might mark an aladd/aiadd on a temp whose value is null as
            // an internal pointer, thus creating a temp, pinning array and internal pointer
            // map for it.

            // Preserve the existing logic which is as follows:
            // true iff not under aladd and null,
            // if non-null, always false.
            // if under aladd, always false.

            // non-null constant return false.
            if (curNode->getAddress() != 0)
               return recordProcessedNodeResult(receiverNode, TR_no, processedNodesCollected, processedNodesNotCollected);
            else
               {
               // null constant under aladd, return false.
               // Null constant under select (i.e. we reached here via recursive calls), return maybe
               // to indicate need to check the other child.
               if (self() != curNode)
                  return recordProcessedNodeResult(receiverNode, TR_no, processedNodesCollected, processedNodesNotCollected);
               else
                  return recordProcessedNodeResult(receiverNode, TR_maybe, processedNodesCollected, processedNodesNotCollected);
               }
         default:
            TR_ASSERT(false, "Unsupported opcode %s on node " POINTER_PRINTF_FORMAT, op.getName(), curNode);
            return TR_no;
         }
      }
   return TR_no;
   }

bool
OMR::Node::addressPointsAtObject()
   {
   TR::Compilation * comp = TR::comp();
   TR_ASSERT(self()->getDataType() == TR::Address, "addressPointsAtObject should only be called for nodes whose data type is TR::Address; node is " POINTER_PRINTF_FORMAT, self());

   if (self()->getOpCodeValue() == TR::aconst)
      return false;

   if (self()->getOpCode().hasSymbolReference() &&
       (comp->getSymRefTab()->isVtableEntrySymbolRef(self()->getSymbolReference())
        ))
      return false;

   return true;
   }



bool
OMR::Node::collectSymbolReferencesInNode(TR_BitVector &symbolReferencesInNode, vcount_t visitCount)
   {
   int32_t i;

   // The visit count in the node must be maintained by this method.
   //
   vcount_t oldVisitCount = self()->getVisitCount();
   if (oldVisitCount == visitCount)
      return true;
   self()->setVisitCount(visitCount);

   // For all other subtrees collect all symbols that could be killed between
   // here and the next reference.
   //
   for (i = self()->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node * child = self()->getChild(i);
      child->collectSymbolReferencesInNode(symbolReferencesInNode, visitCount);
      }

   // Add this node's symbol reference to the set
   //
   if (self()->getOpCode().hasSymbolReference() && self()->getOpCode().isLoadVar())
      symbolReferencesInNode.set(self()->getSymbolReference()->getReferenceNumber());
   return true;
   }

/**
 * Decide whether it is safe to replace the next reference to this node with
 * a copy of the node, i.e. make sure it is not killed between this
 * reference and the next.
 * The treetop containing this reference is provided by the caller.
 * The current visit count must be maintained by this method, i.e. all nodes that
 * have the current visit count must retain it and all nodes that do not must not
 * get it.
 */
bool
OMR::Node::isSafeToReplaceNode(TR::TreeTop * curTreeTop)
   {
   TR::Compilation * c = TR::comp();
   // Collect all symbols that could be killed between here and the next reference
   //
   TR_BitVector symbolReferencesInNode(c->getSymRefCount(), c->trMemory(), stackAlloc);
   c->incVisitCount();
   vcount_t visitCount = c->getVisitCount();
   self()->collectSymbolReferencesInNode(symbolReferencesInNode, visitCount);

   // Now scan forwards through the trees looking for the next use and checking
   // to see if any symbols in the subtree are getting modified; if so it is not
   // safe to replace the node at its next use.
   //
   TR_BitVector temp(c->getSymRefCount(), c->trMemory(), stackAlloc);
   c->incVisitCount();
   for (TR::TreeTop *treeTop = curTreeTop->getNextTreeTop(); treeTop; treeTop = treeTop->getNextTreeTop())

      {
      TR::Node * node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart &&
          !node->getBlock()->isExtensionOfPreviousBlock())
         return true;

      if (node->containsNode(self(), c->getVisitCount()))
         return true;

      TR::SymbolReference *definingSymRef = NULL;
      if (node->getOpCode().isStore())
         {
         definingSymRef = node->getSymbolReference();
         if (symbolReferencesInNode.get(definingSymRef->getReferenceNumber()))
            return false;
         }
      else if (node->getOpCodeValue() == TR::treetop ||
               node->getOpCode().isResolveOrNullCheck())
         {
         TR::Node * child = node->getFirstChild();
         if (child->getOpCode().isStore())
            {
            definingSymRef = child->getSymbolReference();
            if (symbolReferencesInNode.get(definingSymRef->getReferenceNumber()))
               return false;
            }
         else if (child->getOpCode().isCall() ||
                  child->getOpCodeValue() == TR::monexit)
            definingSymRef = child->getSymbolReference();
         else if (node->getOpCode().isResolveCheck())
            definingSymRef = child->getSymbolReference();
         }

      // Check if the definition modifies any symbol in the subtree
      //
      if (definingSymRef &&
          definingSymRef->getUseDefAliases().containsAny(symbolReferencesInNode, c))
        return false;
      }

   return true;
   }



bool
OMR::Node::isl2aForCompressedArrayletLeafLoad()
   {
   if (self()->getOpCodeValue() != TR::l2a)
      return false;

   TR::Node *node = self()->getFirstChild();
   if (node->getOpCodeValue() == TR::lshl)
      node = node->getFirstChild();

   if (node->getOpCodeValue() != TR::iu2l)
      return false;

   node = node->getFirstChild();
   if (node->getOpCodeValue() != TR::iloadi)
      return false;
   if (!node->getOpCode().hasSymbolReference())
      return false;

   return (node->getSymbol()->isArrayletShadowSymbol());
   }



bool
OMR::Node::mayModifyValue(TR::SymbolReference * symRef)
   {
   // this routine doesn't use aliasing information so that it can be used before
   // aliasing information is generated.  It should be commoned with the code in isSafeToReplace
   // so that if the aliasing information is available it's used otherwise, it uses the existing
   // algorithm.
   //

   bool symbolIsUnresolved = false;
   TR::Node * node = self();
   if (self()->getOpCodeValue() == TR::treetop || self()->getOpCode().isResolveOrNullCheck())
      {
      if (self()->getOpCode().isResolveCheck())
         symbolIsUnresolved = true;
      node = self()->getFirstChild();
      }

   TR::Symbol * symbol = symRef->getSymbol();
   if (node->getOpCode().isCall() ||
       node->getOpCodeValue() == TR::monexit ||
       (node->getOpCode().hasSymbolReference() && node->getSymbol()->isVolatile()) ||
       symbolIsUnresolved)
      {
      // assume the call kills everything except for auto, parms and const statics
      //
      return !symbol->isAutoOrParm() && !(symbol->isStatic() && symbol->isConst()) && !symbol->isMethodMetaData();
      }

   if (node->getOpCode().isStore())
      {
      TR::SymbolReference * storeRef = node->getSymbolReference();
      TR::Symbol * storeSymbol = storeRef->getSymbol();

      if (symbol->isAuto())
         return storeSymbol->isAuto() && storeRef->getCPIndex() == symRef->getCPIndex();

      if (symbol->isParm())
         return storeSymbol->isParm() &&
                ((TR::ParameterSymbol *)symbol)->getParameterOffset() == ((TR::ParameterSymbol *)storeSymbol)->getParameterOffset();

      if (symbol->isStatic())
         {
         if (symbol->isConst())
            return false;

         if (symbol->isClassObject())
            return false;

         // check if both symbols refer to the same static field
         if (storeSymbol->isStatic())
            {
            if (symbol->getDataType() != storeSymbol->getDataType())
               return false;
            TR::Compilation * c = TR::comp();
            if (symRef->isUnresolved() || storeRef->isUnresolved())
               return TR::Compiler->cls.jitStaticsAreSame(c, storeRef->getOwningMethod(c), storeRef->getCPIndex(), symRef->getOwningMethod(c), symRef->getCPIndex());

            return ((TR::StaticSymbol *)symbol)->getStaticAddress() == ((TR::StaticSymbol *)storeSymbol)->getStaticAddress();
            }

         return false;
         }

      if (symbol->isShadow() && storeSymbol->isShadow())
         {
         if (symbol->getDataType() != storeSymbol->getDataType())
            return false;

         if (symRef->getCPIndex() == -1) //its an array
            return storeRef->getCPIndex() == -1;

         if (storeRef->getCPIndex() == - 1)
            return false; //its an array

         TR::Compilation * c = TR::comp();
         return TR::Compiler->cls.jitFieldsAreSame(c, storeRef->getOwningMethod(c), storeRef->getCPIndex(), symRef->getOwningMethod(c), symRef->getCPIndex(), false);
         }
      }

   return false;
   }



bool
OMR::Node::performsVolatileAccess(vcount_t visitCount)
   {
   bool foundVolatile=false;

   self()->setVisitCount(visitCount);

   if (self()->getOpCode().hasSymbolReference())
      {
      TR::Symbol *sym = self()->getSymbol();
      if (sym != NULL && sym->isVolatile())
         foundVolatile=true;
      }

   for (int32_t c = 0; c < self()->getNumChildren(); c++)
      {
      TR::Node * child = self()->getChild(c);
      if (child->getVisitCount() != visitCount)
         foundVolatile |= child->performsVolatileAccess(visitCount);
      }

   return foundVolatile;
   }



bool
OMR::Node::uses64BitGPRs()
   {
   TR::Compilation * c = TR::comp();
   return self()->getOpCode().isLong()  || c->cg()->usesImplicit64BitGPRs(self());
   }



bool
OMR::Node::isRematerializable(TR::Node *parent, bool onlyConsiderOpCode)
   {
   TR::Compilation *comp = TR::comp();
   TR::ILOpCode &opCode = self()->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

#ifdef J9_PROJECT_SPECIFIC
   if (opCode.getDataType() == TR::PackedDecimal)
      return false;

   if (opCode.getOpCodeValue() == TR::pd2i || opCode.getOpCodeValue() == TR::pd2iOverflow ||
       opCode.getOpCodeValue() == TR::pd2l || opCode.getOpCodeValue() == TR::pd2lOverflow)
      return false;
#endif

   if ((opCodeValue == TR::lloadi) &&
       self()->isBigDecimalLoad())
      return true;

   // rematerialize loadaddr
   if ((opCodeValue == TR::loadaddr) && !self()->getSymbolReference()->isUnresolved() && !self()->getSymbol()->isLocalObject())
      return true;

   if (parent &&
       (parent->getOpCodeValue() == TR::Prefetch) &&
       (opCodeValue == TR::aloadi))
      return true;

   // Only running this opt to rematerialize big decimal loads
   // since register pressure is not a concern
   //
   if (!comp->cg()->doRematerialization())
       return false;

   if (opCode.isArrayLength() || opCode.isConversion())
      {
      if (onlyConsiderOpCode ||
          (((self()->getFirstChild()->getFutureUseCount() & 0x7fff) > 0) &&
           (self()->getFirstChild()->getReferenceCount() > 1)))
         return true;
      }

   //if (opCodeValue == TR::aloadi)
   //   {
   //   if (node->getSymbolReference()->getSymbol()->isClassObject())
   //    return true;
   //   }

   if (opCode.isAdd() || opCode.isSub() || opCode.isMul() || opCode.isLeftShift() || opCode.isRightShift() || opCode.isAnd() || opCode.isOr() || opCode.isXor())
      {
      //if (opCodeValue == TR::aiadd)
      //return false;

      bool firstChildLive = false;
      bool secondChildLive = false;
      if (self()->getSecondChild()->getOpCode().isLoadConst())
         secondChildLive = true;

      if (!onlyConsiderOpCode)
         {
         if (((self()->getSecondChild()->getFutureUseCount() & 0x7fff) > 0) &&
             (self()->getSecondChild()->getReferenceCount() > 1))
            secondChildLive = true;

         if (((self()->getFirstChild()->getFutureUseCount() & 0x7fff) > 0) &&
             (self()->getFirstChild()->getReferenceCount() > 1))
            firstChildLive = true;

         if (firstChildLive && secondChildLive)
            return true;
         }
      else
         return true;
      }

    if (opCode.isLoadVarDirect())
      {
      TR::Symbol *sym = self()->getSymbolReference()->getSymbol();
      if (sym->isAutoOrParm())
         return true;
      }

   return false;
   }

bool
OMR::Node::isDoNotPropagateNode()
   {
   // Generally, "do not propagate" nodes are those that

   // 1) are impure (where their evaluation does not yield the same value at different tree points)
   if (self()->getOpCode().isCall())
      {
      return true;
      }

   if (self()->hasUnresolvedSymbolReference())
      {
      return true;
      }

   // inspiration from ValuePropagation.cpp:
   switch (self()->getOpCodeValue())
      {
      case TR::New:
      case TR::newarray:
      case TR::anewarray:
      case TR::multianewarray:
      case TR::NULLCHK:
      case TR::ZEROCHK:
      case TR::ResolveCHK:
      case TR::ResolveAndNULLCHK:
      case TR::DIVCHK:
#ifdef J9_PROJECT_SPECIFIC
      case TR::BCDCHK:
#endif
         {
         return true;
         }
      default:
         break;
      }

   return false;
   }

bool
OMR::Node::containsDoNotPropagateNode(vcount_t vc)
   {
   if (self()->getVisitCount() == vc)
      {
      return false;
      }

   self()->setVisitCount(vc);

   if (self()->isDoNotPropagateNode())
      {
      return true;
      }

   for (auto i = 0; i < self()->getNumChildren(); i++)
      {
      if (self()->getChild(i)->containsDoNotPropagateNode(vc))
         {
         return true;
         }
      }

   return false;
   }



bool
OMR::Node::anchorConstChildren()
   {
   TR_ASSERT(self()->getOpCode().isLoadConst(), "constNeedsAnchor only valid for const nodes\n");
   if (self()->getNumChildren() > 0) // aggregate and BCD const nodes have an address child
      {
      TR_ASSERT(self()->getNumChildren() == 1, "expecting BCD/Aggr const node %p to have 1 child and not %d children\n", self(), self()->getNumChildren());
      if (self()->getFirstChild()->safeToDoRecursiveDecrement())
         return false;
      else
         return true;
      }
   else
      {
      return false;
      }
   }



bool
OMR::Node::isFloatToFixedConversion()
   {
   if (self()->getOpCode().isConversion() &&
       (self()->getType().isIntegral()
#ifdef J9_PROJECT_SPECIFIC
        || self()->getType().isBCD()
#endif
        ) &&
       self()->getFirstChild()->getType().isFloatingPoint())
      {
      return true;
      }
   else
      {
      return false;
      }
   }



bool
OMR::Node::isZeroExtension()
   {
   if (self()->getOpCode().isZeroExtension())
      return true;

   //a2x extensions
   if (self()->getOpCode().isConversion() &&
       self()->getType().isIntegral() &&
       self()->getFirstChild()->getType().isAddress() &&
       self()->getSize() > self()->getFirstChild()->getSize())
      return true;

   //x2a extensions
   if (self()->getOpCode().isConversion() &&
       self()->getType().isAddress() &&
       self()->getSize() > self()->getFirstChild()->getSize())
      return true;

   return false;
   }



bool
OMR::Node::isPureCall()
   {
   TR_ASSERT(self()->getOpCode().isCall(), "Opcode must be a call");
   return ((TR::MethodSymbol*)self()->getSymbolReference()->getSymbol())->isPureFunction();
   }



TR_YesNoMaybe
OMR::Node::hasBeenRun()
   {
   if (self()->getOpCode().hasSymbolReference())
      return self()->getSymbolReference()->hasBeenAccessedAtRuntime();
   else
      return TR_maybe;
   }



bool
OMR::Node::isClassUnloadingConst()
   {
   TR::Compilation * c = TR::comp();
   TR::Node * constNode;

   if ((self()->getOpCodeValue() == TR::aloadi) && (self()->getSymbolReference()->isLiteralPoolAddress()))
      constNode= (TR::Node *) self()->getSymbolReference()->getOffset();
   else if (self()->getOpCodeValue() == TR::aconst)
      constNode=self();
   else return false;

   if ((((constNode->isClassPointerConstant()) &&
         !TR::Compiler->cls.sameClassLoaders(c, (TR_OpaqueClassBlock *) constNode->getAddress(),
                                    c->getCurrentMethod()->classOfMethod())) ||
        ((constNode->isMethodPointerConstant()) && !c->compileRelocatableCode() &&
         !TR::Compiler->cls.sameClassLoaders(c, c->fe()->createResolvedMethod(
                                                                  c->trMemory(),
                                                                  (TR_OpaqueMethodBlock *) constNode->getAddress(),
                                                                  c->getCurrentMethod())->classOfMethod(),
                                    c->getCurrentMethod()->classOfMethod()))))

      return true;
   else
      return false;
   }



TR_OpaqueClassBlock *
OMR::Node::getMonitorClass(TR_ResolvedMethod * vmMethod)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::Compilation * c = TR::comp();
   TR::Node * object = self()->getOpCodeValue() != TR::tstart ? self()->getFirstChild() : self()->getThirdChild();
   if (self()->isStaticMonitor())
      {
      return c->getClassClassPointer();
      }

   if (self()->hasMonitorClassInNode())
       return self()->getMonitorClassInNode();

   if (object->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference * symRef = object->getSymbolReference();
      if (symRef->isThisPointer())
         {
         TR_OpaqueClassBlock * clazz = (TR_OpaqueClassBlock *)vmMethod->classOfMethod();
         if (TR::Compiler->cls.classDepthOf(clazz) == 0)
            return 0;
         return clazz;
         }
      if (object->getOpCodeValue() == TR::loadaddr && !symRef->isUnresolved()  && !symRef->getSymbol()->isStatic())
         {
          return (TR_OpaqueClassBlock *)symRef->getSymbol()->castToLocalObjectSymbol()->getClassSymbolReference()->getSymbol()->castToStaticSymbol()->getStaticAddress();
         }
      }
#endif
   return 0;
   }

TR_OpaqueClassBlock *
OMR::Node::getMonitorClassInNode()
   {
   TR_ASSERT(self()->hasMonitorClassInNode(),"Node %p does not have monitor class in Node");
   TR::Node  * classz = (self()->getOpCodeValue() != TR::tstart) ? self()->getChild(1) : self()->getChild(4);
   return (TR_OpaqueClassBlock *)classz;
   }

void
OMR::Node::setMonitorClassInNode(TR_OpaqueClassBlock * classz)
   {
   if (self()->getOpCodeValue() == TR::tstart)
       self()->setChild(4,(TR::Node *)classz);
   else
       self()->setChild(1,(TR::Node *)classz);
   if (classz != NULL)
      self()->setHasMonitorClassInNode(true);
   else
      self()->setHasMonitorClassInNode(false);
   }



/**
 * Given that this node is a NULLCHK or ResolveAndNULLCHK, find the reference
 * that is being checked.
 * Addendum: Node could also be a checkcastAndNULLCHK to make things more
 * interesting
 */
TR::Node *
OMR::Node::getNullCheckReference()
   {
   TR_ASSERT((self()->getOpCode().isNullCheck() || (self()->getOpCodeValue() == TR::checkcastAndNULLCHK) || (self()->getOpCodeValue() == TR::ZEROCHK)), "Node::getNullCheckReference, expected NULLCHK node instead of " POINTER_PRINTF_FORMAT, self());
   TR::Node *firstChild = self()->getFirstChild();

   if ((self()->getOpCodeValue() == TR::checkcastAndNULLCHK) || (self()->getOpCodeValue() == TR::ZEROCHK))
      return firstChild;

   if (firstChild->getNumChildren() == 0)
      return 0;

   // Find the reference to be checked.

   // For indirect calls the this pointer may not be the first argument
   //
   if (firstChild->getOpCode().isCall())
      return firstChild->getChild(firstChild->getFirstArgumentIndex());

   //  We must cast array size from long to int on 64bit platforms.
   //  Hence, the reference is one level lower in the IL tree
   //
   if (firstChild->getOpCodeValue() == TR::l2i)
      return firstChild->getFirstChild()->getFirstChild();

   // For all other nodes it is the first grandchild.
   //
   return firstChild->getFirstChild();
   }

void
OMR::Node::setNullCheckReference(TR::Node *refNode)
   {
   TR_ASSERT((self()->getOpCode().isNullCheck() || (self()->getOpCodeValue() == TR::checkcastAndNULLCHK)), "Node::getNullCheckReference, expected NULLCHK node instead of " POINTER_PRINTF_FORMAT, self());
   TR::Node *firstChild = self()->getFirstChild();

   if (self()->getOpCodeValue() == TR::checkcastAndNULLCHK)
      {
      self()->setAndIncChild(0, refNode);
      return;
      }

   if (firstChild->getNumChildren() == 0)
      {
      TR_ASSERT(0, "Null check node " POINTER_PRINTF_FORMAT " does not have null check reference", self());
      }

   // Find the reference to be checked.

   // For indirect calls the this pointer may not be the first argument
   //
   if (firstChild->getOpCode().isCall())
      {
      firstChild->setAndIncChild(firstChild->getFirstArgumentIndex(), refNode);
      return;
      }

   //  We must cast array size from long to int on 64bit platforms.
   //  Hence, the reference is one level lower in the IL tree
   //
   if (firstChild->getOpCodeValue() == TR::l2i)
      {
      firstChild->getFirstChild()->setAndIncChild(0, refNode);
      return;
      }

   // For all other nodes it is the first grandchild.
   //
   firstChild->setAndIncChild(0, refNode);
   return;
   }



int32_t
OMR::Node::getFirstArgumentIndex()
   {
   TR_ASSERT(self()->getOpCode().isCall(), "getFirstArgumentIndex called for non-call node " POINTER_PRINTF_FORMAT, self());

   if (self()->getOpCode().isIndirect())
      return 1;

   if (self()->getOpCodeValue() == TR::ZEROCHK)
      return 1; // First argument is the int to compare with zero, not an arg to the helper

   return 0;
   }

uint32_t
OMR::Node::getSize()
   {
   if (self()->getType().isAggregate())
      {
      return 0;
      }
   else if (_opCode.hasNoDataType())
      {
      // opcodes without inherent datatypes will be 0 sized; get actual size from their derived datatypes
      return TR::DataType::getSize(self()->getDataType());
      }

   return _opCode.getSize();
   }

uint32_t
OMR::Node::getRoundedSize()
   {
   int32_t roundedSize = (self()->getSize()+3)&(~3);
   return roundedSize ? roundedSize : 4;
   }



int32_t
OMR::Node::getMaxIntegerPrecision()
   {
   switch (self()->getDataType())
      {
      case TR::Int8:
         return TR::getMaxSignedPrecision<TR::Int8>();
         break;
      case TR::Int16:
         return TR::getMaxSignedPrecision<TR::Int16>();
      case TR::Int32:
         return TR::getMaxSignedPrecision<TR::Int32>();
      case TR::Int64:
         return TR::getMaxSignedPrecision<TR::Int64>();
      default:
         break;
      }

   TR_ASSERT(false, "getMaxIntegerPrecision must only be called for an integral value\n");
   return -1;
   }



uint32_t
OMR::Node::getNumberOfSlots()
   {
   return TR::Symbol::convertTypeToNumberOfSlots(self()->getDataType());
   }



uint16_t
OMR::Node::getCaseIndexUpperBound()
   {
   uint16_t n = _numChildren;
   while (n > 2 && !self()->getChild(n-1)->getOpCode().isCase())
      n--;
   return n;
   }



TR::Node *
OMR::Node::getStoreNode()
   {
   if (self()->getOpCode().isStore())
      return self();
   if (self()->getNumChildren() > 0 && self()->getFirstChild()->getOpCode().isStore())
      return self()->getFirstChild();
   return NULL;
   }



TR::TreeTop *
OMR::Node::getVirtualCallTreeForGuard()
   {
   TR::Node *guard = self();
   TR::TreeTop * callTree = NULL;
   TR::Compilation *comp = TR::comp();
   int32_t guardInlinedSiteIndex = guard->getInlinedSiteIndex();

   if (guardInlinedSiteIndex < 0)
      {
      // EscapeAnalysis can create a guard with index -1
      // in that case, return conservative result
      return NULL;
      }

   int32_t guardCallerIndex = comp->getInlinedCallSite(guardInlinedSiteIndex)._byteCodeInfo.getCallerIndex();
   uint32_t guardCallerByteCodeIndex = comp->getInlinedCallSite(guardInlinedSiteIndex)._byteCodeInfo.getByteCodeIndex();

   while (1)
      {
      // Call node is not necessarily the first real tree top in the call block
      // there may be stores/loads introduced by the global register allocator,
      // or stores to privitized inliner arguments.
      //
      // A call can be indirect or direct(if localVP de-virtualized it).
      // To make sure it's the right call we compare guard's caller index with
      // call's inlined site index.
      //
      callTree = guard->getBranchDestination()->getNextRealTreeTop();
      TR::Node * callNode = callTree->getNode();

      while (callTree->getNode()->getOpCodeValue() != TR::BBEnd)
         {
         if ((!callNode->getOpCode().isCall()) &&
             (callNode->getNumChildren() > 0))
            callNode = callTree->getNode()->getFirstChild();

         if (callNode->getOpCode().isCall() ||
             callTree->getNode()->getOpCodeValue() == TR::Goto)
            break;

         callTree = callTree->getNextRealTreeTop();
         callNode = callTree->getNode();
         }

      if (callTree->getNode()->getOpCodeValue() == TR::Goto)
         {
         guard = callTree->getNode();
         continue;
         }
      else if (!callNode->getOpCode().isCall() ||
               callNode->getInlinedSiteIndex() != guardCallerIndex ||
               callNode->getByteCodeIndex() != guardCallerByteCodeIndex ||
               !callNode->isTheVirtualCallNodeForAGuardedInlinedCall())
         {
         return NULL;
         }
      else
         {
         break;
         }
      }

   return callTree;
   }

TR::Node *
OMR::Node::getVirtualCallNodeForGuard()
   {
   TR::TreeTop *tree = self()->getVirtualCallTreeForGuard();
   if (tree) return tree->getNode()->getFirstChild();
   return 0;
   }



TR_OpaqueMethodBlock*
OMR::Node::getOwningMethod()
   {
   TR::Compilation * comp = TR::comp();
   TR_OpaqueMethodBlock *method = NULL;

   if (self()->getInlinedSiteIndex() >= 0)
      {
      TR_InlinedCallSite &ics = comp->getInlinedCallSite(self()->getInlinedSiteIndex());
      method = comp->fe()->getInlinedCallSiteMethod(&ics);
      }
   else
      {
      method = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getPersistentIdentifier());
      }

   return method;
   }



/** \brief
 *  Used to get the ram method from the Bytecode Info.
 *
 *  \param *comp
 *  _compilation of type TR::Compilation
 *
 *  @return
 *  Returns the ram method (TR_OpaqueMethodBlock) from function call getOwningMethod(TR::Compilation *comp, TR_ByteCodeInfo &bcInfo)
 */
TR_OpaqueMethodBlock*
OMR::Node::getOwningMethod(TR::Compilation *comp)
   {
   return TR::Node::getOwningMethod(comp, self()->getByteCodeInfo());
   }



/** \brief
 *  Used to get the ram method from the Bytecode Info.
 *
 *  \param *comp
 *  _compilation of type TR::Compilation
 *
 *  \param &bcInfo
 *  _byteCodeInfo of a node of type TR::Node
 *
 *  @return
 *  Returns the ram method (TR_OpaqueMethodBlock)
 */
TR_OpaqueMethodBlock*
OMR::Node::getOwningMethod(TR::Compilation *comp, TR_ByteCodeInfo &bcInfo)
   {
   TR_OpaqueMethodBlock *method = NULL;
   if (0 <= bcInfo.getCallerIndex())
      method = comp->getInlinedCallSite(bcInfo.getCallerIndex())._methodInfo;
   else
      method = comp->getCurrentMethod()->getPersistentIdentifier();

   return method;
   }



void *
OMR::Node::getAOTMethod()
   {
   TR::Compilation * c = TR::comp();
   int32_t index = self()->getInlinedSiteIndex();
   if (index == -1)
      {
      return c->getCurrentMethod();
      }
   else
      {
      return c->getInlinedResolvedMethod(index);
      }
   }



const char *
OMR::Node::getTypeSignature(int32_t & len, TR_AllocationKind allocKind)
   {
   // TODO: IMPLEMENT
   return 0;
   }



/**
 * 'Value' of the node can change, affecting analysis results in the optimizer such as value number
 * This gives affected node (self) a new unique value number.
 */
void
OMR::Node::notifyChangeToValueOfNode()
   {
   if (auto comp = TR::comp())
      if (auto opt = comp->getOptimizer())
         if (auto vnInfo = opt->getValueNumberInfo())
            vnInfo->setUniqueValueNumber(self());
   }



void
OMR::Node::reverseBranch(TR::TreeTop * newDest)
   {
   TR::ILOpCodes reverseOp = self()->getOpCode().getOpCodeForReverseBranch();
   self()->setOpCodeValue(reverseOp);
   self()->setBranchDestination(newDest);
   }



void
OMR::Node::devirtualizeCall(TR::TreeTop *treeTop)
   {
   TR::Compilation *comp = TR::comp();
   TR_ASSERT(self()->getOpCode().isCall(), "assertion failure");
   TR::ResolvedMethodSymbol *methodSymbol = self()->getSymbol()->castToResolvedMethodSymbol();

   if (self()->getOpCode().isCallIndirect())
      {
      self()->setOpCodeValue(methodSymbol->getMethod()->directCallOpCode());
      int32_t numChildren = self()->getNumChildren();
      self()->getChild(0)->recursivelyDecReferenceCount();

      for (int32_t i = 1; i < numChildren; i = ++i)
         self()->setChild(i - 1, self()->getChild(i));

      self()->setNumChildren(numChildren - 1);
      }
   }



/**
 * This function is called from gatherAllNodesWhichMightKillCondCode, and the
 * assumption is that we are calling it recursively on all the nodes. If the children are
 * evaluated in a register, this function will return true if the evaluation of this node
 * might kill the condition code.
 */
bool
OMR::Node::nodeMightKillCondCode()
   {
   TR::Compilation *comp = TR::comp();
#ifdef TR_HOST_S390
   TR::ILOpCode& opcode = self()->getOpCode();

   // simple loads/stores won't kill the CC
   if (opcode.isStoreReg() && (self()->getSize() == 4 || self()->getSize() == 8) )
      return false;

   // neither will BBStart/BBEnd nodes
   if (opcode.getOpCodeValue() == TR::BBStart || opcode.getOpCodeValue() == TR::BBEnd)
      return false;

   // treetops don't do anything (we already checked the children in the for loop)
   if (self()->getOpCodeValue() == TR::treetop)
      return false;

   // simple loads/stores won't kill the CC
   if ((opcode.isLoadReg() || opcode.isStoreReg() || opcode.isLoadDirect() || opcode.isStoreDirect())
        && (self()->getSize() == 4 || self()->getSize() == 8) )
      return false;

   // this conversion will be a no-op -> CC is not killed unless the child kills it
   if (self()->isUnneededConversion() || (opcode.isConversion() && (self()->getSize() == self()->getFirstChild()->getSize())))
      return false;

   // the rest were determined empirically not to kill the CC
   if (((opcode.isLoad() || opcode.isStore() || opcode.isLoadReg() || opcode.isStoreDirectOrReg() || opcode.isLoadAddr())
          && (self()->getSize() == 4 || self()->getSize() == 8 || self()->getSize() == 2 ||
          self()->getSize() == 1 || self()->getSize() == 0 ) ))
      return false;

   if (opcode.isArrayRef() && self()->getSecondChild()->getOpCode().isLoadConst() &&
            self()->getSecondChild()->get64bitIntegralValue() > 0 && self()->getSecondChild()->get64bitIntegralValue() < 4096) // address increment can use LA or fold into MemoryReference
      return false;
#endif

   return true;
   }

void
OMR::Node::gatherAllNodesWhichMightKillCondCode(vcount_t vc, TR::list< TR::Node *> & nodesWhichKillCondCode)
   {
   if (self()->getVisitCount() == vc)
      return;

   self()->setVisitCount(vc);

   if (self()->nodeMightKillCondCode())
      {
      nodesWhichKillCondCode.push_back(self());
      }

   for (uint16_t i = 0; i < self()->getNumChildren(); i++)
      {
      self()->getChild(i)->gatherAllNodesWhichMightKillCondCode(vc, nodesWhichKillCondCode);
      }
   }



TR::TreeTop *
OMR::Node::extractTheNullCheck(TR::TreeTop * prevTreeTop)
   {
   TR::Compilation * comp = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::NULLCHK || self()->getOpCodeValue() == TR::ResolveAndNULLCHK,
          "extractTheNullCheck can only be called for a null check node; node is " POINTER_PRINTF_FORMAT, self());

   TR::Node * passThru = TR::Node::create(TR::PassThrough, 1, self()->getNullCheckReference());
   TR::Node * newNullCheckNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passThru, self()->getSymbolReference());

   if (self()->getOpCodeValue() == TR::NULLCHK)
      {
      self()->setOpCodeValue(TR::treetop);
      //setSymbolReference(0);
      }
   else
      {
      self()->setOpCodeValue(TR::ResolveCHK);
      self()->setSymbolReference(comp->getSymRefTab()->findOrCreateResolveCheckSymbolRef(0));
      }
   self()->setNumChildren(1);

   return TR::TreeTop::create(comp, prevTreeTop, newNullCheckNode);
   }



int32_t
OMR::Node::countNumberOfNodesInSubtree(vcount_t visitCount)
   {
   if (self()->getVisitCount() == visitCount)
      return 0;

   self()->setVisitCount(visitCount);
   int32_t numNodes = 1;
   int32_t i;
   // for compressedreferences, ignore the anchor node
   //
   if (self()->getOpCode().isAnchor())
      {
      numNodes--;
      numNodes += self()->getChild(0)->countNumberOfNodesInSubtree(visitCount);
      return numNodes;
      }
   for (i=0;i<self()->getNumChildren();i++)
      {
      numNodes += self()->getChild(i)->countNumberOfNodesInSubtree(visitCount);
      }

   return numNodes;
   }



uint32_t
OMR::Node::exceptionsRaised()
   {
   // For a treetop look at the child.
   //
   uint32_t possibleExceptions = 0;
   TR::Node * node = self();

   if (node->getOpCodeValue() == TR::treetop)
      node = node->getFirstChild();
   else if (node->getOpCode().isResolveOrNullCheck())
      {
      if (node->getOpCode().isResolveCheck())
         possibleExceptions |= TR::Block:: CanCatchResolveCheck;
      if (node->getOpCode().isNullCheck())
         possibleExceptions |= TR::Block:: CanCatchNullCheck;
      node = node->getFirstChild();
      }

   // Shortcut if there are no more exceptions that can be raised
   //
   if (!node->getOpCode().canRaiseException())
      return possibleExceptions;

   switch (node->getOpCodeValue())
      {
      case TR::ZEROCHK:
         possibleExceptions |= TR::Block:: CanCatchEverything;
      break;
      case TR::DIVCHK:
         possibleExceptions |= TR::Block:: CanCatchDivCheck;
         break;
      case TR::BNDCHK:
      case TR::BNDCHKwithSpineCHK:
      case TR::ArrayCopyBNDCHK:
         possibleExceptions |= TR::Block:: CanCatchBoundCheck;
         break;
      case TR::ArrayStoreCHK:
         possibleExceptions |= TR::Block:: CanCatchArrayStoreCheck;
         break;
      case TR::ArrayCHK:
         possibleExceptions |= TR::Block:: CanCatchArrayStoreCheck;
         break;
      case TR::arraycopy:
         possibleExceptions |= TR::Block:: CanCatchBoundCheck;
         possibleExceptions |= TR::Block:: CanCatchArrayStoreCheck;
         break;
      case TR::arrayset: // does not throw any exceptions
         break;
      case TR::arraytranslate: // does not throw any exceptions
         break;
      case TR::arraytranslateAndTest:
         // does not throw any exceptions if not an arrayTRT
         if (node->isArrayTRT())
            possibleExceptions |= TR::Block:: CanCatchBoundCheck;
         break;
      case TR::arraycmp: // does not throw any exceptions
      case TR::arraycmplen:
         break;
      case TR::checkcast:
         possibleExceptions |= TR::Block:: CanCatchCheckCast;
         break;
      case TR::checkcastAndNULLCHK:
         possibleExceptions |= TR::Block:: CanCatchCheckCast;
         possibleExceptions |= TR::Block:: CanCatchNullCheck;
         break;
      case TR::New:
         possibleExceptions |= TR::Block:: CanCatchNew;
         break;
      case TR::newvalue:
         possibleExceptions |= TR::Block:: CanCatchNewvalue;
         break;
      case TR::newarray:
      case TR::anewarray:
      case TR::multianewarray:
         possibleExceptions |= TR::Block:: CanCatchArrayNew;
         break;
      case TR::monexit:
         possibleExceptions |= TR::Block:: CanCatchMonitorExit;
         break;
      case TR::monexitfence:
         possibleExceptions |= TR::Block:: CanCatchMonitorExit;
         break;
      case TR::athrow:
         possibleExceptions |= TR::Block:: CanCatchUserThrows;
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::BCDCHK:
         possibleExceptions |= TR::Block:: CanCatchUserThrows;
         break;
#endif
      default:
       if (node->getOpCode().isCall() && !node->isOSRFearPointHelperCall())
            {
            if (!((TR::MethodSymbol*)node->getSymbolReference()->getSymbol())->functionCallDoesNotYieldOSR())
               possibleExceptions |= TR::Block::CanCatchOSR;
            if (!node->isPureCall() && node->getSymbolReference()->canGCandExcept())
               possibleExceptions |= TR::Block::CanCatchUserThrows;
            }
         break;
      }

   return possibleExceptions;
   }



TR::Node *
OMR::Node::skipConversions()
   {
   TR::Node * node = self();
   if (self()->getNumChildren() != 1)
      return node;

   while (node->getOpCode().isConversion() && node->getOpCode().isStrictWidenConversion())
      node = node->getFirstChild();

   return node;
   }



TR::Node *
OMR::Node::createLongIfNeeded()
   {
   TR::Compilation * comp = TR::comp();
   bool usingAladd = comp->target().is64Bit();
   TR::Node *retNode = NULL;

   if (usingAladd)
      {
      if (self()->getOpCode().isLoadConst())
         {
         TR::Node *constNode = TR::Node::create(self(), TR::lconst);
         if (self()->getType().isInt32())
            constNode->setLongInt((int64_t)self()->getInt());
         else
            constNode->setLongInt(self()->getLongInt());
         retNode = constNode;
         }
      else if (self()->getType().isInt32())
         {
         TR::Node *i2lNode = TR::Node::create(TR::i2l, 1, self());
         retNode = i2lNode;
         }
      else
         {
         retNode = self();
         }
      }
   else
      {
      retNode = self();
      }

   return retNode;
   }



/**
 * @return the top (first) inserted tree or null
 */
TR::TreeTop *
OMR::Node::createStoresForVar(TR::SymbolReference * &nodeRef, TR::TreeTop *insertBefore, bool simpleRef)
   {
   TR::Compilation * comp = TR::comp();
   TR::TreeTop *storeTree = NULL;
   if ((self()->getReferenceCount() == 1) &&
       self()->getOpCode().hasSymbolReference() &&
       self()->getSymbolReference()->getSymbol()->isAuto())
      {
      nodeRef = self()->getSymbolReference();
      return NULL;
      }
   else if (!self()->getOpCode().isRef() ||
            simpleRef)
      {
      nodeRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), self()->getDataType(), false);

      if (self()->isNotCollected())
         nodeRef->getSymbol()->setNotCollected();
      TR::Node* storeNode = TR::Node::createStore(nodeRef, self());
      storeTree = TR::TreeTop::create(comp, storeNode);
      insertBefore->insertBefore(storeTree);
      return storeTree;
      }

   //Only refs
   TR::TreeTop *origInsertBefore = insertBefore;
   TR::SymbolReference *newArrayRef = NULL;
   TR::TreeTop *newStoreTree = NULL;

   bool isInternalPointer = false;
   if ((self()->hasPinningArrayPointer() &&
        self()->computeIsInternalPointer()) ||
       (self()->getOpCode().isLoadVarDirect() &&
        self()->getSymbolReference()->getSymbol()->isAuto() &&
        self()->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer()))
      isInternalPointer = true;

   bool storesNeedToBeCreated = true;

   if (self()->isNotCollected())
      {
      nodeRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR::Address);
      nodeRef->getSymbol()->setNotCollected();
      TR::Node* storeNode = TR::Node::createStore(nodeRef, self());
      storeTree = TR::TreeTop::create(comp, storeNode);
      insertBefore->insertBefore(storeTree);
      isInternalPointer = false;
      storesNeedToBeCreated = false;
      }

   if (isInternalPointer &&
       self()->getOpCode().isArrayRef() &&
       (comp->getSymRefTab()->getNumInternalPointers() >= (comp->maxInternalPointers()/2) ||
        comp->cg()->supportsComplexAddressing()) &&
       (self()->getReferenceCount() == 1))
      {
      storesNeedToBeCreated = false;
      TR::Node *firstChild = self()->getFirstChild();
      TR::Node *secondChild = self()->getSecondChild();
      TR::Node *arrayLoadNode = NULL;
      TR::Node *intOrLongNode = NULL;


      if (!firstChild->getOpCode().isArrayRef() &&
          !firstChild->isInternalPointer()  /* &&
             (!firstChild->getOpCode().isLoadVarDirect() ||
              !firstChild->getSymbolReference()->getSymbol()->isAuto()) */)
         {
         TR::SymbolReference *newArrayRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR::Address);

         TR::Node *newStore = TR::Node::createStore(newArrayRef, firstChild);
         TR::TreeTop *newStoreTree = TR::TreeTop::create(comp, newStore);
         insertBefore = origInsertBefore->insertBefore(newStoreTree);

         arrayLoadNode = TR::Node::createLoad(firstChild, newArrayRef);
         }
      else
         storesNeedToBeCreated = true;

      if (!storesNeedToBeCreated)
         {
         if (!secondChild->getOpCode().isLoadConst()  /* &&
            (!secondChild->getOpCode().isLoadVarDirect() ||
            !secondChild->getSymbolReference()->getSymbol()->isAuto()) */)
            {
            TR::SymbolReference *newIntOrLongRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), secondChild->getDataType());

            TR::Node *newStoreIntOrLong = TR::Node::createStore(newIntOrLongRef, secondChild);
            TR::TreeTop *newStoreIntOrLongTree = TR::TreeTop::create(comp, newStoreIntOrLong);
            insertBefore = origInsertBefore->insertBefore(newStoreIntOrLongTree);

            intOrLongNode = TR::Node::createLoad(secondChild, newIntOrLongRef);
            }
         else
            intOrLongNode = secondChild;

         self()->setAndIncChild(0, arrayLoadNode);
         self()->setAndIncChild(1, intOrLongNode);
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         }
      }

   if (storesNeedToBeCreated)
      {
      nodeRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR::Address, isInternalPointer);
      TR::Node* storeNode = TR::Node::createStore(nodeRef, self());

      if (self()->hasPinningArrayPointer() &&
          self()->computeIsInternalPointer())
         self()->setIsInternalPointer(true);

      TR::Node *child = NULL;

      if (isInternalPointer)
         {
         TR::AutomaticSymbol *pinningArray = NULL;
         if (self()->getOpCode().isArrayRef())
            {
            child = self()->getFirstChild();
            if (child->isInternalPointer())
               pinningArray = child->getPinningArrayPointer();
            else
               {
               while (child->getOpCode().isArrayRef())
                  child = child->getFirstChild();

               if (child->getOpCode().isLoadVarDirect() &&
                   child->getSymbolReference()->getSymbol()->isAuto())
                  {
                  if (child->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer())
                     pinningArray = child->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer();
                  else
                     {
                     pinningArray = child->getSymbolReference()->getSymbol()->castToAutoSymbol();
                     pinningArray->setPinningArrayPointer();
                     }
                  }
               else
                  {
                  newArrayRef = comp->getSymRefTab()->
                     createTemporary(comp->getMethodSymbol(), TR::Address);

                  TR::Node *newStore = TR::Node::createStore(newArrayRef, child);
                  newStoreTree = TR::TreeTop::create(comp, newStore);
                  newArrayRef->getSymbol()->setPinningArrayPointer();
                  pinningArray = newArrayRef->getSymbol()->castToAutoSymbol();
                  }
               }
            }
         else
            pinningArray = self()->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer();

         nodeRef->getSymbol()->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArray);
         if (self()->isInternalPointer())
            self()->setPinningArrayPointer(pinningArray);
         }

      storeTree = TR::TreeTop::create(comp, storeNode);
      insertBefore = insertBefore->insertBefore(storeTree);

      if (newStoreTree)
         {
         insertBefore = insertBefore->insertBefore(newStoreTree);
         }
      }

   return insertBefore;
   }



// used for debugging purposes only
// Note that for node with refCount > 1, printed node may NOT be the first referenced node despite not being marked with "==>"
void
OMR::Node::printFullSubtree()
   {
   TR::Compilation *comp = TR::comp();
   TR_BitVector nodeChecklistBeforeDump(comp->getNodeCount(), comp->trMemory(), stackAlloc, notGrowable);
   comp->getDebug()->saveNodeChecklist(nodeChecklistBeforeDump);
   comp->getDebug()->clearNodeChecklist();
   comp->getDebug()->print(comp->getOutFile(), self(), 2, true);
   comp->getDebug()->restoreNodeChecklist(nodeChecklistBeforeDump);
   }



const char *
OMR::Node::getName(TR_Debug *debug)
   {
   return debug ? debug->getName(self()) : "(unknown node)";
   }



bool
OMR::Node::isReferenceArrayCopy()
   {
   auto NUM_CHILDREN_REFERENCE_ARRAYCOPY = 5;
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   return ((self()->getNumChildren() == NUM_CHILDREN_REFERENCE_ARRAYCOPY) ? true : false);
   }

bool
OMR::Node::chkReferenceArrayCopy()
   {
   return self()->getOpCodeValue() == TR::arraycopy && self()->isReferenceArrayCopy();
   }

// OMR::Node::setOpCodeValue is now private, and deprecated. Use OMR::Node::recreate instead.
TR::ILOpCodes
OMR::Node::setOpCodeValue(TR::ILOpCodes op)
   {
   _opCode.setOpCodeValue(op);
   if (self()->hasDataType())
      self()->setDataType(TR::NoType);

   return op;
   }

bool
OMR::Node::isExtendedTo32BitAtSource()
   {
   return self()->isSignExtendedTo32BitAtSource() || self()->isZeroExtendedTo32BitAtSource();
   }

bool
OMR::Node::isExtendedTo64BitAtSource()
   {
   return self()->isSignExtendedTo64BitAtSource() || self()->isZeroExtendedTo64BitAtSource();
   }

bool
OMR::Node::isSignExtendedAtSource()
   {
   return self()->isSignExtendedTo32BitAtSource() || self()->isSignExtendedTo64BitAtSource();
   }

bool
OMR::Node::isZeroExtendedAtSource()
   {
   return self()->isZeroExtendedTo32BitAtSource() || self()->isZeroExtendedTo64BitAtSource();
   }

bool
OMR::Node::isSignExtendedTo32BitAtSource()
   {
   return self()->getOpCode().isLoadVar() && _flags.testAny(signExtendTo32BitAtSource);
   }

bool
OMR::Node::isSignExtendedTo64BitAtSource()
   {
   return self()->getOpCode().isLoadVar() && _flags.testAny(signExtendTo64BitAtSource);
   }

void
OMR::Node::setSignExtendTo32BitAtSource(bool v)
   {
   TR::Compilation* c = TR::comp();
   TR_ASSERT(self()->getOpCode().isLoadVar(), "Can only call this for variable loads\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting signExtendedTo32BitAtSource flag on node %p to %d\n", self(), v))
      {
      _flags.set(signExtendTo32BitAtSource, v);
      }
   }

void
OMR::Node::setSignExtendTo64BitAtSource(bool v)
   {
   TR::Compilation* c = TR::comp();
   TR_ASSERT(self()->getOpCode().isLoadVar(), "Can only call this for variable loads\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting signExtendedTo64BitAtSource flag on node %p to %d\n", self(), v))
      {
      _flags.set(signExtendTo64BitAtSource, v);
      }
   }

bool
OMR::Node::isZeroExtendedTo32BitAtSource()
   {
   return self()->getOpCode().isLoadVar() && _flags.testAny(zeroExtendTo32BitAtSource);
   }

bool
OMR::Node::isZeroExtendedTo64BitAtSource()
   {
   return self()->getOpCode().isLoadVar() && _flags.testAny(zeroExtendTo64BitAtSource);
   }

void
OMR::Node::setZeroExtendTo32BitAtSource(bool v)
   {
   TR::Compilation* c = TR::comp();
   TR_ASSERT(self()->getOpCode().isLoadVar(), "Can only call this for variable loads\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting zeroExtendTo32BitAtSource flag on node %p to %d\n", self(), v))
      {
      _flags.set(zeroExtendTo32BitAtSource, v);
      }
   }

void
OMR::Node::setZeroExtendTo64BitAtSource(bool v)
   {
   TR::Compilation* c = TR::comp();
   TR_ASSERT(self()->getOpCode().isLoadVar(), "Can only call this for variable loads\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting zeroExtendTo64BitAtSource flag on node %p to %d\n", self(), v))
      {
      _flags.set(zeroExtendTo64BitAtSource, v);
      }
   }

/**
 * Misc public functions
 */



/**
 * Node field functions
 */

void
OMR::Node::setFlags(flags32_t f)
   {
   bool nodeExtensionExists = self()->hasNodeExtension();
#ifdef J9_PROJECT_SPECIFIC
   if (self()->getType().isBCD() && f.isClear())
      self()->resetDecimalSignFlags();
#endif
   _flags = f;
   self()->setHasNodeExtension(nodeExtensionExists);
   }

void
OMR::Node::setByteCodeInfo(const TR_ByteCodeInfo &bcInfo)
   {
   _byteCodeInfo = bcInfo;
   if (!TR::comp()->getCurrentIlGenerator())
      _byteCodeInfo.setDoNotProfile(1);
   }

void
OMR::Node::copyByteCodeInfo(TR::Node * from)
   {
   _byteCodeInfo = from->_byteCodeInfo;
   if (!TR::comp()->getCurrentIlGenerator())
      _byteCodeInfo.setDoNotProfile(1);
   }

uint32_t
OMR::Node::getByteCodeIndex()
   {
   return _byteCodeInfo.getByteCodeIndex();
   }

void
OMR::Node::setByteCodeIndex(uint32_t i)
   {
   _byteCodeInfo.setByteCodeIndex(i);
   }

int16_t
OMR::Node::getInlinedSiteIndex()
   {
   return _byteCodeInfo.getCallerIndex();
   }

void
OMR::Node::setInlinedSiteIndex(int16_t i)
   {
   _byteCodeInfo.setCallerIndex(i);
   }

TR::Node *
OMR::Node::setChild(int32_t c, TR::Node * p)
   {
   TR_ASSERT(self()->getOpCodeValue() != TR::BBStart || c != 1, "Can't use second child of TR::BBStart");
   if(self()->hasNodeExtension())
      {
      TR_ASSERT(c < _unionBase._extension.getNumElems(),
                "Out of bounds errror when setting element %d in node %p\n",c,this);
      return _unionBase._extension.getExtensionPtr()->setElem<TR::Node *>(c,p);
      }
   return (_unionBase._children[c] = p);
   }

TR::Node *
OMR::Node::getExtendedChild(int32_t c)
   {
   TR_ASSERT(self()->hasNodeExtension() && c < _unionBase._extension.getNumElems(),"Out of bounds errror when getting element %d in node %p\n",c,this);
   return _unionBase._extension.getExtensionPtr()->getElem<TR::Node *>(c);
   }

TR::Node *
OMR::Node::setFirst(TR::Node * p)
   {
   return self()->setChild(0,p);
   }

TR::Node *
OMR::Node::setSecond(TR::Node * p)
   {
   TR_ASSERT(self()->getOpCodeValue() != TR::BBStart, "Can't use second child of TR::BBStart");
   return self()->setChild(1,p);
   }



/**
 * Swap the (two) children for this node
 */
void
OMR::Node::swapChildren()
   {
//   TR_ASSERT(getNumChildren() == 2, "must have 2 children to swap");
// assume no longer valid since if opcodes have three children, but still just want to swap the first two
   TR::Node *firstChild = self()->getFirstChild();
   self()->setFirst(self()->getSecondChild());
   self()->setSecond(firstChild);
   if (self()->getOpCode().isIf())
      self()->setSwappedChildren(!self()->childrenWereSwapped());
   }

TR::Node *
OMR::Node::removeChild(int32_t i)
   {
   int32_t numChildren = self()->getNumChildren();
   TR::Node *removedNode = self()->getChild(i);
   removedNode->recursivelyDecReferenceCount();
   for (int32_t j = i + 1; j < numChildren; ++j)
      self()->setChild(j - 1, self()->getChild(j));
   self()->setNumChildren(numChildren - 1);
   return removedNode;
   }

TR::Node*
OMR::Node::removeLastChild()
   {
   return self()->removeChild(_numChildren - 1);
   }

/**
 * Remove all the children for this node
 */
void
OMR::Node::removeAllChildren()
   {
   for (int32_t i = self()->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node * child = self()->getChild(i);
      self()->setChild(i, NULL);
      child->recursivelyDecReferenceCount();
      }
   self()->setNumChildren(0);
   }

void
OMR::Node::rotateChildren(int32_t first, int32_t last)
   {
   TR::Node *wraparound = self()->getChild(last);
   if (last > first)
      for (int32_t i = last; i > first; i--)
         self()->setChild(i, self()->getChild(i-1));
   else
      for (int32_t i = last; i < first; i++)
         self()->setChild(i, self()->getChild(i+1));
   self()->setChild(first, wraparound);
   }



TR::Node *
OMR::Node::findChild(TR::ILOpCodes opcode, bool isReversed)
   {
   if (isReversed)
      {
      for (int32_t i = self()->getNumChildren()-1; i >= 0; i--)
         {
         TR::Node * currNode = self()->getChild(i);
         if (currNode->getOpCodeValue() == opcode)
            return currNode;
         }
      }
   else
      {
      for (uint16_t i = 0; i < self()->getNumChildren(); i++)
         {
         TR::Node * currNode = self()->getChild(i);
         if (currNode->getOpCodeValue() == opcode)
            return currNode;
         }
      }
   return NULL;
   }

int32_t
OMR::Node::findChildIndex(TR::Node * child)
   {
   for (uint16_t i = 0; i < self()->getNumChildren(); i++)
      {
      if (self()->getChild(i) == child)
         return i;
      }
   return -1;
   }

int32_t
OMR::Node::countChildren(TR::ILOpCodes opcode)
   {
   int32_t count = 0;
   for (uint16_t i = 0; i < self()->getNumChildren(); i++)
      {
      TR::Node * currNode = self()->getChild(i);
      if (currNode->getOpCodeValue() == opcode)
         count++;
      }
   return count;
   }

bool
OMR::Node::requiresRegisterPair(TR::Compilation *comp)
   {
   return (self()->getType().isInt64() && comp->target().is32Bit() && !comp->cg()->use64BitRegsOn32Bit());
   }


/**
 * Node field functions end
 */



/**
 * OptAttributes functions
 */

void
OMR::Node::resetVisitCounts(vcount_t count)
   {
   if (self()->getVisitCount() != count)
      {
      self()->setVisitCount(count);
      for (int32_t i = 0; i < self()->getNumChildren(); i++)
         self()->getChild(i)->resetVisitCounts(count);
      }
   }

rcount_t
OMR::Node::recursivelyDecReferenceCount()
   {
   rcount_t count;
   if (self()->getReferenceCount() > 0)
      count = self()->decReferenceCount();
   else
      {
      TR_ASSERT(self()->getOpCode().isTreeTop(), "OMR::Node::recursivelyDecReferenceCount() invoked for nontreetop node %s " POINTER_PRINTF_FORMAT " with count == 0", self()->getOpCode().getName(), self());
      count = 0;
      }

   if (count == 0)
      {
      for (int32_t childCount = self()->getNumChildren()-1; childCount >= 0; childCount--)
         {
         TR_ASSERT(self()->getChild(childCount) != NULL, "parent %s " POINTER_PRINTF_FORMAT " trying to decrement reference of NULL child\n", self()->getOpCode().getName(), self());
         self()->getChild(childCount)->recursivelyDecReferenceCount();
         }
      }
   return count;
   }

void
OMR::Node::recursivelyDecReferenceCountFromCodeGen()
   {
   uint32_t count;
   if (self()->getReferenceCount() > 0)
      count = self()->decReferenceCount();
   else
      count = 0;

   if (count == 0 && self()->getRegister() == 0)
      for (int32_t i = self()->getNumChildren()-1; i >= 0; --i)
         self()->getChild(i)->recursivelyDecReferenceCountFromCodeGen();
   }

void
OMR::Node::initializeFutureUseCounts(vcount_t visitCount)
   {
   if (visitCount == self()->getVisitCount())
      return;

   self()->setVisitCount(visitCount);
   self()->setFutureUseCount(self()->getReferenceCount());

   for (int32_t i = self()->getNumChildren() - 1; i >= 0; --i)
      self()->getChild(i)->initializeFutureUseCounts(visitCount);
   }

void
OMR::Node::setIsNotRematerializeable()
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation1(c, "Setting notRematerializeable flag on node %p\n", self()))
      _localIndex = (_localIndex | SCOUNT_HIGH_BIT);
   }

bool
OMR::Node::isRematerializeable()
   {
   if (_localIndex & SCOUNT_HIGH_BIT)
      return false;
   return true;
   }

TR::Register*
OMR::Node::getRegister()
   {
   if (_opCode.isIf())
      return NULL;

   TR_ASSERT(self()->getOpCodeValue() != TR::BBStart, "don't call getRegister for a BBStart");
   if ((uintptr_t)_unionA._register & 1) return 0; // tagged pointer means the field is actually an evaluation priority
   return _unionA._register;
   }

TR::Register *
OMR::Node::setRegister(TR::Register *reg)
   {
   if (_opCode.isIf())
      {
      TR_ASSERT_FATAL_WITH_NODE(self(), reg == NULL, "if node with register");
      return NULL;
      }

   if (reg)
      {
      if (reg->isLive())
         {
         reg->getLiveRegisterInfo()->incNodeCount();

         TR::RegisterPair * pair = reg->getRegisterPair();
         if (pair)
            {
            TR_ASSERT(pair->getLowOrder()->isLive() && pair->getHighOrder()->isLive(), "Register pair components are not live");

            pair->getHighOrder()->getLiveRegisterInfo()->incNodeCount();
            pair->getLowOrder()->getLiveRegisterInfo()->incNodeCount();
            }

         reg->getLiveRegisterInfo()->setNode(self());
         }

#if defined(J9_PROJECT_SPECIFIC) && defined(TR_TARGET_S390)
      if (reg->getPseudoRegister())
         {
         TR_PseudoRegister *pseudoReg = reg->getPseudoRegister();
         if (pseudoReg->getDecimalPrecision() == 0)
            pseudoReg->setDecimalPrecision(self()->getDecimalPrecision());
         pseudoReg->resetTemporaryKnownSignCode();
         pseudoReg->setDataType(self()->getDataType());
         }
      else if (reg->getOpaquePseudoRegister())
         {
         TR_OpaquePseudoRegister *opaquePseudoReg = reg->getOpaquePseudoRegister();
         opaquePseudoReg->setDataType(self()->getDataType());
         }
#endif
      }

   return (_unionA._register = reg);
   }

void *
OMR::Node::unsetRegister()
   {
   TR::Register * reg = self()->getRegister();
   if (reg && reg->isLive())
      {
      reg->getLiveRegisterInfo()->decNodeCount();

      TR::RegisterPair * pair = reg->getRegisterPair();
      if (pair)
         {
         TR_ASSERT(pair->getLowOrder()->isLive() &&
                pair->getHighOrder()->isLive(),
                "Register pair components are not live");

         pair->getHighOrder()->getLiveRegisterInfo()->decNodeCount();
         pair->getLowOrder()->getLiveRegisterInfo()->decNodeCount();
         }

      reg->getLiveRegisterInfo()->setNode(NULL);
      }

   _unionA._register = NULL;
   return NULL;
   }



int32_t
OMR::Node::getEvaluationPriority(TR::CodeGenerator * cg)
   {
   if (_unionA._register == 0) // not evaluated into register & priority unknown
      {
      // Hack: our trees can have cycles (lmul/lumulh). To avoid infinite
      // recursion, initialize this node's priority to zero
      // This way we don't need to resort to visit counts
      self()->setEvaluationPriority(0); // FIXME: remove this once we have
      // dealt with the issue of cycles in nodes
      return self()->setEvaluationPriority(cg->getEvaluationPriority(self()));
      }
   if ((uintptr_t)(_unionA._register) & 1) // evaluation priority
      return static_cast<int32_t>(reinterpret_cast<uintptr_t>(_unionA._register) >> 1);
   else // already evaluated to a register - nonsensical question
      return 0;
   }

int32_t
OMR::Node::setEvaluationPriority(int32_t p)
   {
   if (_unionA._register == 0 ||            // not evaluated, priority unknown
       ((uintptr_t)(_unionA._register) & 1))  // not evaluated, priority known
      {
      _unionA._register = (TR::Register*)(uintptr_t)((p << 1) | 1);
      }
   else // evaluated into a register
      {
      TR_ASSERT(0, "setEvaluationPriority cannot be called after the node has already been evaluated");
      }
   return p;
   }

/**
 * OptAttributes functions end
 */



/**
 * UnionBase functions
 */

bool
OMR::Node::canGet32bitIntegralValue()
   {
   TR::DataType type = self()->getType();
   return self()->getOpCode().isLoadConst() && (type.isInt32() || type.isInt16() || type.isInt8());
   }

int32_t
OMR::Node::get32bitIntegralValue()
   {
   TR_ASSERT(self()->getOpCode().isLoadConst(), "get32bitIntegralValue called on node which is not loadConst");
   TR_ASSERT(self()->canGet32bitIntegralValue(), "get32bitIntegralValue called on a node which returns false from canGet32bitIntegralValue()");
   TR::DataType type = self()->getType();

   if (type.isInt32())
      return self()->getInt();
   else if (type.isInt16())
      return self()->getShortInt();
   else if (type.isInt8())
      return self()->getByte();
   else
      {
      TR_ASSERT(false, "Must be an <= size 4 integral but it is type %s", self()->getDataType().toString());
      return 0;
      }
   }



bool
OMR::Node::canGet64bitIntegralValue()
   {
   TR::DataType type = self()->getType();
   if (!self()->getOpCode().isLoadConst())
      return false;
   else if (type.isInt8() || type.isInt16() || type.isInt32() || type.isInt64())
      return true;
   else if (type.isAddress())
      return true;
   return false;
   }

int64_t
OMR::Node::get64bitIntegralValue()
   {
   TR_ASSERT(self()->getOpCode().isLoadConst(), "get64bitIntegralValue called on node which is not loadConst");
   TR_ASSERT(self()->canGet64bitIntegralValue(), "get64bitIntegralValue called on a node which returns false from canGet64bitIntegralValue()");
   if (!self()->getOpCode().isLoadConst())
      return 0;

   TR::DataType type = self()->getType();

   if (type.isInt8())
      return self()->getByte();
   else if (type.isInt16())
      return self()->getShortInt();
   else if (type.isInt32())
      return self()->getInt();
   else if (type.isInt64())
      return self()->getLongInt();
   else if (type.isAddress())
      return self()->getAddress();
   else
      {
      TR_ASSERT(false, "Must be an integral or address but it is type %s on node %p\n", self()->getDataType().toString(), self());
      return 0;
      }

   return 0;
   }

void
OMR::Node::set64bitIntegralValue(int64_t value)
   {
   TR::DataType type = self()->getType();

   if (type.isInt8())
      self()->setByte((int8_t)value);
   else if (type.isInt16())
      self()->setShortInt((int16_t)value);
   else if (type.isInt32())
      self()->setInt((int32_t)value);
   else if (type.isInt64())
      self()->setLongInt(value);
   else if (type.isAddress())
      {
      if (TR::comp()->target().is64Bit())
         self()->setLongInt(value);
      else
         self()->setInt((int32_t)value);
      }
   else
      {
      TR_ASSERT(false, "Must be an integral value but it is type %s", self()->getDataType().toString());
      }
   }

uint64_t
OMR::Node::get64bitIntegralValueAsUnsigned()
   {
   TR::DataType type = self()->getType();

   if (type.isInt8())
      return self()->getUnsignedByte();
   else if (type.isInt16())
      return self()->getConst<uint16_t>();
   else if (type.isInt32())
      return self()->getUnsignedInt();
   else if (type.isInt64())
      return self()->getUnsignedLongInt();
   else if (type.isAddress())
      return TR::comp()->target().is64Bit() ? self()->getUnsignedLongInt() : self()->getUnsignedInt();
   else
      {
      TR_ASSERT(false, "Must be an integral or address but it is type %s", self()->getDataType().toString());
      return 0;
      }

   return 0;
   }



TR::Node *
OMR::Node::getAllocation()
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::allocationFence), "Opcode must be TR::allocationFence in this method");
   return ((TR::Node *)_unionBase._constValue);
   }

TR::Node *
OMR::Node::setAllocation(TR::Node * p)
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::allocationFence), "Opcode must be TR::allocationFence in this method");
   return ((TR::Node *) (_unionBase._constValue = (int64_t)p));
   }



size_t
OMR::Node::sizeOfExtension()
   {
   size_t extSize = 0;
   if(self()->hasNodeExtension())
      {
      extSize  = (_unionBase._extension.getNumElems()-1) * (sizeof(uintptr_t));
      extSize += sizeof(TR::NodeExtension);
      }
   return extSize;
   }

void
OMR::Node::createNodeExtension(uint16_t numElems)
   {
   TR::Compilation *comp = TR::comp();
   TR_ArenaAllocator * alloc = comp->arenaAllocator();
   TR::NodeExtension * nodeExt = new(numElems,*alloc) TR::NodeExtension(*alloc);
   for(uint32_t i = 0 ; i < NUM_DEFAULT_CHILDREN ; i++)
      nodeExt->setElem<TR::Node *>(i,_unionBase._children[i]);
   _unionBase._extension.setExtensionPtr(nodeExt);
   _unionBase._extension.setNumElems(numElems);
   self()->setHasNodeExtension(true);
   }

void
OMR::Node::copyNodeExtension(TR::NodeExtension * other, uint16_t numElems, size_t size)
   {
   TR::Compilation *comp = TR::comp();
   NodeExtAllocator * alloc = comp->arenaAllocator();
   TR::NodeExtension * nodeExt = new(numElems,*alloc) TR::NodeExtension(*alloc);
   _unionBase._extension.setExtensionPtr(nodeExt);
   memcpy(nodeExt,other,size);
   self()->setHasNodeExtension(true);
   _unionBase._extension.setNumElems(numElems);
   }

void
OMR::Node::addExtensionElements(uint16_t num)
   {
   // create space for num additional extension elements
   uint16_t numElems;

   if (self()->hasNodeExtension())
      numElems = _unionBase._extension.getNumElems();
   else
      numElems = self()->getNumChildren();

   numElems += num;

   if (numElems > NUM_DEFAULT_CHILDREN)
      {
      if (self()->hasNodeExtension())
         self()->copyNodeExtension(_unionBase._extension.getExtensionPtr(), numElems, self()->sizeOfExtension());
      else
         self()->createNodeExtension(numElems);
      }

   for(uint16_t i = 0 ; i < num; i++)
      {
      self()->setChild(i + numElems - num, NULL);
      }
   }

void
OMR::Node::freeExtensionIfExists()
   {
   if (self()->hasNodeExtension())
      {
      TR::NodeExtension * extension = _unionBase._extension.getExtensionPtr();
      size_t size = self()->sizeOfExtension();
      uint16_t numElems = _unionBase._extension.getNumElems();
      for(uint16_t childNum = 0; (childNum < NUM_DEFAULT_CHILDREN && childNum < numElems); childNum++)
         {
         _unionBase._children[childNum] = extension->getElem<TR::Node *>(childNum);
         }
      extension->freeExtension(size);
      self()->setHasNodeExtension(false);
      }
   }

TR::DataType
OMR::Node::getArrayCopyElementType()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy || self()->getOpCodeValue() == TR::arrayset, "getArrayCopyElementType called on an invalid node");

   if (_numChildren == 3)
      {
      return static_cast<TR::DataTypes>(_unionBase._extension.getExtensionPtr()->getElem<uint32_t>(_numChildren));
      }
   else
      {
      TR_ASSERT(_numChildren == 5, "getArrayCopyElementType called on an arraycopy node with an invalid number of children");

      return TR::Address;
      }
   }

void
OMR::Node::setArrayCopyElementType(TR::DataType type)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy || self()->getOpCodeValue() == TR::arrayset, "setArrayCopyElementType called on an invalid node");

   if (_numChildren == 3)
      {
      TR_ASSERT(_unionBase._extension.getNumElems() > _numChildren, "Need an extra extension element slot in setArrayCopyElementType");

      _unionBase._extension.getExtensionPtr()->setElem<uint32_t>(_numChildren, type);
      }
   }

TR_OpaqueClassBlock *
OMR::Node::getArrayStoreClassInNode()
   {
   TR_ASSERT(self()->hasNodeExtension(), "hasArrayStoreCheckInfo node needs extension");
   if(self()->hasArrayStoreCheckInfo())
      {
      ArrayStoreCheckInfo * info = _unionBase._extension.getExtensionPtr()->getElem<ArrayStoreCheckInfo *>(2);
      return info->objectClass;
      }
   return NULL;
   }

void
OMR::Node::setArrayStoreClassInNode(TR_OpaqueClassBlock *o)
   {
   if (!self()->hasArrayStoreCheckInfo())
      self()->createArrayStoreCheckInfo();

   TR_ASSERT(self()->hasNodeExtension(), "hasArrayStoreCheckInfo node needs extension");

   ArrayStoreCheckInfo * info =  _unionBase._extension.getExtensionPtr()->getElem<ArrayStoreCheckInfo *>(2);
   info->objectClass = o;
   }



TR_OpaqueClassBlock *
OMR::Node::getArrayComponentClassInNode()
   {
   TR_ASSERT(self()->hasNodeExtension(), "hasArrayStoreCheckInfo node needs extension");
   if(self()->hasArrayStoreCheckInfo())
      {
      ArrayStoreCheckInfo * info = _unionBase._extension.getExtensionPtr()->getElem<ArrayStoreCheckInfo *>(2);
      return info->arrayComponentClass;
      }
   return NULL;
   }

void
OMR::Node::setArrayComponentClassInNode(TR_OpaqueClassBlock *c)
   {
   if (!self()->hasArrayStoreCheckInfo())
      self()->createArrayStoreCheckInfo();

   TR_ASSERT(self()->hasNodeExtension(), "hasArrayStoreCheckInfo node needs extension");
   ArrayStoreCheckInfo * info = _unionBase._extension.getExtensionPtr()->getElem<ArrayStoreCheckInfo *>(2);
   info->arrayComponentClass = c;
   }

TR::ILOpCodes
OMR::Node::setOverflowCheckOperation(TR::ILOpCodes op)
   {
   TR_ASSERT(self()->getOpCode().isOverflowCheck(),
             "set OverflowCHK operation info for no OverflowCHK node");
   return _unionBase._extension.getExtensionPtr()->setElem<TR::ILOpCodes>(3, op);
   }

TR::ILOpCodes
OMR::Node::getOverflowCheckOperation()
   {
   TR_ASSERT(self()->getOpCode().isOverflowCheck(),
   	     "get OverflowCHK operation info for no OverflowCHK node");
   return _unionBase._extension.getExtensionPtr()->getElem<TR::ILOpCodes>(3);
   }

bool
OMR::Node::hasArrayStoreCheckInfo()
   {
   if (self()->getChild(2))
      return true;
   else
      return false;
   }

void
OMR::Node::createArrayStoreCheckInfo()
   {
   TR::Compilation * comp = TR::comp();
   ArrayStoreCheckInfo *info = (ArrayStoreCheckInfo *)comp->trMemory()->allocateMemory(sizeof(ArrayStoreCheckInfo), heapAlloc);
   memset(info, 0, sizeof(ArrayStoreCheckInfo ));
   TR_ASSERT(self()->hasNodeExtension(), "hasArrayStoreCheckInfo node needs extension");

   _unionBase._extension.getExtensionPtr()->setElem<ArrayStoreCheckInfo *>(2, info);
   }



TR_OpaqueMethodBlock *
OMR::Node::getMethod()
   {
   TR_ASSERT(self()->getInlinedSiteIndex() < -1, "Call getMethod only for dummy nodes in estimate code size");
   TR_ASSERT(!self()->hasNodeExtension(), "Cannot Call getMethod on node with extension");
   return (TR_OpaqueMethodBlock *)_unionBase._children[0];
   }

void
OMR::Node::setMethod(TR_OpaqueMethodBlock * method)
   {
   self()->freeExtensionIfExists();
   _unionBase._children[0] = (TR::Node *)method;
   }



TR::LabelSymbol *
OMR::Node::getLabel()
   {
   //fixme: enable this assertion!
   //TR_ASSERT(self()->getOpCodeValue() == TR::BBStart, "getLabel called on a node that is not a BBStart");
   return (TR::LabelSymbol *) self()->getChild(1);
   }

TR::LabelSymbol *
OMR::Node::setLabel(TR::LabelSymbol *lab)
   {
   self()->freeExtensionIfExists();
   _unionBase._children[1] = (TR::Node *)lab;
   return self()->getLabel();
   }



TR_GlobalRegisterNumber
OMR::Node::getGlobalRegisterNumber()
   {
   return _unionBase._unionedWithChildren._globalRegisterInfo.getLow();
   }

TR_GlobalRegisterNumber
OMR::Node::setGlobalRegisterNumber(TR_GlobalRegisterNumber i)
   {
   _unionBase._unionedWithChildren._globalRegisterInfo.setHigh(-1, self());
   _unionBase._unionedWithChildren._globalRegisterInfo.setLow(i, self());
   return _unionBase._unionedWithChildren._globalRegisterInfo.getLow();
   }

TR_GlobalRegisterNumber
OMR::Node::getLowGlobalRegisterNumber()
   {
   return _unionBase._unionedWithChildren._globalRegisterInfo.getLow();
   }

TR_GlobalRegisterNumber
OMR::Node::setLowGlobalRegisterNumber(TR_GlobalRegisterNumber i)
   {
   _unionBase._unionedWithChildren._globalRegisterInfo.setLow(i, self());
   return _unionBase._unionedWithChildren._globalRegisterInfo.getLow();
   }

TR_GlobalRegisterNumber
OMR::Node::getHighGlobalRegisterNumber()
   {
   return _unionBase._unionedWithChildren._globalRegisterInfo.getHigh();
   }

TR_GlobalRegisterNumber
OMR::Node::setHighGlobalRegisterNumber(TR_GlobalRegisterNumber i)
   {
   _unionBase._unionedWithChildren._globalRegisterInfo.setHigh(i, self());
   return _unionBase._unionedWithChildren._globalRegisterInfo.getHigh();
   }



size_t
OMR::Node::getLiteralPoolOffset()
   {
   return _unionBase._unionedWithChildren._offset;
   }

size_t
OMR::Node::setLiteralPoolOffset(size_t offset, size_t size)
   {
   self()->freeExtensionIfExists();
   return _unionBase._unionedWithChildren._offset = offset;
   }

void *
OMR::Node::getRelocationDestination(uint32_t n)
   {
   return _unionBase._unionedWithChildren._relocationInfo._relocationDestination[n];
   }

void *
OMR::Node::setRelocationDestination(uint32_t n, void * p)
   {
   self()->freeExtensionIfExists();return (_unionBase._unionedWithChildren._relocationInfo._relocationDestination[n] = p);
   }

uint32_t
OMR::Node::getRelocationType()
   {
   return _unionBase._unionedWithChildren._relocationInfo._relocationType;
   }

uint32_t
OMR::Node::setRelocationType(uint32_t r)
   {
   self()->freeExtensionIfExists();
   return (_unionBase._unionedWithChildren._relocationInfo._relocationType = r);
   }

uint32_t
OMR::Node::getNumRelocations()
   {
   return _unionBase._unionedWithChildren._relocationInfo._numRelocations;
   }

uint32_t
OMR::Node::setNumRelocations(uint32_t n)
   {
   self()->freeExtensionIfExists(); return (_unionBase._unionedWithChildren._relocationInfo._numRelocations = n);
   }



CASECONST_TYPE
OMR::Node::getCaseConstant()
   {
   return _unionBase._unionedWithChildren._caseInfo.getConst();
   }

CASECONST_TYPE
OMR::Node::setCaseConstant(CASECONST_TYPE c)
   {
   return _unionBase._unionedWithChildren._caseInfo.setConst(c, self());
   }



void *
OMR::Node::getMonitorInfo()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit || self()->getOpCodeValue() == TR::tstart, "assertion failure");
   if(self()->getOpCodeValue() != TR::tstart)
      return _unionBase._unionedWithChildren._monitorInfo.getInfo();
   // in the case of a tstart, the extra node is child 4.
   return _unionBase._extension.getExtensionPtr()->getElem<void *>(4);
   }

void *
OMR::Node::setMonitorInfo(void *info)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit || self()->getOpCodeValue() == TR::tstart, "assertion failure");
   if(self()->getOpCodeValue() != TR::tstart)
      return _unionBase._unionedWithChildren._monitorInfo.setInfo(info, self());
   // in the case of a tstart, the extra node is child 4.
   return _unionBase._extension.getExtensionPtr()->setElem<void *>(4, info);
   }

/**
 * UnionBase functions end
 */



/**
 * UnionPropertyA functions
 */

bool
OMR::Node::hasSymbolReference()
   {
   return self()->getOpCode().hasSymbolReference();
   }

bool
OMR::Node::hasRegLoadStoreSymbolReference()
   {
   return self()->getOpCode().isStoreReg() || self()->getOpCode().isLoadReg();
   }

bool
OMR::Node::hasSymbolReferenceOfAnyType()
   {
   return self()->hasSymbolReference() || self()->hasRegLoadStoreSymbolReference();
   }

bool
OMR::Node::hasBranchDestinationNode()
   {
   return self()->getOpCode().isBranch();
   }

bool
OMR::Node::hasBlock()
   {
   return self()->getOpCodeValue() == TR::BBStart || self()->getOpCodeValue() == TR::BBEnd;
   }

bool
OMR::Node::hasArrayStride()
   {
   return self()->getOpCode().isArrayLength();
   }

bool
OMR::Node::hasPinningArrayPointer()
   {
   return self()->getOpCode().hasPinningArrayPointer();
   }

bool
OMR::Node::hasDataType()
   {
   // _UnionPropertyA._dataType
   // Some opcodes do not have explicitly encoded datatype. Nodes with these opcodes will
   // automatically deduce datatype and cache it in its _dataType field.
   return _opCode.hasNoDataType() && !_opCode.hasSymbolReference() && !self()->hasRegLoadStoreSymbolReference();
   }

OMR::Node::UnionPropertyA_Type
OMR::Node::getUnionPropertyA_Type()
   {
   if (self()->hasSymbolReference())
      return OMR::Node::HasSymbolReference;
   else if (self()->hasRegLoadStoreSymbolReference())
#if !defined(REMOVE_REGLS_SYMREFS_FROM_GETSYMREF)
      // This code is to be removed, once all illegal accesses of _regLoadStoreSymbolReference are removed
      return OMR::Node::HasSymbolReference;
#else
      return OMR::Node::HasRegLoadStoreSymbolReference;
#endif
   else if (self()->hasBranchDestinationNode())
      return OMR::Node::HasBranchDestinationNode;
   else if (self()->hasBlock())
      return OMR::Node::HasBlock;
   else if (self()->hasArrayStride())
      return OMR::Node::HasArrayStride;
   else if (self()->hasPinningArrayPointer())
      return OMR::Node::HasPinningArrayPointer;
   else if (self()->hasDataType())
      return OMR::Node::HasDataType;
   return OMR::Node::HasNoUnionPropertyA;
   }

TR::SymbolReference *
OMR::Node::getSymbolReference()
   {
#if !defined(REMOVE_REGLS_SYMREFS_FROM_GETSYMREF)
   // Issue #4647 was created to track the investigation and removal of this guarded code.
   //
   // This code is to be removed, once all illegal accesses of _regLoadStoreSymbolReference are removed
   if (self()->hasRegLoadStoreSymbolReference())
      return self()->getRegLoadStoreSymbolReference();
#endif

   TR_ASSERT(self()->hasSymbolReference(), "attempting to access _symbolReference field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return _unionPropertyA._symbolReference;
   }

TR::SymbolReference    *
OMR::Node::setSymbolReference(TR::SymbolReference * p)
   {
#if !defined(REMOVE_REGLS_SYMREFS_FROM_SETSYMREF)
   // Issue #4647 was created to track the investigation and removal of this guarded code.
   //
   if (self()->hasRegLoadStoreSymbolReference())
      {
      p = self()->setRegLoadStoreSymbolReference(p);
      return p;
      }
   else
#endif
      {
      TR_ASSERT(self()->hasSymbolReference(), "attempting to access _symbolReference field for node %s %p that does not have it", self()->getOpCode().getName(), this);
      _unionPropertyA._symbolReference = p;
      }
   return p;
   }

TR::SymbolReference *
OMR::Node::getRegLoadStoreSymbolReference()
   {
   TR_ASSERT(self()->hasRegLoadStoreSymbolReference(), "attempting to access _regLoadStoreSymbolReference field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return _unionPropertyA._regLoadStoreSymbolReference;
   }

TR::SymbolReference *
OMR::Node::setRegLoadStoreSymbolReference(TR::SymbolReference * p)
   {
   TR_ASSERT(self()->hasRegLoadStoreSymbolReference(), "attempting to access _regLoadStoreSymbolReference field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   _unionPropertyA._regLoadStoreSymbolReference = p;
   return p;
   }

TR::SymbolReference *
OMR::Node::getSymbolReferenceOfAnyType()
   {
   TR_ASSERT(self()->hasSymbolReferenceOfAnyType(), "attempting to access a regLoad/regStore or normal symbolReference for node %s %p that does not have it", self()->getOpCode().getName(), this);
   if (self()->hasRegLoadStoreSymbolReference())
      return self()->getRegLoadStoreSymbolReference();

   return _unionPropertyA._symbolReference;
   }

TR::Symbol *
OMR::Node::getSymbol()
   {
#if !defined(REMOVE_REGLS_SYMREFS_FROM_GETSYMBOL)
   if (self()->hasRegLoadStoreSymbolReference())
      return self()->getRegLoadStoreSymbolReference() ? self()->getRegLoadStoreSymbolReference()->getSymbol() : NULL;
#else
   TR_ASSERT(hasSymbolReference(), "attempting to access _symbolReference field for node %s %p that does not have it", self()->getOpCode().getName(), this);
#endif
   return (_unionPropertyA._symbolReference != NULL) ? _unionPropertyA._symbolReference->getSymbol() : NULL;
   }

TR::Block *
OMR::Node::getBlock(bool ignored)
   {
   TR_ASSERT(self()->hasBlock(), "attempting to access _block field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return _unionPropertyA._block;
   }

TR::Block*
OMR::Node::setBlock(TR::Block * p)
   {
   TR_ASSERT(self()->hasBlock(), "attempting to access _block field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return (_unionPropertyA._block = p);
   }

int32_t
OMR::Node::getArrayStride()
   {
   TR_ASSERT(self()->hasArrayStride(), "attempting to access _arrayStride field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return _unionPropertyA._arrayStride;
   }

int32_t
OMR::Node::setArrayStride(int32_t s)
   {
   TR_ASSERT(self()->hasArrayStride(), "attempting to access _arrayStride field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return (_unionPropertyA._arrayStride = s);
   }

TR::AutomaticSymbol*
OMR::Node::getPinningArrayPointer()
   {
   TR_ASSERT(self()->hasPinningArrayPointer(), "attempting to access _pinningArrayPointer field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return _unionPropertyA._pinningArrayPointer;
   }

TR::AutomaticSymbol*
OMR::Node::setPinningArrayPointer(TR::AutomaticSymbol *s)
   {
   s->setPinningArrayPointer();
   TR_ASSERT(self()->hasPinningArrayPointer(), "attempting to access _pinningArrayPointer field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return (_unionPropertyA._pinningArrayPointer = s);
   }

TR::DataType
OMR::Node::computeDataType()
   {
   // If opcode has SymRef, must get DataType from SymRef, since _dataType member is union-aliased with SymRef.
   if (_opCode.hasSymbolReference() || self()->hasRegLoadStoreSymbolReference())
      {
      TR::SymbolReference *symRef = (_opCode.hasSymbolReference()) ? self()->getSymbolReference() :
            (self()->hasRegLoadStoreSymbolReference()) ? self()->getRegLoadStoreSymbolReference() : NULL;

      if (symRef && symRef->getSymbol())
         return symRef->getSymbol()->getDataType();
      }

   TR_ASSERT(self()->hasDataType(), "attempting to access _dataType field for node %s %p that does not have it",
         self()->getOpCode().getName(), self());
   if (_unionPropertyA._dataType != TR::NoType)
      return _unionPropertyA._dataType;

   TR_ASSERT(false, "Unsupported typeless opcode in node %p\n", self());
   return TR::NoType;
   }

/**
 * UnionPropertyA functions end
 */



/**
 * Node flags functions
 */

bool
OMR::Node::isZero()
   {
   return _flags.testAny(nodeIsZero);
   }

void
OMR::Node::setIsZero(bool v)
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nodeIsZero flag on node %p to %d\n", self(), v))
      _flags.set(nodeIsZero, v);
   }

bool
OMR::Node::isNonZero()
   {
   return _flags.testAny(nodeIsNonZero);
   }

void
OMR::Node::setIsNonZero(bool v)
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nodeIsNonZero flag on node %p to %d\n", self(), v))
      _flags.set(nodeIsNonZero, v);
   }

bool
OMR::Node::isNull()
   {
   return (self()->getOpCodeValue() == TR::loadaddr ? false : _flags.testAny(nodeIsNull) );
   }

void
OMR::Node::setIsNull(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() != TR::loadaddr, "Can't call this for loadaddr");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting null flag on node %p to %d\n", self(), v))
      _flags.set(nodeIsNull, v);
   }



bool
OMR::Node::isNonNull()
   {
   return (self()->getOpCodeValue() == TR::loadaddr ? true :
           (_flags.testAny(nodeIsNonNull) || self()->isInternalPointer() ||
            (self()->getOpCode().hasSymbolReference() && self()->getSymbol()->isInternalPointer())));
   }

void
OMR::Node::setIsNonNull(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(v || self()->getOpCodeValue() != TR::loadaddr, "Can't call this for loadaddr");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nonNull flag on node %p to %d\n", self(), v))
      _flags.set(nodeIsNonNull, v);
   }



bool
OMR::Node::pointsToNull()
   {
   return (self()->getOpCodeValue() == TR::loadaddr ? _flags.testAny(nodePointsToNull) : false);
   }

void
OMR::Node::setPointsToNull(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Can only call this for loadaddr");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nodePointsToNull flag on node %p to %d\n", self(), v))
      _flags.set(nodePointsToNull, v);
   }

bool
OMR::Node::chkPointsToNull()
   {
   return self()->getOpCodeValue() == TR::loadaddr && _flags.testAny(nodePointsToNull);
   }

bool
OMR::Node::pointsToNonNull()
   {
   return (self()->getOpCodeValue() == TR::loadaddr ? _flags.testAny(nodePointsToNonNull) : false);
   }

void
OMR::Node::setPointsToNonNull(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Can only call this for loadaddr");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nodePointsToNull flag on node %p to %d\n", self(), v))
      _flags.set(nodePointsToNonNull, v);
   }

bool
OMR::Node::chkPointsToNonNull()
   {
   return self()->getOpCodeValue() == TR::loadaddr && _flags.testAny(nodePointsToNonNull);
   }

bool
OMR::Node::containsCall()
   {
   return _flags.testAny(nodeContainsCall);
   }

void
OMR::Node::setContainsCall(bool v)
   {
   return _flags.set(nodeContainsCall, v);
   }



bool
OMR::Node::isInvalid8BitGlobalRegister()
   {
   return _flags.testAny(invalid8BitGlobalRegister);
   }

void
OMR::Node::setIsInvalid8BitGlobalRegister(bool v)
   {
   TR::Compilation * c = TR::comp();
#if defined(TR_HOST_X86)
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting invalid8BitGlobalRegister flag on node %p to %d\n", self(), v))
      _flags.set(invalid8BitGlobalRegister, v);
#endif
   }

bool
OMR::Node::isDirectMemoryUpdate()
   {
   return _flags.testAny(directMemoryUpdate);
   }

void
OMR::Node::setDirectMemoryUpdate(bool v)
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting directMemoryUpdate flag on node %p to %d\n", self(), v))
      _flags.set(directMemoryUpdate, v);
   }

bool
OMR::Node::isProfilingCode()
   {
   return _flags.testAny(profilingCode);
   }

void
OMR::Node::setIsProfilingCode()
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting profilingCode flag on node %p\n", self()))
      _flags.set(profilingCode);
   }

bool
OMR::Node::hasBeenVisitedForHints()
   {
   return _flags.testAny(visitedForHints);
   }

void
OMR::Node::setHasBeenVisitedForHints(bool v)
   {
   _flags.set(visitedForHints, v);
   }



bool
OMR::Node::isNonNegative()
   {
   return _flags.testAny(nodeIsNonNegative);
   }

void
OMR::Node::setIsNonNegative(bool v)
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nodeIsNonNegative flag on node %p to %d\n", self(), v))
      _flags.set(nodeIsNonNegative, v);
   }

bool
OMR::Node::isNonPositive()
   {
   return _flags.testAny(nodeIsNonPositive);
   }

void
OMR::Node::setIsNonPositive(bool v)
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nodeIsNonPositive flag on node %p to %d\n", self(), v))
      _flags.set(nodeIsNonPositive, v);
   }

bool
OMR::Node::isNonDegenerateArrayCopy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   TR::Node * len = self()->getNumChildren() == 5 ? self()->getChild(4) : self()->getChild(2);
   return len->isNonNegative() && len->isNonZero();
   }

bool
OMR::Node::divisionCannotOverflow()
   {
   // TODO: Implement this more precisely using value propagation.
   //
   TR::Node * dividend = self()->getFirstChild();
   TR::Node * divisor  = self()->getSecondChild();
   bool dividendIsNotMin        = dividend->isNonNegative();
   bool divisorIsNotNegativeOne = divisor ->isNonNegative();
   return dividendIsNotMin || divisorIsNotNegativeOne;
   }

bool
OMR::Node::cannotOverflow()
   {
   return _flags.testAny(cannotOverFlow);
   }

void
OMR::Node::setCannotOverflow(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(!self()->getOpCode().isIf(), "assertion failure");
   TR_ASSERT(self()->getOpCodeValue() != TR::PassThrough, "can't set cannotOverflow on PassThrough\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting cannotOverflow flag on node %p to %d\n", self(), v))
      _flags.set(cannotOverFlow, v);
   }

bool
OMR::Node::chkCannotOverflow()
   {
   return !self()->getOpCode().isIf()
      && (self()->getOpCodeValue() != TR::PassThrough)
      && _flags.testAny(cannotOverFlow);
   }

bool
OMR::Node::isSafeToSkipTableBoundCheck()
   {
   return _flags.testAny(canSkipTableBoundCheck);
   }

void
OMR::Node::setIsSafeToSkipTableBoundCheck(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::table, "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting canSkipTableBoundCheck flag on node %p to %d\n", self(), b))
      _flags.set(canSkipTableBoundCheck, b);
   }

bool
OMR::Node::chkSafeToSkipTableBoundCheck()
   {
   return self()->getOpCodeValue() == TR::table
      && _flags.testAny(canSkipTableBoundCheck);
   }

bool
OMR::Node::isNOPLongStore()
   {
   TR_ASSERT(self()->getOpCode().isStore() && self()->getType().isInt64(), "Opcode must be long store");
   return _flags.testAny(NOPLongStore);
   }

void
OMR::Node::setNOPLongStore(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isStore() && self()->getType().isInt64(), "Opcode must be long store");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting NOPLongStore flag on node %p to %d\n", self(), v))
      _flags.set(NOPLongStore, v);
   }

bool
OMR::Node::chkNOPLongStore()
   {
   return self()->getOpCode().isStore() && self()->getType().isInt64() && _flags.testAny(NOPLongStore);
   }

bool
OMR::Node::storedValueIsIrrelevant()
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCode().isStore() && self()->getSymbolReference()->getSymbol()->isAutoOrParm()), "Opcode must be store to a stack slot");
   if (c->getOption(TR_EnableOSR) && self()->getOpCode().isStore() && self()->getSymbolReference()->getSymbol()->isAutoOrParm())
      return _flags.testAny(StoredValueIsIrrelevant);
   return false;
   }

void
OMR::Node::setStoredValueIsIrrelevant(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(c->getOption(TR_EnableOSR), "Currently this OMR::Node flag means different things on different front ends. If we wanted to reuse this query across front ends other than Java (which uses these kinds of stores for OSR) then we probably need to map it to a different bit in the OMR::Node flags");
   TR_ASSERT((self()->getOpCode().isStore() && self()->getSymbolReference()->getSymbol()->isAutoOrParm()), "Opcode must be store to a stack slot");
   if (self()->getOpCode().isStore() && self()->getSymbolReference()->getSymbol()->isAutoOrParm())
      {
      if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting StoredValueIsIrrelevant flag on node %p to %d\n", self(), v))
         _flags.set(StoredValueIsIrrelevant, v);
      }
   }

bool
OMR::Node::chkStoredValueIsIrrelevant()
   {
   TR::Compilation * c = TR::comp();
   return c->getOption(TR_EnableOSR) && self()->getOpCode().isStore() && self()->getSymbolReference()->getSymbol()->isAutoOrParm() && _flags.testAny(StoredValueIsIrrelevant);
   }

bool
OMR::Node::isHeapificationStore()
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::astore), "Opcode must be astore");
   return _flags.testAny(HeapificationStore);
   }

void
OMR::Node::setHeapificationStore(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCodeValue() == TR::astore), "Opcode must be astore");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting HeapificationStore flag on node %p to %d\n", self(), v))
      _flags.set(HeapificationStore, v);
   }

bool
OMR::Node::chkHeapificationStore()
   {
   return (self()->getOpCodeValue() == TR::astore) && _flags.testAny(HeapificationStore);
   }

bool
OMR::Node::isHeapificationAlloc()
   {
   TR_ASSERT(self()->getOpCode().isNew(), "Opcode must be isNew");
   return _flags.testAny(HeapificationAlloc);
   }

void
OMR::Node::setHeapificationAlloc(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isNew(), "Opcode must be isNew");
   if (performNodeTransformation2(c,"O^O NODE FLAGS: Setting HeapificationAlloc flag on node %p to %d\n", self(), v))
      _flags.set(HeapificationAlloc, v);
   }

bool
OMR::Node::chkHeapificationAlloc()
   {
   return self()->getOpCode().isNew() && _flags.testAny(HeapificationAlloc);
   }

bool
OMR::Node::isIdentityless()
   {
   TR_ASSERT(self()->getOpCode().isNew(), "Opcode must be isNew");
   return _flags.testAny(Identityless);
   }

void
OMR::Node::setIdentityless(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isNew(), "Opcode must be isNew");
   if (performNodeTransformation2(c,"O^O NODE FLAGS: Setting Identityless flag on node %p to %d\n", self(), v))
      _flags.set(Identityless, v);
   }

bool
OMR::Node::chkIdentityless()
   {
   return self()->getOpCode().isNew() && _flags.testAny(Identityless);
   }

bool
OMR::Node::isLiveMonitorInitStore()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::astore, "Opcode must be astore");
   return (self()->getOpCode().hasSymbolReference() && self()->getSymbol()->holdsMonitoredObject() && _flags.testAny(liveMonitorInitStore));
   }

void
OMR::Node::setLiveMonitorInitStore(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCodeValue() == TR::astore), "Opcode must be astore");
   if (self()->getOpCode().hasSymbolReference() && self()->getSymbol()->holdsMonitoredObject() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting liveMonitorInitStore flag on node %p to %d\n", self(), v))
      _flags.set(liveMonitorInitStore, v);
   }

bool
OMR::Node::chkLiveMonitorInitStore()
   {
   return (self()->getOpCodeValue() == TR::astore) && self()->isLiveMonitorInitStore();
   }

bool
OMR::Node::useSignExtensionMode()
   {
   return _flags.testAny(SignExtensionMode) && self()->getOpCode().isLoadVar() && self()->getType().isInt32();
   }

void
OMR::Node::setUseSignExtensionMode(bool b)
   {
   TR::Compilation * c = TR::comp();
   if (self()->getOpCode().isLoadVar() && self()->getType().isInt32() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting useSignExtensionMode flag on node %p to %d\n", self(), b))
      {
      _flags.set(SignExtensionMode, b);
      }
   }

bool
OMR::Node::hasFoldedImplicitNULLCHK()
   {
   TR_ASSERT(self()->getOpCode().isBndCheck() || self()->getOpCode().isSpineCheck(), "assertion failure");
   return _flags.testAny(foldedImplicitNULLCHK);
   }

void
OMR::Node::setHasFoldedImplicitNULLCHK(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isBndCheck() || self()->getOpCode().isSpineCheck(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting foldedImplicitNULLCHK flag on node %p to %d\n", self(), v))
      _flags.set(foldedImplicitNULLCHK, v);
   }

bool
OMR::Node::chkFoldedImplicitNULLCHK()
   {
   return self()->getOpCode().isBndCheck() && _flags.testAny(foldedImplicitNULLCHK);
   }

bool
OMR::Node::throwInsertedByOSR()
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::athrow), "Opcode must be athrow");
   if (self()->getOpCodeValue() == TR::athrow)
      return _flags.testAny(ThrowInsertedByOSR);
   return false;
   }

void
OMR::Node::setThrowInsertedByOSR(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCodeValue() == TR::athrow), "Opcode must be athrow");
   if (c->getOption(TR_EnableOSR) && (self()->getOpCodeValue() == TR::athrow))
      {
      if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting ThrowInsertedByOSR flag on node %p to %d\n", self(), v))
         _flags.set(ThrowInsertedByOSR, v);
      }
   }

bool
OMR::Node::chkThrowInsertedByOSR()
   {
   return (self()->getOpCodeValue() == TR::athrow) && _flags.testAny(ThrowInsertedByOSR);
   }

bool
OMR::Node::isTheVirtualCallNodeForAGuardedInlinedCall()
   {
   if (self()->getOpCode().isCall() && !self()->isArrayCopyCall())
      return _flags.testAny(virtualCallNodeForAGuardedInlinedCall);
   else
      return false;
   }

void
OMR::Node::setIsTheVirtualCallNodeForAGuardedInlinedCall()
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting virtualCallNodeForAGuardedInlinedCall flag on node %p\n", self()))
      _flags.set(virtualCallNodeForAGuardedInlinedCall);
   }

void
OMR::Node::resetIsTheVirtualCallNodeForAGuardedInlinedCall()
   {
   TR::Compilation * c = TR::comp();
   if (performNodeTransformation1(c, "ReO^O NODE FLAGS: Setting virtualCallNodeForAGuardedInlinedCall flag on node %p\n", self()))
      _flags.reset(virtualCallNodeForAGuardedInlinedCall);
   }

bool
OMR::Node::chkTheVirtualCallNodeForAGuardedInlinedCall()
   {
   return self()->getOpCode().isCall() && !self()->isArrayCopyCall() && _flags.testAny(virtualCallNodeForAGuardedInlinedCall);
   }

/**
 * Call operates with the same semantics as java/lang/System/ArrayCopy
 */
bool
OMR::Node::isArrayCopyCall()
   {
   return false;
   }

bool
OMR::Node::isDontTransformArrayCopyCall()
   {
   TR_ASSERT(self()->isArrayCopyCall(), "attempt to set transformArrayCopyCall flag not an arraycopy call");
   if (self()->isArrayCopyCall())
      return _flags.testAny(dontTransformArrayCopyCall);
   else
      return false;
   }

void
OMR::Node::setDontTransformArrayCopyCall()
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->isArrayCopyCall(), "attempt to set transformArrayCopyCall flag not an arraycopy call");
   if (self()->isArrayCopyCall())
      {
      if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting dontTransformArrayCopyCall flag on node %p\n", self()))
         _flags.set(dontTransformArrayCopyCall);
      }

   }

bool
OMR::Node::chkDontTransformArrayCopyCall()
   {
   return self()->isArrayCopyCall() && _flags.testAny(dontTransformArrayCopyCall);
   }

bool
OMR::Node::isNodeRecognizedArrayCopyCall()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::call, "Opcode must be vcall to arraycopy");
   return _flags.testAny(nodeIsRecognizedArrayCopyCall);
   }

void
OMR::Node::setNodeIsRecognizedArrayCopyCall(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::call, "Opcode must be vcall to arraycopy");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nodeIsRecognizedArrayCopyCall flag on node %p to %d\n", self(), v))
      _flags.set(nodeIsRecognizedArrayCopyCall, v);
   }

bool
OMR::Node::chkNodeRecognizedArrayCopyCall()
   {
   return self()->getOpCodeValue() == TR::call && _flags.testAny(nodeIsRecognizedArrayCopyCall);
   }

bool
OMR::Node::canDesynchronizeCall()
   {
   TR_ASSERT(self()->getOpCode().isCall(), "Opcode must be a call");
   return _flags.testAny(desynchronizeCall);
   }

void
OMR::Node::setDesynchronizeCall(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isCall(), "Opcode must be a call");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting desynchronizeCall flag on node %p to %d\n", self(), v))
      _flags.set(desynchronizeCall, v);
   }

bool
OMR::Node::chkDesynchronizeCall()
   {
   return self()->getOpCode().isCall() && _flags.testAny(desynchronizeCall);
   }

bool
OMR::Node::isPreparedForDirectJNI()
   {
   return self()->getOpCode().isCall() && _flags.testAny(preparedForDirectToJNI) && self()->getOpCodeValue() != TR::arraycopy;
   }

void
OMR::Node::setPreparedForDirectJNI()
   {
   _flags.set(preparedForDirectToJNI, true);
#if defined(TR_TARGET_X86)
   self()->getSymbol()->castToMethodSymbol()->setLinkage(TR_J9JNILinkage);
#endif
   }



bool
OMR::Node::isSafeForCGToFastPathUnsafeCall()
   {
   TR_ASSERT(self()->getOpCode().isCall(), "Opcode must be call");
   return _flags.testAny(unsafeFastPathCall);
   }

void
OMR::Node::setIsSafeForCGToFastPathUnsafeCall(bool v)
   {
   TR_ASSERT(self()->getOpCode().isCall(), "Opcode must be call");
   _flags.set(unsafeFastPathCall);
   }



bool
OMR::Node::containsCompressionSequence()
   {
   return _flags.testAny(isCompressionSequence);
   }

void
OMR::Node::setContainsCompressionSequence(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCode().isAdd() || self()->getOpCode().isSub() || self()->getOpCode().isShift(), "Opcode must be an add/sub/shift");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting isCompressionSequence flag on node %p to %d\n", self(), v))
      _flags.set(isCompressionSequence, v);
   }

bool
OMR::Node::chkCompressionSequence()
   {
   return (self()->getOpCode().isAdd()
           || self()->getOpCode().isSub()
           || self()->getOpCode().isShift())
      && _flags.testAny(isCompressionSequence);
   }

bool
OMR::Node::isInternalPointer()
   {
   return _flags.testAny(internalPointer) && (self()->getOpCode().hasPinningArrayPointer() || self()->getOpCode().isArrayRef());
   }

void
OMR::Node::setIsInternalPointer(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCode().hasPinningArrayPointer() || self()->getOpCode().isArrayRef(),
             "Opcode must be one that can have a pinningArrayPointer or must be an array reference");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting internalPointer flag on node %p to %d\n", self(), v))
      _flags.set(internalPointer, v);
   }

bool
OMR::Node::isDataAddrPointer()
   {
   return self()->getOpCodeValue() == TR::aloadi
       && self()->getOpCode().hasSymbolReference()
       && self()->getSymbolReference() == TR::comp()->getSymRefTab()->findContiguousArrayDataAddrFieldShadowSymRef();
   }

bool
OMR::Node::isArrayTRT()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslateAndTest, "Opcode must be arraytranslateAndTest");
   return _flags.testAny(arrayTRT);
   }

void
OMR::Node::setArrayTRT(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslateAndTest, "Opcode must be arraytranslateAndTest");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting arrayTRT flag on node %p to %d\n", self(), v))
      _flags.set(arrayTRT, v);
   }

bool
OMR::Node::chkArrayTRT()
   {
   return self()->getOpCodeValue() == TR::arraytranslateAndTest && _flags.testAny(arrayTRT);
   }

bool
OMR::Node::isCharArrayTRT()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslateAndTest, "Opcode must be arraytranslateAndTest");
   return _flags.testAny(charArrayTRT);
   }

void
OMR::Node::setCharArrayTRT(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslateAndTest, "Opcode must be arraytranslateAndTest");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting charArrayTRT flag on node %p to %d\n", self(), v))
      _flags.set(charArrayTRT, v);
   }

bool
OMR::Node::chkCharArrayTRT()
   {
   return self()->getOpCodeValue() == TR::arraytranslateAndTest && _flags.testAny(charArrayTRT);
   }

bool
OMR::Node::isSourceByteArrayTranslate()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(sourceIsByteArrayTranslate);
   }

void
OMR::Node::setSourceIsByteArrayTranslate(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting sourceIsByteArrayTranslate flag on node %p to %d\n", self(), v))
      _flags.set(sourceIsByteArrayTranslate, v);
   }

bool
OMR::Node::chkSourceByteArrayTranslate()
   {
   return self()->getOpCodeValue() == TR::arraytranslate && _flags.testAny(sourceIsByteArrayTranslate);
   }

bool
OMR::Node::isTargetByteArrayTranslate()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(targetIsByteArrayTranslate);
   }

void
OMR::Node::setTargetIsByteArrayTranslate(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting targetIsByteArrayTranslate flag on node %p to %d\n", self(), v))
      _flags.set(targetIsByteArrayTranslate, v);
   }

bool
OMR::Node::chkTargetByteArrayTranslate()
   {
   return self()->getOpCodeValue() == TR::arraytranslate && _flags.testAny(targetIsByteArrayTranslate);
   }

bool
OMR::Node::isByteToByteTranslate()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(sourceIsByteArrayTranslate) && _flags.testAny(targetIsByteArrayTranslate);
   }

bool
OMR::Node::isByteToCharTranslate()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(sourceIsByteArrayTranslate) && !_flags.testAny(targetIsByteArrayTranslate);
   }

bool
OMR::Node::isCharToByteTranslate()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(targetIsByteArrayTranslate) && !_flags.testAny(sourceIsByteArrayTranslate);
   }

bool
OMR::Node::isCharToCharTranslate()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return !_flags.testAny(sourceIsByteArrayTranslate) && !_flags.testAny(targetIsByteArrayTranslate);
   }

bool
OMR::Node::chkByteToByteTranslate()
   {
   return self()->getOpCodeValue() == TR::arraytranslate
      && _flags.testAny(sourceIsByteArrayTranslate)
      && _flags.testAny(targetIsByteArrayTranslate);
   }

bool
OMR::Node::chkByteToCharTranslate()
   {
   return self()->getOpCodeValue() == TR::arraytranslate
      && _flags.testAny(sourceIsByteArrayTranslate)
      && !_flags.testAny(targetIsByteArrayTranslate);
   }

bool
OMR::Node::chkCharToByteTranslate()
   {
   return self()->getOpCodeValue() == TR::arraytranslate
      && _flags.testAny(targetIsByteArrayTranslate)
      && !_flags.testAny(sourceIsByteArrayTranslate);
   }

bool
OMR::Node::chkCharToCharTranslate()
   {
   return self()->getOpCodeValue() == TR::arraytranslate
      && !_flags.testAny(sourceIsByteArrayTranslate)
      && !_flags.testAny(targetIsByteArrayTranslate);
   }

bool
OMR::Node::getTermCharNodeIsHint()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(termCharNodeIsHint);
   }

void
OMR::Node::setTermCharNodeIsHint(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting termCharNodeIsHint flag on node %p to %d\n", self(), v))
      _flags.set(termCharNodeIsHint, v);
   }

bool
OMR::Node::chkTermCharNodeIsHint()
   {
   return self()->getOpCodeValue() == TR::arraytranslate && _flags.testAny(termCharNodeIsHint);
   }

bool
OMR::Node::getSourceCellIsTermChar()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(sourceCellIsTermChar);
   }

void
OMR::Node::setSourceCellIsTermChar(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting sourceCellIsTermChar flag on node %p to %d\n", self(), v))
      _flags.set(sourceCellIsTermChar, v);
   }

bool
OMR::Node::chkSourceCellIsTermChar()
   {
   return self()->getOpCodeValue() == TR::arraytranslate && _flags.testAny(sourceCellIsTermChar);
   }

bool
OMR::Node::getTableBackedByRawStorage()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   return _flags.testAny(tableBackedByRawStorage);
   }

void
OMR::Node::setTableBackedByRawStorage(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraytranslate, "Opcode must be arraytranslate");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting tableBackedByRawStorage flag on node %p to %d\n", self(), v))
      _flags.set(tableBackedByRawStorage, v);
   }

bool
OMR::Node::chkTableBackedByRawStorage()
   {
   return self()->getOpCodeValue() == TR::arraytranslate && _flags.testAny(tableBackedByRawStorage);
   }

bool
OMR::Node::isArrayCmpSign()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycmp, "Opcode must be arraycmp");
   return _flags.testAny(arrayCmpSign);
   }

void
OMR::Node::setArrayCmpSign(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycmp, "Opcode must be arraycmp");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting arrayCmpSign flag on node %p to %d\n", self(), v))
      _flags.set(arrayCmpSign, v);
   }

bool
OMR::Node::chkArrayCmpSign()
   {
   return self()->getOpCodeValue() == TR::arraycmp && _flags.testAny(arrayCmpSign);
   }

bool
OMR::Node::isHalfWordElementArrayCopy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   return _flags.testValue(arraycopyElementSizeMask, arraycopyElementSize2);
   }

void
OMR::Node::setHalfWordElementArrayCopy(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   if (v)
      {
      if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting halfWordElementArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyElementSizeMask, arraycopyElementSize2);
      }
   else
      {
      if (self()->isHalfWordElementArrayCopy() && performNodeTransformation2(c, "O^O NODE FLAGS: Setting halfWordElementArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyElementSizeMask, arraycopyElementSizeUnknown);
      }
   }

bool
OMR::Node::chkHalfWordElementArrayCopy()
   {
   return self()->getOpCodeValue() == TR::arraycopy && self()->isHalfWordElementArrayCopy();
   }

bool
OMR::Node::isWordElementArrayCopy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   return _flags.testValue(arraycopyElementSizeMask, arraycopyElementSize4);
   }

void
OMR::Node::setWordElementArrayCopy(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   if (v)
      {
      if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting wordElementArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyElementSizeMask, arraycopyElementSize4);
      }
   else
      {
      if (self()->isWordElementArrayCopy() && performNodeTransformation2(c, "O^O NODE FLAGS: Setting wordElementArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyElementSizeMask, arraycopyElementSizeUnknown);
      }
   }

bool
OMR::Node::chkWordElementArrayCopy()
   {
   return self()->getOpCodeValue() == TR::arraycopy && self()->isWordElementArrayCopy();
   }

bool
OMR::Node::isForwardArrayCopy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   return _flags.testAny(arraycopyDirectionForward); // also catches arraycopyDirectionForwardRarePath
   }

void
OMR::Node::setForwardArrayCopy(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   bool wasRarePathForwardArrayCopy = self()->isRarePathForwardArrayCopy();
   if (v)
      {
      if (!wasRarePathForwardArrayCopy && performNodeTransformation2(c, "O^O NODE FLAGS: Setting forwardArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyDirectionMask, arraycopyDirectionForward);
      TR_ASSERT(self()->isForwardArrayCopy() && !self()->isBackwardArrayCopy(), "assertion failure");
      TR_ASSERT(wasRarePathForwardArrayCopy == self()->isRarePathForwardArrayCopy(), "setForwardArrayCopy should not modify isRarePathForwardArrayCopy");
      }
   else
      {
      if (!self()->isBackwardArrayCopy() && performNodeTransformation2(c, "O^O NODE FLAGS: Setting forwardArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyDirectionMask, arraycopyDirectionUnknown);
      TR_ASSERT(!self()->isForwardArrayCopy(), "assertion failure");
      }
   }

bool
OMR::Node::chkForwardArrayCopy()
   {
   return self()->getOpCodeValue() == TR::arraycopy && self()->isForwardArrayCopy();
   }

bool
OMR::Node::isBackwardArrayCopy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   return _flags.testValue(arraycopyDirectionMask, arraycopyDirectionBackward);
   }

void
OMR::Node::setBackwardArrayCopy(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   if (v)
      {
      if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting backwardArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyDirectionMask, arraycopyDirectionBackward);
      TR_ASSERT(self()->isBackwardArrayCopy() && !self()->isForwardArrayCopy() && !self()->isRarePathForwardArrayCopy(), "assertion failure");
      }
   else
      {
      if (self()->isBackwardArrayCopy() && performNodeTransformation2(c, "O^O NODE FLAGS: Setting backwardArrayCopy flag on node %p to %d\n", self(), v))
         _flags.setValue(arraycopyDirectionMask, arraycopyDirectionUnknown);
      TR_ASSERT(!self()->isBackwardArrayCopy(), "assertion failure");
      }
   }

bool
OMR::Node::chkBackwardArrayCopy()
   {
   return self()->getOpCodeValue() == TR::arraycopy && self()->isBackwardArrayCopy();
   }

bool
OMR::Node::isRarePathForwardArrayCopy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   return _flags.testValue(arraycopyDirectionMask, arraycopyDirectionForwardRarePath);
   }

void
OMR::Node::setRarePathForwardArrayCopy(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   bool wasForwardArrayCopy = self()->isForwardArrayCopy();
   if (v != self()->isRarePathForwardArrayCopy() && performNodeTransformation2(c, "O^O NODE FLAGS: Setting rarePathForwardArrayCopy flag on node %p to %d\n", self(), v))
      _flags.setValue(arraycopyDirectionMask, v ? arraycopyDirectionForwardRarePath : arraycopyDirectionUnknown);
   if (v)
      TR_ASSERT(self()->isRarePathForwardArrayCopy() && self()->isForwardArrayCopy() && !self()->isBackwardArrayCopy(), "assertion failure");
   else
      TR_ASSERT(!self()->isRarePathForwardArrayCopy() && (wasForwardArrayCopy == self()->isForwardArrayCopy()), "assertion failure");
   }

bool
OMR::Node::chkRarePathForwardArrayCopy()
   {
   return self()->getOpCodeValue() == TR::arraycopy && self()->isRarePathForwardArrayCopy();
   }

bool
OMR::Node::isNoArrayStoreCheckArrayCopy()
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::arraycopy), "Opcode must be arraycopy");
   return _flags.testAny(noArrayStoreCheckArrayCopy);
   }

void
OMR::Node::setNoArrayStoreCheckArrayCopy(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::arraycopy, "Opcode must be arraycopy");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting noArrayStoreCheckArrayCopy flag on node %p to %d\n", self(), v))
      _flags.set(noArrayStoreCheckArrayCopy, v);
   }

bool
OMR::Node::chkNoArrayStoreCheckArrayCopy()
   {
   return self()->getOpCodeValue() == TR::arraycopy && self()->isNoArrayStoreCheckArrayCopy();
   }

bool
OMR::Node::isArraysetLengthMultipleOfPointerSize()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arrayset, "Opcode must be arrayset");
   return _flags.testAny(arraysetLengthMultipleOfPointerSize);
   }

void
OMR::Node::setArraysetLengthMultipleOfPointerSize(bool v)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::arrayset, "Opcode must be arrayset");
   if (self()->getOpCodeValue() == TR::arrayset)
      _flags.set(arraysetLengthMultipleOfPointerSize, v);
   }



bool
OMR::Node::isXorBitOpMem()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::bitOpMem, "Opcode must be bitOpMem");
   return _flags.testValue(bitOpMemOPMASK, bitOpMemXOR);
   }

void
OMR::Node::setXorBitOpMem(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::bitOpMem, "Opcode must be bitOpMem");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting XOR flag on node %p to %d\n", self(), v))
      _flags.setValue(bitOpMemOPMASK, bitOpMemXOR);
   }

bool
OMR::Node::chkXorBitOpMem()
   {
   return (self()->getOpCodeValue() == TR::bitOpMem)
      && _flags.testValue(bitOpMemOPMASK, bitOpMemXOR);
   }

bool
OMR::Node::isOrBitOpMem()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::bitOpMem, "Opcode must be bitOpMem");
   return _flags.testValue(bitOpMemOPMASK, bitOpMemOR);
   }

void
OMR::Node::setOrBitOpMem(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::bitOpMem, "Opcode must be bitOpMem");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting OR flag on node %p to %d\n", self(), v))
      _flags.setValue(bitOpMemOPMASK, bitOpMemOR);
   }

bool
OMR::Node::chkOrBitOpMem()
   {
   return (self()->getOpCodeValue() == TR::bitOpMem)
      && _flags.testValue(bitOpMemOPMASK, bitOpMemOR);
   }

bool
OMR::Node::isAndBitOpMem()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::bitOpMem, "Opcode must be bitOpMem");
   return _flags.testValue(bitOpMemOPMASK, bitOpMemAND);
   }

void
OMR::Node::setAndBitOpMem(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::bitOpMem, "Opcode must be bitOpMem");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting AND flag on node %p to %d\n", self(), v))
      _flags.setValue(bitOpMemOPMASK, bitOpMemAND);
   }

bool
OMR::Node::chkAndBitOpMem()
   {
   return (self()->getOpCodeValue() == TR::bitOpMem)
      && _flags.testValue(bitOpMemOPMASK, bitOpMemAND);
   }

bool
OMR::Node::isArrayChkPrimitiveArray1()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   return _flags.testAny(arrayChkPrimitiveArray1);
   }

void
OMR::Node::setArrayChkPrimitiveArray1(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting arrayChkPrimitiveArray1 flag on node %p to %d\n", self(), v))
      _flags.set(arrayChkPrimitiveArray1, v);
   }

bool
OMR::Node::chkArrayChkPrimitiveArray1()
   {
   return self()->getOpCodeValue() == TR::ArrayCHK && _flags.testAny(arrayChkPrimitiveArray1);
   }

bool
OMR::Node::isArrayChkReferenceArray1()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   return _flags.testAny(arrayChkReferenceArray1);
   }

void
OMR::Node::setArrayChkReferenceArray1(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting arrayChkReferenceArray1 flag on node %p to %d\n", self(), v))
      _flags.set(arrayChkReferenceArray1, v);
   }

bool
OMR::Node::chkArrayChkReferenceArray1()
   {
   return self()->getOpCodeValue() == TR::ArrayCHK && _flags.testAny(arrayChkReferenceArray1);
   }

bool
OMR::Node::isArrayChkPrimitiveArray2()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   return _flags.testAny(arrayChkPrimitiveArray2);
   }

void
OMR::Node::setArrayChkPrimitiveArray2(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting arrayChkPrimitiveArray2 flag on node %p to %d\n", self(), v))
      _flags.set(arrayChkPrimitiveArray2, v);
   }

bool
OMR::Node::chkArrayChkPrimitiveArray2()
   {
   return self()->getOpCodeValue() == TR::ArrayCHK && _flags.testAny(arrayChkPrimitiveArray2);
   }

bool
OMR::Node::isArrayChkReferenceArray2()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   return _flags.testAny(arrayChkReferenceArray2);
   }

void
OMR::Node::setArrayChkReferenceArray2(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::ArrayCHK, "Opcode must be ArrayCHK");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting arrayChkReferenceArray2 flag on node %p to %d\n", self(), v))
      _flags.set(arrayChkReferenceArray2, v);
   }

bool
OMR::Node::chkArrayChkReferenceArray2()
   {
   return self()->getOpCodeValue() == TR::ArrayCHK && _flags.testAny(arrayChkReferenceArray2);
   }

bool
OMR::Node::skipWrtBar()
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   return _flags.testAny(SkipWrtBar);
   }

void
OMR::Node::setSkipWrtBar(bool v)
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   _flags.set(SkipWrtBar, v);
   }

bool
OMR::Node::chkSkipWrtBar()
   {
   return self()->getOpCode().isWrtBar() && _flags.testAny(SkipWrtBar);
   }

bool
OMR::Node::isLikelyStackWrtBar()
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   return _flags.testAny(likelyStackWrtBar);
   }

void
OMR::Node::setLikelyStackWrtBar(bool v)
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   _flags.set(likelyStackWrtBar, v);
   }



bool
OMR::Node::isHeapObjectWrtBar()
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   return debug("useHeapObjectFlags") && _flags.testAny(heapObjectWrtBar);
   }

void
OMR::Node::setIsHeapObjectWrtBar(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting heapObjectWrtBar flag on node %p to %d\n", self(), v))
      _flags.set(heapObjectWrtBar, v);
   }

bool
OMR::Node::chkHeapObjectWrtBar()
   {
   return self()->getOpCode().isWrtBar() && debug("useHeapObjectFlags") && _flags.testAny(heapObjectWrtBar);
   }

bool
OMR::Node::isNonHeapObjectWrtBar()
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   return debug("useHeapObjectFlags") && _flags.testAny(nonHeapObjectWrtBar);
   }

void
OMR::Node::setIsNonHeapObjectWrtBar(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting nonHeapObjectWrtBar flag on node %p to %d\n", self(), v))
      _flags.set(nonHeapObjectWrtBar, v);
   }

bool
OMR::Node::chkNonHeapObjectWrtBar()
   {
   return self()->getOpCode().isWrtBar() && debug("useHeapObjectFlags") && _flags.testAny(nonHeapObjectWrtBar);
   }

bool
OMR::Node::isUnsafeStaticWrtBar()
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   return _flags.testAny(unsafeStaticWrtBar);
   }

void
OMR::Node::setIsUnsafeStaticWrtBar(bool v)
   {
   TR_ASSERT(self()->getOpCode().isWrtBar(), "Opcode must be wrtbar");
   _flags.set(unsafeStaticWrtBar, v);
   }



bool
OMR::Node::needsSignExtension()
   {
   return _flags.testAny(NeedsSignExtension) && (self()->getOpCodeValue() == TR::iRegStore || self()->getOpCodeValue() == TR::istore);
   }

void
OMR::Node::setNeedsSignExtension(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::iRegStore || self()->getOpCode().isStore(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting needsSignExtension flag on node %p to %d\n", self(), b))
      _flags.set(NeedsSignExtension, b);
   }

bool
OMR::Node::skipSignExtension()
   {
   return _flags.testAny(SkipSignExtension) &&
      (self()->getOpCodeValue() == TR::iRegLoad || self()->getOpCodeValue() == TR::iadd || self()->getOpCodeValue() == TR::isub);
   }
void
OMR::Node::setSkipSignExtension(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::iRegLoad || self()->getOpCodeValue() == TR::iadd || self()->getOpCodeValue() == TR::isub ||
             self()->getOpCode().isLoad(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting skipSignExtension flag on node %p to %d\n", self(), b))
      _flags.set(SkipSignExtension, b);
   }

bool
OMR::Node::isUseBranchOnCount()
   {
   TR_ASSERT(self()->getOpCode().canUseBranchOnCount(), "assertion failure");
   return _flags.testAny(nodeUsedForBranchOnCount);
   }

void
OMR::Node::setIsUseBranchOnCount(bool v)
   {
   TR::Compilation *comp = TR::comp();
   TR_ASSERT(self()->getOpCode().canUseBranchOnCount(), "assertion failure");
   _flags.set(nodeUsedForBranchOnCount, v);
   }



bool
OMR::Node::isDontMoveUnderBranch()
   {
   if (self()->getOpCode().isLoadVarDirect() || self()->getOpCode().isLoadReg())
      return _flags.testAny(dontMoveUnderBranch);
   return false;
   }

void
OMR::Node::setIsDontMoveUnderBranch(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isLoadReg() || self()->getOpCode().isLoadVarDirect(), "assertion failure");
   if (self()->getOpCode().isLoadVarDirect() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting dontMoveUnderBranch flag on node %p to %d\n", self(), v))
      {
      _flags.set(dontMoveUnderBranch, v);
      }
   }

bool
OMR::Node::chkDontMoveUnderBranch()
   {
   return (self()->getOpCode().isLoadReg() || self()->getOpCode().isLoadVarDirect()) && _flags.testAny(dontMoveUnderBranch);
   }

bool
OMR::Node::canChkNodeCreatedByPRE()
   {
   return self()->getOpCode().isLoadVarDirect();
   }

bool
OMR::Node::isNodeCreatedByPRE()
   {
   TR_ASSERT(self()->canChkNodeCreatedByPRE(), "Unexpected opcode %s in isNodeCreatedByPRE()",self()->getOpCode().getName());
   return _flags.testAny(nodeCreatedByPRE);
   }

void
OMR::Node::setIsNodeCreatedByPRE()
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->canChkNodeCreatedByPRE(), "Unexpected opcode %s in setIsNodeCreatedByPRE()",self()->getOpCode().getName());
   if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting nodeCreatedByPRE flag on node %p\n", self()))
      _flags.set(nodeCreatedByPRE);
   }

void
OMR::Node::resetIsNodeCreatedByPRE()
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->canChkNodeCreatedByPRE(), "Unexpected opcode %s in resetIsNodeCreatedByPRE()",self()->getOpCode().getName());
   if (performNodeTransformation1(c,"ReO^O NODE FLAGS: Setting nodeCreatedByPRE flag on node %p\n", self()))
      _flags.reset(nodeCreatedByPRE);
   }

bool
OMR::Node::chkNodeCreatedByPRE()
   {
   return self()->getOpCode().isLoadVarDirect() && _flags.testAny(nodeCreatedByPRE);
   }

bool
OMR::Node::isPrivatizedInlinerArg()
   {
   TR_ASSERT(self()->getOpCode().isStoreDirectOrReg(), "assertion failure");
   return _flags.testAny(privatizedInlinerArg) && !self()->chkLiveMonitorInitStore();
   }

void
OMR::Node::setIsPrivatizedInlinerArg(bool v)
   {
   TR_ASSERT(self()->getOpCode().isStoreDirectOrReg(), "assertion failure");
   _flags.set(privatizedInlinerArg, v);
   }

bool
OMR::Node::chkIsPrivatizedInlinerArg()
   {
   return self()->getOpCode().isStoreDirectOrReg() && self()->isPrivatizedInlinerArg();
   }

bool
OMR::Node::isMaxLoopIterationGuard()
   {
   return _flags.testAny(maxLoopIterationGuard) && self()->getOpCode().isIf();
   }

void
OMR::Node::setIsMaxLoopIterationGuard(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCode().isIf(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting maxLoopIterationGuard flag on node %p to %d\n", self(), v))
      _flags.set(maxLoopIterationGuard, v);
   }

bool
OMR::Node::isStopTheWorldGuard()
   {
   return self()->isHCRGuard() || self()->isOSRGuard() || self()->isBreakpointGuard();
   }

TR_VirtualGuard *
OMR::Node::virtualGuardInfo()
   {
   bool isGuard = _flags.testAny(inlineGuard) && self()->getOpCode().isIf();
   return isGuard ? _unionA._guard : NULL;
   }

void
OMR::Node::setVirtualGuardInfo(TR_VirtualGuard *guard, TR::Compilation *comp)
   {
   TR_ASSERT_FATAL_WITH_NODE(self(), self()->getOpCode().isIf(), "expected an if");
   if (_flags.testAny(inlineGuard))
      comp->removeVirtualGuard(_unionA._guard);

   _unionA._guard = guard;
   _flags.set(inlineGuard, guard != NULL);
   if (guard != NULL)
      comp->addVirtualGuard(guard);
   }

void
OMR::Node::copyVirtualGuardInfoTo(TR::Node *dupNode, TR::Compilation *comp)
   {
   TR_VirtualGuard *guard = self()->virtualGuardInfo();
   if (guard != NULL)
      {
      // dupNode may have just had its flags copied from self(), but the guard
      // itself hasn't been copied yet - that's what we're doing here. Clear
      // the flag before creating the new guard info so that it doesn't look
      // like there's a previous guard info to remove.
      dupNode->_flags.reset(inlineGuard);
      new (comp->trHeapMemory()) TR_VirtualGuard(guard, dupNode, comp);
      }
   }

bool
OMR::Node::isTheVirtualGuardForAGuardedInlinedCall()
   {
   return virtualGuardInfo() != NULL;
   }

bool
OMR::Node::isNopableInlineGuard()
   {
   TR::Compilation * c = TR::comp();
   return self()->isTheVirtualGuardForAGuardedInlinedCall() && !self()->isProfiledGuard() &&
      !(self()->isBreakpointGuard() && c->getOption(TR_DisableNopBreakpointGuard));
   }

bool
OMR::Node::vftEntryIsInBounds()
   {
   TR_ASSERT_FATAL_WITH_NODE(
      self(),
      self()->isTheVirtualGuardForAGuardedInlinedCall(),
      "vftEntryIsInBounds can only be queried on guards");

   return _flags.testAny(vftEntryIsInBoundsFlag);
   }

void
OMR::Node::setVFTEntryIsInBounds(bool inBounds)
   {
   TR_ASSERT_FATAL_WITH_NODE(
      self(),
      self()->isTheVirtualGuardForAGuardedInlinedCall(),
      "vftEntryIsInBounds can only be set on guards");

   _flags.set(vftEntryIsInBoundsFlag, inBounds);
   }

bool
OMR::Node::childrenWereSwapped()
   {
   return _flags.testAny(swappedChildren) && self()->getOpCode().isIf();
   }

void
OMR::Node::setSwappedChildren(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCode().isIf(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting swappedChildren flag on node %p to %d\n", self(), v))
      _flags.set(swappedChildren, v);
   }

bool
OMR::Node::isVersionableIfWithMaxExpr()
   {
   return _flags.testAny(versionIfWithMaxExpr) && self()->getOpCode().isIf();
   }

void
OMR::Node::setIsVersionableIfWithMaxExpr(TR::Compilation * c)
   {
   TR_ASSERT(self()->getOpCode().isIf(), "assertion failure");
   if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting versionIfWithMaxExpr flag on node %p\n", self()))
      _flags.set(versionIfWithMaxExpr);
   }

bool
OMR::Node::isVersionableIfWithMinExpr()
   {
   return _flags.testAny(versionIfWithMinExpr) && self()->getOpCode().isIf();
   }

void
OMR::Node::setIsVersionableIfWithMinExpr(TR::Compilation * c)
   {
   TR_ASSERT(self()->getOpCode().isIf(), "assertion failure");
   if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting versionIfWithMinExpr flag on node %p\n", self()))
      _flags.set(versionIfWithMinExpr);
   }

bool
OMR::Node::isStoreAlreadyEvaluated()
   {
   TR_ASSERT(self()->getOpCode().isStore(), "assertion failure");
   return _flags.testAny(storeAlreadyEvaluated);
   }

void
OMR::Node::setStoreAlreadyEvaluated(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isStore(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting storeAlreadyEvaluated flag on node %p to %d\n", self(), b))
      _flags.set(storeAlreadyEvaluated, b);
   }

bool
OMR::Node::chkStoreAlreadyEvaluated()
   {
   return self()->getOpCode().isStore() && _flags.testAny(storeAlreadyEvaluated);
   }

bool
OMR::Node::isSeenRealReference()
   {
   TR_ASSERT(self()->getOpCode().isLoadReg(), "assertion failure");
   return _flags.testAny(SeenRealReference);
   }

void
OMR::Node::setSeenRealReference(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isLoadReg(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting seenRealReference flag on node %p to %d\n", self(), b))
      _flags.set(SeenRealReference, b);
   }

bool
OMR::Node::chkSeenRealReference()
   {
   return self()->getOpCode().isLoadReg() && _flags.testAny(SeenRealReference);
   }

bool
OMR::Node::normalizeNanValues()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::fbits2i || self()->getOpCodeValue() == TR::dbits2l, "Opcode must be fbits2i or dbits2l");
   return _flags.testAny(mustNormalizeNanValues);
   }

void
OMR::Node::setNormalizeNanValues(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::fbits2i || self()->getOpCodeValue() == TR::dbits2l, "Opcode must be fbits2i or dbits2l");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting mustNormalizeNanValues flag on node %p to %d\n", self(), v))
      _flags.set(mustNormalizeNanValues, v);
   }

bool
OMR::Node::chkNormalizeNanValues()
   {
   return (self()->getOpCodeValue() == TR::fbits2i || self()->getOpCodeValue() == TR::dbits2l) && _flags.testAny(mustNormalizeNanValues);
   }

bool
OMR::Node::isHighWordZero()
   {
   TR_ASSERT(self()->getType().isInt64()  || self()->getType().isAddress(), "Can only call this for a long or address opcode\n");
   return _flags.testAny(highWordZero);
   }

void
OMR::Node::setIsHighWordZero(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getType().isInt64()  || self()->getType().isAddress(), "Can only call this for a long or address opcode\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting highWordZero flag on node %p to %d\n", self(), b))
      _flags.set(highWordZero, b);
   }

bool
OMR::Node::chkHighWordZero()
   {
   return (self()->getType().isInt64() || self()->getType().isAddress()) && _flags.testAny(highWordZero);
   }

bool
OMR::Node::isUnsigned()
   {
   TR_ASSERT(!self()->getType().isInt64(), "can only be used for nonlong opcodes");
   return _flags.testAny(Unsigned);
   }

void
OMR::Node::setUnsigned(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(!self()->getType().isInt64(), "can only be used for nonlong opcodes");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting unsigned flag on node %p to %d\n", self(), b))
      _flags.set(Unsigned, b);
   }

bool
OMR::Node::chkUnsigned()
   {
   return !self()->getType().isInt64() && !self()->getOpCode().isIf() && !self()->getOpCode().isReturn()
      && _flags.testAny(Unsigned) ;
   }

bool
OMR::Node::isClassPointerConstant()
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::aconst)||(self()->getOpCodeValue() == TR::aloadi), "Can only call this for aconst or iaload\n");
   return _flags.testAny(classPointerConstant);
   }

void
OMR::Node::setIsClassPointerConstant(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCodeValue() == TR::aconst), "Can only call this for aconst\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting classPointerConstant flag on node %p to %d\n", self(), b))
      _flags.set(classPointerConstant, b);
   }

bool
OMR::Node::chkClassPointerConstant()
   {
   return (self()->getOpCodeValue() == TR::aconst || self()->getOpCodeValue() == TR::aloadi)
      && _flags.testAny(classPointerConstant);
   }

bool
OMR::Node::isMethodPointerConstant()
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::aconst)||(self()->getOpCodeValue() == TR::aloadi), "Can only call this for aconst or iaload\n");
   return _flags.testAny(methodPointerConstant);
   }

void
OMR::Node::setIsMethodPointerConstant(bool b)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCodeValue() == TR::aconst), "Can only call this for aconst\n");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting methodPointerConstant flag on node %p to %d\n", self(), b))
      _flags.set(methodPointerConstant, b);
   }

bool
OMR::Node::chkMethodPointerConstant()
   {
   return (self()->getOpCodeValue() == TR::aconst || self()->getOpCodeValue() == TR::aloadi) && _flags.testAny(methodPointerConstant);
   }

bool
OMR::Node::isUnneededIALoad()
   {
   return (self()->getOpCodeValue() == TR::aloadi && _flags.testAny(unneededIALoad));
   }

void
OMR::Node::setUnneededIALoad(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::aloadi, "Can only call this for iaload");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting unneededIALoad flag on node %p to %d\n", self(), v))
      _flags.set(unneededIALoad, v);
   }



bool
OMR::Node::canSkipSync()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monexit || self()->getOpCodeValue() == TR::monent, "Opcode must be monexit/monent");
   return _flags.testAny(skipSync);
   }

void
OMR::Node::setSkipSync(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::monexit || self()->getOpCodeValue() == TR::monent, "Opcode must be monexit/monent");
   if (performTransformation(c, "O^O NODE FLAGS: Setting skipSync flag on node %p to %d\n", self(), v))
      _flags.set(skipSync, v);
   }

bool
OMR::Node::chkSkipSync()
   {
   return(self()->getOpCodeValue() == TR::monexit || self()->getOpCodeValue() == TR::monent) && _flags.testAny(skipSync);
   }

bool
OMR::Node::isStaticMonitor()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit || self()->getOpCodeValue() == TR::tstart, "Opcode must be monent or monexit or tstart");
   return _flags.testAny(staticMonitor);
   }

void
OMR::Node::setStaticMonitor(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting staticMonitor flag on node %p to %d\n", self(), v))
      _flags.set(staticMonitor, v);
   }

bool
OMR::Node::chkStaticMonitor()
   {
   return (self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit) && _flags.testAny(staticMonitor);
   }

bool
OMR::Node::isSyncMethodMonitor()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   return _flags.testAny(syncMethodMonitor);
   }

void
OMR::Node::setSyncMethodMonitor(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting syncMethodMonitor flag on node %p to %d\n", self(), v))
      _flags.set(syncMethodMonitor, v);
   }

bool
OMR::Node::chkSyncMethodMonitor()
   {
   return (self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit) && _flags.testAny(syncMethodMonitor);
   }

bool
OMR::Node::isReadMonitor()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   return _flags.testAny(readMonitor);
   }

void
OMR::Node::setReadMonitor(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting readMonitor flag on node %p to %d\n", self(), v))
      _flags.set(readMonitor, v);
   }

bool
OMR::Node::chkReadMonitor()
   {
   return (self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit) && _flags.testAny(readMonitor);
   }

bool
OMR::Node::isLocalObjectMonitor()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   return _flags.testAny(localObjectMonitor);
   }

void
OMR::Node::setLocalObjectMonitor(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting localObjectMonitor flag on node %p to %d\n", self(), v))
      _flags.set(localObjectMonitor, v);
   }

bool
OMR::Node::chkLocalObjectMonitor()
   {
   return (self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit) && _flags.testAny(localObjectMonitor);
   }

bool
OMR::Node::isPrimitiveLockedRegion()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   return _flags.testAny(primitiveLockedRegion);
   }

void
OMR::Node::setPrimitiveLockedRegion(bool v)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit, "Opcode must be monent or monexit");
   _flags.set(primitiveLockedRegion, v);
   }

bool
OMR::Node::chkPrimitiveLockedRegion()
   {
   return (self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit) && _flags.testAny(primitiveLockedRegion);
   }

bool
OMR::Node::hasMonitorClassInNode()
   {
   return _flags.testAny(monitorClassInNode);
   }

void
OMR::Node::setHasMonitorClassInNode(bool v)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::tstart || self()->getOpCodeValue() == TR::monent || self()->getOpCodeValue() == TR::monexit,"Opcode must be monent, monexit or tstart");
   _flags.set(monitorClassInNode,v);
   }



bool
OMR::Node::markedAllocationCanBeRemoved()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::New || self()->getOpCodeValue() == TR::newarray ||
             self()->getOpCodeValue() == TR::anewarray || self()->getOpCodeValue() == TR::newvalue, "Opcode must be new, newarray, anewarray, or newvalue");
   return _flags.testAny(allocationCanBeRemoved);
   }

void
OMR::Node::setAllocationCanBeRemoved(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::New || self()->getOpCodeValue() == TR::newarray ||
             self()->getOpCodeValue() == TR::anewarray || self()->getOpCodeValue() == TR::newvalue, "Opcode must be new, newarray, anewarray, or newvalue");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting allocationCanBeRemoved flag on node %p to %d\n", self(), v))
      _flags.set(allocationCanBeRemoved, v);
   }

bool
OMR::Node::chkAllocationCanBeRemoved()
   {
   return ((self()->getOpCodeValue() == TR::New ||
            self()->getOpCodeValue() == TR::newarray ||
            self()->getOpCodeValue() == TR::anewarray ||
            self()->getOpCodeValue() == TR::newvalue)
           && _flags.testAny(allocationCanBeRemoved));
   }

bool
OMR::Node::canSkipZeroInitialization()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::New ||
             self()->getOpCodeValue() == TR::variableNew || self()->getOpCodeValue() == TR::variableNewArray ||
             self()->getOpCodeValue() == TR::newarray || self()->getOpCodeValue() == TR::anewarray ||
             self()->getOpCodeValue() == TR::multianewarray,
             "Opcode must be newarray or anewarray");
   return _flags.testAny(skipZeroInit);
   }

void
OMR::Node::setCanSkipZeroInitialization(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::New ||
             self()->getOpCodeValue() == TR::newarray || self()->getOpCodeValue() == TR::anewarray  ||
             self()->getOpCodeValue() == TR::variableNewArray || self()->getOpCodeValue() == TR::variableNew ||
             self()->getOpCodeValue() == TR::multianewarray,
             "Opcode must be newarray or anewarray");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting skipZeroInit flag on node %p to %d\n", self(), v))
      _flags.set(skipZeroInit, v);
   }

bool
OMR::Node::chkSkipZeroInitialization()
   {
   return _flags.testAny(skipZeroInit) &&
      (self()->getOpCodeValue() == TR::New || self()->getOpCodeValue() == TR::newarray || self()->getOpCodeValue() == TR::anewarray ||
       self()->getOpCodeValue() == TR::multianewarray);
   }

bool
OMR::Node::isAdjunct()
   {
   return (self()->getOpCodeValue() == TR::lmul
   || self()->getOpCodeValue() == TR::ladd || self()->getOpCodeValue() == TR::lsub) && _flags.testAny(adjunct);
   }

void
OMR::Node::setIsAdjunct(bool v)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::lmul
   || self()->getOpCodeValue() == TR::ladd || self()->getOpCodeValue() == TR::lsub, "Opcode must be lmul, ladd or lsub");
   _flags.set(adjunct, v);
   }



bool
OMR::Node::cannotTrackLocalUses()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Opcode must be loadaddr");
   return _flags.testAny(cantTrackLocalUses);
   }

void
OMR::Node::setCannotTrackLocalUses(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Opcode must be loadaddr");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting cannotTrack flag on node %p to %d\n", self(), v))
      _flags.set(cantTrackLocalUses, v);
   }

bool
OMR::Node::chkCannotTrackLocalUses()
   {
   return self()->getOpCodeValue() == TR::loadaddr && _flags.testAny(cantTrackLocalUses);
   }

bool
OMR::Node::escapesInColdBlock()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Opcode must be loadaddr");
   return _flags.testAny(coldBlockEscape);
   }

void
OMR::Node::setEscapesInColdBlock(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Opcode must be loadaddr");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting escapesInColdBlock flag on node %p to %d\n", self(), v))
      _flags.set(coldBlockEscape, v);
   }

bool
OMR::Node::chkEscapesInColdBlock()
   {
   return self()->getOpCodeValue() == TR::loadaddr && _flags.testAny(coldBlockEscape);
   }

bool
OMR::Node::cannotTrackLocalStringUses()
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Opcode must be loadaddr (flag shared on PLX)");
   return _flags.testAny(cantTrackLocalStringUses);
   }

void
OMR::Node::setCannotTrackLocalStringUses(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::loadaddr, "Opcode must be loadaddr (flag shared on PLX)");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting cannotTrackString flag on node %p to %d\n", self(), v))
      _flags.set(cantTrackLocalStringUses, v);
   }

bool
OMR::Node::chkCannotTrackLocalStringUses()
   {
   return self()->getOpCodeValue() == TR::loadaddr && _flags.testAny(cantTrackLocalStringUses);
   }

bool
OMR::Node::canOmitSync()
   {
   TR_ASSERT((self()->getOpCodeValue() == TR::allocationFence) || (self()->getOpCodeValue() == TR::fullFence), "Opcode must be TR::allocationFence or TR::fullFence");
   return _flags.testAny(omitSync);
   }

void
OMR::Node::setOmitSync(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCodeValue() == TR::allocationFence) || (self()->getOpCodeValue() == TR::fullFence), "Opcode must be TR::allocationFence or TR::fullFence");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting omitSync flag on node %p to %d\n", self(), v))
      _flags.set(omitSync, v);
   }

bool
OMR::Node::chkOmitSync()
   {
   return (self()->getOpCodeValue() == TR::allocationFence) && _flags.testAny(omitSync);
   }

bool
OMR::Node::isSimpleDivCheck()
   {
   TR_ASSERT((self()->getOpCode().isDiv() || self()->getOpCode().isRem()), "Opcode must be div/rem");
   return _flags.testAny(simpleDivCheck);
   }

void
OMR::Node::setSimpleDivCheck(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCode().isDiv() || self()->getOpCode().isRem()), "Opcode must be div/rem");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting simpleDivCheck flag on node %p to %d\n", self(), v))
      _flags.set(simpleDivCheck, v);
   }

bool
OMR::Node::chkSimpleDivCheck()
   {
   return (self()->getOpCode().isDiv() || self()->getOpCode().isRem()) && _flags.testAny(simpleDivCheck);
   }

bool
OMR::Node::isNormalizedShift()
   {
   TR_ASSERT((self()->getOpCode().isLeftShift() || self()->getOpCode().isRightShift() || self()->getOpCode().isRotate()), "Opcode must be shl or shr or rol");
   return _flags.testAny(normalizedShift);
   }

void
OMR::Node::setNormalizedShift(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCode().isLeftShift() || self()->getOpCode().isRightShift() || self()->getOpCode().isRotate()), "Opcode must be shl or shr");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting normalizedShift flag on node %p to %d\n", self(), v))
      _flags.set(normalizedShift, v);
   }

bool
OMR::Node::chkNormalizedShift()
   {
   return (self()->getOpCode().isLeftShift() || self()->getOpCode().isRightShift()) && _flags.testAny(normalizedShift);
   }

bool
OMR::Node::isFPStrictCompliant()
   {
   return self()->getOpCode().isMul() && _flags.testAny(resultFPStrictCompliant);
   }

void
OMR::Node::setIsFPStrictCompliant(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCode().isMul()), "Opcode must be multiply");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting resultFPStrictCompliant flag on node %p to %d\n", self(), v))
      _flags.set(resultFPStrictCompliant, v);
   }

bool
OMR::Node::isUnneededConversion()
   {
   return (self()->getOpCode().isConversion() && _flags.testAny(unneededConv));
   }

void
OMR::Node::setUnneededConversion(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT((self()->getOpCode().isConversion()), "Opcode must be a conv");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting unneededConversion flag on node %p to %d\n", self(), v))
      _flags.set(unneededConv, v);
   }

bool
OMR::Node::parentSupportsLazyClobber()
   {
   return self()->getOpCode().isConversion() && _flags.testAny(ParentSupportsLazyClobber);
   }

void
OMR::Node::setParentSupportsLazyClobber(bool v)
   {
   TR_ASSERT(self()->getOpCode().isConversion(), "Opcode must be a conv");
   TR_ASSERT(self()->getReferenceCount() <= 1, "Lazy clobber requires node reference count of 1 (actual value is %d)", self()->getReferenceCount());
   _flags.set(ParentSupportsLazyClobber, v);
   }

void
OMR::Node::oneParentSupportsLazyClobber(TR::Compilation * comp)
   {
   if (self()->getOpCode().isConversion() && self()->getReferenceCount() <= 1
      && performTransformation(comp, "O^O LAZY CLOBBERING: setParentSupportsLazyClobber(%s)\n", comp->getDebug()->getName(self())))
      self()->setParentSupportsLazyClobber(true);
   }



bool
OMR::Node::useCallForFloatToFixedConversion()
   {
   TR_ASSERT(self()->isFloatToFixedConversion(), "Opcode must be a float to fixed conversion");
   return self()->isFloatToFixedConversion() && _flags.testAny(callForFloatToFixedConversion);
   }

void
OMR::Node::setUseCallForFloatToFixedConversion(bool v)
   {
   TR_ASSERT(self()->isFloatToFixedConversion(), "Opcode must be a float to fixed conversion");
   if (self()->isFloatToFixedConversion())
      _flags.set(callForFloatToFixedConversion, v);
   }

bool
OMR::Node::chkUseCallForFloatToFixedConversion()
   {
   return self()->isFloatToFixedConversion() && _flags.testAny(callForFloatToFixedConversion);
   }

bool
OMR::Node::isLoadFence()
   {
   TR_ASSERT(self()->getOpCode().isFence(), "Opcode must be a fence");
   return _flags.testAny(fenceLoad);
   }

void
OMR::Node::setIsLoadFence()
   {
   TR_ASSERT(self()->getOpCode().isFence(), "Opcode must be a fence");
   _flags.set(fenceLoad);
   }



bool
OMR::Node::isStoreFence()
   {
   TR_ASSERT(self()->getOpCode().isFence(), "Opcode must be a fence");
   return _flags.testAny(fenceStore);
   }

void
OMR::Node::setIsStoreFence()
   {
   TR_ASSERT(self()->getOpCode().isFence(), "Opcode must be a fence");
   _flags.set(fenceStore);
   }



bool
OMR::Node::isReturnDummy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::Return,  "Opcode must be return");
   return _flags.testAny(returnIsDummy);
   }

void
OMR::Node::setReturnIsDummy()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::Return, "Opcode must be return");
   _flags.set(returnIsDummy);
   }

bool
OMR::Node::chkReturnIsDummy()
   {
   return (self()->getOpCodeValue() == TR::Return) && _flags.testAny(returnIsDummy);
   }

bool
OMR::Node::isBigDecimalLoad()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::lloadi, "assertion failure");
   return _flags.testAny(bigDecimal_load);
   }

void
OMR::Node::setIsBigDecimalLoad()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::lloadi, "assertion failure");
   _flags.set(bigDecimal_load);
   }



bool
OMR::Node::shouldAlignTLHAlloc()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::New, "Opcode value must be TR::New");
   return _flags.testAny(alignTLHAlloc);
   }

void
OMR::Node::setAlignTLHAlloc(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCodeValue() == TR::New, "Opcode value must be TR::New");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting align on TLH flag on node %p to %d\n", self(), v))
      _flags.set(alignTLHAlloc, v);
   }



bool
OMR::Node::isCopyToNewVirtualRegister()
   {
   return self()->getOpCodeValue() == TR::PassThrough
      && _flags.testAny(copyToNewVirtualRegister);
   }

void
OMR::Node::setCopyToNewVirtualRegister(bool v)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::PassThrough, "can only set copyToNewVirtualRegister flag on PassThrough\n");
   _flags.set(copyToNewVirtualRegister, v);
   }

bool
OMR::Node::nodeRequiresConditionCodes()
   {
   TR::ILOpCode op = self()->getOpCode();
   return self()->chkOpsNodeRequiresConditionCodes() ? _flags.testAny(requiresConditionCodes) : false;
   }

void
OMR::Node::setNodeRequiresConditionCodes(bool v)
   {
   TR::ILOpCode op = self()->getOpCode();
   TR_ASSERT(self()->chkOpsNodeRequiresConditionCodes(), "setNodeRequiresConditionCodes not supported on node %s (%p)\n", self()->getOpCode().getName(), self());
   _flags.set(requiresConditionCodes, v);
   }

bool
OMR::Node::chkOpsNodeRequiresConditionCodes()
   {
   TR::ILOpCode op = self()->getOpCode();
   return op.isArithmetic() || op.isLoadConst() || op.isOverflowCheck();
   }

static void
resetFlagsAndPropertiesForCodeMotionHelper(TR::Node *node, TR::NodeChecklist &visited)
   {
   if (visited.contains(node))
      return;

   visited.add(node);

   int32_t childNum;
   for (childNum = 0; childNum < node->getNumChildren(); childNum++)
      resetFlagsAndPropertiesForCodeMotionHelper(node->getChild(childNum), visited);

   if (node->getOpCodeValue() != TR::loadaddr)
      {
      node->setIsNull(false);
      node->setIsNonNull(false);
      }
   else
      {
      node->setPointsToNull(false);
      node->setPointsToNonNull(false);
      }

   node->setIsZero(false);
   node->setIsNonZero(false);

   node->setIsNonNegative(false);
   node->setIsNonPositive(false);
   if (node->chkCannotOverflow())
      node->setCannotOverflow(false);
   if (node->chkHighWordZero())
      node->setIsHighWordZero(false);
   if (node->getOpCode().canUseBranchOnCount() && node->isUseBranchOnCount())
      node->setIsUseBranchOnCount(false);

   if (node->chkIsReferenceNonNull())
      node->setReferenceIsNonNull(false);

   if (node->hasKnownObjectIndex())
      node->setKnownObjectIndex(TR::KnownObjectTable::UNKNOWN);

   if (node->isTheVirtualGuardForAGuardedInlinedCall())
      node->setVFTEntryIsInBounds(false);
   }

// Clear out relevant flags set on the node; this will ensure
// flags are not copied incorrectly. This is required as code motion could place
// a node where these properties are not true. Can be used to reset other properties
// as well (over the ones that are already reset).
//
void
OMR::Node::resetFlagsAndPropertiesForCodeMotion()
   {
   TR::NodeChecklist visited(TR::comp());
   resetFlagsAndPropertiesForCodeMotionHelper(self(), visited);
   }

/**
 * Node flags functions end
 */

bool
OMR::Node::isLoadOfStaticFinalField()
   {
   if (self()->hasSymbolReference())
      {
      TR::Symbol *sym = self()->getSymbol();
      if (sym->isFinal() &&
          sym->isStaticField())
         return true;
      }
   return false;
   }

bool
OMR::Node::isOSRFearPointHelperCall()
   {
   TR::Compilation *c = TR::comp();

   if (self()->getOpCode().isCall()
       && self()->getSymbol()->isMethod()
       && c->getSymRefTab()->isNonHelper(self()->getSymbolReference(), TR::SymbolReferenceTable::osrFearPointHelperSymbol))
      return true;

   return false;
   }

bool
OMR::Node::isPotentialOSRPointHelperCall()
   {
   TR::Compilation *c = TR::comp();

   if (self()->getOpCode().isCall()
       && self()->getSymbol()->isMethod()
       && c->getSymRefTab()->isNonHelper(self()->getSymbolReference(), TR::SymbolReferenceTable::potentialOSRPointHelperSymbol))
      return true;

   return false;
   }

bool
OMR::Node::isEAEscapeHelperCall()
   {
   TR::Compilation *c = TR::comp();

   if (self()->getOpCode().isCall()
       && self()->getSymbol()->isMethod()
       && c->getSymRefTab()->isNonHelper(self()->getSymbolReference(), TR::SymbolReferenceTable::eaEscapeHelperSymbol))
      return true;

   return false;
   }
