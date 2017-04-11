/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#include <sys/mman.h>
#include "compile/Compilation.hpp"
#include "env/FEBase.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCacheExceptions.hpp"

namespace TR
{

template <class Derived>
void
FEBase<Derived>::unreserveCodeCache(TR::CodeCache *codeCache)
   {
   codeCacheManager().unreserveCodeCache(codeCache);
   }

template <class Derived>
TR::CodeCache *
FEBase<Derived>::getDesignatedCodeCache(TR::Compilation *)
   {
   int32_t numReserved = 0;
   int32_t compThreadID = 0;
   return codeCacheManager().reserveCodeCache(false, 0, compThreadID, &numReserved);
   }


template <class Derived>
uint8_t *
FEBase<Derived>::allocateCodeMemory(TR::Compilation *comp, uint32_t warmCodeSize, uint32_t coldCodeSize,
                            uint8_t **coldCode, bool isMethodHeaderNeeded)
   {
   TR::CodeCache *codeCache = static_cast<TR::CodeCache *>(comp->getCurrentCodeCache());

   TR_ASSERT(codeCache->isReserved(), "Code cache should have been reserved.");

   uint8_t *warmCode = codeCacheManager().allocateCodeMemory(warmCodeSize, coldCodeSize, &codeCache,
                                                             coldCode, false, isMethodHeaderNeeded);

   if (codeCache != comp->getCurrentCodeCache())
      {
      // Either we didn't get a code cache, or the one we get should be reserved
      TR_ASSERT(!codeCache || codeCache->isReserved(), "Substitute code cache isn't marked as reserved");
      comp->setAotMethodCodeStart(warmCode);
      switchCodeCache(codeCache);
      }

   if (warmCode == NULL)
      {
      if (jitConfig()->isCodeCacheFull())
         {
         comp->failCompilation<TR::CodeCacheError>("Code Cache Full");
         }
      else
         {
         comp->failCompilation<TR::RecoverableCodeCacheError>("Failed to allocate code memory");
         }
      }

   TR_ASSERT( !((warmCodeSize && !warmCode) || (coldCodeSize && !coldCode)), "Allocation failed but didn't throw an exception");

   codeCacheManager().registerCompiledMethod(TR::comp()->signature(), warmCode, warmCodeSize);
   return warmCode;
   }

// This code does not really belong here (along with allocateRelocationData, really)
// We should be relying on the port library to allocate memory, but this connection
// has not yet been made, so as a quick workaround for platforms like OS X <= 10.9,
// where MAP_ANONYMOUS is not defined, is to map MAP_ANON to MAP_ANONYMOUS ourselves
#if !defined(MAP_ANONYMOUS)
  #define NO_MAP_ANONYMOUS
  #if defined(MAP_ANON)
    #define MAP_ANONYMOUS MAP_ANON
  #else
    #error unexpectedly, no MAP_ANONYMOUS or MAP_ANON definition
  #endif
#endif

template <class Derived>
uint8_t *
FEBase<Derived>::allocateRelocationData(TR::Compilation* comp, uint32_t size)
   {
   /* FIXME: using an mmap without much thought into whether that is the best
      way to allocate this */
   if (size == 0) return 0;
   TR_ASSERT(size >= 2048, "allocateRelocationData should be used for whole-sale memory allocation only");
   return (uint8_t *) mmap(0,
                           size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1,
                           0);
   }

// keep the impact of this fix localized
#if defined(NO_MAP_ANONYMOUS)
  #undef MAP_ANONYMOUS
  #undef NO_MAP_ANONYMOUS
#endif

template <class Derived>
void
FEBase<Derived>::switchCodeCache(TR::CodeCache *newCache)
   {
   TR::Compilation *comp = TR::comp();
   TR::CodeCache *oldCache = comp->getCurrentCodeCache();

   comp->switchCodeCache(newCache);

   // If the old CC had pre-loaded code, the current compilation may have initialized it and will therefore depend on it
   // so we should initialize it in the new CC as well
   // XXX: We could avoid this if we knew for sure that this compile wasn't the one who initialized it
   if (newCache && oldCache->isCCPreLoadedCodeInitialized())
      {
      TR::CodeCache *newCC = newCache;
      newCC->getCCPreLoadedCodeAddress(TR_numCCPreLoadedCode, comp->cg());
      }
   }

template <class Derived>
intptrj_t
FEBase<Derived>::indexedTrampolineLookup(int32_t helperIndex, void * callSite)
   {
   void * tramp = codeCacheManager().findHelperTrampoline(callSite, helperIndex);
   TR_ASSERT(tramp!=NULL, "Error: CodeCache is not initialized properly.\n");
   return (intptrj_t)tramp;
   }

template <class Derived>
void
FEBase<Derived>::resizeCodeMemory(TR::Compilation * comp, uint8_t *bufferStart, uint32_t numBytes)
   {
   // I don't see a reason to acquire VM access for this call
   TR::CodeCache *codeCache = comp->getCurrentCodeCache();
   codeCache->resizeCodeMemory(bufferStart, numBytes);
   }


} /* namespace TR */
