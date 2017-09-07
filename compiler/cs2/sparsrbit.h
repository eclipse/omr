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

#ifndef CS2_SPARSRBIT_H
#define CS2_SPARSRBIT_H

#include "cs2/cs2.h"
#include "cs2/allocator.h"
#include "cs2/bitmanip.h"
#include "cs2/bitvectr.h"

#ifdef CS2_ALLOCINFO
#define allocate(x) allocate(x, __FILE__, __LINE__)
#define deallocate(x,y) deallocate(x, y, __FILE__, __LINE__)
#define reallocate(x,y,z) reallocate(x, y, z, __FILE__, __LINE__)
#endif

namespace CS2 {
/***************************************************************************
 * This is the definition of the ASparseBitVector class.
 *
 * It is a bitvector implementation as a sorted array of bits. This
 * makes the storage requirements proportional to the number of 1-bits
 * in the vector, instead of being proportional to the highest bit index.
 *
 * Each bit index is a 4-byte quantity. To reduce the memory
 * requirements of the class, the bits are organized as segments,
 * where each segment contains bit indices that share the two high bytes.
 * The bitsegment implementation stores only the lower 2 bytes per index.
 ***************************************************************************/

#ifdef __64BIT__
typedef uint64_t SparseBitIndex;
#else /* ! __64BIT__ */
typedef uint32_t SparseBitIndex;
#endif

template <class Allocator>
class ASparseBitVector : private Allocator {
  class SparseBitRef;
  public:

  static const SparseBitIndex kSegmentBits = 16;
  static const SparseBitIndex kSegmentMask = 0xFFFF;

  // Constructor defaults to a vector of one BitWord
  ASparseBitVector (const Allocator &a = Allocator()) : Allocator(a), fBase(NULL), fNumberOfSegments(0) { }

  // Copy constructor
  ASparseBitVector (const ASparseBitVector &v) :
    Allocator(v), fBase(NULL), fNumberOfSegments(v.fNumberOfSegments) {
    size_t n = fNumberOfSegments;

    if (n) {
      fBase = (Segment *)Allocator::allocate(n * sizeof(Segment));
      for (size_t i=0; i< n; i++)
        fBase[i].copy(v.fBase[i], *this);
    } else {
      // sets to equiv zero set as v - either null or empty
      fBase = v.fBase;
    }
  }

  // Destructor will free bit vector storage
  ~ASparseBitVector () {
    ClearToNull();
  }

  static bool hasFastRandomLookup() {
    return false;
  }

  Allocator& allocator() { return *this;}

  // Assign one sparse bit vector to another
  ASparseBitVector & operator= (const ASparseBitVector<Allocator> &);

  template <class A2>
  ASparseBitVector & operator= (const ASparseBitVector<A2> &);

  template <class B2>
  ASparseBitVector & operator= (const B2 &);

  // Check if two vectors are the same
  bool operator== (const ASparseBitVector &) const;
  bool operator!= (const ASparseBitVector &) const;
  bool operator<= (const ASparseBitVector &) const;

  //Bitwise operations
  template <class B2>
  ASparseBitVector & operator|= (const B2 &);
  template <class B2>
  ASparseBitVector & operator&= (const B2 &);
  template <class B2>
  ASparseBitVector & operator-= (const B2 &);

  // And from an arbitrary vector representation
  template<class B2> bool And(const B2 &vector);
  // Or from an arbitrary vector representation
  template<class B2> void Or(const B2 &abv);
  template<class ACursor>    void OrCursor(ACursor &abvc);

  // Or from an array of vectors
  void Or(ASparseBitVector** vs, SparseBitIndex nvs);
  void Or(ASparseBitVector** vs, SparseBitIndex nvs, const ASparseBitVector &mask);

  // Free any unused storage
  void Compact();

  // Indexing operator, grow if necessary
  SparseBitRef operator[] (size_t index);
  bool operator[] (size_t index) const;

  // Indexing operator, do not grow
  bool ValueAt(size_t index) const;

  // Get first bit set to one
  SparseBitIndex FirstOne() const;

  // if there is a bit in the specified range, clear the highest one and set
  // foundone to true.  otherwise, set foundone to false
  SparseBitIndex ClearLastOneIfThereIsOneInRange(SparseBitIndex low, SparseBitIndex high, bool& foundone);

  // Get last bit set to one
  SparseBitIndex LastOne() const;

  bool hasBitWordRepresentation() const { return false; } // returns whether this bit vector is based on an array of bitWords (which it is not)
  int32_t FirstOneWordIndex() const { CS2Assert(false, ("This method is not implemented")); return 0; }
  int32_t LastOneWordIndex() const { CS2Assert(false, ("This method is not implemented")); return 0; }
  uint32_t wordSize() const { CS2Assert(false, ("This method is not implemented")); return 0; }
  uint32_t WordAt (uint32_t wordIndex) const { CS2Assert(false, ("This method is not implemented")); return 0; }

  // A growth routine to maintain compatibility with the dense bit vector
  void GrowTo(SparseBitIndex index, bool geometric = true, bool forceGeometric = false);

  // Logical operations in-place and with output vector.
  // Return value indicates if the target vector was changed.
  bool And(const ASparseBitVector &inputVector);
  bool And(const ASparseBitVector &inputVector,
                 ASparseBitVector &outputVector) const;
  bool Andc(const ASparseBitVector &inputVector);
  template <class B2> bool Andc(const B2 &inputVector);

  bool Andc(const ASparseBitVector &inputVector,
                  ASparseBitVector &outputVector) const;
  bool Or(const ASparseBitVector &inputVector);
  bool Or(const ASparseBitVector &inputVector,
                ASparseBitVector &outputVector) const;

  bool OrMask(const ASparseBitVector &inputVector,
              const ASparseBitVector &maskVector);

  // We will not implement COMPLEMENT and OR-WITH-COMPLEMENT since they will
  // by definition produce DENSE vectors when given sparse vectors on input.
  //
  // void Orc     (const ASparseBitVector &inputVector);
  // void Orc     (const ASparseBitVector &inputVector,
  //               ASparseBitVector &outputVector) const;
  // void Comp    ();
  // void Comp    (ASparseBitVector &outputVector) const;

  // EXCLUSIVE-OR and generic logical operations not currently implemented.
  // If there is a good reason, this can be done.
  //
  // void Xor     (const ASparseBitVector &inputVector);
  // void Xor     (const ASparseBitVector &inputVector,
  //               ASparseBitVector &outputVector) const;
  // void Logical (const ASparseBitVector &inputVector, SparseBitIndex map);
  // void Logical (const ASparseBitVector &inputVector,
  //               ASparseBitVector &outputVector,
  //               SparseBitIndex map) const;

  // Dump routine
  template <class str> void Dump(str &) const;

  // Clear all bits in the vector (ie. clear and discard memory), and make empty.
  void Clear ();

  // Clear all bits in the vector (ie. clear and discard memory), and make null.
  void ClearToNull ();

  // deprecated. use Clear.
  void Truncate() { Clear();}

  // Return the index of the first zero bit in the vector
  SparseBitIndex LowestZero() const;

  // Return the index of the first one bit in the vector
  SparseBitIndex LowestOne() const;

  // Determine if the vector is cleared, ie is the empty set
  bool IsZero() const;

  // Determine if the vector is cleared to null, ie is the zero measure set "null set" which is also the empty set.
  bool IsNull() const;

  // Determine if the intersection of this vector and the input vector
  // is non-zero
  template <class B2>
  bool Intersects (const B2 &inputVector) const;

  bool Intersects (const ASparseBitVector &inputVector) const;

  // Determine if the intersection of this vector and the input vector
  // under the given mask is non-zero
  bool IntersectsWithMask (const ASparseBitVector &inputVector,
                              const ASparseBitVector &maskVector) const;

  // Return the number of bytes of memory used by this bit vector
  unsigned long MemoryUsage() const;

  // Return the size in bits of the vector
  SparseBitIndex SizeInBits() const;

  // Return the number of '1' bits in the vector.  Optionally specify the
  // number of leading bits to examine.
  SparseBitIndex PopulationCount (SparseBitIndex numBits = 0xEFFFFFFFul) const;
  SparseBitIndex PopulationCount (const ASparseBitVector &mask) const;

 private:
  class Segment {
  public:
    Segment() : fSegment(NULL), fSize(0), fHighBits(0), fNumValues(0) { }
    void allocate(size_t size, Allocator &a) {
      fSegment = (uint16_t *) a.allocate(size * sizeof(uint16_t));
      fSize = size;
      fNumValues = 0;
    }
    template <class S2>
    void copy(S2 &s, Allocator &a)  {
      allocate(s.PopulationCount(), a);
      fHighBits = s.fHighBits;
      fNumValues = s.PopulationCount();
      memcpy(fSegment, s.Indices(), fNumValues * sizeof(uint16_t));
    }

    // append elements of s starting at fromIndex to toIndex-1 to this Segment
    template <class S2>
    void append(S2 &s, uint16_t fromIndex, uint16_t toIndex) {
      size_t n = toIndex - fromIndex;
      memcpy(&fSegment[fNumValues], &s.Indices()[fromIndex], n * sizeof(uint16_t));
      fNumValues += n;
    }

    // append elements of s to this Segment
    template <class S2>
    void append(S2 &s) {
      append(s, 0, s.PopulationCount());
    }

    void append(uint16_t v) {
      fSegment[fNumValues++] = v;
    }

    void adopt(size_t numValues, size_t size, uint16_t *bits) {
      fSegment = bits;
      fSize = size;
      fNumValues = numValues;
    }

    void adopt(uint16_t highbits, size_t numValues, size_t size, uint16_t *bits) {
      fHighBits = highbits;
      adopt(numValues, size, bits);
    }

    // shallow swap of this with s (cannot override Swap method in cs2.h as Segment is private)
    void swap(Segment &s) {
       uint16_t *data     = fSegment;
       uint16_t size      = fSize;
       uint16_t highBits  = fHighBits;
       uint16_t numValues = fNumValues;
       adopt(s.fHighBits, s.fNumValues, s.fSize, s.fSegment);
       s.adopt(highBits, numValues, size, data);
    }


    void reallocate(size_t size, Allocator &a) {
      fSegment = (uint16_t *) a.reallocate(size*sizeof(uint16_t),
                                           fSegment,
                                           fSize * sizeof(uint16_t));
      fSize = size;
    }

    void deallocate(Allocator &a) { a.deallocate(fSegment, fSize*sizeof(uint16_t)); }

    SparseBitIndex AllocatedSize() const { return SparseBitIndex(fSize); }
    SparseBitIndex HighBits() const { return SparseBitIndex(fHighBits)<<16; }
    SparseBitIndex IsZero() const { return fNumValues==0;}
    SparseBitIndex PopulationCount() const { return fNumValues;}

    SparseBitIndex FirstOne() const { return HighBits() + fSegment[0]; }
    SparseBitIndex LastOne() const { return HighBits() + fSegment[fNumValues-1]; }

    uint16_t *Indices() const { return fSegment; }

    bool operator==(const struct Segment &s1) const {
      if (fHighBits != s1.fHighBits) return false;
      if (fNumValues != s1.fNumValues) return false;

      return !memcmp(fSegment, s1.fSegment, sizeof(uint16_t)*PopulationCount());
    }

    size_t MemoryUsage() {
      return sizeof(Segment)+fSize*sizeof(uint16_t);
    }
  //  TODO: need to add destructor, then change everywhere that deallocates to rely on this destructor

  private:
    uint16_t *fSegment;
    uint16_t fSize;
  public:
    uint16_t fHighBits;
    uint32_t fNumValues;
  };

  Segment *fBase;
  SparseBitIndex fNumberOfSegments;

  Segment *FindSegment(SparseBitIndex index) const;
  Segment *AddSegment(SparseBitIndex index, SparseBitIndex count);
  Segment *AddSegment(SparseBitIndex index, SparseBitIndex count, uint16_t *bits);
  Segment *OrSegment(SparseBitIndex index, SparseBitIndex count, uint16_t *bits);
  void RemoveSegment(SparseBitIndex index);
  void RemoveSegmentAt(SparseBitIndex segmentindex);

  SparseBitIndex FindIndex(const Segment &, uint16_t, SparseBitIndex l=0, SparseBitIndex h=0) const;
  SparseBitIndex AdvanceIndex(const Segment &, uint16_t, SparseBitIndex l=0, SparseBitIndex h=0) const;

  // Grow thisSegment to at least the given size.
  //
  // if numValues is given, then it must accommodate numValues. Thus if numValues exceeds given size then grow size by some factor more.
  // The new size is then guaranteed to exceed numValues.

  void GrowSegment(Segment &thisSegment, uint32_t size, uint32_t numValues = 0);
  void SetSegment(Segment &, SparseBitIndex);
  void ClearSegment(Segment &, SparseBitIndex);
  bool GetSegment(Segment &, SparseBitIndex) const;
  SparseBitIndex ValueAtSegment(const Segment &, SparseBitIndex) const;
  void TruncateSegment(Segment &);

  void CompactSegment(Segment &);

  template <class S2>
  void CopySegment(Segment &,const S2 &);

  bool OrSegment(Segment &, const Segment &);
  bool OrSegment(const Segment &, const Segment &, Segment &);
  bool OrSegmentMask(Segment &, const Segment &, const Segment &);
  bool AndSegment(Segment &, const Segment &);
  bool AndSegment(const Segment &, const Segment &, Segment &);
  bool AndcSegment(Segment &, const Segment &);
  bool AndcSegment(const Segment &, const Segment &, Segment &);

  bool IsSubset(const Segment &thisSegment, const Segment &inputSegment ) const;

  template <class str> void DumpSegment(str &out, const Segment &vector) const;


  class SparseBitRef {
  public:

    SparseBitRef (ASparseBitVector &vector, SparseBitIndex index) : fIndex(index), fVector(vector) {}
    // ~SparseBitRef()  destructor intentionally omitted
    SparseBitRef (const SparseBitRef &r) : fIndex(r.fIndex), fVector(r.fVector) {}

    operator bool();
    SparseBitRef& operator= (bool value);
  private:

    // should not be assigned
    SparseBitRef &operator= (const SparseBitRef&);

    // set/clear with shifts. To be used only from operator=
    SparseBitRef &Set();
    SparseBitRef &Clear ();

    SparseBitIndex         fIndex;
    ASparseBitVector &fVector;
  };

public:
  class Cursor {
    public:

    Cursor (const ASparseBitVector &adoptVector);
    Cursor (const Cursor &adoptCursor);
    // ~Cursor();  // destructor intentionally omitted

    void SetToFirstOne();
    void SetToLastOne();
    bool SetToNextOne();
    bool SetToNextOneAfter(SparseBitIndex);
    bool SetToPreviousOne();

    bool Valid() const;
    operator SparseBitIndex() const;

    Cursor &operator= (SparseBitIndex);

    private:

    const ASparseBitVector      &fVector;
    const uint16_t              *fSegment;
    uint32_t                     fSegmentPopCount;
    uint32_t                     fSegmentHighBits;
    uint32_t                     fSegmentIndex;
    uint32_t                     fSegmentOffset;
    Cursor &operator= (const Cursor &adoptCursor);
  };

  class IntersectionCursor {
    public:

    IntersectionCursor (const ASparseBitVector &adoptVector1,
                        const ASparseBitVector &adoptVector2) :
    c1(adoptVector1), c2(adoptVector2) {}
    IntersectionCursor (const IntersectionCursor &adoptCursor) :
    c1(adoptCursor.c1), c2(adoptCursor.c2) {}

    // ~Cursor();  // destructor intentionally omitted

    bool SetToFirstOne() {
      c1.SetToFirstOne();
      c2.SetToFirstOne();

      if (Valid())
        return FindNextCommonItem();
      return false;
    }
    bool SetToNextOne() {
      if (c1<=c2) {
        if (!c1.SetToNextOne()) return false;
      } else if (!c2.SetToNextOne()) return false;
      return FindNextCommonItem();
    }
    bool SetToNextOneAfter(SparseBitIndex);
    bool SetToPreviousOne();

    bool Valid() const {
      return c1.Valid() && c2.Valid();
    }
    operator SparseBitIndex() const {
      return c1;
    }

    IntersectionCursor &operator= (SparseBitIndex);

    private:
    bool FindNextCommonItem() {
      while (c1!=c2) {
        if (c1<c2)  {
          if (!c1.SetToNextOneAfter(c2-1)) return false;
        } else
          if (!c2.SetToNextOneAfter(c1-1)) return false;
      }
      return true;
    }

    Cursor c1,c2;
  };

  class MaskCursor {
    public:

    MaskCursor (const ASparseBitVector &adoptVector,
                const ASparseBitVector &maskVector) :
        c(adoptVector), m(maskVector) {}
    MaskCursor (const MaskCursor &adoptCursor) :
        c(adoptCursor.c), m(adoptCursor.m) {}

    // ~Cursor();  // destructor intentionally omitted

    bool SetToFirstOne() {
      c.SetToFirstOne();
      m.SetToFirstOne();

      if (c.Valid()) return FindNextMaskedItem();
      return false;
    }
    bool SetToNextOne() {
      if (c.SetToNextOne()) return FindNextMaskedItem();
      return false;
    }
    bool SetToNextOneAfter(SparseBitIndex);
    bool SetToPreviousOne();

    bool Valid() const {
      return c.Valid();
    }
    operator SparseBitIndex() const {
      return c;
    }

    IntersectionCursor &operator= (SparseBitIndex);

    private:
    bool FindNextMaskedItem() {
      if (m.Valid()) {
        while (c>=m) {
          if (c==m) {
            if (!c.SetToNextOne()) return false;
          }
        if (!m.SetToNextOneAfter(c-1))
          break;
        }
      }
      return true;
    }

    Cursor c,m;
  };

  template <class A2>
    friend class ASparseBitVector;

  template <class A2>
  friend
    inline void Swap(ASparseBitVector<A2> &vectorA, ASparseBitVector<A2> &vectorB);
};

// SparseBitRef methods
template <class Allocator>
inline ASparseBitVector<Allocator>::SparseBitRef::operator bool() {
  return fVector.ValueAt(fIndex);
}

template <class Allocator>
inline typename ASparseBitVector<Allocator>::SparseBitRef&  ASparseBitVector<Allocator>::SparseBitRef::operator= (bool bitValue) {
  CS2Assert (bitValue == 0 || bitValue == 1, ("Incorrect bool value"));
  ASparseBitVector<Allocator> &bitArray = fVector;

  if (bitValue) return Set();
  else return Clear();
}

template <class Allocator>
inline typename ASparseBitVector<Allocator>::SparseBitRef&  ASparseBitVector<Allocator>::SparseBitRef::Set() {
  size_t i=0, n = fVector.fNumberOfSegments;

  uint16_t highBits = fIndex>>16;
  Segment *base;
  if (n) {
    base = fVector.fBase;
    for (i=0; i< n; i++) {
      if (base[i].fHighBits >= highBits) {
        if (base[i].fHighBits == highBits) {
          fVector.SetSegment(base[i], fIndex);
          return *this;
        }
        break;
      }
    }
    base = (Segment *)fVector.reallocate((n+1)*sizeof(Segment),
                                       base, n*sizeof(Segment));
    memmove(&base[i+1],&base[i],(n-i)*sizeof(Segment));
  } else {
    base = (Segment *)fVector.allocate((n+1)*sizeof(Segment));
  }

  base[i].allocate(sizeof(size_t)/sizeof(uint16_t), fVector);
  base[i].fNumValues=1;
  base[i].fHighBits=highBits;
  base[i].Indices()[0]=fIndex;

  fVector.fBase = base;
  fVector.fNumberOfSegments=n+1;

  return *this;
}

template <class Allocator>
inline typename ASparseBitVector<Allocator>::SparseBitRef&  ASparseBitVector<Allocator>::SparseBitRef::Clear () {
  typename ASparseBitVector<Allocator>::Segment *segment = fVector.FindSegment(fIndex);
  if (segment) {
    fVector.ClearSegment(*segment, fIndex);
    if (segment->IsZero()) { fVector.RemoveSegment(fIndex); }
  }

  return *this;
}

// ASparseBitVector<Allocator>::operator= (ASparseBitVector)
//
// Assign a sparse bit vector to another.
template <class Allocator>
inline ASparseBitVector<Allocator> &
ASparseBitVector<Allocator>::operator= (const ASparseBitVector<Allocator> &vector) {
  if (vector.IsNull())
    ClearToNull();
  else
    Clear();
  if (vector.fNumberOfSegments) {
    fNumberOfSegments = vector.fNumberOfSegments;
    fBase = (Segment *)Allocator::allocate(fNumberOfSegments * sizeof(Segment));
    SparseBitIndex i;
    for (i=0; i< fNumberOfSegments; i++)
      fBase[i].copy(vector.fBase[i], *this);
  }
  return *this;
}

//
// Swap one sparse bit vector with another.
template <class Allocator>
inline void Swap(ASparseBitVector<Allocator> &vectorA, ASparseBitVector<Allocator> &vectorB) {
  typename ASparseBitVector<Allocator>::Segment *base;
  SparseBitIndex numberOfSegments;

  base = vectorA.fBase;
  numberOfSegments = vectorA.fNumberOfSegments;
  vectorA.fBase = vectorB.fBase;
  vectorA.fNumberOfSegments = vectorB.fNumberOfSegments;
  vectorB.fBase = base;
  vectorB.fNumberOfSegments = numberOfSegments;
}

template <class Allocator>
template <class A2>
inline ASparseBitVector<Allocator> &
ASparseBitVector<Allocator>::operator= (const ASparseBitVector<A2> &vector) {
  if (vector.IsNull())
    ClearToNull();
  else
    Clear();
  if (vector.fNumberOfSegments) {
    fNumberOfSegments = vector.fNumberOfSegments;
    fBase = (Segment *)Allocator::allocate(fNumberOfSegments * sizeof(Segment));
    memset(fBase, 0, fNumberOfSegments * sizeof(Segment));
    SparseBitIndex i;
    for (i=0; i< fNumberOfSegments; i++)
      fBase[i].copy(vector.fBase[i], *this);
  }
  return *this;
}

template <class Allocator>
template <class B2>
inline ASparseBitVector<Allocator> &
ASparseBitVector<Allocator>::operator= (const B2 &vector) {
  Clear();

  typename B2::Cursor c1(vector), c2(vector);

  c1.SetToFirstOne();
  c2.SetToFirstOne();

  while (c1.Valid()) {
    // new segment
    SparseBitIndex highbits = c1 &~kSegmentMask, count=1;

    for (c1.SetToNextOne();
         c1.Valid() && ((c1&~kSegmentMask) == highbits);
         c1.SetToNextOne()) {
      count++;
    }

    Segment *s = AddSegment(highbits, count);
    uint16_t *sp = s->Indices();
    s->fNumValues = count;
    while (count > 0) {
      *sp++ = c2;
      c2.SetToNextOne();
      count-=1;
    }
  }
  return *this;
}

// ASparseBitVector<Allocator>::Compact
// Release any unused memory

template <class Allocator>
inline void ASparseBitVector<Allocator>::Compact() {
  size_t i;
  for (i=0; i< fNumberOfSegments; i++)
    CompactSegment(fBase[i]);
}

// ASparseBitVector<Allocator>::operator[]
//
// Indexing operator, grow if necessary

template <class Allocator>
  inline typename ASparseBitVector<Allocator>::SparseBitRef ASparseBitVector<Allocator>::operator[] (size_t index) {
  return SparseBitRef (*this, index);
}

template <class Allocator>
inline bool ASparseBitVector<Allocator>::operator[] (size_t index) const {
  return ValueAt(index);
}

template <class Allocator>
inline bool
ASparseBitVector<Allocator>::ValueAt(size_t elementIndex) const
{
  Segment *s =FindSegment(elementIndex);
  if (s) return GetSegment(*s, elementIndex);
  return false;
}

template <class Allocator>
inline SparseBitIndex
ASparseBitVector<Allocator>::FirstOne() const
{
  CS2Assert(!IsZero(), ("Could not find any 1-bits looking for FirstOne"));

  Segment &s = fBase[0];
  return s.HighBits() | s.Indices()[0];
}

template <class Allocator>
inline SparseBitIndex
ASparseBitVector<Allocator>::LastOne() const
{
  CS2Assert(!IsZero(), ("Could not find any 1-bits looking for LastOne"));

  Segment &s = fBase[fNumberOfSegments-1];
  return s.HighBits() | s.Indices()[s.fNumValues-1];
}

template <class Allocator>
inline SparseBitIndex
ASparseBitVector<Allocator>::ClearLastOneIfThereIsOneInRange(SparseBitIndex low, SparseBitIndex high, bool& foundone)
{
  if (IsZero()) {
     foundone = false;
     return ~(SparseBitIndex) 0;
  } else {
     Segment &s = fBase[fNumberOfSegments-1];
     SparseBitIndex ret = s.HighBits() | s.Indices()[s.fNumValues-1];
     CS2Assert(ret <= high, ("sparsebitvector and tableof disagree on highbounds"));
     if (s.fNumValues<=1)
       RemoveSegmentAt(fNumberOfSegments-1);
     else
       s.fNumValues-=1;
     foundone = true;
     return ret;
  }
}


// ASparseBitVector<Allocator>::GrowTo
//
// A growth routine to maintain compatibility with the dense bit vector
// class.  This routine does nothing except make non-null.

template <class Allocator>
inline void ASparseBitVector<Allocator>::GrowTo (SparseBitIndex index, bool geometric, bool forceGeometric) {
  if (IsZero()) {
    Clear();
    return;
  }
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::Clear () {

  if (IsZero()) {
    // unallocated, but non-null
    fBase = (Segment *) 1;
    return;
  }
  size_t i, n = fNumberOfSegments;

  for (i=0; i<n; i++) {
    Segment *s = &fBase[i];
    s->deallocate(*this);
  }
  Allocator::deallocate(fBase, sizeof(Segment)*fNumberOfSegments);
  fNumberOfSegments=0;
  // unallocated, but non-null
  fBase = (Segment *) 1;
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::ClearToNull () {
  Clear();
  fBase=NULL;
}

template <class Allocator>
inline ASparseBitVector<Allocator>::Cursor::Cursor (const ASparseBitVector<Allocator> &adoptVector) :
  fVector(adoptVector) {}

template <class Allocator>
inline ASparseBitVector<Allocator>::Cursor::Cursor (const typename ASparseBitVector<Allocator>::Cursor &adoptCursor) :
  fVector(adoptCursor.fVector),
  fSegment(adoptCursor.fSegment),
  fSegmentPopCount(adoptCursor.fSegmentPopCount),
  fSegmentHighBits(adoptCursor.fSegmentHighBits),
  fSegmentIndex(adoptCursor.fSegmentIndex),
  fSegmentOffset(adoptCursor.fSegmentOffset) {}

template <class Allocator>
inline bool ASparseBitVector<Allocator>::Cursor::Valid () const {
  return fSegmentIndex < fVector.fNumberOfSegments;
}

template <class Allocator>
inline ASparseBitVector<Allocator>::Cursor::operator SparseBitIndex() const {
  CS2Assert(Valid(), ("Expecting valid cursor"));
  return fSegmentHighBits | fSegment[fSegmentOffset];
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::Cursor::SetToFirstOne() {
  fSegmentIndex=0;
  fSegmentOffset=0;
  if (Valid()) {
    Segment &s=fVector.fBase[fSegmentIndex];
    fSegment = s.Indices();
    fSegmentPopCount = s.PopulationCount()-1;
    fSegmentHighBits = s.HighBits();
  }
}

template <class Allocator>
inline bool ASparseBitVector<Allocator>::Cursor::SetToNextOne() {
  if (fSegmentOffset < fSegmentPopCount) {
    // Same segment;
    fSegmentOffset+=1;
    return true;
  }

  CS2Assert(Valid(), ("Cannot advance an invalid cursor"));
  CS2Assert(fSegmentPopCount == fVector.fBase[fSegmentIndex].PopulationCount()-1,
    ("ASparseBitVector updated durign traversal %d %d", fSegmentPopCount, fVector.fBase[fSegmentIndex].PopulationCount()-1));
  fSegmentIndex+=1;
  if (!Valid()) return false;
  fSegmentOffset=0;
  Segment &s=fVector.fBase[fSegmentIndex];
  fSegment = s.Indices();
  fSegmentPopCount = s.PopulationCount()-1;
  fSegmentHighBits = s.HighBits();
  return true;
}

template <class Allocator>
inline bool ASparseBitVector<Allocator>::Cursor::SetToPreviousOne() {
  CS2Assert(Valid(), ("Cannot advance an invalid cursor"));

  if (fSegmentOffset > 0) {
    // Same segment;
    fSegmentOffset-=1;
    return true;
  }
  else {
    fSegmentIndex-=1;
    if (!Valid()) return false;
    Segment &s=fVector.fBase[fSegmentIndex];
    fSegment = s.Indices();
    fSegmentPopCount = s.PopulationCount()-1;
    fSegmentHighBits = s.HighBits();
    fSegmentOffset = fSegmentPopCount;
    return true;
  }
}

template <class Allocator>
inline bool  ASparseBitVector<Allocator>::Cursor::SetToNextOneAfter(SparseBitIndex next) {
  CS2Assert(Valid(), ("Cannot advance an invalid cursor"));

  // Find one more than next
  next+=1;
  // Find segment
  SparseBitIndex index=fSegmentOffset;
  SparseBitIndex nexthi = next & ~kSegmentMask;
  SparseBitIndex nextlo = next & kSegmentMask;

  while (fSegmentHighBits < nexthi) {
    fSegmentIndex+=1;
    if (!Valid()) return false;
    index=0;
    Segment &s = fVector.fBase[fSegmentIndex];
    fSegment = s.Indices();
    fSegmentPopCount = s.PopulationCount()-1;
    fSegmentHighBits = s.HighBits();
  }
  if (fSegmentHighBits == nexthi) {
    Segment &s = fVector.fBase[fSegmentIndex];
    index = fVector.AdvanceIndex(s, nextlo, index);
    if (index > fSegmentPopCount) {
      fSegmentIndex+=1;
      if (!Valid()) return false;
      index=0;
      Segment &s = fVector.fBase[fSegmentIndex];
      fSegment = s.Indices();
      fSegmentPopCount = s.PopulationCount()-1;
      fSegmentHighBits = s.HighBits();
    }
  } else
    index=0;
  fSegmentOffset=index;
  return true;
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::Cursor::SetToLastOne() {
  if (fVector.fNumberOfSegments>0) {
    fSegmentIndex=fVector.fNumberOfSegments-1;
    Segment &s = fVector.fBase[fSegmentIndex];

    fSegment=s.Indices();
    fSegmentPopCount=fSegmentOffset=s.PopulationCount()-1;
    fSegmentHighBits=s.HighBits();
  } else
    fSegmentIndex=0;
}

// Return the number of '1' bits in the vector.  Optionally specify the
// number of leading bits to examine.
template <class Allocator>
inline SparseBitIndex ASparseBitVector<Allocator>::PopulationCount (SparseBitIndex numBits) const{
  CS2Assert(numBits==0xEFFFFFFFul,("Population count subset not implemented"));

  size_t i, n = fNumberOfSegments, ret=0;
  for (i=0; i<n; i++) {
    Segment *s = &fBase[i];
    ret += s->PopulationCount();
  }
  return ret;
}

template <class Allocator>
inline SparseBitIndex ASparseBitVector<Allocator>::PopulationCount (const ASparseBitVector<Allocator> &mask) const{
  SparseBitIndex ret=0;

  typename ASparseBitVector<Allocator>::IntersectionCursor c(*this, mask);
  for (c.SetToFirstOne(); c.Valid(); c.SetToNextOne())
    ret+=1;
  return ret;
}

// ASparseBitVector<Allocator>::SizeInBits
//
// Return the number of bits "allocated" in this vector.

template <class Allocator>
inline SparseBitIndex ASparseBitVector<Allocator>::SizeInBits() const {
  return PopulationCount();
}

// ASparseBitVector<Allocator>::IsZero
//
// Determine if this vector is zero

template <class Allocator>
inline bool ASparseBitVector<Allocator>::IsZero() const {
  return fNumberOfSegments==0;
}

// ASparseBitVector<Allocator>::IsNull
//
// Determine if this vector is null (and also is zero)

template <class Allocator>
inline bool ASparseBitVector<Allocator>::IsNull() const {
  return (fNumberOfSegments==0) && (fBase == NULL);
}

template <class Allocator>
inline typename ASparseBitVector<Allocator>::Segment *ASparseBitVector<Allocator>::FindSegment(SparseBitIndex index) const {
  size_t i, n = fNumberOfSegments;
  for (i=0; i< n; i++) {
    if (fBase[i].fHighBits >= index>>16) {
      if (fBase[i].fHighBits == index>>16) return &fBase[i];
      return NULL;
    }
  }
  return NULL;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::Intersects (const ASparseBitVector<Allocator> &inputVector) const{

  size_t i1=0,i2=0,n1 = fNumberOfSegments, n2 = inputVector.fNumberOfSegments;

  if (n1==0 || n2==0) return false;

  while (i1<n1 && i2<n2) {
    Segment *s1 = &fBase[i1];
    Segment *s2 = &inputVector.fBase[i2];
    if (s1->fHighBits==s2->fHighBits) {
      SparseBitIndex j1=0, j2=0;
      SparseBitIndex m1=s1->fNumValues-1, m2=s2->fNumValues-1;

      if ((s1->Indices()[0] <= s2->Indices()[m2]) &&
          (s2->Indices()[0] <= s1->Indices()[m1])) {
        do {
          SparseBitIndex v1=s1->Indices()[j1];
          SparseBitIndex v2=s2->Indices()[j2];

          if (v1==v2) return true;
          else if (v1<v2) {
            SparseBitIndex diff = v2-v1;
            if (j1+diff>m1) diff=m1-j1;

            while (diff>0) {
              if (s1->Indices()[j1+diff]==v2) return true;
              if (s1->Indices()[j1+diff]<v2) break;
              diff-=1;
            }
            j1+=diff+1;
          } else{
            SparseBitIndex diff = v1-v2;
            if (j2+diff>m2) diff=m2-j2;

            while (diff>0) {
              if (s2->Indices()[j2+diff]==v1) return true;
              if (s2->Indices()[j2+diff]<v1) break;
              diff-=1;
            }
            j2+=diff+1;
          }
        } while (j1<=m1 && j2<=m2);
      }
      i1+=1; i2+=1;
    } else if (s1->fHighBits<s2->fHighBits) {
      i1+=1;
    } else
      i2+=1;
  }

  return false;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::operator!=(const ASparseBitVector& inputVector) const{
  return !(*this == inputVector);
}

// FindIndex gets the segment location of SparseBitIndex.
// If SparseBitIndex does not appear in the segment, finds the
// smallest bit in the segment that is larger or equal than search
template <class Allocator>
inline SparseBitIndex ASparseBitVector<Allocator>::FindIndex(const typename ASparseBitVector<Allocator>::Segment &thisSegment, uint16_t search, SparseBitIndex l, SparseBitIndex h) const{
  size_t low =l, high=h;
  CS2Assert(thisSegment.Indices(), ("Expecting non-null srbv segment"));
  if (high==0) high=thisSegment.fNumValues-1;

  if (search <= thisSegment.Indices()[low]) return low;
  if (search == thisSegment.Indices()[high]) return high;
  if (search > thisSegment.Indices()[high]) return high+1;

  while (high-low > 16) {
    size_t mid = (high+low)/2;
    if (search < thisSegment.Indices()[mid]) high = mid;
    else if (search > thisSegment.Indices()[mid]) low = mid;
    else return mid;
  }

  while (low<high && thisSegment.Indices()[low] < search) low+=1;
  return low;
}

// FindIndex gets the segment location of SparseBitIndex.
// If SparseBitIndex does not appear in the segment, finds the
// smallest bit in the segment that is larger or equal than search
template <class Allocator>
inline SparseBitIndex ASparseBitVector<Allocator>::AdvanceIndex(const typename ASparseBitVector<Allocator>::Segment &thisSegment, uint16_t s, SparseBitIndex l, SparseBitIndex h) const {
  size_t search(s), low(l), high(h);
  if (high==0) high=thisSegment.fNumValues-1;

  if (search >= thisSegment.Indices()[high]) {
    if (search == thisSegment.Indices()[high]) return high;
    return high+1;
  }
  uint16_t value = thisSegment.Indices()[low];
  if (search <= value) return low;
  if ( long(search - value) + low < high)
    high = long(search - value) + low;

  const unsigned long residue = 128; /* do linear search under this range */
  size_t mid = (high+low)/2;

  while (high-low > residue) {
    value = thisSegment.Indices()[mid];

    if (value==search) return mid;
    else if (value>search) {
      high = mid;
    } else { /* value < search */
      low = mid;
      if (long(search-value)+low < high) /* trim range based on search value */
        high = long(search-value) + low;
    }
    mid = (high+low)/2;
  }
  while (low+3<=high) {
    if (thisSegment.Indices()[low]>=search  ) return low;
    if (thisSegment.Indices()[low+1]>=search) return low+1;
    if (thisSegment.Indices()[low+2]>=search) return low+2;
    if (thisSegment.Indices()[low+3]>=search) return low+3;
    low+=4;
  }
  if (low>high ||
      thisSegment.Indices()[low]>=search) return low;
  if (low+1>high ||
      thisSegment.Indices()[low+1]>=search) return low+1;
  if (low+2>high ||
      thisSegment.Indices()[low+2]>=search) return low+2;
  return low+3;
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::GrowSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment, uint32_t size, uint32_t numValues) {
  if (numValues > 0) {
    if (numValues <= 1024) {
      size = 2*BitManipulator::FloorPowerOfTwo(numValues-1);
    } else {
      // instead of doubling increase linearly
      size = 1024 + 1024 * ((numValues-1) / 1024);
    }
  }
  if (thisSegment.AllocatedSize() > size) return;

  if (size > kSegmentMask) size = kSegmentMask;

  if (thisSegment.AllocatedSize() == 0)
    thisSegment.allocate(size, *this);
  else
    thisSegment.reallocate(size, *this);
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::SetSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                                    SparseBitIndex bit) {
  SparseBitIndex t=FindIndex(thisSegment, bit);

  if (t<thisSegment.fNumValues && bit==ValueAtSegment(thisSegment, t)) return;

  if (thisSegment.AllocatedSize()==thisSegment.fNumValues) GrowSegment(thisSegment, thisSegment.AllocatedSize(), thisSegment.fNumValues+1);

  memmove(&thisSegment.Indices()[t+1],
          &thisSegment.Indices()[t],
          (thisSegment.fNumValues-t)*sizeof(uint16_t));
  CS2Assert(thisSegment.fNumValues <thisSegment.AllocatedSize(),
            ("Segment Overflow %d(%d %d)>=%d", (thisSegment.fNumValues-1-t)+(t+1),
             thisSegment.fNumValues, t,
             thisSegment.AllocatedSize()));

  thisSegment.Indices()[t]=bit;
  CS2Assert(t<thisSegment.AllocatedSize(),
            ("Segment Overflow %d>=%d", t, thisSegment.AllocatedSize()));

  thisSegment.fNumValues+=1;
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::ClearSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                                      SparseBitIndex bit) {
  SparseBitIndex t = FindIndex(thisSegment, bit);

  if (t<thisSegment.fNumValues && bit==ValueAtSegment(thisSegment, t)) {
    thisSegment.fNumValues-=1;
    memmove(&thisSegment.Indices()[t],
       &thisSegment.Indices()[t+1],
       (thisSegment.fNumValues-t)*sizeof(uint16_t));
  }
}

template <class Allocator>
inline bool ASparseBitVector<Allocator>::GetSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment, SparseBitIndex bit) const{
  CS2Assert(thisSegment.Indices(), ("Expecting non-null srbv segment"));
  SparseBitIndex t = FindIndex(thisSegment, bit);

  if (t<thisSegment.fNumValues)
    return uint16_t(bit)==thisSegment.Indices()[t];
  return 0;
}

template <class Allocator>
inline SparseBitIndex ASparseBitVector<Allocator>::ValueAtSegment(const typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                                             SparseBitIndex i) const{
  CS2Assert(thisSegment.Indices(), ("Expecting non-null srbv segment"));

  if (i<thisSegment.fNumValues)
    return thisSegment.HighBits() | thisSegment.Indices()[i];
  return 0;
}

template <class Allocator>
inline typename ASparseBitVector<Allocator>::Segment *ASparseBitVector<Allocator>::AddSegment(SparseBitIndex index, SparseBitIndex count) {

  size_t i=0, n = fNumberOfSegments;

  Segment *base;

  if (n) {
    base = fBase;
    for (i=0; i< n; i++) {
      if (base[i].fHighBits >= index>>16) {
        if (base[i].fHighBits == index>>16) {
          GrowSegment(base[i], count);
          return &base[i];
        }
        break;
      }
    }
    base = (Segment *) Allocator::reallocate((n+1) * sizeof(Segment),
                        base, n*sizeof(Segment));
    memmove(&base[i+1], &base[i], (n-i) * sizeof(Segment));
  } else {
    base = (Segment *) Allocator::allocate((n+1) * sizeof(Segment));
  }
 init:
  base[i].allocate(count, *this);
  base[i].fHighBits = index >> 16;
  base[i].fNumValues = 0;

  fBase = base;
  fNumberOfSegments = n+1;

  return &base[i];
}

template <class Allocator>
inline typename ASparseBitVector<Allocator>::Segment *ASparseBitVector<Allocator>::AddSegment(SparseBitIndex index, SparseBitIndex count, uint16_t *bits) {

  typename ASparseBitVector<Allocator>::Segment *s = AddSegment(index, count);
  s->fNumValues = count;
  memcpy(s->Indices(), bits, count * sizeof(uint16_t));

  return s;
}

template <class Allocator>
inline typename ASparseBitVector<Allocator>::Segment *ASparseBitVector<Allocator>::OrSegment(SparseBitIndex index, SparseBitIndex count, uint16_t *bits) {
  CS2Assert(count>0 && count <= kSegmentMask,
            ("OrSegment count OOB=%d", count));

  typename ASparseBitVector<Allocator>::Segment *s = FindSegment(index);

  if (s) {
    Segment s2;
    s2.adopt(index>>16, count, count, bits);

    OrSegment(*s, s2);
    return s;
  } else
    return AddSegment(index, count, bits);
}

template <class Allocator>
template <class S2>
inline void ASparseBitVector<Allocator>::CopySegment(typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                                     const S2 &inputSegment) {
  CS2Assert(thisSegment.HighBits()==inputSegment.HighBits(),
            ("Copying segments with diff hb %d!=%d",
             thisSegment.HighBits(),inputSegment.HighBits()));

  thisSegment.reallocate(inputSegment.PopulationCount(), *this);
  memcpy(thisSegment.Indices(), inputSegment.Indices(), inputSegment.PopulationCount());
}

template <class Allocator>
inline void ASparseBitVector<Allocator>::CompactSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment) {
  thisSegment.reallocate(thisSegment.PopulationCount(), *this);
}

// Segment methods

// inplace OR(thisSegment, inputSegment) accumulated into thisSegment
template <class Allocator>
inline
bool ASparseBitVector<Allocator>::OrSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                            const typename ASparseBitVector<Allocator>::Segment &inputSegment) {
  return OrSegment(thisSegment, inputSegment, thisSegment);
}

// OR(thisSegment, inputSegment) result in outputSegment
template <class Allocator>
inline
bool ASparseBitVector<Allocator>::OrSegment(const typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                            const typename ASparseBitVector<Allocator>::Segment &inputSegment,
                                            typename ASparseBitVector<Allocator>::Segment & outputSegment) {
  bool isInplace = (&outputSegment == &thisSegment);
  if (inputSegment.IsZero()) {
    if (!isInplace)
      // not inplace, so need to copy
      CopySegment(outputSegment, thisSegment);
    return false;
  }

  if (thisSegment.IsZero()) {
    CopySegment(outputSegment, inputSegment);
    return true;
  }

  SparseBitIndex toAllocate = (thisSegment.PopulationCount()+inputSegment.PopulationCount());
  if (toAllocate > kSegmentMask) toAllocate = kSegmentMask+1;

  SparseBitIndex myPopCount = thisSegment.PopulationCount();
  SparseBitIndex inPopCount = inputSegment.PopulationCount();

  Segment tmpSegment;
  if (isInplace && (thisSegment.LastOne() < inputSegment.FirstOne())) {
    // inplace AND can be optimized
    GrowSegment(tmpSegment, thisSegment.AllocatedSize(), toAllocate);
    tmpSegment.fHighBits = thisSegment.fHighBits;
    tmpSegment.append(thisSegment);
    tmpSegment.append(inputSegment);
  } else {
    SparseBitIndex i0 = 0, i = 0, ii = 0;

    // find first one X from input
    // Copy this segment until X
    // add X
    // repeat

    while (ii < inPopCount) {
      // if element found is not on *this, add it
      i = AdvanceIndex(thisSegment, inputSegment.Indices()[ii], i);
      if ((i < myPopCount) && thisSegment.Indices()[i] == inputSegment.Indices()[ii]) {
        // Ignore common bits
        ii += 1;
      } else {
        if (tmpSegment.IsZero()) {
          GrowSegment(tmpSegment, thisSegment.AllocatedSize(), toAllocate);
          tmpSegment.fHighBits = thisSegment.fHighBits;
        }
        tmpSegment.append(thisSegment, i0, i);
        i0 = i;

        SparseBitIndex ii0 = ii;
        if (i == myPopCount)
          ii = inPopCount;
        else
          ii = AdvanceIndex(inputSegment, thisSegment.Indices()[i], ii);
        tmpSegment.append(inputSegment,  ii0, ii);
      }
    }
    no_more:
    if (tmpSegment.IsZero()) {
      if (!isInplace)
        // not inplace, so need to copy
        CopySegment(outputSegment, thisSegment);
      return false;
    }

    // COPY LAST SEGMENT
    // COPY_SEGMENT(i0, i);
    tmpSegment.append(thisSegment, i0, myPopCount);
  }
  outputSegment.swap(tmpSegment);
  tmpSegment.deallocate(*this);
  return true;
}

// Segment methods
template <class Allocator>
inline
bool ASparseBitVector<Allocator>::OrSegmentMask(typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                                const typename ASparseBitVector<Allocator>::Segment &inputSegment,
                                                const typename ASparseBitVector<Allocator>::Segment &maskSegment) {
  if (inputSegment.IsZero() || maskSegment.IsZero())
    return false;

  if (thisSegment.IsZero()) {
    return AndSegment(inputSegment, maskSegment, thisSegment);
  }

  SparseBitIndex toAllocate = (thisSegment.PopulationCount()+inputSegment.PopulationCount());
  if (toAllocate>kSegmentMask) toAllocate = kSegmentMask+1;

  SparseBitIndex myPopCount = thisSegment.PopulationCount();
  SparseBitIndex inPopCount = inputSegment.PopulationCount();
  SparseBitIndex maskPopCount = maskSegment.PopulationCount();
  SparseBitIndex i0=0, i=0, ii=0, mi=0;

  // find first one X from masked input
  // Copy this segment until X
  // add X
  // repeat

  Segment tmpSegment;

  while (ii<inPopCount && mi<maskPopCount) {
    // Find common element between input and mask
    while (maskSegment.Indices()[mi] != inputSegment.Indices()[ii]) {
      ii = AdvanceIndex(inputSegment, maskSegment.Indices()[mi], ii);
      if (ii==inPopCount) goto no_more;
      mi = AdvanceIndex(maskSegment, inputSegment.Indices()[ii], mi);
      if (mi==maskPopCount) goto no_more;
    }

    // if element found is not on *this, add it
    i = AdvanceIndex(thisSegment, maskSegment.Indices()[mi], i);
    if (i==myPopCount || thisSegment.Indices()[i]> maskSegment.Indices()[mi]) {
      if (tmpSegment.IsZero()) {
        GrowSegment(tmpSegment, thisSegment.AllocatedSize(), toAllocate);
        tmpSegment.fHighBits = thisSegment.fHighBits;
      }

      // COPY_SEGMENT(i0, i);
      tmpSegment.append(thisSegment, i0, i);
      i0 = i;

      // ADD_SINGLE(mi, i);
      uint16_t v = maskSegment.Indices()[mi];
      tmpSegment.append(v);
    }
    mi += 1;
    ii += 1;
  }
  no_more:
  if (tmpSegment.IsZero()) return false; // Nothing to add

  // COPY LAST SEGMENT
  // COPY_SEGMENT(i0, i);
  tmpSegment.append(thisSegment, i0, myPopCount);

  thisSegment.swap(tmpSegment);
  tmpSegment.deallocate(*this);
  return true;
}

// inplace AND(thisSegment, inputSegment) accumulated into thisSegment
template <class Allocator>
inline
bool ASparseBitVector<Allocator>::AndSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                             const typename ASparseBitVector<Allocator>::Segment &inputSegment) {
  if (thisSegment.IsZero()) return false;
  if (inputSegment.IsZero()) {
    TruncateSegment(thisSegment);
    return true;
  }

  bool changed = false;
  SparseBitIndex myPopCount = thisSegment.PopulationCount();
  SparseBitIndex inPopCount = inputSegment.PopulationCount();
  SparseBitIndex resPopCount = (myPopCount>inPopCount)?myPopCount:inPopCount;

  SparseBitIndex toAllocate = resPopCount;
  Segment tmpSegment;
  tmpSegment.allocate(toAllocate, *this);
  tmpSegment.fHighBits = thisSegment.fHighBits;

  SparseBitIndex i = 0, ii = 0;
  while (i < myPopCount && ii < inPopCount) {
    uint16_t v  = thisSegment.Indices()[i];
    uint16_t iv = inputSegment.Indices()[ii];

    if (v == iv) {
      tmpSegment.append(v);
      i++; ii++;
    } else if (v < iv) {
      i++;
      changed=true;
    } else
      ii++;
  }
  changed = changed || (i < myPopCount);
  if (!changed) {
    tmpSegment.deallocate(*this);
    return false;
  }

  if (tmpSegment.IsZero()) {
    tmpSegment.deallocate(*this);
    TruncateSegment(thisSegment);
    return true;
  }
  thisSegment.swap(tmpSegment);
  tmpSegment.deallocate(*this);
  return true;
}

// AND(thisSegment, inputSegment) result in outputSegment
template <class Allocator>
inline
bool ASparseBitVector<Allocator>::AndSegment(const typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                             const typename ASparseBitVector<Allocator>::Segment &inputSegment,
                                             typename ASparseBitVector<Allocator>::Segment &outputSegment) {
  if (thisSegment.IsZero()) {
    TruncateSegment(outputSegment);
    return false;
  }
  if (inputSegment.IsZero()) {
    TruncateSegment(outputSegment);
    return true;
  }

  bool changed=false;
  GrowSegment(outputSegment, thisSegment.PopulationCount());
  SparseBitIndex i, ii=0,oi=0;
  for (i=0; i< thisSegment.PopulationCount(); i++) {
    while (ii < inputSegment.PopulationCount() &&
         inputSegment.Indices()[ii] < thisSegment.Indices()[i]) {
      ii+=1;
    }
    if (ii == inputSegment.PopulationCount()) {
      changed = true;
      break;
    }
    if (inputSegment.Indices()[ii] == thisSegment.Indices()[i]) {
      CS2Assert(oi< outputSegment.AllocatedSize(),
           ("Writing past the end of segment %d >= %d (is=%d)",
            oi, outputSegment.AllocatedSize(),
            PopulationCount()));
      outputSegment.Indices()[oi++] = thisSegment.Indices()[i];
    } else
      changed = true;
  }
  if (oi>0) {
    outputSegment.fNumValues=oi;
    CompactSegment(outputSegment);
  } else
    TruncateSegment(outputSegment);
  return changed;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::AndcSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                              const typename ASparseBitVector<Allocator>::Segment &inputSegment) {
  if (thisSegment.IsZero()) return false;
  if (inputSegment.IsZero()) return false;

  SparseBitIndex myPopCount = thisSegment.PopulationCount();
  SparseBitIndex inPopCount = inputSegment.PopulationCount();

  SparseBitIndex i=0, ii=0;

  // Find first common element
  while (thisSegment.Indices()[i]!=inputSegment.Indices()[ii]) {
    i = AdvanceIndex(thisSegment, inputSegment.Indices()[ii], i);
    if (i == myPopCount) return false; // reached end of this vector;

    ii = AdvanceIndex(inputSegment, thisSegment.Indices()[i], ii);
    if (ii == inPopCount) return false; // reached end of input vector;
  }

  // Copy chunks of non-common elements
  SparseBitIndex oi=i;
  while (i<myPopCount-1) { // if we're at last element, nothing to copy
    // Find next common element
    i+=1;
    SparseBitIndex ci=i; // beginning of chunk
    while (thisSegment.Indices()[i]!=inputSegment.Indices()[ii]) {
      i = AdvanceIndex(thisSegment, inputSegment.Indices()[ii], i);
      if (i == myPopCount) break;

      ii = AdvanceIndex(inputSegment, thisSegment.Indices()[i], ii);
      if (ii == inPopCount) {
        i = myPopCount; // copy until end of segment
        break;
      }
    }
    // Copy chunk from ci until i-1
    memmove(&thisSegment.Indices()[oi], &thisSegment.Indices()[ci], (i-ci)*sizeof(uint16_t));
    oi += i-ci;
  }
  if (oi==0) TruncateSegment(thisSegment);
  else {
    thisSegment.fNumValues=oi;
    CompactSegment(thisSegment);
  }

  return true;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::AndcSegment(const typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                              const typename ASparseBitVector<Allocator>::Segment &inputSegment,
                                              typename ASparseBitVector<Allocator>::Segment &outputSegment) {
  if (thisSegment.IsZero() || inputSegment.IsZero()) {
    CopySegment(outputSegment, thisSegment);
    return false;
  }

  bool changed=false;
  GrowSegment(outputSegment, thisSegment.PopulationCount());
  SparseBitIndex i, ii=0,oi=0;
  for (i=0; i< thisSegment.PopulationCount(); i++) {
    while (ii < inputSegment.PopulationCount() &&
          inputSegment.Indices()[ii] < thisSegment.Indices()[i]) {
      ii+=1;
    }
    if (ii == inputSegment.PopulationCount() ||
          inputSegment.Indices()[ii] != thisSegment.Indices()[i]) {
      CS2Assert(oi< outputSegment.AllocatedSize(),
            ("Writing past the end of segment %d >= %d (is=%d)",
                  oi, outputSegment.AllocatedSize(),
                  PopulationCount()));
      outputSegment.Indices()[oi++] = thisSegment.Indices()[i];
    } else
      changed = true;
  }
  if (oi>0) {
    outputSegment.fNumValues=oi;
    CompactSegment(outputSegment);
  } else
    TruncateSegment(outputSegment);
  return changed;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::IsSubset(const typename ASparseBitVector<Allocator>::Segment &thisSegment,
                                           const typename ASparseBitVector<Allocator>::Segment &inputSegment ) const {
  if (thisSegment.IsZero()) return true;
  if (inputSegment.IsZero()) return false;

  size_t pc = thisSegment.PopulationCount();
  size_t ipc = inputSegment.PopulationCount();

  if (pc > ipc) return false;

  SparseBitIndex i=0, ii=0;
  while (i< pc && ii<ipc) {
    if (inputSegment.Indices()[ii] < thisSegment.Indices()[i])
      ii+=1;
    else if (inputSegment.Indices()[ii] == thisSegment.Indices()[i]) {
      ii+=1;
      i+=1;
    } else {
      return false;
    }
  }
  if (i<pc) return false;
  return true;
}

// operator<< (ostream &, const ASparseBitVector &)
//
// Print a sparse bit vector.
template <class Allocator>
template <class str>
inline void ASparseBitVector<Allocator>::DumpSegment(str &out, const typename ASparseBitVector<Allocator>::Segment &s) const {
  SparseBitIndex i;

  for (i=0; i< s.PopulationCount(); i++)
    out << ValueAtSegment(s, i) << " ";
}

template <class Allocator>
inline
void ASparseBitVector<Allocator>::TruncateSegment(typename ASparseBitVector<Allocator>::Segment &thisSegment) {
  thisSegment.fNumValues=0;
}

template <class Allocator>
inline
void ASparseBitVector<Allocator>::RemoveSegmentAt(SparseBitIndex segmentIndex) {
  SparseBitIndex i = segmentIndex;

  Segment s = fBase[i];

  if (fNumberOfSegments>1) {
    for (size_t j = i; j <(fNumberOfSegments -1); j++)
      fBase[j] = fBase[j+1];

    fBase = (Segment *)Allocator::reallocate(sizeof(Segment) * (fNumberOfSegments-1),
                                             fBase,
                                             sizeof(Segment) * (fNumberOfSegments));
    fNumberOfSegments -= 1;
  } else {
    Allocator::deallocate(fBase, sizeof(Segment));
    // unallocated, but non-null
    fBase = (Segment *) 1;
    fNumberOfSegments = 0;
  }
  TruncateSegment(s);
}

template <class Allocator>
inline
void ASparseBitVector<Allocator>::RemoveSegment(SparseBitIndex index) {
  SparseBitIndex i;

  for (i=0; i< fNumberOfSegments; i++) {
    if (fBase[i].fHighBits >= index>>16) {
      if (fBase[i].fHighBits == index>>16)
        RemoveSegmentAt(i);
      return;
    }
  }
}

// ASparseBitVector<Allocator>::MemoryUsage
//
// Return the size in bytes of storage used for this vector.
template <class Allocator>
inline
unsigned long ASparseBitVector<Allocator>::MemoryUsage() const {
  unsigned long ret=0;
  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++)
    ret += fBase[i].MemoryUsage();
  ret += sizeof(ASparseBitVector);
  return ret;
}

// Return the number of '1' bits in the vector.  Optionally specify the
// number of leading bits to examine.
//SparseBitIndex ASparseBitVector<Allocator>::PopulationCount (SparseBitIndex numBits) const{
//  SparseBitIndex i,ret=0;
//  CS2Assert(numBits==0xEFFFFFFFul,("Population count subset not implemented"));
//  for (i=0; i< fNumberOfSegments; i++)
//    ret += fBase[i].PopulationCount();
//  return ret;
//}

// operator<< (ostream &, const ASparseBitVector &)
//
// Print a sparse bit vector.
template <class Allocator, class str>
inline
str &operator<<  (str &out, const ASparseBitVector<Allocator> &vector) {
  vector.Dump(out);
  return out;
}

template <class Allocator>
template <class str>
inline void  ASparseBitVector<Allocator>::Dump(str &out) const {
  if (IsNull()) {
    out << "0";
    return;
  }
  out << "( ";
  SparseBitIndex i, n = fNumberOfSegments;
  for (i=0; i< n; i++) {
    DumpSegment(out, fBase[i]);
  }
  out << ")";
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::Andc(const ASparseBitVector<Allocator> &inputVector) {
  if (IsZero()) return false;
  if (inputVector.IsZero()) return false;

  bool changed = false;
  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++) {
    typename ASparseBitVector<Allocator>::Segment *s = &fBase[i];
    const typename ASparseBitVector<Allocator>::Segment *is = inputVector.FindSegment(s->HighBits());

    if (is) {
      changed = AndcSegment(*s, *is) || changed;
      if (s->IsZero()) { RemoveSegment(s->HighBits()); i-=1; }
    }
  }
  return changed;
}

template <class Allocator>
template <class B2>
inline
bool ASparseBitVector<Allocator>::Andc(const B2 &inputVector) {

  if (IsZero()) return false;
  if (inputVector.IsZero()) return true;
  bool changed = false;
  SparseBitIndex i;

  SparseBitIndex lastOne = inputVector.LastOne();
  typename B2::Cursor ic(inputVector);
  ic.SetToFirstOne();

  for (i=0; i<fNumberOfSegments; i++) {
    typename ASparseBitVector<Allocator>::Segment *s = &fBase[i];
    uint16_t *segment = s->Indices();
    SparseBitIndex sHighBits = s->HighBits();

    SparseBitIndex j=0, j0=0;
    SparseBitIndex pc = s->PopulationCount();


    if (inputVector.hasFastRandomLookup()) {
      if (sHighBits>lastOne) break;

      while (j<pc) {
        SparseBitIndex j2=j;
        while (!inputVector.ValueAt(sHighBits|segment[j2])) {
          j2+=1;
          if (j2==pc) break;
          if (segment[j2]>lastOne) {
            j2=pc;
            break;
          }
        }

        memmove(&segment[j0], &segment[j], (j2-j)*sizeof(uint16_t));
        j0+=(j2-j);
        j=j2;
        j+=1;
      }
    } else {
      while ( (ic & ~kSegmentMask) < sHighBits) {
        ic.SetToNextOne();
        if (!ic.Valid()) break;
      }
      if (!ic.Valid()) break;
      if ((ic & ~kSegmentMask) != sHighBits) continue;

      while (j<pc) {
        SparseBitIndex j2=j;
        while (segment[j2] != (ic&kSegmentMask)) {
          if (segment[j2] < (ic&kSegmentMask)) {
            j2+=1;
            if (j2==pc) break;
          } else {
            ic.SetToNextOne();
            if (!ic.Valid()) {
              j2=pc;
              break;
            }
          }
        }

        memmove(&segment[j0], &segment[j], (j2-j)*sizeof(uint16_t));
        j0+=(j2-j);
        j=j2;
        j+=1;
      }
    }
    if (j0==0) {
      RemoveSegment(sHighBits);
      i-=1;
    } else {
      s->fNumValues=j0;
    }
  }
  return changed;
}


template <class Allocator>
inline
bool ASparseBitVector<Allocator>::Andc(const ASparseBitVector<Allocator> &inputVector,
                                             ASparseBitVector<Allocator> &outputVector) const {
  if (IsZero() || inputVector.IsZero()) {
    outputVector = *this;
    return false;
  }

  outputVector.Clear();
  bool changed = false;
  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++) {
    typename ASparseBitVector<Allocator>::Segment *s = &fBase[i];
    typename ASparseBitVector<Allocator>::Segment *is = inputVector.FindSegment(s->HighBits());

    if (!is) {
      outputVector.AddSegment(s->HighBits(), s->PopulationCount(), s->Indices());
    } else {
      typename ASparseBitVector<Allocator>::Segment *os = outputVector.AddSegment(s->HighBits(), s->PopulationCount());

      changed = outputVector.AndcSegment(*s, *is, *os) || changed;
      if (os->IsZero()) outputVector.RemoveSegment(s->HighBits());
    }
  }
  return changed;
}

template <class Allocator>
template <class B2>
inline bool ASparseBitVector<Allocator>::And(const B2 &vector) {
  if (IsZero()) return false;

  typename B2::Cursor c1(vector), c2(vector);

  c1.SetToFirstOne();

  if (!c1.Valid()) {
    Clear();
    return true;
  }
  c2.SetToFirstOne();

  bool changed = false;
  SparseBitIndex i;
  i = 0;
  while (c1.Valid() && (i < fNumberOfSegments)) {
    SparseBitIndex count=1;
    SparseBitIndex highBits = c1 &~ kSegmentMask;

    for (c1.SetToNextOne();
         c1.Valid() && ((c1&~kSegmentMask) == highBits);
         c1.SetToNextOne()) {
      count++;
    }

    typename ASparseBitVector<Allocator>::Segment *s = NULL;
    SparseBitIndex sHighBits;
    for (; i < fNumberOfSegments; /* do not i++ */) {
      s = &fBase[i];
      sHighBits = s->HighBits();
      if (sHighBits >= highBits) {
        if (sHighBits > highBits)
          s = NULL;
        break;
      }
      s = NULL;
      // this Segment does not exist in vector; AND is zero - so delete it, do not i++ after
      RemoveSegmentAt(i);
      changed = true;
    }

    if (s == NULL) { /* did not find segment, need to advance c2 for next bit */
      for (SparseBitIndex bi = 0; bi < count; bi++) {
        c2.SetToNextOne();
      }
      /* either fNumberOfSegments is 0, or sHighBits > highBits for next segment */
    } else { /* (s != NULL) - found segment for highBits at i */
      // build Segment for vector at highBits
      uint16_t *bits = (uint16_t *)Allocator::allocate(count*sizeof(uint16_t));
      for (SparseBitIndex bi = 0; bi < count; bi++) {
        bits[bi]= c2 & kSegmentMask;
        c2.SetToNextOne();
      }
      Segment is;
      is.adopt(highBits>>16, count, count, bits);
      changed = AndSegment(*s, is) || changed;
      Allocator::deallocate(bits,count*sizeof(uint16_t));
      if (s->IsZero()) {
        // AND was zero - so remove found Segment
        RemoveSegmentAt(i);
        // all other segments move up, so do not i++
      } else {
        i++;
      }
    }
  }
  for (; i < fNumberOfSegments; i++) {
    // we are beyond highest Segment for vector; AND is zero - so delete any higher segments of *this
    changed = true;
    RemoveSegmentAt(i);
    i--;
  }
  return changed;
}

template <class Allocator>
template <class B2>
inline void ASparseBitVector<Allocator>::Or(const B2 &abv) {
  typename B2::Cursor abvc(abv);
  OrCursor(abvc);
}

template <class Allocator>
template <class ACursor>
inline void ASparseBitVector<Allocator>::OrCursor(ACursor &abvc) {
  const SparseBitIndex BUFFERSIZE = 1024;

  abvc.SetToFirstOne();
  while (abvc.Valid()) {
    uint16_t *bits = (uint16_t *)Allocator::allocate(BUFFERSIZE*sizeof(uint16_t));
    SparseBitIndex bc=0;
    SparseBitIndex highBits = abvc &~ kSegmentMask;

    do {
      bits[bc++]= abvc & kSegmentMask;
      abvc.SetToNextOne();
    } while (bc < BUFFERSIZE &&
             abvc.Valid() &&
             (highBits == (abvc &~ kSegmentMask)));

    OrSegment(highBits, bc, bits);
    Allocator::deallocate(bits,BUFFERSIZE*sizeof(uint16_t));
  }
}

template <class Allocator>
inline
void ASparseBitVector<Allocator>::Or(ASparseBitVector<Allocator> **vs, SparseBitIndex nvs) {
  if (nvs==0) return;
  if (nvs==1) {
    Or(*vs[0]);
    return;
  }

  ASparseBitVector<Allocator> segments(Allocator(*this));

  // Identify all segments to be added
  for (SparseBitIndex i = 0; i < nvs; i++) {
    for (SparseBitIndex s=0; s< vs[i]->fNumberOfSegments; s++) {
      SparseBitIndex hbits = vs[i]->fBase[s].HighBits();
      segments[hbits>>16]=1;
    }
  }

  const SparseBitIndex TILE_BITS=10;
  const SparseBitIndex TILE_LEN=(1<<(TILE_BITS));
  const SparseBitIndex TILE_COUNT=(kSegmentMask+1)/TILE_LEN;

  typename ASparseBitVector<Allocator>::Segment **ls =
    (typename ASparseBitVector<Allocator>::Segment **)Allocator::allocate(sizeof(void *)*nvs);
  SparseBitIndex *cs = (SparseBitIndex *)Allocator::allocate(sizeof(SparseBitIndex)*nvs);
  bool * processed = (bool *)Allocator::allocate(sizeof(bool)*nvs);
  uint16_t *bits = (uint16_t *)Allocator::allocate(sizeof(uint16_t)*TILE_LEN);
  memset(bits,0,sizeof(uint16_t)*TILE_LEN);

  typename ASparseBitVector<Allocator>::Cursor sc(segments);
  for (sc.SetToFirstOne(); sc.Valid(); sc.SetToNextOne()) {
    SparseBitIndex hbits = SparseBitIndex(sc)<<16;

    SparseBitIndex count=0;
    for (SparseBitIndex i = 0; i < nvs; i++) {
      typename ASparseBitVector<Allocator>::Segment *s =
        vs[i]->FindSegment(hbits);

      if (s) {
        cs[count]=0;
        ls[count] = s;
        processed[count++]=false;
      }
    }

    for (SparseBitIndex tile=0; tile<TILE_COUNT && count>1; tile++) {
      SparseBitIndex low = tile * TILE_LEN;
      SparseBitIndex high = low + TILE_LEN;
      SparseBitIndex i = 0, i2 = 0;
      bool found = false;
      while (i < count) {
        while (processed[i2])
          i2+=1;
        SparseBitIndex c = cs[i2];
        uint16_t *sp = ls[i2]->Indices();
        SparseBitIndex last = ls[i2]->PopulationCount();

        while (c<last && sp[c]<high) {
          bits[sp[c]-low]=1;
          c+=1;
        }
        if (c<last) {
          if (cs[i2]!=c) {
            cs[i2]=c;
            found=true;
          }
          i+=1;
        } else {
          found=true;
          count-=1;
          processed[i2] = true;
        }
        i2+=1;
      }
      if (found) {
        SparseBitIndex eln=0;
        for (SparseBitIndex i=0; i<1024; i++)
          if (bits[i]) {
            bits[i]=0;
            bits[eln++]=low+i;
          }

        OrSegment(hbits, eln, bits);
        memset(bits, 0, sizeof(uint16_t)*eln);
      }
    }
    if (count==1) {
      SparseBitIndex i = 0;
      while (processed[i])
        i+=1;
      SparseBitIndex c = cs[i];
      uint16_t *sp = ls[i]->Indices();
      SparseBitIndex last = ls[i]->PopulationCount();

      OrSegment(hbits, last-c, &sp[c]);
    }
  }
  Allocator::deallocate(ls, sizeof(void *)*nvs);
  Allocator::deallocate(cs, sizeof(SparseBitIndex)*nvs);
  Allocator::deallocate(processed, sizeof(bool)*nvs);
  Allocator::deallocate(bits, sizeof(uint16_t)*TILE_LEN);
}

template <class Allocator>
inline
void ASparseBitVector<Allocator>::Or(ASparseBitVector<Allocator> **vs, SparseBitIndex nvs, const ASparseBitVector<Allocator> &mask) {
  if (nvs==0 || mask.IsZero() ) return;
  if (nvs==1) {
    OrMask(*vs[0], mask);
    return;
  }

  ASparseBitVector<Allocator> segments(Allocator(*this));

  // Identify all segments to be added
  for (SparseBitIndex i = 0; i < nvs; i++) {
    for (SparseBitIndex s=0; s< vs[i]->fNumberOfSegments; s++) {
      SparseBitIndex hbits = vs[i]->fBase[s].HighBits();
      if (mask.FindSegment(hbits))
        segments[hbits>>16]=1;
    }
  }

  const SparseBitIndex TILE_BITS=10;
  const SparseBitIndex TILE_LEN=(1<<(TILE_BITS));
  const SparseBitIndex TILE_COUNT=(kSegmentMask+1)/TILE_LEN;

  typename ASparseBitVector<Allocator>::Segment **ls =
    (typename ASparseBitVector<Allocator>::Segment **)Allocator::allocate(sizeof(void *)*nvs);
  SparseBitIndex *cs = (SparseBitIndex *)Allocator::allocate(sizeof(SparseBitIndex)*nvs);
  bool * processed = (bool *)Allocator::allocate(sizeof(bool)*nvs);
  uint16_t *bits = (uint16_t *)Allocator::allocate(sizeof(uint16_t)*TILE_LEN);
  memset(bits,0,sizeof(uint16_t)*TILE_LEN);

  typename ASparseBitVector<Allocator>::Cursor sc(segments);
  for (sc.SetToFirstOne(); sc.Valid(); sc.SetToNextOne()) {
    SparseBitIndex hbits = SparseBitIndex(sc)<<16;

    typename ASparseBitVector<Allocator>::Segment *ms =
      mask.FindSegment(hbits);
    SparseBitIndex mc=0;
    uint16_t *maskp = ms->Indices();
    SparseBitIndex mlast = ms->PopulationCount();

    SparseBitIndex count=0;
    for (SparseBitIndex i = 0; i < nvs; i++) {
      typename ASparseBitVector<Allocator>::Segment *s =
        vs[i]->FindSegment(hbits);

      if (s) {
        cs[count]=0;
        ls[count] = s;
        processed[count++]=false;
      }
    }

    for (SparseBitIndex tile=0; mc<mlast && tile<TILE_COUNT; tile++) {
      SparseBitIndex low = tile * TILE_LEN;
      SparseBitIndex high = low + TILE_LEN;

      while (mc<mlast && maskp[mc]<high){
        bits[maskp[mc]-low]=2;
        mc+=1;
      }

      SparseBitIndex i = 0, i2 = 0;
      while (i < count) {
        while (processed[i2])
          i2+=1;
        SparseBitIndex c = cs[i2];
        uint16_t *sp = ls[i2]->Indices();
        SparseBitIndex last = ls[i2]->PopulationCount();

        while (c<last && sp[c]<high) {
          if (bits[sp[c]-low]==2)
            bits[sp[c]-low]=1;
          c+=1;
        }
        if (c<last) {
          cs[i2]=c;
          i+=1;
        } else {
          count-=1;
          processed[i2]=true;
        }
        i2+=1;
      }

      SparseBitIndex eln=0;
      for (SparseBitIndex i=0; i<1024; i++)  {
        uint16_t b = bits[i];
        if (b) {
          bits[i]=0;
          if (b==1)
            bits[eln++]=low+i;
        }
      }
      if (eln) {
        OrSegment(hbits, eln, bits);
        memset(bits, 0, sizeof(uint16_t)*eln);
      }
    }
  }
  Allocator::deallocate(ls, sizeof(void *)*nvs);
  Allocator::deallocate(cs, sizeof(SparseBitIndex)*nvs);
  Allocator::deallocate(processed, sizeof(bool)*nvs);
  Allocator::deallocate(bits, sizeof(uint16_t)*TILE_LEN);
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::Or (const ASparseBitVector<Allocator> &inputVector) {
  if (inputVector.IsZero()) return false;
  if (IsZero()) {
    *this = inputVector;
    return true;
  }

  bool changed = false;
  SparseBitIndex i;
  for (i=0; i< inputVector.fNumberOfSegments; i++) {
    const typename ASparseBitVector<Allocator>::Segment *is = &inputVector.fBase[i];
    typename ASparseBitVector<Allocator>::Segment *s = FindSegment(is->HighBits());
    if (s) {
      if (OrSegment(*s, *is)) changed = true;
    } else{
      AddSegment(is->HighBits(), is->PopulationCount(), is->Indices());
      changed = true;
    }
  }
  return changed;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::Or  (const ASparseBitVector<Allocator> &inputVector,
                                        ASparseBitVector<Allocator> &outputVector) const {
  if (inputVector.IsZero()) {
    outputVector = *this;
    return false;
  }
  if (IsZero()) {
    outputVector = inputVector;
    return true;
  }

  outputVector.Clear();
  bool changed = false;
  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++) {
    const typename ASparseBitVector<Allocator>::Segment *s = &fBase[i];
    typename ASparseBitVector<Allocator>::Segment *is = inputVector.FindSegment(s->HighBits());

    if (is) {
      typename ASparseBitVector<Allocator>::Segment *os = outputVector.AddSegment(s->HighBits(), s->PopulationCount());
      if (outputVector.OrSegment(*s, *is, *os)) changed = true;
    } else {
      outputVector.AddSegment(s->HighBits(), s->PopulationCount(), s->Indices());
    }
  }
  for (i=0; i< inputVector.fNumberOfSegments; i++) {
    const typename ASparseBitVector<Allocator>::Segment *is = &inputVector.fBase[i];
    typename ASparseBitVector<Allocator>::Segment *s = FindSegment(is->HighBits());

    if (!s) {
      typename ASparseBitVector<Allocator>::Segment *os = outputVector.AddSegment(is->HighBits(), is->PopulationCount(), is->Indices());
      changed = true;
    }
  }

  return changed;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::OrMask(const ASparseBitVector<Allocator> &inputVector,
                                         const ASparseBitVector<Allocator> &maskVector) {
  if (inputVector.IsZero() || maskVector.IsZero()) return false;
  if (IsZero()) {
    return inputVector.And(maskVector, *this);
  }

  bool changed = false;
  SparseBitIndex i;
  for (i=0; i< inputVector.fNumberOfSegments; i++) {
    const typename ASparseBitVector<Allocator>::Segment *is = &inputVector.fBase[i];
    typename ASparseBitVector<Allocator>::Segment *ms = maskVector.FindSegment(is->HighBits());
    if (ms) {
      typename ASparseBitVector<Allocator>::Segment *s = AddSegment(is->HighBits(), is->PopulationCount());

      if (OrSegmentMask(*s, *is, *ms)) {
        changed = true;
        if (s->IsZero()) RemoveSegment(s->HighBits());
      }
    }
  }
  return changed;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::And(const ASparseBitVector<Allocator> &inputVector) {
  if (IsZero()) return false;
  if (inputVector.IsZero()) { Clear(); return true;}

  bool changed = false;
  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++) {
    typename ASparseBitVector<Allocator>::Segment *s = &fBase[i];
    const typename ASparseBitVector<Allocator>::Segment *is = inputVector.FindSegment(s->HighBits());

    if (!is) {
      RemoveSegment(s->HighBits());
      i-=1;
      changed = true;
    } else {
      changed = AndSegment(*s, *is) || changed;
      if (s->IsZero()) { RemoveSegment(s->HighBits()); i-=1; }
    }
  }
  return changed;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::And(const ASparseBitVector<Allocator> &inputVector,
                                            ASparseBitVector<Allocator> &outputVector) const {
  if (IsZero()) {
    outputVector.Clear();
    return false;
  }

  outputVector.Clear();
  bool changed = false;
  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++) {
    typename ASparseBitVector<Allocator>::Segment *s = &fBase[i];
    typename ASparseBitVector<Allocator>::Segment *is = inputVector.FindSegment(s->HighBits());

    if (is) {
      typename ASparseBitVector<Allocator>::Segment *os = outputVector.AddSegment(s->HighBits(), s->PopulationCount());
      changed = outputVector.AndSegment(*s, *is, *os) || changed;
      if (os->IsZero()) outputVector.RemoveSegment(s->HighBits());
    }
  }
  return changed;
}


template <class Allocator>
template <class B2>
inline
bool ASparseBitVector<Allocator>::Intersects (const B2 &inputVector) const{
  if (IsZero()) return false;

  typename ASparseBitVector<Allocator>::Cursor tc(*this);
  tc.SetToFirstOne();
  if (inputVector.hasFastRandomLookup()){

    do {
      if (inputVector.ValueAt(tc)) return true;

      tc.SetToNextOne();
    } while (tc.Valid());
    return false;
  } else {

    typename B2::Cursor ic(inputVector);
    ic.SetToFirstOne();
    if (!ic.Valid()) return false;

    if (tc<ic) {
      tc.SetToNextOneAfter(ic-1);
      if (!tc.Valid()) return false;
      if (tc==ic) return true;
    }

    for (;;) {
      ic.SetToNextOneAfter(tc-1);
      if (!ic.Valid()) return false;
      if (tc==ic) return true;

      tc.SetToNextOneAfter(ic-1);
      if (!tc.Valid()) return false;
      if (tc==ic) return true;
    }
  }
  return false;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::operator==  (const ASparseBitVector<Allocator> &inputVector) const{
  if (IsZero() && inputVector.IsZero()) return true;
  if (IsZero() || inputVector.IsZero()) return false;

  if (fNumberOfSegments != inputVector.fNumberOfSegments) return false;
  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++) {
    if (!(fBase[i] == inputVector.fBase[i])) return false;
  }
  return true;
}

template <class Allocator>
inline
bool ASparseBitVector<Allocator>::operator<=(const ASparseBitVector<Allocator> &inputVector) const{
  if (IsZero()) return true;
  if (inputVector.IsZero()) return false;

  if (fNumberOfSegments > inputVector.fNumberOfSegments) return false;

  SparseBitIndex i;
  for (i=0; i< fNumberOfSegments; i++) {
    typename ASparseBitVector<Allocator>::Segment *s = &fBase[i];
    typename ASparseBitVector<Allocator>::Segment *is = inputVector.FindSegment(s->HighBits());

    if (!is) return false;
    if (!(IsSubset(*s, *is))) return false;
  }
  return true;
}

template <class Allocator>
template <class B2>
inline ASparseBitVector<Allocator> &
ASparseBitVector<Allocator>::operator-=(const B2 &v) {
  Andc(v);
  return *this;
}

template <class Allocator>
template <class B2>
inline ASparseBitVector<Allocator> &
ASparseBitVector<Allocator>::operator|=(const B2 &v) {
  Or(v);
  return *this;
}

template <class Allocator>
template <class B2>
inline ASparseBitVector<Allocator> &
ASparseBitVector<Allocator>::operator&=(const B2 &v) {
  And(v);
  return *this;
}

}

#ifdef CS2_ALLOCINFO
#undef allocate
#undef deallocate
#undef reallocate
#endif

#endif // SPARSRBIT_H
