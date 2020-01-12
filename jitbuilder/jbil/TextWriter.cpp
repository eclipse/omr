/********************************************************************************
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

#include "TextWriter.hpp"
#include "Builder.hpp"
#include "Case.hpp"
#include "FunctionBuilder.hpp"
#include "Operation.hpp"
#include "Type.hpp"
#include "Value.hpp"

using namespace OMR::JitBuilder;

void
OMR::JitBuilder::TextWriter::writeDictionary(TypeDictionary *types)
   {
   TextWriter &w = *this;
   w << "[ TypeDictionary " << types << " " << types->name() << w.endl();
   w.indentIn();
   if (types->hasLinkedDictionary())
      w.indent() << "[ linkedDictionary " << types->linkedDictionary() << " ]" << w.endl();
   for (TypeIterator typeIt = types->TypesBegin();typeIt != types->TypesEnd();typeIt++)
      {
      Type *type = *typeIt;
      writeType(type);
      }
   w.indentOut();
   w.indent() << "]" << w.endl();
   }

void
OMR::JitBuilder::TextWriter::visitFunctionBuilderPreOps(FunctionBuilder * fb)
   {
   TextWriter &w = *this;

   TypeDictionary *types = fb->dict();
   writeDictionary(types);

   w << "[ FunctionBuilder F" << fb->id() << " \"" << fb->name() << "\"" << w.endl();
   w.indentIn();
   w.indent() << "[ types " << fb->dict() << " ]" << w.endl();
   w.indent() << "[ origin " << fb->fileName() + "::" + fb->lineNumber() << " ]" << w.endl();
   w.indent() << "[ returnType " << fb->getReturnType() << "]" << w.endl();
   for (ParameterSymbolIterator paramIt = fb->ParametersBegin();paramIt != fb->ParametersEnd(); paramIt++)
      {
      const ParameterSymbol *parameter = *paramIt;
      w.indent() << "[ parameter " << parameter << " ]" << w.endl();
      }
   for (LocalSymbolIterator localIt = fb->LocalsBegin();localIt != fb->LocalsEnd();localIt++)
      {
      const LocalSymbol *local = *localIt;
      w.indent() << "[ local " << local << " ]" << w.endl();
      }
   for (FunctionSymbolIterator functionIt = fb->FunctionsBegin();functionIt != fb->FunctionsEnd();functionIt++)
      {
      const FunctionSymbol *function = *functionIt;
      w.indent() << "[ function " << function << " ]" << w.endl();
      }
   w.indent() << "[ operations" << w.endl();
   w.indentIn();
   }

void
OMR::JitBuilder::TextWriter::visitFunctionBuilderPostOps(FunctionBuilder * fb)
   {
   TextWriter &w = *this;

   w.indentOut();
   w.indent() << "]" << w.endl();
   //w.indentOut();
   }

void
OMR::JitBuilder::TextWriter::visitBuilderPreOps(Builder * b)
   {
   // empty (Label) builders aren't printed independently
   if (b->numOperations() > 0)
      {
      TextWriter &w = *this;

      w.indent() << "[ Builder " << b << w.endl();;
      w.indentIn();

      w.indent() << "[ parent " << b->parent() << " ]" << w.endl();

      if (b->numChildren() > 0)
         {
         w.indent() << "[ children" << w.endl();
         w.indentIn();
         for (BuilderIterator bIt = b->ChildrenBegin(); bIt != b->ChildrenEnd(); bIt++)
            {
            Builder *child = *bIt;
            w.indent() << "[ " << child << " ]" << w.endl();
            }
         w.indentOut();
         w.indent() << "]" << w.endl();
         }

      if (b->isBound())
         w.indent() << "[ bound " << b->boundToOperation() << " ]" << w.endl();
      //else
      //   w.indent() << "[ unbound ]" << w.endl();

      if (b->isTarget())
         w.indent() << "[ isTarget ]" << w.endl();
      //else
      //   w.indent() << "[ notTarget ]" << w.endl();

      w.indent() << "[ operations" << w.endl();
      w.indentIn();
      }
   }

void
OMR::JitBuilder::TextWriter::visitBuilderPostOps(Builder * b)
   {
   if (b->numOperations() > 0)
      {
      TextWriter &w = *this;

      w.indentOut();
      w.indent() << "]" << w.endl();
      w.indentOut();
      w.indent() << "]" << w.endl();
      }
   }

void
OMR::JitBuilder::TextWriter::printTypePrefix(Type * type, bool indent)
   {
   TextWriter &w = *this;
   if (indent)
      w.indent();
   w << "[ " << ((void*)type) << " type " << type << " " << type->size() << " " << type->name() << " ";
   }

void
OMR::JitBuilder::TextWriter::writeType(Type *type, bool indent)
   {
   TextWriter &w = *this;
   printTypePrefix(type, indent);
   if (type->isPointer())
      {
      PointerType *pType = static_cast<PointerType *>(type);
      w << "pointerType t" << pType->BaseType()->id() << " ]" << w.endl();
      }
   else if (type->isStruct())
      {
      StructType *sType = static_cast<StructType *>(type);
      w << "structType";
      for (auto fIt = sType->FieldsBegin(); fIt != sType->FieldsEnd(); fIt++)
         {
         FieldType *fType = fIt->second;
         w << " t" << fType->id() << "@" << fType->offset();
         }
      w << " ]" << w.endl();
      }
   else if (type->isUnion())
      {
      UnionType *uType = static_cast<UnionType *>(type);
      w << "unionType ";
      for (auto fIt = uType->FieldsBegin(); fIt != uType->FieldsEnd(); fIt++)
         {
         FieldType *fType = fIt->second;
         w << " t" << fType->id();
         }
      w << " ]" << w.endl();
      }
   else if (type->isField())
      {
      const FieldType *fType = static_cast<FieldType *>(type);
      w << "fieldType struct t" << fType->owningStruct()->id() << " field t" << fType->type()->id() << " " << fType->name() << " offset " << fType->offset() << " ]" << w.endl();
      }
   else if (type->isFunction())
      {
      FunctionType *fType = static_cast<FunctionType *>(type);
      w << "functionType t" << fType->returnType()->id() << " " << fType->numParms();
      for (int32_t p=0;p < fType->numParms();p++)
         w << " t" << fType->parmType(p)->id();
      w << " ]" << w.endl();
      }
   else
      {
      w << "primitiveType";
      Type *layout = type->layout();
      if (layout)
         w << " layout " << layout;
      w << "]" << w.endl();

      }
   }

void
OMR::JitBuilder::TextWriter::printOperationPrefix(Operation * op)
   {
   TextWriter &w = *this;
   w.indent() << op->fb()->name() << "!" << op->parent() << "!o" << op->id() << " : ";
   }

void
OMR::JitBuilder::TextWriter::writeOperation(Operation * op)
   {
   TextWriter &w = *this;
   printOperationPrefix(op);
   switch (op->action())
      {
      case aNone :
         break;

      case aConstInt8 :
         w << op->result() << " = ConstInt8 " << (int32_t) op->literal()->getInt8() << w.endl();
         break;

      case aConstInt16 :
         w << op->result() << " = ConstInt16 " << (int32_t) op->literal()->getInt16() << w.endl();
         break;

      case aConstInt32 :
         w << op->result() << " = ConstInt32 " << op->literal()->getInt32() << w.endl();
         break;

      case aConstInt64 :
         w << op->result() << " = ConstInt64 " << op->literal()->getInt64() << w.endl();
         break;

      case aConstFloat :
         w << op->result() << " = ConstFloat " << op->literal()->getFloat() << w.endl();
         break;

      case aConstDouble :
         w << op->result() << " = ConstDouble " << op->literal()->getDouble() << w.endl();
         break;

      case aConstAddress :
         w << op->result() << " = ConstAddress " << op->literal()->getAddress() << w.endl();
         break;

      case aCoercePointer :
         w << op->result() << " = CoercePointer " << op->type() << " " << op->operand() << w.endl();
         break;

      case aAdd :
         w << op->result() << " = Add " << op->operand(0) << " " << op->operand(1) << w.endl();
         break;

      case aSub :
         w << op->result() << " = Sub " << op->operand(0) << " " << op->operand(1) << w.endl();
         break;

      case aMul :
         w << op->result() << " = Mul " << op->operand(0) << " " << op->operand(1) << w.endl();
         break;

      case aLoad :
         w << op->result() << " = Load " << op->symbol() << w.endl();
         break;

      case aLoadAt :
         w << op->result() << " = LoadAt " << op->type() << " " << op->operand() << w.endl();
         break;

      case aLoadField :
         {
         LoadField *lfOp = static_cast<LoadField *>(op);
         FieldType *fieldType = lfOp->getFieldType();
         StructType *structType = fieldType->owningStruct();
         w << lfOp->result() << " = LoadField " << fieldType << " ( " << structType->name() << " . " << fieldType->name() << " ) " << lfOp->operand() << w.endl();
         }
         break;

      case aLoadIndirect :
         {
         LoadIndirect *liOp = static_cast<LoadIndirect *>(op);
         FieldType *fieldType = liOp->getFieldType();
         StructType *structType = fieldType->owningStruct();
         w << liOp->result() << " = LoadIndirect " << fieldType << " ( " << structType->name() << " -> " << fieldType->name() << " ) " << liOp->operand() << w.endl();
         }
         break;

      case aStore :
         w << "Store " << op->symbol() << " " << op->operand() << w.endl();
         break;

      case aStoreField :
         {
         StoreField *sfOp = static_cast<StoreField *>(op);
         FieldType *fieldType = sfOp->getFieldType();
         StructType *structType = fieldType->owningStruct();
         w << "StoreField " << fieldType << " ( " << structType->name() << " . " << fieldType->name() << " ) " << sfOp->operand(0) << " " << sfOp->operand(1) << w.endl();
         }
         break;

      case aStoreIndirect :
         {
         StoreIndirect *siOp = static_cast<StoreIndirect *>(op);
         FieldType *fieldType = siOp->getFieldType();
         StructType *structType = fieldType->owningStruct();
         w << "StoreIndirect " << fieldType << " ( " << structType->name() << " -> " << fieldType->name() << " ) " << siOp->operand(0) << " " << siOp->operand(1) << w.endl();
         }
         break;

      case aStoreAt :
         w << "StoreAt " << op->operand(0) << " " << op->operand(1) << w.endl();
         break;

      case aIndexAt :
         w << op->result() << " = IndexAt " << op->type() << " " << op->operand(0) << " " << op->operand(1) << w.endl();
         break;

      case aCall :
         {
         Call *callOp = static_cast<Call *>(op);
         if (callOp->result())
            w << callOp->result() << " = ";
         w << "Call " << callOp->function() << " " << callOp->numOperands();
         for (int32_t a=0;a < callOp->numArguments(); a++)
            w << " " << callOp->argument(a);
         w << w.endl();
         }
         break;

      case aAppendBuilder :
         {
         Builder * b = op->builder();
         if (b->numOperations() == 0)
            w << "AppendBuilder " << b << " (Label)" << w.endl();
         else
            {
            w << "AppendBuilder " << b << w.endl();
            if (_visitAppendedBuilders)
               {
               w.indentIn();
               start(b);
               w.indentOut();
               }
            }
         }
         break;

      case aGoto :
         {
         Builder * b = op->builder();
         if (b->numOperations() == 0)
            w << "Goto " << b << " (Label)" << w.endl();
         else
            w << "Goto " << b << w.endl();
         }
         break;

      case aReturn :
         w << "Return";
         if (op->numOperands() > 0)
            {
            for (ValueIterator vIt = op->OperandsBegin(); vIt != op->OperandsEnd(); vIt++)
               {
               Value * v = *vIt;
               w << " " << v;
               }
            }
         else
            {
            w << " nil";
            }
         w << w.endl();
         break;

      case aIfCmpGreaterThan :
         w << "IfCmpGreaterThan " << op->operand(0) << " " << op->operand(1);
         w << " then " << op->builder() << w.endl();;
         break;

      case aIfCmpLessThan :
         w << "IfCmpLessThan " << op->operand(0) << " " << op->operand(1);
         w << " then " << op->builder() << w.endl();
         break;

      case aIfCmpGreaterOrEqual :
         w << "IfCmpGreaterOrEqual " << op->operand(0) << " " << op->operand(1);
         w << " then " << op->builder() << w.endl();;
         break;

      case aIfCmpLessOrEqual :
         w << "IfCmpLessOrEqual " << op->operand(0) << " " << op->operand(1);
         w << " then " << op->builder() << w.endl();
         break;

      case aIfThenElse :
         w << "IfThenElse " << op->operand() << " then " << op->builder() << " else ";
         w << " else ";
         if (op->numBuilders() == 2)
            w << op->builder(1);
         else
            w << "nil";
         w << w.endl();
         break;

      case aSwitch :
         w << "Switch " << op->operand(0);
         for (CaseIterator cIt = op->CasesBegin(); cIt != op->CasesEnd(); cIt++)
            {
            Case * c = *cIt;
            w << " " << c;
            }
         w << " else " << op->builder(0) << w.endl();
         break;

      case aForLoop :
         if ((bool)op->literal()->getInt8())
            w << "ForLoopUp ";
         else
            w << "ForLoopDn ";
         w << op->symbol() << " : " << op->operand(0) << " to " << op->operand(1) << " by " << op->operand(2);
         w << " body " << op->builder(0);
         if (op->builder(1) != NULL)
            w << " continue " << op->builder(1);
         if (op->builder(2) != NULL)
            w << " break " << op->builder(2);
         w << w.endl();
         break;

      case aCreateLocalArray :
         w << op->result() << " = CreateLocalArray " << op->literal()->getInt32() << " " << op->type() << w.endl();
         break;

      case aCreateLocalStruct :
         w << op->result() << " = CreateLocalStruct " << op->type() << w.endl();
         break;

      // New operations
      // BEGIN {
      //

      //
      // } END
      // New operations

      default :
         assert(0);
         break;
      }
   }

void
OMR::JitBuilder::TextWriter::visitOperation(Operation *op)
   {
   writeOperation(op);
   }

void
OMR::JitBuilder::TextWriter::visitEnd()
   {
   TextWriter & w = *this;
   w.indentOut();
   w.indent() << "]" << w.endl();
   }
