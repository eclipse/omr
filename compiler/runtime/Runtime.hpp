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

#ifndef RUNTIME_INCL
#define RUNTIME_INCL

#include <stdint.h>        // for int32_t, uint32_t, uintptr_t, etc
#include "env/defines.h"   // for TR_HOST_X86, TR_HOST_64BIT
#include "env/jittypes.h"  // for intptrj_t

#include "env/Processors.hpp"

#ifdef RUBY_PROJECT_SPECIFIC
#include "ruby/config.h"
#endif

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
#if !defined(TR_TARGET_S390)
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
#endif // !defined(TR_TARGET_S390)
   TR_numCCPreLoadedCode
   };

///////////////////////////////////////////
// TR_LinkageInfo
//
// Non-instantiable Abstract Class
// Cannot have any virtual methods in here
//
///////////////////////////////////////////

class TR_LinkageInfo
   {
   public:
   static TR_LinkageInfo *get(void *startPC) { return (TR_LinkageInfo*)(((uint32_t*)startPC)-1); }

   void setCountingMethodBody()     { _word |= CountingPrologue; }
   void setSamplingMethodBody()     { _word |= SamplingPrologue; }
   void setHasBeenRecompiled();
   void setHasFailedRecompilation();
   void setIsBeingRecompiled()      { _word |= IsBeingRecompiled; }
   void resetIsBeingRecompiled()    { _word &= ~IsBeingRecompiled; }

   bool isCountingMethodBody()    { return (_word & CountingPrologue) != 0; }
   bool isSamplingMethodBody()    { return (_word & SamplingPrologue) != 0; }
   bool isRecompMethodBody()      { return (_word & (SamplingPrologue | CountingPrologue)) != 0; }
   bool hasBeenRecompiled()       { return (_word & HasBeenRecompiled) != 0; }
   bool hasFailedRecompilation()  { return (_word & HasFailedRecompilation) != 0; }
   bool recompilationAttempted()  { return hasBeenRecompiled() || hasFailedRecompilation(); }
   bool isBeingCompiled()         { return (_word & IsBeingRecompiled) != 0; }

   inline uint16_t getReservedWord()            { return (_word & ReservedMask) >> 16; }
   inline void     setReservedWord(uint16_t w)  { _word |= ((w << 16) & ReservedMask); }

   int32_t getJitEntryOffset()
      {
#if defined(TR_TARGET_X86) && defined(TR_TARGET_32BIT)
      return 0;
#else
      return getReservedWord();
#endif
      }

   enum
      {
      ReturnInfoMask                     = 0x0000000F, // bottom 4 bits
      // The VM depends on these four bits - word to the wise: don't mess

      SamplingPrologue                   = 0x00000010,
      CountingPrologue                   = 0x00000020,
      // NOTE: flags have to be set under the compilation monitor or during compilation process
      HasBeenRecompiled                  = 0x00000040,
      HasFailedRecompilation             = 0x00000100,
      IsBeingRecompiled                  = 0x00000200,

      // RESERVED:
      // non-ia32:                         0xffff0000 <---- jitEntryOffset
      // ia32:                             0xffff0000 <---- Recomp/FSD save area

      ReservedMask                       = 0xFFFF0000
      };

   uint32_t _word;

   private:
   TR_LinkageInfo() {};
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
#elif defined(TR_HOST_S390)
   TR_numRuntimeHelpers = TR_S390numRuntimeHelpers
#endif

   };

class TR_RuntimeHelperTable
   {
public:
   void* getAddress(TR_RuntimeHelper h)
      {
      return h < TR_numRuntimeHelpers ? _helpers[h] : (void *) (uintptr_t) 0xDEADB00F;
      }
   TR_LinkageConventions getLinkage(TR_RuntimeHelper h)
      {
      return h < TR_numRuntimeHelpers ? _linkage[h] : TR_None;
      }
   // Linkage convention is essential when calling a helper
   // For example, on X86, there are private linkage, System V ABI, fastcall, cdecl, etc.
   void setAddress(TR_RuntimeHelper h, void * a, TR_LinkageConventions lc = TR_Helper)
      {
      _helpers[h] = translateAddress(a);
      _linkage[h] = lc;
      }

   void setConstant(TR_RuntimeHelper h, void * a)
      {
      _helpers[h] = a;
      }

private:
   // translate address is to allow each platform converting a C function pointer
   // to an address callable by assembly, which is essential for P and Z.
   void* translateAddress(void* a);
   void*                 _helpers[TR_numRuntimeHelpers];
   TR_LinkageConventions _linkage[TR_numRuntimeHelpers];
   };

extern TR_RuntimeHelperTable runtimeHelpers;



inline void*                 runtimeHelperValue(TR_RuntimeHelper h) { return runtimeHelpers.getAddress(h); }
inline TR_LinkageConventions runtimeHelperLinkage(TR_RuntimeHelper h) { return runtimeHelpers.getLinkage(h); }


// -----------------------------------------------------------------------------


// J9

#define S390_FSD_OFFSET_TO_SAVED_JITENTRY_INSTR   (-8)

#if defined(TR_HOST_S390)
void restoreJitEntryPoint(uint8_t *startPC,int32_t );
void saveJitEntryPoint(uint8_t *startPC,int32_t);
#define  OFFSET_BODYINFO_FROM_STARTPC    -(sizeof(uint32_t) + sizeof(intptrj_t))
#endif



// -----------------------------------------------------------------------------
//
// All J9

#if defined(TR_HOST_X86)

inline uint16_t jitEntryOffset(void *startPC)
   {
#if defined(TR_HOST_64BIT)
   return TR_LinkageInfo::get(startPC)->getReservedWord();
#else
   return 0;
#endif
   }

inline uint16_t jitEntryJmpInstruction(void *startPC, int32_t startPCToTarget)
   {
   const uint8_t TWO_BYTE_JUMP_INSTRUCTION = 0xEB;
   int32_t displacement = startPCToTarget - (jitEntryOffset(startPC)+2);
   return (displacement << 8) | TWO_BYTE_JUMP_INSTRUCTION;
   }

void saveFirstTwoBytes(void *startPC, int32_t startPCToSaveArea);
void replaceFirstTwoBytesWithShortJump(void *startPC, int32_t startPCToTarget);
void replaceFirstTwoBytesWithData(void *startPC, int32_t startPCToData);
#endif


// -----------------------------------------------------------------------------

#if defined(TR_HOST_POWER)

#define  OFFSET_REVERT_INTP_PRESERVED_FSD                (-4)
#define  OFFSET_REVERT_INTP_FIXED_PORTION                (-12-2*sizeof(intptrj_t))

#define  OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC        (-(16+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_BRANCH_FROM_STARTPC             (-(12+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_METHODINFO_FROM_STARTPC         (-(8+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_PRESERVED_FROM_STARTPC          (-8)

inline uint32_t getJitEntryOffset(TR_LinkageInfo *linkageInfo)
   {
   return linkageInfo->getReservedWord() & 0x0ffff;
   }
#endif



// -----------------------------------------------------------------------------

#if defined(TR_HOST_ARM)
#define  OFFSET_REVERT_INTP_FIXED_PORTION                (-12-2*sizeof(intptrj_t))
#define  OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC        (-(16+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_BRANCH_FROM_STARTPC             (-(12+sizeof(intptrj_t)))
#define  OFFSET_METHODINFO_FROM_STARTPC         (-(8+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_PRESERVED_FROM_STARTPC          (-8)
#define  START_PC_TO_METHOD_INFO_ADDRESS -8 // offset from startpc to jitted body info
#define  OFFSET_COUNTING_BRANCH_FROM_JITENTRY 36
#endif

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
   TR_NumExternalRelocationKinds          = 56,
   TR_ExternalRelocationTargetKindMask    = 0xff,
   } TR_ExternalRelocationTargetKind;



typedef struct TR_RelocationRecordInformation {
   uintptr_t data1;
   uintptr_t data2;
   uintptr_t data3;
   uintptr_t data4;
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



/* Functions used by AOT runtime to fixup recompilation info for AOT */
#if defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || (defined(TR_HOST_ARM))
uint32_t *getLinkageInfo(void * startPC);
uint32_t isRecompMethBody(void *li);
void fixPersistentMethodInfo(void *table);
void fixupMethodInfoAddressInCodeCache(void *startPC, void *bodyInfo);
#endif

// Multi Code Cache Routine for S390 for checking whether an entry point is within reach of a RIL instruction.
// In RIL instruction, the relative address is specified in number of half words.
#define CHECK_32BIT_TRAMPOLINE_RANGE(x,rip)  (((intptrj_t)(x) == (intptrj_t)(rip) + ((intptrj_t)((int32_t)(((intptrj_t)(x) - (intptrj_t)(rip))/2)))*2) && (x % 2 == 0))

// Routine to check trampoline range for x86-64
#define IS_32BIT_RIP_JUMP(x,rip)  ((intptrj_t)(x) == (intptrj_t)(rip) + (int32_t)((intptrj_t)(x) - (intptrj_t)(rip)))

// Branch limit for PPC and ARM
#define BRANCH_FORWARD_LIMIT      (0x01fffffc)
#define BRANCH_BACKWARD_LIMIT     ((int32_t)0xfe000000)


// -----------------------------------------------------------------------------

// J9
typedef struct TR_InlinedSiteLinkedListEntry
   {
   TR_ExternalRelocationTargetKind reloType;
   uint8_t *location;
   uint8_t *destination;
   uint8_t *guard;
   TR_InlinedSiteLinkedListEntry *next;
   } TR_InlinedSiteLinkedListEntry;

// J9
typedef struct TR_InlinedSiteHastTableEntry
   {
   TR_InlinedSiteLinkedListEntry *first;
   TR_InlinedSiteLinkedListEntry *last;
   } TR_InlinedSiteHastTableEntry;

// J9
typedef enum
   {
   inlinedMethodIsStatic = 1,
   inlinedMethodIsSpecial = 2,
   inlinedMethodIsVirtual = 3
   } TR_InlinedMethodKind;

// J9
typedef enum
   {
   needsFullSizeRuntimeAssumption = 1
   } TR_HCRAssumptionFlags;

// J9
typedef enum
   {
   noPerfAssumptions = 0,
   tooManyFailedValidations = 1,
   tooManyFailedInlinedMethodRelos = 2,
   tooManyFailedInlinedAllocRelos = 3
   } TR_FailedPerfAssumptionCode;


// J9

/* TR_AOTMethodHeader Versions:
 *     1.0    Java6 GA
 *     1.1    Java6 SR1
 *     2.0    Java7
 *     2.1    Java7
 *     3.0    Java 8
 */

#define TR_AOTMethodHeader_MajorVersion   3
#define TR_AOTMethodHeader_MinorVersion   0

typedef struct TR_AOTMethodHeader {
   uint16_t  minorVersion;
   uint16_t  majorVersion;
   uint32_t  offsetToRelocationDataItems;
   uint32_t  offsetToExceptionTable;
   uint32_t  offsetToPersistentInfo;
   uintptr_t compileMethodCodeStartPC;
   uintptr_t compileMethodCodeSize;
   uintptr_t compileMethodDataStartPC;
   uintptr_t compileMethodDataSize;
   uintptr_t compileFirstClassLocation;
   uint32_t flags;
} TR_AOTMethodHeader;


// J9
/* AOT Method flags */
#define TR_AOTMethodHeader_NeedsRecursiveMethodTrampolineReservation 0x00000001
#define TR_AOTMethodHeader_IsNotCapableOfMethodEnterTracing          0x00000002
#define TR_AOTMethodHeader_IsNotCapableOfMethodExitTracing           0x00000004
#define TR_AOTMethodHeader_UsesEnableStringCompressionFolding        0x00000008
#define TR_AOTMethodHeader_StringCompressionEnabled                  0x00000010

// J9
typedef struct TR_AOTInliningStats
   {
   int32_t numFailedValidations;
   int32_t numSucceededValidations;
   int32_t numMethodFromDiffClassLoader;
   int32_t numMethodInSameClass;
   int32_t numMethodNotInSameClass;
   int32_t numMethodResolvedAtCompile;
   int32_t numMethodNotResolvedAtCompile;
   int32_t numMethodROMMethodNotInSC;
   } TR_AOTInliningStats;


// J9
typedef struct TR_AOTStats
   {
   int32_t numCHEntriesAlreadyStoredInLocalList;
   int32_t numStaticEntriesAlreadyStoredInLocalList;
   int32_t numNewCHEntriesInLocalList;
   int32_t numNewStaticEntriesInLocalList;
   int32_t numNewCHEntriesInSharedClass;
   int32_t numEntriesFoundInLocalChain;
   int32_t numEntriesFoundAndValidatedInSharedClass;
   int32_t numClassChainNotInSharedClass;
   int32_t numCHInSharedCacheButFailValiation;
   int32_t numInstanceFieldInfoNotUsed;
   int32_t numStaticFieldInfoNotUsed;
   int32_t numDefiningClassNotFound;
   int32_t numInstanceFieldInfoUsed;
   int32_t numStaticFieldInfoUsed;
   int32_t numCannotGenerateHashForStore; // Shouldn't happen
   int32_t numRuntimeChainNotFound;
   int32_t numRuntimeStaticFieldUnresolvedCP;
   int32_t numRuntimeInstanceFieldUnresolvedCP;
   int32_t numRuntimeUnresolvedStaticFieldFromCP;
   int32_t numRuntimeUnresolvedInstanceFieldFromCP;
   int32_t numRuntimeResolvedStaticFieldButFailValidation;
   int32_t numRuntimeResolvedInstanceFieldButFailValidation;
   int32_t numRuntimeStaticFieldReloOK;
   int32_t numRuntimeInstanceFieldReloOK;

   int32_t numInlinedMethodOverridden;
   int32_t numInlinedMethodNotResolved;
   int32_t numInlinedMethodClassNotMatch;
   int32_t numInlinedMethodCPNotResolved;
   int32_t numInlinedMethodRelocated;
   int32_t numInlinedMethodValidationFailed;

   TR_AOTInliningStats staticMethods;
   TR_AOTInliningStats specialMethods;
   TR_AOTInliningStats virtualMethods;
   TR_AOTInliningStats interfaceMethods;

   TR_AOTInliningStats profiledInlinedMethods;
   TR_AOTInliningStats profiledClassGuards;
   TR_AOTInliningStats profiledMethodGuards;

   int32_t numDataAddressRelosSucceed;
   int32_t numDataAddressRelosFailed;

   int32_t numCheckcastNodesIlgenTime;
   int32_t numInstanceofNodesIlgenTime;
   int32_t numCheckcastNodesCodegenTime;
   int32_t numInstanceofNodesCodegenTime;

   int32_t numRuntimeClassAddressUnresolvedCP;
   int32_t numRuntimeClassAddressFromCP;
   int32_t numRuntimeClassAddressButFailValidation;
   int32_t numRuntimeClassAddressReloOK;

   int32_t numRuntimeClassAddressRelocationCount;
   int32_t numRuntimeClassAddressReloUnresolvedCP;
   int32_t numRuntimeClassAddressReloUnresolvedClass;

   int32_t numVMCheckCastEvaluator;
   int32_t numVMInstanceOfEvaluator;
   int32_t numVMIfInstanceOfEvaluator;
   int32_t numCheckCastVMHelperInstructions;
   int32_t numInstanceOfVMHelperInstructions;
   int32_t numIfInstanceOfVMHelperInstructions;

   int32_t numClassValidations;
   int32_t numClassValidationsFailed;

   TR_FailedPerfAssumptionCode failedPerfAssumptionCode;

   } TR_AOTStats;

#endif
