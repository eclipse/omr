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

#include "env/IO.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "env/FilePointer.hpp"
#include "env/FilePointerDecl.hpp"

#ifdef LINUX
#include <unistd.h>
#endif

class TR_FrontEnd;

TR::FILE * OMR::IO::Null = TR::FilePointer::Null();
TR::FILE * OMR::IO::Stdin = TR::FilePointer::Stdin();
TR::FILE * OMR::IO::Stdout = TR::FilePointer::Stdout();
TR::FILE * OMR::IO::Stderr = TR::FilePointer::Stderr();


TR::FILE *
OMR::IO::fopen(char *fileName, const char *mode, bool encrypt)
   {
   return (TR::FILE *) ::fopen(fileName, mode);
   }


void
OMR::IO::fclose(TR::FILE *fileId)
   {
   ::fclose((::FILE *)fileId);
   }


void
OMR::IO::fseek(TR::FILE *fileId, intptr_t offset, int32_t whence)
   {
   ::fseek((::FILE *)fileId, offset, whence);
   }


long
OMR::IO::ftell(TR::FILE *fileId)
   {
   return ::ftell((::FILE *)fileId);
   }


void
OMR::IO::fflush(TR::FILE *fileId)
   {
   ::fflush((::FILE *)fileId);
   }


int32_t
OMR::IO::printf(const char * format, ...)
   {
   va_list args;
   va_start(args, format);
   int32_t length = ::vprintf(format, args);
   va_end(args);
   return length;
   }


int32_t
OMR::IO::fprintf(TR::FILE *fileId, const char * format, ...)
   {
   va_list args;
   va_start(args, format);
   int32_t length = ::vfprintf((::FILE*)fileId, format, args);
   va_end(args);
   return length;
   }


int32_t
OMR::IO::vfprintf(TR::FILE *fileId, const char *format, va_list args)
   {
   return ::vfprintf((::FILE*)fileId, format, args);
   }



TR::FILE *(*trfopen)(char *fileName, const char *attrs, bool encrypt) = TR::IO::fopen;
void (*trfclose)(TR::FILE *fileId) = TR::IO::fclose;
void (*trfflush)(TR::FILE *fileId) = TR::IO::fflush;
int32_t (*trfprintf)(TR::FILE *fileId, const char *format, ...) = TR::IO::fprintf;
int32_t (*trprintf)(const char *format, ...) = TR::IO::printf;
