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

#ifndef OMR_JITBUILDERRECORDER_BINARYBUFFER_INCL
#define OMR_JITBUILDERRECORDER_BINARYBUFFER_INCL

#include "ilgen/JitBuilderRecorder.hpp"
#include <vector>

namespace TR { class IlBuilder; }
namespace TR { class MethodBuilder; }
namespace TR { class IlType; }
namespace TR { class IlValue; }

namespace OMR
{

class JitBuilderRecorderBinaryBuffer : public TR::JitBuilderRecorder
   {
   public:
   JitBuilderRecorderBinaryBuffer(const TR::MethodBuilder *mb, const char *fileName);
   virtual ~JitBuilderRecorderBinaryBuffer() { }

   virtual void Close();
   virtual void String(const char * const string);
   virtual void Number(int8_t num);
   virtual void Number(int16_t num);
   virtual void Number(int32_t num);
   virtual void Number(int64_t num);
   virtual void Number(float num);
   virtual void Number(double num);
   virtual void ID(TypeID id);
   virtual void Statement(const char *s);
   virtual void Type(const TR::IlType *type);
   virtual void Value(const TR::IlValue *v);
   virtual void Builder(const TR::IlBuilder *b);
   virtual void Location(const void * location);
   virtual void EndStatement();

   std::vector<uint8_t> & buffer() { return _buf; }

   protected:
   std::vector<uint8_t> _buf;
   };

} // namespace OMR

#endif // !defined(OMR_JITBUILDERRECORDER_BINARYBUFFER_INCL)
