/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#ifndef VERBOSELOG_HPP
#define VERBOSELOG_HPP

#pragma once

#include <stdarg.h>

/*
 * Each call to the vlog must now have a 'tag'
 * When adding a tag, update the table in VerboseLog.cpp
 * Tags should be of the format '\n#TAGNAME:  '
 */
enum TR_VlogTag
   {
   TR_Vlog_null=0,

   TR_Vlog_INFO,
   TR_Vlog_SAMPLING,
   TR_Vlog_IPROFILER,
   TR_Vlog_CODECACHE,
   TR_Vlog_JITSTATE,
   TR_Vlog_PERF,
   TR_Vlog_MEMORY,
   TR_Vlog_FAILURE,
   TR_Vlog_JITDUMP,
   TR_Vlog_SCHINTS,
   TR_Vlog_GCCYCLE,
   TR_Vlog_J2I,
   TR_Vlog_CR,       //(compilation request)
   TR_Vlog_RA,       //(runtime assumption)
   TR_Vlog_HK,       //(hook)
   TR_Vlog_HD,       //(hook details)
   TR_Vlog_MH,       //(method handle)
   TR_Vlog_MHD,      //(method handle details)
   TR_Vlog_DLT,
   TR_Vlog_COMP,
   TR_Vlog_COMPSTART,
   TR_Vlog_COMPFAIL,
   TR_Vlog_PRECOMP,
   TR_Vlog_MMAP,
   TR_Vlog_OSR,      //(on-stack replacement)
   TR_Vlog_OSRD,     //(on-stack replacement details)
   TR_Vlog_HWPROFILER,
   TR_Vlog_INL,      //(inlining)
   TR_Vlog_GC,
   TR_Vlog_GPU,
   TR_Vlog_PATCH,
   TR_Vlog_DISPATCH,
   TR_Vlog_RECLAMATION,
   TR_Vlog_PROFILING,
   TR_Vlog_JITServer,
   TR_Vlog_AOTCOMPRESSION,
   TR_Vlog_BI,       //(benefit inliner)
   TR_Vlog_FSD,
   TR_Vlog_numTags
   };

/*
 * TR_VerboseLog stores the config of the jit early on, and can use it for printing to the verbose log
 * Each verbose write should have a Tag (see TR_VlogTag for list of options).  The tag is of the form: '\n#TAG:  '
 */
class TR_VerboseLog
   {
   public:
   //writeLine and write provide multi line write capabilities, and are to be used with vlogAcquire() and vlogRelease(),
   //this ensures nice formatted verbose logs
   static void write(const char *format, ...);
   static void write(TR_VlogTag tag, const char *format, ...);
   static void writeLine(TR_VlogTag tag, const char *format, ...);
   static void writeLine(const char *format, ...);
   //writeLineLocked is a single line print function, it provides the locks for you
   static void writeLineLocked(TR_VlogTag tag, const char *format, ...);
   static void vlogAcquire(); //defined in each front end
   static void vlogRelease(); //defined in each front end
   //only called once early on, after we can print to the vlog
   static void initialize(void *config);
   private:
   static void vwrite(const char *format, va_list args); //defined in each front end
   static void writeTimeStamp();
   static const char *_vlogTable[];
   static void *_config; //we will use void * as the different implementations have different config types
   };

#endif // VERBOSELOG_HPP
