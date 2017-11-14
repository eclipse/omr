/*******************************************************************************
*
* (c) Copyright IBM Corp. 2017, 2017
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
******************************************************************************/

#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"          // for TR_ResolvedMethod
#include "control/CompilationStateManager.hpp"
#include "control/CompileMethod.hpp" 
#include "control/OptimizationPlan.hpp"        // for TR_OptimizationPlan, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"         // for TR::Options, etc
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"                    // for TR_Memory, etc
#include "env/jittypes.h"                      // for uintptrj_t
#include "ilgen/IlGenRequest.hpp"              // for CompileIlGenRequest
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "infra/Assert.hpp"                    // for TR_ASSERT

CompilationStateManager::CompilationStateManager(OMR_VMThread *omrVMThread,
                                                 TR::IlGeneratorMethodDetails & details,
                                                 TR_Hotness hotness) :
   state(INITIALIZING),
   plan(TR_OptimizationPlan::alloc(hotness,false,false)),
   filterInfo(NULL),
   startPC(NULL),
   return_code(COMPILATION_REQUESTED),
   fe(OMR::FrontEnd::singleton()),
   rawAllocator(),
   scratchSegmentProvider(1 << 16, rawAllocator),
   dispatchRegion(scratchSegmentProvider, rawAllocator),
   trMemory(*fe.persistentMemory(), dispatchRegion),
   compilee(*((TR_ResolvedMethod *)details.getMethod())),
   options(NULL),
   request(details)
   {
   if (!methodCanBeCompiled(&fe, compilee, filterInfo, &trMemory))
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompileExclude))
         {
         TR_VerboseLog::write("<JIT: %s cannot be translated>\n",
                              compilee.signature(&trMemory));
         }
      changeState(COMPLETED); 
      return; 
      }

   if (!plan)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompileExclude))
         {
         TR_VerboseLog::write("<JIT: %s out-of-memory allocating optimization plan>\n",
                              compilee.signature(&trMemory));
         }
      changeState(COMPLETED); 
      return; 
      }
   
   int32_t optionSetIndex = filterInfo ? filterInfo->getOptionSet() : 0;
   int32_t lineNumber = filterInfo ? filterInfo->getLineNumber() : 0;
   options= new (trMemory.trHeapMemory()) TR::Options(
                                                      &trMemory,
                                                      optionSetIndex,
                                                      lineNumber,
                                                      &compilee,
                                                      0,
                                                      plan,
                                                      false);
    
   TR_ASSERT(TR::comp() == NULL, "there seems to be a current TLS TR::Compilation object %p for this thread. At this point there should be no current TR::Compilation object", TR::comp());
   compiler = new (trMemory.trHeapMemory()) TR::Compilation(0, omrVMThread, &fe, &compilee, request, *options, dispatchRegion, &trMemory, plan);
   TR_ASSERT(TR::comp() == compiler, "the TLS TR::Compilation object %p for this thread does not match the one %p just created.", TR::comp(), compiler);

   changeState(INITIALIZED);
   }

/**
 * Transition between states. Will assert on an invalid state transition.
 */
void
CompilationStateManager::changeState(State s) 
   {
   const bool ALLOWED_STATE_TRANSITIONS[LAST_COMPILATION_STATE][LAST_COMPILATION_STATE] = {
      // INITIALIZING,  INITIALIZED,    COMPILING,      COMPLETED,
         0,             1,              0,              1, // INITIALIZING
         0,             0,              1,              1, // INITIALIZED
         0,             0,              0,              1, // COMPILING
         0,             0,              0,              0, // COMPLETED
   }; 

   TR_ASSERT_FATAL(ALLOWED_STATE_TRANSITIONS[state][s],
                   "Can't transition from state %d to %d", (int)state, (int)s);

   state = s; 
   }


/**
 * Compile the method
 */
void CompilationStateManager::compile() { 
   changeState(COMPILING); 

   uint64_t translationTime = TR::Compiler->vm.getUSecClock();
   startPC = tryAndCompileCompilation(*compiler, compilee, options, trMemory, scratchSegmentProvider, translationTime, return_code); 

   changeState(COMPLETED); 
}

/**
 * Destroy a compilation state manager
 */
CompilationStateManager::~CompilationStateManager() 
   {
   if (NULL != plan)
      {
      TR_OptimizationPlan::freeOptimizationPlan(plan);
      }

   if (NULL != compiler) 
      {
      //delete compiler; // Cannot delete yet; need a better story for destruction here.
      compiler->~Compilation();
      compiler = NULL; 
      }
   }
