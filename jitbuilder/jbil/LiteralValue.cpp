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

#include "LiteralValue.hpp"
#include "Type.hpp"
#include "TypeDictionary.hpp"

using namespace OMR::JitBuilder;

LiteralValue::~LiteralValue()
   {
   if (_kind == T_string)
      delete v_string;
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, int8_t v)
   {
   return new LiteralValue(dict, v);
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, int16_t v)
   {
   return new LiteralValue(dict, v);
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, int32_t v)
   {
   return new LiteralValue(dict, v);
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, int64_t v)
   {
   return new LiteralValue(dict, v);
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, float v)
   {
   return new LiteralValue(dict, v);
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, double v)
   {
   return new LiteralValue(dict, v);
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, void *v)
   {
   return new LiteralValue(dict, v);
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, std::string v)
   {
   return new LiteralValue(dict, new std::string(v));
   }

LiteralValue *
LiteralValue::create(TypeDictionary *dict, Type *t)
   {
   return new LiteralValue(dict, t);
   }

int8_t
LiteralValue::getInt8()
   {
   assert(_kind == T_int8);
   return v_Int8;
   }

int16_t
LiteralValue::getInt16()
   {
   assert(_kind == T_int16);
   return v_Int16;
   }

int32_t
LiteralValue::getInt32()
   {
   assert(_kind == T_int32);
   return v_Int32;
   }

int64_t
LiteralValue::getInt64()
   {
   assert(_kind == T_int64);
   return v_Int64;
   }

float
LiteralValue::getFloat()
   {
   assert(_kind == T_float);
   return v_Float;
   }

double
LiteralValue::getDouble()
   {
   assert(_kind == T_double);
   return v_Double;
   }

void *
LiteralValue::getAddress()
   {
   assert(_kind == T_address);
   return v_Address;
   }

std::string
LiteralValue::getString()
   {
   if (_kind == T_string)
      return *v_string;
   if (_kind == T_typename)
      return v_type->name();
   assert(0);
   }

Type *
LiteralValue::getType()
   {
   assert(_kind == T_typename);
   return v_type;
   }

std::string
LiteralValue::getTypeString()
   {
   assert(_kind == T_typename);
   return v_type->name();
   }

LiteralValue::LiteralValue(TypeDictionary *dict, int8_t v)
   : _dict(dict)
   , _kind(T_int8)
   , _type(dict->Int8)
   , v_Int8(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, int16_t v)
   : _dict(dict)
   , _kind(T_int16)
   , _type(dict->Int16)
   , v_Int16(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, int32_t v)
   : _dict(dict)
   , _kind(T_int32)
   , _type(dict->Int32)
   , v_Int32(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, int64_t v)
   : _dict(dict)
   , _kind(T_int64)
   , _type(dict->Int64)
   , v_Int64(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, float v)
   : _dict(dict)
   , _kind(T_float)
   , _type(dict->Float)
   , v_Float(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, double v)
   : _dict(dict)
   , _kind(T_double)
   , _type(dict->Double)
   , v_Double(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, void *v)
   : _dict(dict)
   , _kind(T_address)
   , _type(dict->Address)
   , v_Address(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, std::string *v)
   : _dict(dict)
   , _kind(T_string)
   , _type(dict->Address) // until string becomes a recognized type
   , v_string(v)
   { }

LiteralValue::LiteralValue(TypeDictionary *dict, Type *v)
   : _dict(dict)
   , _kind(T_typename)
   , _type(dict->Address)
   , v_type(v)
   { }


// New user code
// BEGIN {
//

//
// } END
// New user code
