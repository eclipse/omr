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

#ifndef TYPEREPLACER_INCL
#define TYPEREPLACER_INCL


#include <map>
#include <set>
#include <vector>
#include "Transformer.hpp"
#include "Mapper.hpp"
#include "Type.hpp"

namespace OMR
{

namespace JitBuilder
{

class Builder;
class FieldType;
class FunctionBuilder;
class FunctionType;
class Operation;
class PointerType;
class StructType;
class Type;

class TypeReplacer : public Transformer
   {
public:
   TypeReplacer(FunctionBuilder * fb);

   // all references of oldType will be changed to newType
   // when transform() returns, there will be no remaining references to oldType
   // (though the type itself is not erased from the FunctionBuilder's TypeDictionary)
   TypeReplacer *replace(Type *oldType, Type *newType=NULL);

   // type must return a non-NULL layout()
   // when transform() returns, there will be no remaining references to type
   // (though the type itself is not erased from the FunctionBuilder's TypeDictionary)
   TypeReplacer *explode(Type *type);

   void transformTypes(TypeDictionary *dict);
   void finalCleanup(FunctionBuilder * fb);

protected:
   // override Transformer functions
   virtual FunctionBuilder * transformFunctionBuilder(FunctionBuilder * fb);
   virtual Builder * transformOperation(Operation * op);
   virtual FunctionBuilder * transformFunctionBuilderAtEnd(FunctionBuilder * fb);

   // overrideable functions from TypeReducer
   virtual void cloneOperation(Builder *b, Operation *op, int numMaps);

   // helper functions
   void recordMapper(Type *type, TypeMapper *mapper);
   void recordOriginalType(Type *type);
   Type * singleMappedType(Type *type);
   Type * mappedLayout(Type *t);
   std::string explodedName(std::string & name, FieldType *field);

   void explodeLayoutTypes(TypeDictionary *dict, StructType *layout, size_t baseOOffset, TypeMapper *m);
   void transformExplodedType(TypeDictionary *dict, Type *type);
   void transformPointerType(TypeDictionary *dict, PointerType *ptrType);
   void transformStructFields(TypeDictionary *dict, StructType *origStruct, StructType *structType, std::string baseName, size_t baseOffset, StructType *type, TypeMapper *mapper);
   void transformStructType(TypeDictionary *dict, StructType *sType);
   void transformFunctionType(TypeDictionary *dict, FunctionType *fnType);
   void transformLiteral(TypeDictionary *dict, LiteralValue *lv);
   void transformTypeIfNeeded(TypeDictionary *dict, Type *type);

   bool                                     _typesTransformed;

   std::set<Type *>                         _typesToRemove;

   std::set<TypeID>                         _typesToExplode;
   std::map<TypeID,TypeID>                  _typesToReplace;

   std::map<LiteralValue *,LiteralMapper *> _literalMappers;
   std::map<Symbol *,SymbolMapper *>        _symbolMappers;
   std::map<Type *,TypeMapper *>            _typeMappers;
   std::map<Value *,ValueMapper *>          _valueMappers;

   std::set<Type *>                         _explodedType;
   std::set<Type *>                         _modifiedType;
   std::set<Type *>                         _examinedType;

   // below are mappers that will be initialized according to the current operation
   size_t _mappedResultsSize;
   ValueMapper **_mappedResults;

   size_t _mappedOperandsSize;
   ValueMapper **_mappedOperands;

   size_t _mappedSymbolsSize;
   SymbolMapper **_mappedSymbols;

   size_t _mappedLiteralsSize;
   LiteralMapper **_mappedLiterals;

   size_t _mappedTypesSize;
   TypeMapper **_mappedTypes;

   size_t _mappedBuildersSize;
   BuilderMapper **_mappedBuilders;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(TYPEREPLACER_INCL)
