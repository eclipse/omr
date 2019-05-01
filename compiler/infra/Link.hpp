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

#ifndef LINK_INCL
#define LINK_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"

template <class T> class TR_Link0
   {
   public:
   TR_Link0(T *n = 0) : _next(n) {}
   T *getNext() { return _next; }
   void setNext(T* n) { _next = n; }
   private:
   T *_next;
   };

template<class T> class TR_LinkHead0
   {
   public:
   TR_LinkHead0() : _head(0) {}
   T *getFirst() { return _head; }
   void setFirst(T *t) { _head = t; }

   bool isEmpty() { return _head == 0; }
   bool isSingleton() {return _head && !_head->getNext();}
   bool isDoubleton() {return _head && _head->getNext() && !_head->getNext()->getNext();}
   bool isMultipleEntry() {return _head && _head->getNext();}

   void add(T * e) { e->setNext(_head); _head = e; }

   T * pop() { T * e = _head; if (e) _head = e->getNext(); return e; }

   bool find(T * f)
      {
      for (T * e = _head; e; e = e->getNext())
         if (e == f) return true;
      return false;
      }

   void insertAfter(T * prev, T * e)
      {
      if (!prev) add(e);
      else { e->setNext(prev->getNext()); prev->setNext(e); }
      }

   void removeAfter(T * prev, T * e)
      {
      if (prev) prev->setNext(e->getNext());
      else setFirst(e->getNext());
      }

   bool remove(T * e)
      {
      for (T * c = _head, * p = 0; c; p = c, c = c->getNext())
         if (c == e)
            {
            if (p) p->setNext(e->getNext());
            else   _head = e->getNext();
            e->setNext(0);
            return true;
            }
      return false;
      }

   int32_t getSize()
      {
      int32_t size = 0;
      for (T * c = _head; c; c = c->getNext())
         ++size;
      return size;
      }

private:
   T * _head;
   };

/**
 * This class has one client, the CFG.
 * I think the intention here is to support invalidating links without breaking
 * pointers, but, it's not clear at all.
 */
template <class T> class TR_Link1
   {
   friend class TR_DebugExt;

   public:
   TR_Link1(T *n = 0) : _next(n), _valid(false) {  }
   T *getNext()  {
                  while(_next && !(_next->isValid()))
                        _next = _next->_next;
                  return _next;
                 }
   void setNext(T* n)    { setValid(true); _next = n; }
   bool isValid()        { return _valid; }
   void setValid(bool b) { _valid = b; }
   private:
   T *_next;
   bool _valid;
   };

template<class T> class TR_LinkHead1
   {
   public:
   TR_LinkHead1() : _head(0) {}
   T *getFirst() { return _head; }
   void setFirst(T *t) { _head = t; t->setValid(true); }

   bool isEmpty() { return _head == 0; }
   bool isSingleton() {return _head && !_head->getNext();}
   bool isDoubleton() {return _head && _head->getNext() && !_head->getNext()->getNext();}
   bool isMultipleEntry() {return _head && _head->getNext();}

   void add(T * e) { e->setNext(_head); _head = e; }

   T * pop() { T * e = _head; if (e && e->isValid()) _head = e->getNext(); else return NULL; return e; }

   bool find(T * f)
      {
      return f->isValid();
      }

   void insertAfter(T * prev, T * e)
      {
      if (prev && prev->isValid()) { e->setNext(prev->getNext()); prev->setNext(e); }
      else add(e);
      }

   void removeAfter(T * prev, T * e)
      {
      if (prev && prev->isValid()) prev->setNext(e->getNext());
      else setFirst(e->getNext());
      }

   bool remove(T * e)
      {
      bool remove = (e->isValid() ||  e == _head);
      if (e == _head) _head = e->getNext();
      if (remove) e->setValid(false);
      return remove;
      }

   int32_t getSize()
      {
      int32_t size = 0;
      for (T * c = _head; c; c = c->getNext())
         ++size;
      return size;
      }

private:
   T * _head;
   };
template <class T> class TR_Link : public TR_Link0<T>
   {
   public:
   TR_ALLOC(TR_Memory::LLLink);
   TR_Link(T *n = 0) : TR_Link0<T>(n) {}
   };

template<class T> class TR_LinkHead : public TR_LinkHead0<T>
   {
   public:
   TR_ALLOC(TR_Memory::LLLinkHead);
   TR_LinkHead() : TR_LinkHead0<T>() {}
   };

template<class T> class TR_LinkHeadAndTail
   {
public:
   TR_LinkHeadAndTail() : _head(0), _tail(0) {}

   T * getFirst() { return _head; }
   T * getLast() { return _tail; }

   void set(T * h, T * t) { _head = h; _tail = t; }

   bool isEmpty() { return _head == 0; }
   bool isSingleton() {return _head && !_head->getNext();}
   bool isDoubleton() {return _head && _head->getNext() && !_head->getNext()->getNext();}
   bool isMultipleEntry() {return _head && _head->getNext();}

   int32_t getSize()
         {
         int32_t size = 0;
         for (T *c = _head; c; c = c->getNext())
            ++size;
         return size;
         }

   T * pop()
      {
      T * e = _head;
      if (!e) return 0;
      _head = e->getNext();
      if (!_head) _tail = 0;
      return e;
      }

   void prepend(T * e)
      {
      e->setNext(_head);
      _head = e;
      if (!_tail) _tail = e;
      }
   void append(T * e)
      {
      if (_tail) _tail->setNext(e);
      else _head = e;
      _tail = e;
      }

   void add(T * e) { prepend(e); }

private:
   T * _head;
   T * _tail;
   };

template <class T> class TR_TwoLinkListIterator
   {
public:
   TR_TwoLinkListIterator() { }
   TR_TwoLinkListIterator(T * l1, T * l2) : _firstListHead(l1), _secondListHead(l2) { }

   T * getFirst()
      {
      if (_firstListHead) { _cursor = _firstListHead; _iteratingThruSecondList = false; }
      else { _cursor = _secondListHead; _iteratingThruSecondList = true; }
      return _cursor;
      }

   T * getNext()
      {
      if (_cursor)
         {
         _cursor = _cursor->getNext();
         if (_cursor)
            return _cursor;
         }
      if (!_iteratingThruSecondList)
         {
         _iteratingThruSecondList = true;
         _cursor = _secondListHead;
         }
      return _cursor;
      }

protected:
   T *  _firstListHead;
   T *  _secondListHead;
   T *  _cursor;
   bool _iteratingThruSecondList;
   };

template <class Key, class Value> class TR_Pair
   {
   public:
   TR_ALLOC(TR_Memory::Pair); // FIXME
   TR_Pair(Key *key, Value *value) : _key(key), _value(value) {}
   Key    *getKey()   { return _key;   }
   Value  *getValue() { return _value; }

   void    setKey(Key* key) { _key = key; }
   void    setValue(Value* value) { _value = value; }
   private:
   Key    *_key;
   Value *_value;
   };

// Template specialization to deal with (ptr,val) pairs that the top class can't deal with unless you do ugly casting
template <class Key, class Value> class TR_Pair<Key *,Value>
   {
   public:
   TR_ALLOC(TR_Memory::Pair); // FIXME
   TR_Pair(Key *key, Value value) : _key(key), _value(value) {}
   Key    *getKey()   { return _key;   }
   Value   getValue() { return _value; }

   void    setKey(Key* key) { _key = key; }
   void    setValue(Value value) { _value = value; }
   private:
   Key    *_key;
   Value _value;
   };

#endif
