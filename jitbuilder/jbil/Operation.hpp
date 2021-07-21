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

#ifndef OPERATION_INCL
#define OPERATION_INCL

#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "Action.hpp"
#include "Case.hpp"
#include "Iterator.hpp"
#include "LiteralValue.hpp"
#include "OperationBase.hpp"
#include "TypeReplacer.hpp"

namespace OMR
{

namespace JitBuilder
{

class Builder;
class Case;
class Location;
class OperationCloner;
class Type;
class TypeDictionary;
class TypeGraph;
class Value;

// Operation defines an interface to all kinds of operations, it cannot itself be instantiated
// Operation classes corresponding to specific Actions (which can be instantiated) follow

class Operation : public OperationBase
   {
   public:
   virtual size_t size() const { return sizeof(Operation); }

   // define any new public API here

protected:
   Operation(Action a, Builder * parent);

   // define any new constructors here

   // define any new protected API here

   };

// Following are "structural" classes that are used to hold results(R), literals(L),
// operand values (V), struct/field names (F), and builders (B) for different kinds
// of Operations. The name signifies the structure using the designator letters
// listed above. So the structural Operation class with one result and two operand
// values is called OperationR1V2. Operation sub classes then derive from these
// structural classes and can add some semantically relevant services.
class OperationR1 : public Operation
   {
   public:
   virtual size_t size() const                { return sizeof(OperationR1); }
   virtual ValueIterator ResultsBegin()       { return ValueIterator(_result); }
   virtual int32_t numResults() const         { return 1; }
   virtual Value * result(int i=0) const
      {
      if (i == 0) return _result;
      return NULL;
      }

   protected:
   OperationR1(Action a, Builder * parent, Value * result)
      : Operation(a, parent)
      , _result(result)
      { }

   Value * _result;
   };

class OperationR1L1 : public OperationR1
   {
   public:
   virtual int32_t numLiterals() const { return 1; }
   virtual LiteralValue *literal(int i=0) const
      {
      if (i == 0) return _v;
      return NULL;
      }
   virtual LiteralIterator LiteralsBegin()       { return LiteralIterator(_v); }

   protected:
   OperationR1L1(Action a, Builder * parent, Value * result, LiteralValue *value)
      : OperationR1(a, parent, result)
      , _v(value)
      { }

   LiteralValue *_v;
   };

class OperationR1L1T1 : public OperationR1L1
   {
   public:
   virtual size_t size() const      { return sizeof(OperationR1L1T1); }
   virtual int32_t numTypes() const { return 1; }
   virtual Type * type(int i=0) const
      {
      if (i == 0) return _elementType;
      return NULL;
      }
   virtual TypeIterator TypesBegin() { return TypeIterator(_elementType); }

   protected:
   OperationR1L1T1(Action a, Builder * parent, Value * result, LiteralValue *numElements, Type *elementType)
      : OperationR1L1(a, parent, result, numElements)
      , _elementType(elementType)
      { }

   Type *_elementType;
   };

class OperationR1S1 : public OperationR1
   {
   public:
   virtual int32_t numSymbols() const { return 1; }
   virtual Symbol *symbol(int i=0) const
      {
      if (i == 0) return _symbol;
      return NULL;
      }
   virtual SymbolIterator SymbolsBegin() { return SymbolIterator(_symbol); }

   protected:
   OperationR1S1(Action a, Builder * parent, Value * result, Symbol *symbol)
      : OperationR1(a, parent, result)
      , _symbol(symbol)
      { }

   Symbol *_symbol;
   };

class OperationR0V1 : public Operation
   {
   public:
   virtual size_t size() const         { return sizeof(OperationR0V1); }
   virtual int32_t numOperands() const { return 1; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0) return _value;
      return NULL;
      }

   virtual ValueIterator OperandsBegin()       { return ValueIterator(_value); }

   protected:
   OperationR0V1(Action a, Builder * parent, Value * value)
      : Operation(a, parent)
      , _value(value)
      { }

   Value * _value;
   };

class OperationR1V1 : public OperationR1
   {
   public:
   virtual size_t size() const         { return sizeof(OperationR1V1); }
   virtual int32_t numOperands() const { return 1; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0) return _value;
      return NULL;
      }

   virtual ValueIterator OperandsBegin()       { return ValueIterator(_value); }

   protected:
   OperationR1V1(Action a, Builder * parent, Value * result, Value * value)
      : OperationR1(a, parent, result)
      , _value(value)
      { }

   Value * _value;
   };

class OperationR1V1T1 : public OperationR1V1
   {
   public:
   virtual size_t size() const         { return sizeof(OperationR1V1T1); }
   virtual int32_t numTypes() const { return 1; }
   virtual Type * type(int i=0) const
      {
      if (i == 0) return _type;
      return NULL;
      }

   virtual TypeIterator TypesBegin()       { return TypeIterator(_type); }

   protected:
   OperationR1V1T1(Action a, Builder * parent, Value * result, Type * t, Value * v)
      : OperationR1V1(a, parent, result, v)
      , _type(t)
      { }

   Type * _type;
   };

class OperationR0V2 : public Operation
   {
   public:
   virtual size_t size() const         { return sizeof(OperationR0V2); }
   virtual int32_t numOperands() const { return 2; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0) return _left;
      if (i == 1) return _right;
      return NULL;
      }
   virtual Value * getLeft() const  { return _left; }
   virtual Value * getRight() const { return _right; }

   virtual ValueIterator OperandsBegin()       { return ValueIterator(_left, _right); }

   protected:
   OperationR0V2(Action a, Builder * parent, Value * left, Value * right)
      : Operation(a, parent)
      , _left(left)
      , _right(right)
      { }

   Value * _left;
   Value * _right;
   };

class OperationR1V2 : public OperationR1
   {
   public:
   virtual size_t size() const         { return sizeof(OperationR1V2); }
   virtual int32_t numOperands() const { return 2; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0) return _left;
      if (i == 1) return _right;
      return NULL;
      }
   virtual Value * getLeft() const  { return _left; }
   virtual Value * getRight() const { return _right; }

   virtual ValueIterator OperandsBegin()       { return ValueIterator(_left, _right); }

   protected:
   OperationR1V2(Action a, Builder * parent, Value * result, Value * left, Value * right)
      : OperationR1(a, parent, result)
      , _left(left)
      , _right(right)
      { }

   Value * _left;
   Value * _right;
   };

class OperationR1V2T1 : public OperationR1V2
   {
   public:
   virtual size_t size() const         { return sizeof(OperationR1V2T1); }
   virtual int32_t numTypes() const { return 1; }
   virtual Type * type(int i=0) const
      {
      if (i == 0) return _type;
      return NULL;
      }
   virtual TypeIterator TypesBegin()       { return TypeIterator(_type); }

   virtual int32_t numOperands() const { return 2; }
   virtual Value *operand(int i=0) const
      {
      if (i == 0) return _left;
      if (i == 1) return _right;
      return NULL;
      }
   virtual Value * getAddress() const  { return _left; }
   virtual Value * getValue() const    { return _right; }
   virtual ValueIterator OperandsBegin()       { return ValueIterator(_left, _right); }


   protected:
   OperationR1V2T1(Action a, Builder * parent, Value * result, Type * t, Value * addr, Value * v)
      : OperationR1V2(a, parent, result, addr, v)
      , _type(t)
      { }

   Type * _type;
   };

class OperationR1T1 : public OperationR1
   {
   public:
   virtual size_t size() const         { return sizeof(OperationR1V2T1); }
   virtual int32_t numTypes() const { return 1; }
   virtual Type * type(int i=0) const
      {
      if (i == 0) return _type;
      return NULL;
      }
   virtual TypeIterator TypesBegin()       { return TypeIterator(_type); }

   protected:
   OperationR1T1(Action a, Builder * parent, Value * result, Type * t)
      : OperationR1(a, parent, result)
      , _type(t)
      { }

   Type * _type;
   };

class OperationR0S1V1 : public Operation
   {
   public:
   virtual size_t size() const { return sizeof(OperationR0S1V1); }

   virtual int32_t numSymbols() const { return 1; }
   virtual Symbol *symbol(int i=0) const
      {
      if (i == 0) return _symbol;
      return NULL;
      }
   virtual SymbolIterator SymbolsBegin() { return SymbolIterator(_symbol); }

   virtual int32_t numOperands() const { return 1; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0) return _value;
      return NULL;
      }
   virtual ValueIterator OperandsBegin() { return ValueIterator(_value); }

   protected:
   OperationR0S1V1(Action a, Builder * parent, Symbol *symbol, Value * value)
      : Operation(a, parent)
      , _symbol(symbol)
      , _value(value)
      { }

   Symbol *_symbol;
   Value * _value;
   };

class OperationR0T1 : public Operation
   {
   public:
   virtual size_t size() const { return sizeof(OperationR0T1); }

   virtual int32_t numTypes() const { return 1; }
   virtual Type *type(int i=0) const
      {
      if (i == 0) return _type;
      return NULL;
      }
   virtual TypeIterator TypesBegin()       { return TypeIterator(_type); }

   protected:
   OperationR0T1(Action a, Builder *parent, Type *type)
      : Operation(a, parent)
      , _type(type)
      { }

   Type *_type;
   };

class OperationR0V2T1 : public OperationR0T1
   {
   public:
   virtual size_t size() const { return sizeof(OperationR0V2T1); }
   virtual int32_t numOperands() const   { return 2; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0) return _structBase;
      if (i == 1) return _value;
      return NULL;
      }
   virtual ValueIterator OperandsBegin()       { return ValueIterator(_structBase, _value); }

   protected:
   OperationR0V2T1(Action a, Builder *parent, FieldType *fieldType, Value * structBase, Value * value)
      : OperationR0T1(a, parent, fieldType)
      , _structBase(structBase)
      , _value(value)
      { }

   Value *_structBase;
   Value *_value;
   };

class OperationB1 : public Operation
   {
   public:
   virtual size_t size() const { return sizeof(OperationB1); }
   virtual int32_t numBuilders() const { return 1; }
   virtual Builder * builder(int i=0) const
      {
      if (i == 0) return _builder;
      return NULL;
      }

   virtual BuilderIterator BuildersBegin()       { return BuilderIterator(_builder); }

   protected:
   OperationB1(Action a, Builder * parent, Builder * b)
      : Operation(a, parent)
      , _builder(b)
      { }

   Builder * _builder;
   };

class OperationR0V2B1 : public OperationR0V2
   {
   public:
   virtual size_t size() const { return sizeof(OperationR0V2B1); }
   virtual int32_t numBuilders() const { return 1; }
   virtual Builder * builder(int i=0) const
      {
      if (i == 0) return _builder;
      return NULL;
      }
   virtual BuilderIterator BuildersBegin()       { return BuilderIterator(_builder); }

   protected:
   OperationR0V2B1(Action a, Builder * parent, Builder * b, Value * left, Value * right)
      : OperationR0V2(a, parent, left, right)
      , _builder(b)
      { }

   Builder * _builder;
   };

class OperationR0V1B1 : public OperationR0V1
   {
   public:
   virtual size_t size() const { return sizeof(OperationR0V1B1); }
   virtual int32_t numBuilders() const { return 1; }
   virtual Builder * builder(int i=0) const
      {
      if (i == 0) return _builder;
      return NULL;
      }
   virtual BuilderIterator BuildersBegin()       { return BuilderIterator(_builder); }

   protected:
   OperationR0V1B1(Action a, Builder * parent, Builder * b, Value * value)
      : OperationR0V1(a, parent, value)
      , _builder(b)
      { }

   Builder * _builder;
   };



//
// classes for specific operations that can be directly instantiated
// These classes are manipulated primarily by Builder
//

class ConstInt8 : public OperationR1L1
   {
   public:
   virtual size_t size() const { return sizeof(ConstInt8); }
   static ConstInt8 * create(Builder * parent, Value * result, int8_t value)
      { return new ConstInt8(parent, result, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal()->getInt8());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal()->getInt8());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ConstInt8(Builder * parent, Value * result, int8_t value);
   };

class ConstInt16 : public OperationR1L1
   {
   public:
   virtual size_t size() const { return sizeof(ConstInt16); }
   static ConstInt16 * create(Builder * parent, Value * result, int16_t value)
      { return new ConstInt16(parent, result, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal()->getInt16());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal()->getInt16());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ConstInt16(Builder * parent, Value * result, int16_t value);
   };

class ConstInt32 : public OperationR1L1
   {
   public:
   virtual size_t size() const { return sizeof(ConstInt32); }
   static ConstInt32 * create(Builder * parent, Value * result, int32_t value)
      { return new ConstInt32(parent, result, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal()->getInt32());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal()->getInt32());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ConstInt32(Builder * parent, Value * result, int32_t value);
   };

class ConstInt64 : public OperationR1L1
   {
   public:
   virtual size_t size() const { return sizeof(ConstInt64); }
   static ConstInt64 * create(Builder * parent, Value * result, int64_t value)
      { return new ConstInt64(parent, result, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal()->getInt64());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal()->getInt64());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ConstInt64(Builder * parent, Value * result, int64_t value);
   };

class ConstFloat : public OperationR1L1
   {
   public:
   virtual size_t size() const { return sizeof(ConstFloat); }
   static ConstFloat * create(Builder * parent, Value * result, float value)
      { return new ConstFloat(parent, result, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal()->getFloat());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal()->getFloat());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ConstFloat(Builder * parent, Value * result, float value);
   };

class ConstDouble : public OperationR1L1
   {
   public:
   virtual size_t size() const { return sizeof(ConstDouble); }
   static ConstDouble * create(Builder * parent, Value * result, double value)
      { return new ConstDouble(parent, result, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal()->getDouble());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal()->getDouble());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ConstDouble(Builder * parent, Value * result, double value);
   };

class ConstAddress : public OperationR1L1
   {
   public:
   virtual size_t size() const { return sizeof(ConstAddress); }
   static ConstAddress * create(Builder * parent, Value * result, void * value)
      { return new ConstAddress(parent, result, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal()->getAddress());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal()->getAddress());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ConstAddress(Builder * parent, Value * result, void * value);
   };

class CoercePointer : public OperationR1V1T1
   {
   public:
   static CoercePointer * create(Builder * parent, Value * result, Type * type, Value * value)
      { return new CoercePointer(parent, result, type, value); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], type(0), operand(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && operands && NULL == builders);
      return create(b, results[0], type(0), operands[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   protected:
   CoercePointer(Builder * parent, Value * result, Type * t, Value * v);
   };

class Add : public OperationR1V2
   {
   public:
   static Add * create(Builder * parent, Value * result, Value * left, Value * right)
      { return new Add(parent, result, left, right); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && operands && NULL == builders);
      return create(b, results[0], operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Add(Builder * parent, Value * result, Value * left, Value * right);
   };

class Sub : public OperationR1V2
   {
   public:
   static Sub * create(Builder * parent, Value * result, Value * left, Value * right)
      { return new Sub(parent, result, left, right); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && operands && NULL == builders);
      return create(b, results[0], operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Sub(Builder * parent, Value * result, Value * left, Value * right);
   };

class Mul : public OperationR1V2
   {
   public:
   static Mul * create(Builder * parent, Value * result, Value * left, Value * right)
      { return new Mul(parent, result, left, right); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == builders);
      return create(b, results[0], operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Mul(Builder * parent, Value * result, Value * left, Value * right);
   };

class IndexAt : public OperationR1V2T1
   {
   public:
   static IndexAt * create(Builder * parent, Value * result, Type * pointerType, Value * address, Value * value)
      { return new IndexAt(parent, result, pointerType, address, value); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], type(0), operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], type(0), operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   IndexAt(Builder * parent, Value * result, Type * pointerType, Value * address, Value * value);
   };

class Load : public OperationR1S1
   {
   public:
   static Load * create(Builder * parent, Value * result, Symbol *local)
      { return new Load(parent, result, local); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], symbol());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], symbol());
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Load(Builder * parent, Value * result, std::string name);
   Load(Builder * parent, Value * result, Symbol *s)
      : OperationR1S1(aLoad, parent, result, s)
      { }
   };

class LoadAt : public OperationR1V1T1
   {
   public:
   static LoadAt * create(Builder * parent, Value * result, Type * pointerType, Value * address)
      { return new LoadAt(parent, result, pointerType, address); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], type(0), operand(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && operands && NULL == builders);
      return create(b, results[0], type(0), operands[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   virtual bool hasExpander() const { return true; }
   virtual bool expand(OperationReplacer *r) const;

   protected:
   LoadAt(Builder * parent, Value * result, Type * pointerType, Value * address);
   };

class LoadField : public OperationR1V1T1
   {
   public:
   static LoadField * create(Builder * parent, Value * result, FieldType *fieldType, Value *structBase)
      { return new LoadField(parent, result, fieldType, structBase); }

   FieldType *getFieldType() const { return static_cast<FieldType *>(_type); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], getFieldType(), operand(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && operands && NULL == builders);
      return create(b, results[0], getFieldType(), operands[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   LoadField(Builder * parent, Value * result, FieldType *fieldType, Value * structBase)
      : OperationR1V1T1(aLoadField, parent, result, fieldType, structBase)
      { }
   };

class LoadIndirect : public OperationR1V1T1
   {
   public:
   static LoadIndirect * create(Builder * parent, Value * result, FieldType *fieldType, Value *structBase)
      { return new LoadIndirect(parent, result, fieldType, structBase); }

   FieldType *getFieldType() const { return static_cast<FieldType *>(_type); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], getFieldType(), operand(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && operands && NULL == builders);
      return create(b, results[0], getFieldType(), operands[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   LoadIndirect(Builder * parent, Value * result, FieldType *fieldType, Value * structBase)
      : OperationR1V1T1(aLoadIndirect, parent, result, fieldType, structBase)
      { }
   };

class Store : public OperationR0S1V1
   {
   public:
   static Store * create(Builder * parent, Symbol *local, Value * value)
      { return new Store(parent, local, value); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, symbol(), operand(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && NULL == builders);
      return create(b, symbol(), operands[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Store(Builder * parent, std::string name, Value * value);
   Store(Builder * parent, Symbol *s, Value * value)
      : OperationR0S1V1(aStore, parent, s, value)
      { }
   };

class StoreAt : public OperationR0V2
   {
   public:
   static StoreAt * create(Builder * parent, Value * address, Value * value)
      { return new StoreAt(parent, address, value); }
   
   virtual Value * getAddress() const { return _left; }
   virtual Value * getValue() const   { return _right; }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, getAddress(), getValue());
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && NULL == builders);
      return create(b, operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   virtual bool hasExpander() const { return true; }
   virtual bool expand(OperationReplacer *r) const;

   protected:
   StoreAt(Builder * parent, Value * address, Value * value);
   };

class StoreField : public OperationR0V2T1
   {
   public:
   static StoreField * create(Builder * parent, FieldType *fieldType, Value *structBase, Value *value)
      { return new StoreField(parent, fieldType, structBase, value); }

   FieldType *getFieldType() const { return static_cast<FieldType *>(_type); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, getFieldType(), operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && NULL == builders);
      return create(b, getFieldType(), operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   StoreField(Builder * parent, FieldType *fieldType, Value * structBase, Value *value)
      : OperationR0V2T1(aStoreField, parent, fieldType, structBase, value)
      { }
   };

class StoreIndirect : public OperationR0V2T1
   {
   public:
   static StoreIndirect * create(Builder * parent, FieldType *fieldType, Value *structBase, Value *value)
      { return new StoreIndirect(parent, fieldType, structBase, value); }

   FieldType *getFieldType() const { return static_cast<FieldType *>(_type); }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, getFieldType(), operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && NULL == builders);
      return create(b, getFieldType(), operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   StoreIndirect(Builder * parent, FieldType *fieldType, Value * structBase, Value *value)
      : OperationR0V2T1(aStoreIndirect, parent, fieldType, structBase, value)
      { }
   };

class AppendBuilder : public OperationB1
   {
   public:
   static AppendBuilder * create(Builder * parent, Builder * b)
      { return new AppendBuilder(parent, b); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, builder(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && NULL == operands && builders);
      return create(b, builders[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   AppendBuilder(Builder * parent, Builder * b);
   };

class Call : public Operation
   {
   public:
   static Call * create(Builder * parent, Value *function, int32_t numArgs, va_list args)
      {
      Call *call = new Call(parent, function, NULL, numArgs, args);
      return call;
      }
   static Call * create(Builder * parent, Value *function, Value *result, int32_t numArgs, va_list args)
      {
      Call *call = new Call(parent, function, result, numArgs, args);
      return call;
      }
   static Call * create(Builder * parent, Value *function, int32_t numArgs, Value **args)
      {
      Call *call = new Call(parent, function, NULL, numArgs, args);
      return call;
      }
   static Call * create(Builder * parent, Value *function, Value *result, int32_t numArgs, Value **args)
      {
      Call *call = new Call(parent, function, result, numArgs, args);
      return call;
      }

   virtual size_t size() const { return sizeof(Call); }
   
   Value *function() const  { return _function; }
   int32_t numArguments() const { return _numArgs; }
   Value *argument(int32_t a) const
      {
      if (a < _numArgs)
         return _args[a];
      return NULL;
      }

   virtual int32_t numOperands() const   { return _numArgs+1; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0)
         return _function;
      else if (i >= 1 && i <= _numArgs)
         return _args[i-1];
      return NULL;
      }
   virtual ValueIterator OperandsBegin()
      {
      ValueIterator it1(_function);
      ValueIterator it2(_args, _numArgs);
      it2.prepend(it1);
      return it2;
      }

   virtual int32_t numResults() const          { return (_result != NULL) ? 1 : 0; }
   virtual Value * result(int i=0) const
      {
      if (i == 0) return _result; // may return NULL if there is no result
      return NULL;
      }
   virtual ValueIterator ResultsBegin()
      {
      if (_result)
         return ValueIterator(_result);
      return ResultsEnd();
      }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      Value **cloneArgs = new Value *[_numArgs];
      for (int32_t a=0;a < _numArgs;a++)
         cloneArgs[a] = _args[a];
      if (_result)
         {
         assert(results);
         return new Call(b, operand(0), results[0], _numArgs, cloneArgs);
         }
      else
         {
         assert(NULL == results);
         return new Call(b, operand(0), NULL, _numArgs, cloneArgs);
         }
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      if (_result)
         {
         assert(results && operands && NULL == builders);
         return new Call(b, operands[0], results[0], _numArgs-1, operands+1);
         }
      else
         {
         assert(NULL == results && operands && NULL == builders);
         return new Call(b, operands[0], NULL, _numArgs-1, operands+1);
         }
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Call(Builder * parent, Value *result, Value *function, int32_t numArgs, va_list args);
   Call(Builder * parent, Value *result, Value *function, int32_t numArgs, Value **args);

   Value *_function;
   Value *_result;
   int32_t _numArgs;
   Value **_args;
   };

class Goto : public OperationB1
   {
   public:
   static Goto * create(Builder * parent, Builder * b)
      { return new Goto(parent, b); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, builder(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && NULL == operands && builders);
      return create(b, builders[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Goto(Builder * parent, Builder * b);
   };

class ForLoop : public Operation
   {
   public:
   virtual size_t size() const { return sizeof(ForLoop); }
   static ForLoop * create(Builder * parent, bool countsUp, LocalSymbol * loopSym,
                           Builder * loopBody, Builder * loopBreak, Builder * loopContinue,
                           Value * initial, Value * end, Value * bump);
   static ForLoop * create(Builder * parent, bool countsUp, LocalSymbol * loopSym,
                           Builder * loopBody, Builder * loopBreak,
                           Value * initial, Value * end, Value * bump);
   static ForLoop * create(Builder * parent, bool countsUp, LocalSymbol * loopSym,
                           Builder * loopBody, Value * initial, Value * end, Value * bump);

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual bool countsUp() const                       { return (bool)(_countsUp->getInt8()); }
   virtual LocalSymbol *getLoopSymbol() const          { return _loopSym; }

   virtual Value * getInitial() const                  { return _initial; }
   virtual Value * getEnd() const                      { return _end; }
   virtual Value * getBump() const                     { return _bump; }

   virtual int32_t numLiterals() const                 { return 1; }
   virtual LiteralValue *literal(int i=0) const
      {
      if (i == 0) return _countsUp;
      return NULL;
      }
   virtual LiteralIterator LiteralsBegin()             { return LiteralIterator(_countsUp); }

   virtual int32_t numSymbols() const                  { return 1; }
   virtual Symbol *symbol(int i=0) const
      {
      if (i == 0) return _loopSym;
      return NULL;
      }
   virtual SymbolIterator SymbolsBegin()               { return SymbolIterator(_loopSym); }

   virtual int32_t numOperands() const                 { return 3; }
   virtual Value * operand(int i=0) const
      {
      if (i == 0) return _initial;
      if (i == 1) return _end;
      if (i == 2) return _bump;
      return NULL;
      }
   virtual ValueIterator OperandsBegin()               { return ValueIterator(_initial, _end, _bump); }

   virtual Builder * getBody() const                   { return _loopBody; }
   virtual Builder * getBreak() const                  { return _loopBreak; }
   virtual Builder * getContinue() const               { return _loopContinue; }
   virtual int32_t numBuilders() const                 { return 1 + (_loopBreak ? 1 : 0) + (_loopContinue ? 1 : 0); }

   virtual Builder * builder(int i=0) const
      {
      if (i == 0) return _loopBody;
      if (i == 1) return _loopBreak;
      if (i == 2) return _loopContinue;
      return NULL;
      }

   virtual BuilderIterator BuildersBegin()
      {
      if (_loopContinue) return BuilderIterator(_loopBody, _loopBreak, _loopContinue);
      if (_loopBreak)    return BuilderIterator(_loopBody, _loopBreak);
      return BuilderIterator(_loopBody);
      }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, literal(0)->getInt8(), _loopSym, builder(0), builder(1), builder(2), operand(0), operand(1), operand(2));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && builders);
      return create(b, literal(0)->getInt8(), _loopSym, builders[0], _loopBreak ? builders[1] : NULL, _loopContinue ? builders[2] : NULL, operands[0], operands[1], operands[2]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   ForLoop(Builder * parent, bool countsUp, LocalSymbol *loopSym,
           Builder * loopBody, Builder * loopBreak, Builder * loopContinue,
           Value * initial, Value * end, Value * bump);
   ForLoop(Builder * parent, bool countsUp, LocalSymbol *loopSym,
           Builder * loopBody, Builder * loopBreak,
           Value * initial, Value * end, Value * bump);
   ForLoop(Builder * parent, bool countsUp, LocalSymbol *loopSym,
           Builder * loopBody,
           Value * initial, Value * end, Value * bump);

   LiteralValue *_countsUp;
   LocalSymbol *_loopSym;

   Builder * _loopBody;
   Builder * _loopBreak;
   Builder * _loopContinue;

   Value * _initial;
   Value * _end;
   Value * _bump;
   };

class IfCmpGreaterThan : public OperationR0V2B1
   {
   public:
   static IfCmpGreaterThan * create(Builder * parent, Builder * tgt, Value * left, Value * right)
      { return new IfCmpGreaterThan(parent, tgt, left, right); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, builder(0), operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && builders);
      return create(b, builders[0], operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   IfCmpGreaterThan(Builder * parent, Builder * tgt, Value * left, Value * right);
   };

class IfCmpLessThan : public OperationR0V2B1
   {
   public:
   static IfCmpLessThan * create(Builder * parent, Builder * tgt, Value * left, Value * right)
      { return new IfCmpLessThan(parent, tgt, left, right); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, builder(0), operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && builders);
      return create(b, builders[0], operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   IfCmpLessThan(Builder * parent, Builder * tgt, Value * left, Value * right);
   };

class IfCmpGreaterOrEqual : public OperationR0V2B1
   {
   public:
   static IfCmpGreaterOrEqual * create(Builder * parent, Builder * tgt, Value * left, Value * right)
      { return new IfCmpGreaterOrEqual(parent, tgt, left, right); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, builder(0), operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && builders);
      return create(b, builders[0], operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   IfCmpGreaterOrEqual(Builder * parent, Builder * tgt, Value * left, Value * right);
   };

class IfCmpLessOrEqual : public OperationR0V2B1
   {
   public:
   static IfCmpLessOrEqual * create(Builder * parent, Builder * tgt, Value * left, Value * right)
      { return new IfCmpLessOrEqual(parent, tgt, left, right); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, builder(0), operand(0), operand(1));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && builders);
      return create(b, builders[0], operands[0], operands[1]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   IfCmpLessOrEqual(Builder * parent, Builder * tgt, Value * left, Value * right);
   };

class IfThenElse : public OperationR0V1B1
   {
   public:
   virtual size_t size() const { return sizeof(IfThenElse); }
   static IfThenElse * create(Builder * parent, Builder * thenB, Builder * elseB, Value * cond);
   static IfThenElse * create(Builder * parent, Builder * thenB, Value * cond);

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Builder * getThenBuilder() const { return _builder; }
   virtual Builder * getElseBuilder() const { return _elseBuilder; }

   virtual int32_t numBuilders() const { return _elseBuilder ? 2 : 1; }
   virtual Builder * builder(int i=0) const
      {
      if (i == 0)
         return _builder;
      else if (i == 1 && _elseBuilder)
         return _elseBuilder;
      return NULL;
      }
   virtual BuilderIterator BuildersBegin()
      {
      if (_elseBuilder)
         return BuilderIterator(_builder, _elseBuilder);
      return BuilderIterator(_builder);
      }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      return create(b, builder(0), builder(1), operand(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && operands && builders);
      return create(b, builders[0], _elseBuilder ? builders[1] : NULL, operands[0]);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   IfThenElse(Builder * parent, Builder * thenB, Builder * elseB, Value * cond);
   IfThenElse(Builder * parent, Builder * thenB, Value * cond);

   Builder * _elseBuilder;
   };

class Return : public Operation
   {
   public:
   virtual size_t size() const { return sizeof(Return); }
   static Return * create(Builder * parent)
      { return new Return(parent); }
   static Return * create(Builder * parent, Value * v)
      { return new Return(parent, v); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual int32_t numOperands() const { return _value ? 1 : 0; }
   virtual Value * operand(int32_t i=0) const
      {
      if (i == 0 && _value)
         return _value;
      return NULL;
      }

   virtual ValueIterator OperandsBegin()
      {
      if (_value)
         return ValueIterator(_value);
      else
         return OperandsEnd();
      }

   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(NULL == results);
      if (numOperands() > 0)
         return create(b, operand(0));
      return create(b);
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(NULL == results && NULL == builders);
      if (numOperands() > 0)
         return create(b, operands[0]);
      return create(b);
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Return(Builder * parent);
   Return(Builder * parent, Value * v);

   Value * _value;
   };

class Switch : public OperationR0V1
   {
   public:
   virtual size_t size() const { return sizeof(Switch); }
   static Switch * create(Builder * parent, Value *selector, Builder *defaultTarget, int numCases, Case ** cases)
      { return new Switch(parent, selector, defaultTarget, numCases, cases); }

   virtual Value * getSelector() const { return _value; }

   virtual int32_t numBuilders() const { return 1 + _cases.size(); }
   virtual Builder *builder(int32_t i=0) const
      {
      if (i == 0)
         return _defaultTarget;
      else if (i-1 < _cases.size())
         return _cases[i-1]->builder();
      return NULL;
      }
   virtual BuilderIterator BuildersBegin()
      {
      std::vector<Builder *> it;
      it.push_back(_defaultTarget);
      for (auto cIt = CasesBegin(); cIt != CasesEnd(); cIt++)
         it.push_back((*cIt)->builder());
      return BuilderIterator(it);
      }

   virtual int32_t numCases() const  { return _cases.size(); }
   virtual CaseIterator CasesBegin() { return CaseIterator(_cases); }

   static void initializeTypeProductions(TypeDictionary * types, TypeGraph * graph);

   virtual Operation * clone(Builder *b, Value **results) const;
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const;
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   Switch(Builder * parent, Value *selector, Builder *defaultCase, int numCases, Case ** cases);

   Builder *_defaultTarget;
   std::vector<Case *> _cases;
   };

class CreateLocalArray : public OperationR1L1T1
   {
   public:
   static CreateLocalArray * create(Builder * parent, Value * result, int32_t numElements, Type * elementType)
      { return new CreateLocalArray(parent, result, numElements, elementType); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], literal(0)->getInt32(), type(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], literal(0)->getInt32(), type(0));
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   CreateLocalArray(Builder * parent, Value * result, int32_t numElements, Type * elementType);
   };

class CreateLocalStruct : public OperationR1T1
   {
   public:
   static CreateLocalStruct * create(Builder * parent, Value * result, Type * structType)
      { return new CreateLocalStruct(parent, result, structType); }
   
   virtual Operation * clone(Builder *b, Value **results) const
      {
      assert(results);
      return create(b, results[0], type(0));
      }
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const
      {
      assert(results && NULL == operands && NULL == builders);
      return create(b, results[0], type(0));
      }
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;

   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   protected:
   CreateLocalStruct(Builder * parent, Value * result, Type * structType);
   };

// Add any new operation classes here
// BEGIN {
//

//
// } END
// Add any new operation classes here

} // namespace JitBuilder

} // namespace OMR

#endif // defined(OPERATION_INCL)
