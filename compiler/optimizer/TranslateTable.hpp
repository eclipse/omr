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

#ifndef TR_TRANSLATETABLE_INCL
#define TR_TRANSLATETABLE_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"

namespace TR { class SymbolReference; }
namespace TR { class Compilation; }

class TR_TranslateTable
   {
   public:
      TR_ALLOC(TR_Memory::LoopTransformer)
      TR_TranslateTable(TR::Compilation* comp) : _comp(comp), _table(NULL), _symRef(NULL) {}

      TR::Compilation * comp() { return _comp; }

      uint8_t* data();
      TR::SymbolReference * createSymbolRef();
      void dumpTable();
      static int tableSize(uint8_t inputSize, uint8_t outputSize);

   protected:

      struct TR_TranslateTableData
         {
         TR_ALLOC(TR_Memory::LoopTransformer)
         TR_TranslateTableData* next;  // next table in linked list
         uint8_t* data;                // pointer to data table
         uint32_t defaultValue;        // default value
         uint32_t startA;              // only applicable if a range
         uint32_t endA;                // only applicable if a range
         uint32_t startB;              // only applicable if a range
         uint32_t endB;                // only applicable if a range
         uint8_t inSize;               // number of bits for input size (8 or 16)
         uint8_t outSize;              // number of bits for output size (8 or 16)
         };
      TR_TranslateTable::TR_TranslateTableData* table();
      void setTable(TR_TranslateTableData* match);


      TR_TranslateTableData* createTable(uint32_t startA, uint32_t endA, uint32_t startB, uint32_t endB, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue);
      TR_TranslateTableData* createTable(uint32_t startA, uint32_t endA, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue);
      TR_TranslateTableData* createTable(uint16_t startA, uint16_t endA, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue);
      TR_TranslateTableData* createTable(uint16_t startA, uint16_t endA, uint16_t startB, uint16_t endB, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue);

      TR_TranslateTableData * matchTable(uint8_t inputSize, uint8_t outputSize, void *array);

   private:
      // msf - methods that update head will need a lock when we move to an async compilation model
      static TR_TranslateTableData* _head;
      TR_TranslateTableData* _table;
      TR::SymbolReference * _symRef;
      TR::Compilation * _comp;

      void updateTable();

      // can do something smarter if number of tables gets big - expected to be quite small (1 or 2 tables in practice)
      TR_TranslateTableData * matchTable(uint32_t startA, uint32_t endA, uint32_t startB, uint32_t endB, uint8_t inputSize, uint8_t outputSize, uint16_t defaultValue);
   };

class TR_RangeTranslateTable : public TR_TranslateTable     // range of rare values stop translation
   {
   public:
      TR_RangeTranslateTable(TR::Compilation*, uint8_t, uint8_t, uint32_t, uint32_t, uint16_t);
      TR_RangeTranslateTable(TR::Compilation*, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, uint16_t);
      TR_RangeTranslateTable(TR::Compilation*, uint8_t, uint8_t, uint16_t, uint16_t, uint16_t);
      TR_RangeTranslateTable(TR::Compilation*, uint8_t, uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
   };

class TR_SetTranslateTable : public TR_TranslateTable       // set of rare values stop translation
   {
   public:
      TR_SetTranslateTable(TR::Compilation* , uint8_t, uint8_t, uint8_t set[]);  // input and output size in bits
      TR_SetTranslateTable(TR::Compilation* , uint8_t, uint8_t, uint16_t set[]); // input and output size in bits
      TR_SetTranslateTable(TR::Compilation* , uint8_t, uint8_t, void *set, int copySize);  // input and output size in bits
   };

#endif
