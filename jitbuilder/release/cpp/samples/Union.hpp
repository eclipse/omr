/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef UNION_INCL
#define UNION_INCL

#include "JitBuilder.hpp"

union TestUnionInt8pChar
   {
   uint8_t v_uint8;
   const char* v_pchar;
   };

union TestUnionInt32Int32
   {
   uint32_t f1;
   uint32_t f2;
   };

union TestUnionInt16Double
   {
   uint16_t v_uint16;
   double v_double;
   };

typedef void (*SetUnionByteFunction)(TestUnionInt8pChar*, uint8_t);
typedef uint8_t (*GetUnionByteFunction)(TestUnionInt8pChar*);
typedef void (*SetUnionStrFunction)(TestUnionInt8pChar*, const char*);
typedef const char* (*GetUnionStrFunction)(TestUnionInt8pChar*);
typedef uint32_t (*TypePunInt32Int32Function)(TestUnionInt32Int32*, uint32_t, uint32_t);
typedef uint16_t (*TypePunInt16DoubleFunction)(TestUnionInt16Double*, uint16_t, double);

class UnionTypeDictionary : public OMR::JitBuilder::TypeDictionary
   {
   public:
   UnionTypeDictionary() :
      OMR::JitBuilder::TypeDictionary()
      {
      DefineUnion("TestUnionInt8pChar");
      UnionField("TestUnionInt8pChar", "v_uint8", toIlType<uint8_t>());
      UnionField("TestUnionInt8pChar", "v_pchar", toIlType<char*>());
      CloseUnion("TestUnionInt8pChar");

      DefineUnion("TestUnionInt32Int32");
      UnionField("TestUnionInt32Int32", "f1", Int32);
      UnionField("TestUnionInt32Int32", "f2", Int32);
      CloseUnion("TestUnionInt32Int32");

      DefineUnion("TestUnionInt16Double");
      UnionField("TestUnionInt16Double", "v_uint16", Int16);
      UnionField("TestUnionInt16Double", "v_double", Double);
      CloseUnion("TestUnionInt16Double");
      }
   };

class SetUnionByteBuilder : public OMR::JitBuilder::MethodBuilder
   {
   public:
   SetUnionByteBuilder(OMR::JitBuilder::TypeDictionary *);
   virtual bool buildIL();
   };

class GetUnionByteBuilder : public OMR::JitBuilder::MethodBuilder
   {
   public:
   GetUnionByteBuilder(OMR::JitBuilder::TypeDictionary *);
   virtual bool buildIL();
   };

class SetUnionStrBuilder : public OMR::JitBuilder::MethodBuilder
   {
   public:
   SetUnionStrBuilder(OMR::JitBuilder::TypeDictionary *);
   virtual bool buildIL();
   };

class GetUnionStrBuilder : public OMR::JitBuilder::MethodBuilder
   {
   public:
   GetUnionStrBuilder(OMR::JitBuilder::TypeDictionary *);
   virtual bool buildIL();
   };

class TypePunInt32Int32Builder : public OMR::JitBuilder::MethodBuilder
   {
   public:
   TypePunInt32Int32Builder(OMR::JitBuilder::TypeDictionary *);
   virtual bool buildIL();
   };

class TypePunInt16DoubleBuilder : public OMR::JitBuilder::MethodBuilder
   {
   public:
   TypePunInt16DoubleBuilder(OMR::JitBuilder::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // UNION_INCL
