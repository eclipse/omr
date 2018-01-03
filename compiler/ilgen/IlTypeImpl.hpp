/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_ILTYPE_IMPL_INCL
#define OMR_ILTYPE_IMPL_INCL


#ifndef TR_ILTYPE_IMPL_DEFINED
#define TR_ILTYPE_IMPL_DEFINED
#define PUT_OMR_ILTYPE_IMPL_INTO_TR
#endif

#include "il/DataTypes.hpp"
#include "ilgen/IlType.hpp"

namespace OMR
{

class IlTypeImpl : public TR::IlType
   {
public:
   IlTypeImpl(const char *name, TR::TypeDictionary *dict) :
      TR::IlType(name, dict)
      { }
   //IlTypeImpl() :
   //   TR::IlType()
   //   { }
   virtual ~IlTypeImpl()
      { }

   virtual TR::IlType *baseType()
      { return NULL; }
   virtual char *getSignatureName();       // TODO: change to signatureName()
   virtual size_t getSize();               // TODO: change to size()
   virtual bool isArray()
      { return false; }
   virtual bool isPointer()
      { return false; }
   virtual bool isStruct()
      {return false; }
   virtual bool isUnion()
      { return false; }

   static const char *_signatureNameForType[];

   virtual TR::DataType getRealPrimitiveType()
      { return TR::NoType; }

   TR::TypeDictionary *dict()
      { return _dict; }
   };

} // namespace OMR


#if defined(PUT_OMR_ILTYPE_IMPL_INTO_TR)
namespace TR
{
class IlTypeImpl : public OMR::IlTypeImpl
   {
   /* there should be no public constructors */

   protected:
      IlTypeImpl(const char *name, TR::TypeDictionary *dict)
         : OMR::IlTypeImpl(name, dict)
         { }
      //IlTypeImpl()
      //   : OMR::IlTypeImpl()
      //   { }
   };
} // namespace TR
#endif // defined(PUT_OMR_ILTYPE_IMPL_INTO_TR)


#endif // !defined(OMR_ILTYPE_IMPL_INCL)
