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

#include <stdint.h>
#include "env/FrontEnd.hpp"
#include "env/TRMemory.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "limits.h"


template <typename AllocatorType>
TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(AllocatorType m, int32_t o)
   {
   return new (m) TR::RegisterMappedSymbol(o);
   }

template <typename AllocatorType>
TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(AllocatorType m, TR::DataType d)
   {
   return new (m) TR::RegisterMappedSymbol(d);
   }

template <typename AllocatorType>
TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(AllocatorType m, TR::DataType d, uint32_t s)
   {
   return new (m) TR::RegisterMappedSymbol(d,s);
   }

OMR::RegisterMappedSymbol::RegisterMappedSymbol(int32_t o) :
   TR::Symbol(),
   _mappedOffset(o),
   _GCMapIndex(-1)
   {
   self()->setLiveLocalIndexUninitialized();
   }

OMR::RegisterMappedSymbol::RegisterMappedSymbol(TR::DataType d) :
   TR::Symbol(d),
   _mappedOffset(0),
   _GCMapIndex(-1)
   {
   self()->setLiveLocalIndexUninitialized();
   }

OMR::RegisterMappedSymbol::RegisterMappedSymbol(TR::DataType d, uint32_t s) :
   TR::Symbol(d, s),
   _mappedOffset(0),
   _GCMapIndex(-1)
   {
   self()->setLiveLocalIndexUninitialized();
   }

TR::RegisterMappedSymbol *
OMR::RegisterMappedSymbol::self()
   {
   return static_cast<TR::RegisterMappedSymbol*>(this);
   }

void
OMR::RegisterMappedSymbol::setLiveLocalIndex(uint16_t i, TR_FrontEnd * fe)
   {
   _liveLocalIndex = i;
   if (self()->isLiveLocalIndexUninitialized())
      {
      TR_ASSERT(0, "OMR::RegisterMappedSymbol::_liveLocalIndex == USHRT_MAX");
      TR::comp()->failCompilation<TR::CompilationException>("OMR::RegisterMappedSymbol::_liveLocalIndex == USHRT_MAX");
      }
   }

bool
OMR::RegisterMappedSymbol::isLiveLocalIndexUninitialized()
   {
   return _liveLocalIndex == USHRT_MAX;
   }

void
OMR::RegisterMappedSymbol::setLiveLocalIndexUninitialized()
   {
   _liveLocalIndex = USHRT_MAX;
   }

TR_MethodMetaDataType
OMR::RegisterMappedSymbol::getMethodMetaDataType()
   {
   TR_ASSERT(self()->isMethodMetaData(), "should be method metadata!");
   return _type;
   }

void
OMR::RegisterMappedSymbol::setMethodMetaDataType(TR_MethodMetaDataType type)
   {
   TR_ASSERT(self()->isMethodMetaData(), "should be method metadata!");
   _type = type;
}

template <typename AllocatorType>
TR::RegisterMappedSymbol *
OMR::RegisterMappedSymbol::createMethodMetaDataSymbol(AllocatorType m, const char *name, TR_MethodMetaDataType type)
   {
   TR::RegisterMappedSymbol * sym = new (m) TR::RegisterMappedSymbol();
   sym->_type                     = type;
   sym->_name                     = name;
   sym->_flags.setValue(KindMask, IsMethodMetaData);
   return sym;
   }

/**
 * Explicit instantiation
 */

template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(TR_HeapMemory m, int32_t o);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(TR_HeapMemory m, TR::DataType d);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(TR_HeapMemory m, TR::DataType d, uint32_t s);

template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(TR_StackMemory m, int32_t o);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(TR_StackMemory m, TR::DataType d);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(TR_StackMemory m, TR::DataType d, uint32_t s);

template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(PERSISTENT_NEW_DECLARE m, int32_t o);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(PERSISTENT_NEW_DECLARE m, TR::DataType d);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::create(PERSISTENT_NEW_DECLARE m, TR::DataType d, uint32_t s);

template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::createMethodMetaDataSymbol(TR_HeapMemory m, const char *name, TR_MethodMetaDataType type);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::createMethodMetaDataSymbol(TR_StackMemory m, const char *name, TR_MethodMetaDataType type);
template TR::RegisterMappedSymbol * OMR::RegisterMappedSymbol::createMethodMetaDataSymbol(PERSISTENT_NEW_DECLARE m, const char *name, TR_MethodMetaDataType type);
