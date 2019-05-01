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

#include "infra/HashTab.hpp"

#include <stdio.h>
#include "compile/Compilation.hpp"
#include "infra/Bit.hpp"

#ifdef DEBUG
void TRHashTab_testit()
   {
   TR::Compilation * comp = TR::comp();
   int32_t i;
   TR_HashTabInt x(comp->trMemory());
   TR_HashId id;
   printf("testing hashtable....\n");
   TR_ASSERT(false == x.locate(12,id),"should be false\n");
   x.add(12,id,(void *)12);
   TR_ASSERT(true == x.locate(12,id),"should be false\n");
   printf("starting\n");
   for (i =13;i < 1000;++i)
      {
      bool rc = x.locate(i,id);
      TR_ASSERT(rc == false,"should not be there\n");
      x.add(i,id,(void *)long(i));
      }
   for (i =12;i < 1000;++i)
      {
      bool rc = x.locate(i,id);
      TR_ASSERT(rc ,"should be there\n");
      long  data = (long)x.getData(id);
      TR_ASSERT((long)data == i,"%d != %d\n",(int32_t)i,(int32_t)data);
      }
   printf("done\n");
   }

#endif


void * TR_HashTab::getData(TR_HashId id)
   {
   TR_HashTableEntry * entry = _table[id];
   TR_ASSERT(id < _tableSize,"%d >= %d\n",id,_tableSize);
   TR_ASSERT(entry,"entry is null for id %d\n",id);
   return entry->_data;
   }


// will allocate table on demand
void TR_HashTab::init(uint32_t newSize, bool growth)
   {
   uint32_t openAreaSize;

   _allowGrowth = growth;

   _closedAreaSize = ceilingPowerOfTwo(newSize);
   _closedAreaSize = _closedAreaSize < kMinimumSize?kMinimumSize:_closedAreaSize;
   _mask = _closedAreaSize -1;
   _nextFree = _closedAreaSize +1;
   openAreaSize = _closedAreaSize /4;
   _tableSize = _closedAreaSize + openAreaSize;
   _table = (TR_HashTableEntry **)trMemory()->allocateMemory(_tableSize * sizeof(TR_HashTableEntry *), _allocType,  TR_Memory::HashTableEntry);
   memset(_table,0,sizeof(TR_HashTableEntry*) * _tableSize);
   }

/**
 * find and set hashIndex for the key.  Return true if element
 * exists in table.  modified hashIndex used when adding an element
 */
bool TR_HashTab::locate(const void* key,TR_HashId &hashIndex)
   {
   TR_HashId hashValue;
   hashValue = calculateHash(key);

   hashIndex = (hashValue & _mask) + 1;

   TR_ASSERT(hashIndex,"hash value is 0\n");

   if (NULL == _table) return false;
   if (NULL == _table[hashIndex]) return false;

   uint32_t collisions=0;
   while ( !isEqual(key,_table[hashIndex]->_key))
      {
      TR_HashId nextId = _table[hashIndex]->_chain;
      ++collisions;
      if (0 == nextId) return false;
      hashIndex = nextId;
      }
   return true;
   }


/**
 * have added too many entries--time to grow the table and rehash it
 */
void TR_HashTab::growAndRehash(uint32_t newSize)
   {
   TR_ASSERT(_allowGrowth,"Hash collision table is full %d\n",_tableSize);
   TR_HashTableEntry **oldTable = _table;
   uint32_t oldSize = _tableSize;

   if (_trace) printf("Regrowing to %d\n",newSize);
   init(newSize);

   TR_HashId  newId=0;
   for (TR_HashId i=0;i < oldSize;++i)
      {
      TR_HashTableEntry *oldEntry =oldTable[i];
      if (oldEntry)
         {
         addElement(oldEntry->_key,newId,oldEntry);
         }
      }
   if ((_allocType == persistentAlloc) &&
       (oldTable != NULL))
      {
      jitPersistentFree(oldTable);
      }
   }

bool TR_HashTab::growTo(uint32_t newSize)
   {
   if (NULL == _table)
      {
      init(newSize);
      }
   else
      {
      growAndRehash(newSize);
      }
   return true;
   }

/**
   * return true if element added, false if already in table.  hashIndex set to
   * element.
   */
bool TR_HashTab::addElement(void * key,TR_HashId &hashIndex,TR_HashTableEntry *entry)
   {
   if (_nextFree == _tableSize-1)
      {
      growAndRehash(uint32_t(_closedAreaSize *1.25)); // grow by 25%
      }

   if (locate(key,hashIndex)) return false;

   TR_ASSERT(hashIndex < _tableSize,"index too big %d >=%d\n",hashIndex,_tableSize);

   entry->_chain = 0;
   if (NULL == _table[hashIndex])
      {
      _table[hashIndex] = entry;
      return true;
      }

   TR_HashId nextIndex = hashIndex;
   do
      {
      hashIndex = nextIndex;
      nextIndex = _table[hashIndex]->_chain;

      if (0 == nextIndex)
         {
         nextIndex = _nextFree++;
         TR_ASSERT(_nextFree < _tableSize,"out of free space: index %d (%d)\n",nextIndex,_tableSize);

         _table[hashIndex]->_chain = nextIndex;
         _table[nextIndex] = entry;
         hashIndex = nextIndex;
         return true;
         }
      } while(!isEqual(key,_table[hashIndex]->_key));

   return false;
   }


void *TR_HashTabIterator::getCurrent ()
   {
   for(;_curHashIndex < _baseHashTab->_tableSize;++_curHashIndex)
      {
      if(_baseHashTab->_table[_curHashIndex])
         return _baseHashTab->getData(_curHashIndex);
      }
   return NULL;
   }

bool TR_HashTabIterator::atEnd() { return _curHashIndex >= _baseHashTab->_tableSize; }
