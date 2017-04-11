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

#include <stdint.h>          // for int32_t, uint32_t, uintptr_t
#include "il/DataTypes.hpp"  // for DataTypes, DataTypes::Address
#include "infra/Assert.hpp"  // for TR_ASSERT
#include "infra/Flags.hpp"   // for flags32_t

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


   /**
    * Copies a subset of bits from an input flags
    *
    * Used as part of the Dynamic Literal Pool code.
    *
    * \TODO: Better document this function and what exactly it's intended
    *        accomplish, as it's not clear at all.
    */
   void setUpDLPFlags(int32_t flags)
      {
      int32_t value = flags & SetUpDLPFlags;
      _flags.set(value);
      }

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
