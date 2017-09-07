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

#ifndef CS2_TABLEOF_H
#define CS2_TABLEOF_H

#include "cs2/arrayof.h"
#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"

namespace CS2{

// ------------------------------------------------------------------------
// TableOf
//
// An extensible table class.  A table object has the ability to add and
// remove elements and to reclaim freed storage.  Element objects are
// constructed or copy-constructed when added and destroyed when removed
// or when the table is destroyed.
// ------------------------------------------------------------------------

typedef size_t TableIndex;

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits = 8,
  template< class = Allocator > class SupportingBitVector = ASparseBitVector
  >
class TableOf {
  public:

  TableOf (const Allocator &a = Allocator());
  TableOf (uint32_t ignore, const Allocator &a = Allocator());
  ~TableOf();
  TableOf (const TableOf &);
  TableOf &operator= (const TableOf &);

  /// \brief Return the element at the given index.  The element must have been
  /// previously added and not subsequently removed from the table.
  AElementType &operator[] (TableIndex) const;

  /// \brief Exactly the same as operator[].  For consistency with the ArrayOf
  /// class.
  AElementType &ElementAt (TableIndex) const;

  /// \brief Add a table entry at the next available position and return its index.
  TableIndex AddEntry();
  TableIndex AddEntryNoConstruct();

  template <class Initializer>
    TableIndex AddEntry(const Initializer &element);

  TableIndex AddEntryAtPosition(TableIndex);
  TableIndex AddEntryAtPositionNoConstruct(TableIndex);

  template <class Initializer>
  TableIndex AddEntryAtPosition(TableIndex, Initializer element);

  /// \brief Remove an entry at a given position.
  void RemoveEntry (TableIndex);

  /// \return Current total number of table entries
  size_t NumberOfElements() const;

  /// \brief Determine if an entry exists at a given position.
  bool Exists (TableIndex) const;

  /// \brief Determine if the table has any entries
  bool IsEmpty() const;

  /// \brief Remove all entries from the table
  void MakeEmpty ();

  /// \brief Return the number of bytes of memory used by this table
  unsigned long MemoryUsage() const;

  // The following is a sub-class used to traverse tables.
  class ConstCursor {
    public:

    ConstCursor (const TableOf &);
    ConstCursor (const TableOf &, const SupportingBitVector<Allocator> &);
    ConstCursor (const ConstCursor &c);
    ConstCursor (const ConstCursor &c, const SupportingBitVector<Allocator> &);

    // Set the cursor position
    void SetToFirst();
    void SetToNext();

    /// \brief Determine if the cursor points at a valid position
    bool Valid() const;

    /// \brief Convert the cursor to integer.  Used to actually index the table.
    operator TableIndex() const;

    private:

    ConstCursor &operator= (const ConstCursor &);

    const TableOf &fTable;
    typename SupportingBitVector<Allocator>::Cursor fFreeCursor;
    TableIndex fNextFree;
    TableIndex fIndex;
  };

  class Cursor : public ConstCursor {
  public:
    Cursor (const TableOf &);
    Cursor (const Cursor &);

  private:
    SupportingBitVector<Allocator> fFreeVector;
  };

  template <class ostr>
  friend
  ostr &operator<<  (ostr &out, const TableOf &table) {
    typename TableOf::ConstCursor tblCursor(table);

    for (tblCursor.SetToFirst();
         tblCursor.Valid();
         tblCursor.SetToNext()) {
      out << "[" << (TableIndex) tblCursor << "]: ";
      out << table.ElementAt(tblCursor);
      out << "\n";
    }

    return out;
  }

  private:
  TableIndex ClearLastOneIfThereIsOne(bool&);
  bool CheckEntryAtPosition(TableIndex);
  void DestroyElements();

  typedef BaseArrayOf< AElementType, Allocator, segmentBits > StorageProvider;
  StorageProvider fStorage;
  TableIndex fHighestIndex;
  TableIndex fLowestPossibleRemoved;
  TableIndex fHighestPossibleRemoved;
  SupportingBitVector<Allocator> fFreeVector;
};

// the logic in here should really be in a helper template class:
// one helper for each of ASparseBitVector and ABitVector
// we are keeping track of the lowest and the highest possible
// bits in the bit vector in order to speed up ABitVector
// for ASparseBitVector, that is not necessary.
// The original implementation preferred to re-use elements starting
// with the highest free-ed element.  So this code maintains that discipline
template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableIndex
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ClearLastOneIfThereIsOne(bool& foundone) {
   foundone = false;
   if (fLowestPossibleRemoved == fHighestPossibleRemoved) {
      // in this case either we really do have a free spot here or we there are no free spots
      TableIndex possibleSlot = fHighestPossibleRemoved;
      if (fFreeVector.ValueAt(possibleSlot)) {
         // this is the last free one
         fFreeVector[possibleSlot] = false;
         // not sure this is necessary...worried about fFreeVector shrinking
         fLowestPossibleRemoved = 0;
         fHighestPossibleRemoved = 0;
         foundone = true;
         return possibleSlot;
      } else {
         return ~(TableIndex) 0;
      }
   } else {
      if (fFreeVector.ValueAt(fHighestPossibleRemoved)) {
         // save ourselves the trouble of a full search
         CS2Assert(fLowestPossibleRemoved < fHighestPossibleRemoved, ("fLowestPossibleRemoved not less than highest"));
         fFreeVector[fHighestPossibleRemoved] = false;
         TableIndex rc = fHighestPossibleRemoved;
         --fHighestPossibleRemoved;
         foundone = true;
         return rc;
      } else {
         TableIndex lastOne = fFreeVector.ClearLastOneIfThereIsOneInRange(fLowestPossibleRemoved, fHighestPossibleRemoved, foundone);
         if (foundone) {
            CS2Assert(lastOne >= fLowestPossibleRemoved && lastOne <= fHighestPossibleRemoved, ("found a removed node less than expected"));
            if (lastOne == fLowestPossibleRemoved) {
               fHighestPossibleRemoved = fLowestPossibleRemoved;
            } else {
               fHighestPossibleRemoved = lastOne - 1;
            }
         }
         return lastOne;
      }
   }
}

// TableOf::ElementAt
//
// Indexing method.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
AElementType &TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ElementAt (TableIndex index) const {
  CS2Assert (Exists(index), ("Table index " CS2_ZU " does not exist", index));
  return fStorage.ElementAt(index);
}

// TableOf::operator[]
//
// Indexing operator.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
AElementType
&TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::operator[] (TableIndex index) const {
  return ElementAt(index);
}

// TableOf::Exists
//
// Predicate to determine if an element exists.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
bool
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::Exists (TableIndex index) const {
  if (index == 0) return false;
  if (index > fHighestIndex) return false;
  return !fFreeVector.ValueAt(index);
}

// TableOf::IsEmpty
//
// Predicate to determine if the table contains any elements.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
bool
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::IsEmpty() const {
  return (fHighestIndex == 0) || fFreeVector.PopulationCount()>=fHighestIndex;
}

// TableOf::TableOf
//
// Construct a table with at least the given number of elements and with
// the (optional) segment size.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::TableOf (const Allocator &a) :
  fStorage(a),
  fHighestIndex (0),
  fLowestPossibleRemoved(0),
  fHighestPossibleRemoved(0),
  fFreeVector(a) { }

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::TableOf (uint32_t, const Allocator &a) :
  fStorage(a),
  fHighestIndex (0),
  fLowestPossibleRemoved(0),
  fHighestPossibleRemoved(0),
  fFreeVector(a) { }

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
void
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::DestroyElements() {
   ConstCursor elementIndex(*this);
   // Destroy every existing element.
   for (elementIndex.SetToFirst();
        elementIndex.Valid();
        elementIndex.SetToNext()) {
     auto derivedElement = fStorage.DerivedElementAt(elementIndex);
     derivedElement->~DerivedElement();
   }
}

// TableOf::~TableOf
//
// Destroy a table and all of its elements.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::~TableOf() {
  if (is_pod<AElementType>()) return;

  if (fHighestIndex==0) return;

  DestroyElements();
}

// TableOf::TableOf (const TableOf &)
//
// Copy construct a table.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::TableOf (const TableOf & table) :
  fStorage(table),
  fHighestIndex(table.fHighestIndex),
  fLowestPossibleRemoved(table.fLowestPossibleRemoved),
  fHighestPossibleRemoved(table.fHighestPossibleRemoved),
  fFreeVector(table.fFreeVector) {

  ConstCursor elementIndex(table);
  // Copy every existing element.
  for (elementIndex.SetToFirst();
       elementIndex.Valid();
       elementIndex.SetToNext()) {
    new (&fStorage.DerivedElementAt(elementIndex)) typename StorageProvider::DerivedElement(table.ElementAt(elementIndex));
  }
}

// TableOf::operator=
//
// Assign a table to another.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector > &
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::operator= (const TableOf &table) {
  MakeEmpty();

  fHighestIndex = table.fHighestIndex;
  fLowestPossibleRemoved = table.fLowestPossibleRemoved;
  fHighestPossibleRemoved = table.fHighestPossibleRemoved;
  fFreeVector = table.fFreeVector;

  ConstCursor elementIndex(table);
  // Copy every existing element.
  for (elementIndex.SetToFirst();
       elementIndex.Valid();
       elementIndex.SetToNext()) {
    new (fStorage.DerivedElementAt(elementIndex)) typename StorageProvider::DerivedElement(table.ElementAt(elementIndex));
  }
  return *this;
}

// TableOf::MakeEmpty
//
// Destroy all existing table elements.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
void TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::MakeEmpty () {
  DestroyElements();

  fHighestIndex = 0;
  fLowestPossibleRemoved = 0;
  fHighestPossibleRemoved = 0;
  fFreeVector.Clear();
  fStorage.ShrinkTo(0);
}

// TableOf::AddEntry
//
// Add an entry to the table at the next available index without constructing
// the object. ONLY USE THIS IF YOU ARE GOING TO EXPLICITLY CONSTRUCT THE
// THE DerivedElement YOURSELF.
template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableIndex TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::AddEntryNoConstruct () {
  TableIndex newIndex;
  bool foundone;

  newIndex = ClearLastOneIfThereIsOne(foundone);
  while (foundone) {
    if (newIndex <=fHighestIndex) goto found;
    newIndex = ClearLastOneIfThereIsOne(foundone);
  }

  newIndex = fHighestIndex + 1;
  fStorage.GrowTo (newIndex + 1);
  fHighestIndex = newIndex;

found:
  return newIndex;
}

// Add an entry to the table at the next available index
template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableIndex TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::AddEntry () {
  // Construct the new table entry
  TableIndex newIndex = AddEntryNoConstruct();
  new (fStorage.DerivedElementAt(newIndex)) typename StorageProvider::DerivedElement;
  return newIndex;
}

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
template <class Initializer>
TableIndex TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::AddEntry (const Initializer &initializer) {
  // Construct the new table entry
  TableIndex newIndex = AddEntryNoConstruct();
  new (fStorage.DerivedElementAt(newIndex)) typename StorageProvider::DerivedElement(initializer);
  return newIndex;
}

// TableOf::AddEntryAtPosition
//
// Add an entry to the table at the specified position without constructing
// the object. ONLY USE THIS IF YOU ARE GOING TO EXPLICITLY CONSTRUCT THE
// THE DerivedElement YOURSELF.
template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
bool TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::CheckEntryAtPosition (TableIndex newIndex) {
  bool shouldConstruct = true;
  if (fHighestIndex < newIndex) {
    TableIndex const currentHighIndex = fHighestIndex;
    fStorage.GrowTo(newIndex + 1);
    if (fHighestIndex < newIndex-1) {
      bool updateLowestRemoved = false;
       if (fLowestPossibleRemoved == fHighestPossibleRemoved &&
           !fFreeVector.ValueAt(fLowestPossibleRemoved)) {
         updateLowestRemoved = true;
       }
       while (fHighestIndex < newIndex-1) {
        /*
         * Each of these bit writes can throw so we have to
         * keep track of our incremental progress :(
         */
         fFreeVector[fHighestIndex+1]=true;
         ++fHighestIndex;
         fHighestPossibleRemoved = fHighestIndex;
       }
      if (updateLowestRemoved) {
         fLowestPossibleRemoved = currentHighIndex + 1;
      }
    }
    fHighestIndex = newIndex;
  } else if (fFreeVector.ValueAt(newIndex)) {
    fFreeVector[newIndex]=false;
    if (newIndex == fHighestPossibleRemoved) {
       if (fLowestPossibleRemoved == fHighestPossibleRemoved) {
          fLowestPossibleRemoved = 0;
          fHighestPossibleRemoved = 0;
       } else {
          --fHighestPossibleRemoved;
       }
    } else if (newIndex == fLowestPossibleRemoved) {
       // lowest cannot == highest in this case
       ++fLowestPossibleRemoved;
    }
  } else {
    CS2Assert (!Exists(newIndex), ("Trying to add index " CS2_ZU " on top of existing element", newIndex));
    shouldConstruct = false;
  }

  return shouldConstruct;
}

// Add an entry to the table at the specified position.
template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableIndex TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::AddEntryAtPositionNoConstruct (TableIndex newIndex) {
  bool ignore = CheckEntryAtPosition(newIndex);
  return newIndex;
}

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableIndex TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::AddEntryAtPosition (TableIndex newIndex) {
  if (CheckEntryAtPosition(newIndex)) {
     // Construct the new table entry
     new (fStorage.DerivedElementAt(newIndex)) typename StorageProvider::DerivedElement;
  }
  return newIndex;
}

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
 template <class Initializer>
  TableIndex TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::AddEntryAtPosition (TableIndex newIndex, Initializer initializer) {
  if (CheckEntryAtPosition(newIndex)) {
     // Construct the new table entry
     new (fStorage.DerivedElementAt(newIndex)) typename StorageProvider::DerivedElement(initializer);
  }
  return newIndex;
}

// TableOf::RemoveEntry
//
// Remove the given entry from the table.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
void TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::RemoveEntry (TableIndex index) {
  // The entry must already exist
  CS2Assert (Exists(index), ("Index " CS2_ZU " does not exist", index));

  if (index==0 || index > fHighestIndex) return;

  // Destroy this element
  auto derivedElement = fStorage.DerivedElementAt(index);
  derivedElement->~DerivedElement();

  if (index==fHighestIndex) {
     fHighestIndex-=1;
  } else {
     fFreeVector[index] = true;
     if (fLowestPossibleRemoved == fHighestPossibleRemoved &&
         !fFreeVector.ValueAt(fLowestPossibleRemoved)) {
        fLowestPossibleRemoved = index;
        fHighestPossibleRemoved = index;
     } else {
        if (index < fLowestPossibleRemoved) fLowestPossibleRemoved = index;
        if (index > fHighestPossibleRemoved) fHighestPossibleRemoved = index;
     }
  }
}

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
size_t
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::NumberOfElements() const {
  return fStorage.NumberOfElements();
  }

// TableOf::MemoryUsage
//
// Return the memory usage in bytes for this table.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
unsigned long TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::MemoryUsage() const {
  unsigned long sizeInBytes;

  sizeInBytes = fStorage.DynamicMemoryUsage();
  sizeInBytes += fFreeVector.MemoryUsage();
  sizeInBytes += sizeof(TableOf);

  return sizeInBytes;
}

/// Cursor class
////////////////////////////////////////////////////////////////////////

// TableOf::Cursor::Cursor
//
// Construct a table cursor.

  template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::ConstCursor (const TableOf &aTable) :
  fTable(aTable),
  fFreeCursor(aTable.fFreeVector),
  fNextFree(0),
  fIndex(0) {}

 template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::ConstCursor(
  const TableOf &aTable,
  const SupportingBitVector<Allocator> &fv
  ) :
  fTable(aTable),
  fFreeCursor(fv),
  fNextFree(0),
  fIndex(0) {}

  template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::Cursor::Cursor (const TableOf &aTable) :
    fFreeVector(aTable.fFreeVector),
    ConstCursor(aTable, fFreeVector) {}

// TableOf::Cursor::Cursor (const TableOf::Cursor &)
//
// Copy construct a table cursor.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::ConstCursor(
  const ConstCursor &inputCursor
  ) :
  fTable(inputCursor.fTable),
  fFreeCursor(inputCursor.fFreeCursor),
  fIndex(inputCursor.fIndex),
  fNextFree(inputCursor.fNextFree) {
}

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::ConstCursor(
  const ConstCursor &inputCursor,
  const SupportingBitVector<Allocator> &fv
  ) :
  fTable(inputCursor.fTable),
  fFreeCursor(fv),
  fIndex(inputCursor.fIndex),
  fNextFree(inputCursor.fNextFree) {
  fFreeCursor.SetToNextOneAfter(fIndex);
}

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::Cursor::Cursor(const Cursor &inputCursor) :
  fFreeVector(inputCursor.fFreeVector),
  ConstCursor(inputCursor, fFreeVector) {
}

// TableOf::Cursor::SetToFirst
//
// Set the cursor to the first allocated element in the table.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
void TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::SetToFirst() {
  fIndex=1;
  fFreeCursor.SetToFirstOne();
  if (fFreeCursor.Valid()){
    fNextFree = fFreeCursor;
    if (fFreeCursor==fIndex) {fIndex=0 ; return SetToNext();}
    if (fNextFree <= fTable.fHighestIndex) return;
  }
  fNextFree=fTable.fHighestIndex+1;
}

// TableOf::Cursor::SetToNext
//
// Set the cursor to the next allocated element in the table.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
void TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::SetToNext() {
  fIndex+=1;
  if (fIndex<fNextFree) return;

  if (fFreeCursor.Valid()){
    while (fFreeCursor.Valid()) {
      if (fFreeCursor > fIndex) {
	fNextFree=fFreeCursor;
	return;
      }
      fIndex+=1;
      fFreeCursor.SetToNextOne();
    }
  }
  fNextFree=fTable.fHighestIndex+1;
}

// TableOf::Cursor::Valid
//
// Predicate to determine if the cursor points at a valid element.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
bool TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::Valid() const {
  return fIndex<fNextFree;
}

// TableOf::Cursor::operator TableIndex
//
// Return the index of the element pointed to by the cursor.

template <
  class AElementType,
  class Allocator,
  uint32_t segmentBits,
  template<class> class SupportingBitVector
  >
TableOf< AElementType, Allocator, segmentBits, SupportingBitVector >::ConstCursor::operator TableIndex() const {
  return fIndex;
}

}
#endif // CS2_TABLEOF_H
