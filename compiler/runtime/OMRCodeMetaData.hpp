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

#ifndef OMR_CODE_METADATA_INCL
#define OMR_CODE_METADATA_INCL

/*
 * The following #define(s) and typedef(s) must appear before any #includes in this file
 */
#ifndef OMR_CODE_METADATA_CONNECTOR
#define OMR_CODE_METADATA_CONNECTOR
namespace OMR { struct CodeMetaData; }
namespace OMR { typedef OMR::CodeMetaData CodeMetaDataConnector; }
#endif

#include "env/jittypes.h"
#include "infra/Annotations.hpp"

namespace TR { class CodeMetaData; }
namespace TR { class Compilation; }

/**
 * CodeMetaData contains metadata information about a single method.
 */

namespace OMR
{

class OMR_EXTENSIBLE CodeMetaData
   {
   public:

   TR::CodeMetaData *self();

   /**
    * @brief Constructor method to create metadata for a method.
    */
   CodeMetaData(TR::Compilation *comp);

   /**
    * @brief Returns the address of allocated code memory within a code
    * cache for a method.
    */
   uintptr_t codeAllocStart() { return _codeAllocStart; }

   /**
    * @brief Returns the total size of code memory allocated for a method 
    * within a code cache.
    */
   uint32_t codeAllocSize() { return _codeAllocSize; }

   /**
    * @brief Returns the starting address of compiled code for a
    * method when invoked from an interpreter.
    *
    * Interpreter entry PC may preceed compiled entry PC and may point 
    * to code necessary for proper execution of this method if invoked 
    * from an interpreter. For example, it may need to marshall method 
    * arguments from an interpreter linkage to a compiled method linkage.
    *
    * By default, the interpreter entry PC and compiled entry PC point 
    * to the same address.
    */
   uintptr_t interpreterEntryPC() { return _interpreterEntryPC; }

   /**
    * @brief Returns the starting address of compiled code for a
    * method when invoked from compiled code.
    * 
    * By default, the interpreter entry PC and compiled entry PC point 
    * to the same address.
    */
   uintptr_t compiledEntryPC() { return _compiledEntryPC; }

   /**
    * @brief Returns the end address of compiled code for a method.
    */
   uintptr_t compiledEndPC() { return _compiledEndPC; }

   /**
    * @brief Returns the compilation hotness level of a compiled method.
    */
   TR_Hotness codeHotness() { return _hotness; }

   protected:

   uintptr_t _codeAllocStart;
   uint32_t _codeAllocSize;

   uintptr_t _interpreterEntryPC;
   uintptr_t _compiledEntryPC;
   uintptr_t _compiledEndPC;

   TR_Hotness _hotness;
   };

}

#endif
