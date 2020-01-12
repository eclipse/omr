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

#ifndef OPERATIONCLONER_INCL
#define OPERATIONCLONER_INCL

#include <stddef.h>
#include <stdint.h>
#include <string>
#include "Mapper.hpp"

namespace OMR
{

namespace JitBuilder
{

class Builder;
class LiteralValue;
class Operation;
class Symbol;
class Type;
class Value;

class OperationCloner
   {
   public:
   OperationCloner(Operation *op);
   ~OperationCloner();

   void init();
   void reset();

   OperationCloner *changeOperand(Value *v, uint32_t i=0)
      {
      if (i < _numOperands)
         _operands[i] = v;
      return this;
      }

   OperationCloner *changeType(Type *t, uint32_t i=0)
      {
      if (i < _numTypes)
         _types[i] = t;
      return this;
      }

   OperationCloner *changeLiteral(LiteralValue *v, uint32_t i=0)
      {
      if (i < _numLiterals)
         _literals[i] = v;
      return this;
      }

   OperationCloner *changeSymbol(Symbol *s, uint32_t i=0)
      {
      if (i < _numSymbols)
         _symbols[i] = s;
      return this;
      }

   OperationCloner *changeBuilder(Builder *b, uint32_t i=0)
      {
      if (i < _numBuilders)
         _builders[i] = b;
      return this;
      }

   Operation *clone(Builder *b);
   Operation *cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMapppers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers);

   uint32_t numResults() const
      {
      return _numResults;
      }
   Value *result(uint32_t i=0) const
      {
      if (i < _numResults) return _results[i];
      return NULL;
      }
   void changeResult(Value *v, uint32_t i=0)
      {
      if (i < _numResults) _results[i] = v;
      }
   void createResult(Builder *b, uint32_t i=0);

   uint32_t numOperands() const
      {
      return _numOperands;
      }
   Value *operand(uint32_t i=0) const
      {
      if (i < _numOperands) return _operands[i];
      return NULL;
      }

   uint32_t numTypes() const
      {
      return _numTypes;
      }
   Type *type(uint32_t i=0) const
      {
      if (i < _numTypes) return _types[i];
      return NULL;
      }

   uint32_t numLiterals() const
      {
      return _numLiterals;
      }
   LiteralValue *literal(uint32_t i=0) const
      {
      if (i < _numLiterals) return _literals[i];
      return NULL;
      }

   uint32_t numSymbols() const
      {
      return _numSymbols;
      }
   Symbol *symbol(uint32_t i=0) const
      {
      if (i < _numSymbols) return _symbols[i];
      return NULL;
      }

   uint32_t numBuilders() const
      {
      return _numBuilders;
      }
   Builder *builder(uint32_t i=0) const
      {
      if (i < _numBuilders) return _builders[i];
      return NULL;
      }

   private:

   Operation *_op;

   int32_t _numResults;
   Value **_results;

   int32_t _numOperands;
   Value **_operands;

   int32_t _numTypes;
   Type **_types;

   int32_t _numLiterals;
   LiteralValue **_literals;

   int32_t _numSymbols;
   Symbol **_symbols;

   int32_t _numBuilders;
   Builder **_builders;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(OPERATIONCLONER_INCL)
