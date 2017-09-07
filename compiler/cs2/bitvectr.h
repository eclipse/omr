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

/***************************************************************************/
/*                                                                         */
/*  File name:  bitvectr.h                                                 */
/*  Purpose:    Definition of the BitVector abstract base class.          */
/*                                                                         */
/***************************************************************************/

#ifndef CS2_BITVECTR_H
#define CS2_BITVECTR_H

#include "cs2/cs2.h"
#include "cs2/bitmanip.h"
#include "cs2/allocator.h"

#ifdef CS2_ALLOCINFO
#define allocate(x) allocate(x, __FILE__, __LINE__)
#define deallocate(x,y) deallocate(x, y, __FILE__, __LINE__)
#define reallocate(x,y,z) reallocate(x, y, z, __FILE__, __LINE__)
#endif

namespace CS2 {

//  BitVector
//
//  This class acts just like an array of bits.  You can
//  use the [] operator to address the bits in the array.
//
//  The bit vector is initially zero.

typedef uint32_t BitIndex;
typedef uint32_t ShortWord;

#ifdef BITVECTOR_64BIT
typedef uint64_t BitWord;
const uint64_t kHighBit       = 0x8000000000000000ull;
const uint64_t kFullMask      = 0xFFFFFFFFFFFFFFFFull;
const uint64_t kZeroBits      = 0x0000000000000000ull;
#else /* ! BITVECTOR_64BIT */
typedef uint32_t BitWord;
const uint32_t kHighBit       = 0x80000000ul;
const uint32_t kFullMask      = 0xFFFFFFFFul;
const uint32_t kZeroBits      = 0x00000000ul;
#endif /* BITVECTOR_64BIT */

const uint32_t kBitWordSize   = 8 * sizeof(BitWord);
const uint32_t kShortWordSize = 8 * sizeof(ShortWord);

template <class Allocator>
class ABitVector : private Allocator {
private:
  class BitRef;
public:

  explicit ABitVector(const Allocator &a = Allocator()) :
    Allocator(a), fNumBits(0), fBitWords(NULL) {
  }

  ~ABitVector() {
    ClearToNull();
  }

  // Copy constructor and assignment operator
  // These just copy the bits, no memory allocation is done
  ABitVector (const ABitVector &adoptVector);
  ABitVector (const ABitVector &adoptVector, const Allocator &a);
  ABitVector & operator= (const ABitVector &adoptVector);

  Allocator& allocator() { return *this;}

  // Return the bit (0 or 1) at the given index
  const BitRef operator[] (BitIndex) const;

  BitRef operator[] (BitIndex bitIndex)  {
    return BitRef(*this, bitIndex);
  }

  static bool hasFastRandomLookup() {
    return true;
  }

  // Return the value of the bit (0 or 1) at the given index
  bool ValueAt (BitIndex) const;
  BitWord WordValueAt (BitIndex, BitIndex) const;

  void GrowTo (BitIndex index, bool geometric = true, bool forceGeometric = false);
  void GrowTo (const ABitVector &vector);

  // Check if bit vectors are equal
  bool operator== (const ABitVector &) const;
  bool operator!= (const ABitVector &) const;

  //Bitwise operations
  ABitVector & operator|= (const ABitVector &);
  ABitVector & operator&= (const ABitVector &);
  ABitVector & operator-= (const ABitVector &);

  //Bitwise operations
  template <class B2>
  ABitVector & operator|= (const B2 &);
  template <class B2>
  ABitVector & operator&= (const B2 &);
  template <class B2>
  ABitVector & operator-= (const B2 &);


  // Clear all bits in the vector (ie. clear and discard memory), and make empty.
  void Clear ();

  // Clear all bits in the vector (ie. clear and discard memory), and make null.
  void ClearToNull ();

  // deprecated. use Clear.
  void Truncate() { Clear();}

  // Set first n bits in the vector
  void SetAll (BitIndex numBits);

  // Return the index of the first zero bit in the vector
  BitIndex LowestZero() const;

  // Return the index of the first one bit in the vector
  BitIndex FirstOne() const;
  BitIndex LastOne() const;
  int32_t FirstOneWordIndex() const;
  int32_t LastOneWordIndex() const;
  // if there is a bit in the specified range, clear the highest one and set
  // foundone to true.  otherwise, set foundone to false
  BitIndex ClearLastOneIfThereIsOneInRange(BitIndex low, BitIndex high, bool& foundone);

  // Determine if the vector is cleared, ie is the empty set.
  bool IsZero() const;

  // Determine if the vector is cleared to null, ie is the zero measure set "null set" which is also the empty set.
  bool IsNull() const;

  // Determine if the intersection of this vector and the input vector
  // is non-zero
  bool Intersects (const ABitVector &inputVector) const;
  template <class B2>
  bool Intersects (const B2 &inputVector) const;

  // Determine if this vector is a subset of the inputVector
  bool  operator <= (const ABitVector &inputVector) const;

  // Return the size in words (as defined by the type BitWord) of a vector
  // of the given bit length.
  uint32_t SizeInWords () const;
  static uint32_t SizeInWords (BitIndex numBits);

  // Return the size in 4-byte words (as defined by the type
  // ShortWord) of a vector of the given bit length. Only to be used
  // by the CopyToMemory() CopyFromMemory routines to maintain bitwise
  // compatibility between 32 and 64-bit implementations
  static uint32_t SizeInShortWords (BitIndex numBits);

  // Return the size in bytes of a vector of the given bit length.
  static uint32_t SizeInBytes (BitIndex numBits);

  // Return the number of bytes of memory used by this bit vector
  unsigned long MemoryUsage() const;

  // Return the number of '1' bits in the vector.  Optionally specify the
  // number of leading bits to examine.
  uint32_t PopulationCount (uint32_t numBits = 0xEFFFFFFFul) const;
  uint32_t PopulationCount (const ABitVector &mask) const;

  // Copy the vector to the given memory area.
  // If numBits if greater than the size of the vector, then pad with
  // zeroes.
  void CopyToMemory (void *ptr, BitIndex numBits = 0) const;

  // Copy the vector from the given memory area.
  // If numBits is greater than the size of the vector, then copy only
  // up to the size of the vector.
  void CopyFromMemory (void *ptr, BitIndex numBits = 0);

  // Bitwise binary logical operations (two operand and three operand formats)
  // Return value indicates if the target vector was changed.
  bool And     (const ABitVector &inputVector);
  bool And     (const ABitVector &inputVector,
                ABitVector &outputVector) const;
  bool Andc    (const ABitVector &inputVector);
  bool Andc    (const ABitVector &inputVector,
                ABitVector &outputVector) const;
  bool Or      (const ABitVector &inputVector);
  bool Or      (const ABitVector &inputVector,
                ABitVector &outputVector) const;
  bool Xor     (const ABitVector &inputVector);
  bool Xor     (const ABitVector &inputVector,
                ABitVector &outputVector) const;

  template <class B2>  void Or(const B2 &v);
  template <class B2>  void Andc(const B2 &v);
  template <class B2>  void And(const B2 &v);
  template <class B2>  ABitVector &operator= (const B2 &v);

  template <class B2>
  void Or(B2** vs, uint32_t nvs);

  static inline uint32_t Minimum(uint32_t v1, uint32_t v2){
      if (v1<v2) return v1; return v2;
  }
  static inline uint32_t Maximum(uint32_t v1, uint32_t v2){
      if (v1>v2) return v1; return v2;
  }
  class Cursor;
  friend class ABitVector::Cursor;

  class Cursor {
  public:
    Cursor (const ABitVector &adoptVector) : fVector(adoptVector),
                                             fWord(0),
                                             fIndex(0),
                                             fWordCount(fVector.SizeInWords())
    {}

    Cursor (const Cursor &adoptCursor) : fVector(adoptCursor.fVector),
                                         fWord(adoptCursor.fVector),
                                         fIndex(adoptCursor.fIndex),
                                         fWordCount(adoptCursor.fWordCount)
    {}

    bool SetToFirstOne() {
      return SetToNextOneAfter(0);
    }

    bool SetToNextOne() {
      fWord<<=1;
      fIndex+=1;
      BitWord word = fWord;
      if (fWord==0) {
        BitWord wordIndex;
        for (wordIndex=(fIndex+kBitWordSize-1)/kBitWordSize;
             wordIndex < fWordCount;
             wordIndex+=1) {
          word = fVector.WordAt(wordIndex);
          if (word) {
            fIndex = wordIndex * kBitWordSize;
            goto find_bit;
          }
        }
        fIndex = wordIndex * kBitWordSize;
        return false;
      }
    find_bit:
      uint32_t count = BitManipulator::LeadingZeroes(word);
      fWord = word << count;
      fIndex += count;
      return true;
    }

    bool Valid() const {
      return fIndex<fWordCount*kBitWordSize;
    }

    bool SetToNextOneAfter(uint32_t v) {
      fWordCount = fVector.SizeInWords();
      fIndex=v;
      if (!Valid()) {
        fIndex=fWordCount * kBitWordSize;
        return false;
      }
      fWord=fVector.WordAt(v/kBitWordSize);
      fWord <<= (v%kBitWordSize);
      if (fWord&kHighBit) return true;
      return SetToNextOne();
    }

    operator BitIndex() const {
      return fIndex;
    }

    bool SetToFirstZero() {
      CS2Assert (false, ("NOT IMPLEMENTED"));
      return false;
    }

    bool SetToNextZero() {
      CS2Assert (false, ("NOT IMPLEMENTED"));
      return false;
    }
    protected:

    const ABitVector &fVector;
    BitWord  fWord;
    BitIndex fIndex;
    uint32_t fWordCount;

    Cursor &operator= (const Cursor &adoptCursor);
  };

  // STL style iteration - bare-bones, input iteration at this point
  // TODO: reverse iterator?
  // TODO: const iterator?
  //
  class iterator : private Cursor {
    public:
    iterator(const ABitVector &adoptVector, BitIndex index = 0) :
      Cursor(adoptVector) {
      Cursor::SetToNextOneAfter(index);
    }

    // Cast to index operator - returns the current index being pointed to
    operator BitIndex() const { return Cursor::fIndex; }

    // Dereference operator - identical to cast to index operator - note it returns the index, not a reference to it.
    BitIndex operator*() const { return Cursor::fIndex; }

    // Member access operator - not applicable to bit vectors, there is no element only an index
    // BitIndex *operator->() const { return & (this->operator*()); }

    // pre-increment
    iterator& operator++()    { Cursor::SetToNextOne(); return *this; }

    // TODO: post-increment

    // comparison
    bool operator==(const iterator &other) const { return this == &other ||
                                                   (Cursor::fVector == other.fVector &&
                                                    Cursor::fIndex == other.fIndex); }
    bool operator!=(const iterator &other) const { return ! operator==(other); }
  };

  // returns the iterator to the first element
  iterator begin() { return iterator(*this); }

  // returns the iterator to the one-past-the-last element
  iterator end() { return iterator(*this, LastOne()+1); }

  template <class ostr>
  friend ostr &operator<< (ostr &out, const ABitVector &vector) {
    typename ABitVector<Allocator>::Cursor vectorCursor (vector);
    uint32_t i;

    out << "( ";
    for (vectorCursor.SetToFirstOne(), i = 0;
         vectorCursor.Valid();
         vectorCursor.SetToNextOne(), ++i) {
      out << (int) vectorCursor << " ";
    }
    out << ")";

    return out;
  }

  private:

  // Return the size in bits of the vector
  uint32_t SizeInBits() const;

  BitWord & WordAt (uint32_t wordIndex) const;

  uint32_t   fNumBits;
  BitWord *fBitWords;

  class BitRef {
  public:

    BitRef (const ABitVector &vector, BitIndex index):
      fIndex(index), fVector(vector) {}

    BitRef (ABitVector &vector, BitIndex index):
      fIndex(index), fVector(vector) {}

    BitRef (const BitRef &b) :
      fIndex(b.fIndex), fVector(b.fVector)
      { }

    operator bool() const {
      BitIndex wordIndex;
      BitIndex bitIndex;
      bool bitValue;

      // Bits beyond the allocated size are assumed to be zero.
           if (fIndex >= fVector.SizeInBits()) return false;

      wordIndex = fIndex / kBitWordSize;
      bitIndex = fIndex % kBitWordSize;

      bitValue = (fVector.WordAt(wordIndex) << bitIndex) >> (kBitWordSize - 1);
      return bitValue;
    }

    BitRef& operator= (bool bitValue) {
      uint32_t wordIndex, bitIndex, shiftAmount;
      BitWord bitMask, resultWord;


      CS2Assert (bitValue == 0 || bitValue == 1, ("Incorrect bool value"));

      // Do not grow to set a 0
      if (bitValue == 0 && fVector.SizeInBits() < fIndex)
        return *this;

      fVector.GrowTo(fIndex+1);
      CS2Assert (fIndex < fVector.SizeInBits(), ("Bit vector index out of range"));

      wordIndex = fIndex / kBitWordSize;
      bitIndex = fIndex % kBitWordSize;
      shiftAmount = kBitWordSize - bitIndex - 1;
      bitMask = ((BitWord)bitValue) << shiftAmount;

      resultWord = fVector.WordAt(wordIndex);
      resultWord &= ~(((BitWord)1) << shiftAmount);
      resultWord |= bitMask;
      fVector.WordAt(wordIndex) = resultWord;

      return *this;
    }

    BitRef& operator= (const BitRef& value) {
      return *this = (bool) value;
    }

  private:

    BitIndex          fIndex;
    ABitVector        &fVector;
  };

  template <class A2>
    friend
    inline void Swap(ABitVector<A2> &vectorA, ABitVector<A2> &vectorB);
};

template <class Allocator>
  inline uint32_t ABitVector<Allocator>::SizeInBits() const {
  return fNumBits;
}

template <class Allocator>
inline BitWord & ABitVector<Allocator>::WordAt (uint32_t wordIndex) const {
  CS2Assert (wordIndex < SizeInWords(fNumBits),
          ("Accessing ABitVector word %d, over current size %d\n",
           wordIndex, SizeInWords(fNumBits)));
  return fBitWords[wordIndex];
}

template <class Allocator>
inline ABitVector<Allocator>::ABitVector (const ABitVector<Allocator> &adoptVector) : Allocator(adoptVector), fNumBits(0), fBitWords(NULL) {
  *this = adoptVector;
}

template <class Allocator>
inline ABitVector<Allocator>::ABitVector (const ABitVector<Allocator> &adoptVector, const Allocator &a) : Allocator(a), fNumBits(0), fBitWords(NULL) {
  *this = adoptVector;
}

template <class Allocator>
inline bool ABitVector<Allocator>::ValueAt (BitIndex bitIndex) const {
  BitIndex wordIndex;

  if (bitIndex>= SizeInBits()) return false;

  wordIndex = bitIndex / kBitWordSize;
  bitIndex = bitIndex % kBitWordSize;

  return (WordAt(wordIndex) << bitIndex) >> (kBitWordSize - 1);
}

template <class Allocator>
inline BitWord ABitVector<Allocator>::WordValueAt (BitIndex bitIndex, BitIndex wordIndex) const {
  if (bitIndex>= SizeInBits()) return BitWord(0);

  return WordAt(wordIndex);
}

// Grow the vector to at least 'newBitSize' bits.  By default, grow the
// vector additional extents to guarantee geometric growth.
// Force geometric growth in special cases
// This routine also makes the vector non-null.
template <class Allocator>
inline
void ABitVector<Allocator>::GrowTo (BitIndex newBitSize, bool geometric, bool forceGeometric) {
  uint32_t newWordSize, oldBitSize;

  if (newBitSize <= fNumBits) {
    if (fNumBits == 0) {
      // make non-null
      Clear();
    }
    return;
  }

  if (geometric &&
      (forceGeometric || newBitSize<1024)) {
    uint32_t power2Size=1;

    while (power2Size<newBitSize) power2Size<<=1;
    newBitSize = power2Size;
  } else {
    // bump newBitSize to next multiple of chunk
    const uint32_t chunk = 128*8;
    newBitSize = (newBitSize + chunk) - (newBitSize % chunk);
  }

  oldBitSize = fNumBits;

  newWordSize = SizeInWords(newBitSize);
  uint32_t numBits = newWordSize * kBitWordSize;

  if (oldBitSize == 0) {
    fBitWords = (BitWord *) Allocator::allocate(SizeInBytes(numBits));
    memset(fBitWords,0, SizeInBytes(numBits));
  } else {
    fBitWords = (BitWord *) Allocator::reallocate(SizeInBytes(numBits),
                                                  fBitWords,
                                                  SizeInBytes(oldBitSize));
    memset(((char *)fBitWords)+SizeInBytes(oldBitSize),0,
           SizeInBytes(numBits) -
           SizeInBytes(oldBitSize));
  }
  fNumBits = numBits;
}

template <class Allocator>
inline
void ABitVector<Allocator>::GrowTo (const ABitVector &vector) {
  GrowTo(vector.fNumBits, false, false);
}

template <class Allocator>
inline bool ABitVector<Allocator>::operator!= (const ABitVector &v) const {
  return !operator==(v);
}

template <class Allocator>
inline uint32_t ABitVector<Allocator>::SizeInWords (BitIndex numBits) {
#ifdef DEV_VERSION
  CS2Assert (numBits < 0xFFFFFFFFul - kBitWordSize, ("Illegal vector length"));
#endif
  return (numBits + kBitWordSize - 1) / kBitWordSize;
}

template <class Allocator>
inline uint32_t ABitVector<Allocator>::SizeInWords () const {
  return SizeInWords(fNumBits);
}

template <class Allocator>
inline uint32_t ABitVector<Allocator>::SizeInShortWords (BitIndex numBits) {
#ifdef DEV_VERSION
  CS2Assert (numBits < 0xFFFFFFFFul - kBitWordSize, ("Illegal vector length"));
#endif
  return (numBits + kShortWordSize - 1) / kShortWordSize;
}

template <class Allocator>
inline uint32_t ABitVector<Allocator>::SizeInBytes (BitIndex numBits) {
  return ABitVector<Allocator>::SizeInWords(numBits) * sizeof(BitWord);
}

// ABitVector<Allocator>::And (const ABitVector &)
//
// Compute the bitwise AND of this vector and the input vector.
// Place the result in this vector.

template <class Allocator>
inline bool ABitVector<Allocator>::And (const ABitVector &inputVector) {
  return And (inputVector, *this);
}

// ABitVector<Allocator>::Andc (const ABitVector &)
//
// Compute the bitwise AND with COMPLEMENT of this vector and the input vector.
// Place the result in this vector.

template <class Allocator>
inline bool ABitVector<Allocator>::Andc (const ABitVector &inputVector) {
  return Andc (inputVector, *this);
}

// ABitVector<Allocator>::Or (const ABitVector &)
//
// Compute the bitwise OR of this vector and the input vector.
// Place the result in this vector.

template <class Allocator>
inline bool ABitVector<Allocator>::Or (const ABitVector &inputVector) {
  GrowTo(inputVector);
  return Or (inputVector, *this);
}

template <class Allocator>
template <class B2>
  inline void ABitVector<Allocator>::Or (const B2 &v) {

  if (v.IsZero()) return;

  GrowTo(v.LastOne()+1, false, false);
  if (!v.hasBitWordRepresentation() || (v.wordSize() != kBitWordSize)) {
    // have to use Cursor to do the OR
    typename B2::Cursor vc(v);
    vc.SetToFirstOne();
    uint32_t wordIndex = vc / kBitWordSize;
    BitWord word = WordAt(wordIndex);

    uint32_t bitIndex =  vc % kBitWordSize;
    word |= (kHighBit >> bitIndex);

    for (vc.SetToNextOne();vc.Valid();vc.SetToNextOne()) {
      if (vc/kBitWordSize != wordIndex) {
        WordAt(wordIndex) = word;

        wordIndex = vc/kBitWordSize;
        word = WordAt(wordIndex);
      }
      bitIndex =  vc % kBitWordSize;
      word |= (kHighBit >> bitIndex);
    }
    WordAt(wordIndex) = word;
    return;
  } else /* (hasBitWordRepresentation() && (v.wordSize() == kBitWordSize)) */ {
    // can use more efficient implementation by ORed corresponding words from the two bit vectors together
    // as their underlying bit representations are equivalent
    for (int32_t i = v.FirstOneWordIndex(); i <= v.LastOneWordIndex(); i++)
      WordAt(i) |= v.WordAt(i);
    return;
  }
}

template <class Allocator>
template <class B2>
  inline void ABitVector<Allocator>::And(const B2 &v) {

  if (IsZero()) {return;}

  if (v.IsZero()) {Clear(); return;}

  if (!v.hasBitWordRepresentation() || (v.wordSize() != kBitWordSize)) {
    uint32_t lastWordIndex = SizeInWords();

    typename B2::Cursor vc(v);
    vc.SetToFirstOne();
    uint32_t wordIndex = vc / kBitWordSize;

    uint32_t i, lastWord=0;
    BitWord word = 0;
    uint32_t bitIndex =  vc % kBitWordSize;
    word |= (kHighBit >> bitIndex);

    for (vc.SetToNextOne();vc.Valid();vc.SetToNextOne()) {
      if (vc/kBitWordSize != wordIndex) {
        if (wordIndex < lastWordIndex) {
          for (i=lastWord; i<wordIndex; i++)
            WordAt(i)=0;
          WordAt(wordIndex) &= word;
          lastWord = wordIndex + 1;
          wordIndex = vc/kBitWordSize;
          word = 0;
        } else break;
      }
      bitIndex =  vc % kBitWordSize;
      word |= (kHighBit >> bitIndex);
    }
    if (wordIndex < lastWordIndex) {
      for (i=lastWord; i<wordIndex; i++) {
        WordAt(i)=0;
      }
      WordAt(wordIndex) &= word;
      lastWord = wordIndex+1;
    }
    for (i=lastWord; i<lastWordIndex; i++)
      WordAt(i)=0;
  } else /* (hasBitWordRepresentation() && (v.wordSize() == kBitWordSize)) */ {
    // can use more efficient implementation by ANDed corresponding words from the two bit vectors together
    // as their underlying bit representations are equivalent
    int32_t low = v.FirstOneWordIndex();
    int32_t high = v.LastOneWordIndex();
    int32_t thisLow = FirstOneWordIndex();
    int32_t thisHigh = LastOneWordIndex();

    if (high < thisLow || low > thisHigh) {
      // No intersection
      Clear();
      return;
    }

    int32_t i;
    // Clear all the words before and after those set in the other vector
    if (low < thisLow)
      low = thisLow;
    else {
      for (i = thisLow; i < low; i++)
        WordAt(i) = kZeroBits;
    }
    if (high > thisHigh) {
      high = thisHigh;
    } else {
      for (i = thisHigh; i > high; i--)
        WordAt(i) = kZeroBits;
    }

    // AND in all of the words from the 2nd vector
    for (i = low; i <= high; i++)
      WordAt(i) &= v.WordAt(i);

    // compress representation
    if (IsZero()) { Clear(); }
  }
}

template <class Allocator>
template <class B2>
  inline void ABitVector<Allocator>::Andc(const B2 &v) {
  if (v.IsZero()) return;
  if (!v.hasBitWordRepresentation() || (v.wordSize() != kBitWordSize)) {
    uint32_t lastWordIndex = SizeInWords(fNumBits);

    typename B2::Cursor vc(v);
    vc.SetToFirstOne();
    uint32_t wordIndex = vc / kBitWordSize;
    if (lastWordIndex <= wordIndex) return;

    BitWord word = 0;
    uint32_t bitIndex =  vc % kBitWordSize;
    word |= (kHighBit >> bitIndex);

    for (vc.SetToNextOne();vc.Valid();vc.SetToNextOne()) {
      if (vc/kBitWordSize != wordIndex) {
        WordAt(wordIndex) &= ~word;

        wordIndex = vc/kBitWordSize;
        if (lastWordIndex <= wordIndex) return;
        word = 0;
      }
      bitIndex =  vc % kBitWordSize;
      word |= (kHighBit >> bitIndex);
    }
    WordAt(wordIndex) &= ~word;
  } else /* (hasBitWordRepresentation() && (v.wordSize() == kBitWordSize)) */ {
    // can use more efficient implementation by ANDC corresponding words from the two bit vectors together
    // as their underlying bit representations are equivalent
    int32_t low = v.FirstOneWordIndex();
    int32_t high = v.LastOneWordIndex();
    int32_t thisLow = FirstOneWordIndex();
    int32_t thisHigh = LastOneWordIndex();
    if (high < thisLow || low > thisHigh)
       return; // No intersection

    // AND in the logical NOT of all of the words from the 2nd vector
    if (low < thisLow)
       low = thisLow;
    if (high > thisHigh)
       high = thisHigh;
    for (int32_t wordIndex = low; wordIndex <= high; wordIndex++)
       WordAt(wordIndex) &= ~v.WordAt(wordIndex);

    // compress representation
    if (IsZero()) { Clear(); }
  }
}

template <class Allocator>
template <class B2>
  inline ABitVector<Allocator> &ABitVector<Allocator>::operator= (const B2 &v) {
  Clear();
  Or(v);
  return *this;
}


template <class Allocator>
template <class B2>
  inline void ABitVector<Allocator>::Or (B2 **vs, uint32_t nvs) {
  uint32_t i;
  for (i=0; i<nvs; i++){
    Or(*(vs[i]));
  }
}

// ABitVector<Allocator>::Xor (const ABitVector &)
//
// Compute the bitwise XOR of this vector and the input vector.
// Place the result in this vector.

template <class Allocator>
inline bool ABitVector<Allocator>::Xor (const ABitVector &inputVector) {
  GrowTo(inputVector);
  return Xor (inputVector, *this);
}

//
// Swap one sparse bit vector with another.
template <class Allocator>
inline void Swap(ABitVector<Allocator> &vectorA, ABitVector<Allocator> &vectorB) {
  uint32_t numBits  = vectorA.fNumBits;
  BitWord* bitWords = vectorA.fBitWords;

  vectorA.fNumBits  = vectorB.fNumBits;
  vectorA.fBitWords = vectorB.fBitWords;
  vectorB.fNumBits  = numBits;
  vectorB.fBitWords = bitWords;
}


template <class Allocator>
inline
ABitVector<Allocator> & ABitVector<Allocator>::operator= (const ABitVector<Allocator> &adoptVector) {

  if (adoptVector.IsNull()) {
    ClearToNull();
    return *this;
  }

  GrowTo(adoptVector);
  uint32_t bitsToCopy, remainder;

  bitsToCopy = adoptVector.fNumBits;
  if (bitsToCopy > fNumBits) {
    bitsToCopy = fNumBits;
    remainder = 0;
  } else {
    remainder = SizeInBytes(fNumBits) - SizeInBytes(bitsToCopy);
  }

  memcpy (fBitWords, adoptVector.fBitWords, SizeInBytes(bitsToCopy));

  if (remainder > 0) {
    memset ((char *) fBitWords + SizeInBytes(bitsToCopy), 0, remainder);
  }

  return *this;
}


template <class Allocator>
inline
bool ABitVector<Allocator>::operator== (const ABitVector<Allocator> &v) const {
  uint32_t bx = 0, wx = 0,
         minLen = Minimum(fNumBits,v.fNumBits);

  while (bx < minLen) {
    if (fBitWords[wx] != v.fBitWords[wx]) return false;
    ++wx;
    bx += kBitWordSize;
  }

  if (bx < fNumBits) {
    while (bx < fNumBits) {
      if (fBitWords[wx] != kZeroBits) return false;
      ++wx;
      bx += kBitWordSize;
    }
  } else if (bx < v.fNumBits) {
    while (bx < v.fNumBits) {
      if (v.fBitWords[wx] != kZeroBits) return false;
      ++wx;
      bx += kBitWordSize;
    }
  }

  return true;
}

template <class Allocator>
inline
unsigned long ABitVector<Allocator>::MemoryUsage() const {
  unsigned long usageBytes;

  usageBytes = sizeof(ABitVector);
  usageBytes += SizeInBytes(fNumBits);

  return usageBytes;
}

// ABitVector<Allocator>::Clear ()
//
// Clear all bits in the vector (ie. clear and discard memory)

template <class Allocator>
inline void ABitVector<Allocator>::Clear () {
  if (fNumBits) {
    Allocator::deallocate(fBitWords, SizeInBytes(fNumBits));
    fNumBits=0;
  }
  // unallocated, but non-null
  fBitWords = (BitWord *) 1;
}

// ABitVector<Allocator>::ClearToNull ()
//
// Clear all bits in the vector (ie. clear and discard memory), clear to null set.

template <class Allocator>
inline void ABitVector<Allocator>::ClearToNull () {
  Clear();
  // unallocated, and null
  fBitWords=NULL;
}

// ABitVector<Allocator>::Set ()
//
// Set first n bits in the vector

template <class Allocator>
inline void ABitVector<Allocator>::SetAll (BitIndex numBits) {
  if (numBits == 0)
    return;

  GrowTo(numBits);
  uint32_t lastWordIndex = SizeInWords(numBits) - 1;
  for (uint32_t i = 0; i < lastWordIndex; i++)
    WordAt(i) = (BitWord)-1;

  uint32_t lastIndex = lastWordIndex * kBitWordSize;
  for (uint32_t i = lastIndex; i < numBits; i++)
    (*this)[i] = true;
}


// ABitVector<Allocator>::LowestZero ()
//
// Return the index of the first zero bit in the vector

template <class Allocator>
inline BitIndex ABitVector<Allocator>::LowestZero () const {
  uint32_t wordIndex, vectorWordSize;

  vectorWordSize = SizeInWords(fNumBits);

  // Scan to the first word which has a zero bit
  for (wordIndex = 0;
       (wordIndex < vectorWordSize) && (WordAt(wordIndex) == kFullMask);
       ++wordIndex);

  // If we found no zero bits return the index just beyond the vector
  if (wordIndex >= vectorWordSize) return fNumBits + 1;

  return wordIndex * kBitWordSize +
         BitManipulator::LeadingOnes (WordAt(wordIndex));
}

// ABitVector<Allocator>::LowestOne ()
//
// Return the index of the first one bit in the vector

template <class Allocator>
inline BitIndex ABitVector<Allocator>::FirstOne () const {
  uint32_t wordIndex, vectorWordSize;

  vectorWordSize = SizeInWords(fNumBits);

  // Scan to the first word which has a one bit
  for (wordIndex=0;
       (wordIndex < vectorWordSize) && (WordAt(wordIndex) == kZeroBits);
       ++wordIndex);

  // If we found no bits return 0;
  if (wordIndex >= vectorWordSize) return 0;

  return wordIndex * kBitWordSize +
         BitManipulator::LeadingZeroes (WordAt(wordIndex));
}

template <class Allocator>
inline BitIndex ABitVector<Allocator>::LastOne () const {
  uint32_t lastIndex, wordIndex, vectorWordSize;

  vectorWordSize = SizeInWords(fNumBits);
  lastIndex = 0;

  // Scan to find the last word which has a one bit
  for (wordIndex=0;
       (wordIndex < vectorWordSize);
       ++wordIndex)
    if (WordAt(wordIndex) != kZeroBits)
      lastIndex = wordIndex;

  // If we found no zero bits return 0;
  if (lastIndex>= vectorWordSize || WordAt(lastIndex) == kZeroBits) return 0;
  return lastIndex * kBitWordSize +
         kBitWordSize -
    BitManipulator::TrailingZeroes (WordAt(lastIndex)) - 1;
}

// ABitVector<Allocator>::FirstOneWordIndex ()
//
// Return the index of the first word with a one bit in the vector, or SizeInWords() if not found
template <class Allocator>
inline int32_t ABitVector<Allocator>::FirstOneWordIndex () const {
  int32_t wordIndex;
  uint32_t vectorWordSize = SizeInWords();

  // Scan to the first word which has a one bit
  for (wordIndex=0;
       (wordIndex < vectorWordSize) && (WordAt(wordIndex) == kZeroBits);
       ++wordIndex);

  return wordIndex;
}

// ABitVector<Allocator>::LastOneWordIndex ()
//
// Return the index of the last word with a one bit in the vector, or -1 if vector isZero
template <class Allocator>
inline int32_t ABitVector<Allocator>::LastOneWordIndex () const {
  int32_t wordIndex;

  // Scan to find the last word which has a one bit
  for (wordIndex = SizeInWords()-1;
       (wordIndex >= 0) && (WordAt(wordIndex) == kZeroBits);
       wordIndex--);

  return wordIndex;
}

template <class Allocator>
inline BitIndex ABitVector<Allocator>::ClearLastOneIfThereIsOneInRange(BitIndex low, BitIndex high, bool& foundone)
{

   CS2Assert (low < SizeInBits(), ("Bit vector index out of range"));
   CS2Assert (high < SizeInBits(), ("Bit vector index out of range"));
   BitIndex lowWordIndex = low / kBitWordSize;
   BitIndex highWordIndex = high / kBitWordSize;

   for (BitIndex wordIndex = highWordIndex;
        wordIndex >= lowWordIndex;) {
      if (WordAt(wordIndex) != kZeroBits) {
         int32_t rc = wordIndex * kBitWordSize +
            kBitWordSize -
            BitManipulator::TrailingZeroes (WordAt(wordIndex)) - 1;
         this->operator[](rc) = false;
         foundone = true;
         return rc;
      } else if (wordIndex == 0) {
         foundone = false;
         return ~(BitIndex) 0;
      } else {
         wordIndex--;
      }
   }
   foundone = false;
   return ~(BitIndex) 0;
}

// ABitVector<Allocator>::IsZero ()
//
// Determine if this vector is zero (cleared)

template <class Allocator>
inline bool ABitVector<Allocator>::IsZero () const {
  uint32_t wordIndex, vectorWordSize;

  vectorWordSize = SizeInWords(fNumBits);

  for (wordIndex = 0; wordIndex < vectorWordSize; ++wordIndex)
    if (WordAt(wordIndex) != kZeroBits) return false;

  return true;
}

// ABitVector<Allocator>::IsNull
//
// Determine if this vector is null (and also is zero)

template <class Allocator>
inline bool ABitVector<Allocator>::IsNull () const {
  return (fNumBits==0) && (fBitWords == NULL);
}


// ABitVector<Allocator>::Intersects (const ABitVector<Allocator> &)
//
// Return true iff the intersection (and) of this vector and the input vector
// in non-zero.

template <class Allocator>
inline bool ABitVector<Allocator>::Intersects (const ABitVector<Allocator> &vector) const {
  uint32_t wordIndex, thisWordSize, vectorWordSize, smallerWordSize;

  thisWordSize = SizeInWords(fNumBits);
  vectorWordSize = SizeInWords(vector.fNumBits);
  smallerWordSize = Minimum (thisWordSize, vectorWordSize);

  for (wordIndex = 0; wordIndex < smallerWordSize; ++wordIndex) {
    if ((WordAt(wordIndex) & vector.WordAt(wordIndex)) != kZeroBits)
      return true;
  }

  return false;
}

template <class Allocator> template <class B2>
inline bool ABitVector<Allocator>::Intersects (const B2 &vector) const {

  typename B2::Cursor b2(vector);
  for (b2.SetToFirstOne(); b2.Valid(); b2.SetToNextOne())
    if (ValueAt(b2)) return true;
  return false;
}

// ABitVector<Allocator>::operator <=
// Determines if this vector is a subset of the inputVector
// Equivalent to IsZero(this &~ inputVector)
template <class Allocator>
inline bool ABitVector<Allocator>::operator <= (const ABitVector<Allocator> &vector) const {
  uint32_t wordIndex, thisWordSize, vectorWordSize, smallerWordSize;

  thisWordSize = SizeInWords(fNumBits);
  vectorWordSize = SizeInWords(vector.fNumBits);
  smallerWordSize = Minimum (thisWordSize, vectorWordSize);

  for (wordIndex = 0; wordIndex < smallerWordSize; ++wordIndex) {
    if ((WordAt(wordIndex) &~ vector.WordAt(wordIndex)) != kZeroBits)
      return false;
  }

  for (; wordIndex < thisWordSize; ++wordIndex) {
    if ((WordAt(wordIndex) ) != kZeroBits)
      return false;
  }

  return true;
}

template <class Allocator>
inline ABitVector<Allocator> &
ABitVector<Allocator>::operator-=(const ABitVector<Allocator> &v) {
  Andc(v);
  return *this;
}

template <class Allocator>
inline ABitVector<Allocator> &
ABitVector<Allocator>::operator|=(const ABitVector<Allocator> &v) {
  Or(v);
  return *this;
}

template <class Allocator>
inline ABitVector<Allocator> &
ABitVector<Allocator>::operator&=(const ABitVector<Allocator> &v) {
  And(v);
  return *this;
}

template <class Allocator>
template <class B2>
inline ABitVector<Allocator> &
ABitVector<Allocator>::operator-=(const B2 &v) {
  Andc(v);
  return *this;
}

template <class Allocator>
template <class B2>
inline ABitVector<Allocator> &
ABitVector<Allocator>::operator|=(const B2 &v) {
  Or(v);
  return *this;
}

template <class Allocator>
template <class B2>
inline ABitVector<Allocator> &
ABitVector<Allocator>::operator&=(const B2 &v) {
  And(v);
  return *this;
}

// ABitVector<Allocator>::PopulationCount
//
// Count the number of '1' bits in the vector

#ifndef USE_POPCNTB
template <class Allocator>
inline
uint32_t ABitVector<Allocator>::PopulationCount (uint32_t numBits) const {
  uint32_t popCount = 0;
  uint32_t wordIndex, thisWordSize, lastWord;

  if (numBits == 0) return popCount;

  thisWordSize = SizeInWords(fNumBits);
  lastWord = Minimum (thisWordSize, SizeInWords(numBits + 1) - 1);

  for (wordIndex = 0; wordIndex < lastWord; ++wordIndex) {
    popCount += BitManipulator::PopulationCount (WordAt(wordIndex));
  }

  if (lastWord < thisWordSize) {
    uint32_t bitIndex;
    BitWord testWord;

    // Shift out (low-order) bits that should not be counted
    bitIndex = numBits % kBitWordSize;
    if (bitIndex) {
      testWord = WordAt(lastWord) >> (kBitWordSize - bitIndex);

      // Add the population count of the shifted word
      popCount += BitManipulator::PopulationCount (testWord);
    }
  }

  return popCount;
}
#else
// This version of PopulationCount uses the pwr5 instruction popcntb,
// which given a word returns in each byte the number "1" bits in the
// corresponding byte
// This is experimental code that has never been turned on. Needs to
// be reviewed before production usage
// If it is ever enabled, we should ensure it is compiled out on zOS
template <class Allocator>
inline
uint32_t ABitVector<Allocator>::PopulationCount (uint32_t numBits) const
{

  // WORDS_IN_8_BIT_ACCUM - worst-case maximum number of words to accumulate popcntb results in 8 bits sections
  // WORDS_IN_16_BIT_ACCUM - worst-case maximum number of words to accumulate popcntb results in 16 bits sections
  const uint32_t WORDS_IN_8_BIT_ACCUM = 31, WORDS_IN_16_BIT_ACCUM = 264*31;


  BitWord popCount = 0;
  BitWord tempCount = 0, tempCount2 = 0, tempCount3 = 0, tempCount4 = 0;
  BitWord tempCount0 = 0, tempCount0a = 0; // for the residue of words;

  uint32_t wordIndex = 0, streamWordSize, thisWordSize, lastWord;

  if (numBits == 0) return popCount;

  thisWordSize = SizeInWords(fNumBits); // number of words in the vector

  // lastWord - the number of words before (not including) the word
  // with selected bit; or it is the total number of words if it's
  // less than the former
  lastWord = Minimum (thisWordSize, SizeInWords(numBits + 1) - 1);

  streamWordSize = (lastWord - lastWord % (WORDS_IN_16_BIT_ACCUM*4)) / 4; // for 4 streams

  if (lastWord >= WORDS_IN_16_BIT_ACCUM * 4){
    // version 1 - more than 32,736 words (4 streams)

    for (wordIndex = 0; wordIndex < streamWordSize; wordIndex+=WORDS_IN_16_BIT_ACCUM){
      BitWord c2=0, c2_1=0, c2_2=0, c2a=0, c2a_1=0, c2a_2=0 ;
      BitWord c2b=0, c2b_1=0, c2b_2=0, c2c=0, c2c_1=0, c2c_2=0 ;

      for (BitIndex i2=wordIndex; i2 < wordIndex+WORDS_IN_16_BIT_ACCUM; i2+=WORDS_IN_8_BIT_ACCUM) {
        BitWord c1 = 0, c1a = 0, c1b = 0, c1c = 0;
        for (BitIndex i1=i2; i1 < i2+WORDS_IN_8_BIT_ACCUM; i1++) {
          c1 += __popcntb(fBitWords[i1]); // count the number of 1's in each byte of the word at wordIndex index
          c1a += __popcntb(fBitWords[streamWordSize+i1]);   // count the number of 1's in each byte of the word at wordIndex+streamWordSize index
          c1b += __popcntb(fBitWords[streamWordSize*2+i1]); // count the number of 1's in each byte of the word at wordIndex+2*streamWordSize index
          c1c += __popcntb(fBitWords[streamWordSize*3+i1]); // count the number of 1's in each byte of the word at wordIndex+3*streamWordSize index
        }
#ifdef BITVECTOR_64BIT
        c2_1 += (c1 & 0x00FF00FF00FF00FFul) ;
        c2_2 += ((c1 >> 8)  & 0x00FF00FF00FF00FFul);
        c2a_1 += (c1a & 0x00FF00FF00FF00FFul);
        c2a_2 += ((c1a >> 8)  & 0x00FF00FF00FF00FFul);
        c2b_1 += (c1b & 0x00FF00FF00FF00FFul);
        c2b_2 += ((c1b >> 8)  & 0x00FF00FF00FF00FFul);
        c2c_1 += (c1c & 0x00FF00FF00FF00FFul);
        c2c_2 += ((c1c >> 8)  & 0x00FF00FF00FF00FFul);
#else /* !BITVECTOR_64BIT */
        c2_1 += (c1 & 0x00FF00FFul) ;
        c2_2 += ((c1 >> 8)  & 0x00FF00FFul);
        c2a_1 += (c1a & 0x00FF00FFul);
        c2a_2 += ((c1a >> 8)  & 0x00FF00FFul);
        c2b_1 += (c1b & 0x00FF00FFul);
        c2b_2 += ((c1b >> 8)  & 0x00FF00FFul);
        c2c_1 += (c1c & 0x00FF00FFul);
        c2c_2 += ((c1c >> 8)  & 0x00FF00FFul);
#endif /* BITVECTOR_64BIT */
      }
      c2 = c2_1 + c2_2 ;
      c2a = c2a_1 + c2a_2 ;
      c2b = c2b_1 + c2b_2 ;
      c2c = c2c_1 + c2c_2 ;

#ifdef BITVECTOR_64BIT
      c2 = ((c2) & 0x0000FFFF0000FFFFul) + ((c2 >> 16) & 0x0000FFFF0000FFFFul);
      tempCount += ((c2) & 0x00000000FFFFFFFFul) + ((c2 >> 32) & 0x00000000FFFFFFFFul);
      c2a = ((c2a) & 0x0000FFFF0000FFFFul) + ((c2a >> 16) & 0x0000FFFF0000FFFFul);
      tempCount2 += ((c2a) & 0x00000000FFFFFFFFul) + ((c2a >> 32) & 0x00000000FFFFFFFFul);
      c2b = ((c2b) & 0x0000FFFF0000FFFFul) + ((c2b >> 16) & 0x0000FFFF0000FFFFul);
      tempCount3 += ((c2b) & 0x00000000FFFFFFFFul) + ((c2b >> 32) & 0x00000000FFFFFFFFul);
      c2c = ((c2c) & 0x0000FFFF0000FFFFul) + ((c2c >> 16) & 0x0000FFFF0000FFFFul);
      tempCount4 += ((c2c) & 0x00000000FFFFFFFFul) + ((c2c >> 32) & 0x00000000FFFFFFFFul);
#else /* !BITVECTOR_64BIT */
      tempCount += ((c2) & 0x0000FFFFul) + ((c2 >> 16) & 0x0000FFFFul);
      tempCount2 += ((c2a) & 0x0000FFFFul) + ((c2a >> 16) & 0x0000FFFFul);
      tempCount3 += ((c2b) & 0x0000FFFFul) + ((c2b >> 16) & 0x0000FFFFul);
      tempCount4 += ((c2c) & 0x0000FFFFul) + ((c2c >> 16) & 0x0000FFFFul);
#endif /* BITVECTOR_64BIT */

    }

    wordIndex = wordIndex + streamWordSize * 3; // advance the wordIndex to catch up with streams
  }

  streamWordSize = ((lastWord - wordIndex) - ((lastWord - wordIndex) % (WORDS_IN_8_BIT_ACCUM*2))) / 2; // for 2 streams


  if (lastWord - wordIndex >= WORDS_IN_8_BIT_ACCUM * 2){
    // version 2 - more than 62 words left (or in total) (2 streams)

    BitWord c2=0, c2_1=0, c2_2=0, c2a=0, c2a_1=0, c2a_2=0;

    for (; wordIndex < wordIndex + streamWordSize; wordIndex+=WORDS_IN_8_BIT_ACCUM) {
      BitWord c1 = 0, c1a = 0, c1b = 0, c1c = 0;
      for (BitIndex i1=wordIndex; i1 < wordIndex+WORDS_IN_8_BIT_ACCUM; i1++) {
        c1 += __popcntb(fBitWords[i1]); // count the number of 1's in each byte of the word at wordIndex index
        c1a += __popcntb(fBitWords[streamWordSize+i1]); // count the number of 1's in each byte of the word at wordIndex index

      }
#ifdef BITVECTOR_64BIT
      c2_1 += (c1 & 0x00FF00FF00FF00FFul) ;
      c2_2 += ((c1 >> 8)  & 0x00FF00FF00FF00FFul);
      c2a_1 += (c1a & 0x00FF00FF00FF00FFul);
      c2a_2 += ((c1a >> 8)  & 0x00FF00FF00FF00FFul);
#else /* !BITVECTOR_64BIT */
      c2_1 += (c1 & 0x00FF00FFul) ;
      c2_2 += ((c1 >> 8)  & 0x00FF00FFul);
      c2a_1 += (c1a & 0x00FF00FFul);
      c2a_2 += ((c1a >> 8)  & 0x00FF00FFul);
#endif /* BITVECTOR_64BIT */

    }

    c2 = c2_1 + c2_2 ;
    c2a = c2a_1 + c2a_2 ;

#ifdef BITVECTOR_64BIT
    c2 = ((c2) & 0x0000FFFF0000FFFFul) + ((c2 >> 16) & 0x0000FFFF0000FFFFul);
    tempCount += ((c2) & 0x00000000FFFFFFFFul) + ((c2 >> 32) & 0x00000000FFFFFFFFul);
    c2a = ((c2a) & 0x0000FFFF0000FFFFul) + ((c2a >> 16) & 0x0000FFFF0000FFFFul);
    tempCount2 += ((c2a) & 0x00000000FFFFFFFFul) + ((c2a >> 32) & 0x00000000FFFFFFFFul);
#else /* !BITVECTOR_64BIT */
    tempCount += ((c2) & 0x0000FFFFul) + ((c2 >> 16) & 0x0000FFFFul);
    tempCount2 += ((c2a) & 0x0000FFFFul) + ((c2a >> 16) & 0x0000FFFFul);
#endif /* BITVECTOR_64BIT */

    wordIndex = wordIndex + streamWordSize; // advance the wordIndex to catch up with 2 streams
  }


  // deal with the residue of words (when less than 62)

  if (lastWord - wordIndex > WORDS_IN_8_BIT_ACCUM){
    for (BitIndex i = wordIndex; i < wordIndex + WORDS_IN_8_BIT_ACCUM; i++){
      tempCount0 += __popcntb(fBitWords[i]);
    }
    wordIndex += WORDS_IN_8_BIT_ACCUM;
  }

  for (BitIndex i = wordIndex; i < lastWord; i++){
    tempCount0a += __popcntb(fBitWords[i]);
  }

    // deal with the residue of bits of the last word (the word which the bit is selected from)
  if (lastWord < thisWordSize) {
    uint32_t bitIndex;
    BitWord testWord;

    // Shift out (low-order) bits that should not be counted
    bitIndex = numBits % kBitWordSize;
    if (bitIndex) {
      testWord = WordAt(lastWord) >> (kBitWordSize - bitIndex);

      // Add the population count of the shifted word
      tempCount0a += __popcntb(testWord);
    }
  }

  // count the remaining bits
#ifdef BITVECTOR_64BIT
  tempCount0 = ((tempCount0) & 0x00FF00FF00FF00FFul) + ((tempCount0 >> 8 ) & 0x00FF00FF00FF00FFul);
  tempCount0a = ((tempCount0a) & 0x00FF00FF00FF00FFul) + ((tempCount0a >> 8 ) & 0x00FF00FF00FF00FFul);
  tempCount0 += tempCount0a;
  tempCount0 = ((tempCount0) & 0x0000FFFF0000FFFFul) + ((tempCount0 >> 16) & 0x0000FFFF0000FFFFul);
  tempCount0 = ((tempCount0) & 0x00000000FFFFFFFFul) + ((tempCount0 >> 32) & 0x00000000FFFFFFFFul);
#else /* !BITVECTOR_64BIT */
  tempCount0 = ((tempCount0) & 0x00FF00FFul) + ((tempCount0 >> 8 ) & 0x00FF00FFul);
  tempCount0a = ((tempCount0a) & 0x00FF00FFul) + ((tempCount0a >> 8 ) & 0x00FF00FFul);
  tempCount0 += tempCount0a;
  tempCount0 = ((tempCount0) & 0x0000FFFFul) + ((tempCount0 >> 16) & 0x0000FFFFul);
#endif /* BITVECTOR_64BIT */

#ifdef DEV_VERSION
// do sanity check

    uint32_t checkCount = PopulationCount(numBits);
    CS2Assert(checkCount == tempCount + tempCount2 + tempCount3 + tempCount4 + tempCount0, ("Wrong result by ABitVector<Allocator>::PopulationCount_pwr5"));
#endif

  return tempCount + tempCount2 + tempCount3 + tempCount4 + tempCount0;

}
#endif


template <class Allocator>
inline
uint32_t ABitVector<Allocator>::PopulationCount (const ABitVector<Allocator> &mask) const {

  uint32_t thisWordSize = SizeInWords(fNumBits);
  uint32_t maskWordSize = mask.SizeInWords(mask.fNumBits);
  uint32_t wordIndex, popCount = 0;

  if (thisWordSize > maskWordSize)
    thisWordSize = maskWordSize;

  for (wordIndex=0; wordIndex<thisWordSize; wordIndex+=1)
    popCount += BitManipulator::PopulationCount (WordAt(wordIndex) & mask.WordAt(wordIndex));

  return popCount;
}

// ABitVector<Allocator>::CopyToMemory (void *, BitIndex)
//
// Copy the vector to the given memory area.
// If numBits if greater than the size of the vector, then pad with
// zeroes up to the next 32-bit boundary, to maintain compatibility
// between implementations using different word sizes

template <class Allocator>
inline void ABitVector<Allocator>::CopyToMemory (void *pMemory, BitIndex numBits) const {
  ShortWord *pBitWord = (ShortWord *) pMemory;
  uint32_t shortwordIndex, wordIndex, smallerWordSize, vectorWordSize, memoryWordSize;

  vectorWordSize = SizeInShortWords(fNumBits);
  memoryWordSize = SizeInShortWords(numBits);
  smallerWordSize = Minimum (vectorWordSize, memoryWordSize);

  // Copy the words from the bit vector to memory
  for (shortwordIndex = wordIndex = 0; shortwordIndex < smallerWordSize; ++shortwordIndex) {
#ifdef BITVECTOR_64BIT
    union {
      BitWord word;
      ShortWord shortword[2];
    } data;

    data.word = WordAt(wordIndex++);
    *pBitWord++ = data.shortword[0];

    if (++shortwordIndex >= smallerWordSize) break;
    *pBitWord++ = data.shortword[1];
#else
    *pBitWord++ = WordAt(shortwordIndex);
#endif
  }

  // Zero any remaining space in memory
  for ( ; shortwordIndex < memoryWordSize; ++shortwordIndex)
    *pBitWord++ = 0;
}

// ABitVector<Allocator>::CopyFromMemory (void *, BitIndex)
//
// Copy the vector from the given memory area.  The number of bits
// copied is the minumum of the number in memory and the size of the
// vector. Assume that the data in memory is valid up to the next
// 32-bit boundary, to maintain compatibility between implementations
// using different word sizes

template <class Allocator>
inline void ABitVector<Allocator>::CopyFromMemory (void *pMemory, BitIndex numBits) {
  ShortWord *pBitWord = (ShortWord *) pMemory;
  uint32_t shortwordIndex, wordIndex, vectorWordSize, memoryWordSize, smallerWordSize;

  vectorWordSize = SizeInShortWords(fNumBits);
  memoryWordSize = SizeInShortWords(numBits);
  smallerWordSize = Minimum (vectorWordSize, memoryWordSize);

  for (shortwordIndex = wordIndex = 0; shortwordIndex < smallerWordSize; ++shortwordIndex) {
#ifdef BITVECTOR_64BIT
    union {
      BitWord word;
      ShortWord shortword[2];
    } data;

    data.shortword[0] = *pBitWord++;

    if (++shortwordIndex < smallerWordSize)
      data.shortword[1] = *pBitWord++;
    else
      data.shortword[1] = 0;
    WordAt(wordIndex++) = data.word;
#else
    WordAt(shortwordIndex) = *pBitWord++;
#endif
  }
}

// ABitVector<Allocator>::And (const ABitVector<Allocator> &, ABitVector<Allocator> &) const
//
// Compute the bitwise AND of this vector and the input vector.
// Place the result in the output vector.

template <class Allocator>
inline
bool ABitVector<Allocator>::And (const ABitVector<Allocator> &inputVector,
                                 ABitVector<Allocator> &outputVector) const {
  uint32_t  wordIndex, thisWordSize, inputWordSize, smallerWordSize,
          largerWordSize, outputWordSize;
  BitWord inputWord, thisWord, newWord;
  bool changed = false;

  thisWordSize = SizeInWords(fNumBits);
  inputWordSize = SizeInWords(inputVector.fNumBits);
  smallerWordSize = Minimum (thisWordSize, inputWordSize);
  largerWordSize = Maximum (thisWordSize, inputWordSize);

  outputVector.GrowTo(largerWordSize*kBitWordSize, false);
  outputWordSize = SizeInWords(outputVector.fNumBits);

  for (wordIndex = 0; wordIndex < smallerWordSize; ++wordIndex) {
    thisWord = WordAt(wordIndex);
    inputWord = inputVector.WordAt(wordIndex);
    newWord = thisWord & inputWord;
    outputVector.WordAt(wordIndex) = newWord;
    changed |= (newWord != thisWord);
  }

  changed |= (wordIndex < largerWordSize);
  for ( ; wordIndex < outputWordSize; ++wordIndex)
    outputVector.WordAt(wordIndex) = kZeroBits;

  return changed;
}

// ABitVector<Allocator>::Andc (const ABitVector<Allocator> &, ABitVector<Allocator> &) const
//
// Compute the bitwise AND with this vector and COMPLEMENT of the input vector.
// Place the result in the output vector.
template <class Allocator>
inline
bool ABitVector<Allocator>::Andc (const ABitVector<Allocator> &inputVector,
                                  ABitVector<Allocator> &outputVector) const {
  uint32_t  wordIndex, thisWordSize, inputWordSize, smallerWordSize,
          largerWordSize, outputWordSize;
  BitWord inputWord, thisWord, newWord;
  bool changed = false;

  thisWordSize = SizeInWords(fNumBits);
  inputWordSize = SizeInWords(inputVector.fNumBits);
  smallerWordSize = Minimum (thisWordSize, inputWordSize);
  largerWordSize = Maximum (thisWordSize, inputWordSize);

  outputVector.GrowTo(largerWordSize*kBitWordSize, false);
  outputWordSize = SizeInWords(outputVector.fNumBits);

  for (wordIndex = 0; wordIndex < smallerWordSize; ++wordIndex) {
    thisWord = WordAt(wordIndex);
    inputWord = inputVector.WordAt(wordIndex);
    newWord = thisWord & ~inputWord;    // COMPLEMENT OF INPUT!!!
    outputVector.WordAt(wordIndex) = newWord;
    changed |= (newWord != thisWord);
  }

  if (thisWordSize > inputWordSize) {
    changed |= (wordIndex < thisWordSize);
    for ( ; wordIndex < thisWordSize; ++wordIndex)
      outputVector.WordAt(wordIndex) = WordAt(wordIndex);
  } else {
    changed |= (wordIndex < inputWordSize);
    for ( ; wordIndex < inputWordSize; ++wordIndex)
      outputVector.WordAt(wordIndex) = kZeroBits;
  }

  for ( ; wordIndex < outputWordSize; ++wordIndex)
    outputVector.WordAt(wordIndex) = kZeroBits;

  return changed;
}

// ABitVector<Allocator>::Or (const ABitVector<Allocator> &, ABitVector<Allocator> &) const
//
// Compute the bitwise OR of this vector and the input vector.
// Place the result in the output vector.

template <class Allocator>
inline
bool ABitVector<Allocator>::Or (const ABitVector<Allocator> &inputVector,
                                ABitVector<Allocator> &outputVector) const {
  uint32_t  wordIndex, thisWordSize, inputWordSize, smallerWordSize,
          largerWordSize, outputWordSize;
  BitWord inputWord, thisWord, newWord;
  bool changed = false;

  thisWordSize = SizeInWords(fNumBits);
  inputWordSize = SizeInWords(inputVector.fNumBits);
  smallerWordSize = Minimum (thisWordSize, inputWordSize);
  largerWordSize = Maximum (thisWordSize, inputWordSize);

  outputVector.GrowTo(largerWordSize*kBitWordSize, false);
  outputWordSize = SizeInWords(outputVector.fNumBits);

  for (wordIndex = 0; wordIndex < smallerWordSize; ++wordIndex) {
    thisWord = WordAt(wordIndex);
    inputWord = inputVector.WordAt(wordIndex);
    newWord = thisWord | inputWord;
    outputVector.WordAt(wordIndex) = newWord;
    changed |= (newWord != thisWord);
  }

  if (thisWordSize > inputWordSize) {
    changed |= (wordIndex < thisWordSize);
    for ( ; wordIndex < thisWordSize; ++wordIndex)
      outputVector.WordAt(wordIndex) = WordAt(wordIndex);
  } else {
    changed |= (wordIndex < inputWordSize);
    for ( ; wordIndex < inputWordSize; ++wordIndex)
      outputVector.WordAt(wordIndex) = inputVector.WordAt(wordIndex);
  }

  for ( ; wordIndex < outputWordSize; ++wordIndex)
    outputVector.WordAt(wordIndex) = kZeroBits;

  return changed;
}

// ABitVector<Allocator>::Xor (const ABitVector<Allocator> &, ABitVector<Allocator> &) const
//
// Compute the bitwise XOR of this vector and the input vector.
// Place the result in the output vector.

template <class Allocator>
inline
bool ABitVector<Allocator>::Xor (const ABitVector<Allocator> &inputVector,
                                 ABitVector<Allocator> &outputVector) const {
  uint32_t  wordIndex, thisWordSize, inputWordSize, smallerWordSize,
          largerWordSize, outputWordSize;
  BitWord inputWord, thisWord, newWord;
  bool changed = false;

  thisWordSize = SizeInWords(fNumBits);
  inputWordSize = SizeInWords(inputVector.fNumBits);
  smallerWordSize = Minimum (thisWordSize, inputWordSize);
  largerWordSize = Maximum (thisWordSize, inputWordSize);

  outputVector.GrowTo(largerWordSize*kBitWordSize, false);
  outputWordSize = SizeInWords(outputVector.fNumBits);

  for (wordIndex = 0; wordIndex < smallerWordSize; ++wordIndex) {
    thisWord = WordAt(wordIndex);
    inputWord = inputVector.WordAt(wordIndex);
    newWord = thisWord ^ inputWord;
    outputVector.WordAt(wordIndex) = newWord;
    changed |= (newWord != thisWord);
  }

  if (thisWordSize > inputWordSize) {
    changed |= (wordIndex < thisWordSize);
    for ( ; wordIndex < thisWordSize; ++wordIndex)
      outputVector.WordAt(wordIndex) = WordAt(wordIndex);
  } else {
    changed |= (wordIndex < inputWordSize);
    for ( ; wordIndex < inputWordSize; ++wordIndex)
      outputVector.WordAt(wordIndex) = inputVector.WordAt(wordIndex);
  }

  for ( ; wordIndex < outputWordSize; ++wordIndex)
    outputVector.WordAt(wordIndex) = kZeroBits;

  return changed;
}

}

#ifdef CS2_ALLOCINFO
#undef allocate
#undef deallocate
#undef reallocate
#endif

#endif // CS2_BITVECTR_H
