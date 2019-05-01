/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include <stdint.h>
#include <cstring>

#include "infra/Assert.hpp"
#include "ilgen/JitBuilderRecorderTextFile.hpp"

OMR::JitBuilderRecorderTextFile::JitBuilderRecorderTextFile(const TR::MethodBuilder *mb, const char *fileName)
   : TR::JitBuilderRecorder(mb, fileName)
   {
   }

void
OMR::JitBuilderRecorderTextFile::Close()
   {
   TR::JitBuilderRecorder::Close();
   _file.close();
   }

void
OMR::JitBuilderRecorderTextFile::String(const char * const string)
   {
   _file << "\"" << strlen(string) << " [" << string << "]\" ";
   }

void
OMR::JitBuilderRecorderTextFile::Number(int8_t num)
   {
   _file << (int32_t) num << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Number(int16_t num)
   {
   _file << num << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Number(int32_t num)
   {
   _file << num << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Number(int64_t num)
   {
   _file << num << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Number(float num)
   {
   _file << num << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Number(double num)
   {
   _file << num << " ";
   }

void
OMR::JitBuilderRecorderTextFile::ID(TypeID id)
   {
   _file << "ID" << id << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Statement(const char *s)
   {
   _file << "S" << lookupID(s) << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Type(const TR::IlType *type)
   {
   _file << "T" << lookupID(type) << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Value(const TR::IlValue *v)
   {
   _file << "V" << lookupID(v) << " ";
   }

void
OMR::JitBuilderRecorderTextFile::Builder(const TR::IlBuilder *b)
   {
   if (b)
      _file << "B" << lookupID(b) << " ";
   else
      _file << "Def ";
   }

void
OMR::JitBuilderRecorderTextFile::Location(const void *location)
   {
   _file << "{" << location << "} ";
   }

void
OMR::JitBuilderRecorderTextFile::EndStatement()
   {
   _file << "\n";
   }
