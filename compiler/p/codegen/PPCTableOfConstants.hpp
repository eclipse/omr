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

#ifndef PPCTABLEOFCONSTANTS_INCL
#define PPCTABLEOFCONSTANTS_INCL

#include "codegen/TableOfConstants.hpp"

#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Monitor.hpp"

class TR_FrontEnd;
namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class PersistentInfo; }
namespace TR { class SymbolReference; }

struct TR_tocHashEntry
   {
   union
      {
      uint64_t   _d;
      int8_t    *_n;
      intptr_t  _cp;
      intptr_t  _addr;
      uint32_t   _f;
      } _key;

   intptr_t   _keyTag ;   // class-loader or anonClass used as key2 for hashed "names"
   int32_t     _flag;
   int32_t     _collisionChain;
   int32_t     _tocIndex;
   int32_t     _staticCPIndex;
   };
#define _nKey     _key._n
#define _dKey     _key._d
#define _fKey     _key._f
#define _addrKey  _key._addr
#define _cpKey    _key._cp
#define TR_FLAG_tocNameKey           0x00000001
#define TR_FLAG_tocAddrKey           0x00000002
#define TR_FLAG_tocDoubleKey         0x00000004
#define TR_FLAG_tocFloatKey          0x00000008
#define TR_FLAG_tocFloatHigh         0x00000010
#define TR_FLAG_tocStatic2ClassKey   0x00000020

// PTOC_FULL_INDEX value has to be special:
//    no valid index or index*sizeof(intptr_t) can equal to PTOC_FULL_INDEX
#define CHAIN_END             (-1)
#define PTOC_FULL_INDEX       0

class TR_PPCTableOfConstants : public TableOfConstants
   {

   TR_PERSISTENT_ALLOC(TR_Memory::TableOfConstants)

   uintptr_t              *_tocBase;
   struct TR_tocHashEntry  *_hashMap;
   int8_t                  *_nameAStart, *_nameACursor;
   int64_t     _nameASize;
   int32_t     _lastFloatCursor, _hashSize, _hashTop, _collisionCursor;
   int32_t     _upLast, _downLast;
   int32_t     _upCursor, _downCursor, _upCursorAfterPermanentEntries, _downCursorAfterPermanentEntries;

   uint8_t                 *_tocPtr;
   uint32_t                 _tocSize;
   bool                     _permanentEntriesAddtionComplete;
   TR::Monitor *_tocMonitor;

   TR::PersistentInfo *_persistentInfo;

   public:

   TR_PPCTableOfConstants(uint32_t size)
      : TableOfConstants(size), _tocBase(NULL)
      {
      _downLast = (size>>1)/sizeof(uintptr_t);
      _upLast = -(_downLast + 1);
      _upCursor = _upCursorAfterPermanentEntries = -1;
      _downCursor = _downCursorAfterPermanentEntries = 0;
      _tocMonitor = NULL;
      }

   ~TR_PPCTableOfConstants()
      {

      if(_tocPtr != 0)
         {
         int results = munmap(_tocPtr, _tocSize);
         TR_ASSERT(0 == results, "_tocPtr is not munmap properly. \n");
         }
      if(_nameAStart != 0)
         {
         jitPersistentFree(_nameAStart);
         }
      if(_hashMap != 0)
         {
         jitPersistentFree(_hashMap);
         }
      if(_tocMonitor != 0)
         {
         TR::Monitor::destroy(_tocMonitor);
         }
      }

   static void       *initTOC(TR_FrontEnd *vm, TR::PersistentInfo *, uintptr_t systemTOC);
   static void        reinitializeMemory();
   static void        shutdown(TR_FrontEnd *vm);
   static int32_t     lookUp(int32_t val, struct TR_tocHashEntry *lk, int32_t *s, TR::CodeGenerator *cg);  // return index
   static int32_t     lookUp(int8_t *n, int32_t len, bool isAddr,intptr_t loader, TR::CodeGenerator *cg); // return index
   static int32_t     lookUp(TR::SymbolReference *symRef, TR::CodeGenerator *);                            // return index
   static int32_t     lookUp(double d, TR::CodeGenerator *cg);                                             // return offset
   static int32_t     lookUp(float f, TR::CodeGenerator *cg);                                              // return offset
   static int32_t     allocateChunk(uint32_t numEntries, TR::CodeGenerator *cg, bool grabMonitor = true);
   static void        onClassUnloading(void *loaderPtr);
   static void        permanentEntriesAddtionComplete();
   static bool        isPermanentEntriesAddtionComplete();

   static uintptr_t   getTOCSlot(int32_t offset);
   static void         setTOCSlot(int32_t offset, uintptr_t v);

   int32_t getUpLast() {return _upLast;}
   int32_t getDownLast() {return _downLast;}

   int32_t getUpCursor() {return _upCursor;}
   void    setUpCursor(int32_t c) {_upCursor = c;}

   int32_t getUpCursorAfterPermanentEntries() {return _upCursorAfterPermanentEntries;}
   void    setUpCursorAfterPermanentEntries(int32_t c) {_upCursorAfterPermanentEntries = c;}
   int32_t getDownCursor() {return _downCursor;}
   void    setDownCursor(int32_t d) {_downCursor = d;}
   int32_t getDownCursorAfterPermanentEntries() {return _downCursorAfterPermanentEntries;}
   void    setDownCursorAfterPermanentEntries(int32_t d) {_downCursorAfterPermanentEntries = d;}
   void    setPermanentEntriesAddtionComplete(bool b) { _permanentEntriesAddtionComplete = b; };
   bool    getPermanentEntriesAddtionComplete() { return _permanentEntriesAddtionComplete; }

   uintptr_t *getTOCBase() {return _tocBase;}
   void       setTOCBase(uintptr_t *b) {_tocBase=b;}

   uint8_t *getTOCPtr() {return _tocPtr;}
   void     setTOCPtr(uint8_t* tocPtr) {_tocPtr = tocPtr;}

   uint32_t getTOCSize() {return _tocSize;}
   void     setTOCSize(uint32_t tocSize) {_tocSize = tocSize;}


   struct TR_tocHashEntry *getHashMap() {return _hashMap;}
   void   setHashMap(TR_tocHashEntry *m) {_hashMap = m;}

   int32_t getLastFloatCursor() {return _lastFloatCursor;}
   void    setLastFloatCursor(int32_t c) {_lastFloatCursor = c;}

   int32_t getHashSize() {return _hashSize;}
   void    setHashSize(int32_t c) {_hashSize = c;}

   int32_t getHashTop() {return _hashTop;}
   void    setHashTop(int32_t c) {_hashTop = c;}

   int32_t getCollisionCursor() {return _collisionCursor;}
   void    setCollisionCursor(int32_t c) {_collisionCursor = c;}

   int8_t *getNameAStart() {return _nameAStart;};
   void    setNameAStart(int8_t *p) {_nameAStart = p;}

   int8_t *getNameACursor() {return _nameACursor;}
   void    setNameACursor(int8_t *p) {_nameACursor = p;}

   int64_t getNameASize() {return _nameASize;}
   void    setNameASize(int64_t s) {_nameASize = s;}

   TR::Monitor *getTOCMonitor() const { return _tocMonitor; }
   void setTOCMonitor(TR::Monitor* m) { _tocMonitor = m; }
   };

inline TR_PPCTableOfConstants *toPPCTableOfConstants(void *p)
   {
   return (TR_PPCTableOfConstants *)p;
   }

class TR_PPCLoadLabelItem
   {
   // Specific label loading requests are tracked in an array of this class
   TR::LabelSymbol  *_label;
   int32_t          _tocOffset;

   public:
      TR_ALLOC(TR_Memory::PPCLoadLabelItem)

      TR_PPCLoadLabelItem(int32_t o, TR::LabelSymbol *l)
      : _label(l), _tocOffset(o) {}

      TR::LabelSymbol *getLabel() {return _label;}

      int32_t getTOCOffset() {return _tocOffset;}
      void setTOCOffset(int32_t idx) {_tocOffset = idx;}
   };

#endif
