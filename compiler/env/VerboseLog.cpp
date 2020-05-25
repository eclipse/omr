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

#include "env/VerboseLog.hpp"
#include "infra/Assert.hpp"

void *TR_VerboseLog::_config = 0;

const char * TR_VerboseLog::_vlogTable[] =
   {
   0,
   "#INFO:  ",
   "#SAMPLE:  ",
   "#IPROFILER:  ",
   "#CODECACHE:  ",
   "#JITSTATE:  ",
   "#PERF:  ",
   "#MEMORY:  ",
   "#FAILURE:  ",
   "#JITDUMP:  ",
   "#SCHINTS:  ",
   "#GCCYCLE:  ",
   "#J2I:  ",
   "#CR:  ",
   "#RA:  ",
   "#HK:  ",
   "#HD:  ",
   "#MH:   ", // extra space to line up with MHd
   "#MHd:  ",
   "#DLT:  ",
   "+ ",      // TR_Vlog_COMP
   " ",       // TR_Vlog_COMPSTART
   "! ",      // TR_Vlog_COMPFAIL
   "-precompileMethod ",
   " ",       // TR_Vlog_MMAP
   "#OSR:  ", // extra space to line up with OSRd
   "#OSRd: ",
   "#HWP: ",
   "#INL: ",
   "#GC: ",
   "[IBM GPU JIT]: ",  // to be consistent with JCL GPU output
   "#PATCH : ",
   "#DISPATCH: ",
   "#RECLAMATION: ",
   "#PROFILING: ",
   "#JITServer: ",
   "#AOTCOMPRESSION: ",
   };

void TR_VerboseLog::writeLine(TR_VlogTag tag, const char *format, ...)
   {
   TR_ASSERT(tag != TR_Vlog_null, "TR_Vlog_null is not a valid Vlog tag");
   va_list args;
   va_start(args, format);
   write("\n");
   writeTimeStamp();
   write(_vlogTable[tag]);
   vwrite(format,args);
   va_end(args);
   }

void TR_VerboseLog::writeLineLocked(TR_VlogTag tag, const char *format, ...)
   {
   TR_ASSERT(tag != TR_Vlog_null, "TR_Vlog_null is not a valid Vlog tag");
   vlogAcquire();
   va_list args;
   va_start(args, format);
   write("\n");
   writeTimeStamp();
   write(_vlogTable[tag]);
   vwrite(format,args);
   va_end(args);
   vlogRelease();
   }

void TR_VerboseLog::write(const char *format, ...)
   {
   va_list args;
   va_start(args, format);
   vwrite(format,args);
   va_end(args);
   }

void TR_VerboseLog::initialize(void *config)
   {
   _config = config;
   }
