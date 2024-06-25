/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_GCREGISTERMAP_INCL
#define OMR_GCREGISTERMAP_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_GCREGISTERMAP_CONNECTOR
#define OMR_GCREGISTERMAP_CONNECTOR
namespace OMR { class GCRegisterMap; }
namespace OMR { typedef OMR::GCRegisterMap GCRegisterMapConnector; }
#endif

#include <stdint.h>
#include "env/TRMemory.hpp"

namespace TR { class GCRegisterMap; }

namespace OMR
{

class GCRegisterMap
   {
   public:

   TR_ALLOC(TR_Memory::GCRegisterMap)

   GCRegisterMap() : _map(0) {}

   TR::GCRegisterMap * self();

   uint32_t getMap() {return _map;}
   void setRegisterBits(uint32_t bits) {_map |= bits;}
   void resetRegisterBits(uint32_t bits) { _map &= ~bits; }
   void empty() {_map = 0;}
   void maskRegisters(uint32_t mask) {_map &= mask;}
   void maskRegistersWithInfoBits(uint32_t mask, uint32_t info) {_map = (mask & (_map | info));}
   void setInfoBits(uint32_t info) {_map |= info;}

   private:

   uint32_t _map;
   };

}

#endif
