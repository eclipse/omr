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

#ifndef BITVECTOR_INCL
#define BITVECTOR_INCL

#include <stdint.h>
#include <string.h>
#include "env/FilePointerDecl.hpp"
#include "env/TRMemory.hpp"
#include "env/defines.h"
#include "infra/Assert.hpp"

class TR_BitVector;
class TR_BitVectorCursor;
namespace TR { class Compilation; }

#if defined(BITVECTOR_64BIT)
typedef uint64_t chunk_t;
#define BITS_IN_CHUNK 64
#define SHIFT    6
#else
typedef uint32_t chunk_t;
#define BITS_IN_CHUNK 32
#define SHIFT    5
#endif

#define BV_SANITY_CHECK 0

enum TR_BitContainerType
   {
   singleton,
   bitvector
   };

class TR_BitContainer
   {
   public:
   TR_ALLOC(TR_Memory::BitVector)

   TR_BitContainer() : _type(bitvector), _bitVector(NULL) {}
   TR_BitContainer(int32_t index) : _type(singleton), _singleBit(index) {}
   TR_BitContainer(TR_BitVector *bv) : _type(bitvector), _bitVector(bv) {}
   operator TR_BitVector *()
      {
      TR_ASSERT(_type == bitvector, "BitContainer cannot be converted to BitVector\n");
      return _bitVector;
      }

   TR_BitVector * getBitVector() { return (_type == bitvector ? _bitVector : NULL); }

   // TR_BitVector methods (have extra run time check)
   int32_t get(int32_t n);
   bool intersects(TR_BitVector& v2);
   bool intersects(TR_BitContainer& v2);
   bool operator== (TR_BitVector& v2);

   bool hasMoreThanOneElement();

   bool isEmpty();
   bool isSingleValue() { return (_type != bitvector); }
   int32_t getSingleValue() { return _singleBit; }

   private:
   union
      {
      int32_t        _singleBit;
      TR_BitVector * _bitVector;
      };
   TR_BitContainerType  _type;

   friend class TR_BitVector;
   friend class TR_BitContainerIterator;
   };

/**
 * A simple data structure for use in single-bit dataflow analyses.
 *
 * The normal BitVector and BitContainer classes used with the dataflow engine
 * can have a significant overhead when only a single bit is being propagated
 * owing to their backing storage. This class encapsulates a simple bool value
 * and implements a BitVector-style interface making it compatible with the
 * data flow engine.
 */
class TR_SingleBitContainer
   {
   public:
   TR_ALLOC(TR_Memory::BitVector)
   typedef int32_t containerCharacteristic; // used by data flow
   static const containerCharacteristic nullContainerCharacteristic = -1;

   TR_SingleBitContainer() : _value(false) {}
   TR_SingleBitContainer(int64_t initBits, TR_Memory * m, TR_AllocationKind allocKind = heapAlloc) : _value(false) { }
   TR_SingleBitContainer(int64_t initBits, TR::Region &region) : _value(false) { }
   int32_t get(int32_t n) { TR_ASSERT(n == 0, "SingleBitContainers only contain one bit\n"); return _value; }
   int32_t get() { return _value; }
   void set() { _value = true; }
   void set(int32_t n) { TR_ASSERT(n == 0, "SingleBitContainers only contain one bit\n"); _value = true; }
   bool isEmpty() { return !_value; }

   bool intersects(TR_SingleBitContainer &other) { return _value && other._value; }
   bool operator==(TR_SingleBitContainer &other) { return _value == other._value; }
   bool operator!=(TR_SingleBitContainer &other) { return !operator==(other); }
   void operator|=(TR_SingleBitContainer &other) { _value = _value || other._value; }
   void operator&=(TR_SingleBitContainer &other) { _value = _value && other._value; }
   void operator-=(TR_SingleBitContainer &other) { if (other._value) { _value = false; } }
   void operator=(TR_SingleBitContainer &other) { _value = other._value; }

   void setAll(int64_t n) { TR_ASSERT(n < 2, "SingleBitContainers only contain one bit\n"); if (n > 0) { _value = true; } }
   void setAll(int64_t m, int64_t n) { if (m == 0 && n == 1) { _value = true; } }

   void resetAll(int64_t n) { TR_ASSERT(n < 2, "SingleBitContainers only contain one bit\n"); if (n > 0) { _value = false; } }
   void resetAll(int64_t m, int64_t n) { if (m == 0 && n == 1) { _value = false; } }

   void empty() { _value = false; }
   bool hasMoreThanOneElement() { return false; }
   int32_t elementCount() { return 1; }
   int32_t numUsedChunks() { return 1; }
   int32_t numNonZeroChunks() { return _value ? 1 : 0; }

   void print(TR::Compilation *comp, TR::FILE *file = NULL);

   private:
   bool _value;
   };

enum TR_BitVectorGrowable
   {
   notGrowable,
   growable
   };

// An optionally growable bit-vector
class TR_BitVector
   {
   public:
   TR_ALLOC(TR_Memory::BitVector)

   typedef TR_BitVectorCursor Cursor;
   typedef int32_t containerCharacteristic; // used by data flow
   static const containerCharacteristic nullContainerCharacteristic = -1;

   // Construct an empty bit vector. All bits are initially off.
   //
   TR_BitVector() : _numChunks(0), _chunks(NULL), _firstChunkWithNonZero(0), _lastChunkWithNonZero(-1), _growable(growable), _region(0) { }
   TR_BitVector(TR::Region &region) : _numChunks(0), _chunks(NULL), _firstChunkWithNonZero(0), _lastChunkWithNonZero(-1), _growable(growable), _region(&region) { }

   // Construct a bit vector with a certain number of bits pre-allocated.
   // All bits are initially off.
   //
   TR_BitVector(int64_t initBits, TR_Memory * m, TR_AllocationKind allocKind = heapAlloc, TR_BitVectorGrowable growableOrNot = growable, TR_MemoryBase::ObjectType ot= TR_MemoryBase::BitVector)
      {
      _chunks = NULL;
      _numChunks = getChunkIndex(initBits-1)+1;
      _firstChunkWithNonZero = _numChunks;
      _lastChunkWithNonZero = -1;
      _region = NULL;
      switch (allocKind)
         {
         case heapAlloc:
            _region = &(m->heapMemoryRegion());
            break;
         case stackAlloc:
            _region = &(m->currentStackRegion());
            break;
         case persistentAlloc:
            _region = NULL;
            break;
         default:
            TR_ASSERT(false, "Unhandled allocation type!");
         }
      #ifdef TRACK_TRBITVECTOR_MEMORY
      _memoryUsed=sizeof(TR_BitVector);
      #endif
      if (_numChunks)
         {
         if (_region)
            {
            _chunks = (chunk_t*)_region->allocate(_numChunks*sizeof(chunk_t));
            }
         else
            {
            TR_ASSERT(allocKind == persistentAlloc, "Should not allocate through trMemory except for persistent memory");
            _chunks = (chunk_t*)TR_Memory::jitPersistentAlloc(_numChunks*sizeof(chunk_t), TR_Memory::BitVector);
            }
         memset(_chunks, 0, _numChunks*sizeof(chunk_t));
         #ifdef TRACK_TRBITVECTOR_MEMORY
         _memoryUsed += _numChunks*sizeof(chunk_t);
         #endif
         }
      _growable = growableOrNot;
      }

   TR_BitVector(int64_t initBits, TR::Region &region, TR_BitVectorGrowable growableOrNot = growable, TR_MemoryBase::ObjectType ot= TR_MemoryBase::BitVector)
      {
      _chunks = NULL;
      _numChunks = getChunkIndex(initBits-1)+1;
      _firstChunkWithNonZero = _numChunks;
      _lastChunkWithNonZero = -1;
      _region = &region;
      #ifdef TRACK_TRBITVECTOR_MEMORY
      _memoryUsed=sizeof(TR_BitVector);
      #endif
      if (_numChunks)
         {
         _chunks = (chunk_t*)_region->allocate(_numChunks*sizeof(chunk_t));
         memset(_chunks, 0, _numChunks*sizeof(chunk_t));
         #ifdef TRACK_TRBITVECTOR_MEMORY
         _memoryUsed += _numChunks*sizeof(chunk_t);
         #endif
         }
      _growable = growableOrNot;
      }

   void setSize(int64_t n)
      {
      int32_t chunkIndex = getChunkIndex(n-1);
      setChunkSize(chunkIndex+1);
      }

   // Construct a bit vector from a second bit vector
   //
   TR_BitVector(const TR_BitVector &v2)
      : _numChunks(0), _firstChunkWithNonZero(0), _lastChunkWithNonZero(-1), _chunks(NULL), _growable(growable)
      {
      _region = v2._region;
      *this = v2;
      _growable = v2._growable;
      }

   // Perform a bitwise assignment from TR_BitContainer to TR_BitVector
   //
   inline TR_BitVector & operator= (TR_BitContainer &bc)
      {
      if (bc._type == singleton)
         {
         empty();
         set(bc._singleBit);
         }
      else
         {
         TR_BitVector &v2 = *(bc._bitVector);
         _region = v2._region;
         *this = v2;
         _growable = v2._growable;
         }
      return *this;
      }

   // Initialize or re-initialize the bit vector to have the given size and
   // to be empty.
   //
   void init(int64_t initBits, TR_Memory * m, TR_AllocationKind allocKind = heapAlloc, TR_BitVectorGrowable growableOrNot = notGrowable)
      {
      if (_chunks && _region == NULL)
         jitPersistentFree(_chunks);
      _region = NULL;
      switch (allocKind)
         {
         case heapAlloc:
            _region = &(m->heapMemoryRegion());
            break;
         case stackAlloc:
            _region = &(m->currentStackRegion());
            break;
         case persistentAlloc:
            _region = NULL;
            break;
         default:
            TR_ASSERT(false, "Unhandled allocation type!");
         }
      
      _growable = growable;
      _chunks = NULL;
      _numChunks = 0;
      _firstChunkWithNonZero = 0;
      _lastChunkWithNonZero = -1;
      setChunkSize(getChunkIndex(initBits-1)+1);
      _growable = growableOrNot;
      }

   void init(int64_t initBits, TR::Region &region, TR_BitVectorGrowable growableOrNot = notGrowable)
      {
      if (_chunks && _region == NULL)
         jitPersistentFree(_chunks);
      _region = &region;
      _growable = growable;
      _chunks = NULL;
      _numChunks = 0;
      _firstChunkWithNonZero = 0;
      _lastChunkWithNonZero = -1;
      setChunkSize(getChunkIndex(initBits-1)+1);
      _growable = growableOrNot;
      }

   // Make sure the vector is large enough to hold the given number of bits.
   //
   void ensureBits(int64_t n)
      {
      int32_t chunkIndex = getChunkIndex(n);
      if (chunkIndex >= _numChunks)
         setChunkSize(chunkIndex+1);
#if BV_SANITY_CHECK
      sanityCheck("ensureBits");
#endif
      }

   // Get the value of the nth bit. The word returned is non-zero if the bit
   // is set and is zero if the bit is not set.
   //
   int32_t get(int64_t n)
      {
      int32_t chunkIndex = getChunkIndex(n);
      if (chunkIndex > _lastChunkWithNonZero)
         return 0;
      return (_chunks[chunkIndex] & getBitMask(n)) != 0;
      }

   bool isSet(int64_t n)
      {
      return get(n) != 0;
      }

   // Set the value of the nth bit.
   //
   void set(int64_t n)
      {
      TR_ASSERT(n >= 0, "assertion failure");
      int32_t chunkIndex = getChunkIndex(n);
      if (chunkIndex >= _numChunks)
         setChunkSize(chunkIndex+1);
      if (chunkIndex < _firstChunkWithNonZero)
         _firstChunkWithNonZero = chunkIndex;
      if (chunkIndex > _lastChunkWithNonZero)
         _lastChunkWithNonZero = chunkIndex;
      _chunks[chunkIndex] |= getBitMask(n);
#if BV_SANITY_CHECK
      sanityCheck("set");
#endif
      }

   void setTo(int64_t n, bool value)
      {
      if (value)
         set(n);
      else
         reset(n);
      }

   int64_t getHighestBitPosition()
      {
      if (_lastChunkWithNonZero < 0)
         return 0;
      int chunkIndex = _lastChunkWithNonZero;
      for (int bitIndex = BITS_IN_CHUNK-1; bitIndex >= 0; bitIndex--)
         if (_chunks[chunkIndex] & getBitMask(bitIndex))
            return getIndexInChunk(bitIndex) + getBitIndex(chunkIndex);
      return 0;
      }

   // Reset the value of the nth bit.
   //
   void reset(int64_t n, bool updateLowHigh = true)
      {
      int32_t chunkIndex = getChunkIndex(n);
      int32_t i;
      if (chunkIndex > _lastChunkWithNonZero || chunkIndex < _firstChunkWithNonZero)
         return;
      if (_chunks[chunkIndex]) {
        _chunks[chunkIndex] &= ~getBitMask(n);
        if (updateLowHigh && _chunks[chunkIndex] == 0)
          resetLowAndHighChunks(_firstChunkWithNonZero, _lastChunkWithNonZero);
      }

#if BV_SANITY_CHECK
      sanityCheck("reset");
#endif
      }

   // like reset except that it tells you if the
   // value had been set
   bool clear(int64_t n)
      {
      bool rc = get(n);
      if (rc)
         reset(n);
#if BV_SANITY_CHECK
      sanityCheck("clear");
#endif
      return rc;
      }

   // Copy a second vector into this one
   //
   void operator= (const TR_BitVector& v2)
      {
      int32_t i;
      int32_t v2Used = v2._numChunks;

      // Grow the this vector if smaller than the 2nd vector
      if (_numChunks < v2Used)
         {
         setChunkSize(v2Used);
         }
      int32_t high = v2._lastChunkWithNonZero;
      if (high < 0)
         {
         // Other vector is empty ... empty this one
         empty();
         }
      else
         {
         // Copy all of the used words from the 2nd vector
         int32_t low = v2._firstChunkWithNonZero;
         for (i = _firstChunkWithNonZero; i < low; i++)
            _chunks[i] = 0;
         for (i = low ; i <= high; i++)
            _chunks[i] = v2._chunks[i];
         for (i = high+1; i <= _lastChunkWithNonZero; i++)
            _chunks[i] = 0;
         _firstChunkWithNonZero = low;
         _lastChunkWithNonZero = high;
         }
#if BV_SANITY_CHECK
      sanityCheck("assign");
#endif
      }

   // Perform a bitwise OR between this vector and a second vector
   // Results are in the this vector
   //
   void operator|= (TR_BitVector& v2)
      {
      if (v2._lastChunkWithNonZero < 0)
         return; // other is empty

      // Grow the this vector if smaller than the 2nd vector
      int32_t v2Used = v2._numChunks;
      if (_numChunks < v2Used)
         setChunkSize(v2Used);

      // OR in all of the words from the 2nd vector
      for (int32_t i = v2._firstChunkWithNonZero; i <= v2._lastChunkWithNonZero; i++)
         _chunks[i] |= v2._chunks[i];
      if (_firstChunkWithNonZero > v2._firstChunkWithNonZero)
         _firstChunkWithNonZero = v2._firstChunkWithNonZero;
      if (_lastChunkWithNonZero < v2._lastChunkWithNonZero)
         _lastChunkWithNonZero = v2._lastChunkWithNonZero;
#if BV_SANITY_CHECK
      sanityCheck("operator|=");
#endif
      }

   void operator|= (TR_BitContainer& v2)
      {
      if (v2._type == singleton)
         set(v2._singleBit);
      else if (v2._bitVector)
         *this |= *v2._bitVector;
      }

   // Perform a bitwise AND between this vector and a second vector
   // Results are in the this vector
   //
   void operator&= (TR_BitVector& v2)
      {
      if (_lastChunkWithNonZero < 0)
         return; // Already empty
      int32_t low = v2._firstChunkWithNonZero;
      int32_t high = v2._lastChunkWithNonZero;
      if (high < _firstChunkWithNonZero || low > _lastChunkWithNonZero)
         {
         // No intersection
         this->empty();
#if BV_SANITY_CHECK
         sanityCheck("operator&=");
#endif
         return;
         }

      // Clear all the chunks before and after those set in the other vector
      int32_t i;
      if (low < _firstChunkWithNonZero)
         low = _firstChunkWithNonZero;
      else
         {
         for (i =_firstChunkWithNonZero; i < low; i++)
            _chunks[i] = 0;
         }
      if (high > _lastChunkWithNonZero)
         high = _lastChunkWithNonZero;
      else
         {
         for (i = _lastChunkWithNonZero; i > high; i--)
            _chunks[i] = 0;
         }

      // AND in all of the words from the 2nd vector
      for (i = low; i <= high; i++)
         _chunks[i] &= v2._chunks[i];

      // Reset first and last chunks with non-zero
      resetLowAndHighChunks(low, high);
#if BV_SANITY_CHECK
      sanityCheck("operator&=");
#endif
      }

   // Determine if any bit is set in both this vector and a second vector.
   //
   bool intersects(TR_BitVector& v2)
      {
      if (_lastChunkWithNonZero < 0)
         return false; // empty - no intersection
      int32_t low = v2._firstChunkWithNonZero;
      int32_t high = v2._lastChunkWithNonZero;
      if (high < _firstChunkWithNonZero || low > _lastChunkWithNonZero)
         return false; // No intersection

      // AND in all of the words from the 2nd vector
      if (low < _firstChunkWithNonZero)
         low = _firstChunkWithNonZero;
      if (high > _lastChunkWithNonZero)
         high = _lastChunkWithNonZero;
      for (int32_t i = low; i<= high; i++)
         if (_chunks[i] & v2._chunks[i])
            return true;
      return false;
      }

   // Perform a bitwise negation (AND-NOT) between this vector and a second vector
   // Results are in the this vector
   //
   void operator-= (TR_BitVector& v2)
      {
      if (_lastChunkWithNonZero < 0)
         return;
      int32_t low = v2._firstChunkWithNonZero;
      int32_t high = v2._lastChunkWithNonZero;
      if (high < _firstChunkWithNonZero || low > _lastChunkWithNonZero)
         return; // No intersection

      // AND in the logical NOT of all of the words from the 2nd vector
      if (low < _firstChunkWithNonZero)
         low = _firstChunkWithNonZero;
      if (high > _lastChunkWithNonZero)
         high = _lastChunkWithNonZero;
      for (int32_t i = low; i<= high; i++)
         _chunks[i] &= ~v2._chunks[i];

      // Reset first and last chunks with non-zero
      resetLowAndHighChunks(_firstChunkWithNonZero, _lastChunkWithNonZero);
#if BV_SANITY_CHECK
      sanityCheck("operator-=");
#endif
      }

   void operator-= (TR_BitContainer& v2)
      {
      if (v2._type == singleton)
         reset(v2._singleBit);
      else if (v2._bitVector)
         *this -= *v2._bitVector;
      }

   // mixed type operations and conversions
   template <class BitVector>
   TR_BitVector & operator= (const BitVector &sparse);
   template <class BitVector>
   TR_BitVector & operator-= (const BitVector &sparse);
   template <class BitVector>
   TR_BitVector & operator&= (const BitVector &sparse);
   template <class BitVector>
   TR_BitVector & operator|= (const BitVector &sparse);

   // Compare this vector with another
   //
   bool operator== (TR_BitVector& v2)
      {
      if (_lastChunkWithNonZero != v2._lastChunkWithNonZero)
         return false;
      if (_lastChunkWithNonZero < 0)
         return true; // Both empty
      // Now both are non-empty
      if (_firstChunkWithNonZero != v2._firstChunkWithNonZero)
         return false;
      if (_lastChunkWithNonZero != v2._lastChunkWithNonZero)
         return false;
      for (int32_t i = _firstChunkWithNonZero; i <= _lastChunkWithNonZero; i++)
         if (_chunks[i] != v2._chunks[i])
            return false;
      return true;
      }

   bool operator!= (TR_BitVector& v2){ return !operator==(v2); }


   // Set the first n elements of the set
   //
   void setAll(int64_t n)
      {
      if (n <= 0)
         return;
      int32_t i;
      int32_t chunkIndex = getChunkIndex(n-1);
      if (chunkIndex >= _numChunks)
         setChunkSize(chunkIndex+1);
      for (i = chunkIndex-1; i >= 0; i--)
         _chunks[i] = (chunk_t)-1;
      for (i = getBitIndex(chunkIndex); i < n; i++)
         _chunks[chunkIndex] |= getBitMask(i);
      _firstChunkWithNonZero = 0;
      if (_lastChunkWithNonZero < chunkIndex)
         _lastChunkWithNonZero = chunkIndex;
#if BV_SANITY_CHECK
      sanityCheck("setAll(n)");
#endif
      }


   // Set elements m to n of the set
   //
   void setAll(int64_t m, int64_t n)
      {
      int32_t firstChunk = getChunkIndex(m);
      int32_t lastChunk  = getChunkIndex(n);
      if (lastChunk >= _numChunks)
         {
         setChunkSize(lastChunk+1);
         }
      if (firstChunk < _firstChunkWithNonZero)
         _firstChunkWithNonZero = firstChunk;
      if (_lastChunkWithNonZero < lastChunk)
         _lastChunkWithNonZero = lastChunk;
      // Special case - we're only modifying part of one chunk
      //
      if (firstChunk == lastChunk)
         {
         for (int32_t i = getIndexInChunk(m); i <= getIndexInChunk(n); i++)
            _chunks[firstChunk] |= getBitMask(i);
#if BV_SANITY_CHECK
         sanityCheck("setAll(m,n)");
#endif
         return;
         }

      if (firstChunk == lastChunk)
         {
         TR_ASSERT(false, "this code is not used");
         _chunks[firstChunk] |= getBitMask(m, n);
         }
      else
         {
         // Set first part-chunk bits
         //
         int32_t i = getIndexInChunk(m);
         if (i > 0)
            {
            for ( ; i < BITS_IN_CHUNK; i++)
               _chunks[firstChunk] |= getBitMask(i);
            }
         else
            {
            _chunks[firstChunk] |= (chunk_t)-1;
            }

         // Set last part-chunk bits
         //
         i = getIndexInChunk(n);
         if (i < (BITS_IN_CHUNK-1))
            {
            for ( ; i >= 0; i--)
               _chunks[lastChunk] |= getBitMask(i);
            }
         else
            {
            _chunks[lastChunk] |= (chunk_t)-1;
            }

         // Set the complete chunks
         //
         for (i = firstChunk+1; i < lastChunk; i++)
            _chunks[i] = (chunk_t)-1;
         }
#if BV_SANITY_CHECK
      sanityCheck("setAll(m,n)");
#endif
      }

   // Reset all elements of the set (i.e. empty the set)
   //
   void empty()
      {
      for (int32_t i = _firstChunkWithNonZero; i <= _lastChunkWithNonZero; i++)
         _chunks[i] = 0;
      _firstChunkWithNonZero = _numChunks;
      _lastChunkWithNonZero = -1;
#if BV_SANITY_CHECK
      sanityCheck("empty");
#endif
      }

   // Reset all elements m to n of the set
   //
   void resetAll(int64_t m, int64_t n)
      {
      int32_t i;
      int32_t firstChunk = getChunkIndex(m);
      int32_t lastChunk  = getChunkIndex(n);
      if (_lastChunkWithNonZero < firstChunk || _firstChunkWithNonZero > lastChunk)
         return; // All relevant bits are already reset
      if (lastChunk >= _numChunks)
         {
         setChunkSize(getChunkIndex(n));
         }

      if (firstChunk == lastChunk)
         {
         _chunks[firstChunk] &= ~getBitMask(m, n);
         }
      else
         {
         // Reset first part-chunk bits
         //
         i = getIndexInChunk(m);
         if (i > 0)
            {
            for ( ; i < BITS_IN_CHUNK; i++)
               _chunks[firstChunk] &= ~getBitMask(i);
            }
         else
            {
            _chunks[firstChunk] = 0;
            }

         // Reset last part-chunk bits
         //
         i = getIndexInChunk(n);
         if (i < (BITS_IN_CHUNK-1))
            {
            for ( ; i >= 0; i--)
               _chunks[lastChunk] &= ~getBitMask(i);
            }
         else
            {
            _chunks[lastChunk] = 0;
            }

         // Set the complete chunks
         //
         for (i = firstChunk+1; i < lastChunk; i++)
            _chunks[i] = 0;
         }
      // Reset first and last chunks with non-zero
      resetLowAndHighChunks(_firstChunkWithNonZero, _lastChunkWithNonZero);
#if BV_SANITY_CHECK
      sanityCheck("resetAll");
#endif
      }

   // Check if the set is empty
   //
   bool isEmpty()
      {
      return _lastChunkWithNonZero < 0;
      }

   bool hasMoreThanOneElement();

   // Find the number of elements in the set
   //
   int32_t elementCount();

   // Find the number of common elements in the sets
   //
   int32_t commonElementCount(TR_BitVector&);

   // Pack the bit vector into the smallest possible representation
   //
   void pack()
      {
      setChunkSize(numUsedChunks());
      }

   // produce a hexadecimal "name" for this bitvector
   const char *getHexString();

   // Print the bit vector to the log file
   //
   void print(TR::Compilation *comp, TR::FILE *file = NULL);

   // Find the number of used chunks
   //
   int32_t numUsedChunks()
      {
      return _lastChunkWithNonZero+1;
      }

   int32_t numNonZeroChunks()
      {
      if (_lastChunkWithNonZero < 0)
         return 0;
      return _lastChunkWithNonZero-_firstChunkWithNonZero+1;
      }

   int32_t numChunks()
      {
      return _numChunks;
      }

   int32_t chunkSize()
      {
      return sizeof(chunk_t);
      }

   #ifdef TRACK_TRBITVECTOR_MEMORY
   uint32_t MemoryUsage()
      {
      return _memoryUsed;
      }
   #endif

   private:

   // Each chunk of bits is an unsigned word, which is 32/64 bits.
   // The low order 5/6 bits of a bit index define the bit position within the
   // chunk and the rest define the chunk index.

   chunk_t             *_chunks;
   TR::Region          *_region;
   #ifdef TRACK_TRBITVECTOR_MEMORY
   uint32_t             _memoryUsed;
   #endif
   int32_t              _numChunks;
   // _firstChunkWithNonZero and _lastChunkWithNonero must be maintained precisely
   int32_t              _firstChunkWithNonZero; // == _numChunks if empty
   int32_t              _lastChunkWithNonZero;  // == -1 if empty
   TR_BitVectorGrowable _growable;

   friend class TR_BitVectorIterator;
   friend class CS2_TR_BitVector;

   // Re-calculate the first and last chunks with non-zero
   void resetLowAndHighChunks(int32_t low, int32_t high)
      {
      for ( ; low <= high &&_chunks[low] == 0; low++)
         ;
      if (low > high)
         {
         // vector is now empty
         _firstChunkWithNonZero = _numChunks;
         _lastChunkWithNonZero = -1;
         return;
         }
      _firstChunkWithNonZero = low;
      // Recalculate last chunk with non-zero
      // Should stop at least at _firstChunkWithNonZero
      for ( ; _chunks[high] == 0; high--)
         ;
      _lastChunkWithNonZero = high;
      }

#if BV_SANITY_CHECK
   void sanityCheck(const char *source)
      {
      // Calculate the correct values for _firstChunkWithNonZero and _lastChunkWithNonZero
      // and check them against the stored values
      int32_t low, high;
      int32_t i;
      if (!_chunks)
         {
         low = 0;
         high = -1;
         }
      else
         {
         for (i = 0; i < _numChunks && _chunks[i] == 0; i++)
            ;
         low = i;
         if (i >= _numChunks)
            high = -1;
         else
            {
            for (i = _numChunks-1; i >= low && _chunks[i] == 0; i--)
               ;
            high = i;
            }
         }
      if (low != _firstChunkWithNonZero || high != _lastChunkWithNonZero)
         {
         printf("TR_BitVector sanity check failed at %s\n", source);
         printf("TR_BitVector Low is %d, should be %d\n", _firstChunkWithNonZero, low);
         printf("TR_BitVector High is %d, should be %d, num chunks = %d\n", _lastChunkWithNonZero, high, _numChunks);
         TR_ASSERT(0, "TR_BitVector sanity check failed");
         }
      }
#endif

   // Given a bit index, calculate the chunk index
   //
   static int32_t getChunkIndex(int64_t bitIndex)
       {
#ifdef DEBUG
       TR_ASSERT((((bitIndex >> SHIFT) & ~((int64_t)0x7fffffff)) == 0) || (bitIndex < 0), "Chunk index out of int32_t range. bitIndex=%lx\n",bitIndex);
#endif
       return bitIndex >> SHIFT;
       }

   // Given a bit index, calculate the bit index within chunk
   //
   static int32_t getIndexInChunk(int64_t bitIndex) {return bitIndex&(BITS_IN_CHUNK-1);}

#if defined(BITVECTOR_BIT_NUMBERING_MSB)
   // Given a bit index, calculate the bit mask within chunk
   //
   static inline chunk_t getBitMask(int32_t bitIndex) { return (chunk_t) 1 << ((BITS_IN_CHUNK-1) - (bitIndex & (BITS_IN_CHUNK-1))); }

   // Given a bit index range, calculate the bit mask within chunk for that range of bits from m to n
   //
   static inline chunk_t getBitMask(int32_t m, int32_t n) { return ((getBitMask(m)-1) << 1) - getBitMask(n) + 2; }

   // Given a mask for a bitIndex, calculate the mask for bitIndex+1
   //
   static inline chunk_t incrementBitMask(chunk_t mask) {return mask >> 1;}
#else
   // Given a bit index, calculate the bit mask within chunk
   //
   static inline chunk_t getBitMask(int32_t bitIndex) {return (chunk_t)1 << (bitIndex&(BITS_IN_CHUNK-1));}

   // Given a bit index range, calculate the bit mask within chunk for that range of bits from m to n
   //
   static inline chunk_t getBitMask(int32_t m, int32_t n) { return ((getBitMask(n)-1) << 1) - getBitMask(m) + 2; }

   // Given a mask for a bitIndex, calculate the mask for bitIndex+1
   //
   static inline chunk_t incrementBitMask(chunk_t mask) {return mask + mask; } // additions are faster than shifts on P4
#endif

   // Given a chunk index, calculate the bit index of the first bit in the chunk
   //
   static int64_t getBitIndex(int32_t chunkIndex) {return ((int64_t)chunkIndex) << SHIFT;}

   // Re-allocate the chunk array so that there are enough chunks to handle the
   // given chunk index
   //
   void setChunkSize(int32_t chunkSize);
   };

class TR_BitVectorIterator
   {
   public:

   // Construct a null iterator. Must be initialized via setBitVector before
   // being used.
   //
   TR_BitVectorIterator()
      {
      _bitVector = NULL;
      }

   TR_BitVectorIterator(TR_BitVector &bv, int32_t startBit = 0)
      {
      setBitVector(bv, startBit);
      }

   // Set up to iterate over a new bit vector
   //
   void setBitVector(TR_BitVector &bv, int32_t startBit = 0)
      {
#if BV_SANITY_CHECK
      bv.sanityCheck("iterator");
#endif
      _bitVector = &bv;
      _startBit = startBit;
      reset();
      }

   int hasMoreElements()
      {
      return (_bitVector->getChunkIndex(_curIndex) < _bitVector->_numChunks);
      }

   int32_t getFirstElement()
      {
      reset();
      return getNextElement();
      }

   void reset()
      {
      _curIndex = _startBit-1; // getNextBit will re-increment to the first bit
      getNextBit();
      }

   int32_t getNextElement()
      {
      int32_t i = _curIndex;
      getNextBit();
      return i;
      }

   protected:

   void getNextBit()
      {
      _curIndex++;
      int32_t curChunk = TR_BitVector::getChunkIndex(_curIndex);
      if (curChunk > _bitVector->_lastChunkWithNonZero)
         {
         // No more chunks with non-zero bits
         _curIndex = _bitVector->_numChunks << SHIFT;
         return;
         }

      chunk_t tmpChunk = _bitVector->_chunks[curChunk];

      // If all bits in the chunk are set, the current bit must be set
      if (tmpChunk == ~(chunk_t) 0)
         return;

      chunk_t  mask = TR_BitVector::getBitMask(_curIndex);
      // zero the trailing bits from the chunk
      //tmpChunk &= ~(mask - 1);
      tmpChunk &= TR_BitVector::getBitMask(_curIndex, BITS_IN_CHUNK-1);
      if (!tmpChunk)
         {
         // current chunk is empty so jump over it and any other empty chunks
         if (curChunk >= _bitVector->_lastChunkWithNonZero)
            {
            // No more chunks with non-zero bits
            _curIndex = _bitVector->_numChunks << SHIFT;
            return;
            }
         // curChunk is less than the last chunk with non-zero bits, so the do loop will terminate
         do {
            curChunk++;
         } while(! _bitVector->_chunks[curChunk]);
         tmpChunk = _bitVector->_chunks[curChunk];
         _curIndex = curChunk << SHIFT;
         mask = TR_BitVector::getBitMask(0);
         }
      // here we are guaranteed to have a chunk with at least one bit set

      //_curIndex = (_curIndex & ~(TR_BitVector::_numBitsChunk-1)) + TrailingZero(tmpChunk);
      while (!(tmpChunk & mask)) // sequential check
         {
         _curIndex++;
         mask = TR_BitVector::incrementBitMask(mask);
         }
      }

   TR_BitVector *_bitVector;
   int32_t       _curIndex;
   int32_t       _startBit;
   };

class TR_BitVectorCursor : public TR_BitVectorIterator
   {
   // CS2-like iterator
public:
   TR_BitVectorCursor(TR_BitVector &bv) : TR_BitVectorIterator(bv)
      {
      SetToFirstOne();
      }

   // CS2-like interface
   bool Valid() { return valid; }
   operator uint32_t() { return value;}
   bool SetToFirstOne() {
     reset();
     SetToNextOne();
     return valid;
   }
   bool SetToNextOne() {
     if (hasMoreElements()) {
       valid = true;
       value = getNextElement();
     } else
       valid = false;
     return valid;
   }
private:
   uint32_t value;
   bool valid;
   };

class TR_BitContainerIterator : public TR_BitVectorIterator
   {
   public:

   // Construct a null iterator. Must be initialized via setBitContainer before
   // being used.
   //
   TR_BitContainerIterator()
      {
      _type = bitvector;
      _bitVector = NULL;
      }

   TR_BitContainerIterator(TR_BitContainer &bc)
      {
      setBitContainer(bc);
      }

   // Set up to iterate over a new bit container
   //
   void setBitContainer(TR_BitContainer &bc)
      {
      _type = bc._type;
      if (_type == bitvector && bc._bitVector)
         setBitVector(*bc._bitVector, 0);
      else if (_type == bitvector)
         _bitVector = NULL;
      else if (_type == singleton)
         {
         _singleBit = bc._singleBit;
         _curIndex = -1;
         }
      }

   int hasMoreElements()
      {
      if (_type == bitvector)
         return _bitVector ? TR_BitVectorIterator::hasMoreElements() : false;
      else
         return (_curIndex == -1);
      }

   int32_t getFirstElement()
      {
      if (_type == bitvector)
         return _bitVector ? TR_BitVectorIterator::getFirstElement() : -1;
      else
         return (_curIndex = _singleBit);
      }

   int32_t getNextElement()
      {
      if (_type == bitvector)
         return _bitVector ? TR_BitVectorIterator::getNextElement() : -1;
      else
         return (_curIndex  = _singleBit);
      }

   private:

   int32_t             _singleBit;
   TR_BitContainerType _type;
   };


class TR_BitMatrix
   {
   public:
   TR_ALLOC(TR_Memory::BitVector)

   TR_BitMatrix(int32_t rows, int32_t bitsPerRow, TR_Memory * m, TR_AllocationKind allocKind = heapAlloc)
      : _rows(rows), _bitsPerRow(bitsPerRow), _vector(rows*bitsPerRow, m, allocKind, notGrowable) {}

   int32_t get(int32_t row, int32_t bitIndex)
      {
      TR_ASSERT(row < _rows && bitIndex < _bitsPerRow, "assertion failure");
      return _vector.get(bitIndex + row * _bitsPerRow);
      }

   bool isSet(int32_t row, int32_t bitIndex)
      {
      return get(row, bitIndex) != 0;
      }

   void set(int32_t row, int32_t bitIndex)
      {
      TR_ASSERT(row < _rows && bitIndex < _bitsPerRow, "assertion failure");
      _vector.set(bitIndex + row * _bitsPerRow);
      }

   void reset(int32_t row, int32_t bitIndex)
      {
      TR_ASSERT(row < _rows && bitIndex < _bitsPerRow, "assertion failure");
      _vector.reset(bitIndex + row * _bitsPerRow);
      }

   private:
   int32_t      _rows;
   int32_t      _bitsPerRow;
   TR_BitVector _vector;
   };

// Perform a bitwise assignment from generic BitVector to TR_BitVector
//
template <class BitVector>
inline TR_BitVector & TR_BitVector::operator= (const BitVector &sparse)
   {
   class BitVector::Cursor bi(sparse);
   empty();
   for (bi.SetToFirstOne(); bi.Valid(); bi.SetToNextOne())
      set(bi);
#if BV_SANITY_CHECK
   sanityCheck("operator=(sparse)");
#endif
   return *this;
   }

class CS2_TR_BitVector {
public:
  CS2_TR_BitVector(TR_BitVector &_bv) : bv(_bv) {}

  static bool hasFastRandomLookup() { return true;}
  uint32_t ValueAt(uint32_t index) const { return bv.get(index); }
  uint32_t LastOne() const { return bv.getHighestBitPosition(); }
  bool hasBitWordRepresentation() const { return true; } // returns whether this bit vector is based on an array of bitWords (ie chunks of bits)
  uint32_t wordSize() const { return BITS_IN_CHUNK; } // returns size of words in bits (ie size of a chunk)
  int32_t FirstOneWordIndex() const { return bv._firstChunkWithNonZero; }
  int32_t LastOneWordIndex() const { return bv._lastChunkWithNonZero; }
  chunk_t WordAt (uint32_t wordIndex) const {
     if (wordIndex >= bv._numChunks)
        return 0;
     return bv._chunks[wordIndex];
  }

  bool IsZero() const { return bv.isEmpty(); }

  struct Cursor {
    Cursor(const CS2_TR_BitVector &vector): bvi(vector.bv) {}
    bool Valid() { return valid; }
    operator uint32_t() { return value;}
    bool SetToFirstOne() {
      bvi.reset();
      SetToNextOne();
      return valid;
    }
    bool SetToNextOne() {
      if (bvi.hasMoreElements()) {
        valid = true;
        value = bvi.getNextElement();
      } else
        valid = false;
      return valid;
    }
    bool SetToNextOneAfter(uint32_t v) {
      while (Valid() && (*this) <=v)
        SetToNextOne();
      return Valid();
    }
  private:
    TR_BitVectorIterator bvi;
    uint32_t value;
    bool valid;
  };
private:
  TR_BitVector &bv;
};

// Assign a generic BitVector from a TR_BitVector
//
template <class BitVector>
inline void Assign(BitVector &cs2bv, TR_BitVector &trbv, bool clear=true) {
  uint32_t count=0;

  if (clear)
     cs2bv = CS2_TR_BitVector(trbv);
  else
    cs2bv.Or(CS2_TR_BitVector(trbv));
}

inline bool TR_BitContainer::isEmpty()
   {
   if (_type == bitvector)
      return (_bitVector != NULL) ? _bitVector->isEmpty() : true;
   else if (_type == singleton)
      return false;
   else
      return true;
   }

// Assign a generic BitVector from a TR_BitContainer
//
template <class BitVector>
inline void Assign(BitVector &cs2bv, TR_BitContainer &trbc, bool clear=true)
   {
   TR_BitVector *trbv = trbc.getBitVector();
   if (clear) cs2bv.Clear();
   if (trbc.isEmpty())
      return;

   if (trbv == NULL)
      {
      // so its a singleton, get first and only element
      TR_BitContainerIterator bci(trbc);
      int32_t singleton = bci.getFirstElement();
      cs2bv[singleton] = true;
      }
   else
      {
      // so its a bit vector, for now just copies the elements of the TR_BitVector into cs2bv
      //cs2bv = *trbv;
      Assign(cs2bv, *trbv, clear);
      }
   }

class Operator
{
public:

template <class T>
static void operate(T &inplace, const T &operand);
};

class SetDiffOperator
{
public:

template <class T>
static void operate(T &inplace, const T &operand)
   {
   inplace &= ~operand;
   }
};

class AndOperator
{
public:

template <class T>
static void operate(T &inplace, const T &operand)
   {
   inplace &= operand;
   }
};

class OrOperator
{
public:

template <class T>
static void operate(T &inplace, const T &operand)
   {
   inplace |= operand;
   }
};


// Perform inplace bitwise negation (AND-NOT) between this vector and a second vector.
//
template <class BitVector>
inline TR_BitVector & TR_BitVector::operator-= (const BitVector &sparse)
   {
     if (isEmpty() || sparse.IsZero()) return *this;

     uint32_t highBit = getHighestBitPosition();
     typename BitVector::Cursor bi(sparse);
     for (bi.SetToFirstOne(); bi.Valid() && bi<= highBit; bi.SetToNextOne()) {
       reset(bi, false);
     }
     resetLowAndHighChunks(_firstChunkWithNonZero, _lastChunkWithNonZero);
#if BV_SANITY_CHECK
     sanityCheck("operator-=(sparse)");
#endif
     return *this;
   }

// Perform inplace bitwise AND between this vector and a second vector.
//
template <class BitVector>
inline TR_BitVector & TR_BitVector::operator&= (const BitVector &sparse)
   {
     if (isEmpty()) return *this;
     if (sparse.IsZero()) { empty(); return *this;}

     // AND the common chunks
     uint32_t low = _firstChunkWithNonZero;
     uint32_t high = _lastChunkWithNonZero;

     typename BitVector::Cursor bi(sparse);
     bi.SetToFirstOne();

     uint32_t chunk;
     for (chunk=low; chunk <= high; chunk++) {
       while (bi.Valid() && chunk > getChunkIndex(bi))
         bi.SetToNextOne();
       if (!bi.Valid()) break;

       chunk_t c = 0;

       while (bi.Valid() && chunk == getChunkIndex(bi)) {
         c |= getBitMask(bi);
         bi.SetToNextOne();
       }
       _chunks[chunk] &= c;
     }

     // Clear high-order chunks if larger than the 2nd vector, and
     // ignore 2nd vector's high-order chunks if 2nd vector is larger than this.
     for (; chunk < high; chunk++)
       _chunks[chunk] = 0;

     // Re-adjust first and last chunks with non-zero
     resetLowAndHighChunks(low, high);
#if BV_SANITY_CHECK
     sanityCheck("operator&=(sparse)");
#endif
     return *this;
   }

// Perform inplace bitwise OR between this vector and a second vector.
//
template <class BitVector>
inline TR_BitVector & TR_BitVector::operator|= (const BitVector &sparse)
   {
     if (sparse.IsZero()) return *this;
     if (isEmpty()) { *this = sparse; return *this;}

     uint32_t first = sparse.FirstOne();
     uint32_t last  = sparse.LastOne();

     ensureBits(last);
     typename BitVector::Cursor bi(sparse);
     for (bi.SetToFirstOne(); bi.Valid(); bi.SetToNextOne()) {
       _chunks[getChunkIndex(bi)] |= getBitMask(bi);
     }

     if (_firstChunkWithNonZero > getChunkIndex(first))
       _firstChunkWithNonZero = getChunkIndex(first);

     if (_lastChunkWithNonZero < getChunkIndex(last))
       _lastChunkWithNonZero = getChunkIndex(last);

#if BV_SANITY_CHECK
     sanityCheck("operator|=(sparse)");
#endif
     return *this;
   }

#endif
