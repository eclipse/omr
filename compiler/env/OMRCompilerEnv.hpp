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

#ifndef OMR_COMPILER_ENV_INCL
#define OMR_COMPILER_ENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_COMPILER_ENV_CONNECTOR
#define OMR_COMPILER_ENV_CONNECTOR
namespace OMR { class CompilerEnv; }
namespace OMR { typedef OMR::CompilerEnv CompilerEnvConnector; }
#endif


#include "infra/Annotations.hpp"
#include "env/RawAllocator.hpp"
#include "env/Environment.hpp"
#include "env/DebugEnv.hpp"
#include "env/PersistentAllocator.hpp"
#include "env/ClassEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/ArithEnv.hpp"
#include "env/VMEnv.hpp"
#include "env/VMMethodEnv.hpp"

namespace TR { class CompilerEnv; }


namespace OMR
{

class OMR_EXTENSIBLE CompilerEnv
   {

public:

   CompilerEnv(TR::RawAllocator raw, const TR::PersistentAllocatorKit &persistentAllocatorKit);

   TR::CompilerEnv *self();

   /// Primordial raw allocator.  This is guaranteed to be thread safe.
   ///
   TR::RawAllocator rawAllocator;

   // Compilation host environment
   //
   TR::Environment host;

   // Compilation target environment
   //
   TR::Environment target;

   // Class information in this compilation environment.
   //
   TR::ClassEnv cls;

   // Information about the VM environment.
   //
   TR::VMEnv vm;

   // Information about methods in this compilation environment
   //
   TR::VMMethodEnv mtd;

   // Object model in this compilation environment
   //
   TR::ObjectModel om;

   // Arithmetic semantics in this compilation environment
   //
   TR::ArithEnv arith;

   // Debug environment for the compiler.  This is not thread safe.
   //
   TR::DebugEnv debug;

   bool isInitialized() { return _initialized; }

   // --------------------------------------------------------------------------

   // Initialize a CompilerEnv.  This should only be executed after a CompilerEnv
   // object has been allocated and its constructor run.  Its intent is to
   // execute initialization logic that may require a completely initialized
   // object beforehand.
   //
   void initialize();

   TR::PersistentAllocator &persistentAllocator() { return _persistentAllocator; }

   OMRPortLibrary * const omrPortLib;

protected:
   // Initialize 'target' environment for this compiler
   //
   void initializeTargetEnvironment();

   // Initialize 'host' environment for this compiler
   //
   void initializeHostEnvironment();

private:

   bool _initialized;

   TR::PersistentAllocator _persistentAllocator;

public:

   /// Compiler-lifetime region allocator.
   /// NOTE: its a raw allocator until the region allocation work is completed.
   ///
   TR::PersistentAllocator &regionAllocator;

   };

}

#endif
