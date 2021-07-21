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

#ifndef DYNAMICOPERATION_INCL
#define DYNAMICOPERATION_INCL

#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "Action.hpp"
#include "Iterator.hpp"
#include "Operation.hpp"

namespace OMR
{

namespace JitBuilder
{

class Builder;
class Case;
class LiteralValue;
class OperationBuilder;
class OperationReplacer;
class Symbol;
class Type;
class TypeDictionary;
class TypeGraph;
class Value;

typedef bool (OperationExpander)(OperationReplacer *replacer);

typedef void (OperationPrinter)(TextWriter *w, Operation *op);

typedef void (OperationRegistrar)(TypeDictionary *dict, TypeGraph *graph);

// DynamicOperation defines a class of operations that can be configured dynamically at run timie. 
// DynamicOperation objects are not created directly: they are created via an OperationBuilder object (see below).
// A DynamicOperation has an OperationExpander, which is a function that converts the operation to
// a Builder object containing Operations that implement the DynamicOperation. Before Code Generation,
// all DynamicOperations will be replaced by the contents of the Builder object returned by its OperationExpander.

class DynamicOperation : public Operation
   {
   friend class OperationBuilder;

   public:

   virtual size_t size() const { return sizeof(DynamicOperation); }

   virtual bool isDynamic() const { return true; }

   virtual LiteralIterator LiteralsBegin()             { return LiteralIterator(_literals, _numLiterals); }
           LiteralIterator &LiteralsEnd()              { return literalEndIterator; }
   virtual int32_t numLiterals() const                 { return _numLiterals; }
   virtual LiteralValue * literal(int i=0) const       { assert(i >= 0 && i < _numLiterals); return _literals[i]; }

   virtual SymbolIterator SymbolsBegin()               { return SymbolIterator(_symbols, _numSymbols); }
           SymbolIterator &SymbolsEnd()                { return symbolEndIterator; }
   virtual int32_t numSymbols() const                  { return _numSymbols; }
   virtual Symbol *symbol(int i=0) const               { assert(i >=0 && i < _numSymbols); return _symbols[i]; }

   virtual ValueIterator OperandsBegin()               { return ValueIterator(_operands, _numOperands); }
           ValueIterator &OperandsEnd()                { return valueEndIterator; }
   virtual int32_t numOperands() const                 { return _numOperands; }
   virtual Value * operand(int i=0) const              { assert(i >= 0 && i < _numOperands); return _operands[i]; }

   virtual ValueIterator ResultsBegin()                { return ValueIterator(_results, _numResults); }
           ValueIterator &ResultsEnd()                 { return valueEndIterator; }
   virtual int32_t numResults() const                  { return _numResults; }
   virtual Value * result(int i=0) const               { assert(i >= 0 && i < _numResults); return _results[i]; }
 
   virtual BuilderIterator BuildersBegin()             { return BuilderIterator(_builders, _numBuilders); }
           BuilderIterator &BuildersEnd()              { return builderEndIterator; }
   virtual int32_t numBuilders() const                 { return _numBuilders; }
   virtual Builder *builder(int i=0) const             { assert(i >= 0 && i < _numBuilders); return _builders[i]; }

   virtual CaseIterator CasesBegin()                   { return CaseIterator(_cases, _numCases); }
   virtual CaseIterator CasesEnd()                     { return CaseIterator(); }
   virtual int32_t numCases() const                    { return _numCases; }
   virtual Case * getCase(int i=0) const               { assert(i >= 0 && i < _numCases); return _cases[i]; }

   virtual TypeIterator TypesBegin()                   { return TypeIterator(_types, _numTypes); }
           TypeIterator &TypesEnd()                    { return typeEndIterator; }
   virtual int32_t numTypes() const                    { return _numTypes; }
   virtual Type * type(int i=0) const                  { assert(i >= 0 && i < _numTypes); return _types[i]; }

   virtual Operation * clone(Builder *b, Value **results) const;
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const;
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMappers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const;
   virtual Operation * clone(Builder *b, OperationCloner *cloner) const;

   void print(TextWriter *w)
      {
      assert(_printer);
      _printer(w, this);
      }

   bool hasExpander() const { return _expander != NULL; }
   bool expand(OperationReplacer *replacer) const
      {
      assert(_expander);
      return _expander(replacer);
      }

   //
   // New public API
   // BEGIN {

   // } END
   // New public API
   //

protected:
   DynamicOperation(uint32_t action, Builder *parent,
                    uint32_t numReturnValues, Value **returnValues,
                    uint32_t numBuilders, Builder **builders,
                    uint32_t numCases, Case **cases,
                    uint32_t numLiterals, LiteralValue **literals,
                    uint32_t numOperands, Value **operands,
                    uint32_t numSymbols, Symbol **symbols,
                    uint32_t numTypes, Type **types,
                    OperationExpander *expander,
                    OperationPrinter *printer)
      : Operation(action, parent)
      , _numResults(numReturnValues)
      , _results(returnValues)
      , _numBuilders(numBuilders)
      , _builders(builders)
      , _numCases(numCases)
      , _cases(cases)
      , _numLiterals(numLiterals)
      , _literals(literals)
      , _numOperands(numOperands)
      , _operands(operands)
      , _numSymbols(numSymbols)
      , _symbols(symbols)
      , _numTypes(numTypes)
      , _types(types)
      , _expander(expander)
      , _printer(printer)
      { }

   //
   // New protected API
   // BEGIN {

   // } END
   // New protected API
   //

   uint32_t            _numResults;
   Value            ** _results;
   uint32_t            _numBuilders;
   Builder          ** _builders;
   uint32_t            _numCases;
   Case             ** _cases;
   uint32_t            _numLiterals;
   LiteralValue     ** _literals;
   uint32_t            _numOperands;
   Value            ** _operands;
   uint32_t            _numSymbols;
   Symbol           ** _symbols;
   uint32_t            _numTypes;
   Type             ** _types;
   OperationExpander * _expander;
   OperationPrinter  * _printer;
   };

class OperationBuilder
   {
   public:
   OperationBuilder()
      : _action(aNone)
      , _numResults(0)
      , _currentResultTypeIndex(0)
      , _resultTypes(NULL)
      , _currentResultIndex(0)
      , _results(NULL)
      , _numBuilders(0)
      , _currentBuilderIndex(0)
      , _builders(NULL)
      , _numCases(0)
      , _currentCaseIndex(0)
      , _cases(NULL)
      , _numLiterals(0)
      , _currentLiteralIndex(0)
      , _literals(NULL)
      , _numOperands(0)
      , _currentOperandIndex(0)
      , _operands(NULL)
      , _numSymbols(0)
      , _currentSymbolIndex(0)
      , _symbols(NULL)
      , _numTypes(0)
      , _currentTypeIndex(0)
      , _types(NULL)
      , _expander(NULL)
      , _printer(NULL)
      , _registrar(NULL)
      { }

   DynamicOperation *createOperation(Builder *parent);
   DynamicOperation *create(Builder *parent, Value *result, Value *v)
      {
      return addResult(result)
             ->addOperand(v)
             ->createOperation(parent);
      }
   DynamicOperation *create(Builder *parent, Value *result, Value *left, Value *right)
      {
      return addResult(result)
             ->addOperand(left)
             ->addOperand(right)
             ->createOperation(parent);
      }

   void initializeTypeProductions(TypeDictionary *dict, TypeGraph *graph)
      {
      if (_registrar)
         _registrar(dict, graph);
      }

   OperationBuilder *newAction(std::string name)
      {
      _action = NumActions++;
      registerDynamicActionName(_action, name);
      return this;
      }
   OperationBuilder *setAction(Action a)
      {
      _action = a;
      return this;
      }

   Action action()
      {
      return _action;
      }

   uint32_t numResults() { return _numResults; }
   OperationBuilder *setNumResults(uint32_t n)
      {
      _numResults = n;
      _resultTypes = new Type *[n];
      for (auto i = 0;i < n;i++)
         _resultTypes[i] = NULL;
      _currentResultTypeIndex = 0;
      _results = new Value *[n];
      for (auto i = 0;i < n;i++)
         _results[i] = NULL;
      _currentResultIndex = 0;
      return this;
      }
   OperationBuilder *addResultType(Type *t)
      {
      assert(_currentResultTypeIndex < _numResults);
      _resultTypes[_currentResultTypeIndex] = t;
      return this;
      }
   OperationBuilder *addResult(Value *v)
      {
      assert(_currentResultIndex < _numResults);
      _results[_currentResultIndex] = v;
      return this;
      }
   Type *resultType(uint32_t i=0) { assert(i < _numResults); return _resultTypes[i]; }

   uint32_t numBuilders() { return _numBuilders; }
   OperationBuilder *setNumBuilders(uint32_t n)
      {
      _numBuilders = n;
      _builders = new Builder *[n];
      for (auto i = 0;i < n;i++)
         _builders[i] = NULL;
      _currentBuilderIndex = 0;
      return this;
      }
   OperationBuilder *addBuilder(Builder *b)
      {
      assert(_currentBuilderIndex < _numBuilders);
      _builders[_currentBuilderIndex] = b;
      return this;
      }

   uint32_t numCases() { return _numCases; }
   OperationBuilder *setNumCases(uint32_t n)
      {
      _numCases = n;
      _cases = new Case *[n];
      for (auto i = 0;i < n;i++)
         _cases[i] = NULL;
      _currentCaseIndex = 0;
      return this;
      }
   OperationBuilder *addCase(Case *c)
      {
      assert(_currentCaseIndex < _numCases);
      _cases[_currentCaseIndex] = c;
      return this;
      }

   uint32_t numLiterals() { return _numLiterals; }
   OperationBuilder *setNumLiterals(uint32_t n)
      {
      _numLiterals = n;
      _literals = new LiteralValue *[n];
      for (auto i = 0;i < n;i++)
         _literals[i] = NULL;
      _currentLiteralIndex = 0;
      return this;
      }
   OperationBuilder *addLiteral(LiteralValue *l)
      {
      assert(_currentLiteralIndex < _numLiterals);
      _literals[_currentLiteralIndex] = l;
      return this;
      }

   uint32_t numOperands() { return _numOperands; }
   OperationBuilder *setNumOperands(uint32_t n)
      {
      _numOperands = n;
      _operands = new Value *[n];
      for (auto i = 0;i < n;i++)
         _operands[i] = NULL;
      _currentOperandIndex = 0;
      return this;
      }
   OperationBuilder *addOperand(Value *v)
      {
      assert(_currentOperandIndex < _numOperands);
      _operands[_currentOperandIndex] = v;
      return this;
      }

   uint32_t numSymbols() { return _numSymbols; }
   OperationBuilder *setNumSymbols(uint32_t n)
      {
      _numSymbols = n;
      _symbols = new Symbol *[n];
      for (auto i = 0;i < n;i++)
         _symbols[i] = NULL;
      _currentSymbolIndex = 0;
      return this;
      }
   OperationBuilder *addSymbol(Symbol *s)
      {
      assert(_currentSymbolIndex < _numSymbols);
      _symbols[_currentSymbolIndex] = s;
      return this;
      }

   uint32_t numTypes() { return _numTypes; }
   OperationBuilder *setNumTypes(uint32_t n)
      {
      _numTypes = n;
      _types = new Type *[n];
      for (auto i = 0;i < n;i++)
         _types[i] = NULL;
      return this;
      }
   OperationBuilder *addType(Type *t)
      {
      assert(_currentTypeIndex < _numTypes);
      _types[_currentTypeIndex] = t;
      return this;
      }

   OperationBuilder *setExpander(OperationExpander *expander) { _expander = expander; return this; }

   OperationBuilder *setPrinter(OperationPrinter *printer) { _printer = printer; return this; }

   OperationBuilder *setRegistrar(OperationRegistrar *registrar) { _registrar = registrar; return this; }

   protected:

   Action _action;

   uint32_t _numResults;
   uint32_t _currentResultTypeIndex;
   Type **_resultTypes;
   uint32_t _currentResultIndex;
   Value **_results;

   uint32_t _numBuilders;
   uint32_t _currentBuilderIndex;
   Builder **_builders;

   uint32_t _numCases;
   uint32_t _currentCaseIndex;
   Case **_cases;

   uint32_t _numLiterals;
   uint32_t _currentLiteralIndex;
   LiteralValue **_literals;

   uint32_t _numOperands;
   uint32_t _currentOperandIndex;
   Value **_operands;

   uint32_t _numSymbols;
   uint32_t _currentSymbolIndex;
   Symbol **_symbols;

   uint32_t _numTypes;
   uint32_t _currentTypeIndex;
   Type **_types;

   OperationExpander *_expander;
   OperationPrinter *_printer;
   OperationRegistrar *_registrar;

   static uint32_t nextDynamicAction;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // !defined(DYNAMICOPERATION_INCL)
