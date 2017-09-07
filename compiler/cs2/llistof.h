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
/*  File name:  llistof.h                                                  */
/*  Purpose:    Definition of the LinkedListOf template class.             */
/*                                                                         */
/***************************************************************************/

#ifndef CS2_LLISTOF_H
#define CS2_LLISTOF_H
#define IN_CS2_LLISTOF_H

#include "cs2/cs2.h"
#include "cs2/allocator.h"

#ifdef CS2_ALLOCINFO
#define allocate(x) allocate(x, __FILE__, __LINE__)
#define deallocate(x,y) deallocate(x, y, __FILE__, __LINE__)
#define reallocate(x,y,z) reallocate(x, y, z, __FILE__, __LINE__)
#endif

namespace CS2 {
#define CS2_LL_TEMP template <class ADataType, class Allocator>
#define CS2_LL_DECL LinkedListOf <ADataType, Allocator>

template <class ADataType, class Allocator>
class LinkedListOf : private Allocator {
  public:
  LinkedListOf (const Allocator &a = Allocator()) : Allocator(a), fFirst(NULL) {}
  ~LinkedListOf();
  LinkedListOf (const CS2_LL_DECL &);

  const Allocator& allocator() const { return *this;}
  Allocator& allocator() { return *this;}

  CS2_LL_DECL &operator= (const CS2_LL_DECL &);

  // Add an item to the list.  By default, items are added at the beginning
  // of the list.  Specify atEnd = true if the item is to be added at the end.
  // For adding items anywhere else in the list, use the cursor class.
  void Add (const ADataType &, bool atEnd = false);

  // Remove an item from the list.  By default, items are removed from the beginning
  // of the list.  Specify atEnd = true if the item is to be removed from  the end.
  // For removing items anywhere else in the list, use the cursor class.
  void Remove (bool atEnd = false);

  // Convenience methods for Add and Remove with atEnd = false
  void Push (const ADataType &);
  void Pop ();

  // Return first item of the list, or NULL if empty.
  ADataType *Head ();

  // Append another list at the end of this list
  void Append (const CS2_LL_DECL &);

  // Check if the list is empty.
  bool IsEmpty() const;

  // Make the list empty.
  void MakeEmpty();

  // Compute the number of elements (expensive)
  uint32_t NumberOfElements() const;

  // Return the number of bytes of memory used by the list
  unsigned long MemoryUsage() const;

  // Dump the linked list
  template <class str>
    friend str &operator<<  (str &out, const CS2_LL_DECL &ll) {
    LinkedListItem *p = ll.fFirst;
    out << "( ";
    while (p) {
      out << p->Data() << " ";
      p = p->Next();
    }
    out << ")";

    return out;
  }

  private:

  #define CS2_LL_ITEM CS2_LL_DECL::LinkedListItem

  class LinkedListItem {
    public:

    void *operator new (size_t, void *ptr) { return ptr;}

    LinkedListItem();
    // ~LinkedListItem();
    LinkedListItem (const ADataType &, typename CS2_LL_ITEM *);

    ADataType &Data();
    void       SetData (const ADataType &);

    typename CS2_LL_ITEM *Next() const;
    void     SetNext (typename CS2_LL_ITEM *);

    private:

    LinkedListItem (const LinkedListItem &);
    LinkedListItem &operator= (const LinkedListItem &);

    ADataType  fData;
    typename CS2_LL_ITEM   *fNext;
  };

  friend class LinkedListItem;

  public:

  #define CS2_LLC_DECL CS2_LL_DECL::Cursor

  class Cursor {
    public:

    Cursor (CS2_LL_DECL &);
    // ~Cursor();
    Cursor (const typename CS2_LLC_DECL &);

    // Set the cursor position - return flag indicating if position is valid
    bool SetToFirst();
    bool SetToNext();
    bool SetToLast();
    typename CS2_LL_ITEM *Next() const;

    // Determine if the cursor points at a valid position
    bool Valid() const;

    // Check if a cursor points to the last position (must be valid)
    bool IsLast() const;

    // Get/set the data at the current position in the list.
    ADataType &Data() const;
    ADataType &operator*();
    ADataType *operator->();
    void SetData (const ADataType &);

    // Add an item to the list after the current position.
    void AddAfter (const ADataType &);

    // Delete the item on the list after the current position.
    void DeleteAfter();

    // Check for equality
    int operator== (const typename CS2_LLC_DECL &) const;

    friend class CS2_LL_DECL;

    private:

    typename CS2_LLC_DECL &operator= (const typename CS2_LLC_DECL &);

    protected:

    CS2_LL_DECL& fList;
    typename CS2_LL_ITEM *fItem;
  };

  friend class Cursor;

  private:

  LinkedListItem *fFirst;
};

// LinkedListOf::MakeEmpty
//
// Make the list empty.

CS2_LL_TEMP inline
void CS2_LL_DECL::MakeEmpty() {
  typename CS2_LL_ITEM *p, *np;

  for (p = fFirst; p != NULL; p = np) {
    np = p->Next();
    p->~LinkedListItem();
    Allocator::deallocate(p, sizeof(LinkedListItem));
  }
  fFirst = NULL;
}

CS2_LL_TEMP inline
CS2_LL_DECL::~LinkedListOf() {
  MakeEmpty();
}

// LinkedListOf::Add
//
// Add an item to the list.  By default, items are added at the beginning
// of the list.  Specify atEnd = true if the item is to be added at the end.
// For adding items anywhere else in the list, use the cursor class.

CS2_LL_TEMP inline
void CS2_LL_DECL::Add (const ADataType &data, bool atEnd) {
  typename CS2_LL_ITEM *newp;

  if (atEnd && fFirst) {
    typename CS2_LL_ITEM *p = fFirst;
    while (p->Next()) p = p->Next();
    newp = (LinkedListItem *)Allocator::allocate(sizeof(LinkedListItem));
    newp = new (newp) typename CS2_LL_ITEM (data, NULL);
    p->SetNext (newp);
  } else {
    newp = (LinkedListItem *) Allocator::allocate(sizeof(LinkedListItem));
    newp = new (newp) typename CS2_LL_ITEM (data, fFirst);
    fFirst = newp;
  }
}

// LinkedListOf::Remove
//
// Remove an item from the list.  By default, items are removed from the beginning
// of the list.  Specify atEnd = true if the item is to be removed from the end.
// For removing items anywhere else in the list, use the cursor class.

CS2_LL_TEMP inline
void CS2_LL_DECL::Remove (bool atEnd) {
  typename CS2_LL_ITEM *p, *np;

  if (fFirst != NULL) {
    if (atEnd) {
      p = NULL; np = fFirst;
      while (np->Next()) {p = np; np = np->Next(); }
      // np is last, p is previous to last
      np->~LinkedListItem();
      Allocator::deallocate(np, sizeof(LinkedListItem));
      if (p) {
         p->SetNext(NULL);
      } else {
         fFirst = NULL;
      }
    } else {
      p = fFirst;
      np = p->Next();
      p->~LinkedListItem();
      Allocator::deallocate(p, sizeof(LinkedListItem));
      fFirst = np;
    }
  }
}

// LinkedListOf::Push
//
// Convenience method pushing an item onto the list. Identical to Add(data)

CS2_LL_TEMP inline
void CS2_LL_DECL::Push (const ADataType &data) {
  Add(data);
}

// LinkedListOf::Pop
//
// Convenience method popping an item from the top of the list.  Identical to Remove(data)

CS2_LL_TEMP inline
void CS2_LL_DECL::Pop () {
  Remove();
}

// LinkedListOf::Head
//
// return the first item of the list, or NULL if empty.

CS2_LL_TEMP inline
ADataType *CS2_LL_DECL::Head () {
  if (fFirst == NULL)
    return NULL;
  return &(fFirst->Data());
}

// LinkedListOf::IsEmpty
//
// Check if the list is empty.

CS2_LL_TEMP inline
bool CS2_LL_DECL::IsEmpty() const {
  return (fFirst == NULL);
}

// LinkedListOf::NumberOfElements
//
// Compute the number of elements (expensive)

CS2_LL_TEMP inline
uint32_t CS2_LL_DECL::NumberOfElements() const {
  uint32_t count = 0;
  typename CS2_LL_ITEM *p = fFirst;
  while (p) {
    count++;
    p = p->Next();
  }
  return count;
}

CS2_LL_TEMP inline
CS2_LL_ITEM::LinkedListItem() : fNext(NULL) { }

CS2_LL_TEMP inline
CS2_LL_ITEM::LinkedListItem (const ADataType &data, typename CS2_LL_ITEM *next) :
  fData(data) { fNext = (typename CS2_LL_ITEM *) next; }

CS2_LL_TEMP inline
ADataType &CS2_LL_ITEM::Data() {
  return fData;
}

CS2_LL_TEMP inline
void CS2_LL_ITEM::SetData (const ADataType &data) {
  fData = data;
}

CS2_LL_TEMP inline
typename CS2_LL_ITEM *CS2_LL_ITEM::Next() const {
  return fNext;
}

CS2_LL_TEMP inline
void CS2_LL_ITEM::SetNext (typename CS2_LL_ITEM *next) {
  fNext = (typename CS2_LL_ITEM *) next;
}

CS2_LL_TEMP inline
CS2_LLC_DECL::Cursor (CS2_LL_DECL &ll) : fList(ll), fItem(ll.fFirst) { }

CS2_LL_TEMP inline
CS2_LLC_DECL::Cursor (const typename CS2_LLC_DECL &c) :
  fList(c.fList), fItem(c.fItem) { }

// LinkedListOf::Cursor::operator==
//

CS2_LL_TEMP inline
int CS2_LLC_DECL::operator== (const typename CS2_LLC_DECL &c) const {
  return (&fList == &(c.fList)) && (fItem == c.fItem);
}

// LinkedListOf::Cursor::SetTo...
//
// Set the cursor position - return flag indicating if position is valid

CS2_LL_TEMP inline
bool CS2_LLC_DECL::SetToFirst() {
  fItem = fList.fFirst;
  return (fItem != NULL);
}

CS2_LL_TEMP inline
bool CS2_LLC_DECL::SetToNext() {
  fItem = fItem->Next();
  return (fItem != NULL);
}

CS2_LL_TEMP inline
bool CS2_LLC_DECL::SetToLast() {
  fItem = fList.fFirst;
  if (fItem == NULL) return false;
  while (fItem->Next()) fItem = fItem->Next();
  return true;
}

// LinkedListOf::Cursor::Valid
//
// Determine if the cursor points at a valid position

CS2_LL_TEMP inline
bool CS2_LLC_DECL::Valid() const {
  return (fItem != NULL);
}

// LinkedListOf::Cursor::IsLast
//
// Check if a cursor points to the last position (must be valid)

CS2_LL_TEMP inline
bool CS2_LLC_DECL::IsLast() const {
  CS2Assert (fItem != NULL, ("Expecting valid cursor"));
  return (fItem->Next() == NULL);
}

// LinkedListOf::Cursor::Data
//
// Get/set the data at the current position in the list.

CS2_LL_TEMP inline
ADataType &CS2_LLC_DECL::Data() const {
  return fItem->Data();
}

CS2_LL_TEMP inline
ADataType &CS2_LLC_DECL::operator*() {
  return fItem->Data();
}

CS2_LL_TEMP inline
ADataType *CS2_LLC_DECL::operator->() {
  return &(fItem->Data());
}

CS2_LL_TEMP inline
void CS2_LLC_DECL::SetData (const ADataType &data) {
  fItem->SetData (data);
}

// LinkedListOf::Cursor::AddAfter
//
// Add an item to the list after the current position.

CS2_LL_TEMP inline
void CS2_LLC_DECL::AddAfter (const ADataType &data) {
  typename CS2_LL_ITEM *newp;

  CS2Assert (fItem != NULL, ("Expecting valid cursor"));
  newp = (LinkedListItem *)fList.allocator().allocate(sizeof(LinkedListItem));
  newp = new (newp) typename CS2_LL_ITEM (data, fItem->Next());

  fItem->SetNext (newp);
}

// LinkedListOf::Cursor::DeleteAfter
//
// Delete the item on the list after the current position.
CS2_LL_TEMP inline
void CS2_LLC_DECL::DeleteAfter() {
  CS2Assert (fItem != NULL, ("Expecting valid cursor"));

  typename CS2_LL_ITEM *deletep = fItem->Next();

  if (deletep) {
    fItem->SetNext(deletep->Next());
    deletep->~LinkedListItem();
    fList.allocator().deallocate(deletep, sizeof(LinkedListItem));
  }
}

CS2_LL_TEMP inline
typename CS2_LL_ITEM *CS2_LLC_DECL::Next () const {
  CS2Assert (fItem != NULL, ("Expecting valid cursor"));
  return fItem->Next();
}

CS2_LL_TEMP inline
  CS2_LL_DECL::LinkedListOf (const CS2_LL_DECL &ll) : Allocator(ll.allocator()), fFirst(NULL) {
  *this = ll;
}

CS2_LL_TEMP inline
CS2_LL_DECL &CS2_LL_DECL::operator= (const CS2_LL_DECL &ll) {
  MakeEmpty();

  if (ll.fFirst) {
      typename CS2_LL_ITEM *p, *thisp, *nextp;

      thisp = (LinkedListItem *) Allocator::allocate(sizeof(LinkedListItem));
      thisp = fFirst = new (thisp) typename CS2_LL_ITEM (ll.fFirst->Data(), NULL);
      p = ll.fFirst->Next();
      while (p) {
        nextp = (LinkedListItem *)Allocator::allocate(sizeof(LinkedListItem));
	nextp = new (nextp) typename CS2_LL_ITEM (p->Data(), NULL);
	thisp->SetNext (nextp);
	thisp = nextp;
	p = p->Next();
      }
  } else {
    fFirst = NULL;
  }

  return *this;
}

// LinkedListOf::Append
//
// Append the given list at the end of this list

CS2_LL_TEMP inline
void CS2_LL_DECL::Append (const CS2_LL_DECL &inList) {
  if (inList.IsEmpty()) return;

  typename CS2_LLC_DECL thisc(*this), inc((CS2_LL_DECL &)inList);
  inc.SetToFirst();
  if (IsEmpty()) {
    Add (*inc);
    inc.SetToNext();
  }
  thisc.SetToLast();
  while (inc.Valid()) {
    thisc.AddAfter (*inc);
    thisc.SetToNext();
    inc.SetToNext();
  }
}

// LinkedListOf::MemoryUsage
//
// Return the number of bytes of memory used by the list

CS2_LL_TEMP inline
unsigned long CS2_LL_DECL::MemoryUsage() const {
  typename CS2_LL_ITEM *p = fFirst;
  unsigned long mem = sizeof (CS2_LL_DECL);

  while (p) {
    mem += sizeof (typename CS2_LL_ITEM);
    p = p->Next();
  }

  return mem;
}

template <class ADataType, class Allocator>
class QueueOf : public LinkedListOf<ADataType, Allocator>
   {
   public:
   QueueOf (const Allocator &a = Allocator()) : LinkedListOf<ADataType, Allocator>(a) {}
   using LinkedListOf<ADataType, Allocator>::Add;
   using LinkedListOf<ADataType, Allocator>::Remove;
   using LinkedListOf<ADataType, Allocator>::Head;
   void Push(const ADataType &data) { Add(data, true); }
   ADataType Pop() { ADataType head = *(Head()); Remove(); return head; }
   };
template <class ADataType, class Allocator>
class StackOf : public QueueOf<ADataType, Allocator>
   {
   public:
   StackOf (const Allocator &a = Allocator()) : QueueOf<ADataType, Allocator>(a) {}
   using LinkedListOf<ADataType, Allocator>::Add;
   void Push(const ADataType &data) { Add(data); }
   };
}

#undef CS2_LLC_DECL
#undef CS2_LL_TEMPARGS
#undef CS2_LL_TEMP
#undef CS2_LL_DECL

#ifdef CS2_ALLOCINFO
#undef allocate
#undef deallocate
#undef reallocate
#endif

#endif // CS2_LLISTOF_H
