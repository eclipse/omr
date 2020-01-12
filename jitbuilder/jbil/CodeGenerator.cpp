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

#include "CodeGenerator.hpp"
#include "Builder.hpp"
#include "FunctionBuilder.hpp"
#include "Location.hpp"
#include "Operation.hpp"
#include "TextWriter.hpp"
#include "Type.hpp"
#include "Value.hpp"

#include "ilgen/IlBuilder.hpp"
#include "ilgen/IlType.hpp"
#include "ilgen/IlValue.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "env/FrontEnd.hpp"
#include "env/TRMemory.hpp"

using namespace OMR::JitBuilder;

CodeGenerator::CodeGenerator(FunctionBuilder * fb, TR::MethodBuilder * mb)
   : Transformer(fb)
   , _mb(mb)
   {
   setTraceEnabled(fb->config()->traceCodeGenerator());
   }

TR::IlBuilder *
CodeGenerator::mapBuilder(Builder * b)
   {
   if (b == NULL)
      return NULL;
   if (_builders.find(b->id()) == _builders.end())
      storeBuilder(b, mapBuilder(b->parent())->OrphanBuilder());
   return _builders[b->id()];
   }

void
CodeGenerator::storeBuilder(Builder * b, TR::IlBuilder *omr_b)
   {
   _builders[b->id()] = omr_b;
   }

TR::IlType *
CodeGenerator::mapType(Type * t)
   {
   return _types[t->id()];
   }

TR::IlType *
CodeGenerator::mapPointerType(TR::TypeDictionary * types, PointerType * t)
   {
   if (_types.find(t->id()) != _types.end())
      return mapType(t);

   Type * baseType = t->BaseType();
   TR::IlType *baseIlType;
   if (baseType->isPointer())
      baseIlType = mapPointerType(types, (PointerType *)baseType);
   else
      baseIlType = mapType(baseType);

   TR::IlType *ptrIlType;
   if (baseIlType)
      ptrIlType = types->PointerTo(baseIlType);

   return ptrIlType;
   }

// mapStructFields is designed to iterate (recursively) through the fields of a struct/union,
// defining the fields of that struct in the `types` dictionary. If a field has a struct or
// union type, then the fields of that struct/union are also walked and added directly to
// *this* structure. That means "inner" structs/unions are inlined into the current struct.
//
// structName/fieldName are the names for the entire structure (field names are assembled to
//     include containing struct/union names)
// sType/fType are the struct and field types for this specific struct
// As mapStructFields recursively visits inner types, structName/fieldName will stay the same,
//     but sType/fType will reflect each inner type in succession
TR::IlType *
CodeGenerator::mapStructFields(TR::TypeDictionary * types, StructType * sType, char *structName, std::string fNamePrefix, size_t baseOffset)
   {
   for (auto fIt = sType->FieldsBegin(); fIt != sType->FieldsEnd(); fIt++)
      {
      FieldType *fType = fIt->second;
      std::string fieldName = fNamePrefix + fType->name();
      const char *fieldString = findOrCreateString(fieldName);
      size_t fieldOffset = baseOffset + fType->offset();

      if (fType->isStruct() || fType->isUnion())
         {
         // define a "dummy" field corresponding to the struct field itself, so we can ask for its address easily
         // in case this field's struct needs to be passed to anything
         _mb->typeDictionary()->DefineField(structName, fieldString, types->NoType, fieldOffset/8);
         StructType *innerStructType = static_cast<StructType *>(fType->type());
         mapStructFields(types, innerStructType, structName, fieldName + ".", fieldOffset);
         }
      else
         _mb->typeDictionary()->DefineField(structName, fieldString, mapType(fType->type()), fieldOffset/8); // JB1 uses bytes offsets
      }
   return mapType(sType);
   }

void
CodeGenerator::storeType(Type * t, TR::IlType *omr_t)
   {
   _types[t->id()] = omr_t;
   }

TR::IlValue *
CodeGenerator::mapValue(Value * v)
   {
   return _values[v->id()];
   }

void
CodeGenerator::storeValue(Value * v, TR::IlValue *omr_v)
   {
   _values[v->id()] = omr_v;
   }

#define MAP_CASE(omr_b,c) (reinterpret_cast<TR::IlBuilder::JBCase *>(mapCase((omr_b),(c))))

void *
CodeGenerator::mapCase(TR::IlBuilder *omr_b, Case *c)
   {
   if (_cases.find(c->id()) == _cases.end())
      {
      TR::IlBuilder *omr_target = mapBuilder(c->builder());
      TR::IlBuilder::JBCase *jbCase = omr_b->MakeCase(c->value(), &omr_target, c->fallsThrough());
      _cases[c->id()] = jbCase;
      return jbCase;
      }
   return _cases[c->id()];
   }

char *
CodeGenerator::findOrCreateString(std::string str)
   {
   if (_strings.find(str) != _strings.end())
      return _strings[str];

   char *s = new char[str.length()+1];
   strcpy(s, str.c_str());
   _strings[str] = s;
   return s;
   }

void
OMR::JitBuilder::CodeGenerator::printAllMaps()
   {
   TextWriter * pLog = _fb->logger(traceEnabled());
   if (pLog)
      {
      TextWriter & log = *pLog;

      log << "[ printAllMaps" << log.endl();
      log.indentIn();

      log.indent() << "[ Builders" << log.endl();
      log.indentIn();
      for (auto builderIt = _builders.cbegin(); builderIt != _builders.cend(); builderIt++)
         {
         log.indent() << "[ builder " << builderIt->first << " -> TR::IlBuilder " << (int64_t *)(void *) builderIt->second << " ]" << log.endl();
         }
      log.indentOut();
      log.indent() << "]" << log.endl();

      log.indent() << "[ Values" << log.endl();
      log.indentIn();
      for (auto valueIt = _values.cbegin(); valueIt != _values.cend(); valueIt++)
         {
         log.indent() << "[ value " << valueIt->first << " -> TR::IlValue " << (int64_t *)(void *) valueIt->second << " ]" << log.endl();
         }
      log.indentOut();
      log.indent() << "]" << log.endl();

      log.indent() << "[ Types" << log.endl();
      log.indentIn();
      for (auto typeIt = _types.cbegin(); typeIt != _types.cend(); typeIt++)
         {
         log.indent() << "[ type " << typeIt->first << " -> TR::IlType " << (int64_t *)(void *) typeIt->second << " ]" << log.endl();
         }
      log.indentOut();
      log.indent() << "]" << log.endl();

      log.indentOut();
      log.indent() << "]" << log.endl();
      }
   }

void
OMR::JitBuilder::CodeGenerator::generateFunctionAPI(FunctionBuilder *fb)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   if (log) log->indent() << "CodeGenerator::generateFunctionAPI F" << fb->id() << log->endl();

   TypeDictionary *types = fb->dict();
   TR::TypeDictionary * typesJB1 = _mb->typeDictionary();
   _typeDictionaries[types->id()] = typesJB1;

   // First, make sure all base types have mapped TR::IlType's
   //    in this pass, we skip Pointer types and only define Structs and Unions (without iterating their fields)
   //    Note that unions are converted to Structs in JitBuilder1 (doesn't use JB1's DefineUnion/UnionField services)
   storeType(types->NoType,  typesJB1->NoType );
   storeType(types->Int8,    typesJB1->Int8   );
   storeType(types->Int16,   typesJB1->Int16  );
   storeType(types->Int32,   typesJB1->Int32  );
   storeType(types->Int64,   typesJB1->Int64  );
   storeType(types->Float,   typesJB1->Float  );
   storeType(types->Double,  typesJB1->Double );
   storeType(types->Address, typesJB1->Address);
   storeType(types->Word,    typesJB1->Word   );

   if (log) log->indent() << "First pass:" << log->endl();
   for (TypeIterator typeIt = types->TypesBegin(); typeIt != types->TypesEnd(); typeIt++)
      {
      Type * type = *typeIt;
      if (log) log->writeType(type);
      if (type->isStruct() || type->isUnion())
         {
         char *name = findOrCreateString(type->name());
         storeType(type, _mb->typeDictionary()->DefineStruct(name));
         }
      else if (type->isFunction())
         {
         // function types all map to Address in JitBuilder 1.0
         storeType(type, _mb->typeDictionary()->Address);
         }
      else if (type->isPointer() || type->isField())
         {
         // skip function and pointer types in this pass
         }
      else
         {
         // should be a primitive type; verify that the type has been mapped already
         //assert(mapType(type));
         }
      }

   // Second pass: map all Pointer types now that anything a Pointer can point to has been mapped
   for (TypeIterator typeIt = types->TypesBegin(); typeIt != types->TypesEnd(); typeIt++)
      {
      Type * type = *typeIt;
      if (type->isPointer())
         {
         TR::IlType *ptrIlType = mapPointerType(typesJB1, static_cast<PointerType *>(type));
         storeType(type, ptrIlType);
         }
      }

   // all basic types should have mappings now. what remains is to define the fields of
   // structs and unions to JB1 so that fields will be mapped to appropriate offsets
   // in this process, any inner structs/unions are inlined into containing struct

   // Third pass: revisit all Structs and Unions to iterate over field types to map them
   for (TypeIterator typeIt = types->TypesBegin(); typeIt != types->TypesEnd(); typeIt++)
      {
      Type * type = *typeIt;
      if (type->isStruct())
         {
         StructType *sType = static_cast<StructType *>(type);
         char * structName = findOrCreateString(sType->name());
         mapStructFields(typesJB1, sType, structName, std::string(""), 0);
         _mb->typeDictionary()->CloseStruct(structName, sType->size()/8);
         }
      else if (type->isUnion())
         {
         UnionType *uType = static_cast<UnionType *>(type);
         char * structName = findOrCreateString(uType->name());
         mapStructFields(typesJB1, uType, structName, std::string(""), 0);
         _mb->typeDictionary()->CloseStruct(structName, uType->size()/8);
         }
      }

   // All types should be represented in the JB1 layer now, and mapTypes should be set up for every
   // type in TypeDictionary
   for (TypeIterator typeIt = types->TypesBegin(); typeIt != types->TypesEnd(); typeIt++)
      {
      Type * type = *typeIt;
      assert(type->isField() || mapType(type) != NULL);
      }

   _methodBuilders[fb->id()] = _mb;
   storeBuilder(fb, _mb);

   _mb->DefineName(findOrCreateString(fb->name()));
   _mb->DefineFile(findOrCreateString(fb->fileName()));
   _mb->DefineLine(findOrCreateString(fb->lineNumber()));
   _mb->DefineReturnType(mapType(fb->getReturnType()));

   for (ParameterSymbolIterator paramIt = fb->ParametersBegin();paramIt != fb->ParametersEnd(); paramIt++)
      {
      const ParameterSymbol *parameter = *paramIt;
      char *paramName = findOrCreateString(parameter->name());
      _mb->DefineParameter(paramName, mapType(parameter->type()));
      }
   for (LocalSymbolIterator localIt = fb->LocalsBegin();localIt != fb->LocalsEnd();localIt++)
      {
      const LocalSymbol *symbol = *localIt;
      char *localName = findOrCreateString(symbol->name());
      _mb->DefineLocal(localName, mapType(symbol->type()));
      }
   for (FunctionSymbolIterator fnIt = fb->FunctionsBegin();fnIt != fb->FunctionsEnd();fnIt++)
      {
      const FunctionSymbol *symbol = *fnIt;
      const FunctionSymbol *fSym = static_cast<const FunctionSymbol *>(symbol);
      const FunctionType *fType = fSym->functionType();
      TR::IlType *parmTypes[fType->numParms()];
      for (int32_t p=0;p < fType->numParms();p++)
         parmTypes[p] = mapType(fType->parmType(p));
      _mb->DefineFunction(findOrCreateString(fType->name()),
                          findOrCreateString(fSym->fileName()),
                          findOrCreateString(fSym->lineNumber()),
                          fSym->entryPoint(),
                          mapType(fType->returnType()),
                          fType->numParms(),
                          parmTypes);
       }
   }

FunctionBuilder *
CodeGenerator::transformFunctionBuilder(FunctionBuilder *fb)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   if (log) log->indent() << "CodeGenerator transformFunctionBuilder F" << fb->id() << log->endl();
   if (log) log->indentIn();
   return NULL;
   }

Builder *
CodeGenerator::transformOperation(Operation * op)
   {
   Builder * b = op->parent();
   TR::IlBuilder *omr_b = mapBuilder(b);
   omr_b->setBCIndex(op->location()->bcIndex())->SetCurrentIlGenerator();
   switch (op->action())
      {
      case aConstInt8 :
         storeValue(op->result(), omr_b->ConstInt8(op->literal()->getInt8()));
         break;

      case aConstInt16 :
         storeValue(op->result(), omr_b->ConstInt16(op->literal()->getInt16()));
         break;

      case aConstInt32 :
         storeValue(op->result(), omr_b->ConstInt32(op->literal()->getInt32()));
         break;

      case aConstInt64 :
         storeValue(op->result(), omr_b->ConstInt64(op->literal()->getInt64()));
         break;

      case aConstFloat :
         storeValue(op->result(), omr_b->ConstFloat(op->literal()->getFloat()));
         break;

      case aConstDouble :
         storeValue(op->result(), omr_b->ConstDouble(op->literal()->getDouble()));
         break;

      case aConstAddress :
         {
         TR::IlValue *rv = omr_b->ConstAddress(op->literal()->getAddress());
         storeValue(op->result(),rv);
         }
         break;

      case aCoercePointer :
         {
         TR::IlValue *object = mapValue(op->operand());
         storeValue(op->result(), mapValue(op->operand()));
         }
         break;

      case aAdd :
         storeValue(op->result(), omr_b->Add(mapValue(op->operand(0)), mapValue(op->operand(1))));
         break;

      case aSub :
         storeValue(op->result(), omr_b->Sub(mapValue(op->operand(0)), mapValue(op->operand(1))));
         break;

      case aMul :
         storeValue(op->result(), omr_b->Mul(mapValue(op->operand(0)), mapValue(op->operand(1))));
         break;

      case aLoad :
         {
         Symbol *sym = op->symbol();
         if (sym->isFunction())
            {
            FunctionSymbol *fnSym = static_cast<FunctionSymbol *>(sym);
            storeValue(op->result(), omr_b->ConstAddress(reinterpret_cast<void *>(fnSym->entryPoint())));
            }
         else
            storeValue(op->result(), omr_b->Load(findOrCreateString(sym->name())));
         }
         break;

      case aLoadAt :
         {
         Value *v = op->operand();
         TR::IlValue *jb_v = mapValue(v);
         Type *t = op->type();
         TR::IlType *jb_t = mapType(op->type());
         Value *r = op->result();
         storeValue(r, omr_b->LoadAt(jb_t, jb_v));
         }
         break;

      case aLoadIndirect :
         {
         LoadIndirect *liOp = static_cast<LoadIndirect *>(op);
         FieldType *fieldType = liOp->getFieldType();
         const char *structName = findOrCreateString(fieldType->owningStruct()->name());
         const char *fieldName = findOrCreateString(fieldType->name());
         TR::IlValue *object = mapValue(op->operand());
         assert(object);
         storeValue(op->result(), omr_b->LoadIndirect(structName, fieldName, mapValue(op->operand())));
         }
         break;

      case aStore :
         omr_b->Store(findOrCreateString(op->symbol()->name()), mapValue(op->operand()));
         break;

      case aStoreAt :
         omr_b->StoreAt(mapValue(op->operand(0)), mapValue(op->operand(1)));
         break;

      case aStoreIndirect :
         {
         StoreIndirect *siOp = static_cast<StoreIndirect *>(op);
         FieldType *fieldType = siOp->getFieldType();
         const char *structName = findOrCreateString(fieldType->owningStruct()->name());
         const char *fieldName = findOrCreateString(fieldType->name());
         TR::IlValue *object = mapValue(op->operand(0));
         assert(object);
         TR::IlValue *value = mapValue(op->operand(1));
         assert(value);
         omr_b->StoreIndirect(structName, fieldName, object, value);
         }
         break;

      case aIndexAt :
         storeValue(op->result(), omr_b->IndexAt(mapType(op->type()), mapValue(op->operand(0)), mapValue(op->operand(1))));
         break;

      case aCall :
         {
         Call *callOp = static_cast<Call *>(op);
         FunctionType *fType = static_cast<FunctionType *>(callOp->function()->type());
         TR::IlValue *args[op->numOperands()];
         args[0] = mapValue(callOp->function());
         for (int32_t a=1;a < op->numOperands();a++)
            args[a] = mapValue(callOp->operand(a));

         if (op->result() != NULL)
            storeValue(op->result(), omr_b->ComputedCall(fType->name().c_str(), callOp->numOperands(), args));
         else
            omr_b->ComputedCall(fType->name().c_str(), callOp->numOperands(), args);
         }
         break;

      case aAppendBuilder :
         omr_b->AppendBuilder(mapBuilder(op->builder()));
         break;

      case aGoto :
         omr_b->Goto(mapBuilder(op->builder()));
         break;

      case aReturn :
         if (op->numOperands() > 0)
            omr_b->Return(mapValue(op->operand()));
         else
            omr_b->Return();
         break;

      case aIfCmpGreaterThan :
         omr_b->IfCmpGreaterThan(mapBuilder(op->builder()), mapValue(op->operand(0)), mapValue(op->operand(1)));
         break;

      case aIfCmpLessThan :
         omr_b->IfCmpLessThan(mapBuilder(op->builder()), mapValue(op->operand(0)), mapValue(op->operand(1)));
         break;

      case aIfCmpGreaterOrEqual :
         omr_b->IfCmpGreaterOrEqual(mapBuilder(op->builder()), mapValue(op->operand(0)), mapValue(op->operand(1)));
         break;

      case aIfCmpLessOrEqual :
         omr_b->IfCmpLessOrEqual(mapBuilder(op->builder()), mapValue(op->operand(0)), mapValue(op->operand(1)));
         break;

      case aIfThenElse :
         {
         TR::IlBuilder *omr_thenB = mapBuilder(op->builder(0));
         TR::IlBuilder *omr_elseB = mapBuilder(op->builder(1));
         omr_b->IfThenElse(&omr_thenB, &omr_elseB, mapValue(op->operand()));
         break;
         }

      case aForLoop :
         {
         TR::IlBuilder *omr_body = mapBuilder(op->builder(0));
         TR::IlBuilder *omr_break = mapBuilder(op->builder(1));
         TR::IlBuilder *omr_continue = mapBuilder(op->builder(2));
         omr_b->ForLoop(static_cast<bool>(op->literal()->getInt8()),
                        findOrCreateString(op->symbol()->name()),
                        &omr_body,
                        &omr_break,
                        &omr_continue,
                        mapValue(op->operand(0)),
                        mapValue(op->operand(1)),
                        mapValue(op->operand(2)));
         break;
         }

      case aSwitch :
         {
         TR::IlBuilder::JBCase *cases[op->numCases()];
         int32_t cNum = 0;
         for (auto cIt = op->CasesBegin(); cIt != op->CasesEnd(); cIt++)
            cases[cNum++] = MAP_CASE(omr_b, *cIt);
         TR::IlBuilder *omr_defaultTarget = mapBuilder(op->builder());
         omr_b->Switch(mapValue(op->operand()), &omr_defaultTarget, op->numCases(), cases);
         break;
         }

      case aCreateLocalArray :
         storeValue(op->result(), omr_b->CreateLocalArray(op->literal(0)->getInt32(), mapType(op->type())));
         break;

      case aCreateLocalStruct :
         storeValue(op->result(), omr_b->CreateLocalStruct(mapType(op->type())));
         break;

      default :
         assert(0); // unhandled action!!
         break;
      }

   return NULL;
   }

FunctionBuilder *
OMR::JitBuilder::CodeGenerator::transformFunctionBuilderAtEnd(FunctionBuilder * fb)
   {
   TextWriter *log = fb->logger(traceEnabled());
   if (log) log->indentOut();
   printAllMaps();
   return NULL;
   }
