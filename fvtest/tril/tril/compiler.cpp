/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "default_compiler.hpp"
#include "Jit.hpp"
#include <cstdio>
#include <string>

int main(int argc, char** argv)
   { 

   FILE* out = stdout; 
   FILE* in = stdin;

   std::string program_name = argv[0];

   bool isDumper = program_name.find("tril_dumper") != std::string::npos;  

   if (2 == argc)
      {
      in = fopen(argv[1], "r"); 
      }

   auto trees = parseFile(in);

   if (trees)
      {
      printTrees(out, trees, 1); 
      if (!isDumper) 
         {
         initializeJit();
         Tril::DefaultCompiler compiler(trees); 
         if (compiler.compile() != 0) { 
            fprintf(out, "Error compiling trees!"); 
         }
         shutdownJit();
         }
      }
   else
      { 
      fprintf(out, "Parse error\n");
      }


   if (2 == argc)
      {
      fclose(in);
      }
   }
