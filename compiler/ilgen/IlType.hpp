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

#ifndef OMR_ILTYPE_INCL
#define OMR_ILTYPE_INCL


#ifndef TR_ILTYPE_DEFINED
#define TR_ILTYPE_DEFINED
#define PUT_OMR_ILTYPE_INTO_TR
#endif

#include <stddef.h>

namespace JitBuilder { class ResolvedMethod; }
namespace TR { class IlBuilderImpl; }
namespace TR { class IlType; }
namespace TR { class IlTypeImpl; }
namespace TR { class TypeDictionary; }

namespace OMR
{

class IlType
   {
   friend class IlBuilder;
   friend class IlBuilderImpl;
   friend class MethodBuilder;
   friend class ThunkBuilder;
   friend class TypeDictionary;
   friend class TypeDictionaryImpl;
   friend class VirtualMachineRegister;
   friend class ::JitBuilder::ResolvedMethod;

public:
   /* there should be no public constructors */

   virtual TR::IlType *baseType()
      { return 0; }

   const char *getName()     // TODO: rename to GetName()
      { return _name; }

   virtual TR::IlType *getPrimitiveType(); // TODO: change to PrimitiveType()

   virtual size_t getSize()  // TODO: change to Size()
      { return -1; }

   virtual bool isArray()
      { return false; }

   virtual bool isPointer()
      { return false; }

   virtual bool isStruct()
      {return false; }

   virtual bool isUnion()
      { return false; }

protected:
   IlType(const char *name, TR::TypeDictionary *dict) :
      _dict(dict),
      _name(name)
      { }
   IlType() :
      _name(0)
      { }
   virtual ~IlType()
      { }

   TR::IlTypeImpl *impl();

   TR::TypeDictionary * _dict;
   const char         * _name;
   };

} // namespace OMR


#if defined(PUT_OMR_ILTYPE_INTO_TR)
namespace TR
{
class IlType : public OMR::IlType
   {
   /* there should be no public constructors */

   protected:
      IlType(const char *name, TR::TypeDictionary *dict)
         : OMR::IlType(name, dict)
         { }
      IlType()
         : OMR::IlType()
         { }
   };
} // namespace TR
#endif // defined(PUT_OMR_ILTYPE_INTO_TR)


#endif // !defined(OMR_ILTYPE_INCL)
