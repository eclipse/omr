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

#ifndef OMR_STATICSYMBOL_INCL
#define OMR_STATICSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_STATICSYMBOL_CONNECTOR
#define OMR_STATICSYMBOL_CONNECTOR
namespace OMR { class StaticSymbol; }
namespace OMR { typedef OMR::StaticSymbol StaticSymbolConnector; }
#endif

#include "il/Symbol.hpp"

#include <stdint.h>
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

namespace TR { class LabelSymbol; }
namespace TR { class StaticSymbol; }

namespace OMR
{

/**
 * A symbol with an adress
 */
class OMR_EXTENSIBLE StaticSymbol : public TR::Symbol
   {
protected:
   TR::StaticSymbol* self();

public:

   template <typename AllocatorType>
   static TR::StaticSymbol * create(AllocatorType t, TR::DataType d);

   template <typename AllocatorType>
   static TR::StaticSymbol * createWithAddress(AllocatorType t, TR::DataType d, void * address);

   template <typename AllocatorType>
   static TR::StaticSymbol * createWithSize(AllocatorType t, TR::DataType d, uint32_t s);

protected:

   StaticSymbol(TR::DataType d) :
      TR::Symbol(d),
      _staticAddress(0),
      _assignedTOCIndex(0)
      {
      _flags.setValue(KindMask, IsStatic);
      }

   StaticSymbol(TR::DataType d, void * address) :
      TR::Symbol(d),
      _staticAddress(address),
      _assignedTOCIndex(0)
      {
      _flags.setValue(KindMask, IsStatic);
      }

   StaticSymbol(TR::DataType d, uint32_t s) :
      TR::Symbol(d, s),
      _staticAddress(0),
      _assignedTOCIndex(0)
      {
      _flags.setValue(KindMask, IsStatic);
      }

public:

   void * getStaticAddress()                  { return _staticAddress; }
   void   setStaticAddress(void * a)          { _staticAddress = a; }

   uint32_t getTOCIndex()                     { return _assignedTOCIndex; }
   void     setTOCIndex(uint32_t idx)         { _assignedTOCIndex = idx; }

private:

   void * _staticAddress;

   uint32_t _assignedTOCIndex;

   /* ------ TR_NamedStaticSymbol --------------- */
public:

   template <typename AllocatorType>
   static TR::StaticSymbol * createNamed(AllocatorType m, TR::DataType d, const char * name);

   template <typename AllocatorType>
   static TR::StaticSymbol * createNamed(AllocatorType m, TR::DataType d, void * addr, const char * name);

   const char *getName();

private:

   void makeNamed(const char * name) { _name = name;  _flags.set(IsNamed); }

   };

}

#endif
