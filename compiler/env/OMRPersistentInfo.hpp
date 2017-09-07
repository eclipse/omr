/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_PERSISTENTINFO_INCL
#define OMR_PERSISTENTINFO_INCL

#ifndef OMR_PERSISTENTINFO_CONNECTOR
#define OMR_PERSISTENTINFO_CONNECTOR
namespace OMR { class PersistentInfo; }
namespace OMR { typedef PersistentInfo PersistentInfoConnector; }
#endif

#include <stddef.h>                        // for NULL
#include <stdint.h>                        // for int32_t, etc
#include "codegen/TableOfConstants.hpp"

class TR_AddressSet;
class TR_DebugExt;
class TR_FrontEnd;
class TR_PersistentMemory;
class TR_PseudoRandomNumbersListElement;

namespace OMR { class Options; }
namespace TR { class PersistentInfo; }
namespace TR { class DebugCounterGroup; }
namespace TR { class CodeCacheManager; }
namespace TR { class Monitor; }


#define PSEUDO_RANDOM_NUMBERS_SIZE 1000
class TR_PseudoRandomNumbersListElement
   {
   public:

   TR_PseudoRandomNumbersListElement()
     :_next(0), _curIndex(0)
      {}

   int32_t _pseudoRandomNumbers[PSEUDO_RANDOM_NUMBERS_SIZE];
   int32_t _curIndex;
   TR_PseudoRandomNumbersListElement *_next;
   };



namespace OMR
{

class PersistentInfo
   {
   public:
   friend class ::TR_DebugExt;
   friend class ::OMR::Options;
   PersistentInfo(TR_PersistentMemory *pm) :
         _persistentMemory(pm),
         _lastDebugCounterResetSeconds(0),
         _pseudoRandomNumbersListHead(NULL),
         _curPseudoRandomNumbersListElem(NULL),
         _curIndex(0),
         _dynamicCounters(NULL),
         _staticCounters(NULL),
         _codeCacheManager(NULL),
         _persistentTOC(NULL)
      {}

   TR::PersistentInfo * self();

   TR::DebugCounterGroup *getStaticCounters() { if (!_staticCounters)  createCounters(_persistentMemory); return _staticCounters;  }
   TR::DebugCounterGroup *getDynamicCounters(){ if (!_dynamicCounters) createCounters(_persistentMemory); return _dynamicCounters; }


   int64_t getLastDebugCounterResetSeconds() const { return _lastDebugCounterResetSeconds; }
   void setLastDebugCounterResetSeconds(int64_t newValue) { _lastDebugCounterResetSeconds = newValue; }

   void createCounters(TR_PersistentMemory *mem);

   // For CFG.
   int32_t getCurIndex() { return _curIndex; }
   TR_PseudoRandomNumbersListElement  *getCurPseudoRandomNumbersListElem() { return _curPseudoRandomNumbersListElem; }
   TR_PseudoRandomNumbersListElement *getPseudoRandomNumbersList() { return _pseudoRandomNumbersListHead; }
   TR_PseudoRandomNumbersListElement **getPseudoRandomNumbersListPtr() { return &_pseudoRandomNumbersListHead; }

   TR_PseudoRandomNumbersListElement *advanceCurPseudoRandomNumbersListElem() { return NULL; }
   int32_t getNextPseudoRandomNumber(int32_t i)
      {
      advanceCurPseudoRandomNumbersListElem();
      if (_curPseudoRandomNumbersListElem)
         return _curPseudoRandomNumbersListElem->_pseudoRandomNumbers[_curIndex];
      else
         return i;
      }


   TR::CodeCacheManager *getCodeCacheManager() const { return _codeCacheManager; }
   void setCodeCacheManager(TR::CodeCacheManager * manager) { _codeCacheManager = manager; }

   TableOfConstants *getPersistentTOC() {return _persistentTOC;}
   void setPersistentTOC(TableOfConstants *toc) {_persistentTOC = toc;}

   bool isObsoleteClass(void *v, TR_FrontEnd *fe) { return false; } // Has class been unloaded, replaced (HCR), etc.

   bool isRuntimeInstrumentationEnabled() { return false; }



   protected:
   TR_PersistentMemory *_persistentMemory;
   TR_PseudoRandomNumbersListElement *_pseudoRandomNumbersListHead;
   TR_PseudoRandomNumbersListElement *_curPseudoRandomNumbersListElem;
   int32_t _curIndex;


   private:
   TR::DebugCounterGroup *_staticCounters;
   TR::DebugCounterGroup *_dynamicCounters;
   int64_t _lastDebugCounterResetSeconds;
   TR::CodeCacheManager *_codeCacheManager;
   TableOfConstants *_persistentTOC;
   };

}

#endif
