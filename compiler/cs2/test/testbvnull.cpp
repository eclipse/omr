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

#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"

template <class ABitVector, class Allocator>
int foo(int seed, Allocator &a) {
  ABitVector v1(a),v2(a),v3(a);
  int count;

  if (!v1.IsNull()) {
    printf("%d: ERROR v1 initial value should be NULL\n", __LINE__);
    return 0;
  }

  if (!v2.IsNull()) {
    printf("%d: ERROR v2 initial value should be NULL\n", __LINE__);
    return 0;
  }

  if (!v3.IsNull()) {
    printf("%d: ERROR v3 initial value should be NULL\n", __LINE__);
    return 0;
  }

  v1.Clear();
  v2 = v1;
  v3 = v2;

  if (v1.IsNull()) {
    printf("%d: ERROR v1 should not be NULL after a clear\n", __LINE__);
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
  }

  if (v2.IsNull()) {
    printf("%d: ERROR v2 should not be NULL after an assign from non NULL\n", __LINE__);
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  if (v3.IsNull()) {
    printf("%d: ERROR v3 should not be NULL after an assign from non NULL\n", __LINE__);
    printf("%d: v3.IsZero() = %s\n", __LINE__, v3.IsZero() ? "true" : "false");
    printf("%d: v3.IsNull() = %s\n", __LINE__, v3.IsNull() ? "true" : "false");
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    return 0;
  }

  {
    ABitVector v4(v3);
    if (v4.IsNull()) {
      printf("%d: ERROR v4 initial value should not be NULL after a copy constructor from non NULL\n", __LINE__);
      return 0;
    }
  }

  v1.ClearToNull();
  v2 = v1;
  v3 = v2;

  if (!v1.IsNull()) {
    printf("%d: ERROR v1 should be NULL after ClearToNull\n", __LINE__);
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  if (!v2.IsNull()) {
    printf("%d: ERROR v2 should be NULL after an assign from NULL\n", __LINE__);
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  if (!v3.IsNull()) {
    printf("%d: ERROR v3 should be NULL after an assign from NULL\n", __LINE__);
    printf("%d: v3.IsZero() = %s\n", __LINE__, v3.IsZero() ? "true" : "false");
    printf("%d: v3.IsNull() = %s\n", __LINE__, v3.IsNull() ? "true" : "false");
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    return 0;
  }

  {
    ABitVector v4(v3);
    if (!v4.IsNull()) {
      printf("%d: ERROR v4 initial value should be NULL after a copy constructor from NULL\n", __LINE__);
      return 0;
    }
  }

  v1.GrowTo(0);
  v2 = v1;
  v3 = v2;

  if (v1.IsNull()) {
    printf("%d: ERROR v1 should be non-NULL after GrowTo\n", __LINE__);
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  if (v2.IsNull()) {
    printf("%d: ERROR v2 should be non-NULL after an assign from non-NULL\n", __LINE__);
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  if (v3.IsNull()) {
    printf("%d: ERROR v3 should be non-NULL after an assign from non-NULL\n", __LINE__);
    printf("%d: v3.IsZero() = %s\n", __LINE__, v3.IsZero() ? "true" : "false");
    printf("%d: v3.IsNull() = %s\n", __LINE__, v3.IsNull() ? "true" : "false");
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    return 0;
  }

  v1.ClearToNull();
  v2 = v1;
  v3 = v2;

  srand(seed);

  count =rand()%1000;
  for (int i=0; i<count; i++) {
    int ix=rand()%0xFFFFFF;
    v1[ix]=1;
    if (v1.IsNull()) {
      printf("%d: ERROR v1 should not be NULL after v1[%d]=1  (iteration %d)\n", __LINE__, ix, i);
      return 0;
    }
  }

  count =rand()%1000;
  bool wasSet = false;
  for (int i=0; i<count; i++) {
    int ix=rand()%0xFFFFFF;
    int iy=rand()%0xFFFFFF;
    v2[ix]= (bool) v1[iy];
    if ((int) v1[iy])
      wasSet = true;
    if (v2.IsNull() && wasSet) {
      printf("%d: ERROR v2 should not be NULL after v2[%d]=v1[%d], v1[%d]=%d  (iteration %d)\n", __LINE__, ix, iy, iy, (int) v1[iy], i);
      return 0;
    }
  }

  count =rand()%1000;
  wasSet = false;
  for (int i=0; i<count; i++) {
    int ix=rand()%0xFFFFFF;
    int iy=rand()%0xFFFFFF;
    v3[ix]= (bool) v2[iy];
    if ((int) v2[iy])
      wasSet = true;
    if (v3.IsNull() && wasSet) {
      printf("%d: ERROR v3 should not be NULL after v3[%d]=v2[%d], v2[%d]=%d  (iteration %d)\n", __LINE__, ix, iy, iy, (int) v2[iy], i);
      return 0;
    }
  }

  v2.Or(v3);

  if (v2.IsNull() && !v3.IsNull()) {
    printf("%d: ERROR v2 should not be NULL after an Or with non-Null\n", __LINE__);
    printf("%d: v3.IsZero() = %s\n", __LINE__, v3.IsZero() ? "true" : "false");
    printf("%d: v3.IsNull() = %s\n", __LINE__, v3.IsNull() ? "true" : "false");
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    return 0;
  }

  v1.And(v3);

  if (v1.IsNull() && !v3.IsNull()) {
    printf("%d: ERROR v1 should not be NULL after an And with non-NULL\n", __LINE__);
    printf("%d: v3.IsZero() = %s\n", __LINE__, v3.IsZero() ? "true" : "false");
    printf("%d: v3.IsNull() = %s\n", __LINE__, v3.IsNull() ? "true" : "false");
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  v1.Or(v2,v3);

  if (v3.IsNull() && (!v1.IsNull() || !v2.IsNull()) ) {
    printf("%d: ERROR v1 should not be NULL after an Or\n", __LINE__);
    printf("%d: v3.IsZero() = %s\n", __LINE__, v3.IsZero() ? "true" : "false");
    printf("%d: v3.IsNull() = %s\n", __LINE__, v3.IsNull() ? "true" : "false");
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  v3.And(v2,v1);

  if (v1.IsNull() && (!v3.IsNull() || !v2.IsNull()) ) {
    printf("%d: ERROR v3 should not be NULL after an And\n", __LINE__);
    printf("%d: v3.IsZero() = %s\n", __LINE__, v3.IsZero() ? "true" : "false");
    printf("%d: v3.IsNull() = %s\n", __LINE__, v3.IsNull() ? "true" : "false");
    printf("%d: v2.IsZero() = %s\n", __LINE__, v2.IsZero() ? "true" : "false");
    printf("%d: v2.IsNull() = %s\n", __LINE__, v2.IsNull() ? "true" : "false");
    printf("%d: v1.IsZero() = %s\n", __LINE__, v1.IsZero() ? "true" : "false");
    printf("%d: v1.IsNull() = %s\n", __LINE__, v1.IsNull() ? "true" : "false");
    return 0;
  }

  if (v1.IsZero()) return 0;
  return v1.FirstOne()+v1.LastOne()+v1.PopulationCount();
}

typedef CS2::shared_allocator<CS2::heap_allocator<8192,10> > allocator;
typedef CS2::ASparseBitVector<allocator> sparse;
typedef CS2::ABitVector<allocator> dense;

int main() {
  CS2::heap_allocator<8192,10> h;
  allocator a(h);

  for (int seed = 0; seed < 1000; seed++)
    if (foo<sparse,allocator>(seed,a) != foo<dense,allocator>(seed,a)) {
      printf("%d: ERROR %d!=%d\n", __LINE__, foo<sparse,allocator>(seed,a), foo<dense,allocator>(seed,a));
      return 1;
    }
  return 0;
}
