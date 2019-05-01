/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp. and others
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

#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "control/CompileMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/FrontEnd.hpp"
#include "env/IO.hpp"
#include "env/RawAllocator.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/JBJitConfig.hpp"

#if defined(TR_TARGET_S390)
#include "z/codegen/TRSystemLinkage.hpp"
#endif

extern TR_RuntimeHelperTable runtimeHelpers;
extern void setupCodeCacheParameters(int32_t *, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t *numHelpers, int32_t *CCPreLoadedCodeSize);

static void
initHelper(void *helper, TR_RuntimeHelper id)
   {
   #if defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)
      //Implies Big-Endian POWER.
      //Helper Address is stored in a function descriptor consisting of [address, TOC, envp]
      //Load out Helper address from this function descriptor.

      //Little-Endian POWER can directly load the helper address, no function descriptor used.
      helper = *(void **)helper;
   #endif
   runtimeHelpers.setAddress(id, helper);
   }

static void
initializeAllHelpers(JitBuilder::JitConfig *jitConfig, TR_RuntimeHelper *helperIDs, void **helperAddresses, int32_t numHelpers)
   {
   initializeJitRuntimeHelperTable(false);

   if (numHelpers > 0)
      {
      for (int32_t h=0;h < numHelpers;h++)
         initHelper(helperAddresses[h], helperIDs[h]);

      #if defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__)
         jitConfig->setInterpreterTOC(((size_t *)helperAddresses[0])[1]);
      #endif
      }
   }

static void
initializeCodeCache(TR::CodeCacheManager & codeCacheManager)
   {
   TR::CodeCacheConfig &codeCacheConfig = codeCacheManager.codeCacheConfig();
   codeCacheConfig._codeCacheKB = 128;

   // setupCodeCacheParameters must stay before JitBuilder::CodeCacheManager::initialize() because it needs trampolineCodeSize
   setupCodeCacheParameters(&codeCacheConfig._trampolineCodeSize,
                            &codeCacheConfig._mccCallbacks,
                            &codeCacheConfig._numOfRuntimeHelpers,
                            &codeCacheConfig._CCPreLoadedCodeSize);

   codeCacheConfig._needsMethodTrampolines = false;
   codeCacheConfig._trampolineSpacePercentage = 5;
   codeCacheConfig._allowedToGrowCache = true;
   codeCacheConfig._lowCodeCacheThreshold = 0;
   codeCacheConfig._verboseCodeCache = false;
   codeCacheConfig._verbosePerformance = false;
   codeCacheConfig._verboseReclamation = false;
   codeCacheConfig._doSanityChecks = false;
   codeCacheConfig._codeCacheTotalKB = 16*1024;
   codeCacheConfig._codeCacheKB = 128;
   codeCacheConfig._codeCachePadKB = 0;
   codeCacheConfig._codeCacheAlignment = 32;
   codeCacheConfig._codeCacheFreeBlockRecylingEnabled = true;
   codeCacheConfig._largeCodePageSize = 0;
   codeCacheConfig._largeCodePageFlags = 0;
   codeCacheConfig._maxNumberOfCodeCaches = 96;
   codeCacheConfig._canChangeNumCodeCaches = true;
   codeCacheConfig._emitExecutableELF = TR::Options::getCmdLineOptions()->getOption(TR_PerfTool) 
                                    ||  TR::Options::getCmdLineOptions()->getOption(TR_EmitExecutableELFFile);
   codeCacheConfig._emitRelocatableELF = TR::Options::getCmdLineOptions()->getOption(TR_EmitRelocatableELFFile);

   TR::CodeCache *firstCodeCache = codeCacheManager.initialize(true, 1);
   }

// helperIDs is an array of helper id corresponding to the addresses passed in "helpers"
// helpers is an array of pointers to helpers that compiled code needs to reference
//   currently this argument isn't needed by anything so this function can stay internal
// options is any JIT option string passed in to globally influence compilation
bool
initializeJitBuilder(TR_RuntimeHelper *helperIDs, void **helperAddresses, int32_t numHelpers, char *options)
   {

   // Create a bootstrap raw allocator.
   //
   TR::RawAllocator rawAllocator;

   try
      {
      // Allocate the host environment structure
      //
      TR::Compiler = new (rawAllocator) TR::CompilerEnv(rawAllocator, TR::PersistentAllocatorKit(rawAllocator));
      }
   catch (const std::bad_alloc& ba)
      {
      return false;
      }

   TR::Compiler->initialize();

   // --------------------------------------------------------------------------
   static JitBuilder::FrontEnd fe;
   auto jitConfig = fe.jitConfig();

   initializeAllHelpers(jitConfig, helperIDs, helperAddresses, numHelpers);

   if (commonJitInit(fe, options) < 0)
      return false;

   initializeCodeCache(fe.codeCacheManager());

   return true;
   }

/*
 _____      _                        _
| ____|_  _| |_ ___ _ __ _ __   __ _| |
|  _| \ \/ / __/ _ \ '__| '_ \ / _` | |
| |___ >  <| ||  __/ |  | | | | (_| | |
|_____/_/\_\\__\___|_|  |_| |_|\__,_|_|

 ___       _             __
|_ _|_ __ | |_ ___ _ __ / _| __ _  ___ ___
 | || '_ \| __/ _ \ '__| |_ / _` |/ __/ _ \
 | || | | | ||  __/ |  |  _| (_| | (_|  __/
|___|_| |_|\__\___|_|  |_|  \__,_|\___\___|

*/

// An individual program should link statically against JitBuilder, then call:
//     initializeJit() or initializeJitWithOptions() to initialize the Jit
//     compileMethodBuilder() as many times as needed to create compiled code
//     shuwdownJit() when the test is complete
//




bool
internal_initializeJitWithOptions(char *options)
   {
   return initializeJitBuilder(0, 0, 0, options);
   }

bool
internal_initializeJit()
   {
   return initializeJitBuilder(0, 0, 0, (char *)"-Xjit:acceptHugeMethods,enableBasicBlockHoisting,omitFramePointer,useILValidator");
   }

int32_t
internal_compileMethodBuilder(TR::MethodBuilder *m, void **entry)
   {
   return m->Compile(entry);
   }

void
internal_shutdownJit()
   {
   auto fe = JitBuilder::FrontEnd::instance();

   TR::CodeCacheManager &codeCacheManager = fe->codeCacheManager();
   codeCacheManager.destroy();
   }
