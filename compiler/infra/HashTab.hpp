/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef HASHTAB_INCL
#define HASHTAB_INCL

#include <stdint.h>
#include <string.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"

#include "infra/Bit.hpp"

class TR_HashTab;
namespace TR { class Compilation; }

/**
 * general purpose hash table that assume a key which fits in 8 bytes
 * and stores a 'void *'.  Less space efficient than a template,
 * but templates have their own headaches.
 * User can overwrite two routines, and they must be explicitly
 * cast when being used. See HashTab.cpp for examples
 *
 *     uint32_t calculateHash(const void *key) const;
 */


// assume sizeof(long) >= sizeof(uint32_t)
typedef unsigned long TR_HashId;

class TR_HashTabIterator
   {
   public:
   TR_ALLOC(TR_Memory::HashTab)
   TR_HashTabIterator() : _curHashIndex(0) {}
   TR_HashTabIterator(TR_HashTab *h) : _baseHashTab(h),_curHashIndex(0){}
   void *getCurrent ();

   void *getFirst() { _curHashIndex = 0; return getCurrent(); }
   void *getNext() { ++_curHashIndex; return getCurrent(); }
   void reset() { _curHashIndex = 0; }
   bool atEnd();

   private:
   TR_HashId   _curHashIndex;
   TR_HashTab *_baseHashTab;
    };
// hash table with chaining.  Chained buckets are in a separate area
class TR_HashTab
   {

   protected:
   enum  { kMinimumSize=16,
           kDefaultSize=64};

   public:

   TR_ALLOC(TR_Memory::HashTab)

   // inherit and then overwrite these two methods as appropriate
   // assume a pointer
   virtual TR_HashId calculateHash(const void *key) const { return  (TR_HashId((uintptr_t)key) >> 2) %_closedAreaSize;}
   virtual bool isEqual(const void * key1, const void *key2) const { return key1 == key2; }

   TR_HashTab(TR_Memory *mem,
              TR_AllocationKind a=heapAlloc,
              uint32_t initialSize=kDefaultSize,
              bool g=true):
      _tableSize(0), _allocType(a),_table(NULL),_closedAreaSize(0),_allowGrowth(g),_trMemory(mem)
      {
      init(initialSize);
      _trace=false;
      }

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }

   void init(uint32_t size, bool allowGrowth=true);

   bool locate(const void* key,TR_HashId &hashIndex);

   bool growTo(uint32_t newSize);

   void reset()
      {
      _tableSize=0;
      if ((_allocType == persistentAlloc) &&
          (_table != NULL))
         {
         for (TR_HashId i=0; i < _tableSize ;++i)
            jitPersistentFree(_table[i]);
         jitPersistentFree(_table);
         }
      _table = NULL; // TODO if heap, notify vm that it's free
      }

   void clear() { memset(_table,0,sizeof(TR_HashTableEntry*) * _tableSize); }

   void * getData(TR_HashId id);
   void updateData(TR_HashId id, void *data)
      {
      _table[id]->_data = data;
      return;
      }
   void setKey(TR_HashId id, void* key) { _table[id]->_key = key;return; } ;

 /**
   * return true if successfully added
   * search table for matching record.  If
   * a collision, chain in a new entry.
   * if we run out of free space, die--in future grow and rehash
   */
   bool add(void *key,TR_HashId hashIndex,void * data)
      {
      TR_HashTableEntry *entry = new (trMemory(), _allocType) TR_HashTableEntry(key,data,0);
      return addElement(key,hashIndex,entry);
      }

   void replace(void *key, TR_HashId hashIndex, void *data)
      {
      TR_HashTableEntry *entry = new (trMemory(), _allocType) TR_HashTableEntry(key,data,0);
      TR_ASSERT(_table && _table[hashIndex], "entry in hashIndex is NULL\n");
      entry->_chain = _table[hashIndex]->_chain;
      _table[hashIndex] = entry;
      }

   private:

   void growAndRehash(uint32_t);

   class TR_HashTableEntry
      {
      public:

      TR_ALLOC(TR_Memory::HashTableEntry)
      TR_HashTableEntry():
         _chain(0),
         _data(NULL){}

      TR_HashTableEntry(void *key,void * data,uint32_t chain)
         {
         _key = key;
         _chain = chain;
         _data = data;
         }

      TR_HashTableEntry(const TR_HashTableEntry & entry):
         _key(entry._key),
         _chain(entry._chain),
         _data(entry._data){}

      void *       _key;
      void *       _data;
      uint32_t     _chain;

      };

   private:
   bool addElement(void * key,TR_HashId &hashIndex,TR_HashTableEntry *entry);

   uint32_t          _tableSize; // total table size (closed + open)
   TR::Compilation *  _comp;
   TR_Memory *       _trMemory;
   TR_AllocationKind _allocType;
   TR_HashId         _nextFree;  // next free slot in open area
   uint32_t          _mask;
   TR_HashTableEntry  **_table;
   bool              _allowGrowth;
   bool              _trace;
   protected:
   uint32_t          _closedAreaSize; // range of hashes
   friend bool TR_HashTabIterator::atEnd();
   friend void *TR_HashTabIterator::getCurrent();
   };

class TR_HashTabInt : public TR_HashTab
   {
   public:
   TR_HashTabInt(TR_Memory *mem, TR_AllocationKind allocType=heapAlloc, uint32_t initialSize=kDefaultSize, bool allowGrowth=true):
      TR_HashTab(mem, allocType, initialSize, allowGrowth) { }

   TR_HashId calculateHash(const void *key) const { return (TR_HashId((intptr_t)key) %_closedAreaSize);}

   bool locate(int32_t key,TR_HashId &hashIndex){ return TR_HashTab::locate((const void*)(uintptr_t)key,hashIndex);}
   bool add(int32_t key,TR_HashId hashIndex,void * data){ return TR_HashTab::add((void *)(uintptr_t)key,hashIndex,data);}
   };

class TR_HashTabString : public TR_HashTab
   {
   public:
   TR_HashTabString(TR_Memory *mem, TR_AllocationKind allocType=heapAlloc, uint32_t initialSize=kDefaultSize, bool allowGrowth=true):
      TR_HashTab(mem, allocType, initialSize, allowGrowth) { }

   TR_HashId calculateHash(const void *key) const // Bernstein's string hash
         {
         uint64_t hash = 5381;
         char * str = (char *) key;
         int32_t c;
         while((c = *str++))
            hash = ((hash <<5) + hash) + c; // hash * 33 + c
         return (TR_HashId(hash) %_closedAreaSize);
         }

   bool isEqual(const void * key1, const void *key2) const { return 0 == strcmp((char*)key1,(char *) key2); }
   bool locate(const char * key,TR_HashId &hashIndex){ return TR_HashTab::locate((const void*)key,hashIndex);}
   bool add(const char * key,TR_HashId hashIndex,void * data){ return TR_HashTab::add((void *)key,hashIndex,data);}
   };
class TR_HashTabDouble : public TR_HashTab
   {
   public:
   TR_HashTabDouble(TR_Memory *mem, TR_AllocationKind allocType=heapAlloc, uint32_t initialSize=kDefaultSize, bool allowGrowth=true):
      TR_HashTab(mem, allocType, initialSize, allowGrowth) { }

   TR_HashId calculateHash(const void *key) const // TODO: research better hash fns for double
         {
         union
            {
            double dbl;
            struct {
            int32_t upperInt;
            int32_t lowerInt;
               }intVal;
            } x;
          x.dbl = *(double *)key;

         int32_t hash = x.intVal.lowerInt | x.intVal.upperInt;

         return (TR_HashId(hash) %_closedAreaSize);
         }

   bool isEqual(const void * key1, const void *key2) const { double a = *(double*)key1; double b = *(double*)key2; return a == b; }
   bool locate(const double * key,TR_HashId &hashIndex){ return TR_HashTab::locate((const void*)key,hashIndex);}
   bool add(double * key,TR_HashId hashIndex,void * data){ return TR_HashTab::add((void *)key,hashIndex,data);}
   };

class TR_HashTabLong : public TR_HashTab
   {
   public:
   TR_HashTabLong(TR_Memory *mem, TR_AllocationKind allocType=heapAlloc, uint32_t initialSize=kDefaultSize, bool allowGrowth=true):
      TR_HashTab(mem, allocType, initialSize, allowGrowth) { }

   TR_HashId calculateHash(const void *key) const // Bernstein's string hash
         {
         union
            {
            int64_t longVal;
            struct {
            int32_t upperInt;
            int32_t lowerInt;
               }intVal;
            } x;
          x.longVal = *(int64_t *)key;

         int32_t hash = x.intVal.lowerInt | x.intVal.upperInt;

         return (TR_HashId(hash) %_closedAreaSize);
         }

   bool isEqual(const void * key1, const void *key2) const { return *(int64_t *)key1 == *(int64_t*)key2; }
   bool locate(const int64_t * key,TR_HashId &hashIndex){ return TR_HashTab::locate((const void*)key,hashIndex);}
   bool add(int64_t * key,TR_HashId hashIndex,void * data){ return TR_HashTab::add((void *)key,hashIndex,data);}
   };

class TR_HashTabFloat : public TR_HashTab
   {
   public:
   TR_HashTabFloat(TR_Memory *mem, TR_AllocationKind allocType=heapAlloc,
                   uint32_t initialSize=kDefaultSize, bool allowGrowth=true):
      TR_HashTab(mem, allocType, initialSize, allowGrowth) { }

   TR_HashId calculateHash(const void *key) const
         {
         union
            {
            float flt;
            int32_t intVal;
            } x;
         x.flt = *(float*)key;

         int32_t hash = x.intVal;

         return (TR_HashId(hash) %_closedAreaSize);
         }

   bool isEqual(const void * key1, const void *key2) const { return *(float*)key1 == *(float*)key2;}
   bool locate(const float *key,TR_HashId &hashIndex){ return TR_HashTab::locate((const void*)key,hashIndex);}
   bool add(float *key,TR_HashId hashIndex,void * data){ return TR_HashTab::add((void *)key,hashIndex,data);}
   };


#endif
