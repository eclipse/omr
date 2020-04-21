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

#ifndef OMR_Z_SNIPPET_PPA1SNIPPET_INCL
#define OMR_Z_SNIPPET_PPA1SNIPPET_INCL

#include <stdint.h>

#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"

namespace TR { class S390zOSSystemLinkage; }

namespace TR {

/** \brief
 *
 *  Represents the XPLINK PPA1 (Program Prologue Area) snippet which is mandatory in method bodies for all languages
 *  participating in the XPLINK calling convention.
 *
 *  \details
 *
 *  The PPA1 snippet is composed of a fixed area which is always present and a sequence of optional areas which are
 *  present if and only if the corresponding bits in the Flags 3 and Flags 4 fields are active.
 *
 *  - PPA1 fixed area:
 *
 *  \verbatim
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Version                          | Signature                        | Saved GPR Mask                                                      |
 *  0x04 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Signed offset to PPA2 from start of PPA1                                                                                                  |
 *  0x08 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Flags 1                          | Flags 2                          | Flags 3                          | Flags 4                          |
 *       |                                  |                                  |                                  |                                  |
 *       | 0  DSA format                    | 0  External function             | 0  State variable locator        | 0  Reserved                      |
 *       | 1  Reserved                      | 1  Debug info                    | 1  Argument area length          | 1  Reserved                      |
 *       | 2  Exception model               | 2  Debug info                    | 2  FPR mask                      | 2  Reserved                      |
 *       | 3  BDI type flag                 | 3  Reserved                      | 3  AR mask                       | 3  Reserved                      |
 *       | 4  Invoke for DSA exit           | 4  Reserved                      | 4  Member PPA1 word              | 4  Reserved                      |
 *       | 5  Reserved                      | 5  Reserved                      | 5  Block debug info              | 5  Reserved                      |
 *       | 6  Reserved                      | 6  Reserved                      | 6  Interface mapping flags       | 6  Reserved                      |
 *       | 7  Vararg function               | 7  Reserved                      | 7  Java method locator table     | 7  Name length and name          |
 *  0x0C +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Length/4 of parameters                                              | Length/2 of prologue             | Alloca register                  |
 *       |                                                                     |                                  | Offset/2 to stack pointer update |
 *  0x10 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Length of code section                                                                                                                    |
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *  \endverbatim
 *
 *  - PPA1 optional area:
 *
 *  \verbatim
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | State variable locator                                                                                                                    | PPA1 Flags 3 bit 0
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Argument area length                                                                                                                      | PPA1 Flags 3 bit 1
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | FPR mask                                                            | AR mask                                                             | PPA1 Flags 3 bit 2 or 3
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | FPR save area locator                                                                                                                     | PPA1 Flags 3 bit 2
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | AR save area locator                                                                                                                      | PPA1 Flags 3 bit 3
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | PPA1 member word                                                                                                                          | PPA1 Flags 3 bit 4
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Block debug info address                                                                                                                  | PPA1 Flags 3 bit 5
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Interference mapping flags (31-bit mode only)                                                                                             | PPA1 Flags 3 bit 6
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Java method locator table (MLT)                                                                                                           | PPA1 Flags 3 bit 7
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *
 *                                         0x01                               0x02                               0x03
 *  0x00 +----------------------------------+----------------------------------+                                                                      
 *       | Length of name                                                      |                                                                       PPA1 Flags 4 bit 7
 *  0x02 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *       | Name of function (variable sized aligned to 4 bytes)                                                                                      |
 *       +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
 *  \endverbatim
 */
class PPA1Snippet : public TR::Snippet
   {
   public:

   /** \brief
    *     Size (in bytes) of the fixed area of the PPA1 data structure which is always present.
    */
   static const size_t FIXED_AREA_SIZE = 20;

   public:

   PPA1Snippet(TR::CodeGenerator* cg, TR::S390zOSSystemLinkage* linkage);

   virtual uint8_t* emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   private:

   // TODO: We need to generalize the relocations in Relocation.hpp to accept a divisor and remove this relocation
   class PPA1OffsetToPPA2Relocation : public TR::LabelRelocation
      {
      public:

      PPA1OffsetToPPA2Relocation(uint8_t* cursor, TR::LabelSymbol* ppa2);

      virtual void apply(TR::CodeGenerator* cg);
      };

   TR::S390zOSSystemLinkage* _linkage;
   };
}

#endif
