/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdint.h>
#include "infra/Assert.hpp"

#define IS_32BIT_SIGNED(x)   ((x) == ( int32_t)(x))

extern "C" void _patchingFence16(void *startAddr);

extern "C" void _patchVirtualGuard(uint8_t *locationAddr, uint8_t *destinationAddr, int32_t smpFlag)
   {
   intptr_t destinationDistance = destinationAddr - locationAddr;
   TR_ASSERT(IS_32BIT_SIGNED(destinationDistance), "Destination address must be in range of 5-byte jmp instruction");

   if (-126 <= destinationDistance && destinationDistance <= 129)
      {
      // Two-byte jmp instruction
      //
      intptr_t displacement = destinationDistance-2;
      *(uint16_t*)locationAddr = 0xeb + (displacement << 8);
      }
   else
      {
      // Five-byte jmp instruction
      //
      intptr_t displacement = destinationDistance-5;

      // Self-loop
      *(uint16_t*)locationAddr = 0xfeeb;

      _patchingFence16(locationAddr);

      // Bytes 2-4
      locationAddr[2] = (displacement >> 8);
      locationAddr[3] = (displacement >> 16);
      locationAddr[4] = (displacement >> 24);

      // Bytes 0-1; unlock the self-loop/JMP+3
      _patchingFence16(locationAddr);
      *(uint16_t*)locationAddr = 0xe9 + ((displacement & 0xff) << 8);
      }
   }
