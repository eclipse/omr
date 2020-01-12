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

#ifndef SYMBOL_INCL
#define SYMBOL_INCL

#include <string>
#include <vector>
#include "Type.hpp"

namespace OMR
{

namespace JitBuilder
{

class Symbol
   {
   public:
   static Symbol *create(std::string name, Type * type) { return new Symbol(name, type); }

   std::string name() const { return _name; }
   Type * type() const      { return _type; }
   uint64_t id()   const    { return _id; }

   virtual bool isLocal() const     { return false; }
   virtual bool isParameter() const { return false; }
   virtual bool isField() const     { return false; }
   virtual bool isFunction() const  { return false; }

   static uint64_t maxID() { return globalIndex; }

   protected:
   Symbol(std::string name, Type * type)
      : _name(name)
      , _type(type)
      , _id(globalIndex++)
      { }
   std::string _name;
   Type      * _type;
   uint64_t    _id;

   static int64_t globalIndex;
   };

class LocalSymbol : public Symbol
   {
   public:
   static LocalSymbol *create(std::string name, Type * type) { return new LocalSymbol(name, type); }

   virtual bool isLocal() const { return true; }

   protected:
   LocalSymbol(std::string name, Type * type)
      : Symbol(name, type)
      { }
   };

class ParameterSymbol : public LocalSymbol
   {
   public:
   static ParameterSymbol *create(std::string name, Type * type, int index) { return new ParameterSymbol(name, type, index); }

   int index() const { return _index; }

   virtual bool isParameter() const { return true; }

   protected:
   ParameterSymbol(std::string name, Type * type, int index)
      : LocalSymbol(name, type)
      , _index(index)
      { }

   int _index;
   };

class FunctionSymbol : public Symbol
   {
   public:
   static FunctionSymbol *create(FunctionType *type, std::string name, std::string fileName, std::string lineNumber, void *entryPoint)
      {
      return new FunctionSymbol(type, name, fileName, lineNumber, entryPoint);
      }

   FunctionType *functionType() const { return static_cast<FunctionType *>(_type); }
   std::string fileName() const       { return _fileName; }
   std::string lineNumber() const     { return _lineNumber; }
   void *entryPoint() const           { return _entryPoint; }

   virtual bool isFunction() const { return true; }

   protected:
   FunctionSymbol(FunctionType *type, std::string name, std::string fileName, std::string lineNumber, void *entryPoint)
      : Symbol(name, type)
      , _fileName(fileName)
      , _lineNumber(lineNumber)
      , _entryPoint(entryPoint)
      { }

   std::string _fileName;
   std::string _lineNumber;
   void *_entryPoint;
   };

class FieldSymbol : public Symbol
   {
   public:
   static FieldSymbol *create(std::string name, StructType *structType, FieldType *fieldType)
      {
      return new FieldSymbol(name, structType, fieldType);
      }

   StructType *structType() const { return _structType; }
   FieldType *fieldType() const   { return _fieldType; }

   virtual bool isLocal() const   { return false; }
   virtual bool isField() const   { return false; }

   protected:
   FieldSymbol(std::string name, StructType *structType, FieldType *fieldType)
      : Symbol(name, fieldType->type())
      , _structType(structType)
      , _fieldType(fieldType)
      { }
   StructType *_structType;
   FieldType *_fieldType;
   };
   

typedef std::vector<Symbol *>                    SymbolVector;

typedef std::vector<LocalSymbol *>               LocalSymbolVector;
typedef std::vector<LocalSymbol *>::iterator     LocalSymbolIterator;

typedef std::vector<ParameterSymbol *>           ParameterSymbolVector;
typedef std::vector<ParameterSymbol *>::iterator ParameterSymbolIterator;

typedef std::vector<FunctionSymbol *>            FunctionSymbolVector;
typedef std::vector<FunctionSymbol *>::iterator  FunctionSymbolIterator;

} // namespace JitBuilder

} // namespace OMR

#endif // defined(SYMBOL_INCL)
