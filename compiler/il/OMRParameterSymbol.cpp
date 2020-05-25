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
#include "env/KnownObjectTable.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/Symbol.hpp"
#include "infra/Flags.hpp"

TR::ParameterSymbol *
OMR::ParameterSymbol::self()
   {
   return static_cast<TR::ParameterSymbol*>(this);
   }

OMR::ParameterSymbol::ParameterSymbol(TR::DataType d, int32_t slot) :
   TR::RegisterMappedSymbol(d),
   _registerIndex(-1),
   _allocatedHigh(-1),
   _allocatedLow(-1),
   _fixedType(0),
   _isPreexistent(false),
   _knownObjectIndex(TR::KnownObjectTable::UNKNOWN)
   {
   _flags.setValue(KindMask, IsParameter);
   _addressSize = TR::ParameterSymbol::convertTypeToSize(TR::Address);
   self()->setOffset(slot * TR::ParameterSymbol::convertTypeToSize(TR::Address));
   }

OMR::ParameterSymbol::ParameterSymbol(TR::DataType d, int32_t slot, size_t size) :
   TR::RegisterMappedSymbol(d, (uint32_t)size), // cast argument size explicitly \TODO: Document why?
   _registerIndex(-1),
   _allocatedHigh(-1),
   _allocatedLow(-1),
   _fixedType(0),
   _isPreexistent(false),
   _knownObjectIndex(TR::KnownObjectTable::UNKNOWN)
   {
   _flags.setValue(KindMask, IsParameter);
   _addressSize = TR::ParameterSymbol::convertTypeToSize(TR::Address);
   self()->setOffset(slot * TR::ParameterSymbol::convertTypeToSize(TR::Address));
   }

void
OMR::ParameterSymbol::setParameterOffset(int32_t o)
   {
   self()->setOffset(o);
   }

int32_t
OMR::ParameterSymbol::getSlot()
   {
   return self()->getParameterOffset() / (uint32_t)_addressSize; // cast _addressSize explicity
   }

template <typename AllocatorType>
TR::ParameterSymbol * OMR::ParameterSymbol::create(AllocatorType m, TR::DataType d, int32_t slot)
   {
   return new (m) TR::ParameterSymbol(d, slot);
   }

template <typename AllocatorType>
TR::ParameterSymbol * OMR::ParameterSymbol::create(AllocatorType m, TR::DataType d, int32_t slot, size_t size)
   {
   return new (m) TR::ParameterSymbol(d, slot, size);
   }


template TR::ParameterSymbol * OMR::ParameterSymbol::create(TR_StackMemory, TR::DataType, int32_t);
template TR::ParameterSymbol * OMR::ParameterSymbol::create(TR_StackMemory, TR::DataType, int32_t, size_t);
template TR::ParameterSymbol * OMR::ParameterSymbol::create(TR_HeapMemory, TR::DataType, int32_t);
template TR::ParameterSymbol * OMR::ParameterSymbol::create(TR_HeapMemory, TR::DataType, int32_t, size_t);
template TR::ParameterSymbol * OMR::ParameterSymbol::create(PERSISTENT_NEW_DECLARE, TR::DataType, int32_t);
template TR::ParameterSymbol * OMR::ParameterSymbol::create(PERSISTENT_NEW_DECLARE, TR::DataType, int32_t, size_t);
