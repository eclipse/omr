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

#ifndef CS2_LISTOF_H
#define CS2_LISTOF_H
#define IN_CS2_LISTOF_H

// ------------------------------------------------------------------------
// ListOf
//
// Extensible list data structure.  This is a slightly augmented form of
// the extensible array structure.  It includes methods for adding elements
// and for making the list empty.  Elements are assumed to be stored in
// indices 0..NumberOfElements-1.
// ------------------------------------------------------------------------

#include "cs2/arrayof.h"

namespace CS2 {
#define CS2_LI_TEMP template <class AElementType, class Allocator, uint32_t segmentBits>
#define CS2_LI_DECL ListOf <AElementType, Allocator, segmentBits>
#define CS2_LIC_DECL CS2_LI_DECL::Cursor

#define CS2_AR_DECL BaseArrayOf<AElementType, Allocator, segmentBits>

typedef uint32_t ListIndex;

template <class AElementType, class Allocator, uint32_t segmentBits = 8>
 class ListOf : public BaseArrayOf<AElementType, Allocator, segmentBits>  {
  public:

  ListOf (const Allocator &a = Allocator()) :
    BaseArrayOf<AElementType, Allocator, segmentBits> (a),
    fNextAvailable(0), fInitialSize () {}

  ListOf (uint32_t ignore, const Allocator &a = Allocator()) :
    BaseArrayOf<AElementType, Allocator, segmentBits> (a),
    fNextAvailable(0), fInitialSize () {}

  ~ListOf ();

  ListOf (const CS2_LI_DECL &);
  CS2_LI_DECL &operator= (const CS2_LI_DECL &);

  // Allocate a new entry in the list.
  ListIndex Add ();

  // Add a new member to the list with the given value.
  template <class Initializer>
  ListIndex Add (const Initializer &);

  // Return a reference to the given indexed member of the list.
  // The index must be in the range 0..NumberOfElements-1
  AElementType &operator[] (ListIndex) const;
  AElementType &ElementAt (ListIndex) const;

  // Nullify the list.
  void MakeEmpty (bool freeStorage = true);

  // Check if the list is empty.
  bool IsEmpty() const;

  // Return the number of bytes of memory used for this list.
  unsigned long MemoryUsage() const;

  // Return the number of elements in the list.
  uint32_t NumberOfElements() const;

  // Append from the given list
  void Append (const CS2_LI_DECL &);

  // Print out the list

  template <class ostr>
       friend ostr &operator<< (ostr &out, const CS2_LI_DECL &list) {
    uint32_t listIndex;

    for (listIndex = 0;
	 listIndex < list.NumberOfElements();
	 ++listIndex) {
      out << "[" << listIndex << "]:" ;
      out << list.ElementAt(listIndex) << "\n";
    }

    return out;
  }

  class Cursor {
  public:
    Cursor(const CS2_LI_DECL &a) : fList(a), fSegmentIndex(0), fElementIndex(0), fSegment(NULL) {}

    void SetTo(size_t index) {
      fNumSegments = fList.fNumberOfSegments;
      fSegmentIndex = index / fList.ElementsPerSegment();
      fElementIndex = index % fList.ElementsPerSegment();
      if (fSegmentIndex < fNumSegments) {
        fSegment = fList.fSegmentMap[fSegmentIndex];
        fNumElements = fList.NumberOfElements() - (fSegmentIndex)*fList.ElementsPerSegment();
        if (fNumElements > fList.ElementsPerSegment()) {
          fNumElements = fList.ElementsPerSegment();
          if (fElementIndex >= fNumElements)
            fSegmentIndex+=1;
        }
      } else
	fNumElements = 0;
    }

    void SetToFirst() {
      fNumSegments = fList.fNumberOfSegments;
      fSegmentIndex = 0;
      fElementIndex = 0;
      if (fSegmentIndex < fNumSegments) {
        fSegment = fList.fSegmentMap[fSegmentIndex];
        if (fList.NumberOfElements() <  fList.ElementsPerSegment())
          fNumElements = fList.NumberOfElements();
        else
          fNumElements = fList.ElementsPerSegment();
      } else
	fNumElements = 0;
    }
    void SetToNext() {
      fElementIndex+=1;
      if (fElementIndex < fNumElements ) return;
      fSegmentIndex+=1;
      if (fSegmentIndex < fNumSegments) {
        fElementIndex = 0;
        fSegment = fList.fSegmentMap[fSegmentIndex];
        if (fSegmentIndex == fNumSegments-1) // last segment
          fNumElements = fList.NumberOfElements() - (fSegmentIndex)*fList.ElementsPerSegment();
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
      return (fSegmentIndex * fList.ElementsPerSegment()) + fElementIndex;
    }

    AElementType &Data() const {
      return (fSegment+fElementIndex)->Element();
    }
    AElementType &operator*() const { return Data(); }
    AElementType *operator->() const { return &Data(); }

    typename CS2_LI_DECL::DerivedElement *DerivedElement() const {
      return fSegment+fElementIndex;
    }
    Cursor &operator= (const Cursor &);

    const CS2_LI_DECL &fList;
    size_t          fNumSegments;
    size_t          fSegmentIndex;
    size_t          fElementIndex;
    size_t          fNumElements;
    typename CS2_LI_DECL::DerivedElement *fSegment;
  };

  protected:

  uint32_t fNextAvailable;
  uint32_t fInitialSize;
};

// ListOf::Add
//
// Add an initialized entry to the list and return its index.

CS2_LI_TEMP inline ListIndex CS2_LI_DECL::Add() {
  ListIndex newIndex;

  newIndex = fNextAvailable++;

  // Construct the new element
  AElementType *newElement = & CS2_AR_DECL::operator[] (newIndex);
  new (newElement) typename CS2_AR_DECL::DerivedElement();

  return newIndex;
}

// ListOf::Add (const AElementType &)
//
// Add an entry to the list with the given value and return its index.

CS2_LI_TEMP
template <class Initializer>
inline ListIndex CS2_LI_DECL::Add (const Initializer &inputElement) {
  ListIndex newIndex;

  newIndex = fNextAvailable++;

  // Copy construct the new element
  AElementType *newElement = & CS2_AR_DECL::operator[] (newIndex);
  new (newElement) typename CS2_AR_DECL::DerivedElement(inputElement);

  return newIndex;
}

// ListOf::ElementAt
//
// Indexing method.

CS2_LI_TEMP inline AElementType &CS2_LI_DECL::ElementAt (ListIndex index) const {
  CS2Assert (index < NumberOfElements(), ("List index out of range: %d", index));
  return CS2_AR_DECL::ElementAt(index);
}

// ListOf::operator[]
//
// Indexing operator.

CS2_LI_TEMP inline AElementType &CS2_LI_DECL::operator[] (ListIndex index) const {
  return ElementAt(index);
}

// ListOf::IsEmpty
//
// Predicate to determine if the list is empty.

CS2_LI_TEMP inline bool CS2_LI_DECL::IsEmpty() const {
  return (fNextAvailable == 0);
}

// ListOf::NumberOfElements
//
// The number of allocated elements in the list.

CS2_LI_TEMP inline uint32_t CS2_LI_DECL::NumberOfElements() const {
  return fNextAvailable;
}

// ListOf::~ListOf
//
// Destroy a list and any currently allocated elements.

CS2_LI_TEMP inline
CS2_LI_DECL::~ListOf() {
  ListIndex listIndex;

  // Destroy the existing elements.
  for (listIndex = 0; listIndex < NumberOfElements(); ++listIndex) {
    typename CS2_AR_DECL::DerivedElement *derivedElement = (typename CS2_AR_DECL::DerivedElement *) & ElementAt(listIndex);
    derivedElement->CS2_AR_DECL::DerivedElement::~DerivedElement();
  }
}

// ListOf::ListOf
//
// Copy construct a list.

CS2_LI_TEMP inline
CS2_LI_DECL::ListOf (const CS2_LI_DECL &list) :
CS2_AR_DECL(list, list.allocator()),
fNextAvailable(list.fNextAvailable),
fInitialSize(list.fInitialSize) {
  ListIndex listIndex;

  for (listIndex = 0; listIndex < NumberOfElements(); ++listIndex) {
    new (&ElementAt(listIndex)) typename CS2_AR_DECL::DerivedElement (list.ElementAt(listIndex));
  }
}

// ListOf::operator=
//
// Assign a list to another.

CS2_LI_TEMP inline
CS2_LI_DECL &CS2_LI_DECL::operator= (const CS2_LI_DECL &list) {
  ListIndex listIndex;
  uint32_t minElements;

  minElements = NumberOfElements();

  if (minElements > list.NumberOfElements())
    minElements = list.NumberOfElements();

  this->GrowTo (list.NumberOfElements());

  for (listIndex = 0; listIndex < minElements; ++listIndex) {
    ElementAt(listIndex) = list.ElementAt(listIndex);
  }

  if (NumberOfElements() > minElements) {
    // Destroy extra list members
    for (; listIndex < NumberOfElements(); ++listIndex) {
      typename CS2_AR_DECL::DerivedElement *currentElement = CS2_AR_DECL::DerivedElementAt(listIndex);
      currentElement->CS2_AR_DECL::DerivedElement::~DerivedElement();
    }
  } else {
    // Copy construct additional list members.
    for (; listIndex < list.NumberOfElements(); ++listIndex) {
      typename CS2_AR_DECL::DerivedElement *currentElement = CS2_AR_DECL::DerivedElementAt(listIndex);
      new (currentElement) AElementType (list.ElementAt(listIndex));
    }
  }

  fNextAvailable = list.fNextAvailable;

  return *this;
}

// ListOf::MakeEmpty
//
// Remove all existing elements.

CS2_LI_TEMP inline
void CS2_LI_DECL::MakeEmpty (bool freeStorage) {
  ListIndex listIndex;

  // Destroy the existing elements.
  for (listIndex = 0; listIndex < NumberOfElements(); ++listIndex) {
    typename CS2_AR_DECL::DerivedElement *derivedElement = (typename CS2_AR_DECL::DerivedElement *) & ElementAt(listIndex);
    derivedElement->CS2_AR_DECL::DerivedElement::~DerivedElement();
  }

  fNextAvailable = 0;

  if (freeStorage) {
    // Shrink the base array to the initial size.
    this->ShrinkTo (fInitialSize);
  }
}

// ListOf::MemoryUsage
//
// The memory usage in bytes for the list.

CS2_LI_TEMP inline
unsigned long CS2_LI_DECL::MemoryUsage() const {
  unsigned long sizeInBytes;

  sizeInBytes = CS2_AR_DECL::MemoryUsage();
  sizeInBytes += sizeof(CS2_LI_DECL) - sizeof(CS2_AR_DECL);

  return sizeInBytes;
}

// ListOf::Append
//
// Append from the given list.

CS2_LI_TEMP inline
void CS2_LI_DECL::Append (const CS2_LI_DECL &list) {
  uint32_t listIndex;

  this->GrowTo (list.NumberOfElements());

  for (listIndex = 0;
       listIndex < list.NumberOfElements();
       ++listIndex) {
    Add (list.ElementAt(listIndex));
  }
}

#undef CS2_LI_TEMPARGS
#undef CS2_LI_TEMP
#undef CS2_LI_DECL
#undef CS2_LIC_DECL

}

#endif // CS2_LISTOF_H
