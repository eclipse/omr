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

#ifndef DEBUGGER_INCL
#define DEBUGGER_INCL

#include <list>
#include "Action.hpp"
#include "FunctionBuilder.hpp"
#include "Object.hpp"
#include "TextWriter.hpp"
#include "TypeDictionary.hpp"

namespace OMR
{
namespace JitBuilder
{

class Operation;
class Type;
class TypeDictionary;
class Value;

class Debugger;
class DebuggerFrame;
class DebuggerThunk;
class DebugValue;
class FunctionDebugInfo;

class Debugger : public Object
   {
   friend class DebuggerThunk;

   public:
   Debugger(FunctionBuilder *fb);
   void * createDebugger(int32_t *returnCode);

   uint64_t index(const Symbol *symbol) const { return symbol->id(); };
   uint64_t index(const Value *value) const   { return value->id(); }

   void ensureOperationDebugger(Operation * op);

   // These functions are static so they can be called by generated DebuggerThunk
   static void debugFunction(Debugger *dbgr, FunctionBuilder *b, DebugValue *returnValues, DebugValue *locals);

   uint64_t time() { return _time; }

   // public because needs to be called by the DebuggerActionHandler objects
   void switchTo(Builder *b); // { _frame->switchTo(b); }
   Builder * switchedFromBuilder(); // { return _frame->_builderFrom; }

   DebugDictionary *getDictionary(FunctionBuilder *fb);

   protected:

   virtual void debug(FunctionBuilder *fb, DebugValue *returnValues, DebugValue *locals);
   virtual void debug(Builder *b);
   virtual bool debug(Operation *op);

   virtual void printDebugValue(DebugValue *val);
   virtual void printTypeName(Type *type);
   virtual void printType(Type *type);
   virtual void printValue(uint64_t idx);
   virtual void printSymbol(std::string name);
   virtual void printHelp();
   virtual void acceptCommands(Operation *op, Operation *nextOp=NULL);
   virtual void showOp(Operation *op, std::string msg);

   virtual void beforeOp(Operation *op, Operation *nextOp);
   virtual void afterOp(Operation *op, Operation *nextOp);

   virtual bool breakBefore(Builder *b);
   virtual bool breakBefore(Operation *op);
   virtual bool breakAfter(Operation *op);

   void setup();

   void recordReentryPoint(Builder *b, OperationIterator & opIt);
   OperationIterator * fetchReentryPoint(Builder *b);
   void removeReentryPoint(Builder *b);

   TextWriter *_writer;
   std::list<std::string> _commandHistory;
   uint64_t _time;
   DebuggerFrame *_frame;
   std::map<int64_t, FunctionDebugInfo *> _functionDebugInfos; // maps from fb->id() to FDI
   bool _firstEntry;
   };

} // namespace JitBuilder
} // namespace OMR

#endif // defined(DEBUGGER_INCL)
