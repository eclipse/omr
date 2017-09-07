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

#define CS2_ASSERT
#define CS2_ALLOCINFO
#include <cstdio>
#include <iostream>
#include <cs2/arrayof.h>
#include <cs2/allocator.h>
#include <cs2/timer.h>
#include <cs2/sparsrbit.h>
#include <cs2/bitvectr.h>
#include <cs2/hashtab.h>

typedef CS2::trace_allocator<> a;

int main(int argc, char **argv) {
  CS2::ArrayOf<uint32_t, a> A1( a(), 3);
  CS2::ArrayOf<uint32_t, a> A2;

  CS2::PhaseTimingSummary<a> x("noname", a(), argc>1);

  CS2::LexicalBlockTimer<a> t("whatever", x);

  CS2::ABitVector<a> b,b2;
  CS2::ASparseBitVector<a> sb,sb2;

  b[34]=0;
  sb[34]=0;

  b2 = b;
  sb2 = sb;
  sb = b;

  CS2::HashTable<uint32_t, bool, a> h;

  CS2::HashIndex hi;
  bool f = h.Locate(10, hi);
}
