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

#include <stdint.h>
#include <string.h>

extern "C" void *getCodeSync();

#if defined(TR_HOST_POWER)
#if defined(__IBMC__) || defined(__IBMCPP__) || defined(__open_xl__)

#if defined(__cplusplus)
#include <builtins.h>
#endif /* __cplusplus */

#define dcbf(x) __dcbf(x)
#define sync() __sync()
#define isync() __isync()

#else /* defined(__IBMC__) || defined(__IBMCPP__) || defined(__open_xl__) */

static inline void dcbf(unsigned char *addr)
   {
   __asm__(
      "dcbf 0,%0"
      : /* no outputs */
      : "r" (addr) );
   }

static inline void sync()
   {
   __asm__("sync");
   }

static inline void isync()
   {
   __asm__("isync");
   }

#endif /* defined(__IBMC__) || defined(__IBMCPP__) || defined(__open_xl__) */

static inline void icbi(unsigned char *addr)
   {
   __asm__(
      "icbi 0,%0"
      : /* no outputs */
      : "r" (addr) );
   }

#endif /* TR_HOST_POWER */

uint32_t getPPCCacheLineSize();

#if defined(TR_HOST_POWER)
extern "C" void jitPPCDataCacheBlockZero(void *);
#endif

//Returns the L1 cache line size on PPC, which is computed dynamically for the JIT.
uint32_t getPPCCacheLineSize()
   {
#if defined(TR_HOST_POWER)
   static int32_t ppcCacheLineSize = -1;

   if (ppcCacheLineSize < 0)
      {
      int32_t  count;
      int32_t  i;
      char buf[1024];

      memset(buf, 255, 1024);
      jitPPCDataCacheBlockZero((void *) &buf[512]);

      for (i = 0, count = 0; i < 1024; i++)
         if (buf[i] == 0)
            count++;
      ppcCacheLineSize = count;
      }

   return (uint32_t) ppcCacheLineSize;
#else
return 32; //For AOT compile, a constant default is returned.
#endif
}

// make sure the code gets from the data cache to the instruction cache
void ppcCodeSync(unsigned char *codeStart, unsigned int codeSize)
   {
#if defined(TR_HOST_POWER)
	uint32_t       cacheLineSize = getPPCCacheLineSize();
	unsigned char  *addr;
	unsigned char  *limit;

    limit = (unsigned char *)(((uintptr_t)codeStart + codeSize + (cacheLineSize - 1))
						/ cacheLineSize * cacheLineSize);

	// for each cache line, do a data cache block flush
	for (addr = codeStart; addr < limit; addr += cacheLineSize)
		dcbf(addr);

	sync();

	// for each cache line, do an icache block invalidate
	for (addr = codeStart; addr < limit; addr += cacheLineSize)
		icbi(addr);

	sync();
	isync();
#endif /* TR_HOST_POWER */
   }

void *getCodeSync()
   {
   return (void *) ppcCodeSync;
   }
