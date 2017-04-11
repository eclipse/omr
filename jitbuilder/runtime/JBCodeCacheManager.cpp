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
 *******************************************************************************/

#include <sys/mman.h>
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "env/FrontEnd.hpp"


// Allocate and initialize a new code cache
// If reservingCompThreadID >= -1, then the new code codecache will be reserved
// A value of -1 for this parameter means that an application thread is requesting the reservation
// A positive value means that a compilation thread is requesting the reservation
// A value of -2 (or less) means that no reservation is requested


TR::CodeCacheManager *JitBuilder::CodeCacheManager::_codeCacheManager = NULL;
JitBuilder::JitConfig *JitBuilder::CodeCacheManager::_jitConfig = NULL;


TR::CodeCacheManager *
JitBuilder::CodeCacheManager::self()
   {
   return static_cast<TR::CodeCacheManager *>(this);
   }


TR::CodeCache *
JitBuilder::CodeCacheManager::initialize(bool useConsolidatedCache, uint32_t numberOfCodeCachesToCreateAtStartup)
   {
   _jitConfig = pyfe()->jitConfig();
   //_allocator = TR::globalAllocator("CodeCache");
   return this->OMR::CodeCacheManager::initialize(useConsolidatedCache, numberOfCodeCachesToCreateAtStartup);
   }

void *
JitBuilder::CodeCacheManager::getMemory(size_t sizeInBytes)
   {
   void * ptr = malloc(sizeInBytes);
   //fprintf(stderr,"JitBuilder::CodeCacheManager::getMemory(%d) allocated %p\n", sizeInBytes, ptr);

   return ptr;
   //return _allocator.allocate(sizeInBytes);
   }

void
JitBuilder::CodeCacheManager::freeMemory(void *memoryToFree)
   {
   //fprintf(stderr,"JitBuilder::CodeCacheManager::free(%p)\n", memoryToFree);
   free(memoryToFree);
   //return _allocator.deallocate(memoryToFree, 0);
   }

TR::CodeCacheMemorySegment *
JitBuilder::CodeCacheManager::allocateCodeCacheSegment(size_t segmentSize,
                                              size_t &codeCacheSizeToAllocate,
                                              void *preferredStartAddress)
   {
   // We should really rely on the port library to allocate memory, but this connection
   // has not yet been made, so as a quick workaround for platforms like OS X <= 10.9
   // where MAP_ANONYMOUS is not defined, is to map MAP_ANON to MAP_ANONYMOUS ourselves
   #if !defined(MAP_ANONYMOUS)
      #define NO_MAP_ANONYMOUS
      #if defined(MAP_ANON)
         #define MAP_ANONYMOUS MAP_ANON
      #else
         #error unexpectedly, no MAP_ANONYMOUS or MAP_ANON definition
      #endif
   #endif

   // ignore preferredStartAddress for now, since it's NULL anyway
   //   goal would be to allocate code cache segments near the JIT library address
   codeCacheSizeToAllocate = segmentSize;
   TR::CodeCacheConfig & config = codeCacheConfig();
   if (segmentSize < config.codeCachePadKB() << 10)
      codeCacheSizeToAllocate = config.codeCachePadKB() << 10;
   uint8_t *memorySlab = (uint8_t *) mmap(NULL,
                                          codeCacheSizeToAllocate,
                                          PROT_READ | PROT_WRITE | PROT_EXEC,
                                          MAP_ANONYMOUS | MAP_PRIVATE,
                                          0,
                                          0);

   // keep the impact of this fix localized
   #if defined(NO_MAP_ANONYMOUS)
      #undef MAP_ANONYMOUS
      #undef NO_MAP_ANONYMOUS
   #endif
   TR::CodeCacheMemorySegment *memSegment = (TR::CodeCacheMemorySegment *) ((size_t)memorySlab + codeCacheSizeToAllocate - sizeof(TR::CodeCacheMemorySegment));
   new (memSegment) TR::CodeCacheMemorySegment(memorySlab, reinterpret_cast<uint8_t *>(memSegment));
   return memSegment;
   }
