/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_COMPILE_TIME_PROFILER_HPP
#define OMR_COMPILE_TIME_PROFILER_HPP

#include "compile/Compilation.hpp"

#include <stdlib.h>
#include <stdio.h>

#if defined(LINUX)

#include <sys/wait.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace TR {

/**
 * Utility to generate a profile for the current compilation,
 * to assist in determining the source of compile time overhead. This
 * is useful in situations where there is a particularly long running
 * compile, such as a profiled very-hot or scorching compile, but profiling
 * all compilations results in too much noise.
 *
 * This implementation is Linux specific, due to its reliance on perf to
 * generate the profile.
 *
 * It will fork and execute perf, configured such that it will
 * profile the current compilation thread until the current compilation ends.
 *
 * Perf, by default, is configured to collect call graph information at a
 * high frequency, with the following options:
 *   --call-graph -F9999
 * These defaults can be overridden using the TR_CompileTimeProfiler env var.
 *
 * The current implementation only supports perf, but other profiling
 * utilities could be introduced. 
 *
 * In testing, the initial portion of the compile was lost, due to perf's
 * start up. A delay has been added to manage this.
 */
class CompileTimeProfiler
   {
public:
   CompileTimeProfiler(TR::Compilation *comp, const char *identifier)
      {
      _pid = 0;
      if (comp->getOption(TR_CompileTimeProfiler))
         {
         static char *cacheOptions[_optionsLength];
         static size_t filenamePos;
         static size_t threadIDPos;

         char timestr[_timeLength];
         time_t timer = time(NULL);
         snprintf(timestr, sizeof(timestr), "%i", (int32_t)timer % 100000);
         
         char tidstr[_threadIDLength];
         snprintf(tidstr, sizeof(tidstr), "%d", syscall(SYS_gettid));

         // Construct the output filename, with the thread id and time in seconds
         char filename[_fileLength];
         snprintf(filename, sizeof(filename), "%s.%s.%s.data", identifier ? identifier : "perf", tidstr, timestr);

         // Build up the constant options, with env var override
         if (filenamePos == 0) 
            parseOptions(cacheOptions, filenamePos, threadIDPos); 

         // Copy and specialize options
         char *options[_optionsLength];
         memcpy(options, cacheOptions, sizeof(options));
         options[filenamePos] = filename;
         options[threadIDPos] = tidstr;

         // Dump the options
         if (TR::Options::getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::write("\nProfiling %s compile time with:", comp->signature());
            char **iter = options;
            while (*iter)
               {
               TR_VerboseLog::write(" %s", *iter);
               iter++;
               }
            TR_VerboseLog::write("\n");
            TR_VerboseLog::vlogRelease();
            }

         _pid = fork();
         if (_pid == 0)
            {
            execv("/usr/bin/perf", options);
            exit(0);
            }

         // Give perf some time to start
         usleep(_initMicroSecDelay);
         }
      } 

   ~CompileTimeProfiler()
      {
      if (_pid != 0)
         {
         // Kill perf at the end of the compile
         kill(_pid, SIGINT);
         waitpid(_pid, NULL, 0);
         }
      }

private:
    void parseOptions(char *options[], size_t &filenamePos, size_t &threadIDPos)
      {
      size_t pos = 0;
      options[pos++] = "perf";
      options[pos++] = "record";
      options[pos++] = "-o";
      filenamePos = pos;
      options[pos++] = NULL;
      options[pos++] = "-t";
      threadIDPos = pos;
      options[pos++] = NULL;

      static char *envOptions = feGetEnv("TR_CompileTimeProfiler");
      if (envOptions)
         {
         // A string of length optionsLength cannot specify enough
         // space separated arguments to overflow the options array
         static char buffer[_optionsLength];
         snprintf(buffer, sizeof(buffer), "%s", envOptions);

         // Replace spaces with null terminators and
         // add words to options list
         bool newArg = true;
         char *iter = buffer;
         while (*iter)
            {
            if (*iter == ' ')
               {
               *iter = '\0';
               newArg = true; 
               }
            else if (newArg)
               {
               options[pos++] = iter;
               newArg = false;
               }
            iter++;
            }
         }
      else
         {
         options[pos++] = "--call-graph";
#if defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)
         options[pos++] = "dwarf";
#endif
         options[pos++] = "-F9999";
         }
      options[pos] = NULL;
      }

   static const uint32_t _timeLength        = 16;
   static const uint32_t _threadIDLength    = 16;
   static const uint32_t _fileLength        = 1024;
   static const uint32_t _optionsLength     = 1024;
   static const uint32_t _initMicroSecDelay = 250000;
   pid_t _pid;
   };

};

#else

namespace TR {

class CompileTimeProfiler
   {
public:
   CompileTimeProfiler(TR::Compilation *comp, const char *identifier)
      {
      }

   ~CompileTimeProfiler()
      {
      }
   };

};

#endif

#endif
