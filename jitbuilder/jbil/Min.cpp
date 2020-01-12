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

class Min : public FunctionBuilder
   {
public:
   Min(TypeDictionary * types)
      : FunctionBuilder(types)
      {
      DefineName("min");
      DefineLine(LINETOSTR(__LINE__));
      DefineFile(__FILE__);
      DefineParameter("x", Int32);
      DefineParameter("y", Int32);
      DefineReturnType(Int32);
      }

   virtual bool buildIL()
      {
      Store("m", Load("x"));
      Builder *ymin = OrphanBuilder();
      Builder *merge = OrphanBuilder();
      IfCmpLessThan(ymin, Load("y"), Load("x"));
         {
         ymin->Store("m", ymin->Load("y"));
         ymin->Goto(merge);
         }
      AppendBuilder(merge);
      Return(Load("m"));
      return true;
      }
   };

typedef int32_t (MinFunc)(int32_t, int32_t);

int
main(int argc, char *argv[])
   {
   bool success = initializeJit();
   if (!success)
      {
      printf("initializeJit failure\n");
      exit(-1);
      }

   TypeDictionary types("Min_Types");
   Min method(&types);
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
   MinFunc *minfunc = method.DebugEntry<MinFunc>(&rc);
   if (rc != 0)
      {
      printf("Simulation request returned error code %d\n", rc);
      exit(rc);
      }

   int32_t mlae = minfunc(42, 45);
   printf("rv is %d\n", mlae);

   mlae = minfunc(45, 42);
   printf("rv is %d\n", mlae);

   shutdownJit();

   exit(0);
   }
