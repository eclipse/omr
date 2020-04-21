/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef OMR_Z_SNIPPET_PPA2SNIPPET_INCL
#define OMR_Z_SNIPPET_PPA2SNIPPET_INCL

#include <stdint.h>

#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"

namespace TR { class S390zOSSystemLinkage; }

namespace TR {

/** \brief
 *
 *  Represents the XPLINK PPA2 (Program Prologue Area) snippet which is mandatory in method bodies for all languages
 *  participating in the XPLINK calling convention.
 *
 *  \details
 *
 *  The PPA2 snippet is composed of a fixed area which is defined as follows:
 *
 *  \verbatim
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Member identifer                 | Member subid                     | Member defined                   | Control level                    |
 *  0x04 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Signed offset from PPA2 to CELQSTRT for load module                                                                                       |
 *  0x08 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Signed offset from PPA2 to PPA4 (zero if not present)                                                                                     |
 *  0x0C +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Signed offset from PPA2 to timestamp/version information (zero if not present)                                                            |
 *  0x10 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Signed offset from PPA2 to compilation unit's primary Entry Point (at the lowest address)                                                 |
 *  0x14 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Compilation flags                                                   | Reserved                                                            |
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *  \endverbatim
 */
class PPA2Snippet : public TR::Snippet
   {
   public:

   /** \brief
    *     Size (in bytes) of the fixed area of the PPA1 data structure which is always present.
    */
   static const size_t FIXED_AREA_SIZE = 24;

   public:

   PPA2Snippet(TR::CodeGenerator* cg, TR::S390zOSSystemLinkage* linkage);

   virtual uint8_t* emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   private:

   TR::S390zOSSystemLinkage* _linkage;
   };
}

#endif
