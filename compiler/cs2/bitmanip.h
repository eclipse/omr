/*******************************************************************************
 * Copyright IBM Corp. and others 1996
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/***************************************************************************/
/*                                                                         */
/*  File name:  bitmanip.h                                                 */
/*  Purpose:    Definition of various bitwise operations.                  */
/*                                                                         */
/***************************************************************************/

#ifndef CSBITMANIP_H
#define CSBITMANIP_H

#if (defined(__IBMCPP__) || defined(__open_xl__) && defined(__cplusplus)) && defined (__PPC__)
// to __cntlz4 and related routines
# include "builtins.h"
#endif

namespace CS2 {

static const uint8_t kByteLeadingZeroes[256] = {
  /* 0x00 */ 8,
  /* 0x01 */ 7,
  /* 0x02 */ 6, 6,
  /* 0x04 */ 5, 5, 5, 5,
  /* 0x08 */ 4, 4, 4, 4, 4, 4, 4, 4,
  /* 0x10 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  /* 0x20 */ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
             2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  /* 0x40 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
  /* the remainder of the array is filled with zeroes */
  };

/// BitManipulator
///
/// \brief A class of functions to manipulate bit masks
/// \ingroup CompilerServices
namespace BitManipulator {

  /// \brief count the number of 1-bits in the argument, and return the count
  uint32_t PopulationCount (uint32_t);
  uint32_t PopulationCount (uint64_t);

  /// \brief Return a count 0..32 of leading zeroes in the given word
  uint32_t LeadingZeroes (uint32_t);
  uint32_t EmulateLeadingZeroes (uint32_t);

  /// \brief Return a count 0..64 of leading zeroes in the given doubleword
  uint32_t LeadingZeroes (uint64_t);
  uint32_t EmulateLeadingZeroes (uint64_t);

  /// \brief Return a count 0..32 of leading ones in the given word
  uint32_t LeadingOnes (uint32_t);

  /// \brief Return a count 0..64 of leading ones in the given doubleword
  uint32_t LeadingOnes (uint64_t);

  /// \brief Return a count 0..32 of trailing zeroes in the given word
  uint32_t TrailingZeroes (uint32_t);

  /// \brief Return a count 0..64 of trailing zeroes in the given doubleword
  uint32_t TrailingZeroes (uint64_t);

  /// \brief Return a count 0..32 of trailing ones in the given word
  uint32_t TrailingOnes (uint32_t);

  /// \brief Return a count 0..64 of trailing ones in the given doubleword
  uint32_t TrailingOnes (uint64_t);

  /// \brief Determine the ceiling power of two of the given number (ie. return
  /// the next higher power of two unless the input is already a power of
  /// two, in which case the input is returned).
  uint32_t CeilingPowerOfTwo (uint32_t);
  uint64_t CeilingPowerOfTwo (uint64_t);

  /// \brief Determine the floor power of two of the given number (ie. return
  /// the next lower power of two unless the input is already a power of
  /// two, in which case the input is returned).
  uint64_t FloorPowerOfTwo (uint64_t);

  /// \brief Byte reverse a short
  uint16_t ByteReverse (uint16_t);

  /// \brief Byte reverse a word
  uint32_t ByteReverse (uint32_t);

  /// \brief Byte reverse a doubleword
  uint64_t ByteReverse (uint64_t);

  /// \brief Determine if a given numbers 1-bits are continugous in the form of:
  /// 00...011...100...0 or 11...100...011...1, (or if == 0)
  bool ContiguousOnes (int32_t);
  bool ContiguousOnes (int64_t);

  /// \brief Return a count 0..8 of leading zeroes in the given byte
  uint32_t LeadingZeroes (uint8_t);

  uint32_t High32Of64(uint64_t u64);
  uint32_t Low32Of64(uint64_t u64);
};

inline
  uint32_t BitManipulator::High32Of64(uint64_t u64){
      return uint32_t(u64>>32);
  }

inline
  uint32_t BitManipulator::Low32Of64(uint64_t u64){
      return uint32_t(u64);
  }

#if (defined(__IBMCPP__) || defined(__ibmxl__) || defined(__open_xl__)) && defined (__PPC__)
inline uint32_t BitManipulator::LeadingZeroes (uint32_t inputWord) {
  return __cntlz4 (inputWord);
}

#  if defined(__64BIT__)
inline uint32_t BitManipulator::LeadingZeroes (uint64_t inputWord) {
  return __cntlz8 (inputWord);
}
#  else
inline uint32_t BitManipulator::LeadingZeroes (uint64_t inputWord) {
  return BitManipulator::EmulateLeadingZeroes (inputWord);
}
#  endif
#else
inline uint32_t BitManipulator::LeadingZeroes (uint32_t inputWord) {
  return BitManipulator::EmulateLeadingZeroes (inputWord);
}

inline uint32_t BitManipulator::LeadingZeroes (uint64_t inputWord) {
  return BitManipulator::EmulateLeadingZeroes (inputWord);
}
#endif

inline uint32_t BitManipulator::LeadingOnes (uint32_t inputWord) {
  return BitManipulator::LeadingZeroes (~inputWord);
}

inline uint32_t BitManipulator::LeadingOnes (uint64_t inputWord) {
  return BitManipulator::LeadingZeroes (~inputWord);
}

inline uint32_t BitManipulator::TrailingZeroes (uint32_t inputWord) {
  uint32_t work;

  work = inputWord;
  work = ~work & (work - 1);

  return 32 - LeadingZeroes(work);
}

inline uint32_t BitManipulator::TrailingZeroes (uint64_t inputDoubleWord) {
  uint32_t lowerWord, upperWord;

  lowerWord = Low32Of64(inputDoubleWord);
  if (lowerWord == 0) {
    upperWord = High32Of64(inputDoubleWord);
    return 32 + TrailingZeroes(upperWord);
  } else return TrailingZeroes(lowerWord);
}

inline uint32_t BitManipulator::TrailingOnes (uint32_t inputWord) {
  return BitManipulator::TrailingZeroes (~inputWord);
}

inline uint32_t BitManipulator::TrailingOnes (uint64_t inputWord) {
  return BitManipulator::TrailingZeroes (~inputWord);
}

inline uint32_t BitManipulator::CeilingPowerOfTwo (uint32_t inputWord) {
  return 1 << (32 - LeadingZeroes(inputWord - 1));
}

inline uint64_t BitManipulator::CeilingPowerOfTwo (uint64_t inputDoubleWord) {
  return (uint64_t)1 << (64 - LeadingZeroes(inputDoubleWord - (uint64_t)1));
}

inline uint64_t BitManipulator::FloorPowerOfTwo (uint64_t inputDoubleWord) {
  return (uint64_t)1 << (63 - LeadingZeroes(inputDoubleWord));
}

inline uint16_t BitManipulator::ByteReverse (uint16_t inputHalfWord) {
  uint16_t reversedHalfWord;
  reversedHalfWord = (inputHalfWord << 8) | (inputHalfWord >> 8);
  return reversedHalfWord;
}

inline uint32_t BitManipulator::ByteReverse (uint32_t inputWord) {
  uint32_t reversedWord;
  reversedWord = (inputWord << 24) |
                 ((inputWord & 0xFF00ul) << 8) |
                 ((inputWord >> 8) & 0xFF00ul) |
                 (inputWord >> 24);
  return reversedWord;
}

// BitManipulator::PopulationCount
//
// count the number of 1-bits in the argument, and return the count

inline uint32_t BitManipulator::PopulationCount (uint32_t inputWord) {
  uint32_t popCount;

  uint32_t work, temp;

  work = inputWord;
  if(0 == work) return 0;
  work = work - ((work >> 1) & 0x55555555ul);
  temp = ((work >> 2) & 0x33333333ul);
  work = (work & 0x33333333ul) + temp;
  work = (work + (work >> 4)) & 0x0F0F0F0Ful;
  work = work + (work << 8);
  work = work + (work << 16);
  popCount = work >> 24;

  return popCount;

}

#ifdef __64BIT__
inline uint32_t BitManipulator::PopulationCount (uint64_t inputDoubleWord) {
  uint64_t work;
  uint32_t res;

  work = inputDoubleWord;
  if (inputDoubleWord == 0) return 0;

  work = work - ((work >> 1) & 0x5555555555555555ull);
  work = (work & 0x3333333333333333ull) + ((work>>2) & 0x3333333333333333ull);

  res = ((uint32_t) work) + ((uint32_t) (work >> 32));
  res = (res & 0x0F0F0F0F) + ((res >> 4) & 0x0F0F0F0F);
  res = (res & 0xFFFF) + (res >> 16);
  res = (res & 0xFF) + (res >> 8);
  return res;
}
#else
inline uint32_t BitManipulator::PopulationCount (uint64_t inputDoubleWord) {
  return PopulationCount(High32Of64(inputDoubleWord)) +
         PopulationCount(Low32Of64(inputDoubleWord));
}
#endif

inline uint32_t BitManipulator::LeadingZeroes (uint8_t inputByte) {
  return kByteLeadingZeroes[inputByte];
}

// BitManipulator::EmulateLeadingZeroes (uint32_t)
//
// Return a count 0..32 of leading zeroes in the given word

inline
uint32_t BitManipulator::EmulateLeadingZeroes (uint32_t inputWord) {
  uint32_t byteMask, bitCount;

  byteMask = 0xFFul << 24;  // a byte mask high order justified

  // find first non-zero byte
  for (bitCount = 0; bitCount < 32; bitCount += 8) {
    uint8_t byteValue;
    uint32_t testWord;

    testWord = inputWord & byteMask;
    if (testWord != 0) {
      byteValue = testWord >> (24 - bitCount);
      return bitCount + BitManipulator::LeadingZeroes (byteValue);
    }

    byteMask >>= 8;
  }

  return 32;
}

// BitManipulator::EmulateLeadingZeroes (uint64_t)
//
// Return a count 0..64 of leading zeroes in the given doubleword

inline
uint32_t BitManipulator::EmulateLeadingZeroes (uint64_t inputDoubleWord) {
  uint32_t testWord = uint32_t(inputDoubleWord >> 32);
  uint32_t adjust   = 0;

  if (testWord == 0) {
    // upper word is zero
    testWord = uint32_t(inputDoubleWord);
    adjust = 32;
  }
  return adjust + BitManipulator::EmulateLeadingZeroes (testWord);
}

// BitManipulator::ByteReverse (uint64_t)
//
// Byte reverse a doubleword

inline
uint64_t BitManipulator::ByteReverse (uint64_t inputDoubleWord) {
  uint64_t reversedDoubleWord;
  uint32_t upperWord, reversedUpperWord,
         lowerWord, reversedLowerWord;

  upperWord = uint32_t(inputDoubleWord>>32);
  reversedUpperWord = BitManipulator::ByteReverse (upperWord);

  lowerWord = uint32_t(inputDoubleWord);
  reversedLowerWord = BitManipulator::ByteReverse (lowerWord);

  reversedDoubleWord = ((uint64_t) reversedLowerWord << 32u) |
                       ((uint64_t) reversedUpperWord);

  return reversedDoubleWord;
}

// BitManipulator::ContiguousOnes (int32_t)
//
// Return true if there is a set of contiguous ones/zeroes
inline
bool BitManipulator::ContiguousOnes (int32_t mask)
{
  // 1's complement if negative
  mask ^= mask >> 31;
  mask = ((mask | (mask - 1)) + 1) & mask;
  if (mask == 0)
    return true;
  return false;
}

// BitManipulator::ContiguousOnes (int64_t)
//
// Return true if there is a set of contiguous ones/zeroes
inline
bool BitManipulator::ContiguousOnes (int64_t mask)
{
  // 1's complement if negative
  mask ^= mask >> 63;
  uint64_t umask = mask;
  umask = ((umask | (umask - (uint64_t)1)) + (uint64_t)1) & umask;
  if (umask == (uint64_t)0)
    return true;
  return false;
}

}
#endif // CSBITMANIP_H
