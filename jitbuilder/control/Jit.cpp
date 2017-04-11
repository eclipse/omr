/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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
   codeCacheConfig._emitElfObject = TR::Options::getCmdLineOptions()->getOption(TR_PerfTool);

   TR::CodeCache *firstCodeCache = codeCacheManager.initialize(true, 1);
   }

// helperIDs is an array of helper id corresponding to the addresses passed in "helpers"
// helpers is an array of pointers to helpers that compiled code needs to reference
//   currently this argument isn't needed by anything so this function can stay internal
// options is any JIT option string passed in to globally influence compilation
static
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




extern "C"
bool
initializeJitWithOptions(char *options)
   {
   return initializeJitBuilder(0, 0, 0, options);
   }

extern "C"
bool
initializeJit()
   {
   return initializeJitBuilder(0, 0, 0, (char *)"-Xjit:acceptHugeMethods");
   }

extern "C"
int32_t
compileMethodBuilder(TR::MethodBuilder *m, uint8_t **entry)
   {
   TR::ResolvedMethod resolvedMethod(m);
   TR::IlGeneratorMethodDetails details(&resolvedMethod);

   int32_t rc=0;
   *entry = compileMethodFromDetails(NULL, details, warm, rc);
   m->typeDictionary()->NotifyCompilationDone();
   return rc;
   }

extern "C"
void
shutdownJit()
   {
   auto fe = JitBuilder::FrontEnd::instance();

   TR::CodeCacheManager &codeCacheManager = fe->codeCacheManager();
   codeCacheManager.destroy();
   }
