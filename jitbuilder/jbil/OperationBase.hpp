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

#ifndef OPERATIONBASE_INCL
#define OPERATIONBASE_INCL

#include <stdint.h>
#include <string>
#include <vector>
#include "Action.hpp"
#include "Iterator.hpp"
#include "Object.hpp"
#include "Symbol.hpp"

namespace OMR
{

namespace JitBuilder
{

class Builder;
class Case;
class LiteralValue;
class Location;
class Operation;
class OperationCloner;
class Type;
class TypeDictionary;
class TypeGraph;
class Value;


class OperationBase : public Object
   {
   friend class Builder;
   friend class BuilderBase;
   friend class Transformer;

   public:
   int64_t id() const                                  { return _index; }
   Action action() const                               { return _action; }
   Builder * parent() const                            { return _parent; }
   Location * location() const                         { return _location; }

   virtual LiteralIterator LiteralsBegin()             { return LiteralIterator(); }
           LiteralIterator &LiteralsEnd()              { return literalEndIterator; }
   virtual int32_t numLiterals() const                 { return 0; }
   virtual LiteralValue * literal(int i=0) const       { assert(0); return 0; }

   virtual SymbolIterator SymbolsBegin()               { return SymbolIterator(); }
           SymbolIterator &SymbolsEnd()                { return symbolEndIterator; }
   virtual int32_t numSymbols() const                  { return 0; }
   virtual Symbol *symbol(int i=0) const               { assert(0); return 0; }

   virtual ValueIterator OperandsBegin()               { return ValueIterator(); }
           ValueIterator &OperandsEnd()                { return valueEndIterator; }
   virtual int32_t numOperands() const                 { return 0; }
   virtual Value * operand(int i=0) const              { return NULL; }

   virtual ValueIterator ResultsBegin()                { return ValueIterator(); }
           ValueIterator &ResultsEnd()                 { return valueEndIterator; }
   virtual int32_t numResults() const                  { return 0; }
   virtual Value * result(int i=0) const               { return NULL; }
 
   virtual SymbolIterator ReadSymbolsBegin()           { return SymbolIterator(); }
           SymbolIterator ReadSymbolsEnd()             { return symbolEndIterator; }
   virtual int32_t numReadSymbols() const              { return 0; }
   virtual Symbol * readSymbol(int i=0) const          { return NULL; }

   virtual SymbolIterator WrittenSymbolsBegin()        { return SymbolIterator(); }
           SymbolIterator WrittenSymbolsEnd()          { return symbolEndIterator; }
   virtual int32_t numWrittenSymbols() const           { return 0; }
   virtual Symbol * writtenSymbol(int i=0) const       { return NULL; }

   virtual BuilderIterator BuildersBegin()             { return BuilderIterator(); }
           BuilderIterator &BuildersEnd()              { return builderEndIterator; }
   virtual int32_t numBuilders() const                 { return 0; }
   virtual Builder *builder(int i=0) const             { return NULL; }

   virtual CaseIterator CasesBegin()                   { return CaseIterator(); }
   virtual CaseIterator CasesEnd()                     { return CaseIterator(); }
   virtual int32_t numCases() const                    { return 0; }
   virtual Case * getCase(int i=0) const               { return NULL; }

   virtual TypeIterator TypesBegin()                   { return TypeIterator(); }
           TypeIterator &TypesEnd()                    { return typeEndIterator; }
   virtual int32_t numTypes() const                    { return 0; }
   virtual Type * type(int i=0) const                  { return NULL; }

   // deprecated
   virtual Operation * clone(Builder *b, Value **results) const = 0;
   virtual Operation * clone(Builder *b, Value **results, Value **operands, Builder **builders) const = 0;
   virtual void cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMappers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const = 0;

   // the new clone API
   virtual Operation * clone(Builder *b, OperationCloner *cloner) const = 0;

protected:
   OperationBase(Action a, Builder * parent);

   Operation * setParent(Builder * newParent);
   Operation * setLocation(Location *location);

   int64_t                  _index;
   Builder                * _parent;
   Action                   _action;
   Location               * _location;

   static int64_t globalIndex;
   static BuilderIterator builderEndIterator;
   static LiteralIterator literalEndIterator;
   static SymbolIterator symbolEndIterator;
   static TypeIterator typeEndIterator;
   static ValueIterator valueEndIterator;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(OPERATIONBASE_INCL)
