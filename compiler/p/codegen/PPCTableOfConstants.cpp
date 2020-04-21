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

#include "p/codegen/PPCTableOfConstants.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Monitor.hpp"
#include "runtime/Runtime.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

class TR_OpaqueClassBlock;

void *TR_PPCTableOfConstants::initTOC(TR_FrontEnd *fe, TR::PersistentInfo * pinfo, uintptr_t systemTOC)
   {
   int32_t tsize = TR::Options::getCmdLineOptions()->getTOCSize();

   TR_ASSERT(pinfo->getPersistentTOC()==0, "TOC needs to be initialized once only.\n");

   if (tsize == 0)
      return (void*) 0x1; // error code

   if (tsize < 64)
      {
      tsize = 64; // On Java the default TOC size will be 64k.
      /// FIXME: do we need to have a hard lower limit?
      }

   if (tsize > 2048)
      {
      tsize = 2048;
      }
   tsize <<= 10;     // Turn it into size in byte

   TR_PPCTableOfConstants *tocManagement = new (PERSISTENT_NEW) TR_PPCTableOfConstants(tsize);
   if (!tocManagement)
      return (void*)0x1; // error code
   pinfo->setPersistentTOC(tocManagement);

   // Allocate the TOC memory: use relocationData for now -- we will get a specific allocator
   uint8_t *tocPtr;
   tocPtr = fe->allocateRelocationData(0, tsize);
   if (!tocPtr)
      return (void*)0x1; // error code // FIXME: should set persistent toc to null

   tocManagement->setPermanentEntriesAddtionComplete(false);
   tocManagement->setTOCSize(tsize);
   tocManagement->setTOCPtr(tocPtr);
   memset(tocPtr, 0, tsize);

   // Always: base is at the middle of TOC
   uintptr_t *tocBase = (uintptr_t *)(tocPtr + (tsize>>1));
   tocManagement->setTOCBase(tocBase);

   // Initialize the helper function table (0 to TR_PPCnumRuntimeHelpers-2)
   int32_t idx;
   for (idx=1; idx<TR_PPCnumRuntimeHelpers; idx++)
      tocBase[idx-1] = (uintptr_t)runtimeHelperValue((TR_RuntimeHelper)idx);
#if defined(JITTEST)
   //Store at the index of TR_PPCnumRuntimeHelpers, the systemTOC. Note: Slot tocBase[TR_PPCnumRuntimeHelpers-1] is free.
   //See TR::TreeEvaluator::restoreTOCRegister.
   tocBase[TR_PPCnumRuntimeHelpers] = systemTOC;
   //Set Down Cursor to be TR_PPCnumRuntimeHelpers+1, so nothing between [0..TR_PPCnumRuntimeHelpers] should be used for hashtable.
   tocManagement->setDownCursor(TR_PPCnumRuntimeHelpers+1);
#else
   //Java is free to use slots [TR_PPCnumRuntimeHelpers-1.._downLast].
   tocManagement->setDownCursor(TR_PPCnumRuntimeHelpers-1);
#endif

   // Initialize the hash map
   int32_t top = tsize/(sizeof(intptr_t)-1);
   int32_t hashSizeInByte = sizeof(TR_tocHashEntry)*top;
   TR_tocHashEntry *hash = (TR_tocHashEntry *)jitPersistentAlloc(hashSizeInByte);
   if (!hash)
      return (void*)0x1; // error code // FIXME: should set persistent toc to null
   memset(hash, 0, hashSizeInByte);
   tocManagement->setHashTop(top);
   tocManagement->setHashSize(tsize/sizeof(intptr_t) - 111);
   tocManagement->setCollisionCursor(tocManagement->getHashSize());
   tocManagement->setHashMap(hash);

   // Initialize the name area
   int64_t nameSize = tsize/sizeof(intptr_t) * (int64_t)40;
   tocManagement->setNameAStart((int8_t *)jitPersistentAlloc(nameSize));
   tocManagement->setNameACursor(tocManagement->getNameAStart());
   tocManagement->setNameASize(nameSize);

   tocManagement->setLastFloatCursor(PTOC_FULL_INDEX);

   tocManagement->setTOCMonitor(TR::Monitor::create("TOC_Monitor"));
   if (!tocManagement->getTOCMonitor())
      return (void*)0x1; // FIXME: should set persistent toc to null

   return ((void *)tocBase);
   }

void TR_PPCTableOfConstants::reinitializeMemory()
   {
   TR::PersistentInfo *pinfo = TR_PersistentMemory::getNonThreadSafePersistentInfo();
   TR_PPCTableOfConstants *tocManagement = toPPCTableOfConstants(pinfo->getPersistentTOC());

   if (!tocManagement) return;

   // It is not mandatory to clean up this memory, but it may make debugging easier should it be required
   memset(tocManagement->getTOCBase()+tocManagement->getDownCursorAfterPermanentEntries(), 0,
          (tocManagement->getTOCSize()>>1) - tocManagement->getDownCursorAfterPermanentEntries()*sizeof(uintptr_t));
   memset(tocManagement->getTOCPtr(), 0, (tocManagement->getTOCSize()>>1) - (tocManagement->getUpCursorAfterPermanentEntries() + 1 )*sizeof(uintptr_t));

   // Initialize the hash map
   int32_t top = tocManagement->getTOCSize()/(sizeof(intptr_t)-1);
   int32_t hashSizeInByte = sizeof(TR_tocHashEntry)*top;
   memset(tocManagement->getHashMap(), 0, hashSizeInByte);
   tocManagement->setHashTop(top);
   tocManagement->setCollisionCursor(tocManagement->getHashSize());
   tocManagement->setDownCursor(tocManagement->getDownCursorAfterPermanentEntries());
   tocManagement->setUpCursor(tocManagement->getUpCursorAfterPermanentEntries());
   }

void TR_PPCTableOfConstants::shutdown(TR_FrontEnd *fe)
   {
   TR_ASSERT(0, "shutdown only enabled for zEmulator.\n");
   }

static int32_t hashValue(int8_t *key, int32_t len)
   {
   // This is an ELF hashing algorithm shown to have strong distributions
   int32_t value = 0;
   int32_t i;

   for (i=0; i<len; i++)
      {
      int32_t feed;
      value = (value<<4) + key[i];
      feed = value & 0xf0000000;
      if (feed != 0)
	 value ^= feed>>24;
      value &= ~feed;
      }
   return value;
   }

int32_t
TR_PPCTableOfConstants::lookUp(int32_t val, struct TR_tocHashEntry *tmplate, int32_t *offsetInSlot, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (comp->compileRelocatableCode() || comp->getOption(TR_DisableTOC) || (comp->getOption(TR_EnableHCR) && comp->getOption(TR_HCRPatchClassPointers)) || comp->getOption(TR_MimicInterpreterFrameShape))
      return PTOC_FULL_INDEX;

   if (comp->isOptServer())
      {
      if (comp->getMethodHotness() < warm || comp->isDLT() || comp->isProfilingCompilation() || cg->getCurrentBlock()->isCold())
         return PTOC_FULL_INDEX;
      }

   TR::PersistentInfo     *pinfo = TR_PersistentMemory::getNonThreadSafePersistentInfo();
   TR_PPCTableOfConstants *tocManagement = toPPCTableOfConstants(pinfo->getPersistentTOC());
   struct TR_tocHashEntry *hash;
   int32_t                 idx;
   int32_t                 chain, rval, collisionArea, restoreIdx;

   if (tocManagement == NULL)
      return PTOC_FULL_INDEX;

   // Acquire monitor on TOC
   tocManagement->getTOCMonitor()->enter();
   hash = tocManagement->getHashMap();
   idx = tocManagement->getHashSize();

   val ^= (val>>16);
   val &= 0x75A9FFFF;
   idx = (val<idx)?val:(val%idx);
   chain = (hash[idx]._flag == 0)?CHAIN_END:idx;
   collisionArea = 0;
   restoreIdx = CHAIN_END;

   *offsetInSlot = 0;

   while (chain!=CHAIN_END)
      {
      idx = chain;
      rval = hash[idx]._tocIndex;
      switch (hash[idx]._flag & tmplate->_flag)
         {
         case TR_FLAG_tocNameKey:
            if ((strcmp((char *)hash[idx]._nKey, (char *)tmplate->_nKey) == 0) &&
                (hash[idx]._keyTag == tmplate->_keyTag))
               {
               tocManagement->getTOCMonitor()->exit();
               return rval;
               }
            break;

         case TR_FLAG_tocAddrKey:
            if (hash[idx]._addrKey == tmplate->_addrKey)
               {
               tocManagement->getTOCMonitor()->exit();
               return rval;
               }
            break;

         case TR_FLAG_tocDoubleKey:
            if (hash[idx]._dKey == tmplate->_dKey)
               {
               tocManagement->getTOCMonitor()->exit();
               return rval;
               }
            break;

         case TR_FLAG_tocFloatKey:
            if (hash[idx]._fKey == tmplate->_fKey)
               {
               if (hash[idx]._flag & TR_FLAG_tocFloatHigh)
                  *offsetInSlot = 4;
               tocManagement->getTOCMonitor()->exit();
               return rval;
               }
            break;

         case TR_FLAG_tocStatic2ClassKey:
            if (hash[idx]._keyTag == tmplate->_keyTag &&
                hash[idx]._cpKey == tmplate->_cpKey   &&
                hash[idx]._staticCPIndex == tmplate->_staticCPIndex)
               {
               tocManagement->getTOCMonitor()->exit();
               return rval;
               }
            break;
         }
      chain = hash[idx]._collisionChain;
      }

   if (hash[idx]._flag != 0)
      {
      // Get a collsion entry in hash table
      int32_t jdx = tocManagement->getCollisionCursor();

      if (jdx < tocManagement->getHashTop())
         {
         tocManagement->setCollisionCursor(jdx+1);
         collisionArea = 1;
         }
      else
         {
         for (jdx=idx+1; jdx<tocManagement->getHashSize(); jdx++)
            if (hash[jdx]._flag == 0)
               break;
         if (jdx >= tocManagement->getHashSize())
            {
            for (jdx=idx-1; jdx>=0; jdx--)
               if (hash[jdx]._flag == 0)
                  break;

            // Easily extensible: future work.
            if (jdx < 0)
               {
               tocManagement->getTOCMonitor()->exit();
               return PTOC_FULL_INDEX;
               }
            }
         }
      hash[idx]._collisionChain = jdx;
      restoreIdx = idx;
      idx = jdx;
      }

   // New entry in table
   hash[idx]._flag = tmplate->_flag;
   if (tmplate->_flag==TR_FLAG_tocFloatKey)
      {
      rval = tocManagement->getLastFloatCursor();
      if (rval!=PTOC_FULL_INDEX)
         {
         tocManagement->setLastFloatCursor(PTOC_FULL_INDEX);
         hash[idx]._flag |= TR_FLAG_tocFloatHigh;
         *offsetInSlot = 4;
         }
      else
         {
         rval = allocateChunk(1, cg, false /*!grabMonitor*/);
         tocManagement->setLastFloatCursor(rval);
         }
      }
   else
      rval = allocateChunk(1, cg, false /*!grabMonitor*/);

   if (rval == PTOC_FULL_INDEX)
      {
      if (restoreIdx != CHAIN_END)
         hash[restoreIdx]._collisionChain = CHAIN_END;
      if (collisionArea != 0)
         tocManagement->setCollisionCursor(idx);
      hash[idx]._flag = 0;
      tocManagement->getTOCMonitor()->exit();
      return rval;
      }
   hash[idx]._collisionChain = CHAIN_END;
   hash[idx]._tocIndex = rval;

   switch (tmplate->_flag)
      {
      case TR_FLAG_tocNameKey:
         {
         int32_t nlen = strlen((char *)tmplate->_nKey);
         if (tocManagement->getNameACursor()-tocManagement->getNameAStart()+nlen+1 <
             tocManagement->getNameASize())
	    {
            hash[idx]._nKey = tocManagement->getNameACursor();
            tocManagement->setNameACursor(hash[idx]._nKey+nlen+1);
	    }
         else
	    {
	    hash[idx]._nKey = (int8_t *)TR_Memory::jitPersistentAlloc(nlen+1);
	    }
         if (hash[idx]._nKey != NULL)
            strcpy((char *)hash[idx]._nKey, (char *)tmplate->_nKey);
         else
            {
            // Effectively making this entry not shared, since _staicCPIndex is a null string.
            hash[idx]._nKey = (int8_t *)&hash[idx]._staticCPIndex;
            }
         hash[idx]._keyTag = tmplate->_keyTag ;
         break;
         }
      case TR_FLAG_tocAddrKey:
         hash[idx]._addrKey = tmplate->_addrKey;
         break;
      case TR_FLAG_tocDoubleKey:
         hash[idx]._dKey = tmplate->_dKey;
         setTOCSlot(rval*sizeof(intptr_t), hash[idx]._dKey);
         break;
      case TR_FLAG_tocFloatKey:
         hash[idx]._fKey = tmplate->_fKey;
         uintptr_t slotValue;
         if (comp->target().cpu.isBigEndian())
            slotValue = (*offsetInSlot == 0)?(((uintptr_t)hash[idx]._fKey)<<32):(getTOCSlot(rval*sizeof(intptr_t))|hash[idx]._fKey);
         else
            slotValue = (*offsetInSlot == 0)?((uintptr_t)hash[idx]._fKey):(getTOCSlot(rval*sizeof(intptr_t))|(((uintptr_t)hash[idx]._fKey)<<32));
         setTOCSlot(rval*sizeof(intptr_t), slotValue);
         break;
      case TR_FLAG_tocStatic2ClassKey:
         hash[idx]._keyTag = tmplate->_keyTag;
         hash[idx]._cpKey = tmplate->_cpKey;
         hash[idx]._staticCPIndex = tmplate->_staticCPIndex;
         break;
      }
   tocManagement->getTOCMonitor()->exit();
   return rval;
   }

int32_t TR_PPCTableOfConstants::lookUp(int8_t *name, int32_t len, bool isAddr, intptr_t keyTag, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   struct TR_tocHashEntry localEntry;
   int32_t                val, offsetInSlot;

   if (comp->compileRelocatableCode() || comp->getOption(TR_DisableTOC) || (comp->getOption(TR_EnableHCR) && comp->getOption(TR_HCRPatchClassPointers)))
      return PTOC_FULL_INDEX;

   if (comp->isOptServer())
      {
      if (comp->getMethodHotness() < warm || comp->isDLT() || comp->isProfilingCompilation() || cg->getCurrentBlock()->isCold())
         return PTOC_FULL_INDEX;
      }

   if (isAddr)
      {
      int8_t seed[12] = {'e', 'X', 't', 'R', 'e', 'M', 'e', 'S', '\0', '\0', '\0', '\0'};
      *(int64_t *)&seed[4] ^= *(intptr_t *)name;
      val = hashValue(seed, 12);
      localEntry._flag = TR_FLAG_tocAddrKey;
      localEntry._addrKey = *(intptr_t *)name;
      }
   else
      {
      // For name entries, we will also use the loader/anonClass as key2, in addition to the name
      val = hashValue(name, len);
      localEntry._flag = TR_FLAG_tocNameKey;
      localEntry._nKey = name;
      localEntry._keyTag = keyTag ;
      }

   return lookUp(val, &localEntry, &offsetInSlot, cg);
   }

int32_t TR_PPCTableOfConstants::lookUp(double dvalue, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_DisableTOC))
      return PTOC_FULL_INDEX;

   if (comp->isOptServer())
      {
      if (comp->getMethodHotness() < warm || comp->isDLT() || comp->isProfilingCompilation() || cg->getCurrentBlock()->isCold())
         return PTOC_FULL_INDEX;
      }

   struct TR_tocHashEntry localEntry;
   int32_t                offsetInSlot, val, tindex;
   int8_t                 seed[8] = {'U', 'p', 'E', 'd', 'G', 'a', 'M', 'e'};

   *(int64_t *)seed ^= *(int64_t *)&dvalue;
   val = hashValue(seed, 8);
   localEntry._flag = TR_FLAG_tocDoubleKey;
   localEntry._dKey = *(uint64_t *)&dvalue;
   tindex = lookUp(val, &localEntry, &offsetInSlot, cg);
   if (tindex == PTOC_FULL_INDEX)
      return tindex;

   // Return offset
   return tindex*sizeof(intptr_t);
   }

int32_t TR_PPCTableOfConstants::lookUp(float fvalue, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_DisableTOC))
      return PTOC_FULL_INDEX;

   if (comp->isOptServer())
      {
      if (comp->getMethodHotness() < warm || comp->isDLT() || comp->isProfilingCompilation() || cg->getCurrentBlock()->isCold())
         return PTOC_FULL_INDEX;
      }

   struct TR_tocHashEntry localEntry;
   intptr_t              entryVal = 0;
   int32_t                offsetInSlot, val, tindex;
   int8_t                 seed[8] = {'d', 'O', 'w', 'N', 'k', 'I', 'c', 'K'};

   *(int64_t *)seed ^= *(int32_t *)&fvalue;
   val = hashValue(seed, 8);
   localEntry._flag = TR_FLAG_tocFloatKey;
   localEntry._fKey = *(uint32_t *)&fvalue;
   tindex = lookUp(val, &localEntry, &offsetInSlot, cg);
   if (tindex == PTOC_FULL_INDEX)
      return tindex;

   // Return offset
   tindex *= sizeof(intptr_t);
   return tindex+offsetInSlot;
   }

int32_t TR_PPCTableOfConstants::lookUp(TR::SymbolReference *symRef, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_DisableTOC))
      return PTOC_FULL_INDEX;

   if (comp->isOptServer())
      {
      if (comp->getMethodHotness() < warm || comp->isDLT() || comp->isProfilingCompilation() || cg->getCurrentBlock()->isCold())
         return PTOC_FULL_INDEX;
      }

   TR::StaticSymbol *sym = symRef->getSymbol()->castToStaticSymbol();
   intptr_t        addr = (intptr_t)sym->getStaticAddress();
   int32_t          nlen, tindex;
   int8_t           local_buffer[1024];
   int8_t          *name = local_buffer;
   bool             isAddr = false;
   intptr_t        myTag;

   if (!symRef->isUnresolved() || symRef->getCPIndex()<0 || sym->isAddressOfClassObject() || sym->isConstObjectRef() || sym->isConst())
      {
      if (sym->isConstObjectRef() && symRef->isUnresolved())
         addr = symRef->getOffset();
      *(intptr_t *)local_buffer = addr;
      nlen = sizeof(intptr_t);
      local_buffer[nlen] = 0;
      isAddr = true;
      }
   else
      {
      TR_OpaqueClassBlock *myClass = comp->getOwningMethodSymbol(symRef->getOwningMethodIndex())->getResolvedMethod()->containingClass();

      if (TR::Compiler->cls.isAnonymousClass(comp, myClass))
         myTag = (intptr_t)myClass;
      else
         {
#ifdef J9_PROJECT_SPECIFIC
         myTag = (intptr_t)cg->fej9()->getClassLoader(myClass) ;
#endif
         }

      if (sym->isClassObject())
         {
         int8_t *className;
         if (sym->addressIsCPIndexOfStatic())
            {
            struct TR_tocHashEntry st2cEntry;
            int32_t st2cVal, st2cOffsetInSlot;

            st2cEntry._flag = TR_FLAG_tocStatic2ClassKey;
            st2cEntry._keyTag = myTag;
            st2cEntry._cpKey = (intptr_t)symRef->getOwningMethod(comp)->constantPool();
            st2cEntry._staticCPIndex = symRef->getCPIndex();

            st2cVal = st2cEntry._cpKey * st2cEntry._staticCPIndex;
            return lookUp(st2cVal, &st2cEntry, &st2cOffsetInSlot, cg);
            }
         else
            {
            className = (int8_t *)TR::Compiler->cls.classNameChars(comp, symRef, nlen);
            }

         TR_ASSERT(className!=NULL, "Class object name is expected");

         if (nlen >= 1024)
            {
            name = (int8_t *)cg->trMemory()->allocateHeapMemory(nlen+1);
            }
         strncpy((char *)name, (char *)className, nlen);
         name[nlen] = 0;
         }
      else
         {
         name = (int8_t *)comp->getOwningMethodSymbol(symRef->getOwningMethodIndex())->getResolvedMethod()->staticName(symRef->getCPIndex(), cg->trMemory());
         TR_ASSERT(name!=NULL, "Variable name is expected");
         nlen = strlen((char *)name);
         }
      }

   return lookUp(name, nlen, isAddr, myTag, cg);
   }

int32_t TR_PPCTableOfConstants::allocateChunk(uint32_t numEntries, TR::CodeGenerator *cg, bool grabMonitor)
   {
   TR_PPCTableOfConstants *tocManagement = toPPCTableOfConstants(TR_PersistentMemory::getNonThreadSafePersistentInfo()->getPersistentTOC());

   if (tocManagement == NULL || cg->comp()->getOption(TR_DisableTOC) || cg->comp()->compileRelocatableCode() || (cg->comp()->getOption(TR_EnableHCR) && cg->comp()->getOption(TR_HCRPatchClassPointers)))
      return PTOC_FULL_INDEX;

   if (grabMonitor)
      tocManagement->getTOCMonitor()->enter();

   int32_t result = PTOC_FULL_INDEX;
   int32_t ret = tocManagement->getDownCursor();
   if (ret+numEntries<=tocManagement->getDownLast())
      {
      tocManagement->setDownCursor(ret+numEntries);

      result = ret;
      goto Lexit;
      }

   ret = tocManagement->getUpCursor() - numEntries;
   if (ret >= tocManagement->getUpLast())
      {
      tocManagement->setUpCursor(ret);

      result = ret + 1;
      goto Lexit;
      }

   // Should post an event, fail this compilation, and relocate the TOC --- future work
   result =  PTOC_FULL_INDEX;
   /* fall through */

Lexit:
   if (grabMonitor)
      tocManagement->getTOCMonitor()->exit();
   return result;
   }

void
TR_PPCTableOfConstants::onClassUnloading(void *tagPtr)
   {
   TR::PersistentInfo *pinfo = TR_PersistentMemory::getNonThreadSafePersistentInfo();
   TR_PPCTableOfConstants *ptoc = toPPCTableOfConstants(pinfo->getPersistentTOC());
   struct TR_tocHashEntry *hash;
   int32_t                 idx, endIx;

   if (ptoc == NULL)
      return;
   hash = ptoc->getHashMap();
   endIx = ptoc->getCollisionCursor();

   // hashEntry and toc slot are not recycled currently
   for (idx=0; idx<endIx; idx++)
      {
      if ((hash[idx]._flag & (TR_FLAG_tocNameKey|TR_FLAG_tocStatic2ClassKey)) && hash[idx]._keyTag==(intptr_t)tagPtr)
         {
         hash[idx]._keyTag = -1;
         setTOCSlot(hash[idx]._tocIndex * sizeof(uintptr_t), -1);
         }
      }
   }

uintptr_t TR_PPCTableOfConstants::getTOCSlot(int32_t offset)
   {
   TR_PPCTableOfConstants *tocManagement = toPPCTableOfConstants(TR_PersistentMemory::getNonThreadSafePersistentInfo()->getPersistentTOC());
   uintptr_t *base = tocManagement->getTOCBase(); // no lock needed because base does not change

   return base[offset/sizeof(uintptr_t)];
   }

void TR_PPCTableOfConstants::setTOCSlot(int32_t offset, uintptr_t v)
   {
   TR_PPCTableOfConstants *tocManagement = toPPCTableOfConstants(TR_PersistentMemory::getNonThreadSafePersistentInfo()->getPersistentTOC());
   uintptr_t *base = tocManagement->getTOCBase();

   base[offset/sizeof(uintptr_t)] = v; // no lock needed
   }
void TR_PPCTableOfConstants::permanentEntriesAddtionComplete() // only zEmulator uses this
  {
  TR_PPCTableOfConstants *tocManagement = toPPCTableOfConstants(TR_PersistentMemory::getNonThreadSafePersistentInfo()->getPersistentTOC());
  if (!tocManagement) return;
  tocManagement->getTOCMonitor()->enter();
  tocManagement->setUpCursorAfterPermanentEntries(tocManagement->getUpCursor());
  tocManagement->setDownCursorAfterPermanentEntries(tocManagement->getDownCursor());
  tocManagement->setPermanentEntriesAddtionComplete(true);
  tocManagement->getTOCMonitor()->exit();
  }
bool TR_PPCTableOfConstants::isPermanentEntriesAddtionComplete() // only zEmulator uses this
  {
  bool retValue;
  TR_PPCTableOfConstants *tocManagement = toPPCTableOfConstants(TR_PersistentMemory::getNonThreadSafePersistentInfo()->getPersistentTOC());
  if (!tocManagement) return true;
  tocManagement->getTOCMonitor()->enter();
  retValue = tocManagement->getPermanentEntriesAddtionComplete();
  tocManagement->getTOCMonitor()->exit();
  return retValue;
  }
