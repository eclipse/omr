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


#ifndef OMR_COMPILATION_STATE_MANAGER 
#define OMR_COMPILATION_STATE_MANAGER


#include "compile/CompilationTypes.hpp"  // for TR_Hotness
#include "control/OptimizationPlan.hpp"  // for TR_OptimizationPlan, etc
#include "env/SystemSegmentProvider.hpp"
#include "env/ConcreteFE.hpp"            // for FrontEnd
#include "ilgen/IlGenRequest.hpp"        // for CompileIlGenRequest

// Not extensible yet, though could be made extensible if necessary.


namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class Options; } 
class TR_FilterBST;

/**
 * A stateful object responsible for managing compilation. The intention is that
 * this can be passed around to drive compilation at various points (and allow 
 * initialization and compilation to happen under different locking contexts!) 
 *
 * There are four states worth considering here 
 * - Initializing: Pre-compilation initialization.  
 * - Initialized: At this point the manaager is prepared to compile, and has 
 *                all the information required to compile a method. 
 * - Compiling:   When compilation is requested to begin
 * - Completed:   When the compilation is completed (or has been declined), 
 *                the state manager enters the completed state. 
 *
 * Huh. Maybe this is more of a compilation promise? 
 *
 * A note on threading: this object must be created and processed on the same 
 * thread, as it will set the thread-local compilation global during construction. 
 */
class CompilationStateManager { 
public:
   enum State { 
      INITIALIZING = 0,
      INITIALIZED, 
      COMPILING,
      COMPLETED,
      LAST_COMPILATION_STATE
   }; 

   CompilationStateManager(OMR_VMThread *,
                           TR::IlGeneratorMethodDetails&,
                           TR_Hotness); 
   ~CompilationStateManager(); 

   State getState() { return state; } 

   int32_t getReturnCode()
      { 
      TR_ASSERT_FATAL(state == COMPLETED, "Asking for a return code before it's available -- call compile()");
      return return_code; 
      }

   uint8_t* getStartPC()
      {
      TR_ASSERT_FATAL(state == COMPLETED, "Asking for a PC  before it's available -- call compile()");
      return startPC; 
      }

   void compile(); 

private:
   void changeState(State);

   State state; 
   TR_OptimizationPlan *plan;
   TR_FilterBST *filterInfo;
   uint8_t* startPC; 

   int32_t return_code; 
   OMR::FrontEnd &fe;
   TR::RawAllocator rawAllocator;
   TR::SystemSegmentProvider scratchSegmentProvider;
   TR::Region dispatchRegion;
   TR_Memory trMemory;
   TR_ResolvedMethod &compilee;

   TR::Options* options;
   TR::CompileIlGenRequest request;
   TR::Compilation* compiler; 
};
#endif
