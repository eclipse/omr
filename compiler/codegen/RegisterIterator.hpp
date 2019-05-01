/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef TR_REGISTER_ITERATOR_INCL
#define TR_REGISTER_ITERATOR_INCL

#include "env/TRMemory.hpp"

namespace TR { class Machine; }
namespace TR { class Register; }

namespace TR
{

/**
 * \brief
 * An implementation of the iterator pattern to allow users (for example RAS)
 * to traverse the platform-specific list of a certain kind of registers (GPR,
 * FPR, VRF, etc.)
 */
class RegisterIterator
   {
   public:
   TR_ALLOC(TR_Memory::RegisterIterator)

   /**
    * @brief
    * Constructor of the register iterator. The iterator traverses registers
    * from \p firstRegIndex till \p lastRegIndex.
    *
    * @param[in] machine : an instance of the target machine representation
    * @param[in] firstRegIndex : the index of the first available register
    * @param[in] lastRegIndex : the index of the last available register
    */
   RegisterIterator(TR::Machine *machine, int32_t firstRegIndex, int32_t lastRegIndex):
      _machine(machine),
      _firstRegIndex(firstRegIndex),
      _lastRegIndex(lastRegIndex),
      _cursor(firstRegIndex)
      {};

   /**
    * @return the pointer to the first available register
    */
   TR::Register *getFirst();

   /**
    * @return the pointer to the current (from the iterator's point of view) register
    */
   TR::Register *getCurrent();

   /**
    * @return the pointer to the next (from the iterator's point of view) register
    */
   TR::Register *getNext();

   private:
   TR::Machine *_machine;
   int32_t _firstRegIndex;
   int32_t _lastRegIndex;
   int32_t _cursor;
   };

}

#endif
