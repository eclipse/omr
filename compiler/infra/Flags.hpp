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

#ifndef OMR_FLAGS_INCL
#define OMR_FLAGS_INCL

#include <stdint.h>


#define TO_MASK8(b) (1<<(uint8_t)b)
#define TO_MASK16(b) (1<<(uint16_t)b)
#define TO_MASK32(b) (1<<(uint32_t)b)

namespace OMR
{

// Maintain a set of ``packed'' flags and values as an unsigned type.
// Supply methods to test the values of flags and values.
//
template<class T> class FlagsTemplate
   {

   public:

   FlagsTemplate(T t = 0) : _flags(t) {}

   bool testAny(T mask) const { return (_flags & mask) != 0; }
   bool testAll(T mask) const { return (_flags & mask) == mask; }

   void set(T mask) { _flags |= mask; }
   void set(FlagsTemplate<T> mask) { _flags |= mask._flags; }
   void reset(T mask) { _flags &= ~mask; }
   void set(T mask, bool b) { b ? set(mask) : reset(mask); }
   void toggle(T mask) { _flags ^= mask; }
   void clear() { _flags = 0; }
   bool isClear() { return (_flags == 0); }

   // The next three methods manipulate a single multi-bit value.
   // It can be used, for example, to pack small integers together.
   //
   bool testValue(T mask, T value) const { return (_flags & mask) == value; }
   void setValue(T mask, T value) { _flags = (_flags & ~mask) | value; }
   T getValue(T mask) const { return _flags & mask; }
   T getValue() const { return _flags; }

   bool operator==(FlagsTemplate<T> &other) { return getValue() == other.getValue(); }

   private:

   T _flags;
   };

typedef FlagsTemplate<uint8_t>  flags8_t;
typedef FlagsTemplate<uint16_t> flags16_t;
typedef FlagsTemplate<uint32_t> flags32_t;
typedef FlagsTemplate<uint64_t> flags64_t;

}

using OMR::flags8_t;
using OMR::flags16_t;
using OMR::flags32_t;
using OMR::flags64_t;

#endif
