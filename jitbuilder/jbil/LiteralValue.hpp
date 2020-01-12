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

#ifndef LITERALVALUE_INCL
#define LITERALVALUE_INCL

#include <map>
#include <vector>

#include "Object.hpp"

// New includes
// BEGIN {
//

//
// } END
// New includes

namespace OMR
{

namespace JitBuilder
{

class Type;
class TypeDictionary;

enum LiteralKind
   {
     T_none=-1
   , T_int8
   , T_int16
   , T_int32
   , T_int64
   , T_float
   , T_double
   , T_address
   , T_string
   , T_typename
   , T_aggregate

   // new literal base types
   // BEGIN {
   //

   //
   // } END
   // new literal base types
   };

class LiteralValue //: public Object
   {
   public:
   ~LiteralValue();

   static LiteralValue *create(TypeDictionary *dict, int8_t v);
   static LiteralValue *create(TypeDictionary *dict, int16_t v);
   static LiteralValue *create(TypeDictionary *dict, int32_t v);
   static LiteralValue *create(TypeDictionary *dict, int64_t v);
   static LiteralValue *create(TypeDictionary *dict, float v);
   static LiteralValue *create(TypeDictionary *dict, double v);
   static LiteralValue *create(TypeDictionary *dict, void *v);
   static LiteralValue *create(TypeDictionary *dict, std::string v);
   static LiteralValue *create(TypeDictionary *dict, Type *t);

   enum LiteralKind kind() const { return _kind; }
   Type *type() const { return _type; }

   int8_t getInt8();
   int16_t getInt16();
   int32_t getInt32();
   int64_t getInt64();
   float getFloat();
   double getDouble();
   void *getAddress();
   std::string getString();
   Type * getType();
   std::string getTypeString();

   // New public API
   // BEGIN {
   //
   
   //
   // } END
   // New public API

   protected:
   LiteralValue(TypeDictionary *dict, int8_t v);
   LiteralValue(TypeDictionary *dict, int16_t v);
   LiteralValue(TypeDictionary *dict, int32_t v);
   LiteralValue(TypeDictionary *dict, int64_t v);
   LiteralValue(TypeDictionary *dict, float v);
   LiteralValue(TypeDictionary *dict, double v);
   LiteralValue(TypeDictionary *dict, void *v);
   LiteralValue(TypeDictionary *dict, std::string *v);
   LiteralValue(TypeDictionary *dict, Type *t);

   // New protected API
   // BEGIN {
   //

   //
   // } END
   // New protected API

   TypeDictionary *_dict;
   LiteralKind _kind;
   Type *_type;
   union
      {
      int8_t v_Int8; // also bool
      int16_t v_Int16;
      int32_t v_Int32;
      int64_t v_Int64;
      float v_Float;
      double v_Double;
      void *v_Address;
      std::string *v_string;
      Type *v_type;
      std::map<std::string, LiteralValue *> *v_aggregate;

      // New literal values
      // BEGIN {
      //

      //
      // } END
      // New literal values
 
      };
   };

typedef std::vector<LiteralValue *> LiteralValueVector;

} // namespace JitBuilder

} // namespace OMR

#endif // defined(LITERALVALUE_INCL)
