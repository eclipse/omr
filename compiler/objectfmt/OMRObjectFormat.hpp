/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef OMR_OBJECTFORMAT_INCL
#define OMR_OBJECTFORMAT_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_OBJECTFORMAT_CONNECTOR
#define OMR_OBJECTFORMAT_CONNECTOR
namespace OMR { class ObjectFormat; }
namespace OMR { typedef OMR::ObjectFormat ObjectFormatConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "env/FilePointerDecl.hpp"
#include "env/TRMemory.hpp"
#include "infra/Annotations.hpp"

namespace TR { class FunctionCallData; }
namespace TR { class Instruction; }

namespace OMR
{

/**
 * @class ObjectFormat
 *
 * @details
 *    The \c ObjectFormat class is an extensible class that encapsulates the
 *    encoding of a function call.  The concrete class \c TR::ObjectFormat
 *    defines an abstract API for emitting and encoding function calls.  This
 *    concrete class is intended to serve as a base for other object formats
 *    that provide implementations of the encoding functions.  For example:
 *
 *    * \c JitCodeRWXObjectFormat for calls in a Read+Writable+eXecutable code
 *      cache in a dynamic language
 *    * \c JitCodeRXObjectFormat for calls in a Read+eXecutable code cache in a
 *      dynamic language
 *    * \c ELFObjectFormat for static compiles using ELF
 *    * \c MachOObjectFormat for static compiles using Mach-O
 *
 *    A language environment is expected to install whatever object format
 *    encoding that makes sense for that environment.
 *
 *    The \c TR::ObjectFormat API is expected to be used at all places in
 *    the compiled code that call another function.
 *
 *    To provide a consistent API for all platforms, the data required to
 *    emit each call site is encapsulated in an extensible
 *    \c TR::FunctionCallData class that is passed into each function.
 *    Each platform and language environment is responsible for refining
 *    the contents and populating it accordingly at each call site with i
 *    whatever data values are necessary to encode the call.
 */
class OMR_EXTENSIBLE ObjectFormat
   {

public:

   TR_ALLOC(TR_Memory::ObjectFormat)

   /**
    * @brief Emit a sequence of \c TR::Instruction 's to call a function using this
    *        object format.
    *
    * @param[in,out] data : a populated \c TR::FunctionCallData structure with
    *            valid parameters for this call site.  Note that this function
    *            may alter the contents of this structure.
    *
    * @return : the final \c TR::Instruction produced in the instruction stream to emit a
    *           call to a function for this object format.
    */
   virtual TR::Instruction *emitFunctionCall(TR::FunctionCallData &data) = 0;

   /**
    * @brief Emit the binary code to call a function using this object format.
    *        This function is suitable for calling during binary encoding and does not
    *        use \c TR::Instruction 's.
    *
    * @param[in,out] data : a populated \c TR::FunctionCallData structure with valid
    *            parameters for this call site.  Note that this function may alter
    *            the contents of this structure.
    *
    * @return : the next buffer address following the necessary instruction(s) to emit
    *           a function call using this object format.
    */
   virtual uint8_t *encodeFunctionCall(TR::FunctionCallData &data) = 0;

   /**
    * @brief Return the length in bytes of the binary encoding sequence to generate a
    *        call to a function using this object format.  If the exact length
    *        cannot be determined, this should return a conservatively correct estimate
    *        that is at least as large as the actual length.
    *
    * @return : the length in bytes of the encoding of a function call
    */
   virtual int32_t estimateBinaryLength() = 0;

   /**
    * @brief Print an encoded function call to a file stream
    *
    * @param[in] file : the \c TR::FILE to print to
    * @param[in] data : a populated \c TR::FunctionCallData structure with valid parameters
    *          for an encoded function call.
    * @param[in] bufferPos : starting position in buffer to begin printing
    */
   virtual void printEncodedFunctionCall(TR::FILE *file, TR::FunctionCallData &data) = 0;

   };

}

#endif
