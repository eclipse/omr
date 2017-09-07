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

#include <iostream>
#include "cs2/llistof.h"

struct pair {
  pair(int _a, int _b): a(_a), b(_b) {}
  int a,b;
};

std::ostream &operator<< (std::ostream &out, struct pair &p) {
  return out << "(" << p.a << ":" << p.b <<")";
}

int main() {
  CS2::check_allocator<> ca;
  typedef CS2::LinkedListOf<pair, CS2::check_allocator<> > LIST;
  LIST lp(ca);

  bool isAddedToEnd = false;
  for (int i=0; i<10; i++) {
    lp.Add(pair(i,i*i), isAddedToEnd);
    isAddedToEnd = !isAddedToEnd;
  }

  LIST::Cursor c(lp);

  for (c.SetToFirst(); c.Valid(); c.SetToNext())
    std::cout << "[" << c->a << "," << c->b << "] ";
  std::cout << "\n";

  // test stack-based methods
  int i=0;
  while (!lp.IsEmpty())
    {
    pair &p = *lp.Head();
    int a = p.a;
    std::cout << "[" << p.a << "," << p.b << "] ";
    lp.Pop();

    if (a > 0)
      lp.Push(pair(a-1, a));
    else
      std::cout << "\n";
    i++;
    }

  for (int i=0; i<10; i++) {
    lp.Add(pair(i,i*i), true);
  }

  LIST l2(lp, lp);
  l2.Add(pair(3,5), true);

  std::cout << l2 << "\n";

  l2 = lp;
  std::cout << l2 << "\n";
  std::cout << "\n";
}
