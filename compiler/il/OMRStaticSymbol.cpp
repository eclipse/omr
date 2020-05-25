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
#include "env/TRMemory.hpp"
#include "il/LabelSymbol.hpp"
#include "il/StaticSymbol.hpp"

TR::StaticSymbol*
OMR::StaticSymbol::self()
   {
   return static_cast<TR::StaticSymbol*>(this);
   }

template <typename AllocatorType>
TR::StaticSymbol * OMR::StaticSymbol::create(AllocatorType m, TR::DataType d)
   {
   return new (m) TR::StaticSymbol(d);
   }

template <typename AllocatorType>
TR::StaticSymbol * OMR::StaticSymbol::createWithAddress(AllocatorType m, TR::DataType d, void * address)
   {
   //Explict cast to disambiguate constructor!
   return new (m) TR::StaticSymbol(d, (void*)address);
   }

template <typename AllocatorType>
TR::StaticSymbol * OMR::StaticSymbol::createWithSize(AllocatorType m, TR::DataType d, uint32_t size)
   {
   //Explict cast to disambiguate constructor!
   return new (m) TR::StaticSymbol(d, (uint32_t)size);
   }


template <typename AllocatorType>
TR::StaticSymbol * OMR::StaticSymbol::createNamed(AllocatorType m, TR::DataType d, const char * name)
   {
   TR::StaticSymbol * sym = new (m) TR::StaticSymbol(d);
   sym->makeNamed(name);
   return sym;
   }

template <typename AllocatorType>
TR::StaticSymbol * OMR::StaticSymbol::createNamed(AllocatorType m, TR::DataType d, void * addr, const char * name)
   {
   TR::StaticSymbol * sym = new (m) TR::StaticSymbol(d,addr);
   sym->makeNamed(name);
   return sym;
   }


const char *
OMR::StaticSymbol::getName()
   {
   TR_ASSERT(self()->isNamed(),"Must have called makeNamed() to get a valid name");
   return _name;
   }


//Explicit Instantiations
template TR::StaticSymbol * OMR::StaticSymbol::createNamed(TR_HeapMemory m, TR::DataType d, const char * name) ;
template TR::StaticSymbol * OMR::StaticSymbol::createNamed(TR_HeapMemory m, TR::DataType d, void * addr, const char * name) ;

template TR::StaticSymbol * OMR::StaticSymbol::createNamed(TR_StackMemory m, TR::DataType d, const char * name) ;
template TR::StaticSymbol * OMR::StaticSymbol::createNamed(TR_StackMemory m, TR::DataType d, void * addr, const char * name) ;

template TR::StaticSymbol * OMR::StaticSymbol::createNamed(PERSISTENT_NEW_DECLARE m, TR::DataType d, const char * name) ;
template TR::StaticSymbol * OMR::StaticSymbol::createNamed(PERSISTENT_NEW_DECLARE m, TR::DataType d, void * addr, const char * name) ;

template TR::StaticSymbol * OMR::StaticSymbol::create(TR_HeapMemory, TR::DataType);
template TR::StaticSymbol * OMR::StaticSymbol::create(TR_StackMemory, TR::DataType);
template TR::StaticSymbol * OMR::StaticSymbol::create(TR_PersistentMemory*, TR::DataType);
template TR::StaticSymbol * OMR::StaticSymbol::create(PERSISTENT_NEW_DECLARE, TR::DataType);

template TR::StaticSymbol * OMR::StaticSymbol::createWithAddress(TR_HeapMemory, TR::DataType, void *);
template TR::StaticSymbol * OMR::StaticSymbol::createWithAddress(TR_StackMemory, TR::DataType, void *);
template TR::StaticSymbol * OMR::StaticSymbol::createWithAddress(PERSISTENT_NEW_DECLARE, TR::DataType, void*);

template TR::StaticSymbol * OMR::StaticSymbol::createWithSize(TR_HeapMemory, TR::DataType, uint32_t);
template TR::StaticSymbol * OMR::StaticSymbol::createWithSize(TR_StackMemory, TR::DataType, uint32_t);
template TR::StaticSymbol * OMR::StaticSymbol::createWithSize(TR_PersistentMemory*, TR::DataType, uint32_t);
template TR::StaticSymbol * OMR::StaticSymbol::createWithSize(PERSISTENT_NEW_DECLARE, TR::DataType, uint32_t);
