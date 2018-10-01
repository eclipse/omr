/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
#include <string>
#include <cstring>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/JitBuilderRecorderTextFile.hpp"
#include "RecordReplayTest.hpp"

// Uncomment line on hpp file to enable REPLAY functionality 
#ifdef REPLAY
#include "ilgen/JitBuilderReplayTextFile.hpp"
#include "ilgen/MethodBuilderReplay.hpp"
#endif

using std::cout;
using std::cerr;

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
   TR::TypeDictionary types;
   // Create a recorder so we can directly control the file for this particular test
   TR::JitBuilderRecorderTextFile recorder(NULL, "recordReplayIL.ilt"); // .ilt (intermediate language text)

   cout << "Step 3: compile method builder\n";
   RecordReplayTest method(&types, &recorder);

   #ifdef REPLAY
   int32_t rc = recordMethodBuilder(&method);
   #endif

   #ifndef REPLAY 
   uint8_t *entry1 = 0;
   int32_t rc = compileMethodBuilder(&method, &entry1); // Process buildIL
   #endif

   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }
  
   int32_t v, t;

   // **********************************************************************
   // ******************************** REPLAY ******************************
   // **********************************************************************

   #ifdef REPLAY

   cout << "********** REPLAY ON **********\n";
   uint8_t *entry2 = 0;
   cout << "Step 4: Replay\n";
   TR::JitBuilderReplayTextFile replay("recordReplayIL.ilt");
   TR::JitBuilderRecorderTextFile recorder2(NULL, "recordReplayILReplay.ilt");

   TR::TypeDictionary types2;

   cout << "Step 5: verify output file\n";
   TR::MethodBuilderReplay mb(&types2, &replay, &recorder2); // Process Constructor
   rc = compileMethodBuilder(&mb, &entry2); // Process buildIL

   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "Step 6: run compiled code from replay\n";

   RecordReplayMethodFunction *incrementTwo = (RecordReplayMethodFunction *) entry2;
   v=0, t=5; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";
   v=1, t=2; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";
   v=10, t=15; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";
   v=-15, t=7; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";

   cout << "Step 7: shutdown JIT\n";
   #endif

   #ifndef REPLAY

   cout << "********** REPLAY OFF **********\n";
   cout << "Step 4: run compiled code\n";
   RecordReplayMethodFunction *incrementTwo = (RecordReplayMethodFunction *) entry1;

   v=0, t=5; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";
   v=1, t=2; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";
   v=10, t=15; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";
   v=-15, t=7; cout << "incrementTwo(" << v << "+" << t << ") == " << incrementTwo(v,t) << "\n";

   cout << "Step 5: shutdown JIT\n";
   #endif
  
   shutdownJit();

   printf("PASS\n");
   }


RecordReplayTest::RecordReplayTest(TR::TypeDictionary *d, TR::JitBuilderRecorder *recorder)
   : MethodBuilder(d, NULL, recorder)
   {

   DefineLine(LINETOSTR(__LINE__));
   
   DefineFile(__FILE__);

   DefineName("incrementTwo");

   DefineParameter("value1", Int32);
   DefineParameter("value2", Int32);
   DefineReturnType(Int32);
   }

bool
RecordReplayTest::buildIL()
   {
   cout << "RecordReplayTest::buildIL() running!\n";

   Return(
      Add(
         Load("value1"),
         Load("value2")));

   return true;
   }
