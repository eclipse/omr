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
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "ras/Debug.hpp"

template <typename AllocatorType>
TR::Symbol * OMR::Symbol::create(AllocatorType m)
   {
   return new (m) TR::Symbol();
   }

template <typename AllocatorType>
TR::Symbol * OMR::Symbol::create(AllocatorType m, TR::DataType d)
   {
   return new (m) TR::Symbol(d);
   }

template <typename AllocatorType>
TR::Symbol * OMR::Symbol::create(AllocatorType m, TR::DataType d, uint32_t s)
   {
   return new (m) TR::Symbol(d,s);
   }

OMR::Symbol::Symbol(TR::DataType d) :
   _size(0),
   _name(0),
   _flags(0),
   _flags2(0),
   _localIndex(0)
   {
   self()->setDataType(d);
   }

OMR::Symbol::Symbol(TR::DataType d, uint32_t size) :
   _name(0),
   _flags(0),
   _flags2(0),
   _localIndex(0)
   {
   self()->setDataType(d);
   _size = size;
   }

bool
OMR::Symbol::isReferenced()
   {
   return self()->isVariableSizeSymbol() && self()->castToVariableSizeSymbol()->isReferenced();
   }

bool
OMR::Symbol::dontEliminateStores(TR::Compilation *comp, bool isForLocalDeadStore)
   {
   return (self()->isAuto() && _flags.testAny(PinningArrayPointer)) ||
          (self()->isParm() && _flags.testAny(ReinstatedReceiver)) ||
          _flags.testAny(HoldsMonitoredObject) ||
          (comp->getSymRefTab()->findThisRangeExtensionSymRef() && (self() == comp->getSymRefTab()->findThisRangeExtensionSymRef()->getSymbol()));
   }

uint32_t
OMR::Symbol::getNumberOfSlots()
   {
   uint32_t numSlots = self()->getRoundedSize()/self()->convertTypeToSize(TR::Address);

   // We should always give at least 1 slot.
   //  This is specifically for the case of an int type on 64bit pltfrms
   return (numSlots? numSlots : 1);
   }

TR::DataType
OMR::Symbol::convertSigCharToType(char sigChar)
   {
   switch (sigChar)
      {
      case 'Z': return TR::Int8;
      case 'B': return TR::Int8;
      case 'C': return TR::Int16;
      case 'S': return TR::Int16;
      case 'I': return TR::Int32;
      case 'J': return TR::Int64;
      case 'F': return TR::Float;
      case 'D': return TR::Double;
      case 'L':
      case '[': return TR::Address;
      }
   TR_ASSERT(0, "unknown signature character");
   return TR::Int32;
   }

/**
 * Sets the data type of a symbol, and the size, if the size can be inferred
 * from the data type.
 */
void
OMR::Symbol::setDataType(TR::DataType dt)
   {
   uint32_t inferredSize = TR::DataType::getSize(dt);
   if (inferredSize)
      {
      _size = inferredSize;
      }
   _flags.setValue(DataTypeMask, dt);
   }

uint32_t
OMR::Symbol::getRoundedSize()
   {
   int32_t roundedSize = (int32_t)((self()->getSize()+3)&(~3)); // cast explicitly
   return roundedSize ? roundedSize : 4;
   }

int32_t
OMR::Symbol::getOffset()
   {
   TR::RegisterMappedSymbol * r = self()->getRegisterMappedSymbol();
   return r ? r->getOffset() : 0;
   }

/*
 * Static factory methods
 */
template <typename AllocatorType>
TR::Symbol * OMR::Symbol::createShadow(AllocatorType m)
   {
   TR::Symbol * sym = new (m) TR::Symbol();
   sym->_flags.setValue(KindMask, IsShadow);
   return sym;
   }

template <typename AllocatorType>
TR::Symbol * OMR::Symbol::createShadow(AllocatorType m, TR::DataType d)
   {
   TR::Symbol * sym = new (m) TR::Symbol(d);
   sym->_flags.setValue(KindMask, IsShadow);
   return sym;
   }

template <typename AllocatorType>
TR::Symbol * OMR::Symbol::createShadow(AllocatorType m, TR::DataType d, uint32_t s)
   {
   TR::Symbol * sym = new (m) TR::Symbol(d,s);
   sym->_flags.setValue(KindMask, IsShadow);
   return sym;
   }

template <typename AllocatorType>
TR::Symbol * OMR::Symbol::createNamedShadow(AllocatorType m, TR::DataType d, uint32_t s, char *name)
   {
   auto * sym = createShadow(m,d,s);
   sym->_name = name;
   sym->_flags2.set(NamedShadow);
   return sym;
   }

/*
 * Explicit instantiation of factories for each TR_Memory type.
 */

template TR::Symbol * OMR::Symbol::create(TR_StackMemory);
template TR::Symbol * OMR::Symbol::create(TR_HeapMemory);
template TR::Symbol * OMR::Symbol::create(PERSISTENT_NEW_DECLARE);

template TR::Symbol * OMR::Symbol::create(TR_StackMemory, TR::DataType);
template TR::Symbol * OMR::Symbol::create(TR_HeapMemory, TR::DataType);
template TR::Symbol * OMR::Symbol::create(PERSISTENT_NEW_DECLARE, TR::DataType);

template TR::Symbol * OMR::Symbol::create(TR_StackMemory, TR::DataType, uint32_t);
template TR::Symbol * OMR::Symbol::create(TR_HeapMemory, TR::DataType, uint32_t);
template TR::Symbol * OMR::Symbol::create(PERSISTENT_NEW_DECLARE, TR::DataType, uint32_t);

template TR::Symbol * OMR::Symbol::createShadow(TR_StackMemory);
template TR::Symbol * OMR::Symbol::createShadow(TR_HeapMemory);
template TR::Symbol * OMR::Symbol::createShadow(PERSISTENT_NEW_DECLARE);

template TR::Symbol * OMR::Symbol::createShadow(TR_StackMemory,         TR::DataType);
template TR::Symbol * OMR::Symbol::createShadow(TR_HeapMemory,          TR::DataType);
template TR::Symbol * OMR::Symbol::createShadow(PERSISTENT_NEW_DECLARE, TR::DataType);

template TR::Symbol * OMR::Symbol::createShadow(TR_StackMemory,         TR::DataType, uint32_t);
template TR::Symbol * OMR::Symbol::createShadow(TR_HeapMemory,          TR::DataType, uint32_t);
template TR::Symbol * OMR::Symbol::createShadow(PERSISTENT_NEW_DECLARE, TR::DataType, uint32_t);

template TR::Symbol * OMR::Symbol::createNamedShadow(TR_StackMemory,         TR::DataType, uint32_t, char *);
template TR::Symbol * OMR::Symbol::createNamedShadow(TR_HeapMemory,          TR::DataType, uint32_t, char *);
template TR::Symbol * OMR::Symbol::createNamedShadow(PERSISTENT_NEW_DECLARE, TR::DataType, uint32_t, char *);
