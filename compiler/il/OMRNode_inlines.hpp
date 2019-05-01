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

#ifndef OMR_NODE_INLINE_INCL
#define OMR_NODE_INLINE_INCL

#include "il/OMRNode.hpp"

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Environment.hpp"
#include "env/IO.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

namespace TR { class TreeTop; }

/*
 * Performance sensitive implementations are included here to support inlining.
 *
 * This file will be included by Node_inlines.hpp
 */

TR::Node *
OMR::Node::self()
   {
   return static_cast<TR::Node*>( this );
   }

/**
 * Node constructors
 */

#if !defined(_MSC_VER) && !defined(LINUXPPC)
/*
 * These are defined for compilers that support variadic templates
 * XLC (but not on LINUX PPC) and GCC support C++11 variadic templates
 */

template <class...ChildrenAndSymRefType>
uint16_t
OMR::Node::addChildrenAndSymRef(uint16_t childIndex, TR::Node *child,
                                ChildrenAndSymRefType... childrenAndSymRef)
   {
   self()->addChildrenAndSymRef(childIndex, child);
   return addChildrenAndSymRef(childIndex + 1, childrenAndSymRef...);
   }

template <class...ChildrenAndSymRefType>
TR::Node *
OMR::Node::recreateWithSymRefWithoutProperties(TR::Node *originalNode, TR::ILOpCodes op,
                              uint16_t numChildren, uint16_t numChildArgs,
                              TR::Node *first,
                              ChildrenAndSymRefType... childrenAndSymRef)
   {
   TR::Node *node = TR::Node::createInternal(first, op, numChildren, originalNode);
   uint16_t numChildArgsActual = node->addChildrenAndSymRef(0, first, childrenAndSymRef...);
   TR_ASSERT(numChildArgs == numChildArgsActual, "numChildArgs = %d does not match actual number of child args = %d",
             numChildArgs, numChildArgsActual);
   return node;
   }

template <class...Children>
TR::Node *
OMR::Node::recreateWithoutSymRef(TR::Node *originalNode, TR::ILOpCodes op,
                                 uint16_t numChildren, uint16_t numChildArgs,
                                 TR::Node *first, Children... children)
   {
   return recreateWithSymRefWithoutProperties(originalNode, op, numChildren, numChildArgs,
                             first, children...);
   }

template <class...ChildrenAndSymRefType>
TR::Node *
OMR::Node::createWithSymRefInternal(TR::ILOpCodes op, uint16_t numChildren,
                                    uint16_t numChildArgs, TR::Node *first,
                                    ChildrenAndSymRefType... childrenAndSymRef)
   {
   return recreateWithSymRefWithoutProperties(0, op, numChildren, numChildArgs, first,
                             childrenAndSymRef...);
   }
template <class...Children>
TR::Node *
OMR::Node::createWithoutSymRef(TR::ILOpCodes op, uint16_t numChildren,
                               uint16_t numChildArgs, TR::Node *first,
                               Children... children)
   {
   return createWithSymRefInternal(op, numChildren, numChildArgs, first, children...);
   }

template <class...ChildrenAndSymRefType>
TR::Node *
OMR::Node::createWithSymRef(TR::ILOpCodes op, uint16_t numChildren,
                            uint16_t numChildArgs, TR::Node *first,
                            ChildrenAndSymRefType... childrenAndSymRef)
   {
   TR_ASSERT(TR::Node::isLegalCallToCreate(op), "assertion failure");
   return createWithSymRefInternal(op, numChildren, numChildArgs, first, childrenAndSymRef...);
   }

#endif // Variadic template support

/**
 * Node constructors end
 */

/**
 * Misc public functions
 */

// A common query used by the optimizer
bool
OMR::Node::isSingleRef()
   {
   return self()->getReferenceCount() == 1;
   }

// A common query used by the code generators
bool
OMR::Node::isSingleRefUnevaluated()
   {
   return self()->isSingleRef() && !self()->getRegister();
   }

int32_t
OMR::Node::getNumArguments()
   {
   return self()->getNumChildren() - self()->getFirstArgumentIndex();
   }

TR::Node*
OMR::Node::getArgument(int32_t index)
   {
   return self()->getChild(self()->getFirstArgumentIndex() + index);
   }

TR::Node*
OMR::Node::getFirstArgument()
   {
   return self()->getArgument(0);
   }

TR::DataType
OMR::Node::getType()
   {
   return self()->getDataType();
   }

TR_NodeUseAliasSetInterface
OMR::Node::mayUse()
   {
   TR_NodeUseAliasSetInterface aliasSetInterface(self());
   return aliasSetInterface;
   }

TR_NodeKillAliasSetInterface
OMR::Node::mayKill(bool gcSafe)
   {
   TR_NodeKillAliasSetInterface aliasSetInterface(self(), self()->getOpCode().isCallDirect(), gcSafe);
   return aliasSetInterface;
   }

/**
 * Misc public functions end
 */

/**
 * Node field functions
 */

inline TR::Node *
OMR::Node::getChild(int32_t c)
   {
   if(!self()->hasNodeExtension())
      {
      TR_ASSERT(c < NUM_DEFAULT_CHILDREN, "getChild(%d) called on node " POINTER_PRINTF_FORMAT " with %d children.", c, self(), self()->getNumChildren());
      return _unionBase._children[c];
      }
   else
      {
      return self()->getExtendedChild(c);
      }
   }

inline TR::Node *
OMR::Node::getFirstChild()
   {
   TR_ASSERT(self()->getNumChildren() >= 1, "getFirstChild() called on node " POINTER_PRINTF_FORMAT " with no children", self());
   return self()->getChild(0);
   }

inline TR::Node *
OMR::Node::getLastChild()
   {
   TR_ASSERT(self()->getNumChildren() >= 1, "getLastChild() called on node " POINTER_PRINTF_FORMAT " with no children", self());
   return self()->getChild(_numChildren - 1);
   }

inline TR::Node *
OMR::Node::getSecondChild()
   {
   TR_ASSERT(self()->getNumChildren() >= 2, "getSecondChild() called on node " POINTER_PRINTF_FORMAT " with less than 2 children", self());
   return self()->getChild(1);
   }

inline TR::Node *
OMR::Node::getThirdChild()
   {
   TR_ASSERT(self()->getNumChildren() >= 3, "getThirdChild() called on node " POINTER_PRINTF_FORMAT " with less than 3 children", self());
   return self()->getChild(2);
   }

inline OMR::Node::ChildIterator
OMR::Node::childIterator(int32_t startIndex)
   {
   return ChildIterator(self(), startIndex);
   }

inline TR::Node *
OMR::Node::ChildIterator::currentChild()
   {
   if (_index < _node->getNumChildren())
      return _node->getChild(_index);
   else
      return NULL;
   }

inline void
OMR::Node::ChildIterator::stepForward()
   {
   if (_index < _node->getNumChildren())
      ++_index;
   }

/**
 * Node field functions end
 */

/**
 * OptAttributes functions
 */

vcount_t
OMR::Node::getVisitCount()
   {
   return _visitCount;
   }

vcount_t
OMR::Node::setVisitCount(vcount_t vc)
   {
   return (_visitCount = vc);
   }

vcount_t
OMR::Node::incVisitCount()
   {
   ++_visitCount;
   TR_ASSERT(_visitCount > 0, "Assertion failure : %s (0x%p)", self()->getOpCode().getName(), this);
   return _visitCount;
   }

rcount_t
OMR::Node::getReferenceCount()
   {
   return _referenceCount;
   }

rcount_t
OMR::Node::setReferenceCount(rcount_t rc)
   {
   return (_referenceCount = rc);
   }

rcount_t
OMR::Node::incReferenceCount()
   {
   ++(_referenceCount);
   TR_ASSERT(_referenceCount > 0, "Assertion failure : %s (0x%p)", self()->getOpCode().getName(), this);
   return _referenceCount;
   }

rcount_t
OMR::Node::decReferenceCount()
   {
   TR_ASSERT(_referenceCount > 0 || self()->getOpCode().isTreeTop(), "Assertion failure : %s (0x%p)", self()->getOpCode().getName(), this);
   return --(_referenceCount);
   }

scount_t
OMR::Node::getLocalIndex()
   {
   return _localIndex;
   }

scount_t
OMR::Node::setLocalIndex(scount_t li)
   {
   return (_localIndex = li);
   }

scount_t
OMR::Node::incLocalIndex()
   {
   ++_localIndex;
   TR_ASSERT(_localIndex>0, "assertion failure"); return _localIndex;
   }

scount_t
OMR::Node::decLocalIndex()
   {
   TR_ASSERT(_localIndex > 0, "assertion failure"); return --_localIndex;
   }

scount_t
OMR::Node::getFutureUseCount()
   {
   return _localIndex;
   }

scount_t
OMR::Node::setFutureUseCount(scount_t li)
   {
   return (_localIndex = (scount_t)li);
   }

scount_t
OMR::Node::incFutureUseCount()
   {
   ++_localIndex;
   return _localIndex;
   }

scount_t
OMR::Node::decFutureUseCount()
   {
   return --_localIndex;
   }

uint16_t
OMR::Node::getUseDefIndex()
   {
   return _unionA._useDefIndex;
   }

uint16_t
OMR::Node::setUseDefIndex(uint16_t udi)
   {
   return (_unionA._useDefIndex = udi);
   }

/**
 * OptAttributes functions end
 */

/**
 * UnionBase functions
 */

int32_t
OMR::Node::getOSRInductionOffset()
   {
   TR_ASSERT(self()->isPotentialOSRPointHelperCall(), "TR::Node::getOSRInductionOffset: used for a non potentialOSRPointHelper call node n%dn %p", self()->getGlobalIndex(), self());
   return (int32_t)_unionBase._osrInductionOffset;
   }

int32_t
OMR::Node::setOSRInductionOffset(int32_t offset)
   {
   TR_ASSERT(self()->isPotentialOSRPointHelperCall(), "TR::Node::setOSRInductionOffset: used for a non potentialOSRPointHelper call node n%dn %p", self()->getGlobalIndex(), self());
   return (int32_t)(_unionBase._osrInductionOffset = (int64_t)offset);
   }

int64_t
OMR::Node::getConstValue()
   {
   return _unionBase._constValue;
   }

uint64_t
OMR::Node::getUnsignedConstValue()
   {
   return (uint64_t)_unionBase._constValue;
   }

void
OMR::Node::setConstValue(int64_t val)
   {
   self()->freeExtensionIfExists();
   _unionBase._constValue = val;
   }

int64_t
OMR::Node::getLongInt()
   {
   return (int64_t)_unionBase._constValue;
   }

int64_t
OMR::Node::setLongInt(int64_t li)
   {
   self()->freeExtensionIfExists();
   return (int64_t)(_unionBase._constValue = (int64_t)li);
   }

int32_t
OMR::Node::getLongIntLow()
   {
   return (int32_t)self()->getLongInt();
   }

int32_t
OMR::Node::getLongIntHigh()
   {
   return (int32_t)(self()->getLongInt() >> 32);
   }

uint64_t
OMR::Node::getUnsignedLongInt()
   {
   return (uint64_t)_unionBase._constValue;
   }

uint64_t
OMR::Node::setUnsignedLongInt(uint64_t uli)
   {
   self()->freeExtensionIfExists();
   return (uint64_t)(_unionBase._constValue = (int64_t)uli);
   }

uint32_t
OMR::Node::getUnsignedLongIntLow()
   {
   return (uint32_t)self()->getUnsignedLongInt();
   }

uint32_t
OMR::Node::getUnsignedLongIntHigh()
   {
   return (uint32_t)(self()->getUnsignedLongInt() >> 32);
   }

int32_t
OMR::Node::getInt()
   {
   TR_ASSERT(self()->getOpCodeValue() != TR::fconst, "TR::Node::getInt: used for an fconst node");
   return (int32_t)_unionBase._constValue;
   }

int32_t
OMR::Node::setInt(int32_t i)
   {
   TR_ASSERT(self()->getOpCodeValue() != TR::fconst, "TR::Node::setInt: used for an fconst node");
   self()->freeExtensionIfExists();
   return  _unionBase._constValue= (int64_t)i;
   }

uint32_t
OMR::Node::getUnsignedInt()
   {
   return (uint32_t)_unionBase._constValue;
   }

uint32_t
OMR::Node::setUnsignedInt(uint32_t ui)
   {
   self()->freeExtensionIfExists();
   return (uint32_t)(_unionBase._constValue = (int64_t)ui);
   }

int16_t
OMR::Node::getShortInt()
   {
   return (int16_t)_unionBase._constValue;
   }

int16_t
OMR::Node::setShortInt(int16_t si)
   {
   self()->freeExtensionIfExists();
   return (int16_t)(_unionBase._constValue = (int64_t)si);
   }

uint16_t
OMR::Node::getUnsignedShortInt()
   {
   return (uint16_t)_unionBase._constValue;
   }

uint16_t
OMR::Node::setUnsignedShortInt(uint16_t c)
   {
   self()->freeExtensionIfExists();
   return (uint16_t)(_unionBase._constValue = (int64_t)c);
   }

int8_t
OMR::Node::getByte()
   {
   return (int8_t)_unionBase._constValue;
   }

int8_t
OMR::Node::setByte(int8_t b)
   {
   self()->freeExtensionIfExists();
   return (int8_t)(_unionBase._constValue = (int64_t)b);
   }

uint8_t
OMR::Node::getUnsignedByte()
   {
   return (uint8_t)_unionBase._constValue;
   }

uint8_t
OMR::Node::setUnsignedByte(uint8_t b)
   {
   self()->freeExtensionIfExists();
   return (uint8_t)(_unionBase._constValue = (int64_t)b);
   }

float
OMR::Node::getFloat()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::fconst, "TR::Node::getFloat: used for a non fconst node");
   return _unionBase._fpConstValue;
   }

float
OMR::Node::setFloat(float f)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::fconst, "TR::Node::setFloat: used for a non fconst node");
   self()->freeExtensionIfExists();
   return (_unionBase._fpConstValue = f);
   }

uint32_t
OMR::Node::setFloatBits(uint32_t f)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::fconst,"TR::Node::setFloat: used for a non fconst node");
   self()->freeExtensionIfExists();
   return (_unionBase._fpConstValueBits = f);
   }

uint32_t
OMR::Node::getFloatBits()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::fconst,"TR::Node::getFloat: used for a non fconst node");
   return _unionBase._fpConstValueBits;
   }

double
OMR::Node::getDouble()
   {
   return _unionBase._dpConstValue;
   }

double
OMR::Node::setDouble(double d)
   {
   self()->freeExtensionIfExists();
   return (_unionBase._dpConstValue = d);
   }

uint64_t
OMR::Node::getDoubleBits()
   {
   return self()->getUnsignedLongInt();
   }

uint64_t
OMR::Node::setDoubleBits(uint64_t d)
   {
   return self()->setUnsignedLongInt(d);
   }

uint64_t
OMR::Node::getAddress()
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::aconst,"TR::Node::getAddress: used for a non aconst node");
   return (uint64_t)_unionBase._constValue;
   }

uint64_t
OMR::Node::setAddress(uint64_t a)
   {
   TR_ASSERT(self()->getOpCodeValue() == TR::aconst,"TR::Node::setAddress: used for a non aconst node");
   self()->freeExtensionIfExists();

   if (TR::Compiler->target.is32Bit())
      a = a & 0x00000000ffffffff;

   return (uint64_t)(_unionBase._constValue = (int64_t)a);
   }

namespace OMR {
template <> inline  uint8_t Node::getConst< uint8_t>() { return self()->getUnsignedByte(); }
template <> inline   int8_t Node::getConst<  int8_t>() { return self()->getByte(); }
template <> inline uint16_t Node::getConst<uint16_t>() { return self()->getUnsignedShortInt(); }
template <> inline  int16_t Node::getConst< int16_t>() { return self()->getShortInt(); }
template <> inline uint32_t Node::getConst<uint32_t>() { return self()->getUnsignedInt(); }
template <> inline  int32_t Node::getConst< int32_t>() { return self()->getInt(); }
template <> inline uint64_t Node::getConst<uint64_t>() { return self()->getUnsignedLongInt(); }
template <> inline  int64_t Node::getConst< int64_t>() { return self()->getLongInt(); }
template <> inline    float Node::getConst<   float>() { return self()->getFloat(); }
template <> inline   double Node::getConst<  double>() { return self()->getDouble(); }

template <> inline  uint8_t Node::setConst< uint8_t>( uint8_t b) { return self()->setUnsignedByte(b); }
template <> inline   int8_t Node::setConst<  int8_t>(  int8_t b) { return self()->setByte(b); }
template <> inline uint16_t Node::setConst<uint16_t>(uint16_t c) { return self()->setUnsignedShortInt(c); }
template <> inline  int16_t Node::setConst< int16_t>( int16_t s) { return self()->setShortInt(s); }
template <> inline uint32_t Node::setConst<uint32_t>(uint32_t i) { return self()->setUnsignedInt(i); }
template <> inline  int32_t Node::setConst< int32_t>( int32_t i) { return self()->setInt(i); }
template <> inline uint64_t Node::setConst<uint64_t>(uint64_t l) { return self()->setUnsignedLongInt(l); }
template <> inline  int64_t Node::setConst< int64_t>( int64_t l) { return self()->setLongInt(l); }
template <> inline    float Node::setConst<   float>(   float f) { return self()->setFloat(f); }
template <> inline   double Node::setConst<  double>(  double d) { return self()->setDouble(d); }
}

template <class T> T
OMR::Node::getIntegerNodeValue()
   {
   TR::Compilation *comp = TR::comp();
   T length = 0;
   TR::ILOpCodes opcode = self()->getOpCodeValue();
   if (opcode == TR::aconst)
      {
      if (TR::Compiler->target.is64Bit())
         opcode = TR::lconst;
      else
         opcode = TR::iconst;
      }
   switch(opcode)
      {
      case TR::buconst:
      case TR::bconst: length = (T)self()->getUnsignedByte(); break;
      case TR::cconst: length = (T)self()->getConst<uint16_t>(); break;
      case TR::sconst: length = (T)self()->getShortInt(); break;
      case TR::iconst:
      case TR::iuconst:
         length = (T)self()->getUnsignedInt(); break;
      case TR::lconst:
      case TR::luconst:
         length = (T)self()->getUnsignedLongInt(); break;
      case TR::iloadi:
         TR_ASSERT(false, "Invalid type for length node."); break;
      default:
         TR_ASSERT(false, "Invalid type for length node %p",this); break;
      }
   return length;
   }

/**
 * UnionBase functions end
 */

/**
 * UnionPropertyA functions
 */

TR::TreeTop*
OMR::Node::getBranchDestination()
   {
   TR_ASSERT(self()->hasBranchDestinationNode(), "attempting to access _branchDestinationNode field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return _unionPropertyA._branchDestinationNode;
   }

TR::TreeTop*
OMR::Node::setBranchDestination(TR::TreeTop * p)
   {
   TR_ASSERT(self()->hasBranchDestinationNode(), "attempting to access _branchDestinationNode field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return (_unionPropertyA._branchDestinationNode = p);
   }

TR::DataType
OMR::Node::getDataType()
   {
   if (_opCode.hasNoDataType())
      return self()->computeDataType();
   return _opCode.getDataType();
   }

TR::DataType
OMR::Node::setDataType(TR::DataType dt)
   {
   TR_ASSERT(self()->hasDataType(), "attempting to access _dataType field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return (_unionPropertyA._dataType = dt.getDataType());
   }

/**
 * UnionPropertyA functions end
 */

/**
 * Node flag functions
 */

inline bool
OMR::Node::hasNodeExtension()
   {
   return _flags.testAny(nodeHasExtension);
   }

inline void
OMR::Node::setHasNodeExtension(bool v)
   {
   _flags.set(nodeHasExtension,v);
   }

inline bool
OMR::Node::isNegative()
   {
   return self()->isNonPositive() && self()->isNonZero();
   }

inline bool
OMR::Node::isPositive()
   {
   return self()->isNonNegative() && self()->isNonZero();
   }

inline bool
OMR::Node::canCheckReferenceIsNonNull()
   {
   switch (self()->getOpCodeValue())
      {
      case TR::checkcast:
      case TR::checkcastAndNULLCHK:
      case TR::instanceof:
         return true;
      default:
         return false;
      }
   }

inline bool
OMR::Node::isReferenceNonNull()
   {
   TR_ASSERT(self()->canCheckReferenceIsNonNull(), "Unexpected opcode %s for isReferenceNonNull", self()->getOpCode().getName());
   return _flags.testAny(referenceIsNonNull);
   }

inline void
OMR::Node::setReferenceIsNonNull(bool v)
   {
   TR_ASSERT(self()->canCheckReferenceIsNonNull(), "Unexpected opcode %s for isReferenceNonNull", self()->getOpCode().getName());
   _flags.set(referenceIsNonNull, v);
   }

inline bool
OMR::Node::chkIsReferenceNonNull()
   {
   return self()->canCheckReferenceIsNonNull() && self()->isReferenceNonNull();
   }

inline const char *
OMR::Node::printIsReferenceNonNull()
   {
   return self()->chkIsReferenceNonNull() ? "referenceIsNonNull " : "";
   }

/**
 * Node flag functions end
 */

#endif
