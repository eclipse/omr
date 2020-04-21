/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#ifndef CALL_INCL
#define CALL_INCL

#include "JitBuilder.hpp"

typedef int32_t (CallFunctionType1Arg)(int32_t);
typedef int32_t (CallFunctionType2Arg)(int32_t, int32_t);

class JitToNativeCallMethod : public OMR::JitBuilder::MethodBuilder
   {
   public:
   JitToNativeCallMethod(OMR::JitBuilder::TypeDictionary *types);
   virtual bool buildIL();
   };

class JitToNativeComputedCallMethod : public OMR::JitBuilder::MethodBuilder
   {
   public:
   JitToNativeComputedCallMethod(OMR::JitBuilder::TypeDictionary *types);
   virtual bool buildIL();
   };

class NativeToJitCallMethod : public OMR::JitBuilder::MethodBuilder
   {
   public:
   NativeToJitCallMethod(OMR::JitBuilder::TypeDictionary *types);
   virtual bool buildIL();
   };

class JitToJitCallMethod : public OMR::JitBuilder::MethodBuilder
   {
   public:
   JitToJitCallMethod(OMR::JitBuilder::TypeDictionary *types, OMR::JitBuilder::MethodBuilder *jitMethodBuilder);
   virtual bool buildIL();

   OMR::JitBuilder::MethodBuilder *jitMethodBuilder;
   };

#endif // !defined(CALL_INCL)
