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

#include <stdint.h>
#include "Builder.hpp"
#include "Case.hpp"
#include "DynamicOperation.hpp"
#include "FunctionBuilder.hpp"
#include "LiteralValue.hpp"
#include "Location.hpp"
#include "Operation.hpp"
#include "OperationCloner.hpp"
#include "TypeDictionary.hpp"
#include "TypeGraph.hpp"
#include "Value.hpp"

using namespace OMR::JitBuilder;

namespace OMR
{
namespace JitBuilder
{

uint32_t OperationBuilder::nextDynamicAction = NumStaticActions;

DynamicOperation *
OperationBuilder::createOperation(Builder *parent)
   {
   DynamicOperation *newOp = new DynamicOperation(_action, parent,
                                                  _numResults, _results,
                                                  _numBuilders, _builders,
                                                  _numCases, _cases,
                                                  _numLiterals, _literals,
                                                  _numOperands, _operands,
                                                  _numSymbols, _symbols,
                                                  _numTypes, _types,
                                                  _expander,
                                                  _printer);

   // prepare for next Operation to build
   this->setNumResults(_numResults)
       ->setNumBuilders(_numBuilders)
       ->setNumCases(_numCases)
       ->setNumLiterals(_numLiterals)
       ->setNumOperands(_numOperands)
       ->setNumSymbols(_numSymbols)
       ->setNumTypes(_numTypes);

   return newOp;
   }

Operation *
DynamicOperation::clone(Builder *b, Value **results) const
   {
   DynamicOperation *newOp = new DynamicOperation(_action, b,
                                                  _numResults, results,
                                                  _numBuilders, _builders,
                                                  _numCases, _cases,
                                                  _numLiterals, _literals,
                                                  _numOperands, _operands,
                                                  _numSymbols, _symbols,
                                                  _numTypes, _types,
                                                  _expander,
                                                  _printer);
   return newOp;
   }

Operation *
DynamicOperation::clone(Builder *b, Value **results, Value **operands, Builder **builders) const
   {
   DynamicOperation *newOp = new DynamicOperation(_action, b,
                                                  _numResults, results,
                                                  _numBuilders, builders,
                                                  _numCases, _cases,
                                                  _numLiterals, _literals,
                                                  _numOperands, operands,
                                                  _numSymbols, _symbols,
                                                  _numTypes, _types,
                                                  _expander,
                                                  _printer);
   return newOp;
   }

void
DynamicOperation::cloneTo(Builder *b, ValueMapper **resultMappers, ValueMapper **operandMappers, TypeMapper **typeMappers, LiteralMapper **literalMappers, SymbolMapper **symbolMappers, BuilderMapper **builderMappers) const
   {
   OperationBuilder bldr;
   bldr.setAction(_action)
       ->setNumResults(_numResults)
       ->setNumBuilders(_numBuilders)
       ->setNumCases(_numCases)
       ->setNumLiterals(_numLiterals)
       ->setNumOperands(_numOperands)
       ->setNumSymbols(_numSymbols)
       ->setNumTypes(_numTypes)
       ->setExpander(_expander)
       ->setPrinter(_printer);

   for (auto i=0;i < _numResults;i++)
      bldr.addResultType(result(i)->type());
   for (auto i=0;i < _numBuilders;i++)
      bldr.addBuilder(builderMappers[i]->next());
   for (auto i=0;i < _numCases;i++)
      bldr.addCase(_cases[i]);
   for (auto i=0;i < _numLiterals;i++)
      bldr.addLiteral(literalMappers[i]->next());
   for (auto i=0;i < _numOperands;i++)
      bldr.addOperand(operandMappers[i]->next());
   for (auto i=0;i < _numSymbols;i++)
      bldr.addSymbol(symbolMappers[i]->next());
   for (auto i=0;i < _numTypes;i++)
      bldr.addType(typeMappers[i]->next());

   Operation *newOp = b->Append(&bldr);

   for (auto i=0;i < _numResults;i++)
      resultMappers[i]->add(newOp->result(i));
   }

Operation *
DynamicOperation::clone(Builder *b, OperationCloner *cloner) const
   {
   OperationBuilder bldr;
   bldr.setAction(_action)
       ->setNumResults(_numResults)
       ->setNumBuilders(_numBuilders)
       ->setNumCases(_numCases)
       ->setNumLiterals(_numLiterals)
       ->setNumOperands(_numOperands)
       ->setNumSymbols(_numSymbols)
       ->setNumTypes(_numTypes)
       ->setExpander(_expander)
       ->setPrinter(_printer);

   for (auto i=0;i < _numResults;i++)
      bldr.addResultType(result(i)->type());
   for (auto i=0;i < _numBuilders;i++)
      bldr.addBuilder(cloner->builder(i));
   for (auto i=0;i < _numCases;i++)
      bldr.addCase(_cases[i]);
   for (auto i=0;i < _numLiterals;i++)
      bldr.addLiteral(cloner->literal(i));
   for (auto i=0;i < _numOperands;i++)
      bldr.addOperand(operand(i));
   for (auto i=0;i < _numSymbols;i++)
      bldr.addSymbol(symbol(i));
   for (auto i=0;i < _numTypes;i++)
      bldr.addType(type(i));

   Operation *newOp = b->Append(&bldr);

   for (auto i=0;i < _numResults;i++)
      cloner->changeResult(newOp->result(i), i);

   return newOp;
   }

} // namespace JitBuilder

} // namespace OMR