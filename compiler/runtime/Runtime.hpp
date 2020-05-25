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

#ifndef RUNTIME_INCL
#define RUNTIME_INCL

#include <stdint.h>
#include "env/defines.h"
#include "env/jittypes.h"

#include "env/Processors.hpp"

#include "codegen/LinkageConventionsEnum.hpp"

namespace TR { class Compilation; }


void ia32CodeCacheParameters(int32_t *, void **, int32_t *, int32_t*);
void amd64CodeCacheParameters(int32_t *, void **, int32_t *, int32_t*);
void ppcCodeCacheParameters(int32_t *, void **, int32_t *, int32_t*);
void armCodeCacheParameters(int32_t *, void **, int32_t *, int32_t*);
void s390zLinux64CodeCacheParameters(int32_t *, void **, int32_t *, int32_t*);

uint32_t getPPCCacheLineSize();

extern void setDllSlip(char* codeStart,char* codeEnd,char* dllName, TR::Compilation *);

void initializeJitRuntimeHelperTable(char jvmpi);


// -----------------------------------------------------------------------------


enum TR_CCPreLoadedCode
   {
   TR_AllocPrefetch = 0,
#if defined(TR_TARGET_POWER)
   TR_ObjAlloc,
   TR_VariableLenArrayAlloc,
   TR_ConstLenArrayAlloc,
   TR_writeBarrier,
   TR_writeBarrierAndCardMark,
   TR_cardMark,
   TR_arrayStoreCHK,
#endif // TR_TARGET_POWER
   TR_NonZeroAllocPrefetch,
   TR_numCCPreLoadedCode
   };


/**
 * Runtime Helper Table.
 *
 * \note the value of these enumerators are used to index into
 *       trampolines
 */
enum TR_RuntimeHelper
   {
#define SETVAL(A,G) A = (G),
#include "runtime/Helpers.inc"
#undef SETVAL

#if defined(TR_HOST_X86)
#if defined(TR_HOST_64BIT)
   TR_numRuntimeHelpers = TR_AMD64numRuntimeHelpers
#else
   TR_numRuntimeHelpers = TR_IA32numRuntimeHelpers
#endif
#elif defined(TR_HOST_POWER)
   TR_numRuntimeHelpers = TR_PPCnumRuntimeHelpers
#elif defined(TR_HOST_ARM)
   TR_numRuntimeHelpers = TR_ARMnumRuntimeHelpers
#elif defined(TR_HOST_ARM64)
   TR_numRuntimeHelpers = TR_ARM64numRuntimeHelpers
#elif defined(TR_HOST_S390)
   TR_numRuntimeHelpers = TR_S390numRuntimeHelpers
#elif defined(TR_HOST_RISCV)
   TR_numRuntimeHelpers = TR_RISCVnumRuntimeHelpers
#endif

   };

class TR_RuntimeHelperTable
   {
public:

    static const intptr_t INVALID_FUNCTION_POINTER = 0xdeadb00f;

    /**
     * \brief
     *
     * Looks up the given helper table and returns the function pointer if it's a function index.
     * For constant number entries, return INVALID_FUNCTION_POINTER.
     */
    void* getFunctionPointer(TR_RuntimeHelper h);

   /**
    * \brief
    *
    * For helper functions, this API returns the raw entry address. If the runtime
    * helper's corresponding linkage is not a function, return the helper as data.
    *
    */
   void* getFunctionEntryPointOrConst(TR_RuntimeHelper h);

   /**
    * \brief
    *
    * Returns the linkage type of the given helper index. The linkage types are set during
    * the JIT initilization phase.
    */
   TR_LinkageConventions getLinkage(TR_RuntimeHelper h)
      {
      return h < TR_numRuntimeHelpers ? _linkage[h] : TR_None;
      }

   /**
    * \brief
    *
    * Sets the address and linkage convention.
    *
    * Linkage convention is essential when calling a helper
    * For example, on X86, there are private linkage, System V ABI, fastcall, cdecl, etc.
    */
   void setAddress(TR_RuntimeHelper h, void * a, TR_LinkageConventions lc = TR_Helper)
      {
      _helpers[h] = a;
      _linkage[h] = lc;
      }

   /**
    * \brief
    *
    * Stores the given pointer as a helper constant.
    */
   void setConstant(TR_RuntimeHelper h, void * a)
      {
      _helpers[h] = a;
      _linkage[h] = TR_None;
      }

private:
   /**
    * \brief
    * Translate address is to allow converting a C function pointer
    * to an address callable by assembly.
    */
   void* translateAddress(void* a);

   void*                 _helpers[TR_numRuntimeHelpers];
   TR_LinkageConventions _linkage[TR_numRuntimeHelpers];
   };

extern TR_RuntimeHelperTable runtimeHelpers;


/**
 * \brief
 * Returns the value to which the runtime helper index corresponds to.
 * The values stored in the helper table are either entry point addresses or constant values, depending on the
 * linkage type.
*/
inline void*                 runtimeHelperValue(TR_RuntimeHelper h) { return runtimeHelpers.getFunctionEntryPointOrConst(h); }
inline TR_LinkageConventions runtimeHelperLinkage(TR_RuntimeHelper h) { return runtimeHelpers.getLinkage(h); }


// -----------------------------------------------------------------------------


#define RELOCATION_TYPE_DESCRIPTION_MASK  15
#define RELOCATION_TYPE_ORDERED_PAIR  32
#define RELOCATION_TYPE_EIP_OFFSET  0x40
#define RELOCATION_TYPE_WIDE_OFFSET  0x80
#define RELOCATION_CROSS_PLATFORM_FLAGS_MASK (RELOCATION_TYPE_EIP_OFFSET | RELOCATION_TYPE_WIDE_OFFSET)
#define RELOCATION_RELOC_FLAGS_MASK (~RELOCATION_CROSS_PLATFORM_FLAGS_MASK)

#define RELOCATION_TYPE_ARRAY_COPY_SUBTYPE 32
#define RELOCATION_TYPE_ARRAY_COPY_TOC     64
// These macros are intended for use when HI_VALUE and LO_VALUE will be recombined after LO_VALUE is sign-extended
// (e.g. when LO_VALUE is used with an instruction that takes a signed 16-bit operand).
// In this case we have to adjust HI_VALUE now so that the original value will be obtained once the two are recombined.
#define HI_VALUE(x)               (((x)>>16) + (((x)>>15)&1))
#define LO_VALUE(x)               ((int16_t)(x))

/*
 * WARNING: If you add a new relocation type, then you need to handle it
 * in OMR::AheadOfTimeCompile::dumpRelocationData in OMRAheadOfTimeCompile.cpp
 * You also need to a corresponding string to
 * _externalRelocationTargetKindNames[TR_NumExternalRelocationKinds]
 */

/*
 * Define some internal temporary types for use during compilation which will get converted to actual relocation types later on
 * (purposely all capital to differentiate from actual relo types and larger than actual relocation type so we catch any incorrect
 * use that treats the temp type as actual relocation type)
 */
enum
   {
   TR_HEAP_BASE = 100,
   TR_HEAP_TOP = 101,
   TR_HEAP_BASE_FOR_BARRIER_RANGE = 102,
   TR_ACTIVE_CARD_TABLE_BASE = 103,
   TR_HEAP_SIZE_FOR_BARRIER_RANGE = 104
   };

/* actual relocation types */
typedef enum
   {
   TR_NoRelocation                        = -1,
   TR_ConstantPool                        = 0,
   TR_HelperAddress                       = 1,
   TR_RelativeMethodAddress               = 2,
   TR_AbsoluteMethodAddress               = 3,
   TR_DataAddress                         = 4,
   TR_ClassObject                         = 5,
   TR_MethodObject                        = 6,
   TR_InterfaceObject                     = 7,
   TR_AbsoluteHelperAddress               = 8,
   TR_FixedSequenceAddress                = 9,
   TR_FixedSequenceAddress2               = 10,
   TR_JNIVirtualTargetAddress             = 11,
   TR_JNIStaticTargetAddress              = 12,
   TR_ArrayCopyHelper                     = 13,
   TR_ArrayCopyToc                        = 14,
   TR_BodyInfoAddress                     = 15,
   TR_Thunks                              = 16,
   TR_StaticRamMethodConst                = 17,
   TR_Trampolines                         = 18,
   TR_PicTrampolines                      = 19,
   TR_CheckMethodEnter                    = 20,
   TR_RamMethod                           = 21,
   TR_RamMethodSequence                   = 22,
   TR_RamMethodSequenceReg                = 23,
   TR_VerifyClassObjectForAlloc           = 24,
   TR_ConstantPoolOrderedPair             = 25,
   TR_AbsoluteMethodAddressOrderedPair    = 26,
   TR_VerifyRefArrayForAlloc              = 27,
   TR_J2IThunks                           = 28,
   TR_GlobalValue                         = 29,
   TR_BodyInfoAddressLoad                 = 30,
   TR_ValidateInstanceField               = 31,
   TR_InlinedStaticMethodWithNopGuard     = 32,
   TR_InlinedSpecialMethodWithNopGuard    = 33,
   TR_InlinedVirtualMethodWithNopGuard    = 34,
   TR_InlinedInterfaceMethodWithNopGuard  = 35,
   TR_SpecialRamMethodConst               = 36,
   TR_InlinedHCRMethod                    = 37,
   TR_ValidateStaticField                 = 38,
   TR_ValidateClass                       = 39,
   TR_ClassAddress                        = 40,
   TR_HCR                                 = 41,
   TR_ProfiledMethodGuardRelocation       = 42,
   TR_ProfiledClassGuardRelocation        = 43,
   TR_HierarchyGuardRelocation            = 44,
   TR_AbstractGuardRelocation             = 45,
   TR_ProfiledInlinedMethodRelocation     = 46,
   TR_MethodPointer                       = 47,
   TR_ClassPointer                        = 48,
   TR_CheckMethodExit                     = 49,
   TR_ValidateArbitraryClass              = 50,
   TR_EmitClass                           = 51,
   TR_JNISpecialTargetAddress             = 52,
   TR_VirtualRamMethodConst               = 53,
   TR_InlinedInterfaceMethod              = 54,
   TR_InlinedVirtualMethod                = 55,
   TR_NativeMethodAbsolute                = 56,
   TR_NativeMethodRelative                = 57,
   TR_ArbitraryClassAddress               = 58,
   TR_DebugCounter                        = 59,
   TR_ClassUnloadAssumption               = 60, // this should not be used in AOT relocations
   TR_J2IVirtualThunkPointer              = 61,
   TR_InlinedAbstractMethodWithNopGuard   = 62,
   TR_ValidateRootClass                   = 63,
   TR_ValidateClassByName                 = 64,
   TR_ValidateProfiledClass               = 65,
   TR_ValidateClassFromCP                 = 66,
   TR_ValidateDefiningClassFromCP         = 67,
   TR_ValidateStaticClassFromCP           = 68,
   TR_ValidateClassFromMethod             = 69,
   TR_ValidateComponentClassFromArrayClass= 70,
   TR_ValidateArrayClassFromComponentClass= 71,
   TR_ValidateSuperClassFromClass         = 72,
   TR_ValidateClassInstanceOfClass        = 73,
   TR_ValidateSystemClassByName           = 74,
   TR_ValidateClassFromITableIndexCP      = 75,
   TR_ValidateDeclaringClassFromFieldOrStatic=76,
   TR_ValidateClassClass                  = 77,
   TR_ValidateConcreteSubClassFromClass   = 78,
   TR_ValidateClassChain                  = 79,
   TR_ValidateRomClass                    = 80,
   TR_ValidatePrimitiveClass              = 81,
   TR_ValidateMethodFromInlinedSite       = 82,
   TR_ValidateMethodByName                = 83,
   TR_ValidateMethodFromClass             = 84,
   TR_ValidateStaticMethodFromCP          = 85,
   TR_ValidateSpecialMethodFromCP         = 86,
   TR_ValidateVirtualMethodFromCP         = 87,
   TR_ValidateVirtualMethodFromOffset     = 88,
   TR_ValidateInterfaceMethodFromCP       = 89,
   TR_ValidateMethodFromClassAndSig       = 90,
   TR_ValidateStackWalkerMaySkipFramesRecord=91,
   TR_ValidateArrayClassFromJavaVM        = 92,
   TR_ValidateClassInfoIsInitialized      = 93,
   TR_ValidateMethodFromSingleImplementer = 94,
   TR_ValidateMethodFromSingleInterfaceImplementer = 95,
   TR_ValidateMethodFromSingleAbstractImplementer = 96,
   TR_ValidateImproperInterfaceMethodFromCP=97,
   TR_SymbolFromManager                   = 98,
   TR_MethodCallAddress                   = 99,
   TR_DiscontiguousSymbolFromManager      = 100,
   TR_ResolvedTrampolines                 = 101,
   TR_NumExternalRelocationKinds          = 102,
   TR_ExternalRelocationTargetKindMask    = 0xff,
   } TR_ExternalRelocationTargetKind;

namespace TR {

enum SymbolType
   {
   typeOpaque,
   typeClass,
   typeMethod,
   };

}

typedef struct TR_RelocationRecordInformation {
   uintptr_t data1;
   uintptr_t data2;
   uintptr_t data3;
   uintptr_t data4;
   uintptr_t data5;
} TR_RelocationRecordInformation;



typedef enum
   {
   orderedPairSequence1 = 1,
   orderedPairSequence2 = 2
   } TR_OrderedPairSequenceKind;

typedef enum
   {
   fixedSequence1 = 1,
   fixedSequence2 = 2,
   fixedSequence3 = 3,
   fixedSequence4 = 4,
   fixedSequence5 = 5,
   fixedSequence6 = 6
   } TR_FixedSequenceKind;



// Multi Code Cache Routine for S390 for checking whether an entry point is within reach of a RIL instruction.
// In RIL instruction, the relative address is specified in number of half words.
#define CHECK_32BIT_TRAMPOLINE_RANGE(x,rip)  (((intptr_t)(x) == (intptr_t)(rip) + ((intptr_t)((int32_t)(((intptr_t)(x) - (intptr_t)(rip))/2)))*2) && (x % 2 == 0))

// Routine to check trampoline range for x86-64
#define IS_32BIT_RIP_JUMP(x,rip)  ((intptr_t)(x) == (intptr_t)(rip) + (int32_t)((intptr_t)(x) - (intptr_t)(rip)))

// Branch limit for PPC and ARM ARM64
#define BRANCH_FORWARD_LIMIT      (0x01fffffc)
#define BRANCH_BACKWARD_LIMIT     ((int32_t)0xfe000000)





#endif
