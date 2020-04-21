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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/Processors.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/Runtime.hpp"
#include "env/CompilerEnv.hpp"

#if defined(JITTEST)
#include "env/ConcreteFE.hpp"
#endif

namespace TR { class CodeGenerator; }

namespace TR {

void createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void ** CCPreLoadedCodeTable, TR::CodeGenerator *cg);
uint32_t getCCPreLoadedCodeSize();

}

#define PPC_INSTRUCTION_LENGTH 4

#if defined(TR_TARGET_POWER)

#if defined(TR_TARGET_64BIT)
#define TRAMPOLINE_SIZE       28
#else
#define TRAMPOLINE_SIZE       20
#endif

#define BRANCH_FORWARD_LIMIT      (0x01fffffc)
#define BRANCH_BACKWARD_LIMIT     ((int32_t)0xfe000000)

#if defined(TR_HOST_POWER) || defined(TR_TARGET_POWER)
extern void     ppcCodeSync(uint8_t *, uint32_t);
#endif

void
ppcCodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // Estimated: 2KB per method, with 10% being recompiled(multi-times)

   *numTempTrampolines = ccSizeInByte>>12;
   }

extern void registerTrampoline(uint8_t *start, uint32_t size, const char *name);

void
ppcCreateHelperTrampolines(uint8_t *trampPtr, int32_t numHelpers)
   {
   TR::CodeCacheManager &manager = OMR::FrontEnd::singleton().codeCacheManager();
   TR::CodeCacheConfig &config = manager.codeCacheConfig();
   char name[256];

   static TR_Processor proc = TR_DefaultPPCProcessor;

   uint8_t *bufferStart = trampPtr, *buffer;
   for (int32_t cookie=1; cookie<numHelpers; cookie++)
      {
      intptr_t helper = (intptr_t)runtimeHelperValue((TR_RuntimeHelper)cookie);
      // Skip over the first one for index 0
      bufferStart += config.trampolineCodeSize();
      buffer = bufferStart;

#if defined(__LITTLE_ENDIAN__)

      // mtctr r12
      *(int32_t *)buffer = 0x7d8903a6;
      buffer += 4;

#else //!defined(__LITTLE_ENDIAN__)
#if defined(TR_TARGET_64BIT)
      // ld gr11, [gr2, 8*(cookie-1)]
      *(int32_t *)buffer = 0xe9620000 | (((cookie-1)*sizeof(intptr_t)) & 0x0000ffff);
      buffer += 4;
#else //!defined(TR_TARGET_64BIT)
      // For POWER4 which has a problem with the CTR/LR cache when the upper
      // bits are not 0 extended.. Use li/oris when the 16th bit is off
      if( !(helper & 0x00008000) )
         {
         // li r11, lower
         *(int32_t *)buffer = 0x39600000 | (helper & 0x0000ffff);
         buffer += 4;
         // oris r11, r11, upper
         *(int32_t *)buffer = 0x656b0000 | ((helper>>16) & 0x0000ffff);
         buffer += 4;
         }
      else
         {
         // lis r11, upper
         *(int32_t *)buffer = 0x3d600000 | (((helper>>16) + (helper&(1<<15)?1:0)) & 0x0000ffff);
         buffer += 4;

         // addi r11, r11, lower
         *(int32_t *)buffer = 0x396b0000 | (helper & 0x0000ffff);
         buffer += 4;

         // Now, if highest bit is on we need to clear the sign extend bits on 64bit CPUs
         // ** POWER4 pref fix **
         if( helper & 0x80000000 )
            {
            // rlwinm r11,r11,sh=0,mb=0,me=31
            *(int32_t *)buffer = 0x556b003e;
            buffer += 4;
            }
         }

#endif //defined(TR_TARGET_64BIT)

      //ld gr2, [gr2, systemTOCOffset]
      *(int32_t *)buffer = 0xe8420000 | ((numHelpers*sizeof(intptr_t)) & 0x0000ffff);
      buffer += 4;

      // mtctr r11
      *(int32_t *)buffer = 0x7d6903a6;
      buffer += 4;

#endif //defined(__LITTLE_ENDIAN__)

      // bctr
      *(int32_t *)buffer = 0x4e800420;
      buffer += 4;

      const char *helperName = TR::Options::findOrCreateDebug()->getRuntimeHelperName((TR_RuntimeHelper)cookie);
      if (strlen(helperName) < 256)
         sprintf(name, "%s (trampoline)", helperName);
      else
         sprintf(name, "unknown helper (trampoline)", helperName);

      manager.registerCompiledMethod(name, bufferStart, buffer - bufferStart);
      registerTrampoline(bufferStart, buffer - bufferStart, name);
      }

   ppcCodeSync(trampPtr, config.trampolineCodeSize() * numHelpers);

   }

void ppcCodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
#if defined(TR_TARGET_64BIT)
   *trampolineSize = TRAMPOLINE_SIZE;
#else
   *trampolineSize = TRAMPOLINE_SIZE + 4;
#endif
   callBacks[0] = (void *)&ppcCodeCacheConfig;
   callBacks[1] = (void *)&ppcCreateHelperTrampolines;
   callBacks[2] = (void *)NULL;
   callBacks[3] = (void *)NULL;
   callBacks[4] = (void *)TR::createCCPreLoadedCode;
   *CCPreLoadedCodeSize = TR::getCCPreLoadedCodeSize();
   *numHelpers = TR_PPCnumRuntimeHelpers;
   }

#undef TRAMPOLINE_SIZE
#undef BRANCH_FORWARD_LIMIT
#undef BRANCH_BACKWARD_LIMIT

#endif /*(TR_TARGET_POWER)*/

#if defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)

// Hack markers
//
// TODO:AMD64: We should obtain the actual boundary, but 8 is conservatively safe
//
#define INSTRUCTION_PATCH_ALIGNMENT_BOUNDARY (8)

#if defined(TR_TARGET_64BIT)
/*FIXME this IS_32BIT_RIP is already define in different places; should be moved to a header file*/
#define IS_32BIT_RIP(x,rip)  ((intptr_t)(x) == (intptr_t)(rip) + (int32_t)((intptr_t)(x) - (intptr_t)(rip)))
#define TRAMPOLINE_SIZE    16
#define CALL_INSTR_LENGTH  5

void amd64CodeCacheConfig(int32_t ccSizeInByte, uint32_t *numTempTrampolines)
   {
   // AMD64 method trampoline can be modified in place.
   *numTempTrampolines = 0;
   }

void amd64CreateHelperTrampolines(uint8_t *trampPtr, int32_t numHelpers)
   {
   uint8_t *bufferStart = trampPtr, *buffer;

   for (int32_t i=1; i<numHelpers; i++)
      {
      intptr_t helperAddr = (intptr_t)runtimeHelperValue((TR_RuntimeHelper)i);

      // Skip the first trampoline for index 0
      bufferStart += TRAMPOLINE_SIZE;
      buffer = bufferStart;

      // JMP [RIP]
      // DQ  helperAddr
      // 2-byte padding
      //
      *(uint16_t *)buffer = 0x25ff;
      buffer += 2;
      *(uint32_t *)buffer = 0x00000000;
      buffer += 4;
      *(int64_t *)buffer = helperAddr;
      buffer += 8;
      *(uint16_t *)buffer = 0x9090;
      }
   }


void amd64CodeCacheParameters(int32_t *trampolineSize, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = TRAMPOLINE_SIZE;
   callBacks->codeCacheConfig = &amd64CodeCacheConfig;
   callBacks->createHelperTrampolines = &amd64CreateHelperTrampolines;
   callBacks->createMethodTrampoline = NULL;
   callBacks->patchTrampoline = NULL;
   callBacks->createCCPreLoadedCode = TR::createCCPreLoadedCode;
   *CCPreLoadedCodeSize = TR::getCCPreLoadedCodeSize();
   *numHelpers = TR_AMD64numRuntimeHelpers;
   }


#endif

#undef TRAMPOLINE_SIZE

#endif /*(TR_TARGET_X86) && (TR_TARGET_64BIT)*/


#if defined(TR_TARGET_X86) && defined(TR_TARGET_32BIT)

void ia32CodeCacheParameters(int32_t *trampolineSize, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 0;
   callBacks->codeCacheConfig = NULL;
   callBacks->createHelperTrampolines = NULL;
   callBacks->createMethodTrampoline = NULL;
   callBacks->patchTrampoline = NULL;
   callBacks->createCCPreLoadedCode = TR::createCCPreLoadedCode;
   *CCPreLoadedCodeSize = TR::getCCPreLoadedCodeSize();
   *numHelpers = 0;
   }

#endif /*(TR_TARGET_X86) && (TR_TARGET_32BIT)*/


#if defined(TR_TARGET_ARM)

#define TRAMPOLINE_SIZE         8

/*copied here from ARMMachine.hpp*/
#define BRANCH_FORWARD_LIMIT    0x01fffffc
#define BRANCH_BACKWARD_LIMIT   0xfe000000

#if defined(TR_HOST_ARM)
extern void armCodeSync(uint8_t *, uint32_t);
#endif /* TR_HOST_ARM */

void armCodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // ARM method trampoline can be modified in place.
   *numTempTrampolines = 0;
   }

void armCreateHelperTrampolines(void *trampPtr, int32_t numHelpers)
   {
   uint32_t *buffer = (uint32_t *)((uint8_t *)trampPtr + TRAMPOLINE_SIZE);  // Skip the first trampoline for index 0

   for (int32_t i=1; i<numHelpers; i++)
      {
      // LDR  PC, [PC, #-4]
      // DCD  helperAddr
      //
      *buffer = 0xe51ff004;
      buffer += 1;
      *buffer = (intptr_t)runtimeHelperValue((TR_RuntimeHelper)i);
      buffer += 1;
      }

#if defined(TR_HOST_ARM)
   armCodeSync((uint8_t*)trampPtr, TRAMPOLINE_SIZE * numHelpers);
#endif

// TODO: X-compilation support and tprof support
   }

void armCodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = TRAMPOLINE_SIZE;
   callBacks[0] = (void *)&armCodeCacheConfig;
   callBacks[1] = (void *)&armCreateHelperTrampolines;
   callBacks[2] = (void *)NULL;
   callBacks[3] = (void *)NULL;
   callBacks[4] = (void *)0;
   *CCPreLoadedCodeSize = 0;
   *numHelpers = TR_ARMnumRuntimeHelpers;
   }

#undef TRAMPOLINE_SIZE
#undef BRANCH_FORWARD_LIMIT
#undef BRANCH_BACKWARD_LIMIT

#endif /* TR_TARGET_ARM */

#if defined(TR_TARGET_ARM64)

#define TRAMPOLINE_SIZE         8

/*copied here from Runtime.hpp*/
// TODO:ARM64
#define BRANCH_FORWARD_LIMIT      (0x01fffffc)
#define BRANCH_BACKWARD_LIMIT     ((int32_t)0xfe000000)


void arm64CodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // ARM64 method trampoline can be modified in place.
   *numTempTrampolines = 0;
   }

void arm64CreateHelperTrampolines(void *trampPtr, int32_t numHelpers)
   {
   uint32_t *buffer = (uint32_t *)((uint8_t *)trampPtr + TRAMPOLINE_SIZE);  // Skip the first trampoline for index 0

   for (int32_t i=1; i<numHelpers; i++)
      {
      *((int32_t *)buffer) = 0x58000050; //LDR R16 PC+8
      buffer += 1;
      *buffer = 0xD61F0200; //BR R16
      buffer += 1;
      *((intptr_t *)buffer) = (intptr_t)runtimeHelperValue((TR_RuntimeHelper)i);
      buffer += 2;
      }
   }

void arm64CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = TRAMPOLINE_SIZE;
   callBacks[0] = (void *)&arm64CodeCacheConfig;
   callBacks[1] = (void *)&arm64CreateHelperTrampolines;
   callBacks[2] = (void *)NULL;
   callBacks[3] = (void *)NULL;
   callBacks[4] = (void *)0;
   *CCPreLoadedCodeSize = 0;
   *numHelpers = TR_ARM64numRuntimeHelpers;
   }

#undef TRAMPOLINE_SIZE
#undef BRANCH_FORWARD_LIMIT
#undef BRANCH_BACKWARD_LIMIT

#endif /* TR_TARGET_ARM64 */


#if defined(J9ZOS390) && defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT)

//-----------------------------------------------------------------------------
//   zOS 31 Bit - "plain" Multi Code Cache Support
//-----------------------------------------------------------------------------

// ZOS390 MCC Support: minimum hookup without callbacks.
void s390zOS31CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 0;
   callBacks[0] = NULL;
   callBacks[1] = NULL;
   callBacks[2] = NULL;
   callBacks[3] = NULL;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = 0;
   }

#elif defined(J9ZOS390) && defined(TR_TARGET_64BIT) && defined(TR_TARGET_S390)

//-----------------------------------------------------------------------------
//   zOS 64 Bit - "plain" Multi Code Cache Support
//-----------------------------------------------------------------------------

// ZOS390 MCC Support: minimum hookup without callbacks.
// Executable space is limited to 2GB: No Trampolinoes required.
void s390zOS64CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 0;
   callBacks[0] = NULL;
   callBacks[1] = NULL;
   callBacks[2] = NULL;
   callBacks[3] = NULL;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = 0;
   }


#elif !defined(J9ZOS390) && defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT)

//-----------------------------------------------------------------------------
//   zLinux 31 Bit - "plain" Multi Code Cache Support
//-----------------------------------------------------------------------------

// ZLinux390 MCC Support: minimum hookup without callbacks.
void s390zLinux31CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 0;
   callBacks[0] = NULL;
   callBacks[1] = NULL;
   callBacks[2] = NULL;
   callBacks[3] = NULL;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = 0;
   }

#elif defined(TR_TARGET_S390) && defined(TR_TARGET_64BIT) && !defined(J9ZOS390)

// Size of a Trampoline (Padded to be 8-byte aligned)
#define TRAMPOLINE_SIZE 24
#define CALL_INSTR_LENGTH 0

// Atomic Storage of a 4 byte value - Picbuilder.m4
extern "C" void _Store4(int32_t * addr, uint32_t newData);
extern "C" void _Store8(intptr_t * addr, uintptr_t newData);

// zLinux64 Configuration of Code Cache.
void s390zLinux64CodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // The method trampolines used by zLinux can be modified in place.
   // Only modification of the data constant after the BCR instruction is required.
   // This DC is guaranteed to be aligned for atomic ST.
   *numTempTrampolines = 0;
   }

// zLinux64 Create Trampolines to Runtime Helper Routines.
void s390zLinux64CreateHelperTrampoline(void *trampPtr, int32_t numHelpers)
   {
   uint8_t *bufferStart = (uint8_t *)trampPtr, *buffer;

   // This code assumes that Helpers use System Linkage.
   //
   // We need a temp register. The only volatile registers that we can use are
   // R0 and R1 as the rest are used for argument passing. We can't use R0 as
   // We can't branch using R0. That only leaves R1.
   uint16_t rT = 1;
   // Create a Trampoline for each runtime helper.
   for (int32_t i = 1; i < numHelpers; i++)
      {

      // Get the helper address
      intptr_t helperAddr = (intptr_t)runtimeHelperValue((TR_RuntimeHelper)i);

      // Skip the first trampoline for index 0
      bufferStart += TRAMPOLINE_SIZE;
      buffer = bufferStart;


      // Trampoline Code:
      // zLinux Linkage rT = r4.
      // 0d40           BASR  rT, 0
      // e340400e04     LG    rT, 14(,rT)
      // 07f4           BCR   rT
      // 6-bytes padding
      //                DC    helperAddr

      // BASR rT, 0;
      *(int16_t *)buffer = 0x0d00 + (rT << 4);
      buffer += sizeof(int16_t);

      // LG rT, 14(,rT)
      *(int32_t *)buffer = 0xe300000e + (rT << 12) + (rT << 20);
      buffer += sizeof(int32_t);
      *(int16_t *)buffer = 0x0004;
      buffer += sizeof(int16_t);

      // BCR rT
      *(int16_t *)buffer = 0x07f0 | rT;
      buffer += sizeof(int16_t);

      // 6-byte Padding.
      *(int32_t *)buffer = 0x00000000;
      buffer += sizeof(int32_t);
      *(int16_t *)buffer = 0x0000;
      buffer += sizeof(int16_t);

      // DC mAddr
      *(intptr_t *)buffer = helperAddr;
      buffer += sizeof(intptr_t);
      }
   }

// zLinux64 MCC Support: Maximum Hookup.
void s390zLinux64CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = TRAMPOLINE_SIZE;
   // Call Backs for Code Gen.
   callBacks[0] = (void *)&s390zLinux64CodeCacheConfig;
   callBacks[1] = (void *)&s390zLinux64CreateHelperTrampoline;
   callBacks[2] = NULL;
   callBacks[3] = NULL;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = TR_S390numRuntimeHelpers;
   }

#endif

void setupCodeCacheParameters(int32_t *trampolineSize, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t * numHelpers, int32_t *CCPreLoadedCodeSize)
   {
#if defined(TR_TARGET_POWER)
      ppcCodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
#endif

#if defined(TR_TARGET_X86) && defined(TR_TARGET_32BIT)
      ia32CodeCacheParameters(trampolineSize, callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
#endif

#if defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)
      amd64CodeCacheParameters(trampolineSize, callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
#endif

#if defined(TR_TARGET_ARM)
      armCodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
#endif

#if defined(TR_TARGET_ARM64)
      arm64CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
#endif

#if defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT) && defined(J9ZOS390)
      s390zOS31CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
#endif

#if defined(TR_TARGET_S390) && defined(TR_TARGET_64BIT) && defined(J9ZOS390)
      s390zOS64CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
#endif

#if defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT) && !defined(J9ZOS390)
      s390zLinux31CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
#endif

#if defined(TR_TARGET_S390) && defined(TR_TARGET_64BIT) && !defined(J9ZOS390)
      s390zLinux64CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
#endif
   }

uint32_t
TR::getCCPreLoadedCodeSize()
   {
   return 0;
   }

void
TR::createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase,
                      uint8_t *CCPreLoadedCodeTop,
                      void ** CCPreLoadedCodeTable,
                      TR::CodeGenerator *cg)
   {
   return;
   }
