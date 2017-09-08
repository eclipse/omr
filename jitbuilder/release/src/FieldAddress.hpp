/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef FIELDADDRESS_INCL
#define FIELDADDRESS_INCL

#include "ilgen/MethodBuilder.hpp"

namespace TR { class TypeDictionary; }

struct Struct
   {
   uint16_t f1;
   uint8_t f2;
   };

union Union
   {
   uint16_t f1;
   uint8_t f2;
   };

typedef uint8_t* (GetStructFieldAddressFunction)(Struct*);
typedef uint8_t* (GetUnionFieldAddressFunction)(Union*);

class GetStructFieldAddressBuilder : public TR::MethodBuilder
   {
   private:
   TR::IlType *pStructType;

   public:
   GetStructFieldAddressBuilder(TR::TypeDictionary *);
   virtual bool buildIL();
   };

class GetUnionFieldAddressBuilder : public TR::MethodBuilder
   {
   private:
   TR::IlType *pUnionType;

   public:
   GetUnionFieldAddressBuilder(TR::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // FIELDADDRESS_INCL
