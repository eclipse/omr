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

#include "il/symbol/OMRStaticSymbol.hpp"

#include <stddef.h>                    // for NULL
#include "env/TRMemory.hpp"            // for PERSISTENT_NEW_DECLARE
#include "il/symbol/LabelSymbol.hpp"   // for LabelSymbol
#include "il/symbol/StaticSymbol.hpp"  // for StaticSymbolBase, etc

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
