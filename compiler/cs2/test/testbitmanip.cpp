/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "cs2/bitvectr.h"

template <class ABitVector>
int foo(int seed) {
  ABitVector v1,v2,v3;
  int count;
  srand(seed);

  count =rand()%1000000;
  for (int i=0; i<count; i++)
    v1[rand()%0xFFFFFF]=1;

  count =rand()%1000000;
  for (int i=0; i<count; i++)
    v2[rand()%0xFFFFFF]=(bool)v1[rand()%0xFFFFFF];

  count =rand()%1000000;
  for (int i=0; i<count; i++)
    v3[rand()%0xFFFFFF]=(bool)v2[rand()%0xFFFFFF];

  v2.Or(v3);
  v1.And(v3);

  v1.Or(v2,v3);
  v3.And(v2,v1);

  uint64_t x = 0x0200000000000000ull;

  uint32_t lastWordIndex = v1.LastWord();
  uint64_t lastWord = v1.WordValueAt(0, lastWordIndex);
  printf("x                        = %0*llX\n", CS2::kBitWordSize/4, x);
  printf("trailingZeroes(x)        = %d\n", CS2::BitManipulator::TrailingZeroes(x));
  if (x != lastWord) {
    printf("%0*llX != %0*llX\n", CS2::kBitWordSize/4, x, CS2::kBitWordSize/4, lastWord);
  } else {
    printf("Comparison shows that x is equal to lastWord\n");
  }
  printf("lastWord                 = %0*llX\n", CS2::kBitWordSize/4, lastWord);
  printf("trailingZeroes(lastWord) = %d\n", CS2::BitManipulator::TrailingZeroes(lastWord));
  return 1;
}

typedef CS2::BitVector dense;

int main() {
  int seed = 2;
  foo<dense>(seed);
  return 1;
}
