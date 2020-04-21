/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#ifndef OMR_ILTYPE_INCL
#define OMR_ILTYPE_INCL

#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"

namespace TR { class IlType; }
namespace TR { class TypeDictionary; }

extern "C" {
typedef void * (*ClientAllocator)(void * impl);
typedef void * (*ImplGetter)(void *client);
}

namespace OMR
{

class IlType
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   IlType(const char *name) :
      _client(0),
      _name(name)
      { }
   IlType() :
      _client(0),
      _name(0)
      { }
   virtual ~IlType()
      { }

   const char *getName() { return _name; }
   virtual char *getSignatureName();

   virtual TR::IlType *primitiveType(TR::TypeDictionary * d);

   virtual TR::DataType getPrimitiveType() { return TR::NoType; }

   virtual bool isArray() { return false; }
   virtual bool isPointer() { return false; }
   virtual TR::IlType *baseType() { return NULL; }

   virtual bool isStruct() {return false; }
   virtual bool isUnion() { return false; }

   virtual size_t getSize();

   /**
    * @brief associates this object with a particular client object
    */
   void setClient(void *client)
      {
      _client = client;
      }

   /**
    * @brief returns the client object associated with this object
    */
   void *client();

   /**
    * @brief Set the Client Allocator function
    *
    * @param allocator a function pointer to the client object allocator
    */
   static void setClientAllocator(ClientAllocator allocator)
      {
      _clientAllocator = allocator;
      }

   /**
    * @brief Set the Get Impl function
    *
    * @param getter function pointer to the impl getter
    */
   static void setGetImpl(ImplGetter getter)
      {
      _getImpl = getter;
      }

protected:
   /**
    * @brief pointer to a client object that corresponds to this object
    */
   void                 * _client;

   /**
    * @brief pointer to the function used to allocate an instance of a
    * client object
    */
   static ClientAllocator _clientAllocator;

   /**
    * @brief pointer to impl getter function
    */
   static ImplGetter _getImpl;

   const char           * _name;

   static const char    * signatureNameForType[TR::NumOMRTypes];
   static const uint8_t   primitiveTypeAlignment[TR::NumOMRTypes];
   };

} // namespace OMR

#endif // !defined(OMR_ILTYPE_INCL)
