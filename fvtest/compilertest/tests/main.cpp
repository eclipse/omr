/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include "il/DataTypes.hpp"
#include "gtest/gtest.h"
#include "OMRTestEnv.hpp"

#if defined(OMR_OS_WINDOWS)
#undef BYTE
#include "windows.h"
#define PATH_MAX MAXPATHLEN
#else
#include <dlfcn.h>
#endif /* defined(OMR_OS_WINDOWS) */

int main(int argc, char **argv)
   {
   bool useOMRTestEnv = true;

   /* Disable OMRTestEnv on some tests. This is needed when the test
    * wants to initialize the JIT with special options which cannot be
    * easily cleaned up.
    */
   const char *exitAssertFlag="--gtest_internal_run_death_test=";
   for(int i = 0; i < argc; ++i)
      {
      if(!strncmp(argv[i], exitAssertFlag, strlen(exitAssertFlag)))
         if(strstr(argv[i], "LimitFileTest.cpp") || strstr(argv[i], "LogFileTest.cpp"))
            {
            useOMRTestEnv = false;
            }
      }

   ::testing::InitGoogleTest(&argc, argv);

   if(useOMRTestEnv)
      ::testing::AddGlobalTestEnvironment(new TestCompiler::OMRTestEnv);

   return RUN_ALL_TESTS();
   }
