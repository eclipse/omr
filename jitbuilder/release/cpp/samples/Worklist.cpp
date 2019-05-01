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


#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <cstring>

#include "Worklist.hpp"

using std::cout;
using std::cerr;

#define REPORT_BCI(bci,e_bci)   { if ((bci) != (e_bci)) { cerr << "Error: bytecode " << (bci) << " not found in worklist ( expected " << (e_bci) << ")\n"; return false; } }

#define REPORT_VMSTATE(b,s)

//#define REPORT_VMSTATE(b,s)     { if ((b)->vmState() != (s)) { cerr << "Error: bytecode " << ((b)->bcIndex()) << " does not have correct vmState (" << (s) << ")\n"; return false; } }

#define REPORT_RESULT(p,r,e)    { if ((r) == (e)) { success++; } else { cerr << "Error: path " << (p) << " generated wrong result (" << (r) << " versus " << (e) << ")\n"; fails++; } }


static bool verbose = false;
static int32_t fails = 0;
static int32_t success = 0;

// BC0:  r = p;
// BC1:  r++;
//       if (r > 10) goto BC3;
// BC2:  if (r > 5)  goto BC5;
// BC4:  if (r < 3)  goto BC1; // loop back
// BC9:  return 9;
//
// BC5:  if (r < 8) goto BC11;
// BC17: return 17;
//
// BC7:  if (r < 17) goto BC1; // loop back
// BC8:  return 8;
//
//
// BC3:  if (r > 15) goto BC7;
// BC6:  if (r > 12) goto BC13;
// BC16: return 16;

// BC7:  return 7;
// BC11: return 11;
// BC13: return 13;

// no BC10, BC12, BC14, BC15, BC18, BC19

int32_t
computeResult(int32_t p)
   {
   // BC0
   int32_t r = p;
BC1:
   r++;
   if (r > 10)
      {
      // BC3
      if (r > 15)
         {
         // BC7
         if (r < 17)
            goto BC1;
         else
            return 8; // BC8
         }
      else
         {
         // BC6
         if (r > 12)
            return 13; // BC13
         else
            return 16; // BC16
         }
      }
   else
      {
      // BC2
      if (r > 5)
         {
         // BC5
         if (r < 8)
            return 11; // BC11
         else
            return 17; // BC17
         }
      else
         {
         // BC4
         if (r < 3)
            goto BC1;
         else
            return 9; // BC9
         }
      }
   }


int
main(int argc, char *argv[])
   {
   if (argc == 2 && strcmp(argv[1], "--verbose") == 0)
      verbose = true;

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
   WorklistMethod method(&types);
   void *entry = 0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      cerr << "fail: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "step 4: invoke compiled code and print results\n";
   typedef int32_t (WorklistMethodFunction)(int32_t);
   WorklistMethodFunction *testPath = (WorklistMethodFunction *) entry;

   const int32_t expectation[20] = {
       9,  9,  9,  9,  9, 11, 11, 17, 17, 17,
      16, 16, 13, 13, 13,  8,  8,  8,  8,  8 };

   for (int32_t p=0;p < 20;p++)
      {
      int32_t r = testPath(p);
      if (verbose) cout << "\ttestPath(" << p << ") == " << testPath(p) << "\n";
      //REPORT_RESULT(p, r, expectation[p]);
      REPORT_RESULT(p, r, computeResult(p));
      }

   cout << "Step 5: shutdown JIT\n";

   cout << "Failed tests: " << fails << "\n";
   cout << "Passed tests: " << success << "\n";
   if (fails == 0)
      cout << "ALL PASSED\n";

   shutdownJit();
   }



WorklistMethod::WorklistMethod(OMR::JitBuilder::TypeDictionary *d)
   : OMR::JitBuilder::MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("testPath");
   DefineParameter("path", Int32);
   DefineReturnType(Int32);
   }

bool
WorklistMethod::buildIL()
   {
   cout << "WorklistMethod::buildIL() running!\n";

   const char *bcName[20] = {
      "bc0",  "bc1",  "bc2",  "bc3",  "bc4",  "bc5",  "bc6",  "bc7",  "bc8",  "bc9",
      "bc10", "bc11", "bc12", "bc13", "bc14", "bc15", "bc16", "bc17", "bc18", "bc19" };

   OMR::JitBuilder::BytecodeBuilder *builders[20];
   for (int32_t i=0;i < 20;i++)
      builders[i] = OrphanBytecodeBuilder(i, (char *)bcName[i]);

   OMR::JitBuilder::VirtualMachineState *vmState = new OMR::JitBuilder::VirtualMachineState();
   setVMState(vmState);

   Store("result",
      Load("path"));

   // should add 0 to the worklist
   AppendBytecodeBuilder(builders[0]);

   OMR::JitBuilder::BytecodeBuilder *b;

   // BCI 0
   int32_t bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,0);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->AddFallThroughBuilder(builders[1]);

   // BCI 1
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,1);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Store("result",
   b->   Add(
   b->      Load("result"),
   b->      ConstInt32(1)));
   b->IfCmpGreaterThan(builders[3],
   b->   Load("result"),
   b->   ConstInt32(10));
   b->AddFallThroughBuilder(builders[2]);

   // BCI 2
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,2);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpGreaterThan(builders[5],
   b->   Load("result"),
   b->   ConstInt32(5));
   b->AddFallThroughBuilder(builders[4]);

   // BCI 3
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,3);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpGreaterThan(builders[7],
   b->   Load("result"),
   b->   ConstInt32(15));
   b->AddFallThroughBuilder(builders[6]);

   // BCI 4
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,4);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpLessThan(builders[1], // should not revisit 1
   b->   Load("result"),
   b->   ConstInt32(3));
   b->AddFallThroughBuilder(builders[9]);

   // BCI 5
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,5);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpLessThan(builders[11],
   b->   Load("result"),
   b->   ConstInt32(8));
   b->AddFallThroughBuilder(builders[17]);

   // BCI 6
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,6);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpGreaterThan(builders[13],
   b->   Load("result"),
   b->   ConstInt32(12));
   b->AddFallThroughBuilder(builders[16]);

   // BCI 7
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,7);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->IfCmpLessThan(builders[1],   // should not revisit 1
   b->   Load("result"),
   b->   ConstInt32(17));
   b->AddFallThroughBuilder(builders[8]);

   // BCI 8
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,8);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(8));

   // BCI 9
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,9);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(9));

   // BCI 11
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,11);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(11));

   // BCI 13
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,13);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(13));

   // BCI 16
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,16);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(16));

   // BCI 17
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,17);
   b = builders[bci];
   REPORT_VMSTATE(b,vmState);
   b->Return(
   b->   ConstInt32(17));

   // should be no unvisited bytecodes left
   bci = GetNextBytecodeFromWorklist();
   REPORT_BCI(bci,-1);

   return true;
   }
