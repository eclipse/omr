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

#include <cstdio>
#include <stdlib.h>
#include <iostream>

#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"

typedef CS2::shared_allocator<CS2::check_allocator<> > allocator;
typedef CS2::ASparseBitVector<allocator> sparse;
typedef CS2::ABitVector<allocator> dense;

#if 1
#define MAXVECTORS 128
#define MAXBIT (65536*128)
#define MAXPC (65536)
#else
#define MAXVECTORS 8
#define MAXBIT (65536*128)
#define MAXPC (4096)
#endif

int main(int argc, char **argv) {
  CS2::check_allocator<> ch;
  allocator a(ch);

  if (argc!=2) {
    printf("ERROR: argc=%d\n", argc);
    return 0;
  }

  int seed = atoi(argv[1]);
  srand(seed);

  sparse *s[MAXVECTORS];
  int count =rand()%MAXVECTORS;

  for (int i=0; i<count; i++) {
    s[i] = new sparse(a);
    if (!s[i]) {printf("ERROR %d\n", i); exit(1);}

    int pc =rand()%MAXPC;
    for (int j=0; j<pc; j++)
      (*s[i])[rand()%MAXBIT]=1;
  }

  sparse result(a);
  int pc =rand()%MAXPC;
  for (int j=0; j<pc; j++)
    result[rand()%MAXBIT]=1;

  sparse mask(a);
  pc =rand()%MAXPC;
  for (int j=0; j<pc; j++)
    mask[rand()%MAXBIT]=1;

  sparse copy(result);

  for (int i=0; i<count; i++)
    copy.Or(*s[i]);
  copy.And(mask);

  result.And(mask);
  sparse result2(result);
  result2.Or(&s[0], count);
  result2.And(mask);

  result.Or(&s[0], count, mask);

  printf("SEED=%d RES=%d\n", seed, (int)(copy==result));
  printf("SEED=%d RES=%d\n", seed, (int)(result2==result));
  if (!(copy==result) || !(result2==result)) {
    for (int i=0; i<count; i++)
      std::cout << i << ":" << *s[i] <<"\n";

    std::cout << "MASK=" << mask << "\n";
    std::cout << "RES=" << result << "\n";
    std::cout << "RES2=" << result2 << "\n";
    std::cout << "COPY=" << copy << "\n";
  }
  for (int i=0; i<count; i++)
    delete s[i];

  return 0;
}
