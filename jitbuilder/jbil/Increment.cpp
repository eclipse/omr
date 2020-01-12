/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "JitBuilder.hpp"

using namespace OMR::JitBuilder;

class Increment : public FunctionBuilder
   {
public:
   Increment(TypeDictionary * types)
      : FunctionBuilder(types)
      {
      DefineName("Increment");
      DefineLine(LINETOSTR(__LINE__));
      DefineFile(__FILE__);
      DefineParameter("value", Int32);
      DefineReturnType(Int32);
      }

   virtual bool buildIL()
      {
      Return(Add(Load("value"), ConstInt32(1)));

      #if 0
         Builder *b = OrphanBuilder();
         AppendBuilder(b);
      #endif

      return true;
      }
   };

typedef int32_t (IncrementFunc)(int32_t);

int
main(int argc, char *argv[])
   {
   bool success = initializeJit();
   if (!success)
      {
      printf("initializeJit failure\n");
      exit(-1);
      }

   TypeDictionary types("Increment_Types");
   Increment method(&types);
   success = method.Construct();
   if (!success)
      {
      printf("construction failure\n");
      exit(-2);
      }


   std::cerr.setf(std::ios_base::skipws);
   std::cerr << "Method to debug:" << std::endl;
   OMR::JitBuilder::TextWriter printer(&method, std::cout, std::string("    "));
   method.setLogger(&printer);
   printer.print();

   int32_t rc=0;
   IncrementFunc *func = method.DebugEntry<IncrementFunc>(&rc);
   if (rc != 0)
      {
      printf("Simulation request returned error code %d\n", rc);
      exit(rc);
      }

   int32_t mlae = func(41);
 
   printf("rv is %d\n", mlae);

   shutdownJit();

   exit(0);
   }
