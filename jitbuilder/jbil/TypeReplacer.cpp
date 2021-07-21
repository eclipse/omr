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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "TypeReplacer.hpp"
#include "Builder.hpp"
#include "DynamicType.hpp"
#include "FunctionBuilder.hpp"
#include "Operation.hpp"
#include "OperationReplacer.hpp"
#include "Symbol.hpp"
#include "TextWriter.hpp"
#include "Type.hpp"
#include "TypeDictionary.hpp"
#include "Value.hpp"

using namespace OMR::JitBuilder;

// TypeReplacer is responsible for rewriting a FunctionBuilder according to a
// list of types to be replaced (with a corresponding list of replacement types)
// as well as "exploding" a list of types to be replaced by their layout types.
// Typically, user defined types are exploded before they can be generated. For
// example, a user defined Complex type might be exploded into two Double values.
// Upon creation, one can describe the types to be replaced/exploded by calling
// replaceType/explodeType on the TypeReplacer:
// e.g.
//    TypeReplacer *tr = new TypeReplacer(fb)
//                       ->explodeType(fb->Complex)
//                       ->explodeType(fb->Point3D)
//                       ->replaceType(fb-Double, fb->Float)
//                       ->replaceType(fb->T, fb->Int32);
//
// TypeReplacer uses Mappers to perform explosion and replacement: a Mapper provides
// a way to repetitively iterate through a Type's replacement Type or through the
// Types that result from exploding a Type's layout. By creating a specific Mapper
// for each element (operand, literal value, type, symbol) of an Operation, the
// Operation can be transformed into as many replacement Operations as needed by
// simply cloning it repeatedly from the Mappers built from the original Operation's
// elements, with each clone asking for the "next" element from each corresponding
// Mapper. Non-exploded elements will simply keep providing the same (possibly replaced)
// element type for each clone, whereas exploded elements will provide a different
// element for each clone of the Operation.
//
// To save space and time, TypeReplacer caches all Mapper objects created so they
// can be reused by later replacing/exploding similar kinds of elements. It begins by
// walking each Type to see if it references a Type that needs to be "transformed"
// (either replaced or exploded or built directly or indirectly from such a Type).
// The FunctionBuilder's signature elements (return value, parameters) are then
// processed, followed by an iteration over all the Operations in the FunctionBuilder.
//
// When an element (Value, LiteralValue, Type, Symbol) is first encountered while
// visiting the operations of the FunctionBuilder, that element is transformed to
// an appropriate Mapper object (ValueMapper, LiteralMapper, TypeMapper, SymbolMapper)
// and then recorded in a std::map from the original element to its Mapper object.
// The collection of Mapper objects is fed into the cloneTo() function of the original
// Operation to create new Operations. As these new Operations are created, if
// they produce result Values, these Values are added to a new ValueMapper which, once
// completely filled, is recorded into the ValueMapper cache so that subsequent
// uses of the original result Value will find the result ValueMapper object.
//
// A FunctionBuilder's signature (set of parameters) are also remapped in this
// way, so the resulting FunctionBuilder may take more parameters than the
// original FunctionBuilder (if any Types were exploded, for example). The
// same is true for all LocalSymbols (they may be exploded into multiple scalar
// values). Note that Type's cannot be exploded if they are referenced via a
// PointerType, so TypeReplacer replaces PointerTo(explodedType) with
// PointerTo(explodedType->layout())
//
// A type that "explodes" will be replaced with its layout fields where ever
// it is used. Similarly, structs that have a field defined as an exploded type
// will have their fields exploded into the containing struct. Parameters of an
// exploded Type will be replaced with multiple parameters from the fields of the
// layout struct. Local symbols are similarly expanded. Unions with a field of an
// exploded type will be replaced with the layout struct type.
//
// Types that do not directly refer to exploded or replaced Types may still be changed
// by this pass. For example, PointerTo(PointerTo(explodedType)) must be changed because
// PointerTo(explodedType) will be changed to a set of PointerTo types for each field
// of the exploded Type's layout, and therefore the original Type reference will also
// need to change to set of PointerTo(PointerTo(all fields of explodedType->layout()))
//


TypeReplacer::TypeReplacer(FunctionBuilder * fb)
   : Transformer(fb)
   , _typesTransformed(false)
   {
   setTraceEnabled(fb->config()->traceReducer());
   }

TypeReplacer *
TypeReplacer::replace(Type *oldType, Type *newType)
   {
   assert(newType != oldType || oldType->isStruct());
   _typesToReplace[oldType->id()] = newType->id();
   return this;
   }

TypeReplacer *
TypeReplacer::explode(Type *type)
   {
   assert(type->layout() != NULL);
   assert(type->layout()->size() == type->size());
   _typesToExplode.insert(type->id());
   return this;
   }

void
TypeReplacer::recordMapper(Type *type, TypeMapper *mapper)
   {
   _typeMappers[type] = mapper;
   TextWriter *log = _fb->logger(traceEnabled());
   if (log)
      {
      log->indent() << "type t" << type->id() << " mapper registered:" << log->endl();
      LOG_INDENT_REGION(log)
         {
         for (int i=0;i < mapper->size();i++)
            {
            Type *newType = mapper->current();
            if (log) log->indent() << i << " : " << "\"" << mapper->name() << "\"" << " offset " << mapper->offset() << " : ";
            if (log) log->writeType(newType, false);
            mapper->next();
            }
         }
      LOG_OUTDENT
      }
   }

void
TypeReplacer::recordOriginalType(Type *type)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   if (log) log->indent() << "type t" << type->id() << " unchanged" << log->endl();
   if (_typeMappers.find(type) == _typeMappers.end())
      {
      TypeMapper *m = new TypeMapper(type);
      recordMapper(type, m);
      }
   assert(_modifiedType.find(type) == _modifiedType.end());
   }

Type *
TypeReplacer::singleMappedType(Type *type)
   {
   auto it = _typeMappers.find(type);
   assert(it != _typeMappers.end());
   TypeMapper *m = it->second;
   assert(m->size() == 1); // should map only to a Struct
   return m->next();
   }

Type *
TypeReplacer::mappedLayout(Type *t)
   {
   Type *layout = t->layout();
   assert(layout);
   if (_modifiedType.find(layout) != _modifiedType.end())
      {
      layout = singleMappedType(layout);
      assert(layout->isStruct());
      }

   return layout;
   }

std::string
TypeReplacer::explodedName(std::string & baseName, FieldType *field)
   {
   LiteralValue *name = field->fieldName();
   if (baseName.length() > 0)
      return baseName + "." + name->getString();
   return name->getString();
   }

void
TypeReplacer::explodeLayoutTypes(TypeDictionary *dict, StructType *layout, size_t baseOffset, TypeMapper *m)
   {
   for (auto fIt = layout->FieldsBegin(); fIt != layout->FieldsEnd(); fIt++)
      {
      FieldType *fType = fIt->second;
      Type *t = fType->type();
      transformTypeIfNeeded(dict, t);

      size_t fieldOffset = baseOffset + fType->offset();
      if (_typesToExplode.find(t->id()) != _typesToExplode.end())
         {
         StructType *innerLayout = t->layout();
         assert(innerLayout);
         explodeLayoutTypes(dict, innerLayout, fieldOffset, m);
         }
      else
         {
         Type *mappedType = singleMappedType(t);
         LiteralValue *name = fType->fieldName();
         std::string fieldName = name->getString();
         if (name->kind() == T_typename)
            fieldName = mappedType->name();
         m->add(mappedType, fieldName, fieldOffset);
         }
      }
   }

void
TypeReplacer::transformExplodedType(TypeDictionary *dict, Type *type)
   {
   TypeMapper *m = new TypeMapper();
   StructType *layout = type->layout();
   assert(layout);

   explodeLayoutTypes(dict, layout, 0, m);

   _explodedType.insert(type);
   recordMapper(type, m);

   _typesToRemove.insert(type);

   // also explode the layout type itself, in case it has inner exploded types
   transformTypeIfNeeded(dict, layout);
   }

void
TypeReplacer::transformPointerType(TypeDictionary *dict, PointerType *ptrType)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   Type *baseType = ptrType->BaseType();
   Type *newBaseType = NULL;
   if (_explodedType.find(baseType) != _explodedType.end())
      newBaseType = mappedLayout(baseType);
   else
      newBaseType = singleMappedType(baseType);

   Type *newPtrType = dict->PointerTo(newBaseType);
   TypeMapper *ptrTypeMapper = new TypeMapper(newPtrType);
   _modifiedType.insert(ptrType);
   _examinedType.insert(newPtrType); // avoid looking at it again

   recordMapper(ptrType, ptrTypeMapper);
   LOG_INDENT_REGION(log)
      {
      recordOriginalType(newPtrType);
      }
   LOG_OUTDENT

   _typesToRemove.insert(ptrType);
   }

void
TypeReplacer::transformStructFields(TypeDictionary *dict, StructType *origStruct, StructType *structType, std::string baseName, size_t baseOffset, StructType *type, TypeMapper *mapper)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   bool removeFields = true;
   if (type == origStruct && _typesToRemove.find(origStruct) != _typesToRemove.end())
      removeFields = false;

   for (auto fIt = type->FieldsBegin(); fIt != type->FieldsEnd(); fIt++)
      {
      FieldType *fType = fIt->second;
      std::string fieldName = explodedName(baseName, fType);
      Type *t = fType->type();
      if (_typesToExplode.find(t->id()) != _typesToExplode.end())
         {
         TypeMapper *m = new TypeMapper();
         transformStructFields(dict, origStruct, structType, fieldName, fType->offset(), t->layout(), m);
         recordMapper(fType, m);
         }
      else
         {
         Type *mappedType = singleMappedType(t);
         LiteralValue *name = fType->fieldName();
         LiteralValue *newFieldName = NULL;
         if (name->kind() == T_typename)
            newFieldName = LiteralValue::create(dict, mappedType);
         else
            newFieldName = LiteralValue::create(dict, fieldName);
         FieldType *newType = dict->DefineField(structType, newFieldName, mappedType, baseOffset + fType->offset());
         _examinedType.insert(newType); // avoid looking at later
         recordMapper(fType, new TypeMapper(newType));
         if (mapper)
            mapper->add(newType);
         LOG_INDENT_REGION(log)
            {
            recordOriginalType(newType);
            }
         LOG_OUTDENT

         if (removeFields)
            _typesToRemove.insert(fType);
         }
      }
   }

void
TypeReplacer::transformStructType(TypeDictionary *dict, StructType *sType)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   if (log) log->indent() << "TransformStructType " << sType->name() << " because at least one field modified" << log->endl();

   std::string newName = std::string("_X_::") + sType->name();
   StructType *newType = NULL;
   if (sType->isStruct())
      newType = dict->DefineStruct(newName, sType->size());
   else if (sType->isUnion())
      newType = dict->DefineUnion(newName);
   else
      assert(0);

   // do this now so fields of sType don't need to add themselves for removal
   _typesToRemove.insert(sType);

   std::string baseName("");
   transformStructFields(dict, sType, newType, baseName, 0, sType, NULL);

   dict->CloseStruct(newType);
   _modifiedType.insert(sType);
   _examinedType.insert(newType); // avoid looking at later

   TypeMapper *m = new TypeMapper(newType);
   recordMapper(sType, m);
   LOG_INDENT_REGION(log)
      {
      recordOriginalType(newType);
      }
   LOG_OUTDENT
   }

void
TypeReplacer::transformFunctionType(TypeDictionary *dict, FunctionType *fnType)
   {
   Type *returnType = fnType->returnType();
   assert(_typesToExplode.find(returnType->id()) == _typesToExplode.end()); // can't explode return types yet
   Type *newReturnType = singleMappedType(returnType);

   // count how many parameters needed
   int32_t numParms = fnType->numParms();
   int32_t numNewParms = 0; // each original parameter explodes to at least one remapped parameter
   for (int32_t p=0;p < numParms;p++)
      {
      Type *parmType = fnType->parmType(p);
      auto it = _typeMappers.find(parmType);
      assert(it != _typeMappers.end());
      TypeMapper *parmMapper = it->second;
      numNewParms += parmMapper->size();
      }

   // allocate array and then assign the new parameter types
   Type **newParmTypes = new Type *[numNewParms];
   int parmNum = 0;
   for (int32_t p=0;p < numParms;p++)
      {
      Type *parmType = fnType->parmType(p);
      auto it = _typeMappers.find(parmType);
      assert(it != _typeMappers.end());
      TypeMapper *parmMapper = it->second;
      for (int i=0;i < parmMapper->size();i++)
         newParmTypes[parmNum++] = parmMapper->next();
      }

   assert(parmNum == numNewParms);

   FunctionType *newType = FunctionType::create(dict, fnType->name(), newReturnType, numNewParms, newParmTypes);
   _modifiedType.insert(fnType);
   _examinedType.insert(newType);

   TypeMapper *m = new TypeMapper(newType);
   recordMapper(fnType, m);
   recordOriginalType(newType);

   _typesToRemove.insert(fnType);
   }

void
TypeReplacer::transformTypeIfNeeded(TypeDictionary *dict, Type *type)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   if (log) log->writeType(type);

   uint64_t typeID = type->id();
   if (_examinedType.find(type) != _examinedType.end())
      return;

   _examinedType.insert(type);
   _modifiedType.erase(type);
   _explodedType.erase(type);

   LOG_INDENT_REGION(log)
      {
      // pointer to a type that has been transformed must be transformed to refer
      //   to the new base type
      if (type->isPointer())
         {
         PointerType *ptrType = static_cast<PointerType *>(type);
         Type *baseType = ptrType->BaseType();

         LOG_INDENT_REGION(log)
            {
            if (log) log->indent() << "PointerType base t" << baseType->id() << log->endl();

            transformTypeIfNeeded(dict, baseType);
            if (_modifiedType.find(baseType) != _modifiedType.end())
               transformPointerType(dict, ptrType);
            else
               recordOriginalType(type);
            }
         LOG_OUTDENT
         }

      else if (type->isField())
         {
         FieldType *fType = static_cast<FieldType *>(type);
         Type *fieldType = fType->type();
         LOG_INDENT_REGION(log)
            {
            if (log) log->indent() << "FieldType " << fType->name() << " type t" << fieldType->id() << log->endl();

            transformTypeIfNeeded(dict, fieldType);
            if (_modifiedType.find(fieldType) == _modifiedType.end())
               recordOriginalType(type); // ensure recorded
            else
               {
               // modified types are handled via struct/union types so just ignore here
               if (log) log->indent() << "modified field to be handled when struct is transformed" << log->endl();
               }
            }
         LOG_OUTDENT
         }

      // for structs/unions, look for any remapped field types (recursively!) and if we find one (doesn't matter what it maps to here)
      //   then construct a new struct/union with remapped fields
      else if (type->isStruct() || type->isUnion())
         {
         StructType *sType = static_cast<StructType *>(type);
         bool transform = false;

         LOG_INDENT_REGION(log)
            {
            if (log) log->indent() << "Struct/UnionType" << log->endl();

            for (auto fIt = sType->FieldsBegin(); fIt != sType->FieldsEnd(); fIt++)
               {
               FieldType *fType = fIt->second;
               if (log) log->indent() << "Examining field " << fType << " ( " << fType->name() << " )" << log->endl();

               transformTypeIfNeeded(dict, fType);
               if (_modifiedType.find(fType->type()) != _modifiedType.end())
                  transform = true;
               }

            if (transform)
               transformStructType(dict, sType);
            else
               recordOriginalType(type);
            }
         LOG_OUTDENT
         }

      // for functions, if return type or any parameter type needs to be changed (doesn't matter to what, here)
      //   then construct a new function type with new types
      else if (type->isFunction())
         {
         FunctionType *fnType = static_cast<FunctionType *>(type);
         bool transform = false;

         LOG_INDENT_REGION(log)
            {
            if (log) log->indent() << "FunctionType" << log->endl();

            Type *returnType = fnType->returnType();
            transformTypeIfNeeded(dict, returnType);

            if (_modifiedType.find(returnType) != _modifiedType.end())
               transform = true;

            for (int32_t p=0;p < fnType->numParms();p++)
               {
               Type *pType = fnType->parmType(p);
               transformTypeIfNeeded(dict, pType);

               if (_modifiedType.find(pType) != _modifiedType.end())
                  transform = true;
               }

            if (transform)
               transformFunctionType(dict, fnType);
            else
               recordOriginalType(type);
            }
         LOG_OUTDENT
         }

      //
      // User composite type handling here
      // BEGIN {

      // } END
      // User composite type handling here
      //

      else if (_typesToExplode.find(type->id()) != _typesToExplode.end())
         {
         transformExplodedType(dict, type);
         _modifiedType.insert(type);
         _explodedType.insert(type);
         }
      else
         {
         auto it = _typesToReplace.find(typeID);
         if (it != _typesToReplace.end())
            {
            Type *typeToReplace = dict->LookupType(it->second);
            TypeMapper *m = new TypeMapper(typeToReplace);
            recordMapper(type, m);
            _modifiedType.insert(type);
            }
         else
            recordOriginalType(type);
         }
      }
      LOG_OUTDENT
   }

void
TypeReplacer::transformTypes(TypeDictionary *dict)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   if (log)
      {
      log->indent() << "TypeReplacer::transformTypes " << dict << log->endl();
      log->writeDictionary(dict);
      (*log) << log->endl();
      log->indent() << "Types to explode:" << log->endl();
      LOG_INDENT_REGION(log)
         {
         for (auto it = dict->TypesBegin();it != dict->TypesEnd();it++)
            {
            Type *type = *it;
            if (_typesToExplode.find(type->id()) != _typesToExplode.end())
               log->indent() << type << log->endl();
            }
         }
      LOG_OUTDENT

      (*log) << log->endl();
      log->indent() << "Types to replace:" << log->endl();
      LOG_INDENT_REGION(log)
         {
         for (auto it = dict->TypesBegin();it != dict->TypesEnd();it++)
            {
            Type *type = *it;
            auto it2 = _typesToReplace.find(type->id());
            if (it2 != _typesToReplace.end())
               log->indent() << "Replace " << type << " with " << dict->LookupType(it2->second) << log->endl();
            }
         }
      LOG_OUTDENT
      log->indent() << "Transforming now:" << log->endl();
      }

   // just to make sure and in case someone calls it twice?
   _examinedType.clear();
   _modifiedType.clear();
   _explodedType.clear();

   LOG_INDENT_REGION(log)
      {
      for (TypeIterator typeIt = dict->TypesBegin(); typeIt != dict->TypesEnd(); typeIt++)
         {
         Type *type = *typeIt;
         transformTypeIfNeeded(dict, type);
         }
      }
   LOG_OUTDENT

   if (log) log->indent() << log->endl() << "Transformed dictionary:" << log->endl();
   if (log) log->writeDictionary(dict);

   _typesTransformed = true;
   if (log)
      {
      log->indent() << "Types to remove in final step:" << log->endl();
      LOG_INDENT_REGION(log)
         {
         for (auto it = dict->TypesBegin();it != dict->TypesEnd();it++)
            {
            Type *type = *it;
            if (_typesToRemove.find(type) != _typesToRemove.end())
               log->indent() << type << log->endl();
            }
         }
      LOG_OUTDENT
      }
   }

FunctionBuilder *
TypeReplacer::transformFunctionBuilder(FunctionBuilder * fb)
   {
   TypeDictionary *dict = fb->dict();

   TextWriter *log = fb->logger(traceEnabled());
   if (log) log->indent() << "TypeReplacer::transformFunctionBuilder F" << fb->id() << log->endl();

   if (log) log->indent() << "TypeReplacer::look for new Types:" << log->endl();
   LOG_INDENT_REGION(log)
      {
      for (auto it = dict->TypesBegin(); it != dict->TypesEnd(); it++)
         {
         Type *type = *it;
         if (_examinedType.find(type) == _examinedType.end())
            transformTypeIfNeeded(dict, type);
         }
      }
   LOG_OUTDENT

   // replace return type if needed
   Type *returnType = fb->getReturnType();
   Type *newReturnType = singleMappedType(returnType);
   if (newReturnType != returnType)
      fb->DefineReturnType(newReturnType);

   if (log) log->indent() << "Return type t" << returnType->id() << " -> t" << newReturnType->id() << log->endl();

   // replace parameters if needed, creating new Symbols and filling in _remapSymbol
   bool changeSomeParm = false;
   for (auto pIt = fb->ParametersBegin(); pIt != fb->ParametersEnd(); pIt++)
      {
      ParameterSymbol *parm = *pIt;
      Type *parmType = parm->type();
      if (_modifiedType.find(parmType) != _modifiedType.end())
         {
         changeSomeParm = true;
         break;
         }
      else
         _symbolMappers[parm] = new SymbolMapper(parm);
      }

   if (changeSomeParm)
      {
      ParameterSymbolVector prevParameters = fb->ResetParameters();
      for (auto pIt = prevParameters.begin(); pIt != prevParameters.end(); pIt++)
         {
         ParameterSymbol *parm = *pIt;
         Type *parmType = parm->type();
         if (log) log->indent() << "Parm " << parm->name() << " (" << parmType->name() << " t" << parmType->id() << "):" << log->endl();
         LOG_INDENT_REGION(log)
            {
            auto it = _typeMappers.find(parm->type());
            TypeMapper *parmTypeMapper = it->second;
            SymbolMapper *parmSymMapper = new SymbolMapper();
            if (parmTypeMapper->size() == 1)
               {
               Type *newType = parmTypeMapper->next();
               fb->DefineParameter(parm->name(), newType);
               if (log) log->indent() << "now DefineParameter " << parm->name() << " (" << newType->name() << " t" << newType->id() << ")" << log->endl();
               Symbol *newSym = fb->getSymbol(parm->name());
               parmSymMapper->add(newSym);
               _symbolMappers[newSym] = new SymbolMapper(newSym);
               }
            else
               {
               for (int i=0;i < parmTypeMapper->size();i++)
                  {
                  std::string name = parmTypeMapper->name();
                  std::string parmName = parm->name() + "." + name;
                  Type *parmType = parmTypeMapper->next();
                  fb->DefineParameter(parmName, parmType);
                  if (log) log->indent() << "now DefineParameter " << parmName << " (" << parmType->name() << " t" << parmType->id() << ")" << log->endl();
                  Symbol *newSym = fb->getSymbol(parmName);
                  parmSymMapper->add(newSym);
                  _symbolMappers[newSym] = new SymbolMapper(newSym);
                  }
               }
            _symbolMappers[parm] = parmSymMapper;
            }
         LOG_OUTDENT
         }
      }

   // replace locals if needed, creating new Symbols and filling in _remapSymbol
   bool changeSomeLocal = false;
   for (auto lIt = fb->LocalsBegin(); lIt != fb->LocalsEnd(); lIt++)
      {
      Symbol *local = *lIt;
      Type *type = local->type();
      if (_modifiedType.find(type) != _modifiedType.end())
         {
         changeSomeLocal = true;
         break;
         }
      else
         _symbolMappers[local] = new SymbolMapper(local);
      }

   if (changeSomeLocal)
      {
      LocalSymbolVector locals = fb->ResetLocals();
      for (auto lIt = locals.begin(); lIt != locals.end(); lIt++)
         {
         Symbol *local = *lIt;
         Type *type = local->type();
         auto it = _typeMappers.find(type);
         TypeMapper *typeMapper = it->second;
         SymbolMapper *symMapper = new SymbolMapper();

         if (log) log->indent() << "Local " << local->name() << " (" << type->name() << " t" << type->id() << "):" << log->endl();
         LOG_INDENT_REGION(log)
            {
            if (typeMapper->size() == 1)
               {
               Type *newType = typeMapper->next();
               fb->DefineLocal(local->name(), newType);
               if (log) log->indent() << "now DefineLocal " << local->name() << " (" << newType->name() << " t" << newType->id() << ")" << log->endl();
               Symbol *newSym = fb->getSymbol(local->name());
               symMapper->add(newSym);
               _symbolMappers[newSym] = new SymbolMapper(newSym);
              }
            else
               {
               for (int i=0;i < typeMapper->size();i++)
                  {
                  std::string name = typeMapper->name();
                  std::string newName = local->name() + "." + name;
                  Type *newType = typeMapper->next();
                  fb->DefineLocal(newName, newType);
                  if (log) log->indent() << "now DefineLocal " << newName << " (" << newType->name() << " t" << newType->id() << ")" << log->endl();
                  Symbol *newSym = fb->getSymbol(newName);
                  symMapper->add(newSym);
                  _symbolMappers[newSym] = new SymbolMapper(newSym);
                  }
               }
            }
         LOG_OUTDENT
         _symbolMappers[local] = symMapper;
         }
      }

   bool changeSomeFunction = false;
   for (auto fnIt = fb->FunctionsBegin(); fnIt != fb->FunctionsEnd(); fnIt++)
      {
      FunctionSymbol *function = *fnIt;
      FunctionType *type = function->functionType();
      if (_modifiedType.find(type) != _modifiedType.end())
         {
         changeSomeFunction = true;
         break;
         }
      else
         _symbolMappers[function] = new SymbolMapper(function);
      }

   if (changeSomeFunction)
      {
      // replace functions if needed, creating new Symbols and filling in _remapSymbol
      FunctionSymbolVector functions = fb->ResetFunctions();
      for (auto fnIt = functions.begin(); fnIt != functions.end(); fnIt++)
         {
         FunctionSymbol *function = *fnIt;
         FunctionType *type = function->functionType();
         if (log) log->indent() << "Function " << function->name() << " (" << type->name() << " t" << type->id() << "):" << log->endl();

         auto it = _typeMappers.find(type);
         TypeMapper *typeMapper = it->second;
         assert(typeMapper->size() == 1); // shouldn't be multiple FunctionTypes
         SymbolMapper *symMapper = new SymbolMapper();

         LOG_INDENT_REGION(log)
            {
            Type *newType = typeMapper->next();
            assert(newType->isFunction());
            FunctionType *newFnType = static_cast<FunctionType *>(newType);
            fb->DefineFunction(function->name(),
                               function->fileName(),
                               function->lineNumber(),
                               function->entryPoint(),
                               newFnType->returnType(),
                               newFnType->numParms(),
                               newFnType->parmTypes());
            if (log) log->indent() << "now DefineFunction " << function->name() << " (" << newType->name() << " t" << newType->id() << ")" << log->endl();
            FunctionSymbol *newSym = fb->LookupFunction(function->name());
            symMapper->add(newSym);
            _symbolMappers[newSym] = new SymbolMapper(newSym);
            if (newType != type)
               _typesToRemove.insert(type);
            }
         LOG_OUTDENT
         _symbolMappers[function] = symMapper;
         }
      }

   // values in operations will be replaced last, handled by transformOperation
   // first, set up a set of Mappers for those operations to use
   _mappedResultsSize = 1;
   _mappedResults = new ValueMapper *[1];
   _mappedResults[0] = NULL;

   _mappedOperandsSize = 2;
   _mappedOperands = new ValueMapper *[2];
   _mappedOperands[0] = NULL;
   _mappedOperands[1] = NULL;

   _mappedTypesSize = 1;
   _mappedTypes = new TypeMapper *[1];
   _mappedTypes[0] = new TypeMapper();

   _mappedSymbolsSize = 1;
   _mappedSymbols = new SymbolMapper *[1];
   _mappedSymbols[0] = new SymbolMapper();

   _mappedLiteralsSize = 1;
   _mappedLiterals = new LiteralMapper *[1];
   _mappedLiterals[0] = new LiteralMapper();

   if (log) log->indent() << log->endl() << "About to transform operations" << log->endl() << log->endl();

   return NULL;
   }

void
TypeReplacer::transformLiteral(TypeDictionary *dict, LiteralValue *lv)
   {
   LiteralMapper *m = new LiteralMapper();
   Type *t = lv->type();
   TypeID typeID = t->id();

   if (lv->kind() == T_typename)
      {
      Type *namedType = lv->getType();
      auto it = _typeMappers.find(namedType);
      TypeMapper *typeMapper = it->second;
      for (int i=0;i < typeMapper->size();i++)
         m->add(LiteralValue::create(dict, m->next()));
      }
   else if (_explodedType.find(t) != _explodedType.end())
      m = t->explode(lv);
   else if (_modifiedType.find(t) != _modifiedType.end())
      {
      // TODO: m = t->convert(lv, _typeMappers.find(t));
      }
   else
      m = new LiteralMapper(lv);

   _literalMappers[lv] = m;
   }

Builder *
TypeReplacer::transformOperation(Operation * op)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   TypeDictionary *dict = _fb->dict();
   Builder *b = NULL;

   LOG_INDENT_REGION(log)
      {
      int numMaps = 0;
      bool cloneNeeded = false;

      // make sure there are enough mapper slots for this operation's result Values
      if (op->numResults() > _mappedResultsSize)
         {
         delete[] _mappedResults;
         _mappedResults = new ValueMapper *[op->numResults()];
         _mappedResultsSize = op->numResults();
         }

      for (int i=0;i < _mappedResultsSize;i++)
         _mappedResults[i] = new ValueMapper();
      // mapOperation() will add result Values to the result mappers as they are mapped
      // nothing more to do here

      // make sure there are enough mapper slots for this operation's operands Values
      if (op->numOperands() > _mappedOperandsSize)
         {
         delete[] _mappedOperands;
         _mappedOperands = new ValueMapper *[op->numOperands()];
         _mappedOperandsSize = op->numOperands();
         }

      // fill in appropriate mappers based on this operation's operand Values
      for (int o=0;o < op->numOperands();o++)
         {
         Value *v = op->operand(o);
         auto it = _valueMappers.find(v);
         assert(it != _valueMappers.end()); // must have been produced earlier
         ValueMapper *valueMapper = it->second;
         _mappedOperands[o] = valueMapper;
         valueMapper->start();
         if (valueMapper->size() != 1 || valueMapper->current()->id() != v->id())
            cloneNeeded = true;
         int s = valueMapper->size();
         if (s > numMaps)
            numMaps = s;
         }

      // make sure there are enough mappers for this operation's LiteralValues
      if (op->numLiterals() > _mappedLiteralsSize)
         {
         delete[] _mappedLiterals;
         _mappedLiterals = new LiteralMapper *[op->numLiterals()];
         _mappedLiteralsSize = op->numLiterals();
         }

      // transform literals as needed and fill in appropriate mappers for this operation's LiteralValues
      for (int l=0;l < op->numLiterals();l++)
         {
         LiteralValue *lv = op->literal(l);
         if (_literalMappers.find(lv) == _literalMappers.end())
            transformLiteral(dict, lv);

         auto it = _literalMappers.find(lv);
         assert(it != _literalMappers.end());
         LiteralMapper *literalMapper = it->second;
         _mappedLiterals[l] = literalMapper;
         literalMapper->start();

         if (literalMapper->size() != 1
             || (literalMapper->current()->type() != NULL && literalMapper->current()->type()->id() != lv->type()->id()))
            cloneNeeded = true;

         int s = _mappedLiterals[l]->size();
         if (s > numMaps)
            numMaps = s;
         }

      // make sure there are enough mappers for this operation's Symbols
      if (op->numSymbols() > _mappedSymbolsSize)
         {
         delete[] _mappedSymbols;
         _mappedSymbols = new SymbolMapper *[op->numSymbols()];
         _mappedSymbolsSize = op->numSymbols();
         }

      // fill in appropriate mappers for this operations Symbols
      for (int i=0;i < op->numSymbols();i++)
         {
         Symbol *sym = op->symbol(i);
         if (_modifiedType.find(sym->type()) != _modifiedType.end())
            cloneNeeded = true;

         auto it = _symbolMappers.find(sym);
         assert(it != _symbolMappers.end());
         SymbolMapper *symbolMapper = it->second;
         _mappedSymbols[i] = symbolMapper;
         symbolMapper->start();

         if (symbolMapper->size() != 1 || symbolMapper->current()->id() != sym->id())
            cloneNeeded = true;

         int s = it->second->size();
         if (s > numMaps)
            numMaps = s;
         }

      // make sure there are enough mappers for this operation's Types
      if (op->numTypes() > _mappedTypesSize)
         {
         delete[] _mappedTypes;
         _mappedTypes = new TypeMapper *[op->numTypes()];
         _mappedTypesSize = op->numTypes();
         }

      // fill in appropriate mappers for this operation's Types
      for (int t=0;t < op->numTypes();t++)
         {
         Type *type = op->type(t);
         auto it = _typeMappers.find(type);
         assert(it != _typeMappers.end());
         TypeMapper *typeMapper = it->second;
         _mappedTypes[t] = typeMapper;
         typeMapper->start();

         if (typeMapper->size() != 1 || typeMapper->current()->id() != type->id())
            cloneNeeded = true;

         int s = it->second->size();
         if (s > numMaps)
            numMaps = s;
         }

      // make sure there are enough mappers for this operation's Builders
      if (op->numBuilders() > _mappedBuildersSize)
         {
         delete[] _mappedBuilders;
         _mappedBuilders = new BuilderMapper *[op->numBuilders()];
         _mappedBuildersSize = op->numBuilders();
         }

      // no Builder mappings are done at this time, so just create a mapper for each Builder
      //   initialized with the operation's original Builder
      for (int b=0;b < op->numBuilders();b++)
         {
         _mappedBuilders[b] = new BuilderMapper(op->builder(b));
         if (numMaps < 1)
            numMaps = 1;
         }

      if (!cloneNeeded)
         {
         if (log) log->indent() << "No clone needed, using original operation result(s) if any" << log->endl();

         // just map results to themselves and we're done
         for (int i=0;i < op->numResults();i++)
            {
            Value *result = op->result(i);
            _valueMappers[result] = new ValueMapper(result);
            }
         return NULL;
         }

      // otherwise this operation needs to be cloned
      b = fb()->OrphanBuilder();
      cloneOperation(b, op, numMaps);

      // store any new result mappings
      for (int i=0;i < op->numResults();i++)
         {
         Value *result = op->result(i);
         assert(_valueMappers.find(result) == _valueMappers.end());
         _valueMappers[result] = _mappedResults[i];
         }
      }
   LOG_OUTDENT
   return b;
   }

void
TypeReplacer::cloneOperation(Builder *b, Operation *op, int numMaps)
   {
   TextWriter *log = _fb->logger(traceEnabled());
   if (log) log->indent() << "Cloning operation" << log->endl();

   bool needReplacer = false;
   if (op->hasExpander())
      needReplacer = true;
   else
      {
      for (auto it=op->OperandsBegin();it != op->OperandsEnd();it++)
         needReplacer |= ((*it)->type()->isDynamic());
      }
   
   if (needReplacer)
      {
      OperationReplacer r(op);
      r.setBuilder(b);
      for (auto i=0;i < _mappedResultsSize;i++)
         r.setResultMapper(_mappedResults[i], i);
      for (auto i=0;i < _mappedOperandsSize;i++)
         r.setOperandMapper(_mappedOperands[i], i);
      for (auto i=0;i < _mappedBuildersSize;i++)
         r.setBuilderMapper(_mappedBuilders[i], i);
      for (auto i=0;i < _mappedLiteralsSize;i++)
         r.setLiteralMapper(_mappedLiterals[i], i);
      for (auto i=0;i < _mappedSymbolsSize;i++)
         r.setSymbolMapper(_mappedSymbols[i], i);
      for (auto i=0;i < _mappedTypesSize;i++)
         r.setTypeMapper(_mappedTypes[i], i);
      r.setExplodedTypes(&_explodedType);
     
      if (op->hasExpander() && op->expand(&r))
         return;

      for (auto it=op->OperandsBegin();it != op->OperandsEnd();it++)
         {
         Value *v = *it;
         Type *t = v->type();
         if (t->isDynamic())
            {
            DynamicType *type = static_cast<DynamicType *>(t);
            if (type->expand(&r))
               return;
            }
         }
      }

   // otherwise, map the operation generically
   for (int i=0;i < numMaps;i++)
      op->cloneTo(b, _mappedResults, _mappedOperands, _mappedTypes, _mappedLiterals, _mappedSymbols, _mappedBuilders);
   }

FunctionBuilder *
TypeReplacer::transformFunctionBuilderAtEnd(FunctionBuilder * fb)
   {
   finalCleanup(fb);
   return NULL;
   }

void
TypeReplacer::finalCleanup(FunctionBuilder * fb)
   {
   TextWriter *log = fb->logger(traceEnabled());
   if (log) log->indent() << "Final stage: removing types (" << _typesToRemove.size() << " types registered for removal):" << log->endl();

   LOG_INDENT_REGION(log)
      {
      TypeDictionary *dict = fb->dict();
      for (auto typeIt = _typesToRemove.begin(); typeIt != _typesToRemove.end(); typeIt++)
         {
         Type *typeToRemove = *typeIt;
         if (typeToRemove->isField())
            {
            // be careful: make sure owning struct isn't marked for removal
            //             if it is, then we would remove this field type twice
            FieldType *fType = static_cast<FieldType *>(typeToRemove);
            if (fType->owningStruct()->owningDictionary() == dict
                && _typesToRemove.find(fType->owningStruct()) != _typesToRemove.end())
               {
               if (log) log->indent() << "Ignoring field type inside to-be-removed struct: ";
               if (log) log->writeType(typeToRemove);
               continue;
               }
            }
         if (log) log->indent() << "Removing ";
         if (log) log->writeType(typeToRemove);
         dict->RemoveType(typeToRemove);
         }
      }
   LOG_OUTDENT

   if (log) log->indent() << "Final dictionary:" << log->endl();
   if (log) log->writeDictionary(fb->dict());
   }
