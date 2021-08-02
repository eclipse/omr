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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string>
#include "complex.hpp"

#include "JitBuilder.hpp"
#include "ComplexMatMult.hpp"
#include "ComplexSupport.hpp"

using namespace OMR::JitBuilder;

ComplexMatMult::ComplexMatMult(TypeDictionary * types)
   : FunctionBuilder(types)
   , pComplex(types->PointerTo(Complex))
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("complexmatmult");

   // C = A * B, all NxN matrices
   DefineParameter("C", pComplex);
   DefineParameter("A", pComplex);
   DefineParameter("B", pComplex);
   DefineParameter("N", Int64);

   DefineReturnType(NoType);

   DefineLocal("sum", Complex);
   }


void
ComplexMatMult::Store2D(Builder *bldr,
                        Value *base,
                        Value *first,
                        Value *second,
                        Value *N,
                        Value *value)
   {
   bldr->StoreAt(
   bldr->   IndexAt(pComplex,
               base,
   bldr->      Add(
   bldr->         Mul(
                    first,
                    N),
                 second)),
            value);
   }

Value *
ComplexMatMult::Load2D(Builder *bldr,
                       Value *base,
                       Value *first,
                       Value *second,
                       Value *N)
   {
   return
      bldr->LoadAt(pComplex,
      bldr->   IndexAt(pComplex,
                  base,
      bldr->      Add(
      bldr->         Mul(
                       first,
                       N),
                     second)));
   }

bool
ComplexMatMult::buildIL()
   {
   Value *A = Load("A");
   Value *B = Load("B");
   Value *C = Load("C");
   Value *N = Load("N");
   Value *zero = ConstInt64(0);
   Value *one = ConstInt64(1);

   Builder *iloop=OrphanBuilder();
   ForLoopUp("i", iloop, zero, N, one);
      {
      Value *i = iloop->Load("i");

      Builder *jloop = iloop->OrphanBuilder();
      iloop->ForLoopUp("j", jloop, zero, N, one);
         {
         Value *j = jloop->Load("j");

         complex<double> complexZero(0.0, 0.0);
         jloop->Store("sum",
         jloop->   Append(ConstComplexBuilder, LiteralValue::create(dict(), Complex, &complexZero)));

         Builder *kloop = jloop->OrphanBuilder();
         jloop->ForLoopUp("k", kloop, zero, N, one);
            {
            Value *k = kloop->Load("k");

            Value *A_ik = Load2D(kloop, A, i, k, N);
            Value *B_kj = Load2D(kloop, B, k, j, N);
            kloop->Store("sum",
            kloop->   Add(
            kloop->      Load("sum"),
            kloop->      Mul(A_ik, B_kj)));
            }

         Store2D(jloop, C, i, j, N, jloop->Load("sum"));
         }
      }

   Return();

   return true;
   }


void
printMatrix(complex<double> *M, int64_t N, const char *name)
   {
   printf("%s = [\n", name);
   for (int64_t i=0;i < N;i++)
      {
      printf("      [ (%lf,%lf)", M[i*N].real, M[i*N].imag);
      for (int64_t j=1;j < N;j++)
          printf(", (%lf,%lf)", M[i * N + j].real, M[i * N + j].imag);
      printf(" ],\n");
      }
   printf("    ]\n\n");
   }

int
main(int argc, char *argv[])
   {
   printf("Step 1: initialize JIT\n");
   bool initialized = initializeJit();
   if (!initialized)
      {
      fprintf(stderr, "FAIL: could not initialize JIT\n");
      exit(-1);
      }

   printf("Step 2: define matrices\n");
   const int64_t N=4;
   complex<double> A[N*N];
   complex<double> B[N*N];
   complex<double> C[N*N];
   complex<double> D[N*N];
   for (int64_t i=0;i < N;i++)
      {
      for (int64_t j=0;j < N;j++)
         {
         A[i*N+j] = complex<double>(1.0,0.0);
         B[i*N+j] = complex<double>((double)i,(double)j);
         C[i*N+j] = complex<double>(0.0,0.0);
         D[i*N+j] = complex<double>(0.0,0.0);
         }
      }
   printMatrix(A, N, "A");
   printMatrix(B, N, "B");

   printf("Step 3: define type dictionaries\n");
   TypeDictionary types("ComplexMatMultTypes");
   initializeComplexSupport(&types);
   assert(Complex != NULL);
   assert(ConstComplexBuilder != NULL);

   printf("Step 4: construct MatMult method builder\n");
   ComplexMatMult method(&types);

   Transformer *typeReplacer = (new TypeReplacer(&method))
                           //->setTraceEnabled()
                           ->explode(Complex);

   std::cerr.setf(std::ios_base::skipws);
   TextWriter printer(&method, std::cout, std::string("    "));
   method.setLogger(&printer);
   method.config()//->setReportMemory()
                  //->setTraceBuildIL()
                  //->setTraceReducer()
                  //->setTraceCodeGenerator()
                  ->setTypeReplacer(typeReplacer);

   bool success = constructFunctionBuilder(&method);
   if (!success)
      {
      fprintf(stderr,"FAIL: construction error\n");
      exit(-2);
      }
   fprintf(stderr, "Builder successfully constructed!\n");

   printer.print();

   int32_t rc=0;
#if 1
   ComplexMatMultFunctionType *dbgTest = method.DebugEntry<ComplexMatMultFunctionType>(&rc);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: debug entry construction error: %d\n", rc);
      exit(-2);
      }
   dbgTest(C, A, B, N);
   printf("Matrix Multiply operands:\n");
   printMatrix(A, N, "A");
   printMatrix(B, N, "B");
   printf("Result after debugging is:\n");
   printMatrix(C, N, "C");
   exit(0);
#endif

#if 0
   printf("Print function builder before reduction to jbil_ll dialect\n");
   printer.print();

   printf("Step 5: reduce method builder to jbil_ll dialect\n");
   //typeReplacer.transform();

   printf("Print function builder after reduction to jbil_ll dialect\n");
   printer.print();

   printf("Step 6: inline AppendBuilders\n");
   AppendBuilderInliner inliner(&method);
   inliner.transform();

   printf("After inlining orphan AppendBuilder\n");
   printer.print();
#endif

   printf("Step n: explode Complex type\n");

   printer.print();
   typeReplacer->transform();
   printer.print();

   printf("Step 7: compile MatMult jbil\n");
   void *entry = NULL;
   rc = compileFunctionBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation failed %d\n", rc);
      exit(rc);
      }

   printf("Step 8: invoke MatMult compiled code\n");
   ComplexMatMultFunctionType *test = (ComplexMatMultFunctionType *)entry;
   test(C, A, B, N);

   printMatrix(A, N, "A");
   printMatrix(B, N, "B");
   printMatrix(C, N, "C");

   printf ("Step 9: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
