/*******************************************************************************
 * Copyright (c) 2000, 2023 IBM Corp. and others
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

#ifndef COMPILATIONCONTROLLER_INCL
#define COMPILATIONCONTROLLER_INCL

#include "control/OptimizationPlan.hpp"
#include "env/TRMemory.hpp"

class TR_MethodEvent;
namespace TR { class Recompilation; }

//------------------------------- TR::CompilationController ------------------------
// All methods and fields are static. The most important field is _compilationStrategy
// that store the compilation strategy in use.
//---------------------------------------------------------------------------------
namespace TR
{
class CompilationController
   {
   public:
   enum {LEVEL1=1, LEVEL2, LEVEL3}; // verbosity levels;
   enum {DEFAULT, THRESHOLD}; // strategy codes
   static bool    init(TR::CompilationInfo *);
   static void    shutdown();
   static bool    useController() { return _useController; }
   static int32_t verbose() { return _verbose; }
   static void    setVerbose(int32_t v) { _verbose = v; }
   static TR::CompilationStrategy * getCompilationStrategy() { return _compilationStrategy; }
   static TR::CompilationInfo     * getCompilationInfo() { return _compInfo; }
   private:
   static TR::CompilationStrategy *_compilationStrategy;
   static TR::CompilationInfo     *_compInfo;        // stored here for convenience
   static int32_t                 _verbose;
   static bool                    _useController;
   static bool                    _tlsCompObjCreated;
   };

} // namespace TR

namespace TR
{

class DefaultCompilationStrategy : public TR::CompilationStrategy
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   DefaultCompilationStrategy() {}
   TR_OptimizationPlan *processEvent(TR_MethodEvent *event, bool *newPlanCreated);
   void shutdown() {}
   virtual bool enableSwitchToProfiling() { return true; }
   };

} // namesapce TR
#endif
