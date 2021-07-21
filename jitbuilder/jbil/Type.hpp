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

#ifndef TYPE_INCL
#define TYPE_INCL

#include <stdint.h>
#include <stdarg.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>

#include "LiteralValue.hpp"
#include "Mapper.hpp"

namespace OMR
{

namespace JitBuilder
{

class Builder;
class FieldType;
class StructType;
class TextWriter;
class Type;
class TypeDictionary;

class Int8Type;
class Int16Type;
class Int32Type;
class Int64Type;
class FloatType;
class DoubleType;
class AddressType;

//
// User type forward declarations
// BEGIN here {

// } END here
// User type forward declarations
//

typedef int64_t TypeID;

class Type
   {
   friend class TypeDictionary;
protected:
   typedef void (ValuePrinter)(TextWriter *w, const Type *t, void *p);

public:
   std::string name() const                 { return _name; }
   TypeDictionary *owningDictionary() const { return _dict; }
   TypeID id() const                        { return _id; }
   size_t size() const                      { return _size; }
   ValuePrinter *printer() const            { return _valuePrinter; }

   bool operator!=(const Type & other) const
      {
      return _id != other._id;
      }
   bool operator==(const Type & other) const
      {
      return _id == other._id;
      }
   virtual bool isPointer() const      { return false; }
   virtual bool isStruct() const       { return false; }
   virtual bool isUnion() const        { return false; }
   virtual bool isField() const        { return false; }
   virtual bool isFunction() const     { return false; }
   virtual bool isDynamic() const      { return false; }

   virtual void printType(TextWriter *w);
   virtual void printValue(TextWriter *w, void *p) const { assert(_valuePrinter); _valuePrinter(w, this, p); }

   // returning NULL from the next function means that values of this type cannot be broken down further
   virtual StructType *layout() const { return NULL; }
   virtual LiteralMapper *explode(LiteralValue *value, LiteralMapper *m=NULL) const { return NULL; }

   static TypeID maxID() { return globalIndex; }

   //
   // User type api
   // BEGIN here {

   // } END here
   // User type api
   // 

protected:
   static Type * create(TypeDictionary *dict, std::string name, size_t size, ValuePrinter *printer) { return new Type(dict, name, size, printer); }
   Type(TypeDictionary *dict, std::string name, size_t size, ValuePrinter *printer=NULL)
      : _dict(dict)
      , _id(globalIndex++)
      , _name(name)
      , _size(size)
      , _valuePrinter(printer)
      { }

   TypeDictionary * _dict;
   TypeID           _id;
   std::string      _name;
   size_t           _size;
   ValuePrinter *   _valuePrinter;

   static TypeID globalIndex;
   };

class NoTypeType : public Type
   {
   public:
   static NoTypeType * create(TypeDictionary *dict) { return new NoTypeType(dict); }

   protected:
   NoTypeType(TypeDictionary *dict) : Type(dict, "NoType", 0, &printer) { }
   static void printer(TextWriter *w, const Type *t, void *p);
   };


class NumericType : public Type
   {
   protected:
   NumericType(TypeDictionary *dict, std::string name, size_t size, Type::ValuePrinter *printer=NULL)
      : Type(dict, name, size, printer)
      { }
   };

class IntegerType : public NumericType
   {
   protected:
   IntegerType(TypeDictionary *dict, std::string name, size_t size, Type::ValuePrinter *printer=NULL)
      : NumericType(dict, name, size, printer)
      { }
   };

class Int8Type : public IntegerType
   {
   public:
   static Int8Type * create(TypeDictionary *dict) { return new Int8Type(dict); }

   protected:
   Int8Type(TypeDictionary *dict) : IntegerType(dict, "Int8", 8, &printer) { }
   static void printer(TextWriter *w, const Type *t, void *p);
   };

class Int16Type : public IntegerType
   {
   public:
   static Int16Type * create(TypeDictionary *dict) { return new Int16Type(dict); }

   protected:
   Int16Type(TypeDictionary *dict) : IntegerType(dict, "Int16", 16, &printer) { }
   static void printer(TextWriter *w, const Type *t, void *p);
   };

class Int32Type : public IntegerType
   {
   public:
   static Int32Type * create(TypeDictionary *dict) { return new Int32Type(dict); }

   protected:
   Int32Type(TypeDictionary *dict) : IntegerType(dict, "Int32", 32, &printer) { }
   static void printer(TextWriter *w, const Type *t, void *p);
   };

class Int64Type : public IntegerType
   {
   public:
   static Int64Type * create(TypeDictionary *dict) { return new Int64Type(dict); }

   protected:
   Int64Type(TypeDictionary *dict) : IntegerType(dict, "Int64", 64, &printer) { }
   static void printer(TextWriter *w, const Type *t, void *p);
   };

class FloatingPointType : public NumericType
   {
   protected:
   FloatingPointType(TypeDictionary *dict, std::string name, size_t size, Type::ValuePrinter *printer=NULL)
      : NumericType(dict, name, size, printer)
      { }
   };

class FloatType : public FloatingPointType
   {
   public:
   static FloatType * create(TypeDictionary *dict) { return new FloatType(dict); }

   protected:
   FloatType(TypeDictionary *dict) : FloatingPointType(dict, "Float", 32, &printer) { }
   static void printer(TextWriter *w, const Type *t, void *p);
   };

class DoubleType : public FloatingPointType
   {
   public:
   static DoubleType * create(TypeDictionary *dict) { return new DoubleType(dict); }

   protected:
   DoubleType(TypeDictionary *dict) : FloatingPointType(dict, "Double", 64, &printer) { }
   static void printer(TextWriter *w, const Type *t, void *p);
   };

class AddressType : public Type
   {
   public:
   static AddressType * create(TypeDictionary *dict) { return new AddressType(dict); }

   protected:
   AddressType(TypeDictionary *dict) : Type(dict, "Address", 64, &printer) { } // should be 32/64
   AddressType(TypeDictionary *dict, std::string name) : Type(dict, name, 64) { } // should be 32/64
   static void printer(TextWriter *w, const Type *t, void *p);
   };


class PointerType : public AddressType
   {
   friend class Type;
   friend class TypeDictionary;

public:
   virtual bool isPointer() const { return true; }

   Type * BaseType() { return _baseType; }

   virtual void printType(TextWriter *w);
   virtual void printValue(TextWriter *w, void *p) const;

protected:
   static PointerType * create(TypeDictionary *dict, std::string name, Type * baseType) { return new PointerType(dict, name, baseType); }
   PointerType(TypeDictionary *dict, std::string name, Type * baseType)
      : AddressType(dict, name) // should really be 32 or 64
      , _baseType(baseType)
      { }
   Type * _baseType;
   };

typedef std::map<std::string, FieldType *>::const_iterator FieldIterator;

class StructType : public Type
   {
   friend class TypeDictionary;

   public:
   virtual bool isStruct() const { return true; }

   bool closed() const           { return _closed; }
   void setClosed(bool c=true)   { _closed = c; }

   virtual FieldType * addField(LiteralValue *name, Type *type, size_t offset);
   FieldIterator FieldsBegin() const { return _fieldsByName.cbegin(); }
   FieldIterator FieldsEnd() const   { return _fieldsByName.cend(); }
   FieldType *LookupField(std::string fieldName)
      {
      auto it = _fieldsByName.find(fieldName);
      if (it == _fieldsByName.end())
         return NULL;
      return it->second;
      }
   FieldIterator RemoveField(FieldIterator &it);

   virtual void printType(TextWriter *w);
   virtual void printValue(TextWriter *w, void *p) const;

   protected:
   static StructType * create(TypeDictionary *dict, std::string name)
      {
      return new StructType(dict, name, 0);
      }

   static StructType * create(TypeDictionary *dict, std::string name, size_t size)
      {
      return new StructType(dict, name, size); // size in bits, used to be bytes!
      }

   StructType(TypeDictionary *dict, std::string name, size_t size)
      : Type(dict, name, size)
      , _closed(false)
      { }

   bool _closed;
   std::map<std::string, FieldType *> _fieldsByName;
   std::multimap<size_t, FieldType *> _fieldsByOffset;
   };

class FieldType : public Type
   {
   public:
   static FieldType * create(TypeDictionary *dict, StructType *structType, LiteralValue *fieldName, Type *type, size_t offset)
      {
      return new FieldType(dict, structType, fieldName, type, offset);
      }

   StructType *owningStruct() const  { return _structType; }
   LiteralValue *fieldName() const   { return _fieldName; }
   Type *type() const                { return _type; }
   size_t offset() const             { return _offset; }

   virtual bool isField() const      { return true; }

   virtual void printType(TextWriter *w);
   virtual void printValue(TextWriter *w, void *p) const;

   protected:
   FieldType(TypeDictionary *dict, StructType *structType, LiteralValue *fieldName, Type *type, size_t offset)
      : Type(dict, fieldName->getString(), type->size())
      , _structType(structType)
      , _fieldName(fieldName)
      , _type(type)
      , _offset(offset)
      { }

   StructType *_structType;
   LiteralValue *_fieldName;
   Type *_type;
   size_t _offset;
   };

class UnionType : public StructType
   {
   friend class TypeDictionary;

   public:
   virtual bool isUnion() const { return true; }

   virtual FieldType * addField(LiteralValue *name, Type *type, size_t unused)
      {
      if (type->size() > _size)
         _size = type->size();
      return this->StructType::addField(name, type, 0);
      }

   virtual void printType(TextWriter *w);
   virtual void printValue(TextWriter *w, void *p) const;

   protected:
   static UnionType * create(TypeDictionary *dict, std::string name)
      {
      return new UnionType(dict, name, 0);
      }
   static UnionType * create(TypeDictionary *dict, std::string name, size_t size)
      {
      return new UnionType(dict, name, size);
      }

   UnionType(TypeDictionary *dict, std::string name, size_t size)
      : StructType(dict, name, size)
      { }
   };

class FunctionType : public Type
   {
   friend class TypeDictionary;

   public:
   static FunctionType * create(TypeDictionary *dict, std::string name, Type *returnType, int32_t numParms, Type ** parmTypes);

   ~FunctionType()
      {
      delete[] _parmTypes;
      }

   virtual bool isFunction() const
      {
      return true;
      }

   Type *returnType() const { return _returnType; }
   int32_t numParms() const { return _numParms; }
   Type *parmType(int p) const { return _parmTypes[p]; }
   Type **parmTypes() const { return _parmTypes; }

   virtual void printType(TextWriter *w);
   virtual void printValue(TextWriter *w, void *p) const;

   protected:
   FunctionType(TypeDictionary *dict, std::string name, Type *returnType, int32_t numParms, Type ** parmTypes)
      : Type(dict, name, 0)
      , _returnType(returnType)
      , _numParms(numParms)
      , _parmTypes(parmTypes)
      {
      }

   Type *_returnType;
   int32_t _numParms;
   Type **_parmTypes;
   };

//
// User type classes
// BEGIN here {

// } END here
// User type classes
// 

} // namespace JitBuilder

} // namespace OMR

#endif // defined(TYPE_INCL)
