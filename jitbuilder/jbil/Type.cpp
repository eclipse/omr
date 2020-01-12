/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "Type.hpp"
#include "TextWriter.hpp"

namespace OMR
{

namespace JitBuilder
{

int64_t OMR::JitBuilder::Type::globalIndex = 0;


void
NoTypeType::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name();
   }

void
Int8Type::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name() << " " << *reinterpret_cast<int8_t *>(p);
   }

void
Int16Type::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name() << " " << *reinterpret_cast<int16_t *>(p);
   }

void
Int32Type::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name() << " " << *reinterpret_cast<int32_t *>(p);
   }

void
Int64Type::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name() << " " << *reinterpret_cast<int64_t *>(p);
   }

void
FloatType::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name() << " " << *reinterpret_cast<float *>(p);
   }

void
DoubleType::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name() << " " << *reinterpret_cast<double *>(p);
   }

void
AddressType::printer(TextWriter *w, const Type *t, void *p)
   {
   *w << t->name() << " " << *(reinterpret_cast<void **>(p));
   }

void
Type::printType(TextWriter *w)
   {
   }

void
PointerType::printType(TextWriter *w)
   {
   }

void
PointerType::printValue(TextWriter *w, void *p) const
   {
   *w << name() << " " << *(reinterpret_cast<void **>(p));
   }

void
FieldType::printType(TextWriter *w)
   {
   }

void
FieldType::printValue(TextWriter *w, void *p) const
   {
   }

FieldType *
StructType::addField(LiteralValue *name, Type *type, size_t offset)
   {
   assert(name->kind() == T_string || name->kind() == T_typename);
   FieldType *field = LookupField(name->getString());
   if (field)
      return field;

   field = FieldType::create(owningDictionary(), this, name, type, offset);
   _fieldsByName.insert({name->getString(), field});
   _fieldsByOffset.insert({offset, field});

   if (_size < offset + type->size())
      _size = offset + type->size();

   return field;
   }

FieldIterator
StructType::RemoveField(FieldIterator &it)
   {
   FieldType *fieldType = it->second;

   // iterator is for _fieldsByName, so just erase
   auto it1 = _fieldsByName.erase(it);

   // have to look for it in _fieldsByOffset and erase from it
   auto it2 = _fieldsByOffset.find(fieldType->offset());
   while (it2 != _fieldsByOffset.end())
      {
      // can have multiple fields at an offset so make sure we remove the right one
      FieldType *fType = it2->second;
      if (fType == fieldType)
         {
         _fieldsByOffset.erase(it2);
         break;
         }
      else
         it2++;
      }

   return it1;
   }

void
StructType::printType(TextWriter *w)
   {
   }

void
StructType::printValue(TextWriter *w, void *p) const
   {
   }

void
UnionType::printType(TextWriter *w)
   {
   // TODO
   }

void
UnionType::printValue(TextWriter *w, void *p) const
   {
   // TODO
   }

FunctionType *
FunctionType::create(TypeDictionary *dict,
                     std::string name,
                     Type *returnType,
                     int32_t numParms,
                     Type ** parmTypes)
   {
   return new FunctionType(dict, name, returnType, numParms, parmTypes);
   }

void
FunctionType::printType(TextWriter *w)
   {
   // TODO
   }

void
FunctionType::printValue(TextWriter *w, void *p) const
   {
   // TODO
   }

//
// User type instances
// BEGIN here {

// } END here
// User type instances
// 


} // namespace JitBuilder
} // namespace OMR
