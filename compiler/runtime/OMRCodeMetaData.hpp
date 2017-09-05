/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
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

#include <stdint.h>               // for uintptr_t
#include "env/jittypes.h"         // for uintptrj_t
#include "infra/Annotations.hpp"  // for OMR_EXTENSIBLE

namespace TR { class CodeMetaData; }

/**
 * CodeMetaData contains meta data information about a compilation unit 
 * such as a method, a trace, etc.
 *
 * It provides a static method to create an instance of this class,
 * as well as all its field getters.
 */

namespace OMR
{

class OMR_EXTENSIBLE CodeMetaData
   {
   public:

   TR::CodeMetaData *self();

   /**
    * @brief Constructor method to create meta data for a compilation unit.
    */
   CodeMetaData(TR::Compilation *comp);

   /**
    * @brief Returns address of allocated code memory within a code
    * cache for a compilation unit.
    */
   uintptrj_t getCodeAllocStart() { return _codeAllocStart; }

   /**
    * @brief Returns size of allocated code memory within a code cache 
    * for a compilation unit.
    *
    * This requires compiled code segments, including snippets to be 
    * laid out contiguously.
    */
   uint32_t getCodeAllocSize() { return _codeAllocSize; }

   /**
    * @brief Returns start PC address of compiled code for a
    * compilation unit when invoked from interpreter.
    *
    * Interpreter entry PC may preceed compiled entry PC and may point 
    * to code necessary for compiled code to execute properly, i.e.
    * it may need to load parameters into designated registers.
    *
    * For OMR, interpreter entry PC and compiled entry PC point to the
    * same place in the code.
    */
   uintptrj_t getInterpreterEntryPC() { return _interpreterEntryPC; }

   /**
    * @brief Returns start PC address of compiled code for a
    * compilation unit when invoked from compiled code.
    * 
    * For OMR, interpreter entry PC and compiled entry PC point to the
    * same place in the code.
    */
   uintptrj_t getCompiledEntryPC() { return _compiledEntryPC; }

   /**
    * @brief Returns end PC address of compiled code for a
    * compilation unit.
    */
   uintptrj_t getCompiledEndPC() { return _compiledEndPC; }

   /**
    * @brief Returns compilation hotness level of compiled code.
    */
   uintptrj_t getCodeHotness() { return _hotness; }

   protected:

   uintptrj_t _codeAllocStart;
   uint32_t _codeAllocSize;

   uintptrj_t _interpreterEntryPC;
   uintptrj_t _compiledEntryPC;
   uintptrj_t _compiledEndPC;

   TR_Hotness _hotness;
   };

}

#endif
