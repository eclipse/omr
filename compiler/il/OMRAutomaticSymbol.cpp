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

#include <stddef.h>
#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "env/TRMemory.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"

class TR_FrontEnd;
namespace TR { class Node; }
namespace TR { class SymbolReference; }

OMR::AutomaticSymbol::AutomaticSymbol() :
   TR::RegisterMappedSymbol()
   {
   self()->init();
   }

OMR::AutomaticSymbol::AutomaticSymbol(TR::DataType d) :
   TR::RegisterMappedSymbol(d)
   {
   self()->init();
   }

OMR::AutomaticSymbol::AutomaticSymbol(TR::DataType d, uint32_t s) :
   TR::RegisterMappedSymbol(d, s)
   {
   self()->init();
   }

OMR::AutomaticSymbol::AutomaticSymbol(TR::DataType d, uint32_t s, const char *name) :
   TR::RegisterMappedSymbol(d, s)
   {
   self()->init(); _name = name;
   }

TR::AutomaticSymbol *
OMR::AutomaticSymbol::self()
   {
   return static_cast<TR::AutomaticSymbol*>(this);
   }

rcount_t
OMR::AutomaticSymbol::setReferenceCount(rcount_t i)
   {
   if (self()->isVariableSizeSymbol() && i > 0)
      self()->castToVariableSizeSymbol()->setIsReferenced();
   return (_referenceCount = i);
   }

rcount_t
OMR::AutomaticSymbol::incReferenceCount()
   {
   if (self()->isVariableSizeSymbol())
      self()->castToVariableSizeSymbol()->setIsReferenced();
   return ++_referenceCount;
   }

void
OMR::AutomaticSymbol::init()
   {
   _referenceCount = 0;
   _flags.setValue(KindMask, IsAutomatic);
   _name = NULL;
   }

TR::ILOpCodes
OMR::AutomaticSymbol::getOpCodeKind()
  {
  TR_ASSERT(self()->isLocalObject(), "Should be local object");
  return _kind;
  }

TR::SymbolReference *
OMR::AutomaticSymbol::getClassSymbolReference()
   {
   TR_ASSERT(self()->isLocalObject(), "Should be tagged as local object");
   return _kind != TR::newarray ? _classSymRef : 0;
   }

TR::SymbolReference *
OMR::AutomaticSymbol::setClassSymbolReference(TR::SymbolReference *s)
   {
   TR_ASSERT(self()->isLocalObject(), "Should be tagged as local object");
   return (_classSymRef = s);
   }

int32_t
OMR::AutomaticSymbol::getArrayType()
   {
   TR_ASSERT(self()->isLocalObject(), "Should be tagged as local object");
   return _kind == TR::newarray ? _arrayType : 0;
   }

TR::AutomaticSymbol *
OMR::AutomaticSymbol::getPinningArrayPointer()
   {
   TR_ASSERT(self()->isInternalPointer(), "Should be internal pointer");
   return _pinningArrayPointer;
   }

TR::AutomaticSymbol *
OMR::AutomaticSymbol::setPinningArrayPointer(TR::AutomaticSymbol *s)
   {
   TR_ASSERT(self()->isInternalPointer(), "Should be internal pointer");
   return (_pinningArrayPointer = s);
   }

uint32_t
OMR::AutomaticSymbol::getActiveSize()
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   return _activeSize;
   }

uint32_t
OMR::AutomaticSymbol::setActiveSize(uint32_t s)
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   return (_activeSize = s);
   }

bool
OMR::AutomaticSymbol::isReferenced()
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   return _variableSizeSymbolFlags.testAny(IsReferenced);
   }

void
OMR::AutomaticSymbol::setIsReferenced(bool b)
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   _variableSizeSymbolFlags.set(IsReferenced, b);
   }

bool
OMR::AutomaticSymbol::isAddressTaken()
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   return _variableSizeSymbolFlags.testAny(IsAddressTaken);
   }

void
OMR::AutomaticSymbol::setIsAddressTaken(bool b)
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   _variableSizeSymbolFlags.set(IsAddressTaken, b);
   }

bool
OMR::AutomaticSymbol::isSingleUse()
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   return _variableSizeSymbolFlags.testAny(IsSingleUse);
   }

void
OMR::AutomaticSymbol::setIsSingleUse(bool b)
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   _variableSizeSymbolFlags.set(IsSingleUse, b);
   }

TR::Node *
OMR::AutomaticSymbol::getNodeToFreeAfter()
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   return _nodeToFreeAfter;
   }

TR::Node *
OMR::AutomaticSymbol::setNodeToFreeAfter(TR::Node *n)
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "Should be variable sized symbol");
   return _nodeToFreeAfter = n;
   }


template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::create(AllocatorType t)
   {
   return new (t) TR::AutomaticSymbol();
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::create(AllocatorType t,  TR::DataType d)
   {
   return new (t) TR::AutomaticSymbol(d);
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::create(AllocatorType t,  TR::DataType d, uint32_t s)
   {
   return new (t) TR::AutomaticSymbol(d,s);
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::create(AllocatorType t,  TR::DataType d, uint32_t s, const char * n)
   {
   return new (t) TR::AutomaticSymbol(d,s,n);
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(AllocatorType m, int32_t arrayType, TR::DataType d, uint32_t s, TR_FrontEnd * fe)
   {
   TR::AutomaticSymbol * sym   = new (m) TR::AutomaticSymbol(d, s);

   sym->_kind                 = TR::newarray;
   sym->_referenceSlots       = NULL;
   sym->_arrayType            = arrayType;
   sym->setLocalObject();
   return sym;
   }



template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(AllocatorType m, TR::ILOpCodes kind, TR::SymbolReference * classSymRef, TR::DataType d, uint32_t s, TR_FrontEnd * fe)
   {
   TR_ASSERT(kind == TR::newarray     ||
             kind == TR::New          ||
             kind == TR::anewarray,
             "Invalid kind passed to local object factory");
   TR::AutomaticSymbol * sym  = new (m) TR::AutomaticSymbol(d, s);
   sym->_kind                 = kind;
   sym->_referenceSlots       = NULL;
   sym->_classSymRef          = classSymRef;
   sym->setLocalObject();
   return sym;
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(AllocatorType m, TR::AutomaticSymbol *pinningArrayPointer)
   {
   TR::AutomaticSymbol * sym = new (m) TR::AutomaticSymbol();
   sym->setInternalPointer();
   sym->setPinningArrayPointer(pinningArrayPointer);
   return sym;
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(AllocatorType m, TR::DataType d, TR::AutomaticSymbol *pinningArrayPointer)
   {
   TR::AutomaticSymbol * sym = new (m) TR::AutomaticSymbol(d);
   sym->setInternalPointer();
   sym->setPinningArrayPointer(pinningArrayPointer);
   return sym;
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(AllocatorType m, TR::DataType d, uint32_t s, TR_FrontEnd * fe)
   {
   TR::AutomaticSymbol * sym = new (m) TR::AutomaticSymbol(d,s);
   sym->setInternalPointer();
   sym->setPinningArrayPointer(NULL);
   return sym;
   }

template <typename AllocatorType>
TR::AutomaticSymbol * OMR::AutomaticSymbol::createVariableSized(AllocatorType m, uint32_t s)
   {
   TR::AutomaticSymbol * sym = new (m) TR::AutomaticSymbol(TR::NoType, s);
   sym->_activeSize = s;
   sym->_nodeToFreeAfter = NULL;
   sym->_variableSizeSymbolFlags = 0;
   sym->setVariableSizeSymbol();
   return sym;
   }



// Explicit instantiations.


template TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(TR_HeapMemory m, int32_t arrayType, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(TR_HeapMemory m, TR::ILOpCodes kind, TR::SymbolReference * classSymRef, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(TR_HeapMemory m, TR::AutomaticSymbol *pinningArrayPointer);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(TR_HeapMemory m, TR::DataType d, TR::AutomaticSymbol *pinningArrayPointer);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(TR_HeapMemory m, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createVariableSized(TR_HeapMemory m, uint32_t s);

template TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(TR_StackMemory m, int32_t arrayType, TR::DataType   d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(TR_StackMemory m, TR::ILOpCodes kind, TR::SymbolReference * classSymRef, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(TR_StackMemory m, TR::AutomaticSymbol *pinningArrayPointer);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(TR_StackMemory m, TR::DataType d, TR::AutomaticSymbol *pinningArrayPointer);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(TR_StackMemory m, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createVariableSized(TR_StackMemory m, uint32_t s);

template TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(PERSISTENT_NEW_DECLARE m, int32_t arrayType, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createLocalObject(PERSISTENT_NEW_DECLARE m, TR::ILOpCodes kind, TR::SymbolReference * classSymRef, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(PERSISTENT_NEW_DECLARE m, TR::AutomaticSymbol *pinningArrayPointer);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(PERSISTENT_NEW_DECLARE m, TR::DataType d, TR::AutomaticSymbol *pinningArrayPointer);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createInternalPointer(PERSISTENT_NEW_DECLARE m, TR::DataType d, uint32_t s, TR_FrontEnd * fe);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::createVariableSized(PERSISTENT_NEW_DECLARE m, uint32_t s);


template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_StackMemory);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_StackMemory,  TR::DataType);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_StackMemory,  TR::DataType, uint32_t);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_StackMemory,  TR::DataType, uint32_t, const char *);

template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_HeapMemory);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_HeapMemory,  TR::DataType);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_HeapMemory,  TR::DataType, uint32_t);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(TR_HeapMemory,  TR::DataType, uint32_t, const char *);

template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(PERSISTENT_NEW_DECLARE);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(PERSISTENT_NEW_DECLARE,  TR::DataType);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(PERSISTENT_NEW_DECLARE,  TR::DataType, uint32_t);
template TR::AutomaticSymbol * OMR::AutomaticSymbol::create(PERSISTENT_NEW_DECLARE,  TR::DataType, uint32_t, const char *);
