/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifndef BITMANIP_H
#define BITMANIP_H
#define IN_BITMANIP_H

#include <algorithm>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"

#if defined(TR_TARGET_X86) && defined(OMR_OS_WINDOWS)
   #define abs64 _abs64
#else
   #define abs64 labs
#endif /* defined(TR_TARGET_X86) && defined(OMR_OS_WINDOWS) */

// For getting at different parts of a 32 bit integer
class intParts  {
   public:
      intParts(int32_t x)             { intVal = x; }
      intParts()                      { intVal = 0; }
      int32_t getValue()              { return intVal; }
      int32_t setValue(int32_t value) { return intVal=value; }
      int32_t getHighBits()           { return (uint32_t)intVal >> 16; }
      int32_t getHighBitsSigned()     { return intVal >> 16; }
      int32_t getLowBits()            { return (uint32_t)intVal & 0xffff; }
      int32_t getLowBitsSigned()      { return (intVal << 16) >> 16; }
      int32_t getLowSign()            { return ((uint32_t)intVal >> 15) & 0x1; }
      int32_t getHighSign()           { return (uint32_t)intVal >> 31; }
      int32_t getByte3()              { return (uint32_t)intVal >> 24; }
      int32_t getByte2()              { return ((uint32_t)intVal >> 16) & 0xff; }
      int32_t getByte1()              { return ((uint32_t)intVal >> 8) & 0xff; }
      int32_t getByte0()              { return (uint32_t)intVal & 0xff; }

   private:
      int32_t intVal;
};

// Returns true iff all the 1-bits of mask are contiguous
// in the sense of "rlinm", e.g. of one of the forms
// 00...011...100...0 or 11...100...011...1, (or if mask = 0)
static inline bool contiguousBits(int32_t lmask)
   {
   int32_t amask;                            // mask as a signed value so the shifts will be algebraic
   amask = lmask;
   lmask = lmask^(amask>>31);            // 1's-complement if negative.
   lmask = ((lmask|(lmask-1))+1)&lmask;  // Turn off rightmost contiguous
                                         // string of 1's; result is 0 iff
                                         // orig. mask was in desired form.
   amask = lmask;
   lmask = abs(amask)-1;

   return  (lmask>>31) != 0;             // Map 0 --> '1'b, else --> '0'b
   }

static inline bool contiguousBits(uint32_t lmask)
   {
   return contiguousBits((int32_t) lmask);
   }

static inline bool contiguousBits(int64_t lmask)
  {
   int64_t amask;                            // mask as a signed value so the shifts will be algebraic
   amask = lmask;
   lmask = lmask^(amask>>63);            // 1's-complement if negative.
   lmask = ((lmask|(lmask-1))+1)&lmask;  // Turn off rightmost contiguous
                                         // string of 1's; result is 0 iff
                                         // orig. mask was in desired form.
   amask = lmask;
   lmask = abs64(amask)-1;
   return  (lmask>>63) != 0;             // Map 0 --> '1'b, else --> '0'b

  }
static inline bool contiguousBits(uint64_t lmask)
   {
   return contiguousBits((int64_t) lmask);
   }

#if defined(TR_HOST_POWER)  && (defined(__IBMC__) || defined(__IBMCPP__) || defined(__ibmxl__))
#include <builtins.h>

// Return a count 0..32 of leading zeroes in the given word
static inline int32_t leadingZeroes (int32_t inputWord)
   {
   return __cntlz4 (inputWord);
   }

#ifdef TR_HOST_64BIT
// Return a count 0..64 of leading zeroes in the given doubleword
static inline int32_t leadingZeroes (int64_t input)
   {
   return __cntlz8 (input);
   }
#else
// Return a count 0..64 of leading zeroes in the given doubleword
static inline int32_t leadingZeroes (int64_t input)
   {
   uint32_t highWord=input>>32;
   uint32_t lowWord=input&0xffffffff;
   uint32_t count = __cntlz4(highWord);
   if (count==32)
     count += __cntlz4(lowWord);

   return count;
   }
#endif
#else
extern int32_t leadingZeroes (int32_t inputWord);
extern int32_t leadingZeroes (int64_t input);
#endif


static inline int32_t leadingZeroes (uint64_t input)
   {
   return (leadingZeroes((int64_t)input));
   }

static inline int32_t leadingZeroes (uint32_t inputWord)
   {
   return leadingZeroes ((int32_t)inputWord);
   }

// Return a count 0..32 of trailing zeroes in the given word
static inline int32_t trailingZeroes (int32_t inputWord)
   {
   int32_t work;
   work = inputWord;
   work = ~work & (work - 1);
   return 32 - leadingZeroes(work);
   }
static inline int32_t trailingZeroes (uint32_t inputWord)
   {
   return trailingZeroes((int32_t)inputWord);
   }

// Return a count 0..64 of trailing zeroes in the given doubleword
static inline int32_t trailingZeroes (int64_t input)
   {
   int64_t work;
   work = input;
   work = ~work & (work - 1);
   return 64 - leadingZeroes(work);
   }

static inline int32_t trailingZeroes(uint64_t input)
   {
   return trailingZeroes((int64_t)input);
   }

static inline int32_t ceilingPowerOfTwo (int32_t inputWord)
   {
  return 1 << (32 - leadingZeroes(inputWord - 1));
   }

static inline int32_t floorPowerOfTwo (int32_t inputWord)
   {
   return 1 << (31 - leadingZeroes(inputWord));
   }

static inline int64_t floorPowerOfTwo64 (int64_t inputWord)
   {
   return 1LL << (63 - leadingZeroes(inputWord));
   }

static inline int32_t leadingOnes (int32_t inputWord)
   {
   return leadingZeroes (~inputWord);
   }

static inline int32_t leadingOnes (uint32_t inputWord)
   {
   return leadingZeroes (~inputWord);
   }

static inline int32_t leadingOnes (int64_t input)
   {
   return leadingZeroes (~input);
   }

static inline int32_t leadingOnes (uint64_t input)
   {
   return leadingZeroes (~input);
   }

#if defined(TR_HOST_ARM64) && (defined(__GNUC__) || defined(__clang__))
// return the number of 1-bits in the argument
static inline int32_t populationCount (int32_t inputWord)
   {
   return __builtin_popcount(inputWord);
   }
#else
// return the number of 1-bits in the argument
static inline int32_t populationCount (int32_t inputWord)
   {
   uint32_t work, temp;

   work = inputWord;
   if (0 == work) return 0;
   work = work - ((work >> 1) & 0x55555555ul);
   temp = ((work >> 2) & 0x33333333ul);
   work = (work & 0x33333333ul) + temp;
   work = (work + (work >> 4)) & 0x0F0F0F0Ful;
   work = work + (work << 8);
   work = work + (work << 16);
   return work >> 24;
   }
#endif

static inline int32_t populationCount (uint32_t inputWord)
   {
   return populationCount((int32_t)inputWord);
   }

#if defined(TR_HOST_ARM64) && (defined(__GNUC__) || defined(__clang__))
// return the number of 1-bits in the argument
static inline int32_t populationCount (int64_t inputWord)
   {
   return __builtin_popcountll(inputWord);
   }
#else
// return the number of 1-bits in the argument
static inline int32_t populationCount (int64_t inputWord)
   {
   uint64_t work, temp;

   work = inputWord;
   if (0 == work) return 0;
   work = work - ((work >> 1) & CONSTANT64(0x5555555555555555));
   temp = ((work >> 2) & CONSTANT64(0x3333333333333333));
   work = (work & CONSTANT64(0x3333333333333333)) + temp;
   work = (work + (work >> 4)) & CONSTANT64(0x0F0F0F0F0F0F0F0F);
   work = work + (work << 8);
   work = work + (work << 16);
   work = work + (work << 32);
   return (int32_t)(work >> 56);
   }
#endif

static inline int32_t populationCount (uint64_t inputWord)
   {
   return populationCount((int64_t)inputWord);
   }

// return 10^exponent
static inline uint64_t computePositivePowerOfTen(int32_t exponent)
   {
   // TODO: there is a better algorithm to reduce the number of multiplies -- see Simplifier.cpp reduceExpTwoAndGreaterToMultiplication
   TR_ASSERT(exponent >= 0 && exponent <= TR::getMaxSignedPrecision<TR::Int64>(),"exponent %d should be in the inclusive range 0->%d\n",exponent,TR::getMaxSignedPrecision<TR::Int64>());
   uint64_t base = 1;
   for (int32_t i = 0; i < exponent; i++)
      base = base * 10;
   return base;
   }

static inline bool isPositivePowerOfTen(int64_t val)
   {
   int32_t exponent = trailingZeroes((uint64_t)val);
   if (exponent <= TR::getMaxSignedPrecision<TR::Int64>() && (uint64_t)val == computePositivePowerOfTen(exponent))
      return true;
   else
      return false;
   }

#define TR_MAX_PRECISION_LOOKUP 18
static int64_t maxAbsoluteValueTable[TR_MAX_PRECISION_LOOKUP] =
   {
   // value for               // precision
   9,                         // 1
   99,                        // 2
   999,                       // 3
   9999,                      // 4
   99999,                     // 5
   999999,                    // 6
   9999999,                   // 7
   99999999,                  // 8
   999999999,                 // 9
   CONSTANT64(9999999999),                // 10
   CONSTANT64(99999999999),               // 11
   CONSTANT64(999999999999),              // 12
   CONSTANT64(9999999999999),             // 13
   CONSTANT64(99999999999999),            // 14
   CONSTANT64(999999999999999),           // 15
   CONSTANT64(9999999999999999),          // 16
   CONSTANT64(99999999999999999),         // 17
   CONSTANT64(999999999999999999),        // 18
   };

static inline int64_t getMaxAbsValueForPrecision(int32_t precision)
   {
   if (precision > 0 && precision <= TR_MAX_PRECISION_LOOKUP)
      return maxAbsoluteValueTable[precision-1];
   else
      return TR::getMaxSigned<TR::Int64>();
   }

static int32_t getPrecisionFromValue(int64_t value)
   {
   if (value == TR::getMinSigned<TR::Int64>())
      return TR::getMaxSignedPrecision<TR::Int64>();

   if (value < 0)
      value *= -1;

   for (int32_t i = 0; i < TR_MAX_PRECISION_LOOKUP; i++)
      {
      if (value <= maxAbsoluteValueTable[i])
         return i + 1;
      }

   return TR::getMaxSignedPrecision<TR::Int64>();
   }

static inline int32_t getPrecisionFromRange(int64_t low, int64_t high)
   {
   return std::max(getPrecisionFromValue(low), getPrecisionFromValue(high));
   }

static inline bool isEven(int32_t input)
   {
   return (input&0x1) == 0;
   }

static inline bool isOdd(int32_t input)
   {
   return (input&0x1) != 0;
   }

static inline bool isEven(uint32_t input)
   {
   return isEven((int32_t)input);
   }

static inline bool isOdd(uint32_t input)
   {
   return isOdd((int32_t)input);
   }

static inline bool isEven(int64_t input)
   {
   return (input&0x1) == 0;
   }

static inline bool isOdd(int64_t input)
   {
   return (input&0x1) != 0;
   }

static inline bool isEven(uint64_t input)
   {
   return isEven((int64_t)input);
   }

static inline bool isOdd(uint64_t input)
   {
   return isOdd((int64_t)input);
   }

static inline bool isNonNegativePowerOf2(int32_t input)
   {
   if (input == INT_MIN)
      {
      return false;
      }
   else
      {
      return (input & -input) == input;
      }
   }

static inline bool isNonPositivePowerOf2(int32_t input)
   {
   return (input & -input) == -input;
   }

/**
 * \brief Tests whether the operand is a power of 2
 *
 * \param input The 32-bit signed integer value to be tested
 *
 * \return \c true, if the operand is a power of 2;
 *         \c false, otherwise
 */
static inline bool isPowerOf2(int32_t input)
   {
   input = input < 0 ? -input : input;
   return (input & -input) == input;
   }

/**
 * \brief Tests whether the operand is a power of 2
 *
 * \param input The 32-bit unsigned integer value to be tested
 *
 * \return \c true, if the operand is a power of 2;
 *         \c false, otherwise
 */
static inline bool isPowerOf2(uint32_t input)
   {
   return (input != 0) && (input & (input-1)) == 0;
   }

static inline bool isNonNegativePowerOf2(int64_t input)
   {
   if (input == LONG_MIN)
      {
      return false;
      }
   else
      {
      return (input & -input) == input;
      }
   }

static inline bool isNonPositivePowerOf2(int64_t input)
   {
   return (input & -input) == -input;
   }

/**
 * \brief Tests whether the operand is a power of 2
 *
 * \param input The 64-bit signed integer value to be tested
 *
 * \return \c true, if the operand is a power of 2;
 *         \c false, otherwise
 */
static inline bool isPowerOf2(int64_t input)
   {
   input = input < 0 ? -input : input;
   return (input & -input) == input;
   }

/**
 * \brief Tests whether the operand is a power of 2
 *
 * \param input The 64-bit unsigned integer value to be tested
 *
 * \return \c true, if the operand is a power of 2;
 *         \c false, otherwise
 */
static inline bool isPowerOf2(uint64_t input)
   {
   return (input != 0) && (input & (input-1)) == 0;
   }

#if defined(OSX)
// On OSX, intptr_t isn't int32_t nor int64_t

static inline int32_t leadingZeroes (intptr_t input)
   {
#ifdef TR_HOST_64BIT
   return leadingZeroes ((int64_t)input);
#else
   return leadingZeroes ((int32_t)input);
#endif
   }

static inline int32_t trailingZeroes(intptr_t input)
   {
#ifdef TR_HOST_64BIT
   return trailingZeroes((int64_t)input);
#else
   return trailingZeroes((int32_t)input);
#endif
   }

static inline int32_t populationCount(intptr_t input)
   {
#ifdef TR_HOST_64BIT
   return populationCount((int64_t)input);
#else
   return populationCount((int32_t)input);
#endif
   }

static inline bool isPowerOf2(intptr_t input)
   {
#ifdef TR_HOST_64BIT
   return isPowerOf2((int64_t)input);
#else
   return isPowerOf2((int32_t)input);
#endif
   }
#endif

#undef IN_BITMANIP_H

#endif // BITMANIP_H
