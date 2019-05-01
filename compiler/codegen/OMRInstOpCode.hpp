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

#ifndef OMR_INSTOPCODE_INCL
#define OMR_INSTOPCODE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTOPCODE_CONNECTOR
#define OMR_INSTOPCODE_CONNECTOR
namespace OMR { class InstOpCode; }
namespace OMR { typedef OMR::InstOpCode InstOpCodeConnector; }
#endif

#include <stdint.h>
#include "env/TRMemory.hpp"

namespace OMR
{

class InstOpCode
   {
   public:

   TR_ALLOC(TR_Memory::Instruction)

   enum Mnemonic
      {
      #include "codegen/InstOpCodeEnum.hpp"
      NumOpCodes
      };

   protected:

   InstOpCode(Mnemonic m) :
      _mnemonic(m) {}

   public:

   Mnemonic getMnemonic() { return _mnemonic; }
   void setMnemonic(Mnemonic op) { _mnemonic = op; }

   static int32_t getNumOpCodes() { return NumOpCodes; }

   /*
    * Length of the encoded binary representation of an opcode.
    */
   static uint8_t length(Mnemonic m);

   /*
    * Copies the encoded binary representation of a given mnemonic to the
    * provided buffer.  This will copy at most length() bytes.
    *
    * Returns the cursor position after the bytes have been copied.
    */
   static uint8_t *copyBinaryToBuffer(Mnemonic m, uint8_t *cursor);
   static const char *getOpCodeName(Mnemonic m);
   static const char *getMnemonicName(Mnemonic m);

   const char *getOpCodeName()    { return getOpCodeName(_mnemonic);  }
   const char *getMnemonicName()  { return getMnemonicName(_mnemonic);  }

   /*
    * 0-terminated, printable description of the given mnemonic
    */
   //static char *description(Mnemonic m);

   protected:

   /*
    * Pointer to the encoded binary representation of an instruction.
    */
   static uint8_t *binaryEncoding(Mnemonic m);

   Mnemonic _mnemonic;

   private:

   InstOpCode() {}
   };

}

#endif
