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

#include <cstring>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "FunctionBuilder.hpp"
#include "Debugger.hpp"
#include "OperationCloner.hpp"
#include "Symbol.hpp"
#include "Type.hpp"
#include "TypeReplacer.hpp"
#include "Value.hpp"

namespace OMR
{

namespace JitBuilder
{


// Support for debugging at JBIL level

// DebugValue is used to record concrete values and locals computed by the debugged functions
// The possible values that can be stored here are determined at runtime per FunctionBuilder based
// on the variety of Types that are manipulated by the FunctionBuilder. This type as defined is just
// a prefix for the actual type: the payload of the type is determined dynamically per FunctionBuilder.
// Allocating and indexing into arrays of debug values will use the size of the DebugDictionary's
// _DebugValue type which takes into account all these Types. Because it's a union, the address of
// the _firstValueData field is used as the offset where any debug value is stored, regardless of its
// Type.
class DebugValue
   {
public:
   Type *_type;
   // conceptually followed by:
   // union {
   //   one field per type defined in the FunctionBuilder's dictionary
   // };

   // dummy field: not used by anything
   uintptr_t _firstValueData;
   // to allocate space for a DebugValue, you have to ask a specific FunctionBuilder's DebugDictionary for the size of its _DebugValue type
   // Note that different FunctionBuilders have different corresponding _DebugValue types.
   };

class FunctionDebugInfo;

// This structure represents the Debugger state for each activation on a thread stack.
// Because the generated code accesses fields of this struct, we consciously use a
// 'struct' rather than a class and it should never contain any virtual functions or
// be subclassed..
struct DebuggerFrame
   {
   FunctionDebugInfo * _info;
   Debugger *_debugger;
   DebugValue *_locals; // array of values, but size of each determined by _DebugValue->size()
   DebugValue *_values; // array of values, but size of each determined by _DebugValue->size()
   DebugValue *_returnValues; // array of values, but size of each determined by _DebugValule->size()
   Builder * _fromBuilder;
   bool      _returning;
   Builder * _builderToDebug;

   std::map<uint64_t, OperationIterator> _builderReentryPoints;
   std::list<struct Breakpoint *> _breakpoints;

   DebugValue *getValueInArray(uint8_t *p, uint64_t idx);
   DebugValue *getValue(uint64_t idx);
   DebugValue *getLocal(uint64_t idx);
   };

class DebugDictionary : public TypeDictionary
   {
   public:
   DebugDictionary(FunctionBuilder *fbToDebug);
   DebugDictionary(FunctionBuilder *fbToDebug, DebugDictionary *baseDict);

   StructType *_DebugValue;
   PointerType *_pDebugValue;
   FieldType *_DebugValue_type;
   std::map<Type *, FieldType *> _DebugValue_fields;

   StructType *_DebugFrame;
   PointerType *_pDebugFrame;
   FieldType *_DebugFrame_info;
   FieldType *_DebugFrame_debugger;
   FieldType *_DebugFrame_locals;
   FieldType *_DebugFrame_values;
   FieldType *_DebugFrame_returnValues;
   FieldType *_DebugFrame_fromBuilder;
   FieldType *_DebugFrame_returning;
   FieldType *_DebugFrame_builderToDebug;

   protected:
   void createTypes(FunctionBuilder *fbToDbug);
   void initTypes(DebugDictionary *baseDict);
   };

class DebuggerFunctionBuilder : public FunctionBuilder
   {
   public:

   DebuggerFunctionBuilder(Debugger *dbgr, FunctionBuilder *fbToDebug);
   DebuggerFunctionBuilder(Debugger *dbgr, FunctionBuilder *fbToDebug, DebugDictionary *types);

   protected:
   DebugDictionary *dbgDict() { return static_cast<DebugDictionary *>(_types); }

   void initialize(DebugDictionary *types);

   void storeValue(Builder *b, Symbol *local, Value *value);
   void storeValue(Builder *b, Value *debugvalue, Value *value);
   void storeReturnValue(Builder *b, int32_t resultIdx, Value *value);
   Value *loadValue(Builder *b, Symbol *local);
   Value *loadValue(Builder *b, Value *value);
   void storeToDebugValue(Builder *b, Value *debugValue, Value *value);
   Value *loadFromDebugValue(Builder *b, Value *debugValue, Type *type);

   FieldType *lookupTypeField(Type *type);

   void transferToBoundBuilder(Builder *b, Builder *bound);
   void transferToUnboundBuilder(Builder *b, Builder *target);

   Debugger *_debugger;
   StructType *_DebugValue;
   PointerType *_pDebugValue;
   FieldType *_DebugValue_type;
   std::map<Type *, FieldType *> *_DebugValue_fields;

   StructType *_DebugFrame;
   PointerType *_pDebugFrame;
   FieldType *_DebugFrame_info;
   FieldType *_DebugFrame_debugger;
   FieldType *_DebugFrame_locals;
   FieldType *_DebugFrame_values;
   FieldType *_DebugFrame_returnValues;
   FieldType *_DebugFrame_fromBuilder;
   FieldType *_DebugFrame_returning;
   FieldType *_DebugFrame_builderToDebug;
   };

DebuggerFunctionBuilder::DebuggerFunctionBuilder(Debugger *dbgr, FunctionBuilder *fbToDebug)
   : FunctionBuilder(new DebugDictionary(fbToDebug, dbgr->getDictionary(fbToDebug)))
   , _debugger(dbgr)
   {
   initialize(dbgDict());
   _config = *(fbToDebug->config());
   }

DebuggerFunctionBuilder::DebuggerFunctionBuilder(Debugger *dbgr, FunctionBuilder *fbToDebug, DebugDictionary *types)
   : FunctionBuilder(types)
   , _debugger(dbgr)
   {
   initialize(types);
   _config = *fbToDebug->config();
   }

void
DebuggerFunctionBuilder::initialize(DebugDictionary *types)
   {
   _DebugValue = types->_DebugValue;
   _pDebugValue = types->_pDebugValue;
   _DebugValue_type = types->_DebugValue_type;
   _DebugValue_fields = &types->_DebugValue_fields;

   _DebugFrame = types->_DebugFrame;
   _pDebugFrame = types->_pDebugFrame;
   _DebugFrame_info = types->_DebugFrame_info;
   _DebugFrame_debugger = types->_DebugFrame_debugger;
   _DebugFrame_locals = types->_DebugFrame_locals;
   _DebugFrame_values = types->_DebugFrame_values;
   _DebugFrame_returnValues = types->_DebugFrame_returnValues;
   _DebugFrame_fromBuilder = types->_DebugFrame_fromBuilder;
   _DebugFrame_returning = types->_DebugFrame_returning;
   _DebugFrame_builderToDebug = types->_DebugFrame_builderToDebug;
   }

void
DebuggerFunctionBuilder::storeValue(Builder *b, Symbol *local, Value *value)
   {
   storeToDebugValue(b, b->IndexAt(_pDebugValue, b->Load("locals"), b->ConstInt64(_debugger->index(local))), value);
   }

void
DebuggerFunctionBuilder::storeValue(Builder *b, Value *destValue, Value *value)
   {
   storeToDebugValue(b, b->IndexAt(_pDebugValue, b->Load("values"), b->ConstInt64(_debugger->index(destValue))), value);
   }

void
DebuggerFunctionBuilder::storeReturnValue(Builder *b, int32_t resultIdx, Value *value)
   {
   storeToDebugValue(b, b->IndexAt(_pDebugValue, b->LoadIndirect(_DebugFrame_returnValues, b->Load("frame")), b->ConstInt64(resultIdx)), value);
   }

Value *
DebuggerFunctionBuilder::loadValue(Builder *b, Symbol *local)
   {
   return loadFromDebugValue(b, b->IndexAt(_pDebugValue, b->Load("locals"), b->ConstInt64(_debugger->index(local))), local->type());
   }

Value *
DebuggerFunctionBuilder::loadValue(Builder *b, Value *value)
   {
   return loadFromDebugValue(b, b->IndexAt(_pDebugValue, b->Load("values"), b->ConstInt64(_debugger->index(value))), value->type());
   }

FieldType *
DebuggerFunctionBuilder::lookupTypeField(Type *type)
   {
   auto it = _DebugValue_fields->find(type);
   assert(it != _DebugValue_fields->end());
   FieldType *typeField = it->second;
   return typeField;
   }

void
DebuggerFunctionBuilder::storeToDebugValue(Builder *b, Value *debugValue, Value *value)
   {
   Type *type = value->type();
   b->StoreIndirect(_DebugValue_type, debugValue, b->ConstInt64((int64_t)type));
   b->StoreIndirect(lookupTypeField(type), debugValue, value);
   }

Value *
DebuggerFunctionBuilder::loadFromDebugValue(Builder *b, Value *debugValueBase, Type *type)
   {
   assert(debugValueBase->type() == _pDebugValue);
   return b->LoadIndirect(lookupTypeField(type), debugValueBase);
   }

DebugDictionary::DebugDictionary(FunctionBuilder *fbToDebug)
   : TypeDictionary(fbToDebug->dict()->name() + "_DBG", fbToDebug->dict())
   {
   createTypes(fbToDebug);
   }

DebugDictionary::DebugDictionary(FunctionBuilder *fbToDebug, DebugDictionary *baseDict)
   : TypeDictionary(fbToDebug->dict()->name() + "_DBG", baseDict)
   {
   //createTypes(fbToDebug);
   initTypes(baseDict);
   }

void
DebugDictionary::createTypes(FunctionBuilder *fbToDebug)
   {
   TypeDictionary *tdToDebug = fbToDebug->dict();
   size_t sizeDebugValue = 0;
   for (auto typeIt = tdToDebug->TypesBegin(); typeIt != tdToDebug->TypesEnd(); typeIt++)
      {
      Type *type = *typeIt;
      size_t size = type->size();
      if (sizeDebugValue < size)
         sizeDebugValue = size;
      }
   sizeDebugValue = sizeof(DebugValue) - sizeof(uintptr_t) + sizeDebugValue / 8;

   _DebugValue = DefineStruct("DebugValue", sizeDebugValue);
   _DebugValue_type = DefineField(_DebugValue, LiteralValue::create(this, "_type"),  Int64, 8*offsetof(DebugValue, _type));
   for (auto typeIt = tdToDebug->TypesBegin(); typeIt != tdToDebug->TypesEnd(); typeIt++)
      {
      Type *type = *typeIt;
      if (type->size() > 0 && !type->isField())
         {
         Type *myType = LookupType(type->id());
         // special typeString Literal will be handled correctly by TypeReplacer
         // otherwise user defined types may not be handled properly
         LiteralValue *typeName = LiteralValue::create(this, myType);
         _DebugValue_fields.insert({type, DefineField(_DebugValue, typeName, myType, 8*offsetof(DebugValue, _firstValueData))});
         }
      }
   CloseStruct(_DebugValue);
   _pDebugValue = PointerTo(_DebugValue);

   _DebugFrame = DefineStruct("DebugFrame", 8*sizeof(struct DebuggerFrame));
   _DebugFrame_info = DefineField(_DebugFrame, "_info", Address, 8*offsetof(DebuggerFrame, _info));
   _DebugFrame_debugger = DefineField(_DebugFrame, "_debugger", Address, 8*offsetof(DebuggerFrame, _debugger));
   _DebugFrame_locals = DefineField(_DebugFrame, "_locals", _pDebugValue, 8*offsetof(DebuggerFrame, _locals));
   _DebugFrame_values = DefineField(_DebugFrame, "_values", _pDebugValue, 8*offsetof(DebuggerFrame, _values));
   _DebugFrame_returnValues = DefineField(_DebugFrame, "_returnValues", _pDebugValue, 8*offsetof(DebuggerFrame, _returnValues));
   _DebugFrame_fromBuilder = DefineField(_DebugFrame, "_fromBuilder", Address, 8*offsetof(DebuggerFrame, _fromBuilder));
   _DebugFrame_returning = DefineField(_DebugFrame, "_returning", Address, 8*offsetof(DebuggerFrame, _returning));
   _DebugFrame_builderToDebug = DefineField(_DebugFrame, "_builderToDebug", Address, 8*offsetof(DebuggerFrame, _builderToDebug));
   CloseStruct(_DebugFrame);
   _pDebugFrame = PointerTo(_DebugFrame);
   }

void
DebugDictionary::initTypes(DebugDictionary *baseDict)
   {
   _DebugValue =                baseDict->_DebugValue;
   _DebugValue_type =           baseDict->_DebugValue_type;
   _DebugValue_fields =         baseDict->_DebugValue_fields;
   _pDebugValue =               baseDict->_pDebugValue;
   _DebugFrame =                baseDict->_DebugFrame;
   _DebugFrame_info =           baseDict->_DebugFrame_info;
   _DebugFrame_debugger =       baseDict->_DebugFrame_debugger;
   _DebugFrame_locals =         baseDict->_DebugFrame_locals;
   _DebugFrame_values =         baseDict->_DebugFrame_values;
   _DebugFrame_returnValues =   baseDict->_DebugFrame_returnValues;
   _DebugFrame_fromBuilder =    baseDict->_DebugFrame_fromBuilder;
   _DebugFrame_returning =      baseDict->_DebugFrame_returning;
   _DebugFrame_builderToDebug = baseDict->_DebugFrame_builderToDebug;
   _pDebugFrame =               baseDict->_pDebugFrame;
   }

typedef bool (OperationDebuggerFunc)(DebuggerFrame *, int64_t);
class OperationDebugger : public DebuggerFunctionBuilder
   {
   public:
   OperationDebugger(Debugger *dbgr, Operation *op);

   virtual bool buildIL();
   virtual bool debug(DebuggerFrame *frame, Operation *op);

   protected:
   virtual Operation *cloneOperationForDebug(Builder *b, OperationCloner *cloner);
   void setDebuggerBuilderTarget(Builder *b, Builder *targetBuilder);
   std::string valueName(Value *v);
   void handleLocalsAndValuesIncoming(Builder *b);
   void handleLocalsOutgoing(Builder *b);

   void copyResult(DebugValue *dest, DebugValue *src);

   Operation  *_op;
   std::string _frameName;
   std::string _dbgrName;
   std::string _localsName;
   std::string _valuesName;
   std::string _fromBuilderID;
   };

// FunctionDebugInfo holds debugger information corresponding to a FunctionBuilder
// (information that can be shared across multiple DebuggerFrames). Since there
// are read/write fields in this class, synchronization will be required if
// multiple threads access one of these objects. Alternatively, debuggers for all
// Operations in a FunctionBuilder could be generated ahead of time, at which point
// this structure would become read-only.
class FunctionDebugInfo
   {
public:
   FunctionDebugInfo(FunctionBuilder *fb)
      : _fb(fb)
      , _dbgDict(fb)
      , _valueSizeInBytes(_dbgDict._DebugValue->size()/8)
      { }
   FunctionBuilder * _fb;
   DebugDictionary _dbgDict;
   size_t _valueSizeInBytes;
   std::map<int64_t, OperationDebugger *> _operationDebugBuilders;
   std::map<int64_t, OperationDebuggerFunc *> _operationDebuggers;
   std::map<int64_t, bool> _debugOperations;
   };


// new code below
OperationDebugger::OperationDebugger(Debugger *dbgr, Operation *op)
   : DebuggerFunctionBuilder(dbgr, op->fb())
   , _frameName("frame")
   , _dbgrName("debugger")
   , _localsName("locals")
   , _valuesName("values")
   , _fromBuilderID("fromBuilderID")
   , _op(op)
   {
   DefineName(op->fb()->name() + std::string(".op") + std::to_string(op->id()));
   DefineFile("OpDbgr");
   DefineLine(actionName(op->action()));
   DefineParameter(_frameName, dbgDict()->_pDebugFrame);
   DefineParameter(_fromBuilderID, Int32);
   DefineReturnType(Int8);
   }

bool
OperationDebugger::debug(DebuggerFrame *frame, Operation *op)
   {
   return frame->_info->_operationDebuggers[op->id()](frame, frame->_fromBuilder->id());
   }

Operation *
OperationDebugger::cloneOperationForDebug(Builder *b, OperationCloner *cloner)
   {
   // clone the original operation, replacing operands and builder objects as
   // created above
   return b->appendClone(_op, cloner);
   }

void
OperationDebugger::setDebuggerBuilderTarget(Builder *b, Builder *targetBuilder)
   {

   }

void
OperationDebugger::copyResult(DebugValue *dest, DebugValue *src)
   {
   memcpy(dest, src, dbgDict()->_DebugValue->size());
   }

std::string
OperationDebugger::valueName(Value *v)
   {
   return std::string("#_v") + std::to_string(v->id());
   }

// locals are stored in DebugValues in the debugger frame, but operations that access
// locals will do so by their name encoded in Symbol (Local, Parameter, Function).
// At entry, copy the debug values for any string typed LiteralValues that match the
// names of symbols into actual locals for this function with the string name. That way,
// any operation that accesses the local value will be able to load it directly.
// Similarly, values (operands) are stored ni DebugValues in debugger frame. To ensure
// that any code path can access those values safely, we load these into local variables
// at the beginning of the function, That way, whereever the operands are used in the
// generated code for the operation, the operands can be loaded safely from these locals.
// Otherwise, there can be cases where the operand values are actually loaded only on
// some paths (like the entry path) but e.g. not when control is coming back from some
// bound builder.
void
OperationDebugger::handleLocalsAndValuesIncoming(Builder *b)
   {
   for (auto sIt = _op->SymbolsBegin(); sIt != _op->SymbolsEnd(); sIt++)
      {
      Symbol *sym = *sIt;
      b->Store(sym->name(), loadValue(b, sym));
      }
   }

// locals are stored directly in the frame, but other operations will need to access
// their values as DebugValues from the debugger frame. On any outgoing path, then,
// we store the values for any locals into their corresponding DebugValue in the frame.
// Must be called on any outgoing path from the operation, to ensure the local value is
// available to other operation debuggers.
void
OperationDebugger::handleLocalsOutgoing(Builder *b)
   {
   for (auto sIt = _op->SymbolsBegin(); sIt != _op->SymbolsEnd(); sIt++)
      {
      Symbol *sym = *sIt;
      storeValue(b, sym, b->Load(sym->name()));
      }
   }

bool
OperationDebugger::buildIL()
   {
   Value *frame = Load(_frameName);
   Store(_dbgrName, LoadIndirect(_DebugFrame_debugger, frame));
   Store(_localsName, LoadIndirect(_DebugFrame_locals, frame));
   Store(_valuesName, LoadIndirect(_DebugFrame_values, frame));

   handleLocalsAndValuesIncoming(this);

   OperationCloner cloner(_op);

   // convert operands v of this operation to load their values from the DebugValues in the frame
   // operands *must* be loaded here so they reach all paths
   if (_op->numOperands() > 0)
      {
      int32_t opNum=0;
      for (auto opIt = _op->OperandsBegin(); opIt != _op->OperandsEnd(); opIt++)
         {
         Value *origOperand = *opIt;
         cloner.changeOperand(loadValue(this, origOperand), opNum++);
         //cloner.changeOperand(firstEntry->Load(valueName(origOperand)), opNum++);
         }
      }


   // generate switch based on incoming builder id
   //    each case jumps to the END of the corresponding builder used in the operation (builder B for each builder operand)
   // generate the operation Op_dbg, but the builder operands:
   //    contain two builders, A and B
   //        A contains the code to redirect the debugger control to the builder operand of the actual operation being debugged (passed as a parameter)
   //        B is just a label builder that is appended to the end of A. It is used to transfer control back from the corresponding builder of the
   //            operation being debugged via the switch above
   // Operation class must be able to generate Op_dbg via a virtual call
   //   createActionDebugger()

   // allocate builder objects to handle debugger transition to each of the
   //  the builders referenced by this operation
   // Each builder writes the target builder (from the original operation) into the
   //  debugger object and then returns true to the debugger, indicating that this handler
   //  has been "suspended" so that control can flow to debugger->_builderToDebug

   Builder *orphanTargets = NULL;
   int32_t numBoundBuilders = 0;
   if (_op->numBuilders() > 0)
      {
      orphanTargets = OrphanBuilder();
      for (int32_t bIdx = 0; bIdx < _op->numBuilders(); bIdx++)
         {
         Builder *op_b = _op->builder(bIdx);
         Builder *b = OrphanBuilder();
         cloner.changeBuilder(b, bIdx);
         handleLocalsOutgoing(b);
         b->StoreIndirect(_DebugFrame_builderToDebug, b->Load(_frameName), b->ConstAddress(op_b));
         b->Return(b->ConstInt8(1));
         

         if (op_b->isBound() && _op == op_b->boundToOperation())
            numBoundBuilders++;
         else
            orphanTargets->AppendBuilder(b);
         }
      }

   // no need to change Types or LiteralValues because the action debugger borrowed Types from original's
   // TypeDictionary and LiteralValues don't require translation

   // need to change Symbols from those in the original FunctionBuiilder to those in this OperationDebugger
   if (_op->numSymbols() > 0)
      {
      for (auto s=0;s < _op->numSymbols();s++)
         {
         Symbol *origSymbol = _op->symbol(s);
         Symbol *debugOpSymbol = _fb->getSymbol(origSymbol->name());
         cloner.changeSymbol(debugOpSymbol, s);
         }
      }

   // For each bound builder, this action debugger could also be called for
   //  the control flow path that comes from that builder. We set up an initial switch
   //  to direct control flow to the appropriate incoming control flow path in the
   //  operation. If control is flowing into this operation for the first time,
   //  control will flow to the cloned operation (see below). But if control is
   //  flowing from one of the bound/targeted builders, we set up a switch statement
   //  to direct control to the appropriate path in the cloned operation. We create
   //  a new builder object and append it to each corresponding builder handler after
   //  the Return. Control will flow from this new builder object to where ever the
   //  operation dictates. One could consider the Return in the builder handler to be
   //  "saving" the state of this operation, and the Switch is used to "restart" this
   //  operation at the point it left.
   Builder **caseBuilders = NULL;
   Case **cases = NULL;
   int32_t numCases = numBoundBuilders;
   if (numBoundBuilders > 0)
      {
      caseBuilders = new Builder *[numCases];
      cases = new Case *[numCases];
      int32_t caseIdx = 0;

      for (int32_t bIdx=0;bIdx < _op->numBuilders();bIdx++)
         {
         Builder *builder = _op->builder(bIdx);
         if (builder->isBound() && _op == builder->boundToOperation())
            {
            Builder *restartTarget = OrphanBuilder();
            cloner.builder(bIdx)->AppendBuilder(restartTarget);
   
            caseBuilders[caseIdx] = OrphanBuilder();
            caseBuilders[caseIdx]->Goto(restartTarget);
            cases[caseIdx] = Case::create(builder->id(), caseBuilders[caseIdx], false);
            caseIdx++;
            }
         }
         assert(caseIdx == numCases);
      }

   Builder *firstEntry = OrphanBuilder();
   if (numCases > 0)
      {
      // safer would be to add parent builder ID to the set of cases and have a
      // default case that throws some kind of debug error; for now default is firstEntry
      Switch(Load(_fromBuilderID), firstEntry, numCases, cases);
      }
   else
      AppendBuilder(firstEntry);

   if (_op->action() == aReturn)
      {
      // for Return, just copy any operands to the frame's return values
      for (int32_t oIdx=0;oIdx < _op->numOperands();oIdx++)
         {
         storeReturnValue(firstEntry, oIdx, cloner.operand(oIdx));
         }
      }
   else
      {
      Operation *cloneOp = cloneOperationForDebug(firstEntry, &cloner);

      // store any results produced by the cloned operation to the appropriate DebugValues
      // (the result values produced by the original operation)
      assert(cloneOp->numResults() == _op->numResults());
      auto cloneIt = cloneOp->ResultsBegin();
      for (auto rIt = _op->ResultsBegin(); rIt != _op->ResultsEnd(); rIt++, cloneIt++)
         {
         Value *result = *rIt;
         Value *cloneResult = *cloneIt;
         storeValue(firstEntry, result, cloneResult);
         }

      handleLocalsOutgoing(this);
      }

   if (orphanTargets)
      {
      // have to put orphan builders some place
      Builder *merge = OrphanBuilder();
      Goto(merge);
      AppendBuilder(orphanTargets);
      AppendBuilder(merge);
      }

   Return(ConstInt8(0));

   return true;
   }



Debugger::Debugger(FunctionBuilder *fb)
   : Object(fb)
   , _writer(new TextWriter(fb, std::cout, "  "))
   , _time(0)
   , _frame(NULL)
   , _firstEntry(false)
   {
   FunctionDebugInfo *info = _functionDebugInfos[fb->id()];
   if (!info)
      _functionDebugInfos[fb->id()] = info = new FunctionDebugInfo(fb);

   // transform types now because everything else will need them (do it here so only need to do once)
   if (fb->config()->hasReducer())
      static_cast<TypeReplacer *>(fb->config()->reducer())->transformTypes(&info->_dbgDict);
   }

void
Debugger::recordReentryPoint(Builder *b, OperationIterator & opIt)
   {
   _frame->_builderReentryPoints.insert({b->id(), opIt});
   }

OperationIterator *
Debugger::fetchReentryPoint(Builder *b)
  {
   auto reentryPointIt = _frame->_builderReentryPoints.find(b->id());
   if (reentryPointIt == _frame->_builderReentryPoints.end())
      return NULL;
   return &(reentryPointIt->second);
   }
void
Debugger::removeReentryPoint(Builder *b)
   {
   _frame->_builderReentryPoints.erase(b->id());
   }

void
Debugger::printDebugValue(DebugValue *val)
   {
   if (val->_type)
      val->_type->printValue(_writer, &(val->_firstValueData));
   else
      {
      TextWriter &w = (*_writer);
      w << "Undefined";
      }
   }

DebugValue *
DebuggerFrame::getValueInArray(uint8_t *base, uint64_t idx)
   {
   uint8_t *valueBase = base + idx * _info->_valueSizeInBytes;
   return reinterpret_cast<DebugValue *>(valueBase);
   }

DebugValue *
DebuggerFrame::getValue(uint64_t idx)
   {
   uint8_t *p = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(_values));
   return getValueInArray(p, idx);
   }

DebugValue *
DebuggerFrame::getLocal(uint64_t idx)
   {
   uint8_t *p = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(_locals));
   return getValueInArray(p, idx);
   }

void
Debugger::printValue(uint64_t idx)
   {
   TextWriter &w = (*_writer);
   DebugValue *val = _frame->getValue(idx);
   w << "v" << idx << ": ";
   printDebugValue(val);
   w << " ]" << w.endl();
   }

void
Debugger::printTypeName(Type *type)
   {
   TextWriter &w = (*_writer);
   w << "t" << type->id() << " : [ " << type->name() << " ]";
   }

void
Debugger::printType(Type * type)
   {
   TextWriter &w = (*_writer);
   printTypeName(type);
   w << w.endl();
   }

void
Debugger::printSymbol(std::string name)
   {
   TextWriter &w = (*_writer);
   Symbol *sym = _fb->getSymbol(name);
   DebugValue *val = _frame->getLocal(index(sym));
   w << sym->name() << " : ";
   printDebugValue(val);
   w << w.endl();
   }

struct Breakpoint
   {
   Breakpoint()
      : _id(globalID++)
      , _enabled(true)
      , _silent(false)
      , _count(0)
      { }
   virtual bool breakBefore(Operation *op) { return false; }
   virtual bool breakBefore(Builder *b)    { return false; }
   virtual bool breakAfter(Operation *op)  { return false; }
   virtual bool breakAfter(Builder *b)     { return false; }
   virtual bool breakAt(uint64_t time)     { return false; }
   virtual void print(TextWriter *writer);
   virtual bool fire()
      {
      if (_count > 0)
         _count--;
      if (_count == 0)
         _enabled = true;
      return _enabled;
      }
   bool removeAfterFiring()                                 { return _removeAfterFiring; }
   bool silent()                                            { return _silent; }

   Breakpoint * setRemoveAfterFiring(bool r=true)           { _removeAfterFiring = r; return this;}
   Breakpoint * setIgnoreCount(int64_t c)                   { if (c >= 0) _count = c; return this;}
   Breakpoint * setEnabled(bool e=true)                     { _enabled = e; return this; }
   Breakpoint * setSilent(bool s=true  )                    { _silent = s; return this; }
   
   uint64_t _id;
   bool _enabled;
   bool _removeAfterFiring;
   bool _silent;
   int64_t _count;
   static uint64_t globalID;
   };

uint64_t Breakpoint::globalID = 1;

struct InternalBreakpoint : public Breakpoint
   {
   InternalBreakpoint() : Breakpoint() { }
   virtual void print(TextWriter *w) { }
   };

void
Breakpoint::print(TextWriter *writer)
   {
   *writer << "Breakpoint " << _id;
   if (_enabled)
      *writer << " (enabled): ";
   else
      *writer << " (disabled, ignore count " << _count << "): ";
   }

struct BreakpointAtTime : public Breakpoint
   {
   BreakpointAtTime(uint64_t t)
      : Breakpoint()
      , _time(t)
      { }
   virtual bool breakAt(uint64_t time)
      {
      return time == _time && fire();
      }
   virtual void print(TextWriter *writer)
      {
      Breakpoint::print(writer);
      *writer << "Stop at time " << _time << writer->endl();
      }

   uint64_t _time;
   };

struct BreakpointStepInto : public BreakpointAtTime
   {
   BreakpointStepInto(uint64_t t)
      : BreakpointAtTime(t)
      { }

   virtual void print(TextWriter *writer) { }
   };

struct BreakpointAfterOperation : public Breakpoint
   {
   BreakpointAfterOperation(uint64_t id)
      : Breakpoint()
      , _opID(id)
      { }
   virtual bool breakAfter(Operation *op)
      {
      return (op->id() == _opID) && fire();
      }
   virtual void print(TextWriter *writer)
      {
      Breakpoint::print(writer);
      *writer << "Stop after op" << _opID << writer->endl();
      }
   uint64_t _opID;
   };

struct BreakpointBeforeOperation : public Breakpoint
   {
   BreakpointBeforeOperation(uint64_t id)
      : Breakpoint()
      , _opID(id)
      { }
   virtual bool breakBefore(Operation *op)
      {
      return (op->id() == _opID) && fire();
      }
   virtual void print(TextWriter *writer)
      {
      Breakpoint::print(writer);
      *writer << "Stop before op" << _opID << writer->endl();
      }
   uint64_t _opID;
   };

struct BreakpointStepOver : public InternalBreakpoint
   {
   // This one is complicated :( .
   // From the current operation, control could flow:
   //     1) to an unbound builder
   //     2) to a potentially empty builder bound to the current operation
   //     3) to a potentially empty builder bound to some other operation
   //     4) to the next operation in the current builder
   //     5) to the end of of the current bound builder and returning to the parent operation which can then act like any of 1 through 5
   //     6) out of the current function (if current operation is Return)
   //
   // StepOver means to stop at the next executed operation BUT skip any operations executed by a builder object bound to this operation
   // Here is how each scenario is handled:
   //     1) add target builder's first operation to _stopOp list
   //     2) add target builder's bound operation (which is also the current operation) to the _stopOp list
   //     3) add target builder's bound operation to the _stopOp list
   //     4) add next operation to the _stopOp list
   //     5) add this operation's parent builder's bound operation to the _stopOp list
   //     6) do nothing
   // Whichever of the operations 1-5 is encountered first, that will fire this breakpoint (typically also remove the breakpoint)
   //
   BreakpointStepOver(Operation *op, Operation *nextOp)
      : InternalBreakpoint()
      {
      if (nextOp)
         _stopOps.push_back(nextOp->id());
      else if (op->parent() != op->fb() && op->parent()->controlReachesEnd())
         {
         assert(op->parent()->isBound());
         _stopOps.push_back(op->parent()->boundToOperation()->id());
         }

      for (BuilderIterator bIt = op->BuildersBegin(); bIt != op->BuildersEnd(); bIt++)
         {
         Builder *bTgt = *bIt;
         if (bTgt->isBound())
            _stopOps.push_back(bTgt->boundToOperation()->id()); // may be op itself!
         else
            _stopOps.push_back(bTgt->operations()[0]->id());
         }
      }

   virtual bool breakBefore(Operation *op)
      {
      uint64_t opID = op->id();
      for (auto opIt = _stopOps.begin(); opIt != _stopOps.end(); opIt++)
         {
         uint64_t stopID = *opIt;
         if (opID == stopID)
            return fire();
         }
      return false;
      }

   std::vector<uint64_t> _stopOps;
   };

struct BreakpointBeforeBuilder : public Breakpoint
   {
   BreakpointBeforeBuilder(uint64_t id)
      : Breakpoint()
      , _bID(id)
      { }
   virtual bool breakBefore(Builder *b)
      {
      return (b->id() == _bID) && fire();
      }
   virtual void print(TextWriter *writer)
      {
      Breakpoint::print(writer);
      *writer << "Stop before B" << _bID << writer->endl();
      }
   uint64_t _bID;
   };

void
Debugger::printHelp()
   {
   TextWriter &w = *_writer;
   w << "JBDB Command reference" << w.endl();
   w << "   h,  help          display this help summary" << w.endl();
   w << "   l,  list          print the current methodbuilder IL" << w.endl();
   w << "   s,  step          step into the next operation, including operations in bound builders" << w.endl();
   w << "   n,  next          step over the next operation, not including operations in bound builders" << w.endl();
   w << "   c,  cont          continue until the next breakpoint" << w.endl();
   w << "   pv, printvalue    print a value (v#)" << w.endl();
   w << "   pt, printtype     print a type (t#)" << w.endl();
   w << "   p,  print         print a symbol (name)" << w.endl();
   w << "   bl, breaklist     print list of active breakpoints" << w.endl();
   w << "   bb, breakbefore   break before an operation (o#) or builder (B#)" << w.endl();
   w << "   ba, breakafter    break after an operation (o#)" << w.endl();
   w << "   b @#              break at time #" << w.endl();
   w << "   d, debug          debug opcode handler for an operation (o#)" << w.endl();
   w << w.endl();
   }

// accept input on what to do next
// op is the operation currently being debugged; it can be NULL e.g. at a breakpoint *after* an operation
// nextOp is the next operation that would sequentially follow this operation in the current builder object;
//     it can be NULL e.g. if the current operation is the last one in the current builder object
void
Debugger::acceptCommands(Operation *op, Operation *nextOp)
   {
   bool done = false;
   while (!done)
      {

      fprintf(stderr, "[T=%llu] (jbdb) ", _time);
   
      char line[1024];
      if (fgets(line, sizeof(line), stdin))
         {
         if (line[0] == '\n')
            if (_commandHistory.size() > 0) // repeat last
               strcpy(line, _commandHistory.back().c_str());
            else
               continue;
         else
            {
            std::string current(line);
            _commandHistory.push_back(current);
            }

         char *command = strtok(line, " \r\n");
         if (strcmp(command, "h") == 0 || strcmp(command, "help") == 0)
            {
            printHelp();
            }
         if (strcmp(command, "n") == 0 || strcmp(command, "next") == 0)
            {
            Breakpoint *brkpt = (new BreakpointStepOver(op, nextOp))->setRemoveAfterFiring();
            _frame->_breakpoints.push_front(brkpt); // put it at the beginning so it will be found quickly and quick to remove
            done = true;
            break;
            }
         if (strcmp(command, "s") == 0 || strcmp(command, "step") == 0)
            {
            Breakpoint *brkpt = (new BreakpointStepInto(_time+1))->setRemoveAfterFiring();
            _frame->_breakpoints.push_front(brkpt); // put it at the beginning so it will be found quickly and quick to remove
            done = true;
            break;
            }
         else if (strcmp(command, "c") == 0 || strcmp(command, "cont") == 0 || strcmp(command, "continue") == 0)
            {
            done = true;
            break;
            }
         else if (strcmp(command, "pt") == 0 || strcmp(command, "printtype") == 0)
            {
            char *expr = strtok(NULL, " \r\n");
            uint64_t id = strtoul(expr, NULL, 10);
            if (expr[0] == 't')
               id = strtoul(expr+1, NULL, 10);
         
            if (id < Type::maxID())
               printType(_fb->dict()->LookupType(id));
            else
               *(_writer) << "Unrecognized type: should be t# (max id:" << Type::maxID() << ")" << _writer->endl();
            }
         else if (strcmp(command, "pv") == 0 || strcmp(command, "printvalue") == 0)
            {
            char *expr = strtok(NULL, " \r\n");
            uint64_t id = strtoul(expr, NULL, 10);
            if (expr[0] == 'v')
               id = strtoul(expr+1, NULL, 10);

            if (id < Value::maxID())
               printValue(id);
            else
               *(_writer) << "Unrecognized value: should be v# (max id:" << Value::maxID() << ")" << _writer->endl();
            }
         else if (strcmp(command, "p") == 0 || strcmp(command, "print") == 0)
            {
            char *expr = strtok(NULL, " \r\n");
            if (expr && _fb->getSymbol(expr))
               {
               printSymbol(expr);
               }
            else
               fprintf(stderr, "Unrecognized symbol name\n");
            }
         else if (strcmp(command, "l") == 0 || strcmp(command, "list") == 0)
            {
            _writer->print();
            }
         else if (strcmp(command, "bb") == 0 || strcmp(command, "breakbefore") == 0)
            {
            char *bp = strtok(NULL, " \r\n");
            if (strncmp(bp, "o", 1) == 0)
               {
               uint64_t id = strtoul(bp+1, NULL, 10);
               Breakpoint *brkpt = new BreakpointBeforeOperation(id);
               _frame->_breakpoints.push_back(brkpt);
               *_writer << "Breakpoint " << brkpt->_id << " will stop before operation o" << id << _writer->endl();
               }
            else if (bp[0] == 'B')
               {
               uint64_t id = strtoul(bp+1, NULL, 10);
               Breakpoint *brkpt = new BreakpointBeforeBuilder(id);
               _frame->_breakpoints.push_back(brkpt);
               *_writer << "Breakpoint " << brkpt->_id << " will stop before builder b" << id << _writer->endl();
               }
            }
         else if (strcmp(command, "ba") == 0 || strcmp(command, "breakafter") == 0)
            {
            char *bp = strtok(NULL, " \r\n");
            if (strncmp(bp, "o", 1) == 0)
               {
               uint64_t id = strtoul(bp+1, NULL, 10);
               Breakpoint *brkpt = new BreakpointAfterOperation(id);
               _frame->_breakpoints.push_back(brkpt);
               *_writer << "Breakpoint " << brkpt->_id << " will stop after operation o" << id << _writer->endl();
               }
            }
         else if (strcmp(command, "bl") == 0 || strcmp(command, "breaklist") == 0)
            {
            for (auto it=_frame->_breakpoints.begin(); it != _frame->_breakpoints.end(); it++)
               {
               Breakpoint *bp = *it;
               bp->print(_writer);
               }
            }
         else if (strcmp(command, "b") == 0)
            {
            char *bp = strtok(NULL, " \r\n");
            if (bp[0] == '@')
               {
               uint64_t time = strtoul(bp+1, NULL, 10);
               Breakpoint *brkpt = new BreakpointAtTime(time);
               _frame->_breakpoints.push_back(brkpt);
               *_writer << "Breakpoint " << brkpt->_id << " will stop at time " << time << _writer->endl();
               }
            }
         else if (strcmp(command, "d") == 0 || strcmp(command,"debug") == 0)
            {
            char *opStr = strtok(NULL, " \r\n");
            if (strncmp(opStr, "o", 1) == 0)
               {
               uint64_t id = strtoul(opStr+1, NULL, 10);
               _frame->_info->_debugOperations.insert({id, true});
               *_writer << "Will debug into operation handler for o" << id << _writer->endl();
               }
            }
         }
      }
   }

void
Debugger::showOp(Operation *op, std::string msg)
   {
   TextWriter &w = *_writer;
   w << msg;
   w.writeOperation(op);
   }

bool
Debugger::breakBefore(Operation *op)
   {
   for (auto it=_frame->_breakpoints.begin(); it != _frame->_breakpoints.end(); it++)
      {
      Breakpoint *bp=*it;
      if (bp->breakBefore(op) || bp->breakAt(_time))
         {
         bp->print(_writer);
         if (bp->removeAfterFiring())
            _frame->_breakpoints.erase(it);
         return true;
         }
      }
   return false;
   }

bool
Debugger::breakAfter(Operation *op)
   {
   for (auto it=_frame->_breakpoints.begin(); it != _frame->_breakpoints.end(); it++)
      {
      Breakpoint *bp=*it;
      if (bp->breakAfter(op))
         {
         bp->print(_writer);
         if (bp->removeAfterFiring())
            _frame->_breakpoints.erase(it);
         return true;
         }
      }
   return false;
   }

bool
Debugger::breakBefore(Builder *b)
   {
   for (auto it=_frame->_breakpoints.begin(); it != _frame->_breakpoints.end(); it++)
      {
      Breakpoint *bp=*it;
      if (bp->breakBefore(b))
         {
         if (bp->removeAfterFiring())
            _frame->_breakpoints.erase(it);

         if (bp->silent())
            {
            OperationIterator allIter = b->OperationsBegin();
            OperationIterator * pIter = &allIter;
            auto entryPoint = _frame->_builderReentryPoints.find(b->id());
            if (entryPoint != _frame->_builderReentryPoints.end())
               pIter = &(entryPoint->second);
               
            OperationIterator &opIt = *pIter;
            Operation *op = *opIt;
            Breakpoint *newBP = new BreakpointBeforeOperation(op->id());
            newBP->setSilent();
            _frame->_breakpoints.push_front(newBP);

            return false;
            }

         bp->print(_writer);

         return true;
         }
      }
   return false;
   }

void
Debugger::beforeOp(Operation *op, Operation *nextOp)
   {
   if (breakBefore(op))
      {
      showOp(op, "Stopped before ");
      acceptCommands(op, nextOp);
      }
   }

void
Debugger::afterOp(Operation *op, Operation *nextOp)
   {
   if (breakAfter(op))
      {
      showOp(op, "Stopped after ");
      acceptCommands(op, nextOp);
      }
   }

DebugDictionary *
Debugger::getDictionary(FunctionBuilder *fb)
   {
   FunctionDebugInfo *info = _functionDebugInfos[fb->id()];
   assert(info);
   return &info->_dbgDict;
   }

void
Debugger::debug(FunctionBuilder *fb, DebugValue *returnValues, DebugValue *locals)
   {
   FunctionBuilder *savedFB = _fb;
   DebuggerFrame *savedFrame = _frame;

   DebuggerFrame frame;
   frame._debugger = this;
   frame._info = _functionDebugInfos[fb->id()];
   size_t valueSizeInBytes = frame._info->_valueSizeInBytes;
   frame._returnValues = returnValues;
   frame._locals = locals;
   frame._values = reinterpret_cast<DebugValue *>(new uint8_t[fb->numValues() * valueSizeInBytes]);
   frame._fromBuilder = fb;
   frame._returning = false;
   frame._builderToDebug = fb;
   _frame = &frame;
   _fb = fb;

   if (_firstEntry)
      {
      TextWriter &w = (*_writer);
      w << "JitBuilder Debugger (JBDB)" << w.endl();
      w << "Happy debugging!" << w.endl() << w.endl();
      w << "Type h or help for a list of jbdb commands" << w.endl() << w.endl();
      w << "Entering function " << fb->name() << " with arguments:" << w.endl();
      for (auto pIt = fb->ParametersBegin(); pIt != fb->ParametersEnd(); pIt++)
         {
         const ParameterSymbol *param = *pIt;
         w << "    ";
         printSymbol(param->name());
         }
      w << w.endl();
      _firstEntry = false;
      }

   Breakpoint *brkpt = new BreakpointBeforeBuilder(fb->id());
   brkpt->setSilent()
        ->setRemoveAfterFiring();
   _frame->_breakpoints.push_front(brkpt);
 
   _frame->_builderToDebug = _fb;
   while (_frame->_builderToDebug)
      {
      Builder *b = _frame->_builderToDebug;
      //_frame->_builderToDebug = NULL;
      debug(b);
      }

   _fb = savedFB;
   _frame = savedFrame;

   //if (savedFB != fb)
   //   (*_writer) << "Returning from FB" << fb->id() << _writer->endl();
   }

void
Debugger::ensureOperationDebugger(Operation * op)
   {
   FunctionDebugInfo *info = _frame->_info;

   auto opDbgrs = info->_operationDebuggers;
   auto debugger = opDbgrs.find(op->id());
   if (debugger != opDbgrs.end())
      return;

   OperationDebugger *opDebugFB = new OperationDebugger(this, op);
   int32_t rc = opDebugFB->Construct();

//#define TRACE_OPDEBUGGERS
#ifdef TRACE_OPDEBUGGERS
   std::cout.setf(std::ios_base::skipws);
   OMR::JitBuilder::TextWriter printer(opDebugFB, std::cout, std::string("    "));
   opDebugFB->setLogger(&printer);
   opDebugFB->config()
                   ->setTraceBuildIL()
                   //->setTraceReducer()
                   //->setTraceCodeGenerator()
                   ;
   std::cerr << "Operation Debugger for " << op << std::endl;
   printer.print();
#endif

   // debug entry doesn't work yet due to Type name conflicts (DebugFrame, DebugValue)
   bool useDebugEntry = (info->_debugOperations.find(op->id()) != info->_debugOperations.end());
   auto dbgFunc = useDebugEntry ? opDebugFB->DebugEntry<OperationDebuggerFunc>(&rc) : opDebugFB->CompiledEntry<OperationDebuggerFunc>(&rc);
   info->_operationDebuggers[op->id()] = dbgFunc;
   }

void
Debugger::debug(Builder *b)
   {
   auto allIter = b->OperationsBegin();
   OperationIterator * iter = &allIter;
   bool usingReentryPoint = false;

   auto entryPoint = _frame->_builderReentryPoints.find(b->id());
   if (entryPoint != _frame->_builderReentryPoints.end())
      {
      iter = &(entryPoint->second);
      usingReentryPoint = true;
      }
   else if (_frame->_fromBuilder->isBound() && _frame->_returning)
      {
      // this scenario is exemplified by an AppendBuilder operation referencing a bound Builder object to which control has just been directed
      // When that builder object completes executing, it comes "back" to its parent but the parent was never entered
      // note that it is possible for any Goto to direct to *any* bound builder object of an operation
      bool foundFromBuilder = false;
      while (allIter != b->OperationsEnd())
         {
         Operation *possibleOwnerOp = *allIter;
         foundFromBuilder = false;
         for (BuilderIterator bIt = possibleOwnerOp->BuildersBegin(); bIt != possibleOwnerOp->BuildersEnd(); bIt++)
            {
            Builder *bTgt = *bIt;
            if (bTgt == _frame->_fromBuilder)
               {
               foundFromBuilder = true;
               break;
               }
            }

         if (foundFromBuilder)
            break;

         allIter++;
         }

      // something wrong if we didn't find the fromBuilderTarget
      if (!foundFromBuilder)
         {
         TextWriter &w = (*_writer);
         uint64_t fromBuilderID = _frame->_fromBuilder->id();

         w << "Internal debugger error:" << w.endl();
         w << "    Control arrived at B" << b->id() << w.endl();
         w << "    From B" << fromBuilderID << w.endl();
         w << "    but no operation has B" << fromBuilderID << " as a bound builder" << w.endl();
         w << "Aborting frame with no way to recover" << w.endl();

         _frame->_fromBuilder = NULL;
         _frame->_builderToDebug = NULL;
         assert(foundFromBuilder);
         return;
         }
      }
   else
      // first time this builder has been entered
      _frame->_fromBuilder = b;

   OperationIterator &opIt = *iter;
   if (breakBefore(b))
      {
      _writer->print(b);
      Operation *op = NULL;
      if (opIt != b->OperationsEnd())
         op = *opIt;
      acceptCommands(NULL, op);
      }

   _frame->_builderToDebug = NULL;
   _frame->_returning = false;
   while(opIt != b->OperationsEnd())
      {
      Operation *op = *opIt;
      auto nextIt = std::next(opIt);
      Operation *nextOp = (nextIt == b->OperationsEnd()) ? NULL : (*nextIt);

      beforeOp(op, nextOp);
      bool suspend = debug(op);
      afterOp(op, nextOp);

      if (suspend)
         {
         if (_frame->_builderToDebug->isBound() && _frame->_builderToDebug->boundToOperation() == op && !usingReentryPoint)
            _frame->_builderReentryPoints.insert({b->id(), opIt});

         return;
         }

      opIt++; // must do after the above suspension code so bound builders return to the same operation, not the following operation
      //op = nextOp;
      }

   // done with iterator now, so if we got it from the reentry points we can erase it
   if (usingReentryPoint)
      _frame->_builderReentryPoints.erase(b->id());

   if (b->isBound())
      {
      _frame->_fromBuilder = b;
      _frame->_builderToDebug = b->boundToOperation()->parent();
      _frame->_returning = true;
      //(*_writer) << "Returning from B" << b->id() << " to B" << _builderToDebug->id() << _writer->endl();
      return;
      }

   // shouldn't fall off end of an unbound builder unless it's the end of the function!
   assert(_frame->_builderToDebug == NULL);
   }

bool
Debugger::debug(Operation *op)
   {
   ensureOperationDebugger(op);
   showOp(op, "Executing: ");
   bool suspendBuilder = _frame->_info->_operationDebuggers[op->id()](_frame, _frame->_fromBuilder->id());
   _time++;
   return suspendBuilder;
   }

extern "C"
{
#define DEBUGFUNCTION_LINENUMBER LINETOSTR(__LINE__)
void
Debugger::debugFunction(Debugger *dbgr, FunctionBuilder *fb, DebugValue *returnValues, DebugValue *locals)
   {
   dbgr->debug(fb, returnValues, locals);
   }
}


class DebuggerThunk : public DebuggerFunctionBuilder
   {
   public:
   DebuggerThunk(Debugger *dbgr, FunctionBuilder *debugFB)
      : DebuggerFunctionBuilder(dbgr, debugFB) //, dbgr->getDictionary(debugFB))
      , _debugger(dbgr)
      , _debugFB(debugFB)
      {
      DefineName(std::string("jbdb_") + debugFB->name());
      DefineFile(debugFB->fileName());
      DefineLine(debugFB->lineNumber());

      for (auto pIt = debugFB->ParametersBegin(); pIt != debugFB->ParametersEnd(); pIt++)
         {
         const ParameterSymbol *param = *pIt;
         DefineParameter(param->name(), param->type());
         }
      DefineReturnType(debugFB->getReturnType());

      DefineFunction("debugFunction()", __FILE__, DEBUGFUNCTION_LINENUMBER, reinterpret_cast<void *>(&Debugger::debugFunction),
                     NoType, 4, Address, Address, _pDebugValue, _pDebugValue);
      }

   virtual bool buildIL()
      {
      const char *returnValues = "returnValues";
      int32_t numReturnValues = _debugFB->numReturnValues();
      Value *rvPtr = NULL;
      if (numReturnValues > 0)
         rvPtr = CreateLocalArray(numReturnValues, dbgDict()->_DebugValue);
      else
         rvPtr = ConstAddress(0);
      Store(returnValues, CoercePointer(_pDebugValue, rvPtr));

      const char *locals = "locals";
      int32_t numLocals = _debugFB->numLocals();
      Value *lPtr = NULL;
      if (numLocals > 0)
         lPtr = CreateLocalArray(numLocals, dbgDict()->_DebugValue);
      else
         lPtr = ConstAddress(0);
      Store(locals, CoercePointer(_pDebugValue, lPtr));

      for (auto pIt = _debugFB->ParametersBegin(); pIt != _debugFB->ParametersEnd(); pIt++)
         {
         ParameterSymbol *parm = *pIt;
         storeValue(this, parm, Load(parm->name()));
         }
      Call(Load("debugFunction()"), 4, ConstAddress(_debugger), ConstAddress(_debugFB), Load(returnValues), Load(locals));
      if (numReturnValues > 0)
         {
         assert(numReturnValues == 1); // only supporting one return value ffor now
         Return(loadFromDebugValue(this, Load(returnValues), _debugFB->getReturnType()));
         }
      else
         Return();

      return true;
      }

   protected:
   Debugger *_debugger;
   FunctionBuilder *_debugFB;
   };


void *
Debugger::createDebugger(int32_t *returnCode)
   {
   DebuggerThunk *thunk = new DebuggerThunk(this, _fb);
   thunk->Construct();

//#define TRACE_DEBUGTHUNK
#ifdef TRACE_DEBUGTHUNK
   std::cout.setf(std::ios_base::skipws);
   OMR::JitBuilder::TextWriter printer(thunk, std::cout, std::string("    "));
   thunk->setLogger(&printer);
   thunk->config()
                   //->setTraceBuildIL()
                   //->setTraceReducer()
                   ->setTraceCodeGenerator();
   std::cerr << "Debug Entry Thunk:" << std::endl;
   printer.print();
#endif

   return thunk->CompiledEntry<void *>(returnCode);
   }

} // namespace JitBuilder
} // namespace OMR
