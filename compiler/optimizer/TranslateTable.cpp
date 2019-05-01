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

#include "optimizer/TranslateTable.hpp"

#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"

namespace TR { class SymbolReference; }

uint8_t* TR_TranslateTable::data()
   {
   return _table->data;
   }

TR_TranslateTable::TR_TranslateTableData* TR_TranslateTable::table()
   {
   return _table;
   }

void TR_TranslateTable::setTable(TR_TranslateTableData* match)
   {
   _table = match;
   }

void TR_TranslateTable::dumpTable()
   {
   int count = tableSize(table()->inSize, table()->outSize);
   dumpOptDetails(comp(), "\n\nTranslation table at address %p, size %d\n", data(), count);
   dumpOptDetails(comp(), "\n  Range %d to %d and %d to %d default value %d\n", table()->startA, table()->endA, table()->startB, table()->endB, table()->defaultValue);
   if (table()->outSize == 16)
      {
      for (int i=0; i<count*2; i+=2)
         {
         if (i % 16 == 0) dumpOptDetails(comp(), "\n");
         dumpOptDetails(comp(), "%02x%02x ", data()[i], data()[i+1]);
         }
      }
   else
      {
      for (int i=0; i<count; i+=2)
         {
         if (i % 16 == 0) dumpOptDetails(comp(), "\n");
         dumpOptDetails(comp(), "%2x %2x ", data()[i], data()[i+1]);
         }
      }
   }

TR::SymbolReference * TR_TranslateTable::createSymbolRef()
   {
   if (!_symRef)
      {
      _symRef = _comp->getSymRefTab()->createKnownStaticDataSymbolRef(data(), TR::Address);
      }
   return _symRef;
   }

int TR_TranslateTable::tableSize(uint8_t inputSize, uint8_t outputSize)
   {
   if (inputSize == 8 && outputSize == 8)
      {
      return 256;
      }
   else if (inputSize == 16 && outputSize == 8)
      {
      return 65536;
      }
   else if (inputSize == 8 && outputSize == 16)
      {
      // return the number of elements instead
      // of bytes to be allocated
      return 256;
      ///return 512;
      }
   else if (inputSize == 16 && outputSize == 16)
      {
      // return the number of elements instead
      // of bytes to be allocated
      return 65536;
      ///return 131072;
      }
   else
      {
      TR_ASSERT(0, "Invalid input and output sizes specified\n");
      return 0; // keep compiler happy
      }
   }

TR_TranslateTable::TR_TranslateTableData* TR_TranslateTable::createTable(uint32_t startA, uint32_t endA, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue)
   {
   return createTable(startA, endA, 0, 0, inputSize, outputSize, defaultValue);
   }

TR_TranslateTable::TR_TranslateTableData* TR_TranslateTable::createTable(uint16_t startA, uint16_t endA, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue)
   {
   return createTable((uint32_t)startA, (uint32_t)endA, 0, 0, inputSize, outputSize, defaultValue);
   }

TR_TranslateTable::TR_TranslateTableData* TR_TranslateTable::createTable(uint16_t startA, uint16_t endA, uint16_t startB, uint16_t endB, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue)
   {
   return createTable((uint32_t)startA, (uint32_t)endA, (uint32_t)startB, (uint32_t)endB, inputSize, outputSize, defaultValue);
   }

TR_TranslateTable::TR_TranslateTableData* TR_TranslateTable::createTable(uint32_t startA, uint32_t endA, uint32_t startB, uint32_t endB, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue)
   {
   TR_TranslateTableData * match = matchTable(startA, endA, startB, endB, inputSize, outputSize, defaultValue);
   if (match)
      {
      _table = match;
      return _table;
      }

   int totalSize = tableSize(inputSize, outputSize);
   int allocSize = totalSize;
   if (allocSize > 4096)
      {
      allocSize += 4096;
      }
   else
      {
      allocSize += 8;
      }

   // msf - methods that update _head will need a lock when we move to a true async compilation (re-entrant) model

   // double the allocation size
   // if required
   if (outputSize == 16)
      allocSize += allocSize;

   _table = new (PERSISTENT_NEW) TR_TranslateTable::TR_TranslateTableData();
   uint8_t* data = (uint8_t*)jitPersistentAlloc(allocSize);
   if (allocSize > 4096)
      {
      data = (uint8_t*)(((((uint64_t) (data)) + 4096) >> 12) << 12);
      }
   else
      {
      data = (uint8_t*)(((((uint64_t) (data)) + 8) >> 3) << 3);
      }


   if (startA > startB)
      {
      uint16_t tempStart = startA;
      uint16_t tempEnd   = endA;

      startA    = startB;
      endA      = endB;
      startB    = tempStart;
      endB      = tempEnd;
      }

   int i;
   if (outputSize == 16)
      {
      uint16_t* cTable = (uint16_t*) data;
      for (i=0; i<startA; ++i)
         {
         cTable[i] = defaultValue;
         }
      for (i=startA; i<endA; ++i)
         {
         cTable[i] = i;
         }
      if (startB != endB)
         {
         for (i=endA; i<startB; ++i)
            {
            cTable[i] = defaultValue;
            }
         for (i=startB; i<endB; ++i)
            {
            cTable[i] = i;
            }
         for (i=endB; i<totalSize; ++i)
            {
            cTable[i] = defaultValue;
            }
         }
      else
         {
         for (i=endA; i<totalSize; ++i)
            {
            cTable[i] = defaultValue;
            }
         }
      }
   else // outputSize is 8
      {
      uint8_t* bTable = (uint8_t*) data;
      uint8_t  bDefaultValue = (uint8_t) defaultValue;
      for (i=0; i<startA; ++i)
         {
         bTable[i] = bDefaultValue;
         }
      for (i=startA; i<endA; ++i)
         {
         bTable[i] = i;
         }
      if (startB != endB)
         {
         for (i=endA; i<startB; ++i)
            {
            bTable[i] = bDefaultValue;
            }
         for (i=startB; i<endB; ++i)
            {
            bTable[i] = i;
            }
         for (i=endB; i<totalSize; ++i)
            {
            bTable[i] = bDefaultValue;
            }
         }
      else
         {
         for (i=endA; i<totalSize; ++i)
            {
            bTable[i] = defaultValue;
            }
         }
      }
   _table->data = data;
   _table->inSize  = inputSize;
   _table->outSize = outputSize;
   _table->startA = startA;
   _table->endA = endA;
   _table->startB = startB;
   _table->endB = endB;
   _table->defaultValue = defaultValue;
   updateTable();
   return _table;

   }

TR_TranslateTable::TR_TranslateTableData* TR_TranslateTable::_head = 0;
void TR_TranslateTable::updateTable()
   {
   _table->next = _head;
   _head = _table;
   }

// can do something smarter if number of tables gets big - expected to be quite small (1 or 2 tables in practice)
TR_TranslateTable::TR_TranslateTableData * TR_TranslateTable::matchTable(uint32_t startA, uint32_t endA, uint32_t startB, uint32_t endB, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue)
   {
   if (startA == 0 && endA == tableSize(inputSize, outputSize))
      {
      // this is a set - it can not be shared
      return NULL;
      }

   TR_TranslateTable::TR_TranslateTableData * curr = _head;
   while (curr)
      {
      if (curr->startA == startA && curr->endA == endA &&
          curr->startB == startB && curr->endB == endB &&
          curr->inSize == inputSize && curr->outSize == outputSize &&
          curr->defaultValue == defaultValue)
         {
         return curr;
         }
      curr = curr->next;
      }
   return curr;
   }

// searching SetTranslateTable
TR_TranslateTable::TR_TranslateTableData * TR_TranslateTable::matchTable(uint8_t inputSize, uint8_t outputSize, void *array)
   {
   int tableSizeParm = tableSize(inputSize, outputSize);
   int tableByteSize = tableSizeParm * (outputSize/8);
   TR_TranslateTable::TR_TranslateTableData * curr = _head;
   while (curr)
      {
      if (curr->inSize == inputSize && curr->outSize == outputSize)
         {
	 if (memcmp(array, curr->data, tableByteSize) == 0)
	    {
	    return curr;
	    }
         }
      curr = curr->next;
      }
   return curr;
   }

TR_RangeTranslateTable::TR_RangeTranslateTable(TR::Compilation* comp, uint8_t inputSize, uint8_t outputSize, uint16_t start, uint16_t end, uint16_t defaultValue) : TR_TranslateTable(comp)
   {
   createTable(start, end, inputSize, outputSize, defaultValue);
   }
TR_RangeTranslateTable::TR_RangeTranslateTable(TR::Compilation* comp, uint8_t inputSize, uint8_t outputSize, uint16_t startA, uint16_t endA, uint16_t startB, uint16_t endB, uint16_t defaultValue) : TR_TranslateTable(comp)
   {
   createTable(startA, endA, startB, endB, inputSize, outputSize, defaultValue);
   }

TR_RangeTranslateTable::TR_RangeTranslateTable(TR::Compilation* comp, uint8_t inputSize, uint8_t outputSize, uint32_t start, uint32_t end, uint16_t defaultValue) : TR_TranslateTable(comp)
   {
   createTable(start, end, inputSize, outputSize, defaultValue);
   }

TR_RangeTranslateTable::TR_RangeTranslateTable(TR::Compilation* comp, uint8_t inputSize, uint8_t outputSize, uint32_t startA, uint32_t endA, uint32_t startB, uint32_t endB, uint16_t defaultValue) : TR_TranslateTable(comp)
   {
   createTable(startA, endA, startB, endB, inputSize, outputSize, defaultValue);
   }

TR_SetTranslateTable::TR_SetTranslateTable(TR::Compilation* comp, uint8_t inputSize, uint8_t outputSize, uint8_t set[]) : TR_TranslateTable(comp) // input and output size in bits
   {
   createTable((uint16_t)0, tableSize(inputSize, outputSize), inputSize, outputSize, (uint16_t)0);
   for (int i=0; set[i] > 0; ++i)
      {
      data()[i] = set[i];
      }
   }


TR_SetTranslateTable::TR_SetTranslateTable(TR::Compilation* comp, uint8_t inputSize, uint8_t outputSize, uint16_t set[]) : TR_TranslateTable(comp) // input and output size in bits
   {
   createTable((uint32_t)0, tableSize(inputSize, outputSize), inputSize, outputSize, (uint32_t)0);
   for (int i=0; set[i] > 0; ++i)
      {
      ((uint16_t*)data())[i] = set[i];
      }
   }

TR_SetTranslateTable::TR_SetTranslateTable(TR::Compilation* comp, uint8_t inputSize, uint8_t outputSize, void *set, int copyElemSize) : TR_TranslateTable(comp) // input and output size in bits
   {
   int tableSizeParm = tableSize(inputSize, outputSize);
   int tableByteSize = tableSizeParm * (outputSize/8);
   int copyByteSize = copyElemSize * (outputSize/8);
   TR_ASSERT(copyByteSize <= tableByteSize, "error!");

   if (tableByteSize == copyByteSize)
      {
      TR_TranslateTableData * match = TR_TranslateTable::matchTable(inputSize, outputSize, set);
      if (match)
	 {
	 setTable(match);
	 return;
	 }
      }
   createTable((uint32_t)0, tableSizeParm, inputSize, outputSize, (uint32_t)0);
   memcpy(data(), set, copyByteSize);
   }
