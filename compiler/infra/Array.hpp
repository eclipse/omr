/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef ARRAY_INCL
#define ARRAY_INCL

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

// TR_Array implements a dynamic array of objects
//
// The objects in the array must be assignable using memcpy
// (eg T must not be a class type with virtual functions)
//
// An object of type TR_Array<T> can be copy constructed and
// assigned.
//
template<class T> class TR_Array
   {
public:
   TR_ALLOC(TR_Memory::Array);


   TR_Array() : _nextIndex(0), _internalSize(0), _allocationKind(heapAlloc), _zeroInit(false), _trMemory(0), _trPersistentMemory(0), _array (NULL) { }

   TR_Array(TR_Memory * m, uint32_t initialSize = 8, bool zeroInit = true, TR_AllocationKind allocKind = heapAlloc)
     {
     init(m, m->trPersistentMemory(), initialSize, zeroInit, allocKind);
     }

   TR_Array(TR_Memory * m, TR_AllocationKind allocKind, uint32_t initialSize = 8, bool zeroInit = true)
     {
     init(m, m->trPersistentMemory(), initialSize, zeroInit, allocKind);
     }

   TR_Array(TR_PersistentMemory * pm, uint32_t initialSize = 8, bool zeroInit = true)
     {
     init(0, pm, initialSize, zeroInit, persistentAlloc);
     }

   void init(TR_Memory * m, uint32_t initialSize = 8, bool zeroInit = true, TR_AllocationKind allocKind = heapAlloc)
         {
         init(m, m->trPersistentMemory(), initialSize, zeroInit, allocKind);
         }

   void init(TR_Memory * m, TR_PersistentMemory * pm, uint32_t initialSize = 8, bool zeroInit = true, TR_AllocationKind allocKind = heapAlloc)
      {
      _nextIndex = 0;
      _internalSize = initialSize;
      _allocationKind = allocKind;
      _zeroInit = zeroInit;
      _trMemory = m;
      _trPersistentMemory = pm;
      if (m)
         _array = (T*)m->allocateMemory(initialSize * sizeof(T), _allocationKind, TR_MemoryBase::Array);
      else if (pm)
         _array = (T*)pm->allocatePersistentMemory(initialSize * sizeof(T));
      else
         TR_ASSERT(false, "Attempting to allocate an array without a memory object");
      if (zeroInit)
         memset(_array, 0, initialSize * sizeof(T));
      }

   TR_Array(const TR_Array<T>& other) { copy(other); }

   TR_Array<T>& operator=(const TR_Array<T>& other) { copy(other); return *this; }

   uint32_t size() const   { return _nextIndex; }
   uint32_t internalSize() { return _internalSize; }

   TR_Memory * trMemory() { return _trMemory; }
   TR_AllocationKind allocationKind() { return _allocationKind; }

   void setSize(uint32_t s)
      {
      if (s > _internalSize)
         growTo(s + _internalSize);
      else
         if (_nextIndex > s && _zeroInit) memset(_array + s, 0, (_nextIndex - s) * sizeof(T));
      _nextIndex = s;
      }

   int32_t lastIndex() { return size() - 1; }

   /**
    * @brief Copies the provided object into the first unused element at the end of the array and returns the index to the newly created element.  This may require growing the backing array.
    * @param t The object to be copied into the array.
    * @return The index of the newly created element.
    */
   uint32_t add(T t)
      {
      if (_nextIndex == _internalSize) { TR_ASSERT(_internalSize < UINT_MAX/2, "Array trying to grow bigger than UINT_MAX\n"); growTo(_internalSize*2); }
      _array[_nextIndex] = t;
      return _nextIndex++;
      }

   void insert(T t, uint32_t index)
      {
      if (_nextIndex <= index)
         add(t);
      else
         {
         if (_nextIndex == _internalSize) growTo(_internalSize*2);
         for (uint32_t i = _nextIndex; i > index; --i)
            _array[i] = _array[i-1];
         _array[index] = t;
         _nextIndex++;
         }
      }

   void append(const TR_Array<T>& other)
      {
      for (uint32_t i = 0; i < other._nextIndex; i++)
         add(other[i]);
      }


   void remove(uint32_t index)
      {
      TR_ASSERT(index < _nextIndex, "TR_Array::remove, index >= _nextIndex");
      for (uint32_t i = index+1; i < _nextIndex; ++i)
         _array[i-1] = _array[i];
      --_nextIndex;
      }

   // element can be used to return an existing element
   //
   T& element(uint32_t index)
      {
      TR_ASSERT(index < _nextIndex, "TR_Array::element, Invalid array index %d while size is %d", index, _nextIndex);
      return _array[index];
      }

   // operator[] can be used to return an existing element or to create
   // space for a new element
   //
   T& operator[](uint32_t index)
      {
      if (index >= _nextIndex)
         {
         if (index >= _internalSize)
            growTo(index + _internalSize);
         _nextIndex = index + 1;
         }

      return _array[index];
      }

   const T& operator[] (uint32_t index) const
      {
      TR_ASSERT(index < _nextIndex, "TR_Array::operator[] const, Invalid array index %d while size is %d", index, _nextIndex);
      return _array[index];
      }


   bool isEmpty() const { return _nextIndex == 0; }

   void clear()
      {
      if (_zeroInit)
         {
         memset(_array, 0, _internalSize * sizeof(T));
         }
      _nextIndex = 0;
      }

   bool contains(T t)
      {
      for (uint32_t i = 0; i < _nextIndex; ++i)
         {
         if (_array[i] == t)
            {
            return true;
            }
         }
      return false;
      }

   void free() { return freeMemory(); }
   void freeMemory()
      {
      if (_allocationKind == persistentAlloc)
         jitPersistentFree(_array);
      }

protected:

   void copy(const TR_Array<T>& other)
      {
      _nextIndex = other._nextIndex;
      _internalSize = other._internalSize;
      _allocationKind = other._allocationKind;
      _trMemory = other._trMemory;
      _trPersistentMemory = other._trPersistentMemory;
      _zeroInit = other._zeroInit;
      if (_trMemory)
         _array = (T*)_trMemory->allocateMemory(_internalSize * sizeof(T), _allocationKind, TR_MemoryBase::Array);
      else if (_trPersistentMemory)
         _array = (T*)_trPersistentMemory->allocatePersistentMemory(_internalSize * sizeof(T));
      uint32_t copySize = _zeroInit ? _internalSize : _nextIndex;
      memcpy(_array, other._array, copySize * sizeof(T));
      }

   void growTo(uint32_t size)
      {
      uint32_t prevSize = _nextIndex * sizeof(T);
      uint32_t newSize = size * sizeof(T);

      char * tmpArray = nullptr;
      if (_trMemory)
         tmpArray = (char*)_trMemory->allocateMemory(newSize, _allocationKind, TR_MemoryBase::Array);
      else if (_trPersistentMemory)
         tmpArray = (char*)_trPersistentMemory->allocatePersistentMemory(newSize);
      else
         TR_ASSERT(false, "Cannot allocate memory for array with no TR_Memory or TR_PersistentMemory.");
      memcpy(tmpArray, _array, prevSize);
      if (_allocationKind == persistentAlloc) _trPersistentMemory->freePersistentMemory(_array);
      if (_zeroInit) memset(tmpArray + prevSize, 0, newSize - prevSize);

      _internalSize = size;
      _array = (T*)tmpArray;
      }

   T *                    _array;
   uint32_t               _nextIndex;
   uint32_t               _internalSize;
   TR_Memory *            _trMemory;
   TR_PersistentMemory*   _trPersistentMemory;
   bool                   _zeroInit;
   TR_AllocationKind      _allocationKind;
   };

template<class T> class TR_PersistentArray : public TR_Array<T>
   {
public:
   TR_ALLOC(TR_Memory::Array);
   TR_PersistentArray(TR_Memory * m,           uint32_t initialSize = 8, bool zeroInit = true) : TR_Array<T>(m, initialSize, zeroInit, persistentAlloc) { }
   TR_PersistentArray(TR_PersistentMemory * m, uint32_t initialSize = 8, bool zeroInit = true) : TR_Array<T>(m, initialSize, zeroInit) { }
   };

// Use to traverse non-null entries of a TR_Array<T *>
// Note the difference between the the template types for the iter, vs. the array
//
template <class T> class TR_ArrayIterator
   {
   private:
   uint32_t       _cursor;
   TR_Array<T *> *_array;

   public:
   TR_ALLOC(TR_Memory::Array)

   TR_ArrayIterator()                 : _array(0), _cursor(-1) {}
   TR_ArrayIterator(TR_Array<T *> *p) : _array(p), _cursor(-1) {}

   void set(TR_Array<T *> *p) { _array = p; _cursor = -1; }
   void reset() { _cursor = -1; }

   T *getFirst()   { _cursor = -1; return getNext(); }
   T *getNext()
         {
         T *elem = NULL;
         while (++_cursor < _array->size() &&
                (elem =_array->element(_cursor)) == NULL)
            ;
         return elem;
         }
   bool atEnd()   { return (_cursor >= _array->size()-1); }
   bool pastEnd() { return (_cursor >= _array->size()); }
   };

#endif
