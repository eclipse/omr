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

#ifndef LIST_INCL
#define LIST_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

template <class T> class List;
template <class T> class ListAppender;
template <class T> class ListElement
   {
   ListElement<T> *_pNext;
   T              *_pDatum;

   public:

   TR_ALLOC(TR_Memory::LLListElement)

   void * operator new(size_t sz, List<T>& freeList)
      {
      void *rc;
      if (freeList._pHead != NULL)
         {
         ListElement<T> *val = freeList._pHead;
         freeList._pHead = val->_pNext;
         val->_pNext = NULL;
         rc = val;
         }
      else
         {
         rc = operator new(sz, freeList.getRegion());
         }
      return rc;
      }

   void operator delete(void *ptr, List<T>& freeList)
      {
	  // the freeList was modified during the new operator invocation
	  operator delete(ptr, freeList.getRegion());
      }

   ListElement(T *p, ListElement<T> *q = NULL) : _pNext(q), _pDatum(p) {}

   T *getData()     {return _pDatum;}
   T *setData(T *p) {return (_pDatum = p);}

   ListElement<T> *getNextElement()                  {return _pNext;}
   ListElement<T> *setNextElement(ListElement<T> *p) {return (_pNext = p);}
   };

/**
 * Non-modifiable list
 */
template <class T> class ListBase
   {
   protected:
   ListElement<T> *_pHead;
   TR::Region     *_region;
   ListBase() {}  // can be created only as a List
   friend void * ListElement<T>::operator new(size_t, List<T>&);
   friend ListElement<T> *ListAppender<T>::add(T *p, List<T>& freeList);
   friend ListElement<T> *ListAppender<T>::add(T *p);

   public:
   TR::Region       &getRegion() { return *_region; }
   void              setRegion(TR::Region &region) { _region = &region; }

   TR_ALLOC(TR_Memory::LLList)

   ListElement<T> *getListHead()                  {return _pHead;}

   bool isEmpty() const {return _pHead == NULL;}
   bool isSingleton() const {return _pHead && !_pHead->getNextElement();}
   bool isDoubleton() const {return _pHead && _pHead->getNextElement() && !_pHead->getNextElement()->getNextElement();}
   bool isMultipleEntry() const {return _pHead && _pHead->getNextElement();}

   bool find(T * elem) const
      {
      for (ListElement<T> * le = _pHead; le; le = le->getNextElement())
         if (elem == le->getData())
            return true;
      return false;
      }

   int32_t getSize() const
      {
      int32_t size = 0;
      for (ListElement<T> *p = _pHead; p != NULL; p = p->getNextElement())
         size++;
      return size;
      }

   ListElement<T> *getLastElement() const
      {
      if (!_pHead)
         return NULL;
      ListElement<T> *p;
      for (p = _pHead; p->getNextElement(); p = p->getNextElement())
         {}
      return p;
      }

    bool operator == (const ListBase<T> &list)
        {
        ListElement<T> *p1 = _pHead;
        ListElement<T> *p2 = list._pHead;

        for (; p1 && p2; p1 = p1->getNextElement(), p2 = p2->getNextElement())
           {
           if (*p1->getData() != *p2->getData())
             return false;
           }
        if (p1 || p2)
           return false;
        else
           return true;
       }

   };


/**
 * Modifiable list
 */
template <class T> class List : public ListBase <T>
   {
   protected:
   List() { this->_pHead = NULL; this->_region = NULL; }
   public:

   TR_ALLOC(TR_Memory::LLList)

   List(TR_Memory *m) { this->_pHead = NULL; this->_region = &(m->heapMemoryRegion()); }
   List(TR::Region &region)  {this->_pHead = NULL; this->_region = &region; }

   /**
    * Re-initialize the list
    */
   void init() {this->_pHead = NULL;}

   ListElement<T> *setListHead(ListElement<T> *p) {return (this->_pHead = p);}

   // probably don't need this unless going to also create smart iterator pointer class
   // to ensure destruction of iterator created by this entry.
   //
   // ListIterator<class T> *createIterator() {return new ListIterator<class T>(this);};

   ListElement<T> *add(T *p)
      {
      TR_ASSERT(this->_region != NULL,
             "Should not be using heap allocation to add to a persistent list. The 'add' routine in TR_PersistentList should be called\n");
      return this->_pHead = new (this->getRegion()) ListElement<T>(p, this->_pHead);
      }

   ListElement<T> *add(T *p, List<T>& freeList) { return this->_phead = new (freeList) ListElement<T>(p, this->_pHead); }

   void deleteAll()
      {
      this->_pHead = NULL;
      }

   T *remove(T * elem)
      {
      if (this->_pHead == NULL)
         return NULL;
      if (elem == this->_pHead->getData())
         {
         this->_pHead = this->_pHead->getNextElement();
         return elem;
         }
      ListElement<T> *cur = this->_pHead;
      ListElement<T> *next = this->_pHead->getNextElement();
      while (next)
         {
         if (elem == next->getData())
            {
            cur->setNextElement(next->getNextElement());
            return elem;
            }
         cur = next;
         next = next->getNextElement();
         }
      return NULL;
      }

   void removeNext(ListElement<T> *previous)
      {
      if (previous == NULL)
         {
         popHead();
         }
      else
         {
         TR_ASSERT(previous->getNextElement(), "List::removeNext(previous) on last element in list");
         previous->setNextElement(previous->getNextElement()->getNextElement());
         }
      }

   ListElement<T> *addAfter(T * elem, ListElement<T> *previous)
      {
      ListElement<T> *p;
      if (previous == NULL)
         p = add(elem);
      else
         {
         TR_ASSERT(this->_region != NULL, "Should not be using heap allocation to add to a persistent list. The 'addAfter' routine in TR_PersistentList should be called\n");
         p = new (this->getRegion()) ListElement<T>(elem, previous->getNextElement());
         previous->setNextElement(p);
         }
      return p;
      }

   T *getHeadData(){ return this->_pHead? this->_pHead->getData() : NULL; }

   T *popHead()
      {
      if (!this->_pHead)
         return 0;

      T *datum = this->_pHead->getData();
      this->_pHead = this->_pHead->getNextElement();
      return datum;
      }


   /**
    * Merges the other list into this list
    */
   void add(List<T> &other)
      {
      for (ListElement<T> *cursor = other._pHead;
           cursor;
           cursor = cursor->getNextElement())
         add(cursor->getData());
      }

   /**
    * Free elements from this list putting them on the free list
    */
   void freeElements(List<T>& freeList)
      {
      if (this->_pHead != NULL)
         {
         ListElement<T>* cursor = this->_pHead;
         while (cursor->getNextElement() != NULL)
            {
            cursor = cursor->getNextElement();
            }
         cursor->setNextElement(freeList._pHead);
         freeList._pHead = this->_pHead;
         this->_pHead = NULL;
         }
      }
   };


template <class T> class ListIterator
   {
   ListElement<T> *_pHead;
   ListElement<T> *_pCursor;

   public:

   TR_ALLOC(TR_Memory::LLListIterator)

   ListIterator() : _pHead(0), _pCursor(0) { }
   ListIterator(ListBase<T> *p) : _pHead(p->getListHead()),
                              _pCursor(p->getListHead()) {}
   ListIterator(const ListIterator&other)
      {
      _pHead = other._pHead;
      _pCursor = other._pCursor;
      }

   void set(ListBase<T> * p) { _pHead = _pCursor = p->getListHead(); }
   void set(ListElement<T> * p)       { _pHead = _pCursor = p; }

   T *getFirst() {_pCursor = _pHead; return (_pHead ? _pHead->getData() : NULL);}

   T *getCurrent() {return (_pCursor ? _pCursor->getData() : NULL);}
   ListElement<T> *getCurrentElement() {return _pCursor;}

   T *getNext()
      {
      if (_pCursor)
         {
         _pCursor = _pCursor->getNextElement();
         return (_pCursor ? _pCursor->getData() : NULL);
         }
      else
         {
         return NULL;
         }
      }

   void reset() {_pCursor = _pHead;}

   bool atEnd() {return _pCursor == NULL;}

   bool atLastElement()
      {
      TR_ASSERT(!atEnd(),"atLastElement() not valid when already at the end of the iterator\n");
      return (_pCursor->getNextElement() == NULL);
      }

   bool empty() {return _pHead == NULL;}

   };

template <class T> class ListAppender
   {
   List<T> *_pList;
   ListElement<T> *_pCursor;

   public:

   TR_ALLOC(TR_Memory::LLListAppender)

   ListAppender(List<T> *p)
      : _pList(p)
      {
      // Get _pCursor to point at the last element of the list
      //
      _pCursor = p->getListHead();
      if (_pCursor != NULL)
         {
         ListElement<T> *pNext;
         for (pNext = _pCursor->getNextElement(); pNext != NULL; _pCursor = pNext, pNext = pNext->getNextElement())
            ;
         }
      }

   void empty()
      {
      _pList->init();
      _pCursor = NULL;
      }

   bool find(T * elem)
      {
      return _pList->find(elem);
      }

   ListElement<T> *addListElement(ListElement<T> *elem)
      {
      if (_pCursor)
         _pCursor->setNextElement(elem);
      else
         _pList->setListHead(elem);
      _pCursor = elem;
      return elem;
      }

   ListElement<T> *add(T *p)
      {
      TR_ASSERT(_pList->_region != NULL, "Should not be using heap allocation to append to a persistent list. Should use 'addPersistent' instead\n");
      return addListElement(new (_pList->getRegion()) ListElement<T>(p,NULL));
      }

   ListElement<T> *add(T *p, List<T>& freeList)
      {
      TR_ASSERT(_pList->_region != NULL, "Should not be using heap allocation to append to a persistent list. Should use 'addPersistent' instead\n");
      return addListElement(new (freeList) ListElement<T>(p,NULL));
      }

   ListElement<T> *addPersistent(T *p)
      {
      TR_ASSERT(_pList->_region == NULL, "Should not be using persistent allocation to append to a non-persistent list.\n");
      return addListElement(new (PERSISTENT_NEW) ListElement<T>(p,NULL));
      }

   T *remove(T * elem)
      {
      ListElement<T> *p;
      p = _pList->getListHead();
      if (p == NULL)
         return NULL;
      if (elem == p->getData())
         {
         _pList->setListHead(p->getNextElement());
         if (_pCursor == p)
            _pCursor = NULL;
         return elem;
         }
      ListElement<T> *next = p->getNextElement();
      while (next)
         {
         if (elem == next->getData())
            {
            p->setNextElement(next->getNextElement());
            if (_pCursor == next)
               _pCursor = p;
            return elem;
            }
         p = next;
         next = next->getNextElement();
         }
      return NULL;
      }
   };

template <class T> class TR_TwoListIterator
   {
public:
   TR_ALLOC(TR_Memory::TwoListIterator)

   TR_TwoListIterator(List<T> & l1, List<T> & l2)
      : _firstListHead(l1.getListHead()), _secondListHead(l2.getListHead())
      { }

   T * getFirst()
      {
      if (_firstListHead) { _cursor = _firstListHead; _iteratingThruSecondList = false; }
      else { _cursor = _secondListHead; _iteratingThruSecondList = true; }
      return _cursor ? _cursor->getData() : 0;
      }

   T * getCurrent()
      {
      if (_cursor)
         return _cursor->getData();
      else
         return NULL;
      }

   T * getNext()
      {
      if (_cursor)
         {
         _cursor = _cursor->getNextElement();
         if (_cursor) return _cursor->getData();
         }
      if (!_iteratingThruSecondList)
         {
         _iteratingThruSecondList = true;
         _cursor = _secondListHead;
         if (_cursor) return _cursor->getData();
         }
      return 0;
      }

   bool inFirstList() {return !_iteratingThruSecondList;}
   bool inSecondList() {return _iteratingThruSecondList;}

protected:
   ListElement<T> * _firstListHead;
   ListElement<T> * _secondListHead;
   ListElement<T> * _cursor;
   bool             _iteratingThruSecondList;
   };

template <class T> class TR_ScratchList : public List<T>
   {
   public:
   TR_ScratchList(TR_Memory * m)
      : List<T>(m) { List<T>::_region = &(m->currentStackRegion()); }

   TR_ScratchList(T *p, TR_Memory * m)
      : List<T>(m)
      {
      List<T>::_region = &(m->currentStackRegion());
      List<T>::_pHead = new (List<T>::getRegion()) ListElement<T>(p);
      }

   ListElement<T> *add(T *p)
      {
      return List<T>::_pHead = new (List<T>::getRegion()) ListElement<T>(p, List<T>::_pHead);
      }

   ListElement<T> *addAfter(T * elem, ListElement<T> *previous)
      {
      ListElement<T> *p;
      if (previous == NULL)
         p = add(elem);
      else
         {
         p = new (List<T>::getRegion()) ListElement<T>(elem, previous->getNextElement());
         previous->setNextElement(p);
         }
      return p;
      }
   };


template <class T> class TR_PersistentList : public List<T>
   {
   public:
   TR_PersistentList() : List<T>() { List<T>::_pHead = 0; }
   TR_PersistentList(T *p) : List<T>() { List<T>::_pHead = new (PERSISTENT_NEW) ListElement<T>(p); }
   ListElement<T> *add(T *p) { return List<T>::_pHead = new (PERSISTENT_NEW) ListElement<T>(p, List<T>::_pHead); }

   ListElement<T> *addAfter(T * elem, ListElement<T> *previous)
      {
      ListElement<T> *p;
      if (previous == NULL)
         p = add(elem);
      else
         {
         p = new (PERSISTENT_NEW) ListElement<T>(elem, previous->getNextElement());
         previous->setNextElement(p);
         }
      return p;
      }

   void deleteAll()
      {
      ListElement<T> *p = this->_pHead;
      while(p)
         {
         ListElement<T> *pNext = p->getNextElement();
         TR_Memory::jitPersistentFree(p);
         p = pNext;
         }
      this->_pHead = NULL;
      }
   };


/**
 * A generic FIFO Queue implementation
 */
template <class T> class TR_Queue : public TR_ScratchList<T>
   {
   public:
   TR_Queue(TR_Memory * m,
            bool useFreeList = false) :
      TR_ScratchList<T>(m), _pTail(NULL), _useFreeList(useFreeList), _freeList(m) {}

   void enqueue(T *elem)
      {
      if (_pTail == NULL)
         {
         List<T>::_pHead = _pTail =
            _useFreeList ?
            new (_freeList) ListElement<T>(elem, List<T>::_pHead) :
            new (List<T>::getRegion()) ListElement<T>(elem, List<T>::_pHead);
         }
      else
         {
         ListElement<T> *p =
            _useFreeList ?
            new (_freeList) ListElement<T>(elem, NULL) :
            new (List<T>::getRegion()) ListElement<T>(elem, NULL);
         _pTail->setNextElement(p);
         _pTail = p;
         }
      }

   T *dequeue()
      {
      if (!List<T>::_pHead)
         return NULL;
      ListElement<T> *toBeFreed = List<T>::_pHead;
      T *datum = List<T>::_pHead->getData();
      List<T>::_pHead = List<T>::_pHead->getNextElement();
      if (List<T>::_pHead == NULL)
         _pTail = NULL;
      if (_useFreeList)
         {
         toBeFreed->setNextElement(_freeList.getListHead());
         _freeList.setListHead(toBeFreed);
         }
      return datum;
      }

   T *peek()
       {
      if (!List<T>::_pHead)
         return NULL;
      T *datum = List<T>::_pHead->getData();
         return datum;
       }

   bool isEmpty()
      {
      return !List<T>::_pHead;
      }

   void releaseFreeList()
      {
      // if you use a queue with freelist in a context where the queue object itself survives
      // beyond the stack scope of allocation of items and thus need to reset the free
      // list for the current stack allocation scope
      TR_ASSERT(_useFreeList, "_useFreeList must be non-null");
      TR_ASSERT(isEmpty(), "must be empty");
      _freeList.init();
      }
   private:
   ListElement<T> *_pTail;
   bool _useFreeList;
   TR_ScratchList<T> _freeList;
   };

 template <class T> class ListHeadAndTail : public List<T>
   {
   protected:
   ListElement<T> *_pTail;

   public:

   TR_ALLOC(TR_Memory::LLList)

   ListHeadAndTail(TR_Memory * m) : List<T>(m->heapMemoryRegion()) { init(); }
   ListHeadAndTail(TR_Memory * m, T *p) : List<T>(m->heapMemoryRegion()) { init(); add(p); }
   ListHeadAndTail(TR::Region &region) : List<T>(region) { init(); }
   ListHeadAndTail(TR::Region &region, T *p) : List<T>(region) { init(); add(p); }

   /** Re-initialize the list */
   void init() {this->_pHead = _pTail = 0;}

   ListElement<T> *getListTail()                  {return _pTail;}
   ListElement<T> *setListTail(ListElement<T> *p) {return (_pTail = p);}
   T *getHeadData() { return this->_pHead->getData(); }

   ListElement<T> *add(T *p)
      {
      this->_pHead = new (this->getRegion()) ListElement<T>(p, this->_pHead);
      if (!_pTail) _pTail = this->_pHead;
      return this->_pHead;
      }
   ListElement<T> *append(T * p)
      {
      ListElement<T> *ret = new (this->getRegion()) ListElement<T>(p, 0);
      if (_pTail) _pTail->setNextElement(ret);
      _pTail = ret;
      if (!this->_pHead) this->_pHead = ret;
      return ret;
      }

   void deleteAll()     { init(); }

   ListElement<T> *getLastElement()      { return getListTail(); }

   T *remove(T *elem) { TR_UNIMPLEMENTED(); }
   };


#endif
