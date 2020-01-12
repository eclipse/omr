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

#ifndef FUNCTIONBUILDER_INCL
#define FUNCTIONBUILDER_INCL


#include <string>
#include <vector>

#include "Builder.hpp"
#include "Config.hpp"
#include "Iterator.hpp"
#include "Operation.hpp"
#include "Symbol.hpp"
#include "Type.hpp"

#define TOSTR(x) #x
#define LINETOSTR(x) TOSTR(x)

namespace OMR
{

namespace JitBuilder
{

class FunctionBuilder;
class Operation;
class Debugger;
class TextWriter;
class TypeDictionary;

class FunctionBuilder : public Builder
   {
   friend class Builder;
   friend class Type;
public:
   virtual ~FunctionBuilder();

   void DefineName(std::string name);
   void DefineFile(std::string file);
   void DefineLine(std::string line);
   void DefineParameter(std::string name, Type * type);
   void DefineReturnType(Type * type);
   LocalSymbol * DefineLocal(std::string name, Type * type);
   void DefineFunction(std::string name, std::string fileName, std::string lineNumber, void *entryPoint, Type *returnType, int32_t numParms, ...);
   void DefineFunction(std::string name, std::string fileName, std::string lineNumber, void *entryPoint, Type *returnType, int32_t numParms, Type **parmTypes);

   std::string name() const                        { return _givenName; }
   std::string fileName() const                    { return _fileName; }
   std::string lineNumber() const                  { return _lineNumber; }

   ParameterSymbolIterator ParametersBegin()       { return _parameters.begin(); }
   ParameterSymbolIterator ParametersEnd()         { return _parameters.end(); }
   ParameterSymbolVector ResetParameters()
      {
      ParameterSymbolVector prev = _parameters;
      _parameters.clear();
      return prev;
      }
   LocalSymbolIterator LocalsBegin()               { return _locals.begin(); }
   LocalSymbolIterator LocalsEnd()                 { return _locals.end(); }
   LocalSymbolVector ResetLocals()
      {
      LocalSymbolVector prev = _locals;
      _locals.clear();
      return prev;
      }

   FunctionSymbolIterator FunctionsBegin()         { return _functions.begin(); }
   FunctionSymbolIterator FunctionsEnd()           { return _functions.end(); }
   FunctionSymbolVector ResetFunctions()
      {
      FunctionSymbolVector prev = _functions;
      _functions.clear();
      return prev;
      }

   FunctionSymbol *LookupFunction(std::string name)
      {
      Symbol *sym = getSymbol(name);
      if (!sym->isFunction())
         return NULL;
      return static_cast<FunctionSymbol *>(sym);
      }

   int32_t numValues() const;
   int32_t numLocals() const;
   int32_t numReturnValues() const                 { return (_returnType == NoType) ? 0 : 1; /* for now */ }

   Type * getReturnType() const                    { return _returnType; }
   TypeDictionary * dict() const                   { return _types; }
   Config * config()                               { return &_config; }
   virtual size_t size() const                     { return sizeof(FunctionBuilder); }

   bool constructIL()
      {
      bool rc = buildIL();
      _ilBuilt = true;
      return rc;
      }

   virtual bool buildIL()
      {
      return false;
      }

   bool Construct();

   template<typename T> T *CompiledEntry(int32_t *returnCode) { return reinterpret_cast<T *>(internalCompile(returnCode)); }
   template<typename T> T *entryPoint()                       { assert(_entryPoint != NULL); return reinterpret_cast<T *>(_entryPoint); }

   template<typename T> T *DebugEntry(int32_t *returnCode)    { return reinterpret_cast<T *>(internalDebugger(returnCode)); }
   template<typename T> T *debugEntryPoint()                  { assert(_debugEntryPoint != NULL); return reinterpret_cast<T *>(_debugEntryPoint); }

   bool ilBuilt() const                                       { return _ilBuilt; }

   Symbol * getSymbol(std::string name);

   int32_t incrementLocations()                               { return _numLocations++; }
   void addLocation(Location *loc )                           { _locations.push_back(loc); }

   int64_t incrementTransformation()                          { return _numTransformations++; }

   void setLogger(TextWriter * logger)                        { _logger = logger; }
   TextWriter * logger(bool enabled=false) const              { return enabled ? _logger : NULL; }

   virtual size_t size()                                      { return sizeof(FunctionBuilder); }
   void registerObject(Object *obj);

   protected:
   FunctionBuilder(TypeDictionary * types)
      : Builder(this, this, types)
      , _types(types)
      , _returnType(NoType)
      , _entryPoint(NULL)
      , _debugEntryPoint(NULL)
      , _ilBuilt(false)
      , _numLocations(0)
      , _memoryAllocated(0)
      , _numTransformations(0)
      , _logger(NULL)
      {
      SourceLocation(); // make sure everything has a location; by default BCIndex is 0
      }

   void internalDefineFunction(std::string name, std::string fileName, std::string lineNumber, void *entryPoint, Type *returnType, int32_t numParms, Type **parmTypes);
   void *internalCompile(int32_t *returnCode);
   void *internalDebugger(int32_t *returnCode);

   TypeDictionary         * _types;
   Config                   _config;
   std::string              _givenName;
   std::string              _fileName;
   std::string              _lineNumber;
   ParameterSymbolVector    _parameters;
   LocalSymbolVector        _locals;
   FunctionSymbolVector     _functions;
   Type                   * _returnType;

   void                   * _entryPoint;

   Debugger               * _debuggerObject;
   void                   * _debugEntryPoint;

   int64_t                  _numLocations;
   std::vector<Location *>  _locations;
   bool                     _ilBuilt;
   uint64_t                 _memoryAllocated;
   std::vector<Object *>    _objects;
   int64_t                  _numTransformations;
   TextWriter             * _logger;

   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(FUNCTIONBUILDER_INCL)
