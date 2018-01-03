/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "ilgen/IlType.hpp"
#include "ilgen/IlTypeImpl.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/TypeDictionaryImpl.hpp"


OMR::TypeDictionary::TypeDictionary()
   : _impl(TR::TypeDictionaryImpl::allocate(static_cast<TR::TypeDictionary *>(this)))
   {
   // primitive types
   NoType       = impl()->NoType;
   Int8         = impl()->Int8;
   Int16        = impl()->Int16;
   Int32        = impl()->Int32;
   Int64        = impl()->Int64;
   Float        = impl()->Float;
   Double       = impl()->Double;
   Address      = impl()->Address;
   VectorInt8   = impl()->VectorInt8;
   VectorInt16  = impl()->VectorInt16;
   VectorInt32  = impl()->VectorInt32;
   VectorInt64  = impl()->VectorInt64;
   VectorFloat  = impl()->VectorFloat;
   VectorDouble = impl()->VectorDouble;

   // pointer to primitive types
   pNoType       = impl()->pNoType;
   pInt8         = impl()->pInt8;
   pInt16        = impl()->pInt16;
   pInt32        = impl()->pInt32;
   pInt64        = impl()->pInt64;
   pFloat        = impl()->pFloat;
   pDouble       = impl()->pDouble;
   pAddress      = impl()->pAddress;
   pVectorInt8   = impl()->pVectorInt8;
   pVectorInt16  = impl()->pVectorInt16;
   pVectorInt32  = impl()->pVectorInt32;
   pVectorInt64  = impl()->pVectorInt64;
   pVectorFloat  = impl()->pVectorFloat;
   pVectorDouble = impl()->pVectorDouble;

   Word         = impl()->Word;
   pWord        = impl()->pWord;
   }

TR::IlType *
OMR::TypeDictionary::PointerTo(TR::DataType baseType)
   {
   return impl()->PointerTo(baseType);
   }

TR::IlType *
OMR::TypeDictionary::PrimitiveType(TR::DataType primitiveType)
   {
   return impl()->PrimitiveType(primitiveType);
   }

void
OMR::TypeDictionary::CloseStruct(const char *structName)
   {
   impl()->CloseStruct(structName);
   }

void
OMR::TypeDictionary::CloseStruct(const char *structName, size_t finalSize)
   {
   impl()->CloseStruct(structName, finalSize);
   }

void
OMR::TypeDictionary::CloseUnion(const char *unionName)
   {
   impl()->CloseUnion(unionName);
   }

void
OMR::TypeDictionary::DefineField(const char *structName, const char *fieldName, TR::IlType *type, size_t offset)
   {
   impl()->DefineField(structName, fieldName, type->impl(), offset);
   }

void
OMR::TypeDictionary::DefineField(const char *structName, const char *fieldName, TR::IlType *type)
   {
   impl()->DefineField(structName, fieldName, type->impl());
   }

TR::IlType *
OMR::TypeDictionary::DefineStruct(const char *structName)
   {
   return impl()->DefineStruct(structName);
   }

TR::IlType *
OMR::TypeDictionary::DefineUnion(const char *unionName)
   {
   return impl()->DefineUnion(unionName);
   }

TR::IlType *
OMR::TypeDictionary::GetFieldType(const char *structName, const char *fieldName)
   {
   return impl()->GetFieldType(structName, fieldName);
   }

TR::IlType *
OMR::TypeDictionary::LookupStruct(const char *structName)
   {
   return impl()->LookupStruct(structName);
   }

TR::IlType *
OMR::TypeDictionary::LookupUnion(const char *unionName)
   {
   return impl()->LookupUnion(unionName);
   }

size_t
OMR::TypeDictionary::OffsetOf(const char *structName, const char *fieldName)
   {
   return impl()->OffsetOf(structName, fieldName);
   }

TR::IlType *
OMR::TypeDictionary::PointerTo(const char *structName)
   {
   return impl()->PointerTo(structName);
   }

TR::IlType *
OMR::TypeDictionary::PointerTo(TR::IlType *baseType)
   {
   return impl()->PointerTo(baseType->impl());
   }

void
OMR::TypeDictionary::UnionField(const char *unionName, const char *fieldName, TR::IlType *type)
   {
   impl()->UnionField(unionName, fieldName, type->impl());
   }

TR::IlType *
OMR::TypeDictionary::UnionFieldType(const char *unionName, const char *fieldName)
   {
   return impl()->UnionFieldType(unionName, fieldName);
   }
