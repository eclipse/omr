/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1996, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

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

#define CS2_TBL_TEMP template <class AElementType, class Allocator, uint32_t segmentBits, template<class> class SupportingBitVector>
#define CS2_TBL_DECL TableOf <AElementType, Allocator, segmentBits, SupportingBitVector>
#define CS2_TAR_DECL BaseArrayOf <AElementType, Allocator, segmentBits>

typedef size_t TableIndex;

template <class AElementType, class Allocator, uint32_t segmentBits = 8, template<class = Allocator>class SupportingBitVector = ASparseBitVector>
class TableOf : public CS2_TAR_DECL {
  public:

  TableOf (const Allocator &a = Allocator());
  TableOf (uint32_t ignore, const Allocator &a = Allocator());
  ~TableOf();
  TableOf (const CS2_TBL_DECL &);
  CS2_TBL_DECL &operator= (const CS2_TBL_DECL &);

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
    TableIndex AddEntry(Initializer &element);

  TableIndex AddEntryAtPosition(TableIndex);
  TableIndex AddEntryAtPositionNoConstruct(TableIndex);

  template <class Initializer>
  TableIndex AddEntryAtPosition(TableIndex, Initializer element);

  /// \brief Remove an entry at a given position.
  void RemoveEntry (TableIndex);

  /// \brief Determine if an entry exists at a given position.
  bool Exists (TableIndex) const;

  /// \brief Determine if the table has any entries
  bool IsEmpty() const;

  /// \brief Remove all entries from the table
  void MakeEmpty ();

  /// \brief Return the number of bytes of memory used by this table
  unsigned long MemoryUsage() const;

  // The following is a sub-class used to traverse tables.
  #define CS2_TBLCC_DECL CS2_TBL_DECL::ConstCursor
  class ConstCursor {
    public:

    ConstCursor (const CS2_TBL_DECL &);
    ConstCursor (const CS2_TBL_DECL &, const SupportingBitVector<Allocator> &);
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

    const CS2_TBL_DECL &fTable;
    typename SupportingBitVector<Allocator>::Cursor fFreeCursor;
    TableIndex fNextFree;
    TableIndex fIndex;
  };

  #define CS2_TBLC_DECL CS2_TBL_DECL::Cursor
  class Cursor : public ConstCursor {
  public:
    Cursor (const CS2_TBL_DECL &);
    Cursor (const Cursor &);

  private:
    SupportingBitVector<Allocator> fFreeVector;
  };

  template <class ostr>
  friend
  ostr &operator<<  (ostr &out, const CS2_TBL_DECL &table) {
    typename CS2_TBL_DECL::ConstCursor tblCursor(table);

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

  TableIndex fHighestIndex;
  TableIndex fLowestPossibleRemoved;
  TableIndex fHighestPossibleRemoved;
  TableIndex ClearLastOneIfThereIsOne(bool&);
  bool       CheckEntryAtPosition(TableIndex);
  SupportingBitVector<Allocator> fFreeVector;
};

// the logic in here should really be in a helper template class:
// one helper for each of ASparseBitVector and ABitVector
// we are keeping track of the lowest and the highest possible
// bits in the bit vector in order to speed up ABitVector
// for ASparseBitVector, that is not necessary.
// The original implementation preferred to re-use elements starting
// with the highest free-ed element.  So this code maintains that discipline
CS2_TBL_TEMP inline TableIndex CS2_TBL_DECL::ClearLastOneIfThereIsOne(bool& foundone) {
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

CS2_TBL_TEMP inline AElementType &CS2_TBL_DECL::ElementAt (TableIndex index) const {
  CS2Assert (Exists(index), ("Table index " CS2_ZU " does not exist", index));
  return CS2_TAR_DECL::ElementAt(index);
}

// TableOf::operator[]
//
// Indexing operator.

CS2_TBL_TEMP inline AElementType &CS2_TBL_DECL::operator[] (TableIndex index) const {
  return ElementAt(index);
}

// TableOf::Exists
//
// Predicate to determine if an element exists.

CS2_TBL_TEMP inline bool CS2_TBL_DECL::Exists (TableIndex index) const {
  if (index == 0) return false;
  if (index > fHighestIndex) return false;
  return !fFreeVector.ValueAt(index);
}

// TableOf::IsEmpty
//
// Predicate to determine if the table contains any elements.

CS2_TBL_TEMP inline bool CS2_TBL_DECL::IsEmpty() const {
  return (fHighestIndex == 0) || fFreeVector.PopulationCount()>=fHighestIndex;
}

// TableOf::TableOf
//
// Construct a table with at least the given number of elements and with
// the (optional) segment size.

CS2_TBL_TEMP inline CS2_TBL_DECL::TableOf (const Allocator &a) :
  CS2_TAR_DECL (a),
  fHighestIndex (0),
  fLowestPossibleRemoved(0),
  fHighestPossibleRemoved(0),
  fFreeVector(a) { }

CS2_TBL_TEMP inline CS2_TBL_DECL::TableOf (uint32_t, const Allocator &a) :
  CS2_TAR_DECL (a),
  fHighestIndex (0),
  fLowestPossibleRemoved(0),
  fHighestPossibleRemoved(0),
  fFreeVector(a) { }

// TableOf::~TableOf
//
// Destroy a table and all of its elements.

CS2_TBL_TEMP inline CS2_TBL_DECL::~TableOf() {
  if (is_pod<AElementType>()) return;

  if (fHighestIndex==0) return;

  typename CS2_TBL_DECL::ConstCursor elementIndex(*this);
  // Destroy every existing element.
  for (elementIndex.SetToFirst();
       elementIndex.Valid();
       elementIndex.SetToNext()) {
    AElementType &currentElement = ElementAt(elementIndex);
    typename CS2_TAR_DECL::DerivedElement *derivedElement = (typename CS2_TAR_DECL::DerivedElement *) &currentElement;
    derivedElement->CS2_TAR_DECL::DerivedElement::~DerivedElement();
  }
}

// TableOf::TableOf (const TableOf &)
//
// Copy construct a table.

CS2_TBL_TEMP inline CS2_TBL_DECL::TableOf (const CS2_TBL_DECL &table) :
  CS2_TAR_DECL(table),
  fHighestIndex(table.fHighestIndex),
  fLowestPossibleRemoved(table.fLowestPossibleRemoved),
  fHighestPossibleRemoved(table.fHighestPossibleRemoved),
  fFreeVector(table.fFreeVector) {

  typename CS2_TBL_DECL::ConstCursor elementIndex(table);
  // Copy every existing element.
  for (elementIndex.SetToFirst();
       elementIndex.Valid();
       elementIndex.SetToNext()) {
    AElementType &currentElement = ElementAt(elementIndex);
    new (&currentElement) typename CS2_TAR_DECL::DerivedElement (table.ElementAt(elementIndex));
  }
}

// TableOf::operator=
//
// Assign a table to another.

CS2_TBL_TEMP inline CS2_TBL_DECL &CS2_TBL_DECL::operator= (const CS2_TBL_DECL &table) {
  MakeEmpty();

  fHighestIndex = table.fHighestIndex;
  fLowestPossibleRemoved = table.fLowestPossibleRemoved;
  fHighestPossibleRemoved = table.fHighestPossibleRemoved;
  fFreeVector = table.fFreeVector;

  typename CS2_TBL_DECL::ConstCursor elementIndex(table);
  // Copy every existing element.
  for (elementIndex.SetToFirst();
       elementIndex.Valid();
       elementIndex.SetToNext()) {
    AElementType &currentElement = ElementAt(elementIndex);
    new (&currentElement) typename CS2_TAR_DECL::DerivedElement (table.ElementAt(elementIndex));
  }
  return *this;
}

// TableOf::MakeEmpty
//
// Destroy all existing table elements.

CS2_TBL_TEMP inline void CS2_TBL_DECL::MakeEmpty () {
  ConstCursor elementIndex(*this);

  // Destroy every existing element.
  for (elementIndex.SetToFirst();
       elementIndex.Valid();
       elementIndex.SetToNext()) {
    AElementType &currentElement = ElementAt(elementIndex);
    typename CS2_TAR_DECL::DerivedElement *derivedElement = (typename CS2_TAR_DECL::DerivedElement *) &currentElement;
    derivedElement->CS2_TAR_DECL::DerivedElement::~DerivedElement();
  }

  fHighestIndex = 0;
  fLowestPossibleRemoved = 0;
  fHighestPossibleRemoved = 0;
  fFreeVector.Clear();
  CS2_TAR_DECL::ShrinkTo(0);
}

// TableOf::AddEntry
//
// Add an entry to the table at the next available index without constructing
// the object. ONLY USE THIS IF YOU ARE GOING TO EXPLICITLY CONSTRUCT THE
// THE DerivedElement YOURSELF.
CS2_TBL_TEMP inline TableIndex CS2_TBL_DECL::AddEntryNoConstruct () {
  TableIndex newIndex;
  bool foundone;

  newIndex = ClearLastOneIfThereIsOne(foundone);
  while (foundone) {
    if (newIndex <=fHighestIndex) goto found;
    newIndex = ClearLastOneIfThereIsOne(foundone);
  }

  fHighestIndex +=1;
  newIndex = fHighestIndex;
  CS2_TAR_DECL::GrowTo (fHighestIndex+1);

found:
  return newIndex;
}

// Add an entry to the table at the next available index
CS2_TBL_TEMP inline TableIndex CS2_TBL_DECL::AddEntry () {
  // Construct the new table entry
  TableIndex newIndex = AddEntryNoConstruct();
  new (&ElementAt(newIndex)) typename CS2_TAR_DECL::DerivedElement;
  return newIndex;
}

CS2_TBL_TEMP
  template <class Initializer>
inline TableIndex CS2_TBL_DECL::AddEntry (Initializer &initializer) {
  // Construct the new table entry
  TableIndex newIndex = AddEntryNoConstruct();
  new (&ElementAt(newIndex)) typename CS2_TAR_DECL::DerivedElement(initializer);
  return newIndex;
}

// TableOf::AddEntryAtPosition
//
// Add an entry to the table at the specified position without constructing
// the object. ONLY USE THIS IF YOU ARE GOING TO EXPLICITLY CONSTRUCT THE
// THE DerivedElement YOURSELF.
CS2_TBL_TEMP inline bool CS2_TBL_DECL::CheckEntryAtPosition (TableIndex newIndex) {
  bool shouldConstruct = true;
  if (fHighestIndex < newIndex) {
    if (fHighestIndex < newIndex-1) {
       if (fLowestPossibleRemoved == fHighestPossibleRemoved &&
           !fFreeVector.ValueAt(fLowestPossibleRemoved)) {
          fLowestPossibleRemoved = fHighestIndex + 1;
       }
       fHighestPossibleRemoved = newIndex - 1;
       while (fHighestIndex < newIndex-1) {
          fHighestIndex+=1;
          fFreeVector[fHighestIndex]=true;
       }
    }
    fHighestIndex = newIndex;
    CS2_TAR_DECL::GrowTo (newIndex+1);
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
CS2_TBL_TEMP inline TableIndex CS2_TBL_DECL::AddEntryAtPositionNoConstruct (TableIndex newIndex) {
  bool ignore = CheckEntryAtPosition(newIndex);
  return newIndex;
}

CS2_TBL_TEMP inline TableIndex CS2_TBL_DECL::AddEntryAtPosition (TableIndex newIndex) {
  if (CheckEntryAtPosition(newIndex)) {
     // Construct the new table entry
     new (&ElementAt(newIndex)) typename CS2_TAR_DECL::DerivedElement;
  }
  return newIndex;
}

CS2_TBL_TEMP template <class Initializer>
  inline TableIndex CS2_TBL_DECL::AddEntryAtPosition (TableIndex newIndex, Initializer initializer) {
  if (CheckEntryAtPosition(newIndex)) {
     // Construct the new table entry
     new (&ElementAt(newIndex)) typename CS2_TAR_DECL::DerivedElement(initializer);
  }
  return newIndex;
}

// TableOf::RemoveEntry
//
// Remove the given entry from the table.

CS2_TBL_TEMP inline void CS2_TBL_DECL::RemoveEntry (TableIndex index) {
  // The entry must already exist
  CS2Assert (Exists(index), ("Index " CS2_ZU " does not exist", index));

  if (index==0 || index > fHighestIndex) return;

  // Destroy this element
  typename CS2_TAR_DECL::DerivedElement *derivedElement = (typename CS2_TAR_DECL::DerivedElement *) & ElementAt(index);
  derivedElement->CS2_TAR_DECL::DerivedElement::~DerivedElement();

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

// TableOf::MemoryUsage
//
// Return the memory usage in bytes for this table.

CS2_TBL_TEMP inline unsigned long CS2_TBL_DECL::MemoryUsage() const {
  unsigned long sizeInBytes;

  sizeInBytes = CS2_TAR_DECL::MemoryUsage();
  sizeInBytes += sizeof(CS2_TBL_DECL) - sizeof(CS2_TAR_DECL);
  sizeInBytes += fFreeVector.MemoryUsage();

  return sizeInBytes;
}

/// Cursor class
////////////////////////////////////////////////////////////////////////

// TableOf::Cursor::Cursor
//
// Construct a table cursor.

  CS2_TBL_TEMP inline CS2_TBLCC_DECL::ConstCursor (const CS2_TBL_DECL &aTable) :
   fTable(aTable),
   fFreeCursor(aTable.fFreeVector),
   fNextFree(0),
   fIndex(0) {}

 CS2_TBL_TEMP inline CS2_TBLCC_DECL::ConstCursor (const CS2_TBL_DECL &aTable,
						  const SupportingBitVector<Allocator> &fv) :
   fTable(aTable),
   fFreeCursor(fv),
   fNextFree(0),
   fIndex(0) {}

  CS2_TBL_TEMP inline CS2_TBLC_DECL::Cursor (const CS2_TBL_DECL &aTable) :
    fFreeVector(aTable.fFreeVector),
    ConstCursor(aTable, fFreeVector) {}

// TableOf::Cursor::Cursor (const TableOf::Cursor &)
//
// Copy construct a table cursor.

CS2_TBL_TEMP inline CS2_TBLCC_DECL::ConstCursor (const typename CS2_TBLCC_DECL &inputCursor) :
  fTable(inputCursor.fTable),
  fFreeCursor(inputCursor.fFreeCursor),
  fIndex(inputCursor.fIndex),
  fNextFree(inputCursor.fNextFree) {
}

CS2_TBL_TEMP inline CS2_TBLCC_DECL::ConstCursor (const typename CS2_TBLCC_DECL &inputCursor,
						 const SupportingBitVector<Allocator> &fv) :
  fTable(inputCursor.fTable),
  fFreeCursor(fv),
  fIndex(inputCursor.fIndex),
  fNextFree(inputCursor.fNextFree) {
  fFreeCursor.SetToNextOneAfter(fIndex);
}

CS2_TBL_TEMP inline CS2_TBLC_DECL::Cursor (const typename CS2_TBLC_DECL &inputCursor) :
  fFreeVector(inputCursor.fFreeVector),
  ConstCursor(inputCursor, fFreeVector) {
}

// TableOf::Cursor::SetToFirst
//
// Set the cursor to the first allocated element in the table.

CS2_TBL_TEMP inline void CS2_TBLCC_DECL::SetToFirst() {
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

CS2_TBL_TEMP inline void CS2_TBLCC_DECL::SetToNext() {
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

CS2_TBL_TEMP inline bool CS2_TBLCC_DECL::Valid() const {
  return fIndex<fNextFree;
}

// TableOf::Cursor::operator TableIndex
//
// Return the index of the element pointed to by the cursor.

CS2_TBL_TEMP inline CS2_TBLCC_DECL::operator TableIndex() const {
  return fIndex;
}



#undef CS2_TBL_TEMP
#undef CS2_TBL_DECL
#undef CS2_TAR_DECL
#undef CS2_TBLC_DECL
#undef CS2_TBLCC_DECL
}
#endif // CS2_TABLEOF_H
