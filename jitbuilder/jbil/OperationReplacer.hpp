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

#ifndef OPERATIONREPLACER_INCL
#define OPERATIONREPLACER_INCL

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

class OperationReplacer
   {
   public:
   OperationReplacer(Operation *op);
   ~OperationReplacer();

   void setBuilder(Builder *b)
      {
      _builder = b;
      }

   void setResultMapper(ValueMapper *m, uint32_t i=0)
      {
      if (i < _numResults)
         _resultMappers[i] = m;
      }

   void setOperandMapper(ValueMapper *m, uint32_t i=0)
      {
      if (i < _numOperands)
         _operandMappers[i] = m;
      }

   void setBuilderMapper(BuilderMapper *m, uint32_t i=0)
      {
      if (i < _numBuilders)
         _builderMappers[i] = m;
      }

   void setLiteralMapper(LiteralMapper *m, uint32_t i=0)
      {
      if (i < _numLiterals)
         _literalMappers[i] = m;
      }

   void setSymbolMapper(SymbolMapper *m, uint32_t i=0)
      {
      if (i < _numSymbols)
         _symbolMappers[i] = m;
      }

   void setTypeMapper(TypeMapper *m, uint32_t i=0)
      {
      if (i < _numTypes)
         _typeMappers[i] = m;
      }

   void setExplodedTypes(std::set<Type *> *explodedTypes)
      {
      _explodedTypes = explodedTypes;
      }

   Builder *builder() const     { return _builder; }
   Operation *operation() const { return _op; }
   uint32_t numResults() const  { return _numResults; }
   ValueMapper *resultMapper(uint32_t i=0) const
      {
      if (i < _numResults) return _resultMappers[i];
      return NULL;
      }
   uint32_t numOperands() const  { return _numOperands; }
   ValueMapper *operandMapper(uint32_t i=0) const
      {
      if (i < _numOperands) return _operandMappers[i];
      return NULL;
      }
   uint32_t numBuilders() const  { return _numBuilders; }
   BuilderMapper *builderMapper(uint32_t i=0) const
      {
      if (i < _numBuilders) return _builderMappers[i];
      return NULL;
      }
   uint32_t numLiterals() const  { return _numLiterals; }
   LiteralMapper *literalMapper(uint32_t i=0) const
      {
      if (i < _numLiterals) return _literalMappers[i];
      return NULL;
      }
   uint32_t numSymbols() const  { return _numSymbols; }
   SymbolMapper *symbolMapper(uint32_t i=0) const
      {
      if (i < _numSymbols) return _symbolMappers[i];
      return NULL;
      }
   uint32_t numTypes() const  { return _numTypes; }
   TypeMapper *typeMapper(uint32_t i=0) const
      {
      if (i < _numTypes) return _typeMappers[i];
      return NULL;
      }
   const std::set<Type *> * explodedTypes() const { return _explodedTypes; }

   private:

   Builder          * _builder;
   Operation        * _op;

   uint32_t           _numResults;
   ValueMapper     ** _resultMappers;

   uint32_t           _numOperands;
   ValueMapper     ** _operandMappers;

   uint32_t           _numBuilders;
   BuilderMapper   ** _builderMappers;

   uint32_t           _numLiterals;
   LiteralMapper   ** _literalMappers;

   uint32_t           _numSymbols;
   SymbolMapper    ** _symbolMappers;

   uint32_t           _numTypes;
   TypeMapper      ** _typeMappers;

   std::set<Type *> * _explodedTypes;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(OPERATIONREPLACER_INCL)
