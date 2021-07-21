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

#include <string>
#include <vector>
#include "Action.hpp"
#include "Operation.hpp"
#include "Builder.hpp"
#include "FunctionBuilder.hpp"
#include "Location.hpp"
#include "DynamicOperation.hpp"
#include "OperationCloner.hpp"
#include "TextWriter.hpp"
#include "Value.hpp"
#include "Transformer.hpp"
#include "TypeDictionary.hpp"

using namespace OMR::JitBuilder;

int64_t BuilderBase::globalIndex = 0;

BuilderBase::BuilderBase(Builder * parent, FunctionBuilder * fb, TypeDictionary *types)
   : Object(fb)
   , _id(globalIndex++)
   , _name("")
   , _parent(parent)
   , _successor(0)
   , _currentLocation(parent->location())
   , _boundToOperation(NULL)
   , _isTarget(false)
   , _isBound(false)
   , _boundness(May)
   , NoType(types->NoType)
   , Int8(types->Int8)
   , Int16(types->Int16)
   , Int32(types->Int32)
   , Int64(types->Int64)
   , Float(types->Float)
   , Double(types->Double)
   , Address(types->Address)
   {
   if (parent != this) // FunctionBuilders have parent==this, so don't add itself as child
      parent->addChild(self());
   }

BuilderBase::BuilderBase(Builder * parent, TypeDictionary *types)
   : Object(parent->fb())
   , _id(globalIndex++)
   , _name("")
   , _parent(parent)
   , _successor(0)
   , _currentLocation(NULL)
   , _boundToOperation(NULL)
   , _isTarget(false)
   , _isBound(false)
   , _boundness(May)
   , NoType(types->NoType)
   , Int8(types->Int8)
   , Int16(types->Int16)
   , Int32(types->Int32)
   , Int64(types->Int64)
   , Float(types->Float)
   , Double(types->Double)
   , Address(types->Address)
   {
   if (parent != this) // FunctionBuilders have parent==this, so don't add itself as child
      parent->addChild(self());
   }

Builder *
BuilderBase::self()
   {
   return static_cast<Builder *>(this);
   }

TypeDictionary *
BuilderBase::dict() const
   {
   return _fb->dict();
   }

void
BuilderBase::creationError(Action a, std::string msg)
   {
   std::cerr << "Error creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << msg << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string sName, std::string s)
   {
   std::cerr << "Unknown name creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << sName << " : " << s << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string vName, Value * v)
   {
   std::cerr << "Incorrect operand type creatign operation " << actionName(a) << "\n";
   std::cerr << "\t(builder b" << _id << ")\n";
   std::cerr << "\t" << vName << " : " << v << " has type " << v->type() << " (" << v->type()->name() << ")\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string tName, Type * t, std::string vName, Value * v)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder b" << _id << ")\n";
   std::cerr << "\t" << tName << " : " << t << "\n";
   std::cerr << "\t" << vName << " : " << v << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string t1Name, Type * t1, std::string t2Name, Type * t2)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << t1Name << " : " << t1 << "\n";
   std::cerr << "\t" << t2Name << " : " << t2 << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string oneName, Value * one,
                       std::string twoName, Value * two, std::string threeName, Value * three)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << oneName   << " : " << one   << "\n";
   std::cerr << "\t" << twoName   << " : " << two   << "\n";
   std::cerr << "\t" << threeName << " : " << three << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string lName, Value * left, std::string rName, Value * right)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << lName << " : " << left  << "\n";
   std::cerr << "\t" << rName << " : " << right << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string tName, Type * t, std::string firstName, Value * first, std::string secondName, Value * second)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << tName      << " : " << t << "\n";
   std::cerr << "\t" << firstName  << " : " << first  << "\n";
   std::cerr << "\t" << secondName << " : " << second << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string sName, std::string sValue, std::string fName, std::string fValue, std::string bName, Value *bValue)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << sName      << " : " << sValue << "\n";
   std::cerr << "\t" << fName      << " : " << fValue << "\n";
   std::cerr << "\t" << bName      << " : " << bValue << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string sName, std::string sValue, std::string fName, std::string fValue, std::string bName, Value *bValue, std::string vName, Value *vValue)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << sName      << " : " << sValue << "\n";
   std::cerr << "\t" << fName      << " : " << fValue << "\n";
   std::cerr << "\t" << bName      << " : " << bValue << "\n";
   std::cerr << "\t" << vName      << " : " << vValue << "\n";
   assert(0);
   }

void
BuilderBase::creationError(Action a, std::string fName, Value *function, int32_t numArgs, va_list args)
   {
   Value ** argsArray = new Value *[numArgs];
   for (int a=0;a < numArgs;a++)
      argsArray[a] = va_arg(args, Value *);
   creationError(a, fName, function, numArgs, argsArray);
   }

void
BuilderBase::creationError(Action a, std::string fName, Value *function, int32_t numArgs, Value **args)
   {
   std::cerr << "Incorrect operand types creating operation " << actionName(a) << "\n";
   std::cerr << "\t(builder " << this << ")\n";
   std::cerr << "\t" << fName      << " : " << function << "\n";
   std::cerr << "\t" << "numArgs"  << " : " << numArgs  << "\n";
   for (int a=0;a < numArgs;a++)
      std::cerr << "\t" << a          << " : " << args[a] << "\n";
   assert(0);
   }

void
BuilderBase::setParent(Builder *parent)
   {
   _parent = parent;
   _parent->addChild(self());
   }

void
BuilderBase::addChild(Builder *child)
   {
   // shouldn't ever be duplicates, but let's make absolutely sure (assert instead?)
   for (int32_t c=0;c < _children.size();c++)
      if (_children[c] == child)
         return;
   _children.push_back(child);
   }

Builder *
BuilderBase::setBoundness(MustMayCant v)
   {
   assert(v != Must || _isBound == true);
   assert(v != Cant || _isBound == false);
   _boundness = v;
   return self();
   }

void
BuilderBase::checkBoundness(bool v) const
   {
   if (_boundness == May)
         return;

   if (v)
      assert(_boundness == Must);
      else
      assert(_boundness == Cant);
   }

Builder *
BuilderBase::setBound(bool v, Operation * boundToOp)
   {
   checkBoundness(v);
   _isBound = v;
   _boundToOperation = boundToOp;
   return self();
   }

Builder *
BuilderBase::setBound(Operation * boundToOp)
   {
   checkBoundness(true);
   _isBound = true;
   _boundToOperation = boundToOp;
   return self();
   }

Builder *
BuilderBase::setTarget(bool v)
   {
   _isTarget = v;
   return self();
   }

Operation *
BuilderBase::appendClone(Operation *op, OperationCloner *cloner)
   {
   Operation *clonedOp = op->clone(self(), cloner);
   add(clonedOp);
   return clonedOp;
   }

Value *
BuilderBase::ConstInt8 (int8_t v)
   {
   Value * result = Value::create(self(), Int8);
   add(OMR::JitBuilder::ConstInt8::create(self(), result, v));
   return result;
   }

Value *
BuilderBase::ConstInt16 (int16_t v)
   {
   Value * result = Value::create(self(), Int16);
   add(OMR::JitBuilder::ConstInt16::create(self(), result, v));
   return result;
   }

Value *
BuilderBase::ConstInt32 (int32_t v)
   {
   Value * result = Value::create(self(), Int32);
   add(OMR::JitBuilder::ConstInt32::create(self(), result, v));
   return result;
   }

Value *
BuilderBase::ConstInt64 (int64_t v)
   {
   Value * result = Value::create(self(), Int64);
   add(OMR::JitBuilder::ConstInt64::create(self(), result, v));
   return result;
   }

Value *
BuilderBase::ConstFloat (float v)
   {
   Value * result = Value::create(self(), Float);
   add(OMR::JitBuilder::ConstFloat::create(self(), result, v));
   return result;
   }

Value *
BuilderBase::ConstDouble (double v)
   {
   Value * result = Value::create(self(), Double);
   add(OMR::JitBuilder::ConstDouble::create(self(), result, v));
   return result;
   }

Value *
BuilderBase::ConstAddress (void *v)
   {
   Value * result = Value::create(self(), Address);
   add(OMR::JitBuilder::ConstAddress::create(self(), result, v));
   return result;
   }

Value *
BuilderBase::CoercePointer(Type * t, Value * v)
   {
   if (!(v->type()->isPointer() || v->type() == Address) || !t->isPointer())
      creationError(aCoercePointer, "type", t, "value", v);

   Value * result = Value::create(self(), t);
   add(OMR::JitBuilder::CoercePointer::create(self(), result, t, v));
   return result;
   }

Value *
BuilderBase::Add (Value * left, Value * right)
   {
   Type *returnType = dict()->producedType(aAdd, left, right);
   if (!returnType)
      creationError(aAdd, "left", left, "right", right);

   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::Add::create(self(), result, left, right));
   return result;
   }

Value *
BuilderBase::Sub (Value * left, Value * right)
   {
   Type *returnType = dict()->producedType(aSub, left, right);
   if (!returnType)
      creationError(aSub, "left", left, "right", right);

   Value * result = Value::create(self(), returnType);

   add(OMR::JitBuilder::Sub::create(self(), result, left, right));
   return result;
   }

Value *
BuilderBase::Mul (Value * left, Value * right)
   {
   Type *returnType = dict()->producedType(aMul, left, right);
   if (!returnType)
      creationError(aMul, "left", left, "right", right);

   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::Mul::create(self(), result, left, right));
   return result;
   }

Value *
BuilderBase::IndexAt (Type * type, Value * base, Value * index)
   {
   Type *returnType = dict()->producedType(aIndexAt, base, index);
   if (!returnType)
      creationError(aIndexAt, "type", type, "base", base, "index", index);

   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::IndexAt::create(self(), result, type, base, index));
   return result;
   }

Value *
BuilderBase::Load(Symbol *local)
   {
   Type *returnType = local->type();
   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::Load::create(self(), result, local));
   return result;
   }

Value *
BuilderBase::Load (std::string name)
   {
   Symbol *local = _fb->getSymbol(name);
   if (!local)
      creationError(aLoad, "localName", name);
   return Load(local);
   }

Value *
BuilderBase::LoadAt (Type * type, Value * address)
   {
   Type *returnType = dict()->producedType(aLoadAt, address);
   if (!returnType)
      creationError(aLoadAt, "type", type, "address", address);

   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::LoadAt::create(self(), result, type, address));
   return result;
   }

Value *
BuilderBase::LoadField (std::string structName, std::string fieldName, Value * structBase)
   {
   StructType *structType = dict()->LookupStruct(structName);
   FieldType *fieldType = structType->LookupField(fieldName);
   assert(fieldType);
   return LoadField(fieldType, structBase);
   }

Value *
BuilderBase::LoadField (FieldType *fieldType, Value *structBase)
   {
   StructType *structType = fieldType->owningStruct();
   Type *returnType = dict()->producedType(aLoadField, fieldType, structBase);
   if (!returnType)
      creationError(aLoadField, "struct", structType->name(), "field", fieldType->name(), "base", structBase);

   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::LoadField::create(self(), result, fieldType, structBase));
   return result;
   }

Value *
BuilderBase::LoadIndirect (std::string structName, std::string fieldName, Value * pStructBase)
   {
   StructType *structType = dict()->LookupStruct(structName);
   FieldType *fieldType = structType->LookupField(fieldName);
   assert(fieldType);
   return LoadIndirect(fieldType, pStructBase);
   }

Value *
BuilderBase::LoadIndirect (FieldType *fieldType, Value *pStructBase)
   {
   StructType *structType = fieldType->owningStruct();
   Type *returnType = dict()->producedType(aLoadIndirect, fieldType, pStructBase);
   if (!returnType)
      creationError(aLoadIndirect, "struct", structType->name(), "field", fieldType->name(), "basePtr", pStructBase);

   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::LoadIndirect::create(self(), result, fieldType, pStructBase));
   return result;
   }

void
BuilderBase::Store(Symbol * local, Value * value)
   {
   add(OMR::JitBuilder::Store::create(self(), local, value));
   }
   
void
BuilderBase::Store (std::string name, Value * value)
   {
   Symbol *local = fb()->getSymbol(name);
   if (local == NULL)
      {
      fb()->DefineLocal(name, value->type());
      local = fb()->getSymbol(name);
      }
   add(OMR::JitBuilder::Store::create(self(), local, value));
   }

void
BuilderBase::StoreAt (Value * address, Value * value)
   {
   Type *returnType = dict()->producedType(aStoreAt, address, value);
   if (returnType != NoType)
      creationError(aStoreAt, "address", address, "value", value);

   add(OMR::JitBuilder::StoreAt::create(self(), address, value));
   }

void
BuilderBase::StoreField (std::string structName, std::string fieldName, Value * structBase, Value *value)
   {
   StructType *structType = dict()->LookupStruct(structName);
   FieldType *fieldType = structType->LookupField(fieldName);
   assert(fieldType);
   return StoreField(fieldType, structBase, value);
   }

void
BuilderBase::StoreField (FieldType *fieldType, Value *structBase, Value *value)
   {
   StructType *structType = fieldType->owningStruct();
   Type *returnType = dict()->producedType(aStoreField, fieldType, structBase, value);
   if (structType != fieldType->owningStruct() || returnType != NoType)
      creationError(aStoreField, "struct", structType->name(), "field", fieldType->name(), "base", structBase, "value", value);

   add(OMR::JitBuilder::StoreField::create(self(), fieldType, structBase, value));
   }

void
BuilderBase::StoreIndirect (std::string structName, std::string fieldName, Value * pStructBase, Value *value)
   {
   StructType *structType = dict()->LookupStruct(structName);
   FieldType *fieldType = structType->LookupField(fieldName);
   assert(fieldType);
   return StoreIndirect(fieldType, pStructBase, value);
   }

void
BuilderBase::StoreIndirect (FieldType *fieldType, Value *pStructBase, Value *value)
   {
   StructType *structType = fieldType->owningStruct();
   Type *returnType = dict()->producedType(aStoreIndirect, fieldType, pStructBase, value);
   if (structType != fieldType->owningStruct() || returnType != NoType)
      creationError(aStoreIndirect, "struct", structType->name(), "field", fieldType->name(), "basePtr", pStructBase, "value", value);

   add(OMR::JitBuilder::StoreIndirect::create(self(), fieldType, pStructBase, value));
   }

void
BuilderBase::AppendBuilder(Builder * b)
   {
   Operation * appendBuilderOp = OMR::JitBuilder::AppendBuilder::create(self(), b);
   add(appendBuilderOp);
   b->setBoundness(May)
    ->setBound(appendBuilderOp)
    ->setBoundness(Must);
   _controlReachesEnd = true; // AppendBuilder establishes a label so control can now reach the end of this builder even if earlier it could not
   }

Value *
BuilderBase::Call(Value *func, int32_t numArgs, ...)
   {
   assert(func->type()->isFunction());

   va_list args ;
   va_start(args, numArgs);
   FunctionType *function = static_cast<FunctionType *>(func->type());
   Type *returnType = dict()->producedType(function, numArgs, args);
   if (returnType == NULL)
      creationError(aCall, "functionType", func, numArgs, args);
   va_end(args);

   Value * result = NULL;
   if (returnType != NoType)
      result = Value::create(self(), returnType);
   va_list args2 ;
   va_start(args2, numArgs);
   Operation * callOp = OMR::JitBuilder::Call::create(self(), result, func, numArgs, args2);
   add(callOp);
   va_end(args2);
   return result;
   }

Value *
BuilderBase::Call(Value *func, int32_t numArgs, Value **args)
   {
   assert(func->type()->isFunction());
   FunctionType *function = static_cast<FunctionType *>(func->type());
   Type *returnType = dict()->producedType(function, numArgs, args);
   if (returnType == NULL)
      creationError(aCall, "functionType", func, numArgs, args);

   Value * result = NULL;
   if (returnType != NoType)
      result = Value::create(self(), returnType);
   Operation * callOp = OMR::JitBuilder::Call::create(self(), result, func, numArgs, args);
   add(callOp);
   return result;
   }

void
BuilderBase::Goto(Builder * b)
   {
   Operation * gotoOp = OMR::JitBuilder::Goto::create(self(), b);
   add(gotoOp);
   _controlReachesEnd = false; // Goto definitely leaves this builder object
   }

void
BuilderBase::IfCmpGreaterThan(Builder * gtBuilder, Value * left, Value * right)
   {
   Type *returnType = dict()->producedType(aIfCmpGreaterThan, left, right);
   if (!returnType || returnType != NoType)
      creationError(aIfCmpGreaterThan, "left", left, "right", right);
   add(OMR::JitBuilder::IfCmpGreaterThan::create(self(), gtBuilder, left, right));
   gtBuilder->setTarget()
            ->setBoundness(Cant);
   }

void
BuilderBase::IfCmpLessThan(Builder * ltBuilder, Value * left, Value * right)
   {
   Type *returnType = dict()->producedType(aIfCmpLessThan, left, right);
   if (!returnType || returnType != NoType)
      creationError(aIfCmpLessThan, "left", left, "right", right);
   add(OMR::JitBuilder::IfCmpLessThan::create(self(), ltBuilder, left, right));
   ltBuilder->setTarget()
            ->setBoundness(Cant);
   }

void
BuilderBase::IfCmpGreaterOrEqual(Builder * goeBuilder, Value * left, Value * right)
   {
   Type *returnType = dict()->producedType(aIfCmpGreaterOrEqual, left, right);
   if (!returnType || returnType != NoType)
      creationError(aIfCmpGreaterOrEqual, "left", left, "right", right);
   add(OMR::JitBuilder::IfCmpGreaterOrEqual::create(self(), goeBuilder, left, right));
   goeBuilder->setTarget()
             ->setBoundness(Cant);
   }

void
BuilderBase::IfCmpLessOrEqual(Builder * loeBuilder, Value * left, Value * right)
   {
   Type *returnType = dict()->producedType(aIfCmpLessOrEqual, left, right);
   if (!returnType || returnType != NoType)
      creationError(aIfCmpLessOrEqual, "left", left, "right", right);
   add(OMR::JitBuilder::IfCmpLessOrEqual::create(self(), loeBuilder, left, right));
   loeBuilder->setTarget()
             ->setBoundness(Cant);
   }

void
BuilderBase::IfThenElse(Builder * thenB, Builder * elseB, Value * cond)
   {
   if (thenB && thenB->boundness() == Cant)
      creationError(aIfThenElse, "Operation invalid because thenB builder cannot be bound");
   if (elseB && elseB->boundness() == Cant)
      creationError(aIfThenElse, "Operation invalid because elseB builder cannot be bound");

   Operation *ifThenElseOp = OMR::JitBuilder::IfThenElse::create(self(), thenB, elseB, cond);
   add(ifThenElseOp);
   if (thenB)
      thenB->setTarget()
           ->setBoundness(May)
           ->setBound(ifThenElseOp)
           ->setBoundness(Must);
   if (elseB)
      elseB->setTarget()
           ->setBoundness(May)
           ->setBound(ifThenElseOp)
           ->setBoundness(Must);
   }

void
BuilderBase::ForLoopUp(std::string loopVar, Builder * body, Value * initial, Value * end, Value * bump)
   {
   LocalSymbol *loopSym = NULL;
   Symbol *sym = fb()->getSymbol(loopVar);
   if (sym && sym->isLocal())
      loopSym = static_cast<LocalSymbol *>(sym);
   else
      loopSym = fb()->DefineLocal(loopVar, initial->type());
   ForLoopUp(loopSym, body, initial, end, bump);
   }

void
BuilderBase::ForLoopUp(LocalSymbol *loopSym, Builder * body, Value * initial, Value * end, Value * bump)
   {
   Type *returnType = dict()->producedType(aForLoop, initial, end, bump);
   if (!returnType || returnType != NoType)
      creationError(aForLoop, "initial", initial, "end", end, "bump", bump);
   if (body->boundness() == Cant)
      creationError(aIfThenElse, "Operation invalid because body builder cannot be bound");

   Operation * forLoopOp = OMR::JitBuilder::ForLoop::create(self(), true, loopSym, body, initial, end, bump);
   add(forLoopOp);
   }

void
BuilderBase::ForLoop(bool countsUp, std::string loopVar, Builder * loopBody, Builder * loopContinue, Builder * loopBreak, Value * initial, Value * end, Value * bump)
   {
   LocalSymbol *loopSym = NULL;
   Symbol *sym = fb()->getSymbol(loopVar);
   if (sym && sym->isLocal())
      loopSym = static_cast<LocalSymbol *>(sym);
   else
      loopSym = fb()->DefineLocal(loopVar, initial->type());
   ForLoop(countsUp, loopSym, loopBody, loopContinue, loopBreak, initial, end, bump);
   }

void
BuilderBase::ForLoop(bool countsUp, LocalSymbol *loopSym, Builder * loopBody, Builder * loopContinue, Builder * loopBreak, Value * initial, Value * end, Value * bump)
   {
   Type *returnType = dict()->producedType(aForLoop, initial, end, bump);
   if (!returnType || returnType != NoType)
      creationError(aForLoop, "initial", initial, "end", end, "bump", bump);
   if (loopBody->boundness() == Cant)
      creationError(aIfThenElse, "Operation invalid because loopBody builder cannot be bound");
   if (loopContinue && loopContinue->boundness() == Cant)
      creationError(aIfThenElse, "Operation invalid because loopContinue builder cannot be bound");
   if (loopBreak && loopBreak->boundness() == Cant)
      creationError(aIfThenElse, "Operation invalid because loopBreak builder cannot be bound");

   Operation * forLoopOp = (OMR::JitBuilder::ForLoop::create(self(), countsUp, loopSym, loopBody, loopContinue, loopBreak, initial, end, bump));
   add(forLoopOp);
   loopBody->setTarget()
           ->setBoundness(May)
           ->setBound(forLoopOp)
           ->setBoundness(Must);
   if (loopContinue)
      loopContinue->setBoundness(May)
                  ->setBound(forLoopOp)
                  ->setBoundness(Must);
   if (loopBreak)
      loopBreak->setBoundness(May)
               ->setBound(forLoopOp)
               ->setBoundness(Must);
   }

void
BuilderBase::Switch(Value * selector, Builder * defaultBuilder, int numCases, Case ** cases)
   {
   Type *returnType = dict()->producedType(aSwitch, selector);
   if (!returnType || returnType != NoType)
      creationError(aSwitch, "selector", selector);
   Operation *switchOp = OMR::JitBuilder::Switch::create(self(), selector, defaultBuilder, numCases, cases);
   add(switchOp);
   if (defaultBuilder)
      defaultBuilder->setTarget()
                    ->setBoundness(May)
                    ->setBound(switchOp)
                    ->setBoundness(Must);
   for (auto c=0;c < numCases;c++)
      {
      Case *thisCase = cases[c];
      Builder *b = thisCase->builder();
      b->setTarget()
       ->setBoundness(May)
       ->setBound(switchOp)
       ->setBoundness(Must);
      }
   }

void
BuilderBase::Return()
   {
   if (_fb->getReturnType() != NoType)
      creationError(aReturn, "expected type", _fb->getReturnType(), "returned type", NoType);

   add(OMR::JitBuilder::Return::create(self()));
   if (boundness() == Must)
      creationError(aReturn, "Operation invalid because target builder is bound");
   setBoundness(Cant);
   }

void
BuilderBase::Return(Value *v)
   {
   Type *returnType = dict()->producedType(aReturn, v);
   if (v->type() != _fb->getReturnType() || !returnType || returnType != NoType)
      creationError(aReturn, "expected type", _fb->getReturnType(), "returned type", v->type());

   add(OMR::JitBuilder::Return::create(self(), v));
   if (boundness() == Must)
      creationError(aReturn, "Operation invalid because target builder is bound");
   setBoundness(Cant);
   }

Value *
BuilderBase::CreateLocalArray(int32_t numElements, Type *elementType)
   {
   Type *returnType = dict()->PointerTo(elementType);
   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::CreateLocalArray::create(self(), result, numElements, elementType));
   return result;
   }

Value *
BuilderBase::CreateLocalStruct(Type *structType)
   {
   Type *returnType = dict()->PointerTo(structType);
   Value * result = Value::create(self(), returnType);
   add(OMR::JitBuilder::CreateLocalStruct::create(self(), result, structType));
   return result;
   }

Location *
BuilderBase::SourceLocation()
   {
   Location *loc = OMR::JitBuilder::Location::create(_fb);
   _fb->addLocation(loc);
   setLocation(loc);
   return loc;
   }

Location *
BuilderBase::SourceLocation(std::string lineNumber)
   {
   Location *loc = OMR::JitBuilder::Location::create(_fb, lineNumber);
   _fb->addLocation(loc);
   setLocation(loc);
   return loc;
   }

Location *
BuilderBase::SourceLocation(std::string lineNumber, int32_t bcIndex)
   {
   Location *loc = OMR::JitBuilder::Location::create(_fb, lineNumber, bcIndex);
   _fb->addLocation(loc);
   setLocation(loc);
   return loc;
   }

Operation *
BuilderBase::Append(OperationBuilder *opBuilder)
   {
   // ideally do some type checking for the operation to be created

   for (auto i=0;i < opBuilder->numResults();i++)
      opBuilder->addResult(Value::create(self(), opBuilder->resultType(i)));

   Operation *newOp = opBuilder->createOperation(self());
   return newOp;
   }

Value *
BuilderBase::Append(OperationBuilder *opBuilder, LiteralValue *l)
   {
   // ideally do some type checking for the operation to be created

   assert(opBuilder->numResults() == 1);
   Value *returnValue = Value::create(self(), opBuilder->resultType());
   opBuilder->addResult(returnValue);
   opBuilder->addLiteral(l);

   Operation *newOp = opBuilder->createOperation(self());
   add(newOp);
   return returnValue;
   }

Value *
BuilderBase::Append(OperationBuilder *opBuilder, Value *v)
   {
   // ideally do some type checking for the operation to be created

   assert(opBuilder->numResults() == 1);
   Value *returnValue = Value::create(self(), opBuilder->resultType());
   opBuilder->addResult(returnValue);
   opBuilder->addOperand(v);

   Operation *newOp = opBuilder->createOperation(self());
   add(newOp);
   return returnValue;
   }

Value *
BuilderBase::Append(OperationBuilder *opBuilder, Value *left, Value *right)
   {
   // ideally do some type checking for the operation to be created

   assert(opBuilder->numResults() == 1);
   Value *returnValue = Value::create(self(), opBuilder->resultType());
   opBuilder->addResult(returnValue);
   opBuilder->addOperand(left);
   opBuilder->addOperand(right);

   Operation *newOp = opBuilder->createOperation(self());
   add(newOp);
   return returnValue;
   }

Builder *
BuilderBase::add(Operation * op)
   {
   _fb->registerObject(op);
   op->setLocation(_currentLocation);
   TextWriter *log = _fb->logger();
   if (_fb->config()->traceBuildIL() && log)
      {
      log->indent() << self() << " : create ";
      log->print(op);
      }

   _operations.push_back(op);
   return self();
   }
