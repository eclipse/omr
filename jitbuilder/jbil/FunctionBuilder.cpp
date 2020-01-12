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
#include "FunctionBuilder.hpp"
#include "Debugger.hpp"
#include "JitBuilder.hpp"
#include "Operation.hpp"
#include "TextWriter.hpp"


using namespace OMR::JitBuilder;


FunctionBuilder::~FunctionBuilder()
   {
   if (config()->reportMemory() && logger())
      logger()->indent() << "FunctionBuilder " << this << " : memory allocated is " << _memoryAllocated << " bytes" << logger()->endl();

   // do accounting for objects that will be freed to see if anything left
   for (auto objIt = _objects.begin(); objIt != _objects.end(); objIt++)
      {
      Object *obj = *objIt;
      _memoryAllocated -= obj->size();
      delete obj;
      }

   if (_memoryAllocated != 0 && config()->reportMemory() && logger())
      logger()->indent() << "Error: unaccounted memory: " << _memoryAllocated << logger()->endl();
   }

void
FunctionBuilder::registerObject(Object *obj)
   {
   if (obj != this)
      {
      _memoryAllocated += obj->size();
      _objects.push_back(obj);
      }
   }

int32_t
FunctionBuilder::numValues() const
   {
   return Value::maxID();
   }

int32_t
FunctionBuilder::numLocals() const
   {
   return _locals.size() + _parameters.size();
   }

void
FunctionBuilder::DefineName(std::string name)
   {
   _givenName = name;
   }

void
FunctionBuilder::DefineFile(std::string file)
   {
   _fileName = file;
   }

void
FunctionBuilder::DefineLine(std::string line)
   {
   _lineNumber = line;
   }

void
FunctionBuilder::DefineParameter(std::string name, Type * type)
   {
   ParameterSymbol *parm = ParameterSymbol::create(name, type, _parameters.size());
   _parameters.push_back(parm);
   }

void
FunctionBuilder::DefineReturnType(Type * type)
   {
   _returnType = type;
   _types->registerReturnType(type);
   }

LocalSymbol *
FunctionBuilder::DefineLocal(std::string name, Type * type)
   {
   Symbol *sym = getSymbol(name);
   if (sym && sym->isLocal())
      return static_cast<LocalSymbol *>(sym);

   LocalSymbol *local = LocalSymbol::create(name, type);
   _locals.push_back(local);
   return local;
   }

void
OMR::JitBuilder::FunctionBuilder::DefineFunction(std::string name,
                                                 std::string fileName,
                                                 std::string lineNumber,
                                                 void *entryPoint,
                                                 Type *returnType,
                                                 int32_t numParms,
                                                 ...)
   {
   Type **parmTypes = new Type*[numParms];
   va_list parms;
   va_start(parms, numParms);
   
   for (int32_t p=0;p < numParms;p++)
      parmTypes[p] = (Type *) va_arg(parms, Type *);
   va_end(parms);

   internalDefineFunction(name, fileName, lineNumber, entryPoint, returnType, numParms, parmTypes);
   }

void
OMR::JitBuilder::FunctionBuilder::DefineFunction(std::string name,
                                                 std::string fileName,
                                                 std::string lineNumber,
                                                 void *entryPoint,
                                                 Type *returnType,
                                                 int32_t numParms,
                                                 Type **parmTypes)
   {
   // copy parameter types so don't have to force caller to keep the parmTypes array alive
   Type **copiedParmTypes = new Type*[numParms];
   for (int32_t p=0;p < numParms;p++)
      copiedParmTypes[p] = parmTypes[p];

   internalDefineFunction(name, fileName, lineNumber, entryPoint, returnType, numParms, copiedParmTypes);
   }

void
OMR::JitBuilder::FunctionBuilder::internalDefineFunction(std::string name,
                                                         std::string fileName,
                                                         std::string lineNumber,
                                                         void *entryPoint,
                                                         Type *returnType,
                                                         int32_t numParms,
                                                         Type **parmTypes)
   {
   FunctionType *type = _types->DefineFunctionType(name, returnType, numParms, parmTypes);
   FunctionSymbol *sym = FunctionSymbol::create(type, name, fileName, lineNumber, entryPoint);

   // TODO: Why? _locals.push_back(sym);
   _functions.push_back(sym);
   }

Symbol *
FunctionBuilder::getSymbol(std::string name)
   {
   for (LocalSymbolIterator lIt = LocalsBegin(); lIt != LocalsEnd(); lIt++)
      {
      LocalSymbol * local = *lIt;
      if (local->name() == name)
         return local;
      }

   for (ParameterSymbolIterator pIt = ParametersBegin(); pIt != ParametersEnd(); pIt++)
      {
      ParameterSymbol * parameter = *pIt;
      if (parameter->name() == name)
         return parameter;
      }

   for (FunctionSymbolIterator fIt = FunctionsBegin(); fIt != FunctionsEnd(); fIt++)
      {
      FunctionSymbol * function = *fIt;
      if (function->name() == name)
         return function;
      }

   return NULL;
   }

bool
OMR::JitBuilder::FunctionBuilder::Construct()
   {
   return constructFunctionBuilder(this);
   }

void *
OMR::JitBuilder::FunctionBuilder::internalCompile(int32_t *returnCode)
   {
   _entryPoint = NULL;
   *returnCode = compileFunctionBuilder(this, &_entryPoint);
   return _entryPoint;
   }

void *
OMR::JitBuilder::FunctionBuilder::internalDebugger(int32_t *returnCode)
   {
   _debuggerObject = new OMR::JitBuilder::Debugger(this);
   _debugEntryPoint = _debuggerObject->createDebugger(returnCode);
   return _debugEntryPoint;
   }
