/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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


#ifndef COMPLEXMATMULT_INCL
#define COMPLEXMATMULT_INCL

#include "complex.hpp"
#include "JitBuilder.hpp"

typedef void (ComplexMatMultFunctionType)(complex<double> *, complex<double> *, complex<double> *, int64_t);

class ComplexMatMult : public OMR::JitBuilder::FunctionBuilder
   {
   private:
   OMR::JitBuilder::Type *pComplex;

   void Store2D(OMR::JitBuilder::Builder *bldr,
                OMR::JitBuilder::Value *base,
                OMR::JitBuilder::Value *first,
                OMR::JitBuilder::Value *second,
                OMR::JitBuilder::Value *N,
                OMR::JitBuilder::Value *value);
   OMR::JitBuilder::Value *Load2D(OMR::JitBuilder::Builder *bldr,
                                  OMR::JitBuilder::Value *base,
                                  OMR::JitBuilder::Value *first,
                                  OMR::JitBuilder::Value *second,
                                  OMR::JitBuilder::Value *N);

   public:
   ComplexMatMult(OMR::JitBuilder::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // !defined(COMPLEXMATMULT_INCL)
