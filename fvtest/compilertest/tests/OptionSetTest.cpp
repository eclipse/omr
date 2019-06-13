/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#if defined(GTEST_HAS_DEATH_TEST)
#include "tests/OptionSetTest.hpp"

#include <fstream>
#include <string>

/**
 * Get the method that was compiled on a given line. The line
 * must be a compilation success line (starting with '+')
 * from a verbose log, without a trailing newline,
 * 
 * @param line The line to extract the method name from.
 * @return The name of the method, or an empty string if
 *         the method name could not be extracted.
 */
std::string
TestCompiler::OptionSetTest::getMethodFromLine(const std::string &line)
   {
   size_t sep = line.find_last_of(':');
   size_t end = line.find_first_of(' ', sep);
   if(sep == std::string::npos)
      return "";
   return line.substr(sep + 1, end - (sep + 1));
   }

/**
 * Take a limit file \p limitFile, modify it by placing some methods
 * into option sets according to \p methods, then place the result
 * into a new file \p optSetFile.
 *
 * @param limitFile The name of the limit file to modify.
 * @param optSetFile The name of the file to create. The
 *        contents will be the same as \p limitFile, with
 *        some methods in option sets.
 * @param methods A mapping from method name to the set
 *        which it should be in. If a method is not specified
 *        in this map, it is not placed in any set.
 */
void
TestCompiler::OptionSetTest::applyOptionSets(const char *limitFile, const char *optSetFile, const TestCompiler::MethodSets &methods)
   {
   std::ifstream in(limitFile);
   ASSERT_TRUE(in.is_open());

   std::ofstream out(optSetFile);
   ASSERT_TRUE(out.is_open());
   delayUnlink(optSetFile);

   std::string line;
   while(std::getline(in, line))
      {
      // Limitfiles have a leading newline, but no newline on the end
      out << '\n';

      if(line.empty())
         continue;

      // Method successfully compiled
      if(line[0] == '+')
         {
         std::string compMethod = getMethodFromLine(line);
         auto search = methods.find(compMethod);
         if(search != methods.end())
            {
            int set = search->second;
            ASSERT_GE(set, 0);
            ASSERT_LE(set, 9);
            out << '+' << set << line.substr(1);
            continue;
            }
         }

      // Normal line, output and continue
      out << line;
      }
   }

namespace TestCompiler {

TEST_F(OptionSetTest, UseOptionSets)
   {
   const char *vlog = "useOptionSets.log";
   const char *limitFile = "useOptionSets.limit";
   const char *optSet = "useOptionSets.options";

   // Create limit file.
   TestCompiler::MethodSets methodSets;
   methodSets["iNeg"] = 2;
   methodSets["iReturn"] = 3;

   createAndCheckVLog(limitFile);

   // Run with option sets.
   applyOptionSets(limitFile, optSet, methodSets);

   std::string limitArg = std::string(optSet)
      + ",2(optlevel=hot)"
      + ",3(optlevel=cold)";

   createVLog(vlog, limitArg.c_str());

   // Ensure option sets were follwed.
   checkVLogForMethod(vlog, "iNeg", "hot");
   checkVLogForMethod(vlog, "iReturn", "cold");
   checkVLogForMethod(vlog, "iAbs", "warm");
   }

TEST_F(OptionSetTest, WithDefault)
   {
   const char *vlog = "optionSetsWithDefault.log";
   const char *limitFile = "optionSetsWithDefault.limit";
   const char *optSet = "optionSetsWithDefault.options";

   // Create limit file.
   TestCompiler::MethodSets methodSets;
   methodSets["iNeg"] = 1;
   methodSets["i2l"] = 2;
   methodSets["iReturn"] = 3;
   methodSets["i2b"] = 3;
   methodSets["i2s"] = 9;

   createAndCheckVLog(limitFile);

   // Run with option sets.
   applyOptionSets(limitFile, optSet, methodSets);

   std::string limitArg = std::string(optSet)
      + ",1(optlevel=hot)"
      + ",3(optlevel=warm)"
      + ",9(optlevel=warm)"
      + ",optlevel=cold";

   createVLog(vlog, limitArg.c_str());

   // Ensure option sets were follwed.
   checkVLogForMethod(vlog, "iNeg", "hot");
   checkVLogForMethod(vlog, "i2l", "cold");
   checkVLogForMethod(vlog, "iReturn", "warm");
   checkVLogForMethod(vlog, "i2b", "warm");
   checkVLogForMethod(vlog, "i2s", "warm");
   checkVLogForMethod(vlog, "iAbs", "cold");
   }

}
#endif /* defined(GTEST_HAS_DEATH_TEST) */
