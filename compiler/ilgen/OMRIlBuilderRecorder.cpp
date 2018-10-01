/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "ilgen/IlBuilderRecorder.hpp"
#include "ilgen/IlBuilder.hpp"

#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include "infra/Assert.hpp"
#include "compile/Compilation.hpp"
#include "ilgen/JitBuilderRecorder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/IlValue.hpp"

#include "ilgen/JitBuilderRecorderTextFile.hpp"

OMR::IlBuilderRecorder::IlBuilderRecorder(TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
      : TR::IlInjector(types),
      _methodBuilder(methodBuilder)
   {
   }

OMR::IlBuilderRecorder::IlBuilderRecorder(TR::IlBuilder *source)
      : TR::IlInjector(source),
      _methodBuilder(source->methodBuilder())
   {
   }

TR::IlBuilder *
OMR::IlBuilderRecorder::asIlBuilder()
   {
   return static_cast<TR::IlBuilder *>(this);
   }

TR::JitBuilderRecorder *
OMR::IlBuilderRecorder::recorder() const
   {
   return _methodBuilder->recorder();
   }

TR::JitBuilderRecorder *
OMR::IlBuilderRecorder::clearRecorder()
   {
   TR::JitBuilderRecorder *recorder = _methodBuilder->recorder();
   _methodBuilder->setRecorder(NULL);
   return recorder;
   }

void
OMR::IlBuilderRecorder::restoreRecorder(TR::JitBuilderRecorder *recorder)
   {
   _methodBuilder->setRecorder(recorder);
   }

void
OMR::IlBuilderRecorder::DoneConstructor(const char * value)
   {
     TR::JitBuilderRecorder *rec = recorder();
     if (rec)
        {
        rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_DONECONSTRUCTOR);
        rec->String(value);
        rec->EndStatement();
        }
   }

void
OMR::IlBuilderRecorder::assertNotRecorded(TR::JitBuilderRecorder * rec, const char * statement)
   {
   if (rec)
      TR_ASSERT(0, "%s does not have Recorder support\n", statement);
   }

void
OMR::IlBuilderRecorder::NewIlBuilder()
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(asIlBuilder());
      rec->BeginStatement(StatementName::STATEMENT_NEWILBUILDER);
      rec->Builder(asIlBuilder());
      rec->EndStatement();
      }
   }

TR::IlBuilder *
OMR::IlBuilderRecorder::createBuilderIfNeeded(TR::IlBuilder *builder)
   {
   if (builder == NULL)
      builder = asIlBuilder()->OrphanBuilder();
   return builder;
   }

TR::IlValue *
OMR::IlBuilderRecorder::newValue()
   {
   TR::IlValue *value = new (_comp->trHeapMemory()) TR::IlValue(_methodBuilder);
   return value;
   }

void
OMR::IlBuilderRecorder::convertTo(TR::IlValue *returnValue, TR::IlType *dt, TR::IlValue *v, const char *s)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), s);
      rec->Value(returnValue);
      rec->Type(dt);
      rec->Value(v);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::binaryOp(const TR::IlValue *returnValue, const TR::IlValue *left, const TR::IlValue *right, const char *s)
   {
   TR::JitBuilderRecorder *rec = recorder();
   TR_ASSERT(rec, "cannot call IlBuilderRecorder::binaryOp if recorder is NULL");

   rec->StoreID(returnValue);
   rec->BeginStatement(asIlBuilder(), s);
   rec->Value(returnValue);
   rec->Value(left);
   rec->Value(right);
   rec->EndStatement();
   }

void
OMR::IlBuilderRecorder::shiftOp(const TR::IlValue *returnValue, const TR::IlValue *v, const TR::IlValue *amount, const char *s)
   {
   TR::JitBuilderRecorder *rec = recorder();
   TR_ASSERT(rec, "cannot call IlBuilderRecorder::shiftOp if recorder is NULL");

   rec->StoreID(returnValue);
   rec->BeginStatement(asIlBuilder(), s);
   rec->Value(returnValue);
   rec->Value(v);
   rec->Value(amount);
   rec->EndStatement();
   }

void
OMR::IlBuilderRecorder::unaryOp(TR::IlValue *returnValue, TR::IlValue *v, const char *s)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(s);
      rec->Value(returnValue);
      rec->Value(v);
      rec->EndStatement();
      }
   }

// constants
TR::IlValue *
OMR::IlBuilderRecorder::NullAddress()
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_NULLADDRESS);
      rec->Value(returnValue);
      rec->EndStatement();
      }

   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstInt8(int8_t value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Int8->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTINT8);
      rec->Value(returnValue);
      rec->Number(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstInt16(int16_t value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Int16->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTINT16);
      rec->Value(returnValue);
      rec->Number(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstInt32(int32_t value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Int32->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTINT32);
      rec->Value(returnValue);
      rec->Number(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstInt64(int64_t value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Int64->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTINT64);
      rec->Value(returnValue);
      rec->Number(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstFloat(float value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Float->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTFLOAT);
      rec->Value(returnValue);
      rec->Number(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstDouble(double value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Double->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTDOUBLE);
      rec->Value(returnValue);
      rec->Number(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstAddress(const void * const value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Address->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTADDRESS);
      rec->Value(returnValue);
      rec->Location(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstString(const char * const value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      Address->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CONSTSTRING);
      rec->Value(returnValue);
      rec->String(value);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConstInteger(TR::IlType *intType, int64_t value)
   {
   if      (intType == Int8)  return ConstInt8 ((int8_t)  value);
   else if (intType == Int16) return ConstInt16((int16_t) value);
   else if (intType == Int32) return ConstInt32((int32_t) value);
   else if (intType == Int64) return ConstInt64(          value);

   TR_ASSERT(0, "unknown integer type");
   return NULL;
   }

// arithmetic
TR::IlValue *
OMR::IlBuilderRecorder::Add(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_ADD);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::AddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_ADDWITHOVERFLOW);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::AddWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_ADDWITHUNSIGNEDOVERFLOW);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::Sub(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_SUB);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::SubWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_SUBWITHOVERFLOW);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::SubWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_SUBWITHUNSIGNEDOVERFLOW);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::Mul(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_MUL);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::MulWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_MULWITHOVERFLOW);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::Div(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_DIV);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::Rem(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_REM);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::And(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_AND);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::Or(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_OR);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::Xor(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_XOR);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ShiftL(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_SHIFTL);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_SHIFTR);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::UnsignedShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      shiftOp(returnValue, v, amount, StatementName::STATEMENT_UNSIGNEDSHIFTR);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::EqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_EQUALTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::NotEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_NOTEQUALTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::GreaterThan(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_GREATERTHAN);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::UnsignedGreaterThan(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_UNSIGNEDGREATERTHAN);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::GreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_GREATEROREQUALTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::UnsignedGreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_UNSIGNEDGREATEROREQUALTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::LessThan(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      binaryOp(returnValue, left, right, StatementName::STATEMENT_LESSTHAN);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::UnsignedLessThan(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_UNSIGNEDLESSTHAN);
   return returnValue;
   }   

TR::IlValue *
OMR::IlBuilderRecorder::LessOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_LESSOREQUALTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::UnsignedLessOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_UNSIGNEDLESSOREQUALTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::Negate(TR::IlValue *v)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_NEGATE);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConvertBitsTo(TR::IlType* type, TR::IlValue* value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_CONVERTBITSTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ConvertTo(TR::IlType *dt, TR::IlValue *v)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      convertTo(returnValue, dt, v, StatementName::STATEMENT_CONVERTTO);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::UnsignedConvertTo(TR::IlType *dt, TR::IlValue *v)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      convertTo(returnValue, dt, v, StatementName::STATEMENT_UNSIGNEDCONVERTTO);
   return returnValue;
   }

// memory
void
OMR::IlBuilderRecorder::Store(const char *name, TR::IlValue *value)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_STORE);
      rec->String(name);
      rec->Value(value);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::StoreOver(TR::IlValue *dest, TR::IlValue *value)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_STOREOVER);
      rec->Value(dest);
      rec->Value(value);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::StoreAt(TR::IlValue *address, TR::IlValue *value)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_STOREAT);
      rec->Value(address);
      rec->Value(value);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::StoreIndirect(const char *type, const char *field, TR::IlValue *object, TR::IlValue *value)
  {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_STOREINDIRECT);
      rec->String(type);
      rec->String(field);
      rec->Value(object);
      rec->Value(value);
      rec->EndStatement();
      }
  }

TR::IlValue *
OMR::IlBuilderRecorder::Load(const char *name)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_LOAD);
      rec->Value(returnValue);
      rec->String(name);
      rec->EndStatement();
      }

   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::LoadIndirect(const char *type, const char *field, TR::IlValue *object)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_LOADINDIRECT);
      rec->Value(returnValue);
      rec->String(type);
      rec->String(field);
      rec->Value(object);
      rec->EndStatement();
      }

   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::LoadAt(TR::IlType *dt, TR::IlValue *address)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_LOADAT);
      rec->Value(returnValue);
      rec->Type(dt);
      rec->Value(address);
      rec->EndStatement();
      }

   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::IndexAt(TR::IlType *dt, TR::IlValue *base, TR::IlValue *index)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_INDEXAT);
      rec->Value(returnValue);
      rec->Type(dt);
      rec->Value(base);
      rec->Value(index);
      rec->EndStatement();
      }

   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::CreateLocalArray(int32_t numElements, TR::IlType *elementType)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CREATELOCALARRAY);
      rec->Value(returnValue);
      rec->Number(numElements);
      rec->Type(elementType);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::CreateLocalStruct(TR::IlType *structType)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CREATELOCALSTRUCT);
      rec->Value(returnValue);
      rec->Type(structType);
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::AtomicAdd(TR::IlValue * baseAddress, TR::IlValue * value)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_ATOMICADD);
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::StructFieldInstanceAddress(const char* structName, const char* fieldName, TR::IlValue* obj)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_STRUCTFIELDINSTANCEADDRESS);
      rec->Value(returnValue);
      rec->String(structName);
      rec->String(fieldName);
      rec->Value(obj);
      rec->EndStatement();
      }

   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::UnionFieldInstanceAddress(const char* unionName, const char* fieldName, TR::IlValue* obj)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_UNIONFIELDINSTANCEADDRESS);
      rec->Value(returnValue);
      rec->String(unionName);
      rec->String(fieldName);
      rec->Value(obj);
      rec->EndStatement();
      }

   return returnValue;
   }

// vector memory
TR::IlValue *
OMR::IlBuilderRecorder::VectorLoad(const char *name)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_VECTORLOAD);
      rec->Value(returnValue);
      rec->String(name);
      rec->EndStatement();
      }

   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::VectorLoadAt(TR::IlType *dt, TR::IlValue *address)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->StoreID(returnValue);
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_VECTORLOADAT);
      rec->Value(returnValue);
      rec->Type(dt);
      rec->Value(address);
      rec->EndStatement();
      }

   return returnValue;
   }

void
OMR::IlBuilderRecorder::VectorStore(const char *name, TR::IlValue *value)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_VECTORSTORE);
      rec->String(name);
      rec->Value(value);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::VectorStoreAt(TR::IlValue *address, TR::IlValue *value)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_VECTORSTOREAT);
      rec->Value(address);
      rec->Value(value);
      rec->EndStatement();
      }
   }

// control
void
OMR::IlBuilderRecorder::Transaction(TR::IlBuilder **persistentFailureBuilder, TR::IlBuilder **transientFailureBuilder, TR::IlBuilder **transactionBuilder)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_TRANSACTION);
   }

void
OMR::IlBuilderRecorder::TransactionAbort()
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_TRANSACTIONABORT);
   }

void
OMR::IlBuilderRecorder::AppendBuilder(TR::IlBuilder *builder)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_APPENDBUILDER);
      rec->Builder(builder);
      rec->EndStatement();
      }
   }

TR::IlValue *
OMR::IlBuilderRecorder::Call(const char *functionName, TR::DataType returnType, int32_t numArgs, TR::IlValue ** argValues)
   {
   TR::IlValue *returnValue = NULL;
   if (returnType != TR::NoType)
      {
      returnValue = newValue();
      }
   TR::JitBuilderRecorder *rec = recorder();
   if (NULL != rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_CALL);
      rec->String(functionName);
      rec->Number(numArgs);
      for (int32_t v=0;v < numArgs;v++)
         rec->Value(argValues[v]);
      if (returnType != TR::NoType)
         {
         rec->StoreID(returnValue);
         rec->Value(returnValue);
         }
      rec->EndStatement();
      }
   return returnValue;
   }

TR::IlValue *
OMR::IlBuilderRecorder::ComputedCall(const char *functionName, int32_t numArgs, TR::IlValue **argValues)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_COMPUTEDCALL);
   return returnValue;
   }

void
OMR::IlBuilderRecorder::Goto(TR::IlBuilder **dest)
   {
   *dest = createBuilderIfNeeded(*dest);
   Goto(*dest);
   }

void
OMR::IlBuilderRecorder::Goto(TR::IlBuilder *dest)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_GOTO);
      rec->Builder(dest);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::Return()
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_RETURN);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::Return(TR::IlValue *value)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_RETURNVALUE);
      rec->Value(value);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::ForLoop(bool countsUp,
                                const char *indVar,
                                TR::IlBuilder *bLoop,
                                TR::IlBuilder *bBreak,
                                TR::IlBuilder *bContinue,
                                TR::IlValue *initial,
                                TR::IlValue *end,
                                TR::IlValue *increment)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_FORLOOP);
      if(countsUp)
         rec->Number(1); // True
      else
         rec->Number(0); // False
      rec->String(indVar);
      rec->Builder(bLoop);
      rec->EnsureAvailableID(bBreak);
      rec->Builder(bBreak);
      rec->EnsureAvailableID(bContinue);
      rec->Builder(bContinue);
      rec->Value(initial);
      rec->Value(end);
      rec->Value(increment);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::ForLoop(bool countsUp,
                                const char *indVar,
                                TR::IlBuilder **loopCode,
                                TR::IlBuilder **breakBuilder,
                                TR::IlBuilder **continueBuilder,
                                TR::IlValue *initial,
                                TR::IlValue *end,
                                TR::IlValue *increment)
   {
   TR_ASSERT(loopCode != NULL, "ForLoop needs to have loopCode builder");
   TR::IlBuilder *bLoop = *loopCode = createBuilderIfNeeded(*loopCode);

   TR::IlBuilder *bBreak = NULL;
   if (breakBuilder)
      {
      TR_ASSERT(*breakBuilder == NULL, "ForLoop returns breakBuilder, cannot provide breakBuilder as input");
      bBreak = *breakBuilder = asIlBuilder()->OrphanBuilder();
      }

   TR::IlBuilder *bContinue = NULL;
   if (continueBuilder)
      {
      TR_ASSERT(*continueBuilder == NULL, "ForLoop returns continueBuilder, cannot provide continueBuilder as input");
      bContinue = *continueBuilder = asIlBuilder()->OrphanBuilder();
      }

   ForLoop(countsUp, indVar, *loopCode, bBreak, bContinue, initial, end, increment);
   }

void
OMR::IlBuilderRecorder::DoWhileLoop(const char *whileCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder, TR::IlBuilder **continueBuilder)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_DOWHILELOOP);
    }

void 
OMR::IlBuilderRecorder::WhileDoLoop(const char *whileCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder, TR::IlBuilder **continueBuilder)
   {
   TR::IlValue *returnValue = newValue();
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_WHILEDOLOOP);
   }

void
OMR::IlBuilderRecorder::IfAnd(TR::IlBuilder **allTrueBuilder, TR::IlBuilder **anyFalseBuilder, int32_t numTerms, ...)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFAND);
   }

void
OMR::IlBuilderRecorder::IfOr(TR::IlBuilder **anyTrueBuilder, TR::IlBuilder **allFalseBuilder, int32_t numTerms, ...)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFOR);
   }

void
OMR::IlBuilderRecorder::IfCmpNotEqualZero(TR::IlBuilder *target, TR::IlValue *condition)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPNOTEQUALZERO);
   }

void
OMR::IlBuilderRecorder::IfCmpNotEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPNOTEQUAL);
   }

void
OMR::IlBuilderRecorder::IfCmpEqualZero(TR::IlBuilder *target, TR::IlValue *condition)
   {
   TR::JitBuilderRecorder *rec = recorder();

   if(rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_IFCMPEQUALZERO);
      rec->Builder(target);
      rec->Value(condition);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::IfCmpEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPEQUAL);
   }

void
OMR::IlBuilderRecorder::IfCmpLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPLESSTHAN);
   }

void
OMR::IlBuilderRecorder::IfCmpUnsignedLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPUNSIGNEDLESSTHAN);
   }

void
OMR::IlBuilderRecorder::IfCmpLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPLESSOREQUAL);
   }

void
OMR::IlBuilderRecorder::IfCmpUnsignedLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPUNSIGNEDLESSOREQUAL);
   }

void
OMR::IlBuilderRecorder::IfCmpGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPGREATERTHAN);
   }

void
OMR::IlBuilderRecorder::IfCmpUnsignedGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPUNSIGNEDGREATERTHAN);
   }

void
OMR::IlBuilderRecorder::IfCmpGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPGREATEROREQUAL);
   }

void
OMR::IlBuilderRecorder::IfCmpUnsignedGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_IFCMPUNSIGNEDGREATEROREQUAL);
   }

void
OMR::IlBuilderRecorder::IfThenElse(TR::IlBuilder *thenPath, TR::IlBuilder *elsePath, TR::IlValue *condition)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asIlBuilder(), StatementName::STATEMENT_IFTHENELSE);
      rec->Builder(thenPath);
      if (elsePath)
         rec->Builder(elsePath);
      else
         rec->Builder(NULL);
      rec->Value(condition);
      rec->EndStatement();
      }
   }

void
OMR::IlBuilderRecorder::IfThenElse(TR::IlBuilder **thenPath, TR::IlBuilder **elsePath, TR::IlValue *condition)
   {
   TR_ASSERT(thenPath != NULL || elsePath != NULL, "IfThenElse needs at least one conditional path");

   TR::IlBuilder *bThen=NULL;
   if (thenPath)
      bThen = *thenPath = createBuilderIfNeeded(*thenPath);

   TR::IlBuilder *bElse=NULL;
   if (elsePath)
      bElse = *elsePath = createBuilderIfNeeded(*elsePath);

   IfThenElse(bThen, bElse, condition);
   }

void
OMR::IlBuilderRecorder::Switch(const char *selectionVar,
                  TR::IlBuilder **defaultBuilder,
                  uint32_t numCases,
                  ...)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      assertNotRecorded(rec, StatementName::STATEMENT_SWITCH);
   }