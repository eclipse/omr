/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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


#ifndef MATMULT_INCL
#define MATMULT_INCL

#include "ilgen/MethodBuilder.hpp"

typedef void (MatMultFunctionType)(double *, double *, double *, int32_t);

class MatMult : public TR::MethodBuilder
   {
   private:
   TR::IlType *pDouble;

   void Store2D(TR::IlBuilder *bldr,
                TR::IlValue *base,
                TR::IlValue *first,
                TR::IlValue *second,
                TR::IlValue *N,
                TR::IlValue *value);
   TR::IlValue *Load2D(TR::IlBuilder *bldr,
                       TR::IlValue *base,
                       TR::IlValue *first,
                       TR::IlValue *second,
                       TR::IlValue *N);

   public:
   MatMult(TR::TypeDictionary *);
   virtual bool buildIL();
   };

class VectorMatMult : public TR::MethodBuilder
   {
   private:
   TR::IlType *pDouble;
   TR::IlType *ppDouble;

   void VectorStore2D(TR::IlBuilder *bldr,
                      TR::IlValue *base,
                      TR::IlValue *first,
                      TR::IlValue *second,
                      TR::IlValue *N,
                      TR::IlValue *value);
   TR::IlValue *VectorLoad2D(TR::IlBuilder *bldr,
                             TR::IlValue *base,
                             TR::IlValue *first,
                             TR::IlValue *second,
                             TR::IlValue *N);
   TR::IlValue *Load2D(TR::IlBuilder *bldr,
                       TR::IlValue *base,
                       TR::IlValue *first,
                       TR::IlValue *second,
                       TR::IlValue *N);

   public:
   VectorMatMult(TR::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // !defined(MATMULT_INCL)
