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

#ifndef RANDOM_INCL
#define RANDOM_INCL

#include <stdint.h>
#include "env/TRMemory.hpp"
namespace TR { class Compilation; }

class TR_RandomGenerator
   {
   public:
   //Cannot new/delete; allocate as a value
   TR_ALLOC(TR_Memory::RandomGenerator)

   uint32_t getRandom();
   int32_t getRandom(int32_t min, int32_t max);
   void setSeed(uint32_t newSeed);

   TR_RandomGenerator(TR_RandomGenerator *parent)
      {
      setSeed(parent->getRandom());
      }

   TR_RandomGenerator(int32_t seedValue)
      {
      setSeed(seedValue);
      }

   TR_RandomGenerator(uint32_t seedValue)
      {
      setSeed(seedValue);
      }

   static void exercise(int32_t period, TR::Compilation *comp); // For testing

   private:
   uint64_t _bits; // Unsigned, so that right-shifts don't wipe the sign bit all over the place
   };

class TR_HasRandomGenerator
   {
   TR_RandomGenerator          _randomGenerator;

   public:
   bool                      randomBoolean(int32_t rarity=2)     { return _randomGenerator.getRandom(1, rarity) == rarity; } // Probability of returning true is 1/rarity
   int32_t                   randomInt()                         { return _randomGenerator.getRandom(); }
   int32_t                   randomInt(int32_t max)              { return _randomGenerator.getRandom(0, max); }
   int32_t                   randomInt(int32_t min, int32_t max) { return _randomGenerator.getRandom(min, max); }
   TR_RandomGenerator       *randomGenerator()                   { return &_randomGenerator; }

   TR_HasRandomGenerator(TR::Compilation *comp);
   };

#endif
