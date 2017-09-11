/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_IO_INCL
#define OMR_IO_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_IO_CONNECTOR
#define OMR_IO_CONNECTOR
namespace OMR { class IO; }
namespace OMR { typedef OMR::IO IOConnector; }
#endif

#include <stddef.h>                 // for NULL
#include <stdint.h>                 // for int32_t, intptr_t
#include "env/FilePointerDecl.hpp"  // for FILE
#include "infra/Annotations.hpp"    // for OMR_EXTENSIBLE

#include "env/FilePointer.hpp"

/* All compilers now use the same format string for signed/unsigned 64-bit values.
 *
 * NOTE: this applies to only those "printf" functions that are handled by the port
 *       library.  If you call a C runtime "printf" or "sprintf", for example, then
 *       that runtime may dictate the format specifiers you may use.
*/
#define INT64_PRINTF_FORMAT  "%lld"
#define INT64_PRINTF_FORMAT_HEX "0x%llx"
#define UINT64_PRINTF_FORMAT "%llu"
#define UINT64_PRINTF_FORMAT_HEX "0x%llx"

#ifdef _MSC_VER
   #define POINTER_PRINTF_FORMAT "0x%p"
#else
   #if defined(LINUX) || defined(OSX)
      #ifdef TR_HOST_64BIT
         #ifdef TR_TARGET_X86
            #define POINTER_PRINTF_FORMAT "%012p"
         #else
            #define POINTER_PRINTF_FORMAT "%018p"
         #endif
      #else
         #define POINTER_PRINTF_FORMAT "%010p"
      #endif
   #else /* assume AIX and ZOS */
      #define POINTER_PRINTF_FORMAT "0x%p"
   #endif
#endif

extern TR::FILE *(*trfopen)(char *fileName, const char *attrs, bool encrypt);
extern void (*trfclose)(TR::FILE *fileId);
extern void (*trfflush)(TR::FILE *fileId);
extern int32_t (*trfprintf)(TR::FILE *fileId, const char *format, ...);
extern int32_t (*trprintf)(const char *format, ...);

namespace OMR
{

class OMR_EXTENSIBLE IO
   {
   public:

   static TR::FILE *Null;

   static TR::FILE *Stdin;

   static TR::FILE *Stdout;

   static TR::FILE *Stderr;

   static TR::FILE *fopen(char *fileName, const char *attrs, bool encrypt);

   static void fclose(TR::FILE *fileId);

   static void fseek(TR::FILE *fileId, intptr_t offset, int32_t whence);

   static long ftell(TR::FILE *fileId);

   static void fflush(TR::FILE *fileId);

   static int32_t printf(const char *format, ...);

   static int32_t fprintf(TR::FILE *fileId, const char *format, ...);

   static int32_t vfprintf(TR::FILE *fileId, const char *format, va_list args);

   };

}

#endif
