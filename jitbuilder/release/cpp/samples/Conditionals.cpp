/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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


/**
 * This file contains code samples for using conditional services like IfCmpLessThan
 */

#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "Conditionals.hpp"

using std::cout;
using std::cerr;

ConditionalMethod::ConditionalMethod(OMR::JitBuilder::TypeDictionary* d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_ifs");
   DefineParameter("x", Int32);
   DefineParameter("L", Int32);
   DefineParameter("U", Int32);
   DefineReturnType(Int32);
   }

/*
 * Method basically shows how various If conditional services can be used
 * to compare an integer argument against some bounds.
 * The generated code simply validates that several equivalent ways to
 * identify if the parameter value is between two other numbers produce
 * the same result.
 */

bool
ConditionalMethod::buildIL()
   {

   // if (x >= L && x < U)
   //    rc1 = 1;
   // else
   //    rc1 = 0;
   IlBuilder *x_ge_L_builder = OrphanBuilder();
   OMR::JitBuilder::IlValue *x_ge_L = x_ge_L_builder->GreaterOrEqualTo(
                                      x_ge_L_builder->   Load("x"),
                                      x_ge_L_builder->   Load("L"));

   IlBuilder *x_lt_U_builder = OrphanBuilder();
   OMR::JitBuilder::IlValue *x_lt_U = x_lt_U_builder->LessThan(
                                      x_lt_U_builder->   Load("x"),
                                      x_lt_U_builder->   Load("U"));

   IlBuilder *rc1True = NULL, *rc1False = NULL;
   IfAnd(&rc1True, &rc1False, 2,
      MakeCondition(x_ge_L_builder, x_ge_L),
      MakeCondition(x_lt_U_builder, x_lt_U));
   rc1True->Store("rc1",
   rc1True->   ConstInt32(1));
   rc1False->Store("rc1",
   rc1False->   ConstInt32(0));

   // if (x < L || x >= U)
   //    rc2 = 0;
   // else
   //    rc2 = 1;
   IlBuilder *x_lt_L_builder = OrphanBuilder();
   OMR::JitBuilder::IlValue *x_lt_L = x_lt_L_builder->LessThan(
                                      x_lt_L_builder->   Load("x"),
                                      x_lt_L_builder->   Load("L"));

   IlBuilder *x_ge_U_builder = OrphanBuilder();
   OMR::JitBuilder::IlValue *x_ge_U = x_ge_U_builder->GreaterOrEqualTo(
                                      x_ge_U_builder->   Load("x"),
                                      x_ge_U_builder->   Load("U"));

   IlBuilder *rc2True = NULL, *rc2False = NULL;
   IfOr(&rc2False, &rc2True, 2,
      MakeCondition(x_lt_L_builder, x_lt_L),
      MakeCondition(x_ge_U_builder, x_ge_U));
   rc2True->Store("rc2",
   rc2True->   ConstInt32(1));
   rc2False->Store("rc2",
   rc2False->   ConstInt32(0));

   // if (x - L < (U-L))
   //    rc3 = 1;
   // else
   //    rc3 = 0;
   IlBuilder *rc3True = NULL, *rc3False = NULL;
   IfThenElse(&rc3True, &rc3False,
      UnsignedLessThan(
         Sub(
            Load("x"),
            Load("L")),
         Sub(
            Load("U"),
            Load("L"))));
   rc3True->Store("rc3",
   rc3True->   ConstInt32(1));
   rc3False->Store("rc3",
   rc3False->   ConstInt32(0));

   // if (x - L >= (U-L))
   //    rc4 = 0;
   // else
   //    rc4 = 1;
   IlBuilder *rc4True = NULL, *rc4False = NULL;
   IfThenElse(&rc4False, &rc4True,
      UnsignedGreaterOrEqualTo(
         Sub(
            Load("x"),
            Load("L")),
         Sub(
            Load("U"),
            Load("L"))));
   rc4True->Store("rc4",
   rc4True->   ConstInt32(1));
   rc4False->Store("rc4",
   rc4False->   ConstInt32(0));

   //if (rc1 == rc2 && rc2 == rc3 && rc3 == rc4) return true;
   //else return false;
   IlBuilder *equiv1 = OrphanBuilder();
   OMR::JitBuilder::IlValue *rc12Equal = equiv1->EqualTo(
                                         equiv1->   Load("rc1"),
                                         equiv1->   Load("rc2"));
   IlBuilder *equiv2 = OrphanBuilder();
   OMR::JitBuilder::IlValue *rc23Equal = equiv1->EqualTo(
                                         equiv1->   Load("rc2"),
                                         equiv1->   Load("rc3"));
   IlBuilder *equiv3 = OrphanBuilder();
   OMR::JitBuilder::IlValue *rc34Equal = equiv1->EqualTo(
                                         equiv1->   Load("rc3"),
                                         equiv1->   Load("rc4"));

   IlBuilder *returnTrue = NULL, *returnFalse = NULL;
   IfAnd(&returnTrue, &returnFalse, 3,
      MakeCondition(equiv1, rc12Equal),
      MakeCondition(equiv2, rc23Equal),
      MakeCondition(equiv3, rc34Equal));

   returnTrue->Return(
   returnTrue->   ConstInt32(1));

   returnFalse->Return(
   returnFalse->   ConstInt32(0));

   return true;
   }

int
main(int argc, char *argv[])
   {
   cout << "Step 1: initialize JIT\n";
   bool initialized = initializeJit();
   if (!initialized)
      {
      cerr << "FAIL: could not initialize JIT\n";
      exit(-1);
      }

   cout << "Step 2: define type dictionary\n";
   OMR::JitBuilder::TypeDictionary types;

   cout << "Step 3: compile method builder\n";
   ConditionalMethod method(&types);
   void *entry = 0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "Step 4: invoke compiled code\n";
   ConditionalMethodFunction *compare = (ConditionalMethodFunction *) entry;
   int32_t x, L, U, pass, total;

   total=0; pass=0;

   L=0, U=10000; x=50;        total++; if (compare(x, L, U) != 0) pass++;
   L=0, U=10000; x=L;         total++; if (compare(x, L, U) != 0) pass++;
   L=0, U=10000; x=L-1;       total++; if (compare(x, L, U) != 0) pass++;
   L=0, U=10000; x=-50000;    total++; if (compare(x, L, U) != 0) pass++;
   L=0, U=10000; x=U-1;       total++; if (compare(x, L, U) != 0) pass++;
   L=0, U=10000; x=U;         total++; if (compare(x, L, U) != 0) pass++;
   L=0, U=10000; x=50000;     total++; if (compare(x, L, U) != 0) pass++;

   L=-25, U=25; x=0;          total++; if (compare(x, L, U) != 0) pass++;
   L=-25, U=25; x=L;          total++; if (compare(x, L, U) != 0) pass++;
   L=-25, U=25; x=L-1;        total++; if (compare(x, L, U) != 0) pass++;
   L=-25, U=25; x=-100;       total++; if (compare(x, L, U) != 0) pass++;
   L=-25, U=25; x=U-1;        total++; if (compare(x, L, U) != 0) pass++;
   L=-25, U=25; x=U;          total++; if (compare(x, L, U) != 0) pass++;
   L=-25, U=25; x=100;        total++; if (compare(x, L, U) != 0) pass++;

   L=-1000, U=-900; x=-950;   total++; if (compare(x, L, U) != 0) pass++;
   L=-1000, U=-900; x=L;      total++; if (compare(x, L, U) != 0) pass++;
   L=-1000, U=-900; x=L-1;    total++; if (compare(x, L, U) != 0) pass++;
   L=-1000, U=-900; x=-10000; total++; if (compare(x, L, U) != 0) pass++;
   L=-1000, U=-900; x=U-1;    total++; if (compare(x, L, U) != 0) pass++;
   L=-1000, U=-900; x=U;      total++; if (compare(x, L, U) != 0) pass++;
   L=-1000, U=-900; x=-500;   total++; if (compare(x, L, U) != 0) pass++;

   cout << "Step 5: shutdown JIT\n";
   shutdownJit();

   if (pass == total)
      cout << "ALL TESTS PASSED\n";
   else
      cout << pass << " out of " << total << " passed\n";

   return pass - total; // == 0 if all passed
   }
