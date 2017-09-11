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

#include <stdlib.h>
#include <iostream>

#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"

template <class ABitVector, class Allocator>
int foo(int seed, Allocator &a) {
  ABitVector v1(a);

  uint32_t n = seed;

  v1.SetAll(n);

  int i = 0;
  auto iv = v1.begin();
  for (; (iv != v1.end()) && i < n ; ++iv, ++i) {
    if (iv != i) {
      if (i > iv)
        printf("%d: seed = %d ERROR: iterator index %d should not exist in vector.\n", __LINE__, seed, (uint32_t) iv);
      else
        printf("%d: seed = %d ERROR: iterator index %d should exist in vector.\n", __LINE__, seed, i);
      return 1;
    }
    // printf("iv    = %d\n", (uint32_t) iv);
  }
  if ((iv == v1.end()) != (i == n)) {
    printf("i     = %d\n", i);
    printf("n     = %d\n", n);
    printf("iv    = %d\n", (uint32_t) iv);
    printf("begin = %d\n", (uint32_t) v1.begin());
    printf("end   = %d\n", (uint32_t) v1.end());
    printf("%d: seed = %d ERROR: iterator has %sterminated, when there should %sindices left.\n",
        __LINE__, seed, (iv == v1.end()) ? "" : "not ", (i < n) ? "still be " : "be no ");
    return 1;
  }

  return 0;
}

typedef CS2::shared_allocator<CS2::heap_allocator<8192,10> > allocator;
typedef CS2::ABitVector<allocator> dense;

int main() {
  CS2::heap_allocator<8192,10> h;
  allocator a(h);

  for(int seed = 0; seed < 1000; seed++)
    if (foo<dense,allocator>(seed,a) == 1)
      return 1;
  return 0;
}
