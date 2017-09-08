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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "MatMult.hpp"


MatMult::MatMult(TR::TypeDictionary *types)
   : MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("matmult");

   pDouble = types->PointerTo(Double);

   // C = A * B, all NxN matrices
   DefineParameter("C", pDouble);
   DefineParameter("A", pDouble);
   DefineParameter("B", pDouble);
   DefineParameter("N", Int32);

   DefineReturnType(NoType);

   DefineLocal("sum", Double);
   }


void
MatMult::Store2D(TR::IlBuilder *bldr,
                 TR::IlValue *base,
                 TR::IlValue *first,
                 TR::IlValue *second,
                 TR::IlValue *N,
                 TR::IlValue *value)
   {
   bldr->StoreAt(
   bldr->   IndexAt(pDouble,
               base,
   bldr->      Add(
   bldr->         Mul(
                     first,
                     N),
                  second)),
            value);
   }

TR::IlValue *
MatMult::Load2D(TR::IlBuilder *bldr,
                TR::IlValue *base,
                TR::IlValue *first,
                TR::IlValue *second,
                TR::IlValue *N)
   {
   return
      bldr->LoadAt(pDouble,
      bldr->   IndexAt(pDouble,
                  base,
      bldr->      Add(
      bldr->         Mul(
                        first,
                        N),
                     second)));
   }

bool
MatMult::buildIL()
   {
   // marking all locals as defined allows remaining locals to be temps
   // which enables further optimization opportunities particularly for
   //    floating point types
   AllLocalsHaveBeenDefined();

   TR::IlValue *i, *j, *k;
   TR::IlValue *A_ik, *B_kj;

   TR::IlValue *A = Load("A");
   TR::IlValue *B = Load("B");
   TR::IlValue *C = Load("C");
   TR::IlValue *N = Load("N");
   TR::IlValue *zero = ConstInt32(0);
   TR::IlValue *one = ConstInt32(1);

   TR::IlBuilder *iloop=NULL, *jloop=NULL, *kloop=NULL;
   ForLoopUp("i", &iloop, zero, N, one);
      {
      i = iloop->Load("i");

      iloop->ForLoopUp("j", &jloop, zero, N, one);
         {
         j = jloop->Load("j");

         jloop->Store("sum",
         jloop->   ConstDouble(0.0));

         jloop->ForLoopUp("k", &kloop, zero, N, one);
            {
            k = kloop->Load("k");

            A_ik = Load2D(kloop, A, i, k, N);
            B_kj = Load2D(kloop, B, k, j, N);
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


VectorMatMult::VectorMatMult(TR::TypeDictionary *types)
   : MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("vecmatmult");

   pDouble = types->PointerTo(Double);

   // C = A * B, all NxN matrices
   DefineParameter("C", pDouble);
   DefineParameter("A", pDouble);
   DefineParameter("B", pDouble);
   DefineParameter("N", Int32);

   DefineReturnType(NoType);

   DefineLocal("sum", VectorDouble);
   }

void
VectorMatMult::VectorStore2D(TR::IlBuilder *bldr,
                             TR::IlValue *base,
                             TR::IlValue *first,
                             TR::IlValue *second,
                             TR::IlValue *N,
                             TR::IlValue *value)
   {
   bldr->VectorStoreAt(
   bldr->   IndexAt(pDouble,
               base,
   bldr->      Add(
   bldr->         Mul(
                     first,
                     N),
                  second)),
            value);
   }

TR::IlValue *
VectorMatMult::VectorLoad2D(TR::IlBuilder *bldr,
                            TR::IlValue *base,
                            TR::IlValue *first,
                            TR::IlValue *second,
                            TR::IlValue *N)
   {
   return
      bldr->VectorLoadAt(pDouble,
      bldr->   IndexAt(pDouble,
                  base,
      bldr->      Add(
      bldr->         Mul(
                        first,
                        N),
                     second)));
   }

TR::IlValue *
VectorMatMult::Load2D(TR::IlBuilder *bldr,
                      TR::IlValue *base,
                      TR::IlValue *first,
                      TR::IlValue *second,
                      TR::IlValue *N)
   {
   return
      bldr->LoadAt(pDouble,
      bldr->   IndexAt(pDouble,
                  base,
      bldr->      Add(
      bldr->         Mul(
                        first,
                        N),
                     second)));
   }

bool
VectorMatMult::buildIL()
   {
   // marking all locals as defined allows remaining locals to be temps
   // which enables further optimization opportunities particularly for
   //    floating point types
   AllLocalsHaveBeenDefined();

   TR::IlValue *i, *j, *k;
   TR::IlValue *A_ik, *B_kj;

   TR::IlValue *A = Load("A");
   TR::IlValue *B = Load("B");
   TR::IlValue *C = Load("C");
   TR::IlValue *N = Load("N");
   TR::IlValue *zero = ConstInt32(0);
   TR::IlValue *one = ConstInt32(1);
   TR::IlValue *two = ConstInt32(2);

   TR::IlBuilder *iloop=NULL, *jloop=NULL, *kloop=NULL;
   ForLoopUp("i", &iloop, zero, N, one);
      {
      i = iloop->Load("i");

      // vectorizing loop j
      iloop->ForLoopUp("j", &jloop, zero, N, two);
         {
         j = jloop->Load("j");

         jloop->VectorStore("sum",                             // sum is a vector
         jloop->   ConstDouble(0.0));

         jloop->ForLoopUp("k", &kloop, zero, N, one);
            {
            k = kloop->Load("k");

            A_ik = Load2D(kloop, A, i, k, N);                  // A[i,k] is scalar over j
            B_kj = VectorLoad2D(kloop, B, k, j, N);            // B[k,j] is vector over j
            kloop->VectorStore("sum",
            kloop->   Add(
            kloop->      VectorLoad("sum"),
            kloop->      Mul(A_ik, B_kj)));
            }

         VectorStore2D(jloop, C, i, j, N, jloop->Load("sum")); // C[i,j] is vector over j
         }
      }

   Return();

   return true;
   }


void
printMatrix(double *M, int32_t N, const char *name)
   {
   printf("%s = [\n", name);
   for (int32_t i=0;i < N;i++)
      {
      printf("      [ %lf", M[i*N]);
      for (int32_t j=1;j < N;j++)
          printf(", %lf", M[i * N + j]);
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
   const int32_t N=4;
   double A[N*N];
   double B[N*N];
   double C[N*N];
   double D[N*N];
   for (int32_t i=0;i < N;i++)
      {
      for (int32_t j=0;j < N;j++)
         {
         A[i*N+j] = 1.0;
         B[i*N+j] = (double)i+(double)j;
         C[i*N+j] = 0.0;
         D[i*N+j] = 0.0;
         }
      }
   printMatrix(A, N, "A");
   printMatrix(B, N, "B");

   printf("Step 3: define type dictionaries\n");
   TR::TypeDictionary types;
   TR::TypeDictionary vectypes;

   printf("Step 4: compile MatMult method builder\n");
   MatMult method(&types);
   uint8_t *entry=0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 5: invoke MatMult compiled code\n");
   MatMultFunctionType *test = (MatMultFunctionType *)entry;
   test(C, A, B, N);
   printMatrix(C, N, "C");

   printf("Step 6: compile VectorMatMult method builder\n");
   VectorMatMult vecmethod(&vectypes);
   uint8_t *vecentry=0;
   rc = compileMethodBuilder(&vecmethod, &vecentry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 7: invoke MatMult compiled code\n");
   MatMultFunctionType *vectest = (MatMultFunctionType *)vecentry;
   vectest(D, A, B, N);
   printMatrix(D, N, "D");

   printf ("Step 8: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
