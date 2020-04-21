/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef FEBASE_HPP_JniLXw
#define FEBASE_HPP_JniLXw

#include "compiler/env/FrontEnd.hpp"

#include "compile/CompilationTypes.hpp"
#include "env/JitConfig.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "env/CompilerEnv.hpp"

class TR_ResolvedMethod;

namespace TR
{

// ::TR_FrontEnd: defines the API that can be used across the codebase for both TR and OMR
// FECommon: is a repository of code and data that is common to all OMR frontends but not all TR frontends
// FEBase<D>: is a repository of code and data that is specific to a single frontend, but described as an abstract templat
//            based on the type D
// D: concrete frontend. Stuff that is not common with other frontends.

class FECommon : public ::TR_FrontEnd
   {
   protected:
   FECommon();


   public:
   virtual TR_PersistentMemory       * persistentMemory() = 0;

   virtual TR_Debug *createDebug(TR::Compilation *comp = NULL);

   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT=false) { return NULL; }
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT=false) { return NULL; }
   virtual const char *       sampleSignature(TR_OpaqueMethodBlock * aMethod, char *buf, int32_t bufLen, TR_Memory *memory) { return NULL; }

   // need this so z codegen can create a sym ref to compare to another sym ref it cannot possibly be equal to
   virtual uintptr_t getOffsetOfIndexableSizeField() { return -1; }
   };

template <class T> struct FETraits {};

template <class Derived>
class FEBase : public FECommon
   {
   public:
   // Define our types in terms of the Traits
   typedef typename TR::FETraits<Derived>::JitConfig        JitConfig;

   private:
   JitConfig            _config;
   TR::CodeCacheManager _codeCacheManager;

   // these two are deprecated in favour of TR::GlobalAllocator and TR::Allocator
   TR_PersistentMemory       _persistentMemory; // global memory

   public:
   FEBase()
      : FECommon(),
      _config(),
      _codeCacheManager(TR::Compiler->rawAllocator),
      _persistentMemory(jitConfig(), TR::Compiler->persistentAllocator())
      {
      ::trPersistentMemory = &_persistentMemory;
      }

   static Derived &singleton() { return *(Derived::instance()); }

   JitConfig *jitConfig() { return &_config; }
   TR::CodeCacheManager &codeCacheManager() { return _codeCacheManager; }

   virtual uint8_t * allocateRelocationData(TR::Compilation* comp, uint32_t size);

   virtual TR_PersistentMemory       * persistentMemory() { return &_persistentMemory; }
   virtual TR::PersistentInfo * getPersistentInfo() { return _persistentMemory.getPersistentInfo(); }

   };

} /* namespace TR */

#endif /* FEBASE_HPP_JniLXw */
