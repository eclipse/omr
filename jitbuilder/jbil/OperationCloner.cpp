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

#include "OperationCloner.hpp"
#include "Operation.hpp"
#include "Value.hpp"

namespace OMR
{

namespace JitBuilder
{

OperationCloner::OperationCloner(Operation *op)
   : _op(op)
   , _numResults(op->numResults())
   , _results(NULL)
   , _numOperands(op->numOperands())
   , _operands(NULL)
   , _numTypes(op->numTypes())
   , _types(NULL)
   , _numLiterals(op->numLiterals())
   , _literals(NULL)
   , _numSymbols(op->numSymbols())
   , _symbols(NULL)
   , _numBuilders(op->numBuilders())
   , _builders(NULL)
   {
   init();
   reset();
   }

OperationCloner::~OperationCloner()
   {
   if (_results)
      delete[] _results;

   if (_operands)
      delete[] _operands;

   if (_types)
      delete[] _types;

   if (_literals)
      delete[] _literals;

   if (_symbols)
      delete[] _symbols;

   if (_builders)
      delete[] _builders;
   }

void
OperationCloner::init()
   {
   if (_numResults > 0)
      _results = new Value *[_numResults];

   if (_numOperands > 0)
      _operands = new Value *[_numOperands];

   if (_numTypes > 0)
      _types = new Type *[_numTypes];

   if (_numLiterals > 0)
      _literals = new LiteralValue *[_numLiterals];

   if (_numSymbols > 0)
      _symbols = new Symbol *[_numSymbols];

   if (_numBuilders > 0)
      _builders = new Builder *[_numBuilders];
   }

void
OperationCloner::reset()
   {
   for (int i=0;i < _numResults;i++)
      _results[i] = _op->result(i);
   for (int i=0;i < _numOperands;i++)
      _operands[i] = _op->operand(i);
   for (int i=0;i < _numTypes;i++)
      _types[i] = _op->type(i);
   for (int i=0;i < _numLiterals;i++)
      _literals[i] = _op->literal(i);
   for (int i=0;i < _numSymbols;i++)
      _symbols[i] = _op->symbol(i);
   for (int i=0;i < _numBuilders;i++)
      _builders[i] = _op->builder(i);
   }

void
OperationCloner::createResult(Builder *b, uint32_t i)
   {
   changeResult( Value::create(b, _op->result(i)->type()) );
   }

Operation *
OperationCloner::clone(Builder *b)
   {
   return _op->clone(b, this);
   }

Operation *
OperationCloner::cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMappers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers)
   {
   for (uint32_t i=0;i < _numOperands;i++)
      changeOperand(operandMappers[i]->next(), i);
   for (uint32_t i=0;i < _numTypes;i++)
      changeType(typeMappers[i]->next(), i);
   for (uint32_t i=0;i < _numLiterals;i++)
      changeLiteral(literalMappers[i]->next(), i);
   for (uint32_t i=0;i < _numSymbols;i++)
      changeSymbol(symbolMappers[i]->next(), i);
   for (uint32_t i=0;i < _numBuilders;i++)
      changeBuilder(builderMappers[i]->next(), i);

   Operation *clonedOp = clone(b);

   for (uint32_t i=0;i < _numResults;i++)
      resultMappers[i]->add( result(i) );

   return clonedOp;
   }
 
} // namespace JitBuilder

} // namespace OMR
