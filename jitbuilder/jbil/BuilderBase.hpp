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

#ifndef BUILDERBASE_INCL
#define BUILDERBASE_INCL

#include <vector>

namespace OMR
{
namespace JitBuilder
{ 

class Operation;
typedef std::vector<Operation *> OperationVector;
typedef OperationVector::iterator OperationIterator;

} // namespace JitBuilder
} // namespace OMR


#include <stdint.h>
#include <vector>

#include "Object.hpp"
#include "TypeDictionary.hpp"


namespace OMR
{

namespace JitBuilder
{

class Builder;
class Case;
class FunctionBuilder;
class Location;
class Operation;
class OperationCloner;
class Type;
class Value;
class Transformer;

enum MustMayCant {
   Must=0,
   May=1,
   Cant=2
};

class BuilderBase : public Object
   {
   friend class Transformer;

public:

   Operation * appendClone(Operation *op);
   Operation * appendClone(Operation *op, OperationCloner *cloner);

   Value * ConstInt8(int8_t v);
   Value * ConstInt16(int16_t v);
   Value * ConstInt32(int32_t v);
   Value * ConstInt64(int64_t v);
   Value * ConstFloat(float v);
   Value * ConstDouble(double v);
   Value * ConstAddress(void *v);

   Value * CoercePointer(Type * t, Value * v);

   Value * Add(Value * left, Value * right);
   Value * Sub(Value * left, Value * right);
   Value * Mul(Value * left, Value * right);

   Value * IndexAt(Type * type, Value * base, Value * index);
   Value * Load(std::string name);
   Value * Load(Symbol *local);
   Value * LoadAt(Type * type, Value * address);
   Value * LoadField(std::string structName, std::string fieldName, Value * structBase);
   Value * LoadField(FieldType *fieldType, Value * structBase);
   Value * LoadIndirect(std::string structName, std::string fieldName, Value * pStructBase);
   Value * LoadIndirect(FieldType *fieldType, Value * pStructBase);
   void Store(std::string name, Value * value);
   void Store(Symbol * local, Value * value);
   void StoreAt(Value * address, Value * value);
   void StoreField(std::string structName, std::string fieldName, Value * structBase, Value *value);
   void StoreField(FieldType *fieldType, Value * structBase, Value *value);
   void StoreIndirect(std::string structName, std::string fieldName, Value * pStructBase, Value *value);
   void StoreIndirect(FieldType *fieldType, Value * pStructBase, Value *value);

   void AppendBuilder(Builder * b);
   Value * Call(Value *function, int32_t numArgs, ...);
   Value * Call(Value *function, int32_t numArgs, Value **args);
   void Goto(Builder * b);
   void IfCmpGreaterThan(Builder * gtBuilder, Value * left, Value * right);
   void IfCmpLessThan(Builder * ltBuilder, Value * left, Value * right);
   void IfCmpGreaterOrEqual(Builder * goeBuilder, Value * left, Value * right);
   void IfCmpLessOrEqual(Builder * loeBuilder, Value * left, Value * right);
   void IfThenElse(Builder * thenB, Value * cond);
   void IfThenElse(Builder * thenB, Builder * elseB, Value * cond);
   void ForLoopUp(std::string loopVar, Builder * body, Value * initial, Value * end, Value * bump);
   void ForLoopUp(LocalSymbol *loopSym, Builder * body, Value * initial, Value * end, Value * bump);
   void ForLoop(bool countsUp, std::string loopVar, Builder * body, Builder * loopContinue, Builder * loopBreak, Value * initial, Value * end, Value * bump);
   void ForLoop(bool countsUp, LocalSymbol *loopSym, Builder * body, Builder * loopContinue, Builder * loopBreak, Value * initial, Value * end, Value * bump);
   void Switch(Value * selector, Builder * defaultBuilder, int numCases, Case ** cases);
   void Return();
   void Return(Value *v);

   Value * CreateLocalArray(int32_t numElements, Type *elementType);
   Value * CreateLocalStruct(Type *elementType);

   Location * SourceLocation();
   Location * SourceLocation(std::string lineNumber);
   Location * SourceLocation(std::string lineNumber, int32_t bcIndex);

   Type * NoType;
   Type * Int8;
   Type * Int16;
   Type * Int32;
   Type * Int64;
   Type * Float;
   Type * Double;
   Type * Address;

   int64_t id() const                          { return _id; }
   std::string name() const                    { return _name; }
   virtual size_t size() const                 { return sizeof(BuilderBase); }

   TypeDictionary * dict() const;
   Builder * parent() const                    { return _parent; }

   int32_t numChildren() const                 { return _children.size(); }
   BuilderIterator ChildrenBegin()             { return BuilderIterator(_children); }
   BuilderIterator ChildrenEnd()               { return BuilderIterator(); }

   int32_t numOperations() const               { return _operations.size(); }
   OperationVector & operations()              { return _operations; }
   OperationIterator OperationsBegin()         { return _operations.begin(); }
   OperationIterator OperationsEnd()           { return _operations.end(); }

   MustMayCant boundness() const               { return _boundness; }
   Builder * setBoundness(MustMayCant v);
   void checkBoundness(bool v) const;

   bool isBound() const                        { return _isBound; }
   Builder * setBound(bool v, Operation * boundToOp=NULL);
   Builder * setBound(Operation * boundToOp);
   Operation * boundToOperation()              { assert(_isBound); return _boundToOperation; }

   bool isTarget() const                       { return _isTarget; }
   Builder * setTarget(bool v=true);

   bool controlReachesEnd() const              { return _controlReachesEnd; }

   static int64_t maxIndex()                   { return globalIndex; }


   protected:
   BuilderBase(Builder * parent, FunctionBuilder * fb, TypeDictionary *types);
   BuilderBase(Builder * parent, TypeDictionary *types);

   Builder *self();

   void creationError(Action a, std::string msg);
   void creationError(Action a, std::string sName, std::string s);
   void creationError(Action a, std::string vName, Value * v);
   void creationError(Action a, std::string tName, Type * t, std::string vName, Value * v);
   void creationError(Action a, std::string t1Name, Type * t1, std::string t2Name, Type * t2);
   void creationError(Action a, std::string lName, Value * left, std::string rName, Value * right);
   void creationError(Action a, std::string oneName, Value * one, std::string twoName, Value * two, std::string threeName, Value * three);
   void creationError(Action a, std::string tName, Type * t, std::string firstName, Value * first, std::string secondName, Value * second);
   void creationError(Action a, std::string sName, std::string sValue, std::string fName, std::string fValue, std::string bName, Value *bValue);
   void creationError(Action a, std::string sName, std::string sValue, std::string fName, std::string fValue, std::string bName, Value *bValue, std::string vName, Value *vValue);
   void creationError(Action a, std::string fName, Value *function, int32_t numArgs, va_list args);
   void creationError(Action a, std::string fName, Value *function, int32_t numArgs, Value **args);

   void setParent(Builder *parent);
   void addChild(Builder *child);
   Builder * add(Operation * op);
   Location *location() const
      {
      return _currentLocation;
      }
   void setLocation(Location * loc)
      {
      _currentLocation = loc;
      }

   int64_t                    _id;
   std::string                _name;
   Builder                  * _parent;
   std::vector<Builder *>     _children;
   Builder                  * _successor;
   OperationVector            _operations;
   Location                 * _currentLocation;
   Operation                * _boundToOperation;
   bool                       _isTarget;
   bool                       _isBound;
   bool                       _controlReachesEnd;
   MustMayCant                _boundness;
   static int64_t             globalIndex;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(BUILDERBASE_INCL)
