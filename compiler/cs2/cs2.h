/*******************************************************************************
 * Copyright (c) 1996, 2016 IBM Corp. and others
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

#ifndef CS2_H
#define CS2_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// __func__  is not supported by Visual Studio 2003, use __FUNCTION__ instead
#if _MSC_VER >= 1300
# define __func__ __FUNCTION__
#endif

#ifdef _MSC_VER
// MSCV doesn't support C99 features, but recommends this as an alternative specifier for size_t
// (http://msdn.microsoft.com/en-us/library/vstudio/tcxf1dw6.aspx)
# define CS2_ZU "%Iu"
// C99 provides the z length modifier for size_t
#elif __STDC_VERSION__ >= 199901L
# define CS2_ZU "%zu"
#else
# define CS2_ZU "%lu"
#endif

#ifdef CS2_ASSERT
#include <stdio.h>
# define CS2Assert(cond, ss) if (!(cond)) { printf("COMPILER ASSERTION|%s|%s\n%s:%d ", "CS2", __func__, __FILE__, __LINE__); printf ss ; abort(); }
#else
# define CS2Assert(cond, ss) {}
#endif

namespace CS2 {

  class exception {
    const char *_f;
    const uint32_t _l;
  public:
    exception(const char *file = NULL, uint32_t line = 0) :
      _f(file), _l(line) {}
    const char * file() const { return _f;}
    uint32_t     line() const { return _l;}
  };

  namespace SystemResourceError {
    inline
      void Memory(){
      CS2Assert(0, ("SystemResourceError::Memory\n"));
    }
  }

  template <class c>
    void Swap (c &c1, c&c2) {
    c ctmp(c2);
    c2=c1;
    c1=ctmp;
  }

  template <class Key, class Value>
  class Pair {
    public:
    Pair(Key key, Value value) : fKey(key), fValue(value) {}
    Key    getKey()   { return fKey;   }
    Value  getValue() { return fValue; }

    void    setKey(Key key) { fKey = key; }
    void    setValue(Value value) { fValue = value; }
    private:
    Key   fKey;
    Value fValue;
  };
}

#endif
