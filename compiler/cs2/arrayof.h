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
/*  File name:  arrayof.h                                                  */
/*  Purpose:    Definition of the ArrayOf template class.                  */
/*                                                                         */
/***************************************************************************/

#ifndef CS2_ARRAYOF_H
#define CS2_ARRAYOF_H

#include "cs2/cs2.h"
#include "cs2/allocator.h"

#ifdef CS2_ALLOCINFO
#define allocate(x) allocate(x, __FILE__, __LINE__)
#define deallocate(x,y) deallocate(x, y, __FILE__, __LINE__)
#define reallocate(x,y,z) reallocate(x, y, z, __FILE__, __LINE__)
#endif

namespace CS2 {

// Poor-man type-traits
  template<class T> struct is_pod { operator bool() { return false;} };
  template<> struct is_pod<uint32_t> { operator bool() { return true;} };
  template<> struct is_pod<int32_t> { operator bool() { return true;} };
  template<> struct is_pod<uint16_t> { operator bool() { return true;} };
  template<> struct is_pod<int16_t> { operator bool() { return true;} };
  template<> struct is_pod<char> { operator bool() { return true;} };
  template<> struct is_pod<unsigned char> { operator bool() { return true;} };
  template<> struct is_pod<void *> { operator bool() { return true;} };
  template<> struct is_pod<char *> { operator bool() { return true;} };

#define CS2_ARTEMP template <class AElementType, class Allocator, size_t segmentBits>
#define CS2_BASEARDECL BaseArrayOf<AElementType, Allocator, segmentBits>
#define CS2_ARDECL ArrayOf <AElementType, Allocator, segmentBits>

// ------------------------------------------------------------------------
/// ArrayOf
///
/// \brief Extensible array data structure.  This data structure will automatically
/// initialize array elements if a default constructor exists for the element
/// type.  In addition, storage will be grown preserving existing array values
/// if an index outside the existing range is used.
///
/// \ingroup CompilerServices
///
// ------------------------------------------------------------------------
template <class AElementType, class Allocator, size_t segmentBits = 8>
struct BaseArrayOf : private Allocator {
public:

  BaseArrayOf (const Allocator &a = Allocator());
  ~BaseArrayOf ();

  /// \brief Copy constructor.
  BaseArrayOf (const CS2_BASEARDECL &, const Allocator &a = Allocator());

  const Allocator& allocator() const { return *this;}

  /// \brief Assignment operator.
  CS2_BASEARDECL &operator= (const CS2_BASEARDECL &);


  /// \brief Access operator (growable).
  /// \param[in] el Array index
  /// \return the array element at the given index.
  /// If the index is larger than the current size of the array, grow
  /// the array large enough to access the given index.
  AElementType &operator[] (size_t el);
  const AElementType &operator[] (size_t el) const;

  /// \brief Access operator (non-growable).
  /// \param[in] el Array index
  /// \return the array element at the given index.
  /// For const case the index must be less than the current size of the array.
  AElementType &ElementAt (size_t el) const;
  AElementType &ElementAt (size_t el);

  /// \return Current number of elements in the array
  size_t NumberOfElements() const;

  /// \brief Grow the array to the given size.  Optionally grow the array
  /// additional extents to guarantee geometric growth.
  /// \param[in] s Size to grow the array.
  void GrowTo (size_t s);

  /// \return Size in bytes of memory dynamically allocated for the array
  unsigned long DynamicMemoryUsage() const;

  /// \return Size in bytes currently allocated for the array
  unsigned long MemoryUsage() const;

  /// \return Number of elements per segment.
  size_t ElementsPerSegment() const;

  /// \return Size of single segment
  size_t SegmentSize() const;

  /// \return Mask of segment index (0000..1111)
  size_t SegmentMask() const;

  /// \brief Shrink the array to the given size.  Discard memory made unused by
  /// shrinking.
  /// \param[in] s Shrink to size s.
  void ShrinkTo (size_t s);

  template <class ostr>
  friend
  ostr &operator<<  (ostr &out, const CS2_BASEARDECL &table) {
    for (size_t i=0; i<table.NumberOfElements(); i++) {
      out << "[" << i << "]:";
      out << table[i];
      out << "\n";
    }

    return out;
  }

  class DerivedElement {
    public:

    void *operator new (size_t, void *ptr) {
      return ptr;
    }

    DerivedElement() : fElement() {}
    template <class Initializer>
    DerivedElement (const Initializer &element) : fElement(element) {}
    ~DerivedElement() {}

    AElementType &Element() { return fElement; }
    operator AElementType() { return fElement; }
    private:

    AElementType fElement;
  };

  /// \brief Provide a pointer to the segment containing a specific
  /// element of the array
  DerivedElement *DerivedElementAt (size_t el) const;
  DerivedElement *Segment(size_t el) const;

  DerivedElement **fSegmentMap;
  uint32_t         fMaxSegments;
  uint32_t         fNumberOfSegments;
};

CS2_ARTEMP inline size_t CS2_BASEARDECL::ElementsPerSegment() const {
    return size_t(1) << segmentBits;
}

CS2_ARTEMP inline size_t CS2_BASEARDECL::SegmentSize() const {
    return ElementsPerSegment() * sizeof(AElementType);
}

CS2_ARTEMP inline size_t CS2_BASEARDECL::SegmentMask() const{
    return ~((~size_t(0)) << segmentBits);
}

CS2_ARTEMP inline AElementType &CS2_BASEARDECL::ElementAt (size_t elementIndex) const {
  return DerivedElementAt(elementIndex)->Element();
}

CS2_ARTEMP inline typename CS2_BASEARDECL::DerivedElement *CS2_BASEARDECL::Segment (size_t elementIndex) const {
  size_t        segmentMapIndex;

  segmentMapIndex = elementIndex >> segmentBits;
  CS2Assert (segmentMapIndex < fNumberOfSegments,
          ("Index %lu does not exist", elementIndex));

  return fSegmentMap[segmentMapIndex];
}

CS2_ARTEMP inline
typename CS2_BASEARDECL::DerivedElement *
CS2_BASEARDECL::DerivedElementAt (size_t elementIndex) const {
  size_t segmentMapIndex = elementIndex >> segmentBits;
  CS2Assert (segmentMapIndex < fNumberOfSegments,
          ("Index %lu does not exist", elementIndex));

  DerivedElement *pSegment = fSegmentMap[segmentMapIndex];
  size_t maskedIndex = elementIndex & SegmentMask();

  return &pSegment[maskedIndex];
}

CS2_ARTEMP inline AElementType &CS2_BASEARDECL::ElementAt (size_t elementIndex) {
  if (elementIndex >= NumberOfElements())
    GrowTo (elementIndex + 1);

  return DerivedElementAt(elementIndex)->Element();
}

CS2_ARTEMP inline const AElementType &CS2_BASEARDECL::operator[] (size_t elementIndex) const {
  return ElementAt(elementIndex);
}

CS2_ARTEMP inline AElementType &CS2_BASEARDECL::operator[] (size_t elementIndex) {
  return ElementAt(elementIndex);
}

CS2_ARTEMP inline size_t CS2_BASEARDECL::NumberOfElements() const {
  return fNumberOfSegments * ElementsPerSegment();
}


CS2_ARTEMP inline CS2_BASEARDECL::BaseArrayOf (const Allocator &a) : Allocator(a) {
  CS2Assert (sizeof(AElementType) <= SegmentSize(),
             ("Segment size %d must be larger than size of element %d.", (int) SegmentSize(), (int) sizeof(AElementType)));

  CS2Assert (ElementsPerSegment() > 0, ("Elements per segment is zero\n"));
  CS2Assert (ElementsPerSegment() < 0x10000,
          ("Too many elements per segment: %lu\n", ElementsPerSegment()));

  fNumberOfSegments = 0;
  fMaxSegments = 0;

  if (fNumberOfSegments == 0) {
    fSegmentMap = NULL;
    return;
  }
}

CS2_ARTEMP inline CS2_BASEARDECL::BaseArrayOf (const CS2_BASEARDECL &inputArray, const Allocator &a) :
    Allocator(a),
    fMaxSegments (inputArray.fMaxSegments),
    fNumberOfSegments (inputArray.fNumberOfSegments){
  size_t segmentIndex;

  if (fNumberOfSegments == 0) {
    fMaxSegments = 0;
    fSegmentMap = NULL;
  } else {
    fSegmentMap = (DerivedElement **) Allocator::allocate( fMaxSegments * sizeof(DerivedElement *));

    for (segmentIndex = 0; segmentIndex < fNumberOfSegments; ++segmentIndex) {
      fSegmentMap[segmentIndex] = (DerivedElement *) Allocator::allocate( SegmentSize());
    }
  }
}

CS2_ARTEMP inline CS2_BASEARDECL &CS2_BASEARDECL::operator= (const CS2_BASEARDECL &inputArray) {
  size_t segmentIndex, firstSegment, elementIndex;
  DerivedElement *currentElement, *newSegment, *oldSegment;
  // Ensure that the segment map is big enough
  if (fMaxSegments < inputArray.fMaxSegments) {

    DerivedElement **newSegmentMap =
      (DerivedElement **) Allocator::allocate (inputArray.fMaxSegments * sizeof(DerivedElement *));

    memcpy(newSegmentMap, fSegmentMap, fMaxSegments * sizeof(DerivedElement *));
    Allocator::deallocate(fSegmentMap, fMaxSegments * sizeof(DerivedElement *));
    fMaxSegments = inputArray.fMaxSegments;

    if (fSegmentMap == NULL) {
      SystemResourceError::Memory();
    }
  }

  {
    firstSegment = (fNumberOfSegments< inputArray.fNumberOfSegments)?fNumberOfSegments:inputArray.fNumberOfSegments;

    for (segmentIndex = 0; segmentIndex < firstSegment; ++segmentIndex) {
        {
        newSegment = fSegmentMap[segmentIndex];
        oldSegment = inputArray.fSegmentMap[segmentIndex];

        {
          for (elementIndex = 0;
               elementIndex < ElementsPerSegment();
               ++elementIndex) {
            newSegment[elementIndex] = oldSegment[elementIndex];
          }
        }
      }
    }
  }

  fNumberOfSegments = inputArray.fNumberOfSegments;

  // Copy segments that exist in the input array but not in this array.
  for (segmentIndex = firstSegment;
       segmentIndex < fNumberOfSegments;
       ++segmentIndex) {
    fSegmentMap[segmentIndex] = (DerivedElement *) Allocator::allocate ( SegmentSize());
  }

  return *this;
}

CS2_ARTEMP inline CS2_BASEARDECL::~BaseArrayOf() {
  size_t segmentIndex;

  for (segmentIndex = 0; segmentIndex < fNumberOfSegments; ++segmentIndex) {
    Allocator::deallocate (fSegmentMap[segmentIndex], SegmentSize());
  }
  if (fSegmentMap)
    Allocator::deallocate (fSegmentMap, fMaxSegments * sizeof(DerivedElement *));
}

CS2_ARTEMP inline void CS2_BASEARDECL::GrowTo (size_t newSize) {
  size_t segmentMapIndex, newSegmentIndex;

  if (newSize==0) return;

  segmentMapIndex = (newSize-1) >> segmentBits;

  if (segmentMapIndex < fNumberOfSegments) return;

  if (segmentMapIndex >= fMaxSegments) {
    if (fSegmentMap == NULL) {
      uint32_t updatedMaxSegments = segmentMapIndex + (fMaxSegments >> 1) + 1;
      fSegmentMap = (DerivedElement **) Allocator::allocate ( updatedMaxSegments * sizeof(DerivedElement *));
      fMaxSegments = updatedMaxSegments;
    } else {
      size_t maxSegments = segmentMapIndex + (fMaxSegments >> 1) + 1;
      void * newSegmentMapAllocation = Allocator::reallocate(
        maxSegments * sizeof(DerivedElement *),
        fSegmentMap,
        fMaxSegments * sizeof(DerivedElement *)
        );
      fSegmentMap = static_cast<DerivedElement **>(newSegmentMapAllocation);
      fMaxSegments = maxSegments;
    }

    if (fSegmentMap == NULL) {
      SystemResourceError::Memory();
    }
  }

  for (newSegmentIndex = fNumberOfSegments;
       newSegmentIndex < segmentMapIndex + 1;
       ++newSegmentIndex) {
    fSegmentMap[newSegmentIndex] = (DerivedElement *) Allocator::allocate ( SegmentSize());
    fNumberOfSegments = newSegmentIndex + 1;
  }

  CS2Assert (segmentMapIndex + 1 == fNumberOfSegments,
    ("Number of current segments %d is smaller than expected size of %d.", (int) fNumberOfSegments, (int) (newSegmentIndex + 1) ));

}

CS2_ARTEMP inline void CS2_BASEARDECL::ShrinkTo (size_t newSize) {
  size_t firstDeadSegment, segmentIndex;

  // Round the size up to the next multiple of segment size.
  if (newSize > 0)
    newSize = ElementsPerSegment() *
              ((newSize + ElementsPerSegment()-1) / ElementsPerSegment());

  // Compute the index of the first segment to be destroyed.
  firstDeadSegment = newSize >> segmentBits;

  // If the array is already smaller than this, then do nothing.
  if (firstDeadSegment >= fNumberOfSegments) return;

  // Destroy all dead segments
  for (segmentIndex = firstDeadSegment;
       segmentIndex < fNumberOfSegments;
       ++segmentIndex) {
    Allocator::deallocate (fSegmentMap[segmentIndex], SegmentSize());
  }

  fNumberOfSegments = firstDeadSegment;

  // When new new size is zero we don't need the segment map
  if (fNumberOfSegments == 0) {
    Allocator::deallocate (fSegmentMap, fMaxSegments * sizeof(DerivedElement *));
    fSegmentMap = NULL;
    fMaxSegments = 0;
  }
}

CS2_ARTEMP inline unsigned long CS2_BASEARDECL::DynamicMemoryUsage() const {
  unsigned long sizeInBytes;

  sizeInBytes = fMaxSegments * sizeof(DerivedElement *);
  sizeInBytes += NumberOfElements() * sizeof(DerivedElement);

  return sizeInBytes;
}

CS2_ARTEMP inline unsigned long CS2_BASEARDECL::MemoryUsage() const {
  return DynamicMemoryUsage() + sizeof(CS2_BASEARDECL);
}


template <class AElementType, class Allocator, size_t segmentBits = 8, class Initializer = AElementType>
  class ArrayOf : public CS2_BASEARDECL {

  using CS2_BASEARDECL::fNumberOfSegments;
  using CS2_BASEARDECL::fSegmentMap;

 public:

  ArrayOf (size_t ignore,
           const Allocator &a = Allocator(),
           const Initializer i = Initializer() ) :
    CS2_BASEARDECL(a) , fInitializer(i), fNumInitialized(0) {
  }

  ArrayOf (const Allocator &a = Allocator(),
           const Initializer i = Initializer() ) :
    CS2_BASEARDECL(a) , fInitializer(i), fNumInitialized(0) {
  }

  ~ArrayOf () {
    MakeEmpty();
  }

  size_t NumberOfElements() const {
    return fNumInitialized;
  }

  /// \brief Copy constructor.
  ArrayOf(ArrayOf<AElementType, Allocator, segmentBits> &inputArray, const Allocator &a = Allocator()) :
    CS2_BASEARDECL(a), fInitializer(inputArray.fInitializer), fNumInitialized(0) {

    GrowTo(inputArray.NumberOfElements());
    Cursor c(*this), ic(inputArray);
    for (c.SetToFirst(), ic.SetToFirst(); c.Valid(); c.SetToNext(), ic.SetToNext())
      new (&*c) typename CS2_BASEARDECL::DerivedElement(*ic);
  }

  ArrayOf<AElementType, Allocator, segmentBits> &
  operator= (const ArrayOf<AElementType, Allocator, segmentBits> &inputArray) {
    MakeEmpty();

    GrowTo(inputArray.NumberOfElements());
    Cursor c(*this), ic(inputArray);
    for (c.SetToFirst(), ic.SetToFirst(); c.Valid(); c.SetToNext(), ic.SetToNext())
        *c = *ic;
  }

  bool
  operator== (const ArrayOf<AElementType, Allocator, segmentBits> &inputArray) const {
    if (NumberOfElements() != inputArray.NumberOfElements()) return false;

    Cursor c(*this), ic(inputArray);
    for (c.SetToFirst(), ic.SetToFirst(); c.Valid(); c.SetToNext(), ic.SetToNext())
      if (!(*c == *ic)) return false;
    return true;
  }

  void GrowTo (size_t newSize) {
    if (newSize <= fNumInitialized) return;
    CS2_BASEARDECL::GrowTo(newSize);

    size_t elementIndex;
    for (elementIndex=fNumInitialized; elementIndex<newSize; elementIndex++) {
      typename CS2_BASEARDECL::DerivedElement *currentElement = CS2_BASEARDECL::DerivedElementAt(elementIndex);
      new (currentElement) typename CS2_BASEARDECL::DerivedElement(fInitializer);
    }
    fNumInitialized = newSize;
  }

  void MakeEmpty() { return ShrinkTo(0); }

  void ShrinkTo(size_t newSize) {
    if (newSize < fNumInitialized) {
      Cursor c(*this);
      for (c.SetTo(newSize); c.Valid(); c.SetToNext())
        (*c.DerivedElement()).CS2_BASEARDECL::DerivedElement::~DerivedElement();

      fNumInitialized = newSize;
      CS2_BASEARDECL::ShrinkTo(newSize);
    }
  }

  const AElementType & operator[] (size_t elementIndex) const {
    return CS2_BASEARDECL::ElementAt(elementIndex);
  }

  AElementType & operator[] (size_t elementIndex) {
    GrowTo (elementIndex + 1);
    return CS2_BASEARDECL::ElementAt(elementIndex);
  }

  AElementType ValueAt(size_t elementIndex) const {
    if (elementIndex >= NumberOfElements())
      return fInitializer;
    else
      return CS2_BASEARDECL::ElementAt(elementIndex);
  }

  void Sort(size_t from, size_t to) {
    if (to<=from+1) return;
    if (to==from+2) {
      if (CS2_BASEARDECL::ElementAt(from+1) < CS2_BASEARDECL::ElementAt(from))
        Swap(CS2_BASEARDECL::ElementAt(from), CS2_BASEARDECL::ElementAt(from+1));
      return;
    }

    // Identify the pivot between the begin/middle/end
    size_t plow=from, pmid=(from+to)/2, phigh=to-1;

    if (CS2_BASEARDECL::ElementAt(phigh) < CS2_BASEARDECL::ElementAt(plow))
      Swap(plow, phigh);
    if (CS2_BASEARDECL::ElementAt(phigh) < CS2_BASEARDECL::ElementAt(pmid))
      Swap(pmid, phigh);
    if (CS2_BASEARDECL::ElementAt(pmid) < CS2_BASEARDECL::ElementAt(plow))
      Swap(pmid, plow);

    if (pmid!=from)
      Swap(CS2_BASEARDECL::ElementAt(pmid), CS2_BASEARDECL::ElementAt(from));

    size_t i,low=from+1, high=to-1;

    AElementType &pivot=CS2_BASEARDECL::ElementAt(from);
    do {
      while (CS2_BASEARDECL::ElementAt(low) < pivot) {
        low+=1;
      }
      while (pivot < CS2_BASEARDECL::ElementAt(high)) {
        high-=1;
      }
      if (low<high) {
        Swap(CS2_BASEARDECL::ElementAt(low), CS2_BASEARDECL::ElementAt(high));
        low+=1;
        high-=1;
      }
    } while (low<high);

    if (pivot < CS2_BASEARDECL::ElementAt(low)) low -=1;
    Swap(pivot, CS2_BASEARDECL::ElementAt(low));

    Sort(from,low);
    Sort(low+1,to);
  }

  void Sort() {
        Sort(0, NumberOfElements());
  }

  template <class ostr>
  friend
  ostr &operator<<  (ostr &out, const CS2_ARDECL &table) {
    Cursor c(table);
    for (c.SetToFirst(); c.Valid(); c.SetToNext())
      out << "[" << int(c) << "]:" << (*c) << "\n";

    return out;
  }


  class Cursor {
  public:
    Cursor(const ArrayOf<AElementType, Allocator, segmentBits, Initializer> &a) : fArray(a), fSegmentIndex(0), fElementIndex(0), fSegment(NULL) {}

    void SetTo(size_t index) {
      fNumSegments = fArray.fNumberOfSegments;
      fSegmentIndex = index / fArray.ElementsPerSegment();
      fElementIndex = index % fArray.ElementsPerSegment();
      if (fSegmentIndex < fNumSegments) {
        fSegment = fArray.fSegmentMap[fSegmentIndex];
        fNumElements = fArray.NumberOfElements() - (fSegmentIndex)*fArray.ElementsPerSegment();
        if (fNumElements > fArray.ElementsPerSegment()) {
          fNumElements = fArray.ElementsPerSegment();
          if (fElementIndex >= fNumElements)
            fSegmentIndex+=1;
        }
      } else
        fNumElements = 0;
    }

    void SetToFirst() {
      fNumSegments = fArray.fNumberOfSegments;
      fSegmentIndex = 0;
      fElementIndex = 0;
      if (fSegmentIndex < fNumSegments) {
        fSegment = fArray.fSegmentMap[fSegmentIndex];
        if (fArray.NumberOfElements() <  fArray.ElementsPerSegment())
          fNumElements = fArray.NumberOfElements();
        else
          fNumElements = fArray.ElementsPerSegment();
      } else
        fNumElements = 0;
    }
    void SetToNext() {
      fElementIndex+=1;
      if (fElementIndex < fNumElements ) return;
      fSegmentIndex+=1;
      if (fSegmentIndex < fNumSegments) {
        fElementIndex = 0;
        fSegment = fArray.fSegmentMap[fSegmentIndex];
        if (fSegmentIndex == fNumSegments-1) // last segment
          fNumElements = fArray.NumberOfElements() - (fSegmentIndex)*fArray.ElementsPerSegment();
      } else
        fNumElements = 0;
    }
    bool Valid() const {
      return fElementIndex <fNumElements;
    }

    bool IsLast() const {
      return (fSegmentIndex == fNumSegments-1 &&
              fElementIndex == fNumElements-1);
    }
    operator uint32_t() const {
      return (fSegmentIndex * fArray.ElementsPerSegment()) + fElementIndex;
    }

    AElementType &operator*() const {
      return (fSegment+fElementIndex)->Element();
    }

    AElementType *operator->() const {
      return &(fSegment+fElementIndex)->Element();
    }

    typename ArrayOf<AElementType, Allocator, segmentBits, Initializer>::DerivedElement *DerivedElement() const {
      return fSegment+fElementIndex;
    }
    Cursor &operator= (const Cursor &);

    friend class ArrayOf<AElementType, Allocator, segmentBits, Initializer>;
    const ArrayOf<AElementType, Allocator, segmentBits, Initializer> &fArray;
    size_t          fNumSegments;
    size_t          fSegmentIndex;
    size_t          fElementIndex;
    size_t          fNumElements;
    typename CS2_BASEARDECL::DerivedElement *fSegment;
  };

  class BackwardsCursor {
  public:
    BackwardsCursor(const ArrayOf<AElementType, Allocator, segmentBits, Initializer> &a) : fArray(a), fSegmentIndex(0), fElementIndex(0), fSegment(NULL) {}

    void SetToFirst() {
      fNumSegments = fArray.fNumberOfSegments;
      if (fNumSegments > 0) {
        fSegmentIndex = fNumSegments-1;
        fSegment = fArray.fSegmentMap[fSegmentIndex];

        fNumElements = fArray.NumberOfElements() - (fSegmentIndex)*fArray.ElementsPerSegment();
        if (fNumElements > fArray.ElementsPerSegment())
          fNumElements = fArray.ElementsPerSegment();
      } else
        fNumElements = 0;
      fElementIndex = fNumElements;
    }
    void SetToNext() {
      fElementIndex-=1;
      if (fElementIndex > 0 ) return;
      if (fSegmentIndex > 0)  {
        fSegmentIndex-=1;
        fSegment = fArray.fSegmentMap[fSegmentIndex];
        fNumElements = fArray.ElementsPerSegment();
      } else
        fNumElements = 0;
      fElementIndex = fNumElements;
    }
    bool Valid() const {
      return fElementIndex >0;
    }

    bool IsLast() const {
      return (fSegmentIndex == 0  && fElementIndex == 1);
    }
    operator uint32_t() const {
      return (fSegmentIndex * fArray.ElementsPerSegment()) + fElementIndex - 1;
    }

    AElementType &operator*() const {
      return (fSegment+fElementIndex-1)->Element();
    }

    AElementType *operator->() const {
      return &(fSegment+fElementIndex-1)->Element();
    }

    typename ArrayOf<AElementType, Allocator, segmentBits, Initializer>::DerivedElement *DerivedElement() const {
      return fSegment+fElementIndex-1;
    }
    BackwardsCursor &operator= (const BackwardsCursor &);

    friend class ArrayOf<AElementType, Allocator, segmentBits, Initializer>;
    const ArrayOf<AElementType, Allocator, segmentBits, Initializer> &fArray;
    size_t          fNumSegments;
    size_t          fSegmentIndex;
    size_t          fElementIndex;
    size_t          fNumElements;
    typename CS2_BASEARDECL::DerivedElement *fSegment;
  };


  // STL style iteration - bare-bones, input iteration at this point
  // TODO: cache the segments like Cursor?
  // TODO: reverse iterator?
  // TODO: const iterator?
  //
  class iterator {
    public:
    iterator(const ArrayOf<AElementType, Allocator, segmentBits, Initializer> &a, size_t index = 0)
        : fArray(a), fCursor(index)
      {}

    // Cast to integer operator - returns the current index being pointed to
    operator uint32_t() const { return fCursor; }

    // Dereference operator - returns the element being pointed to
    AElementType &operator*() const { return fArray.ElementAt(fCursor); }

    // Member access operator
    AElementType *operator->() const { return & (this->operator*()); }

    // pre-increment
    iterator& operator++()    { fCursor++; return *this; }

    // TODO: post-increment

    // comparison
    bool operator==(const iterator &other) const { return this == &other ||
                                                   (fArray == other.fArray &&
                                                    fCursor == other.fCursor); }
    bool operator!=(const iterator &other) const { return ! operator==(other); }

  private:
    const ArrayOf<AElementType, Allocator, segmentBits, Initializer> &fArray;
    size_t fCursor;
  };

  // returns the iterator to the first element
  iterator begin() { return iterator(*this); }

  // returns the iterator to the one-past-the-last element
  iterator end() { return iterator(*this, NumberOfElements()); }

private:
  AElementType fInitializer;
  uint32_t     fNumInitialized;
};

template <class AElementType, class Allocator, size_t segmentBits = 8, class Initializer = AElementType>
  class StaticArrayOf : public ArrayOf<AElementType, Allocator, segmentBits, Initializer> {
  public:
    StaticArrayOf (size_t size,
             const Allocator &a = Allocator(),
             const Initializer i = Initializer() ) :
     ArrayOf<AElementType, Allocator, segmentBits, Initializer>(a,i) {
       ArrayOf<AElementType, Allocator, segmentBits, Initializer>::GrowTo(size);
    }

    AElementType & operator[] (size_t elementIndex) const {
      CS2Assert (elementIndex < CS2_ARDECL::NumberOfElements(),
                 ("Index %lu does not exist (>%lu)", elementIndex, CS2_ARDECL::NumberOfElements()));

      return CS2_BASEARDECL::ElementAt(elementIndex);
    }
  };
}

#undef CS2_ARTEMP
#undef CS2_BASEARDECL
#undef CS2_ARDECL
#ifdef CS2_ALLOCINFO
#undef allocate
#undef deallocate
#undef reallocate
#endif

#endif // CS2_ARRAYOF_H
