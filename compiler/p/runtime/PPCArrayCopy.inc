!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!! Copyright (c) 2000, 2016 IBM Corp. and others
!!
!! This program and the accompanying materials are made available under
!! the terms of the Eclipse Public License 2.0 which accompanies this
!! distribution and is available at http://eclipse.org/legal/epl-2.0
!! or the Apache License, Version 2.0 which accompanies this distribution
!! and is available at https://www.apache.org/licenses/LICENSE-2.0.
!!
!! This Source Code may also be made available under the following Secondary
!! Licenses when the conditions for such availability set forth in the
!! Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
!! version 2 with the GNU Classpath Exception [1] and GNU General Public
!! License, version 2 with the OpenJDK Assembly Exception [2].
!!
!! [1] https://www.gnu.org/software/classpath/license.html
!! [2] http://openjdk.java.net/legal/assembly-exception.html
!!
!! SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

! arrayCopy*, wordArrayCopy*, halfWordArrayCopy*, forwardArrayCopy*,
! forwardWordArrayCopy*, forwardHalfWordArrayCopy*:
!       copyByteLength -- r7
!       srcByteAddr    -- r8
!       dstByteAddr    -- r9
!  64-bit kills: r0, r5, r6, r7, r8, r9, r11, cr0
!  32-bit kills: r0, r3, r4, r5, r6, r7, r8, r9, r10, r11, cr0
!  32-bit float additionally kills: fp8, fp9, fp10, fp11
!  float double pair additionally kills: fp8, fp9, fp10, fp11


#include "p/runtime/ppcasmdefines.inc"

   .set LONG_LENGTH, 33;

#ifdef AIXPPC
   .globl    .__arrayCopy
   .globl    __arrayCopy{DS}
   .globl    .__wordArrayCopy
   .globl    __wordArrayCopy{DS}
   .globl    .__halfWordArrayCopy
   .globl    __halfWordArrayCopy{DS}
   .globl    .__forwardArrayCopy
   .globl    __forwardArrayCopy{DS}
   .globl    .__forwardWordArrayCopy
   .globl    __forwardWordArrayCopy{DS}
   .globl    .__forwardHalfWordArrayCopy
   .globl    __forwardHalfWordArrayCopy{DS}
   .globl    .__arrayCopy_dp
   .globl    __arrayCopy_dp{DS}
   .globl    .__wordArrayCopy_dp
   .globl    __wordArrayCopy_dp{DS}
   .globl    .__halfWordArrayCopy_dp
   .globl    __halfWordArrayCopy_dp{DS}
   .globl    .__forwardArrayCopy_dp
   .globl    __forwardArrayCopy_dp{DS}
   .globl    .__forwardWordArrayCopy_dp
   .globl    __forwardWordArrayCopy_dp{DS}
   .globl    .__forwardHalfWordArrayCopy_dp
   .globl    __forwardHalfWordArrayCopy_dp{DS}
   .globl    .__forwardQuadWordArrayCopy_vsx
   .globl    __forwardQuadWordArrayCopy_vsx{DS}
   .globl    .__quadWordArrayCopy_vsx
   .globl    __quadWordArrayCopy_vsx{DS}
   .globl    .__forwardLEArrayCopy
   .globl    __forwardLEArrayCopy{DS}
   .globl    .__leArrayCopy
   .globl    __leArrayCopy{DS}

   .lglobl    .__codeForShortArrayCopy
   .lglobl    L.0copy
   .lglobl    L.1copy
   .lglobl   L.2copy
   .lglobl    L.3copy
   .lglobl    L.4copy
   .lglobl    L.5copy
   .lglobl    L.6copy
   .lglobl    L.7copy
   .lglobl   L.8copy
   .lglobl    L.9copy
   .lglobl    L.10copy
   .lglobl    L.11copy
   .lglobl   L.12copy
   .lglobl    L.13copy
   .lglobl    L.14copy
   .lglobl    L.15copy
   .lglobl    L.16copy
   .lglobl    L.17copy
   .lglobl   L.18copy
   .lglobl    L.19copy
   .lglobl    L.20copy
   .lglobl    L.21copy
   .lglobl   L.22copy
   .lglobl    L.23copy
   .lglobl    L.24copy
   .lglobl    L.25copy
   .lglobl    L.26copy
   .lglobl    L.27copy
   .lglobl    L.28copy
   .lglobl    L.29copy
   .lglobl    L.30copy
   .lglobl    L.31copy
   .lglobl    L.32copy

#elif defined(LINUXPPC64)
   .globl    FUNC_LABEL(__arrayCopy)
   .type     FUNC_LABEL(__arrayCopy),@function
   .globl    FUNC_LABEL(__wordArrayCopy)
   .type     FUNC_LABEL(__wordArrayCopy),@function
   .globl    FUNC_LABEL(__halfWordArrayCopy)
   .type     FUNC_LABEL(__halfWordArrayCopy),@function
   .globl    FUNC_LABEL(__forwardArrayCopy)
   .type     FUNC_LABEL(__forwardArrayCopy),@function
   .globl    FUNC_LABEL(__forwardWordArrayCopy)
   .type     FUNC_LABEL(__forwardWordArrayCopy),@function
   .globl    FUNC_LABEL(__forwardHalfWordArrayCopy)
   .type     FUNC_LABEL(__forwardHalfWordArrayCopy),@function
   .globl    FUNC_LABEL(__arrayCopy_dp)
   .type     FUNC_LABEL(__arrayCopy_dp),@function
   .globl    FUNC_LABEL(__wordArrayCopy_dp)
   .type     FUNC_LABEL(__wordArrayCopy_dp),@function
   .globl    FUNC_LABEL(__halfWordArrayCopy_dp)
   .type     FUNC_LABEL(__halfWordArrayCopy_dp),@function
   .globl    FUNC_LABEL(__forwardArrayCopy_dp)
   .type     FUNC_LABEL(__forwardArrayCopy_dp),@function
   .globl    FUNC_LABEL(__forwardWordArrayCopy_dp)
   .type     FUNC_LABEL(__forwardWordArrayCopy_dp),@function
   .globl    FUNC_LABEL(__forwardHalfWordArrayCopy_dp)
   .type     FUNC_LABEL(__forwardHalfWordArrayCopy_dp),@function
   .globl    FUNC_LABEL(__forwardQuadWordArrayCopy_vsx)
   .type     FUNC_LABEL(__forwardQuadWordArrayCopy_vsx),@function
   .globl    FUNC_LABEL(__quadWordArrayCopy_vsx)
   .type     FUNC_LABEL(__quadWordArrayCopy_vsx),@function
   .globl    FUNC_LABEL(__forwardLEArrayCopy)
   .type     FUNC_LABEL(__forwardLEArrayCopy),@function
   .globl    FUNC_LABEL(__leArrayCopy)
   .type     FUNC_LABEL(__leArrayCopy),@function

#elif defined(LINUX)
   .globl    __arrayCopy
   .globl    __wordArrayCopy
   .globl    __halfWordArrayCopy
   .globl    __forwardArrayCopy
   .globl    __forwardWordArrayCopy
   .globl    __forwardHalfWordArrayCopy
   .globl    __arrayCopy_dp
   .globl    __wordArrayCopy_dp
   .globl    __halfWordArrayCopy_dp
   .globl    __forwardArrayCopy_dp
   .globl    __forwardWordArrayCopy_dp
   .globl    __forwardHalfWordArrayCopy_dp
   .globl    __forwardQuadWordArrayCopy_vsx
   .globl    __quadWordArrayCopy_vsx
   .globl    __forwardLEArrayCopy
   .globl    __leArrayCopy
#endif


#if defined(LINUXPPC64)
   .section  ".text"
   .align   2
#endif

#ifdef AIXPPC
   .csect   __arrayCopy{PR}
.__arrayCopy:
   .function .__arrayCopy,startproc.__arrayCopy,16,0,(endproc.__arrayCopy-startproc.__arrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__arrayCopy):
#else
__arrayCopy:
#endif
   startproc.__arrayCopy:
L.__arrayCopy:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH       ! Decide if it is a long copy
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7            ! Reverse copy or not
   bc BO_IF, CR0_LT, .L.reverseArrayCopy
.L.testOddAddressed:
   andi. r11, r8, 1              ! Source aligned on PPC
   bc BO_IF, CR0_EQ, .L.testHalfWordAlign
   lbz   r11, 0(r8)
   addi  r8, r8, 1
   addi  r7, r7, -1
   stb   r11, 0(r9)
   addi  r9, r9, 1
.L.testHalfWordAlign:
   andi. r11, r8, 2
   bc BO_IF, CR0_EQ, .L.wordAligned
   lhz   r11, 0(r8)
   addi  r7, r7, -2
   addi  r8, r8, 2
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   sth   r11, 0(r9)
   addi  r9, r9, 2
   bc BO_IF, CR0_LT, .L.tableDispatch
.L.wordAligned:
#ifdef TR_HOST_64BIT
! Some older non-PPC-AS processors may take aligment interrupts on fixed
! doubleword loads and stores that are not word aligned (the 630 for
! example).  We will ignore the performance issue there and always use
! doublewords in a 64-bit environment.
        andi.   r11, r8, 4
        bc      BO_IF, CR0_EQ, .L.dWordAligned
   lwz   r11, 0(r8)
   addi  r7, r7, -4
   addi  r8, r8, 4
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   stw   r11, 0(r9)
   addi  r9, r9, 4
   bc BO_IF, CR0_LT, .L.tableDispatch
.L.dWordAligned:
   srdi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11
.L.dWordLoop:
   ld r6, 0(r8)
   ld r5, 8(r8)
   ld r0, 16(r8)
   ld r11, 24(r8)
   addi  r8, r8, 32
   std   r6, 0(r9)
   std   r5, 8(r9)
   std   r0, 16(r9)
   std   r11, 24(r9)
   addi  r9, r9, 32
   bdnz  .L.dWordLoop
   bclr  BO_IF, CR0_EQ
#else

! On some processors, a float doubleword load/store that is not word aligned
! will take an alignment interrupt.  If the target is not word aligned, fall
! back on using fixed word loads/stores.
        andi.   r11, r9, 3
        bc      BO_IF_NOT, CR0_EQ, .L.targetMisaligned
        andi.   r11, r8, 4
        bc      BO_IF, CR0_EQ, .L.dWordAligned
   lwz   r11, 0(r8)
   addi  r7, r7, -4
   addi  r8, r8, 4
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   stw   r11, 0(r9)
   addi  r9, r9, 4
   bc BO_IF, CR0_LT, .L.tableDispatch
.L.dWordAligned:
   srwi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11
.L.dWordLoop:
   lfd   fp11, 0(r8)
   lfd   fp10, 8(r8)
   lfd   fp9, 16(r8)
   lfd   fp8, 24(r8)
   addi  r8, r8, 32
   stfd  fp11, 0(r9)
   stfd  fp10, 8(r9)
   stfd  fp9, 16(r9)
   stfd  fp8, 24(r9)
   addi  r9, r9, 32
   bdnz  .L.dWordLoop
   bclr  BO_IF, CR0_EQ
   b  .L.tableDispatch
.L.targetMisaligned:

   srwi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11
.L.wordLoop:
   lwz   r6, 0(r8)
   lwz   r5, 4(r8)
   lwz   r0, 8(r8)
   lwz   r11, 12(r8)
   stw   r6, 0(r9)
   stw   r5, 4(r9)
   stw   r0, 8(r9)
   stw   r11, 12(r9)
   lwz   r6, 16(r8)
   lwz   r5, 20(r8)
   lwz   r0, 24(r8)
   lwz   r11, 28(r8)
   addi  r8, r8, 32
   stw   r6, 16(r9)
   stw   r5, 20(r9)
   stw   r0, 24(r9)
   stw   r11, 28(r9)
   addi  r9, r9, 32
   bdnz  .L.wordLoop
   bclr  BO_IF, CR0_EQ
#endif
.L.tableDispatch:
        dcbt    r0, r8
        dcbtst  r0, r9
   laddr r11, J9TR_VMThreadRTOCOffset(J9VM_STRUCT) ! Restore TOC/GOT

#ifdef TR_HOST_64BIT
   rlwinm   r7, r7, 3, 0xFFFFFFF8
#else
   rlwinm   r7, r7, 2, 0xFFFFFFFC
#endif
#ifdef AIXPPC
   laddr r11, TOC__shortArrayCopyLabelTable(r11)
#elif defined(LINUXPPC64)
   laddr r11, TOC__shortArrayCopyLabelTable@toc(r11)
#else
   laddr r11, __shortArrayCopyLabelTable@got(r11)
#endif
   laddrx   r11, r11, r7
   mtctr r11
   bctr

.L.reverseArrayCopy:
   add   r8, r8, r7
   add   r9, r9, r7
   andi. r11, r8, 1
   bc BO_IF, CR0_EQ, .L.testReverseHalfWordAlign
   lbzu  r11, -1(r8)
   addi  r7, r7, -1
   stbu  r11, -1(r9)
.L.testReverseHalfWordAlign:
   andi. r11, r8, 2
   bc BO_IF, CR0_EQ, .L.reverseWordAligned
   addi  r7, r7, -2
   lhzu  r11, -2(r8)
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   sthu  r11, -2(r9)
   bc BO_IF, CR0_LT, .L.reverseTableDispatch
.L.reverseWordAligned:
#ifdef TR_HOST_64BIT
! Some older non-PPC-AS processors may take aligment interrupts on fixed
! doubleword loads and stores that are not word aligned (the 630 for
! example).  We will ignore the performance issue there and always use
! doublewords in a 64-bit environment.
        andi.   r11, r8, 4
        bc      BO_IF, CR0_EQ, .L.reverseDWordAligned
        addi    r7, r7, -4
        lwzu    r11, -4(r8)
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        stwu    r11, -4(r9)
        bc      BO_IF, CR0_LT, .L.reverseTableDispatch
.L.reverseDWordAligned:
   srdi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11
.L.reverseDWordLoop:
        ld      r6, -8(r8)
        ld      r5, -16(r8)
        ld      r0, -24(r8)
   ldu   r11, -32(r8)
        std     r6, -8(r9)
        std     r5, -16(r9)
        std     r0, -24(r9)
        stdu    r11, -32(r9)
   bdnz  .L.reverseDWordLoop
   bclr  BO_IF, CR0_EQ
#else

! On some processors, a float doubleword load/store that is not word aligned
! will take an alignment interrupt.  If the target is not word aligned, fall
! back on using fixed word loads/stores.
        andi.   r11, r9, 3
        bc      BO_IF_NOT, CR0_EQ, .L.reverseTargetMisaligned
        andi.   r11, r8, 4
        bc      BO_IF, CR0_EQ, .L.reverseDWordAligned
        addi    r7, r7, -4
        lwzu    r11, -4(r8)
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        stwu    r11, -4(r9)
        bc      BO_IF, CR0_LT, .L.reverseTableDispatch
.L.reverseDWordAligned:
   srwi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11
.L.reverseDWordLoop:
   lfd   fp11, -8(r8)
   lfd   fp10, -16(r8)
   lfd   fp9, -24(r8)
   lfdu  fp8, -32(r8)
   stfd  fp11, -8(r9)
   stfd  fp10, -16(r9)
   stfd  fp9, -24(r9)
   stfdu fp8, -32(r9)
   bdnz  .L.reverseDWordLoop
   bclr  BO_IF, CR0_EQ
   b  .L.reverseTableDispatch
.L.reverseTargetMisaligned:

   srwi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11
.L.reverseWordLoop:
   lwz   r6, -4(r8)
   lwz   r5, -8(r8)
   lwz   r0, -12(r8)
   lwz   r11, -16(r8)
   stw   r6, -4(r9)
   stw   r5, -8(r9)
   stw   r0, -12(r9)
   stw   r11, -16(r9)
   lwz   r6, -20(r8)
   lwz   r5, -24(r8)
   lwz   r0, -28(r8)
   lwzu  r11, -32(r8)
   stw   r6, -20(r9)
   stw   r5, -24(r9)
   stw   r0, -28(r9)
   stwu  r11, -32(r9)
   bdnz  .L.reverseWordLoop
   bclr  BO_IF, CR0_EQ
#endif
.L.reverseTableDispatch:
   subf  r8, r7, r8
   subf  r9, r7, r9
   b  .L.tableDispatch
   endproc.__arrayCopy:


#ifdef AIXPPC
   .csect   __wordArrayCopy{PR}
.__wordArrayCopy:
   .function .__wordArrayCopy,startproc.__wordArrayCopy,16,0,(endproc.__wordArrayCopy-startproc.__wordArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__wordArrayCopy):
#else
__wordArrayCopy:
#endif
   startproc.__wordArrayCopy:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7
   bc BO_IF_NOT, CR0_LT, .L.wordAligned
   add   r8, r8, r7
   add   r9, r9, r7
   b  .L.reverseWordAligned
   endproc.__wordArrayCopy:


#ifdef AIXPPC
   .csect   __halfWordArrayCopy{PR}
.__halfWordArrayCopy:
   .function .__halfWordArrayCopy,startproc.__halfWordArrayCopy,16,0,(endproc.__halfWordArrayCopy-startproc.__halfWordArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__halfWordArrayCopy):
#else
__halfWordArrayCopy:
#endif
   startproc.__halfWordArrayCopy:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7
   bc BO_IF_NOT, CR0_LT, .L.testHalfWordAlign
   add   r8, r8, r7
   add   r9, r9, r7
   b  .L.testReverseHalfWordAlign
   endproc.__halfWordArrayCopy:


#ifdef AIXPPC
   .csect   __forwardArrayCopy{PR}
.__forwardArrayCopy:
   .function .__forwardArrayCopy,startproc.__forwardArrayCopy,16,0,(endproc.__forwardArrayCopy-startproc.__forwardArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardArrayCopy):
#else
__forwardArrayCopy:
#endif
   startproc.__forwardArrayCopy:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.tableDispatch
   b  .L.testOddAddressed
   endproc.__forwardArrayCopy:


#ifdef AIXPPC
   .csect   __forwardWordArrayCopy{PR}
.__forwardWordArrayCopy:
   .function .__forwardWordArrayCopy,startproc.__forwardWordArrayCopy,16,0,(endproc.__forwardWordArrayCopy-startproc.__forwardWordArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardWordArrayCopy):
#else
__forwardWordArrayCopy:
#endif
   startproc.__forwardWordArrayCopy:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.tableDispatch
   b  .L.wordAligned
   endproc.__forwardWordArrayCopy:


#ifdef AIXPPC
   .csect   __forwardHalfWordArrayCopy{PR}
.__forwardHalfWordArrayCopy:
   .function .__forwardHalfWordArrayCopy,startproc.__forwardHalfWordArrayCopy,16,0,(endproc.__forwardHalfWordArrayCopy-startproc.__forwardHalfWordArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardHalfWordArrayCopy):
#else
__forwardHalfWordArrayCopy:
#endif
   startproc.__forwardHalfWordArrayCopy:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.tableDispatch
   b  .L.testHalfWordAlign
   endproc.__forwardHalfWordArrayCopy:


! This code is for POWER6 and later processors that support store float
! double pair instructions.  The code is scheduled for a POWER6.


#ifdef AIXPPC
   .csect   __arrayCopy_dp{PR}, 5
.__arrayCopy_dp:
   .function .__arrayCopy_dp,startproc.__arrayCopy_dp,16,0,(endproc.__arrayCopy_dp-startproc.__arrayCopy_dp)
#elif defined(LINUXPPC64)
   .align   5
FUNC_LABEL(__arrayCopy_dp):
#elif defined(LINUX)
   .align   5
__arrayCopy_dp:
#else
__arrayCopy_dp:
#endif
   startproc.__arrayCopy_dp:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH       ! Decide if it is a long copy
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7            ! Reverse copy or not
   bc BO_IF, CR0_LT, .L.reverseArrayCopy_dp
.L.testOddAddressed_dp:
   andi. r11, r9, 1              ! Destination aligned on PPC
   bc BO_IF, CR0_EQ, .L.testHalfWordAlign_dp
   lbz   r11, 0(r8)
   addi  r8, r8, 1
   addi  r7, r7, -1
   stb   r11, 0(r9)
   addi  r9, r9, 1
.L.testHalfWordAlign_dp:
   andi. r11, r9, 2
   bc BO_IF, CR0_EQ, .L.wordAligned_dp
   lhz   r11, 0(r8)
   addi  r7, r7, -2
   addi  r8, r8, 2
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   sth   r11, 0(r9)
   addi  r9, r9, 2
   bc BO_IF, CR0_LT, .L.tableDispatch
.L.wordAligned_dp:
! On a P6 processor stores that cross an 8B boundary cause two store
! operations from the core to the GPS, and that will cause a slowdown if
! the store queue becomes full.
   andi. r11, r9, 4
   bc BO_IF, CR0_EQ, .L.dWordAligned_dp
   lwz   r11, 0(r8)
   addi  r7, r7, -4
   addi  r8, r8, 4
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   stw   r11, 0(r9)
   addi  r9, r9, 4
   bc BO_IF, CR0_LT, .L.tableDispatch
.L.dWordAligned_dp:
! Destination is doubleword (8-byte) aligned.
! On a P6 processor a floating point load/store that is not word aligned
! will take an alignment interrupt.  If the source is not word aligned, fall
! back on using fixed point doubleword loads/stores.
   andi. r11, r8, 7
   bc BO_IF_NOT, CR0_EQ, .L.sourceMisaligned_dp
! To use stfdp we need to align the destination on a 16-byte boundary.
   andi. r11, r9, 8
   bc BO_IF, CR0_EQ, .L.qWordAligned_dp
   lfd   fp8, 0(r8)
   addi  r7, r7, -8
   addi  r8, r8, 8
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   stfd  fp8, 0(r9)
   addi  r9, r9, 8
   bc BO_IF, CR0_LT, .L.tableDispatch
#if defined(AIXPPC) || defined(LINUX) || defined(LINUXPPC64)
   .align   5     ! force 32-byte I-cache sector alignment here
#endif
.L.qWordAligned_dp:
#ifdef TR_HOST_64BIT
   srdi. r11, r7, 6  ! number of 64 byte copy loop iterations
#else
   srwi. r11, r7, 6  ! number of 64 byte copy loop iterations
#endif
   addi  r5, r8, 32  ! extra source pointer to avoid AGI delays
   addi  r6, r9, 32  ! extra destination pointer to avoid AGI delays
   bc BO_IF, CR0_EQ, .L.residueBlock
   mtctr r11      ! first/last (alone in group) on P6
.L.qWordLoop_dp:
   lfd   fp8, 0(r8)           ! group 1
   lfd   fp9, 8(r8)
   .long 0xf5090000  ! stfdp  fp8, 0(r9)  ! group 2
! 32-byte I-cache sector boundary (group start forced on DD3)
   lfd   fp10, 16(r8)            ! group 3
   lfd   fp11, 24(r8)
   addi  r8, r8, 64     ! 3 cycles to next load use of r8
   .long 0xf5490010  ! stfdp  fp10, 16(r9)   ! group 4
   addi  r9, r9, 64     ! 3 cycles to next store use of r9
   lfd   fp8, 0(r5)           ! group 5
   lfd   fp9, 8(r5)
   .long 0xf5060000  ! stfdp  fp8, 0(r6)  ! group 6
! 32-byte I-cache sector boundary (group start forced on DD3)
   lfd   fp10, 16(r5)            ! group 7
   lfd   fp11, 24(r5)
   addi  r5, r5, 64     ! 3 cycles to next load use of r5
   .long 0xf5460010  ! stfdp  fp10, 16(r6)   ! group 8
   addi  r6, r6, 64     ! 3 cycles to next store use of r6
   bdnz  .L.qWordLoop_dp    ! 1/cycle, no grouping restriction
.L.residueBlock:
   andi. r11, r7, 32 ! 32-byte residue block?
   bc BO_IF, CR0_EQ, .L.over
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   .long 0xf5090000  ! stfdp fp8, 0(r9)
   lfd   fp10, 16(r8)
   lfd   fp11, 24(r8)
   addi  r8, r8, 32
   .long 0xf5490010  ! stfdp fp10, 16(r9)
   addi  r9, r9, 32
.L.over:
   andi. r7, r7, 31  ! residue bytes
   bclr  BO_IF, CR0_EQ
   b  .L.tableDispatch
.L.sourceMisaligned_dp:
.L.testOddAddressedSourceMisaligned_dp:
        andi.   r11, r8, 1                                      ! source  aligned on PPC
        bc      BO_IF, CR0_EQ, .L.testHalfWordAlignSourceMisaligned_dp
        lbz     r11, 0(r8)
        addi    r8, r8, 1
        addi    r7, r7, -1
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        stb     r11, 0(r9)
        addi    r9, r9, 1
        bc      BO_IF, CR0_LT, .L.tableDispatch
.L.testHalfWordAlignSourceMisaligned_dp:
        andi.   r11, r8, 2
        bc      BO_IF, CR0_EQ, .L.continueSourceMisaligned_dp
        lhz     r11, 0(r8)
        addi    r7, r7, -2
        addi    r8, r8, 2
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        sth     r11, 0(r9)
        addi    r9, r9, 2
        bc      BO_IF, CR0_LT, .L.tableDispatch
.L.continueSourceMisaligned_dp:
#ifdef TR_HOST_64BIT
! Making source 8 byte aligned for 64-bit environment
        andi.   r11, r8, 4
        bc      BO_IF, CR0_EQ, .L.continueSourceMisalignedLoop_dp
        lwz     r11, 0(r8)
        addi    r7, r7, -4
        addi    r8, r8, 4
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        stw     r11, 0(r9)
        addi    r9, r9, 4
        bc      BO_IF, CR0_LT, .L.tableDispatch
.L.continueSourceMisalignedLoop_dp:
! use doubleword loads and stores for the copy
   srdi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11      ! first/last (alone in group) on P6
.L.dWordLoop_dp:
   ld r6, 0(r8)
   ld r5, 8(r8)
   std   r6, 0(r9)
   std   r5, 8(r9)
   ld r0, 16(r8)
   ld r11, 24(r8)
   addi  r8, r8, 32
   std   r0, 16(r9)
   std   r11, 24(r9)
   addi  r9, r9, 32
   bdnz  .L.dWordLoop_dp
   bclr  BO_IF, CR0_EQ
   b  .L.tableDispatch
#else
! use word loads and stores for the copy
   srwi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11      ! first/last (alone in group) on P6
.L.wordLoop_dp:
   lwz   r6, 0(r8)
   lwz   r5, 4(r8)
   stw   r6, 0(r9)
   stw   r5, 4(r9)
   lwz   r0, 8(r8)
   lwz   r11, 12(r8)
   stw   r0, 8(r9)
   stw   r11, 12(r9)
   lwz   r6, 16(r8)
   lwz   r5, 20(r8)
   stw   r6, 16(r9)
   stw   r5, 20(r9)
   lwz   r0, 24(r8)
   lwz   r11, 28(r8)
   addi  r8, r8, 32
   stw   r0, 24(r9)
   stw   r11, 28(r9)
   addi  r9, r9, 32
   bdnz  .L.wordLoop_dp
   bclr  BO_IF, CR0_EQ
   b  .L.tableDispatch
#endif

.L.reverseArrayCopy_dp:
   add   r8, r8, r7
   add   r9, r9, r7
   andi. r11, r9, 1
   bc BO_IF, CR0_EQ, .L.testReverseHalfWordAlign_dp
   lbz   r11, -1(r8)
   addi  r8, r8, -1
   addi  r7, r7, -1
   stb   r11, -1(r9)
   addi  r9, r9, -1
.L.testReverseHalfWordAlign_dp:
   andi. r11, r9, 2
   bc BO_IF, CR0_EQ, .L.reverseWordAligned_dp
   lhz   r11, -2(r8)
   addi  r8, r8, -2
   addi  r7, r7, -2
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   sth   r11, -2(r9)
   addi  r9, r9, -2
   bc BO_IF, CR0_LT, .L.reverseTableDispatch
.L.reverseWordAligned_dp:
! On a P6 processor stores that cross an 8B boundary cause two store
! operations from the core to the GPS, and that will cause a slowdown if
! the store queue becomes full.
   andi. r11, r9, 4
   bc BO_IF, CR0_EQ, .L.reverseDWordAligned_dp
   lwz   r11, -4(r8)
   addi  r8, r8, -4
   addi  r7, r7, -4
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   stw   r11, -4(r9)
   addi  r9, r9, -4
   bc BO_IF, CR0_LT, .L.reverseTableDispatch
.L.reverseDWordAligned_dp:
! Destination is doubleword (8-byte) aligned.
! On a P6 processor a floating point load/store that is not word aligned
! will take an alignment interrupt.  If the source is not word aligned, fall
! back on using fixed point doubleword loads/stores.
   andi. r11, r8, 7
   bc BO_IF_NOT, CR0_EQ, .L.reverseSourceMisaligned_dp
! To use stfdp we need to align the destination on a 16-byte boundary.
   andi. r11, r9, 8
   bc BO_IF, CR0_EQ, .L.reverseQWordAligned_dp
   lfd   fp8, -8(r8)
   addi  r8, r8, -8
   addi  r7, r7, -8
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   stfd  fp8, -8(r9)
   addi  r9, r9, -8
   bc BO_IF, CR0_LT, .L.reverseTableDispatch
#if defined(AIXPPC) || defined(LINUX) || defined(LINUXPPC64)
   .align   5     ! force 32-byte I-cache sector alignment here
#endif
.L.reverseQWordAligned_dp:
#ifdef TR_HOST_64BIT
   srdi. r11, r7, 6  ! number of 64 byte copy loop iterations
#else
   srwi. r11, r7, 6  ! number of 64 byte copy loop iterations
#endif
   addi  r5, r8, -32 ! extra source pointer to avoid AGI delays
   addi  r6, r9, -32 ! extra destination pointer to avoid AGI delays
   bc BO_IF, CR0_EQ, .L.reverseResidueBlock
   mtctr r11      ! first/last (alone in group) on P6
.L.reverseQWordLoop_dp:
   lfd   fp9, -8(r8)          ! group 1
   lfd   fp8, -16(r8)
   .long 0xf509fff0  ! stfdp  fp8, -16(r9)   ! group 2
! 32-byte I-cache sector boundary (group start forced on DD3)
   lfd   fp11, -24(r8)           ! group 3
   lfd   fp10, -32(r8)
   addi  r8, r8, -64    ! 3 cycles to next load use of r8
   .long 0xf549ffe0  ! stfdp  fp10, -32(r9)  ! group 4
   addi  r9, r9, -64    ! 3 cycles to next store use of r9
   lfd   fp9, -8(r5)          ! group 5
   lfd   fp8, -16(r5)
   .long 0xf506fff0  ! stfdp  fp8, -16(r6)   ! group 6
! 32-byte I-cache sector boundary (group start forced on DD3)
   lfd   fp11, -24(r5)           ! group 7
   lfd   fp10, -32(r5)
   addi  r5, r5, -64    ! 3 cycles to next load use of r5
   .long 0xf546ffe0  ! stfdp  fp10, -32(r6)  ! group 8
   addi  r6, r6, -64    ! 3 cycles to next store use of r6
   bdnz  .L.reverseQWordLoop_dp   ! 1/cycle, no grouping restriction
.L.reverseResidueBlock:
   andi. r11, r7, 32 ! 32-byte residue block?
   bc BO_IF, CR0_EQ, .L.reverseOver
   lfd   fp9, -8(r8)
   lfd   fp8, -16(r8)
   .long 0xf509fff0  ! stfdp  fp8, -16(r9)
   lfd   fp11, -24(r8)
   lfd   fp10, -32(r8)
   addi  r8, r8, -32
   .long 0xf549ffe0  ! stfdp  fp10, -32(r9)
   addi  r9, r9, -32
.L.reverseOver:
   andi. r7, r7, 31  ! residue bytes
   bclr  BO_IF, CR0_EQ
   b  .L.reverseTableDispatch
.L.reverseSourceMisaligned_dp:
.L.reverseTestOddAddressedSourceMisaligned_dp:
        andi.   r11, r8, 1                                      ! source  aligned on PPC
        bc      BO_IF, CR0_EQ, .L.reverseTestHalfWordAlignSourceMisaligned_dp
        lbz     r11, -1(r8)
        addi    r8, r8, -1
        addi    r7, r7, -1
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        stb     r11, -1(r9)
        addi    r9, r9, -1
        bc      BO_IF, CR0_LT, .L.reverseTableDispatch
.L.reverseTestHalfWordAlignSourceMisaligned_dp:
        andi.   r11, r8, 2
        bc      BO_IF, CR0_EQ, .L.continueReverseSourceMisaligned_dp
        lhz     r11, -2(r8)
        addi    r8, r8, -2
        addi    r7, r7, -2
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        sth     r11, -2(r9)
        addi    r9, r9, -2
        bc      BO_IF, CR0_LT, .L.reverseTableDispatch
.L.continueReverseSourceMisaligned_dp:
#ifdef TR_HOST_64BIT
! Making source 8 byte aligned for 64-bit environment
        andi.   r11, r8, 4
        bc      BO_IF, CR0_EQ, .L.continueReverseSourceMisalignedLoop_dp
        lwz     r11, -4(r8)
        addi    r7, r7, -4
        addi    r8, r8, -4
        cmpli   cr0, CmpAddr, r7, LONG_LENGTH
        stw     r11, -4(r9)
        addi    r9, r9, -4
        bc      BO_IF, CR0_LT, .L.reverseTableDispatch
.L.continueReverseSourceMisalignedLoop_dp:
! use doubleword loads and stores for the copy
   srdi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11      ! first/last (alone in group) on P6
.L.reverseDWordLoop_dp:
   ld r6, -8(r8)
   ld r5, -16(r8)
   std   r6, -8(r9)
   std   r5, -16(r9)
   ld r0, -24(r8)
   ld r11, -32(r8)
   addi  r8, r8, -32
   std   r0, -24(r9)
   std   r11, -32(r9)
   addi  r9, r9, -32
   bdnz  .L.reverseDWordLoop_dp
   bclr  BO_IF, CR0_EQ
   b  .L.reverseTableDispatch
#else
! use word loads and stores for the copy
   srwi  r11, r7, 5  ! number of loop iterations
   andi. r7, r7, 31  ! residue bytes
   mtctr r11      ! first/last (alone in group) on P6
.L.reverseWordLoop_dp:
   lwz   r6, -4(r8)
   lwz   r5, -8(r8)
   stw   r6, -4(r9)
   stw   r5, -8(r9)
   lwz   r0, -12(r8)
   lwz   r11, -16(r8)
   stw   r0, -12(r9)
   stw   r11, -16(r9)
   lwz   r6, -20(r8)
   lwz   r5, -24(r8)
   stw   r6, -20(r9)
   stw   r5, -24(r9)
   lwz   r0, -28(r8)
   lwz   r11, -32(r8)
   addi  r8, r8, -32
   stw   r0, -28(r9)
   stw   r11, -32(r9)
   addi  r9, r9, -32
   bdnz  .L.reverseWordLoop_dp
   bclr  BO_IF, CR0_EQ
   b  .L.reverseTableDispatch
#endif
   endproc.__arrayCopy_dp:


#ifdef AIXPPC
   .csect   __wordArrayCopy_dp{PR}
.__wordArrayCopy_dp:
   .function .__wordArrayCopy_dp,startproc.__wordArrayCopy_dp,16,0,(endproc.__wordArrayCopy_dp-startproc.__wordArrayCopy_dp)
#elif defined(LINUXPPC64)
FUNC_LABEL(__wordArrayCopy_dp):
#else
__wordArrayCopy_dp:
#endif
   startproc.__wordArrayCopy_dp:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7
   bc BO_IF_NOT, CR0_LT, .L.wordAligned_dp
   add   r8, r8, r7
   add   r9, r9, r7
   b  .L.reverseWordAligned_dp
   endproc.__wordArrayCopy_dp:


#ifdef AIXPPC
   .csect   __halfWordArrayCopy_dp{PR}
.__halfWordArrayCopy_dp:
   .function .__halfWordArrayCopy_dp,startproc.__halfWordArrayCopy_dp,16,0,(endproc.__halfWordArrayCopy_dp-startproc.__halfWordArrayCopy_dp)
#elif defined(LINUXPPC64)
FUNC_LABEL(__halfWordArrayCopy_dp):
#else
__halfWordArrayCopy_dp:
#endif
   startproc.__halfWordArrayCopy_dp:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7
   bc BO_IF_NOT, CR0_LT, .L.testHalfWordAlign_dp
   add   r8, r8, r7
   add   r9, r9, r7
   b  .L.testReverseHalfWordAlign_dp
   endproc.__halfWordArrayCopy_dp:


#ifdef AIXPPC
   .csect   __forwardArrayCopy_dp{PR}
.__forwardArrayCopy_dp:
   .function .__forwardArrayCopy_dp,startproc.__forwardArrayCopy_dp,16,0,(endproc.__forwardArrayCopy_dp-startproc.__forwardArrayCopy_dp)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardArrayCopy_dp):
#else
__forwardArrayCopy_dp:
#endif
   startproc.__forwardArrayCopy_dp:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.tableDispatch
   b  .L.testOddAddressed_dp
   endproc.__forwardArrayCopy_dp:


#ifdef AIXPPC
   .csect   __forwardArrayCopy_dp{PR}
.__forwardWordArrayCopy_dp:
   .function .__forwardWordArrayCopy_dp,startproc.__forwardWordArrayCopy_dp,16,0,(endproc.__forwardWordArrayCopy_dp-startproc.__forwardWordArrayCopy_dp)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardWordArrayCopy_dp):
#else
__forwardWordArrayCopy_dp:
#endif
   startproc.__forwardWordArrayCopy_dp:
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.tableDispatch
   b  .L.wordAligned_dp
   endproc.__forwardWordArrayCopy_dp:


#ifdef AIXPPC
   .csect   __forwardHalfWordArrayCopy_dp{PR}, 5
.__forwardHalfWordArrayCopy_dp:
   .function .__forwardHalfWordArrayCopy_dp,startproc.__forwardHalfWordArrayCopy_dp,16,0,(endproc.__forwardHalfWordArrayCopy_dp-startproc.__forwardHalfWordArrayCopy_dp)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardHalfWordArrayCopy_dp):
#else
__forwardHalfWordArrayCopy_dp:
#endif
   startproc.__forwardHalfWordArrayCopy_dp:
        dcbt    r0, r8
        dcbtst  r0, r9
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
        bc      BO_IF_NOT, CR0_LT, .L.testHalfWordAlign_dp
        laddr   r11, J9TR_VMThreadRTOCOffset(J9VM_STRUCT)       ! Restore TOC/GOT

#ifdef TR_HOST_64BIT
        rlwinm  r7, r7, 3, 0xFFFFFFF8
#else
        rlwinm  r7, r7, 2, 0xFFFFFFFC
#endif
#ifdef AIXPPC
        laddr   r11, TOC__shortArrayCopyLabelTable(r11)
#elif defined(LINUXPPC64)
        laddr   r11, TOC__shortArrayCopyLabelTable@toc(r11)
#else
        laddr   r11, __shortArrayCopyLabelTable@got(r11)
#endif
        laddrx  r11, r11, r7
        mtctr   r11
        bctr
   endproc.__forwardHalfWordArrayCopy_dp:



#ifdef AIXPPC
   .csect   __codeForShortArrayCopy{PR}
.__codeForShortArrayCopy:
   .function .__codeForShortArrayCopy, startproc.__codeForShortArrayCopy,16,0,(endproc.__codeForShortArrayCopy-startproc.__codeForShortArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__codeForShortArrayCopy):
#endif
   startproc.__codeForShortArrayCopy:
! Note: Do not attempt to use floating point loads and stores in
! these short straight-line array copy sequences.  This is because we do
! not know the alignment of the source (r8) and destination (r9), and some
! processors will take alignment interrupts on f.p. loads or stores
! that are not at least word aligned.
! Some older non-PPC-AS processors may take aligment interrupts on fixed
! doubleword loads and stores that are not word aligned (the 630 for
! example).  We will ignore the performance issue there and always use
! doublewords in a 64-bit environment.
L.0copy:
   bclr  BO_ALWAYS, CR0_EQ

L.1copy:
   lbz   r8, 0(r8)
   stb   r8, 0(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.2copy:
   lhz   r8, 0(r8)
   sth   r8, 0(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.3copy:
   lhz   r11, 0(r8)
   lbz   r8, 2(r8)
   sth   r11, 0(r9)
   stb   r8, 2(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.4copy:
#if defined(__LITTLE_ENDIAN__)
   lfs   fp8, 0(r8)
   stfs  fp8, 0(r9)
#else
   lwz   r8, 0(r8)
   stw   r8, 0(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ

L.5copy:
#if defined(__LITTLE_ENDIAN__)
   lfs   fp8, 0(r8)
#else
   lwz   r11, 0(r8)
#endif
   lbz   r8, 4(r8)
#if defined(__LITTLE_ENDIAN__)
   stfs  fp8, 0(r9)
#else
   stw   r11, 0(r9)
#endif
   stb   r8, 4(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.6copy:
#if defined(__LITTLE_ENDIAN__)
   lfs   fp8, 0(r8)
#else
   lwz   r11, 0(r8)
#endif
   lhz   r8, 4(r8)
#if defined(__LITTLE_ENDIAN__)
   stfs  fp8, 0(r9)
#else
   stw   r11, 0(r9)
#endif
   sth   r8, 4(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.7copy:
#if defined(__LITTLE_ENDIAN__)
   lfs   fp8, 0(r8)
#else
   lwz   r7, 0(r8)
#endif
   lhz   r11, 4(r8)
   lbz   r8, 6(r8)
#if defined(__LITTLE_ENDIAN__)
   stfs  fp8, 0(r9)
#else
   stw   r7, 0(r9)
#endif
   sth   r11, 4(r9)
   stb   r8, 6(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.8copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   stfd  fp8, 0(r9)
#else
   ld      r8, 0(r8)
   std     r8, 0(r9)
#endif
#else
   lwz   r11, 0(r8)
   lwz   r8, 4(r8)
   stw   r11, 0(r9)
   stw   r8, 4(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ

L.9copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
#else
   ld      r11, 0(r8)
#endif
   lbz   r8, 8(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
#else
   std   r11, 0(r9)
#endif
#else
   lwz   r7, 0(r8)
   lwz   r11, 4(r8)
   lbz   r8, 8(r8)
   stw   r7, 0(r9)
   stw   r11, 4(r9)
#endif
   stb   r8, 8(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.10copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
#else
   ld      r11, 0(r8)
#endif
   lhz   r8, 8(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
#else
   std   r11, 0(r9)
#endif
#else
   lwz   r7, 0(r8)
   lwz   r11, 4(r8)
   lhz   r8, 8(r8)
   stw   r7, 0(r9)
   stw   r11, 4(r9)
#endif
   sth   r8, 8(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.11copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
#else
   ld r7, 0(r8)
#endif
   lhz   r11, 8(r8)
   lbz   r8, 10(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
#else
   std   r7, 0(r9)
#endif
#else
   lwz   r0, 0(r8)
   lwz   r7, 4(r8)
   lhz   r11, 8(r8)
   lbz   r8, 10(r8)
   stw   r0, 0(r9)
   stw   r7, 4(r9)
#endif
   sth   r11, 8(r9)
   stb   r8, 10(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.12copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfs   fp9, 8(r8)
   stfd  fp8, 0(r9)
   stfs  fp9, 8(r9)
#else
   ld r11, 0(r8)
   lwz   r8, 8(r8)
   std   r11, 0(r9)
   stw   r8, 8(r9)
#endif
#else
   lwz   r7, 0(r8)
   lwz   r11, 4(r8)
   lwz   r8, 8(r8)
   stw   r7, 0(r9)
   stw   r11, 4(r9)
   stw   r8, 8(r9)
#endif

   bclr  BO_ALWAYS, CR0_EQ

L.13copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfs   fp9, 8(r8)
#else
   ld r7, 0(r8)
   lwz   r11, 8(r8)
#endif
   lbz   r8, 12(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfs  fp9, 8(r9)
#else
   std   r7, 0(r9)
   stw   r11, 8(r9)
#endif
   stb   r8, 12(r9)
#else
   lwz   r0, 0(r8)
   lwz   r7, 4(r8)
   lwz   r11, 8(r8)
   lbz   r8, 12(r8)
   stw   r0, 0(r9)
   stw   r7, 4(r9)
   stw   r11, 8(r9)
   stb   r8, 12(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ

L.14copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfs   fp9, 8(r8)
#else
   ld r7, 0(r8)
   lwz   r11, 8(r8)
#endif
   lhz   r8, 12(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfs  fp9, 8(r9)
#else
   std   r7, 0(r9)
   stw   r11, 8(r9)
#endif
#else
   lwz   r0, 0(r8)
   lwz   r7, 4(r8)
   lwz   r11, 8(r8)
   lhz   r8, 12(r8)
   stw   r0, 0(r9)
   stw   r7, 4(r9)
   stw   r11, 8(r9)
#endif
   sth   r8, 12(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.15copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfs   fp9, 8(r8)
#else
   ld r0, 0(r8)
   lwz   r7, 8(r8)
#endif
   lhz   r11, 12(r8)
   lbz   r8, 14(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfs  fp9, 8(r9)
#else
   std   r0, 0(r9)
   stw   r7, 8(r9)
#endif
#else
   lwz   r3, 0(r8)
   lwz   r0, 4(r8)
   lwz   r7, 8(r8)
   lhz   r11, 12(r8)
   lbz   r8, 14(r8)
   stw   r3, 0(r9)
   stw   r0, 4(r9)
   stw   r7, 8(r9)
#endif
   sth   r11, 12(r9)
   stb   r8, 14(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.16copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
#else
   ld r11, 0(r8)
   ld r8, 8(r8)
   std   r11, 0(r9)
   std   r8, 8(r9)
#endif
#else
   lwz   r0, 0(r8)
   lwz   r7, 4(r8)
   lwz   r11, 8(r8)
   lwz   r8, 12(r8)
   stw   r0, 0(r9)
   stw   r7, 4(r9)
   stw   r11, 8(r9)
   stw   r8, 12(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ

L.17copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
#else
   ld r7, 0(r8)
   ld r11, 8(r8)
#endif
   lbz   r8, 16(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
#else
   std   r7, 0(r9)
   std   r11, 8(r9)
#endif
#else
   lwz   r3, 0(r8)
   lwz   r0, 4(r8)
   lwz   r7, 8(r8)
   lwz   r11, 12(r8)
   lbz   r8, 16(r8)
   stw   r3, 0(r9)
   stw   r0, 4(r9)
   stw   r7, 8(r9)
   stw   r11, 12(r9)
#endif
   stb   r8, 16(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.18copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
#else
   ld r7, 0(r8)
   ld r11, 8(r8)
#endif
   lhz   r8, 16(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
#else
   std   r7, 0(r9)
   std   r11, 8(r9)
#endif
#else
   lwz   r3, 0(r8)
   lwz   r0, 4(r8)
   lwz   r7, 8(r8)
   lwz   r11, 12(r8)
   lhz   r8, 16(r8)
   stw   r3, 0(r9)
   stw   r0, 4(r9)
   stw   r7, 8(r9)
   stw   r11, 12(r9)
#endif
   sth   r8, 16(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.19copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
#else
   ld r0, 0(r8)
   ld r7, 8(r8)
#endif
   lhz   r11, 16(r8)
   lbz   r8, 18(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
#else
   std   r0, 0(r9)
   std   r7, 8(r9)
#endif
#else
   lwz   r4, 0(r8)
   lwz   r3, 4(r8)
   lwz   r0, 8(r8)
   lwz   r7, 12(r8)
   lhz   r11, 16(r8)
   lbz   r8, 18(r8)
   stw   r4, 0(r9)
   stw   r3, 4(r9)
   stw   r0, 8(r9)
   stw   r7, 12(r9)
#endif
   sth   r11, 16(r9)
   stb   r8, 18(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.20copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfs   fp10,16(r8)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfs  fp10,16(r9)
#else
   ld r7, 0(r8)
   ld r11, 8(r8)
   lwz   r8, 16(r8)
   std   r7, 0(r9)
   std   r11, 8(r9)
   stw   r8, 16(r9)
#endif
#else
   lwz   r3, 0(r8)
   lwz   r0, 4(r8)
   lwz   r7, 8(r8)
   lwz   r11, 12(r8)
   lwz   r8, 16(r8)
   stw   r3, 0(r9)
   stw   r0, 4(r9)
   stw   r7, 8(r9)
   stw   r11, 12(r9)
   stw   r8, 16(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ

L.21copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfs   fp10,16(r8)
#else
   ld r0, 0(r8)
   ld r7, 8(r8)
   lwz   r11, 16(r8)
#endif
   lbz   r8, 20(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfs  fp10,16(r9)
#else
   std   r0, 0(r9)
   std   r7, 8(r9)
   stw   r11, 16(r9)
#endif
#else
   lwz   r4, 0(r8)
   lwz   r3, 4(r8)
   lwz   r0, 8(r8)
   lwz   r7, 12(r8)
   lwz   r11, 16(r8)
   lbz   r8, 20(r8)
   stw   r4, 0(r9)
   stw   r3, 4(r9)
   stw   r0, 8(r9)
   stw   r7, 12(r9)
   stw   r11, 16(r9)
#endif
   stb   r8, 20(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.22copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfs   fp10,16(r8)
#else
   ld r0, 0(r8)
   ld r7, 8(r8)
   lwz   r11, 16(r8)
#endif
   lhz   r8, 20(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfs  fp10,16(r9)
#else
   std   r0, 0(r9)
   std   r7, 8(r9)
   stw   r11, 16(r9)
#endif
#else
   lwz   r4, 0(r8)
   lwz   r3, 4(r8)
   lwz   r0, 8(r8)
   lwz   r7, 12(r8)
   lwz   r11, 16(r8)
   lhz   r8, 20(r8)
   stw   r4, 0(r9)
   stw   r3, 4(r9)
   stw   r0, 8(r9)
   stw   r7, 12(r9)
   stw   r11, 16(r9)
#endif
   sth   r8, 20(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.23copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfs   fp10,16(r8)
#else
   ld r5, 0(r8)
   ld r0, 8(r8)
   lwz   r7, 16(r8)
#endif
   lhz   r11, 20(r8)
   lbz   r8, 22(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfs  fp10,16(r9)
#else
   std   r5, 0(r9)
   std   r0, 8(r9)
   stw   r7, 16(r9)
#endif
#else
   lwz   r5, 0(r8)
   lwz   r4, 4(r8)
   lwz   r3, 8(r8)
   lwz   r0, 12(r8)
   lwz   r7, 16(r8)
   lhz   r11, 20(r8)
   lbz   r8, 22(r8)
   stw   r5, 0(r9)
   stw   r4, 4(r9)
   stw   r3, 8(r9)
   stw   r0, 12(r9)
   stw   r7, 16(r9)
#endif
   sth   r11, 20(r9)
   stb   r8, 22(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.24copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
#else
   ld r7, 0(r8)
   ld r11, 8(r8)
   ld r8, 16(r8)
   std   r7, 0(r9)
   std   r11, 8(r9)
   std   r8, 16(r9)
#endif
#else
   lwz   r4, 0(r8)
   lwz   r3, 4(r8)
   lwz   r0, 8(r8)
   lwz   r7, 12(r8)
   lwz   r11, 16(r8)
   lwz   r8, 20(r8)
   stw   r4, 0(r9)
   stw   r3, 4(r9)
   stw   r0, 8(r9)
   stw   r7, 12(r9)
   stw   r11, 16(r9)
   stw   r8, 20(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ

L.25copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
#else
   ld r0, 0(r8)
   ld r7, 8(r8)
   ld r11, 16(r8)
#endif
   lbz   r8, 24(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
#else
   std   r0, 0(r9)
   std   r7, 8(r9)
   std   r11, 16(r9)
#endif
#else
   lwz   r5, 0(r8)
   lwz   r4, 4(r8)
   lwz   r3, 8(r8)
   lwz   r0, 12(r8)
   lwz   r7, 16(r8)
   lwz   r11, 20(r8)
   lbz   r8, 24(r8)
   stw   r5, 0(r9)
   stw   r4, 4(r9)
   stw   r3, 8(r9)
   stw   r0, 12(r9)
   stw   r7, 16(r9)
   stw   r11, 20(r9)
#endif
   stb   r8, 24(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.26copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
#else
   ld r0, 0(r8)
   ld r7, 8(r8)
   ld r11, 16(r8)
#endif
   lhz   r8, 24(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
#else
   std   r0, 0(r9)
   std   r7, 8(r9)
   std   r11, 16(r9)
#endif
#else
   lwz   r5, 0(r8)
   lwz   r4, 4(r8)
   lwz   r3, 8(r8)
   lwz   r0, 12(r8)
   lwz   r7, 16(r8)
   lwz   r11, 20(r8)
   lhz   r8, 24(r8)
   stw   r5, 0(r9)
   stw   r4, 4(r9)
   stw   r3, 8(r9)
   stw   r0, 12(r9)
   stw   r7, 16(r9)
   stw   r11, 20(r9)
#endif
   sth   r8, 24(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.27copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
#else
   ld r5, 0(r8)
   ld r0, 8(r8)
   ld r7, 16(r8)
#endif
   lhz   r11, 24(r8)
   lbz   r8, 26(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
#else
   std   r5, 0(r9)
   std   r0, 8(r9)
   std   r7, 16(r9)
#endif
#else
   lwz   r6, 0(r8)
   lwz   r5, 4(r8)
   lwz   r4, 8(r8)
   lwz   r3, 12(r8)
   lwz   r0, 16(r8)
   lwz   r7, 20(r8)
   lhz   r11, 24(r8)
   lbz   r8, 26(r8)
   stw   r6, 0(r9)
   stw   r5, 4(r9)
   stw   r4, 8(r9)
   stw   r3, 12(r9)
   stw   r0, 16(r9)
   stw   r7, 20(r9)
#endif
   sth   r11, 24(r9)
   stb   r8, 26(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.28copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
   lfs   fp11,24(r8)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
   stfs  fp11,24(r9)
#else
   ld r0, 0(r8)
   ld r7, 8(r8)
   ld r11, 16(r8)
   lwz   r8, 24(r8)
   std   r0, 0(r9)
   std   r7, 8(r9)
   std   r11, 16(r9)
   stw   r8, 24(r9)
#endif
#else
   lwz   r5, 0(r8)
   lwz   r4, 4(r8)
   lwz   r3, 8(r8)
   lwz   r0, 12(r8)
   lwz   r7, 16(r8)
   lwz   r11, 20(r8)
   lwz   r8, 24(r8)
   stw   r5, 0(r9)
   stw   r4, 4(r9)
   stw   r3, 8(r9)
   stw   r0, 12(r9)
   stw   r7, 16(r9)
   stw   r11, 20(r9)
   stw   r8, 24(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ

L.29copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
   lfs   fp11,24(r8)
#else
   ld r5, 0(r8)
   ld r0, 8(r8)
   ld r7, 16(r8)
   lwz   r11, 24(r8)
#endif
   lbz   r8, 28(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
   stfs  fp11,24(r9)
#else
   std   r5, 0(r9)
   std   r0, 8(r9)
   std   r7, 16(r9)
   stw   r11, 24(r9)
#endif
#else
   lwz   r6, 0(r8)
   lwz   r5, 4(r8)
   lwz   r4, 8(r8)
   lwz   r3, 12(r8)
   lwz   r0, 16(r8)
   lwz   r7, 20(r8)
   lwz   r11, 24(r8)
   lbz   r8, 28(r8)
   stw   r6, 0(r9)
   stw   r5, 4(r9)
   stw   r4, 8(r9)
   stw   r3, 12(r9)
   stw   r0, 16(r9)
   stw   r7, 20(r9)
   stw   r11, 24(r9)
#endif
   stb   r8, 28(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.30copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
   lfs   fp11,24(r8)
#else
   ld r5, 0(r8)
   ld r0, 8(r8)
   ld r7, 16(r8)
   lwz   r11, 24(r8)
#endif
   lhz   r8, 28(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
   stfs  fp11,24(r9)
#else
   std   r5, 0(r9)
   std   r0, 8(r9)
   std   r7, 16(r9)
   stw   r11, 24(r9)
#endif
#else
   lwz   r6, 0(r8)
   lwz   r5, 4(r8)
   lwz   r4, 8(r8)
   lwz   r3, 12(r8)
   lwz   r0, 16(r8)
   lwz   r7, 20(r8)
   lwz   r11, 24(r8)
   lhz   r8, 28(r8)
   stw   r6, 0(r9)
   stw   r5, 4(r9)
   stw   r4, 8(r9)
   stw   r3, 12(r9)
   stw   r0, 16(r9)
   stw   r7, 20(r9)
   stw   r11, 24(r9)
#endif
   sth   r8, 28(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.31copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
   lfs   fp11,24(r8)
#else
   ld r6, 0(r8)
   ld r5, 8(r8)
   ld r0, 16(r8)
   lwz   r7, 24(r8)
#endif
   lhz   r11, 28(r8)
   lbz   r8, 30(r8)
#if defined(__LITTLE_ENDIAN__)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
   stfs  fp11,24(r9)
#else
   std   r6, 0(r9)
   std   r5, 8(r9)
   std   r0, 16(r9)
   stw   r7, 24(r9)
#endif
#else
   lwz   r10, 0(r8)
   lwz   r6, 4(r8)
   lwz   r5, 8(r8)
   lwz   r4, 12(r8)
   lwz   r3, 16(r8)
   lwz   r0, 20(r8)
   lwz   r7, 24(r8)
   lhz   r11, 28(r8)
   lbz   r8, 30(r8)
   stw   r10, 0(r9)
   stw   r6, 4(r9)
   stw   r5, 8(r9)
   stw   r4, 12(r9)
   stw   r3, 16(r9)
   stw   r0, 20(r9)
   stw   r7, 24(r9)
#endif
   sth   r11, 28(r9)
   stb   r8, 30(r9)
   bclr  BO_ALWAYS, CR0_EQ

L.32copy:
#ifdef TR_HOST_64BIT
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, 0(r8)
   lfd   fp9, 8(r8)
   lfd   fp10,16(r8)
   lfd   fp11,24(r8)
   stfd  fp8, 0(r9)
   stfd  fp9, 8(r9)
   stfd  fp10,16(r9)
   stfd  fp11,24(r9)
#else
   ld r0, 0(r8)
   ld r7, 8(r8)
   ld r11, 16(r8)
   ld r8, 24(r8)
   std   r0, 0(r9)
   std   r7, 8(r9)
   std   r11, 16(r9)
   std   r8, 24(r9)
#endif
#else
   lwz   r6, 0(r8)
   lwz   r5, 4(r8)
   lwz   r4, 8(r8)
   lwz   r3, 12(r8)
   lwz   r0, 16(r8)
   lwz   r7, 20(r8)
   lwz   r11, 24(r8)
   lwz   r8, 28(r8)
   stw   r6, 0(r9)
   stw   r5, 4(r9)
   stw   r4, 8(r9)
   stw   r3, 12(r9)
   stw   r0, 16(r9)
   stw   r7, 20(r9)
   stw   r11, 24(r9)
   stw   r8, 28(r9)
#endif
   bclr  BO_ALWAYS, CR0_EQ
   endproc.__codeForShortArrayCopy:

#ifdef AIXPPC
   .csect   __forwardQuadWordArrayCopy_vsx{PR}
.__forwardQuadWordArrayCopy_vsx:
   .function .__forwardQuadWordArrayCopy_vsx,startproc.__forwardQuadWordArrayCopy_vsx,16,0,(endproc.__forwardQuadWordArrayCopy_vsx-startproc.__forwardQuadWordArrayCopy_vsx)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardQuadWordArrayCopy_vsx):
#else
__forwardQuadWordArrayCopy_vsx:
#endif
startproc.__forwardQuadWordArrayCopy_vsx:

   or r6, r7, r7
   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.tableDispatch
   b  L.quadWordAlignment_vsx

endproc.__forwardQuadWordArrayCopy_vsx:

#ifdef AIXPPC
   .csect   __quadWordArrayCopy_vsx{PR}
.__quadWordArrayCopy_vsx:
   .function .__quadWordArrayCopy_vsx,startproc.__quadWordArrayCopy_vsx,16,0,(endproc.__quadWordArrayCopy_vsx-startproc.__quadWordArrayCopy_vsx)
#elif defined(LINUXPPC64)
FUNC_LABEL(__quadWordArrayCopy_vsx):
#else
__quadWordArrayCopy_vsx:
#endif
startproc.__quadWordArrayCopy_vsx:

   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7
   bc BO_IF_NOT, CR0_LT, L.quadWordAlignment_vsx

   add   r8, r8, r7
   add   r9, r9, r7
   b  L.reverseQuadWordAlignment_vsx

endproc.__quadWordArrayCopy_vsx:


L.quadWordAlignment_vsx:
   !Precondition, must strictly have r7 >= 32 to land here.

   !Need 16 byte alignment for VSX loads/stores.
   !Test 8byte alignment first, before 16 byte.
   andi.   r11, r8, 0x7
   beq     cr0,  L.quadWordAlignment_vsx_test8

   !Here we are not aligned at 8 bytes.
   !Copy data using GPRs till we hit 16 byte alignment.
   andi.   r11, r8, 0x1
   beq     cr0, L.quadWordAlignment_vsx_test2
   lbz     r6, 0(r8)
   stb     r6, 0(r9)
   addi r8, r8,  1
   addi r9, r9,  1
   addi r7, r7, -1

L.quadWordAlignment_vsx_test2:
   andi.   r11, r8, 0x2
   beq     cr0, L.quadWordAlignment_vsx_test4
   lhz     r6, 0(r8)
   sth     r6, 0(r9)
   addi r8, r8,  2
   addi r9, r9,  2
   addi r7, r7, -2

L.quadWordAlignment_vsx_test4:
   andi.   r11, r8, 0x4
   beq     cr0, L.quadWordAlignment_vsx_test8
#if defined(__LITTLE_ENDIAN__)
   lfs     fp8, 0(r8)
   stfs    fp8, 0(r9)
#else
   lwz     r6, 0(r8)
   stw     r6, 0(r9)
#endif
   addi r8, r8,  4
   addi r9, r9,  4
   addi r7, r7, -4

L.quadWordAlignment_vsx_test8:
   andi.   r11, r8, 0x8
   beq     cr0, L.quadWordAligned_vsx
#if defined(__LITTLE_ENDIAN__)
   lfd     fp8, 0(r8)
   stfd    fp8, 0(r9)
#elif defined(TR_HOST_64BIT)
   ld      r6, 0(r8)
   std     r6, 0(r9)
#else
   lwz      r6, 0(r8)
   lwz      r5, 4(r8)
   stw      r6, 0(r9)
   stw      r5, 4(r9)
#endif
   addi r8, r8,  8
   addi r9, r9,  8
   addi r7, r7, -8

L.quadWordAligned_vsx:

   addi   r6,   0, 16
   srwi  r11,  r7,  6            ! number of 64byte loop iterations
   andi.  r7,  r7, 63            ! residue bytes

   !If r11 is zero, then cannot just go forward and do the vector code.
   !we can have a case where 0 <= r7 < 64
   cmpli cr0, CmpAddr, r11, 0
   beq   cr0, L.quadWordAligned_vsx_residue    ! false, goto residue

   mtctr r11

L.quadWordLoop_vsx:
#ifdef AIXPPC
   .machine "push"
   .machine "pwr7"
#endif
   lxvw4x   vs32,   0, r8
   lxvw4x   vs33,  r6, r8
   stxvw4x vs32,   0, r9
   stxvw4x vs33,  r6, r9
   addi  r8, r8, 32
   addi  r9, r9, 32
   lxvw4x   vs32,   0, r8
   lxvw4x   vs33,  r6, r8
   stxvw4x vs32,   0, r9
   stxvw4x vs33,  r6, r9
   addi  r8, r8, 32
   addi  r9, r9, 32
#ifdef AIXPPC
   .machine "pop"
#endif
   bdnz  L.quadWordLoop_vsx

   cmpli cr0, CmpAddr, r7, 0     ! check if any residue left.
   bclr  BO_IF, CR0_EQ           ! return if no residue left.

L.quadWordAligned_vsx_residue:
   !This code is quadWordAligned, and 0 < r7 < 64

   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.tableDispatch

   !This code is quadWordAligned, and 32 <= r7 < 64

#ifdef AIXPPC
   .machine "push"
   .machine "pwr7"
#endif
   lxvw4x   vs32,   0, r8
   lxvw4x   vs33,  r6, r8
   stxvw4x vs32,   0, r9
   stxvw4x vs33,  r6, r9
   addi  r8, r8, 32
   addi  r9, r9, 32
   addi  r7, r7, -32
#ifdef AIXPPC
   .machine "pop"
#endif
   !Now 0 <= r7 < 32
   cmpli cr0, CmpAddr, r7, 0        ! check if any residue left.
   bclr  BO_IF, CR0_EQ              ! return if no residue left.
   b     .L.tableDispatch           ! table dispatch the remaining residue.

L.reverseQuadWordAlignment_vsx:
   !Precondition, must strictly have r7 >= 32 to land here.

   !Need 16 byte alignment for VSX loads/stores.
   !Test 8 byte alignment first, before 16 byte.
   andi.   r11, r8, 0x7
   beq     cr0,  L.reverseQuadWordAlignment_vsx_test8

   !Here we are not aligned at 8 bytes.
   !Copy data using GPRs till we hit 16 byte alignment.
   andi.   r11, r8, 0x1
   beq     cr0, L.reverseQuadWordAlignment_vsx_test2
   lbz     r6, -1(r8)
   stb     r6, -1(r9)
   addi r8, r8, -1
   addi r9, r9, -1
   addi r7, r7, -1

L.reverseQuadWordAlignment_vsx_test2:
   andi.   r11, r8, 0x2
   beq     cr0, L.reverseQuadWordAlignment_vsx_test4
   lhz     r6, -2(r8)
   sth     r6, -2(r9)
   addi r8, r8, -2
   addi r9, r9, -2
   addi r7, r7, -2

L.reverseQuadWordAlignment_vsx_test4:
   andi.   r11, r8, 0x4
   beq     cr0, L.reverseQuadWordAlignment_vsx_test8
#if defined(__LITTLE_ENDIAN__)
   lfs     fp8, -4(r8)
   stfs    fp8, -4(r9)
#else
   lwz     r6, -4(r8)
   stw     r6, -4(r9)
#endif
   addi r8, r8, -4
   addi r9, r9, -4
   addi r7, r7, -4

L.reverseQuadWordAlignment_vsx_test8:
   andi.   r11, r8, 0x8
   beq     cr0, L.reverseQuadWordAligned_vsx
#if defined(__LITTLE_ENDIAN__)
   lfd   fp8, -8(r8)
   stfd  fp8, -8(r9)
#elif defined(TR_HOST_64BIT)
   ld    r6, -8(r8)
   std   r6, -8(r9)
#else
   lwz      r6, -4(r8)
   lwz      r5, -8(r8)
   stw      r6, -4(r9)
   stw      r5, -8(r9)
#endif
   addi r8, r8, -8
   addi r9, r9, -8
   addi r7, r7, -8

L.reverseQuadWordAligned_vsx:

   addi   r6,   0, -16
   addi   r5,   0, -32
   srwi  r11,  r7,  6            ! number of 64byte loop iterations
   andi.  r7,  r7,  63           ! residue bytes

   !If r11 is zero, then cannot just go forward and do the vector code.
   !we can have a case where 0 <= r7 < 64
   cmpli cr0, CmpAddr, r11, 0
   beq   cr0, L.reverseQuadWordAligned_vsx_residue    ! false, goto residue

   mtctr r11

L.reverseQuadWordLoop_vsx:
#ifdef AIXPPC
.machine "push"
.machine "pwr7"
#endif
   lxvw4x   vs32,  r6, r8
   lxvw4x   vs33,  r5, r8
   stxvw4x  vs32,  r6, r9
   stxvw4x  vs33,  r5, r9
   addi     r8, r8, -32
   addi     r9, r9, -32
   lxvw4x   vs32,  r6, r8
   lxvw4x   vs33,  r5, r8
   stxvw4x  vs32,  r6, r9
   stxvw4x  vs33,  r5, r9
   addi     r8, r8, -32
   addi     r9, r9, -32
#ifdef AIXPPC
.machine "pop"
#endif
   bdnz  L.reverseQuadWordLoop_vsx

   cmpli cr0, CmpAddr, r7, 0        ! check if any residue left.
   bclr  BO_IF, CR0_EQ              ! return if no residue left.

L.reverseQuadWordAligned_vsx_residue:
   !This code is quadWordAligned, and 0 < r7 < 64

   cmpli cr0, CmpAddr, r7, LONG_LENGTH
   bc BO_IF, CR0_LT, .L.reverseTableDispatch

   !This code is quadWordAligned, and 32 <= r7 < 64

#ifdef AIXPPC
.machine "push"
.machine "pwr7"
#endif
   lxvw4x   vs32,  r6, r8
   lxvw4x   vs33,  r5, r8
   stxvw4x  vs32,  r6, r9
   stxvw4x  vs33,  r5, r9
   addi     r8, r8, -32
   addi     r9, r9, -32
   addi     r7, r7, -32
#ifdef AIXPPC
.machine "pop"
#endif

   !Now 0 <= r7 < 32
   cmpli cr0, CmpAddr, r7, 0        ! check if any residue left.
   bclr  BO_IF, CR0_EQ              ! return if no residue left.
   b     .L.reverseTableDispatch    ! table dispatch the remaining residue.

#ifdef AIXPPC
   .csect   __forwardLEArrayCopy{PR}
.__forwardLEArrayCopy:
   .function .__forwardLEArrayCopy,startproc.__forwardLEArrayCopy,16,0,(endproc.__forwardLEArrayCopy-startproc.__forwardLEArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__forwardLEArrayCopy):
#else
__forwardLEArrayCopy:
#endif
startproc.__forwardLEArrayCopy:

   cmpli cr0, CmpAddr, r7, 17
   bc BO_IF, CR0_LT, .L.tableDispatch
   b  L.doubleWordAlignment_le

endproc.__forwardLEArrayCopy:

#ifdef AIXPPC
   .csect   __leArrayCopy{PR}
.__leArrayCopy:
   .function .__leArrayCopy,startproc.__leArrayCopy,16,0,(endproc.__leArrayCopy-startproc.__leArrayCopy)
#elif defined(LINUXPPC64)
FUNC_LABEL(__leArrayCopy):
#else
__leArrayCopy:
#endif
startproc.__leArrayCopy:

   cmpli cr0, CmpAddr, r7, 17
   subf  r11, r8, r9
   bc BO_IF, CR0_LT, .L.tableDispatch
   cmpl  cr0, CmpAddr, r11, r7
   bc BO_IF_NOT, CR0_LT, L.doubleWordAlignment_le

   add   r8, r8, r7
   add   r9, r9, r7
   b  L.reverseDoubleWordAlignment_le

endproc.__leArrayCopy:

L.doubleWordAlignment_le:
   !Precondition, must strictly have r7 >= 32 to land here.

   !Need 16 byte alignment for VSX loads/stores.
   !Test 8byte alignment first, before 16 byte.
   andi.   r11, r8, 0x7
   beq     cr0,  L.doubleWordAligned_le

   !Here we are not aligned at 8 bytes.
   !Copy data using GPRs till we hit 16 byte alignment.
   andi.   r11, r8, 0x1
   beq     cr0, L.doubleWordAlignment_le_test2
   lbz     r6, 0(r8)
   stb     r6, 0(r9)
   addi r8, r8,  1
   addi r9, r9,  1
   addi r7, r7, -1

L.doubleWordAlignment_le_test2:
   andi.   r11, r8, 0x2
   beq     cr0, L.doubleWordAlignment_le_test4
   lhz     r6, 0(r8)
   sth     r6, 0(r9)
   addi r8, r8,  2
   addi r9, r9,  2
   addi r7, r7, -2

L.doubleWordAlignment_le_test4:
   andi.   r11, r8, 0x4
   beq     cr0, L.doubleWordAligned_le
   lfs     fp8, 0(r8)
   stfs     fp8, 0(r9)
   addi r8, r8,  4
   addi r9, r9,  4
   addi r7, r7, -4

L.doubleWordAligned_le:

   srwi  r11,  r7,  4            ! number of 16byte loop iterations
   andi.  r7,  r7, 15            ! residue bytes

   !If r11 is zero, then cannot just go forward and do the vector code.
   !we can have a case where 0 <= r7 < 64
   cmpli cr0, CmpAddr, r11, 0
   beq   cr0, .L.tableDispatch    ! false, goto residue

   mtctr r11

L.doubleWordLoop_le:
   lfd     fp8, 0(r8)
   lfd     fp9, 8(r8)
   stfd    fp8, 0(r9)
   stfd    fp9, 8(r9)
   addi  r8, r8, 16
   addi  r9, r9, 16
   bdnz  L.doubleWordLoop_le

   cmpli cr0, CmpAddr, r7, 0     ! check if any residue left.
   bclr  BO_IF, CR0_EQ           ! return if no residue left.
   !This code is quadWordAligned, and 0 < r7 < 16
   b     .L.tableDispatch

L.reverseDoubleWordAlignment_le:
   !Precondition, must strictly have r7 >= 32 to land here.

   !Need 16 byte alignment for VSX loads/stores.
   !Test 8 byte alignment first, before 16 byte.
   andi.   r11, r8, 0x7
   beq     cr0,  L.reverseDoubleWordAligned_le

   !Here we are not aligned at 8 bytes.
   !Copy data using GPRs till we hit 16 byte alignment.
   andi.   r11, r8, 0x1
   beq     cr0, L.reverseDoubleWordAlignment_le_test2
   lbz     r6, -1(r8)
   stb     r6, -1(r9)
   addi r8, r8, -1
   addi r9, r9, -1
   addi r7, r7, -1

L.reverseDoubleWordAlignment_le_test2:
   andi.   r11, r8, 0x2
   beq     cr0, L.reversedoubleWordAlignment_le_test4
   lhz     r6, -2(r8)
   sth     r6, -2(r9)
   addi r8, r8, -2
   addi r9, r9, -2
   addi r7, r7, -2

L.reversedoubleWordAlignment_le_test4:
   andi.   r11, r8, 0x4
   beq     cr0, L.reverseDoubleWordAligned_le
   lfs     fp8, -4(r8)
   stfs    fp8, -4(r9)
   addi r8, r8, -4
   addi r9, r9, -4
   addi r7, r7, -4

L.reverseDoubleWordAligned_le:
   srwi  r11,  r7,  4            ! number of 16byte loop iterations
   andi.  r7,  r7,  15           ! residue bytes

   !If r11 is zero, then cannot just go forward and do the vector code.
   !we can have a case where 0 <= r7 < 16
   cmpli cr0, CmpAddr, r11, 0
   beq   cr0, .L.reverseTableDispatch    ! false, goto residue

   mtctr r11

L.reverseDoubleWordLoop_le:
   lfd     fp8, -8(r8)
   lfd     fp9, -16(r8)
   stfd    fp8, -8(r9)
   stfd    fp9, -16(r9)
   addi     r8, r8, -16
   addi     r9, r9, -16
   bdnz  L.reverseDoubleWordLoop_le

   cmpli cr0, CmpAddr, r7, 0        ! check if any residue left.
   bclr  BO_IF, CR0_EQ              ! return if no residue left.
   !This code is quadWordAligned, and 0 < r7 < 16
   b     .L.reverseTableDispatch    ! table dispatch the remaining residue.

! .data section
#ifdef AIXPPC
   .toc
TOC__shortArrayCopyLabelTable:
   .tc       __shortArrayCopyLabelTable[TC],__shortArrayCopyLabelTable

   .csect    __arrayCopy{DS}
   ADDR      .__arrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __arrayCopy{DS}

   .csect    __wordArrayCopy{DS}
   ADDR      .__wordArrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __wordArrayCopy{DS}

   .csect    __halfWordArrayCopy{DS}
   ADDR      .__halfWordArrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __halfWordArrayCopy{DS}

   .csect    __forwardArrayCopy{DS}
   ADDR      .__forwardArrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardArrayCopy{DS}

   .csect    __forwardWordArrayCopy{DS}
   ADDR      .__forwardWordArrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardWordArrayCopy{DS}

   .csect    __forwardHalfWordArrayCopy{DS}
   ADDR      .__forwardHalfWordArrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardHalfWordArrayCopy{DS}

   .csect    __arrayCopy_dp{DS}
   ADDR      .__arrayCopy_dp
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __arrayCopy_dp{DS}

   .csect    __wordArrayCopy_dp{DS}
   ADDR      .__wordArrayCopy_dp
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __wordArrayCopy_dp{DS}

   .csect    __halfWordArrayCopy_dp{DS}
   ADDR      .__halfWordArrayCopy_dp
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __halfWordArrayCopy_dp{DS}

   .csect    __forwardArrayCopy_dp{DS}
   ADDR      .__forwardArrayCopy_dp
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardArrayCopy_dp{DS}

   .csect    __forwardWordArrayCopy_dp{DS}
   ADDR      .__forwardWordArrayCopy_dp
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardWordArrayCopy_dp{DS}

   .csect    __forwardHalfWordArrayCopy_dp{DS}
   ADDR      .__forwardHalfWordArrayCopy_dp
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardHalfWordArrayCopy_dp{DS}

   .csect    __forwardQuadWordArrayCopy_vsx{DS}
   ADDR      .__forwardQuadWordArrayCopy_vsx
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardQuadWordArrayCopy_vsx{DS}

   .csect    __quadWordArrayCopy_vsx{DS}
   ADDR      .__quadWordArrayCopy_vsx
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __quadWordArrayCopy_vsx{DS}

   .csect    __forwardLEArrayCopy{DS}
   ADDR      .__forwardLEArrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __forwardLEArrayCopy{DS}

   .csect    __leArrayCopy{DS}
   ADDR      .__leArrayCopy
   ADDR      TOC{TC0}
   ADDR      0x00000000
! End   csect     __leArrayCopy{DS}


#elif defined(LINUXPPC64)
   .section  ".toc"
TOC__shortArrayCopyLabelTable:
   .tc       __shortArrayCopyLabelTable[TC],__shortArrayCopyLabelTable

#if !defined(__LITTLE_ENDIAN__)
   .section  ".opd","aw"
   .align    3
   .globl    __arrayCopy
   .size     __arrayCopy,24
__arrayCopy:
   .quad     .__arrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __wordArrayCopy
   .size     __wordArrayCopy,24
__wordArrayCopy:
   .quad     .__wordArrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __halfWordArrayCopy
   .size     __halfWordArrayCopy,24
__halfWordArrayCopy:
   .quad     .__halfWordArrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardArrayCopy
   .size     __forwardArrayCopy,24
__forwardArrayCopy:
   .quad     .__forwardArrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardWordArrayCopy
   .size     __forwardWordArrayCopy,24
__forwardWordArrayCopy:
   .quad     .__forwardWordArrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardHalfWordArrayCopy
   .size     __forwardHalfWordArrayCopy,24
__forwardHalfWordArrayCopy:
   .quad     .__forwardHalfWordArrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __arrayCopy_dp
   .size     __arrayCopy_dp,24
__arrayCopy_dp:
   .quad     .__arrayCopy_dp
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __wordArrayCopy_dp
   .size     __wordArrayCopy_dp,24
__wordArrayCopy_dp:
   .quad     .__wordArrayCopy_dp
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __halfWordArrayCopy_dp
   .size     __halfWordArrayCopy_dp,24
__halfWordArrayCopy_dp:
   .quad     .__halfWordArrayCopy_dp
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardArrayCopy_dp
   .size     __forwardArrayCopy_dp,24
__forwardArrayCopy_dp:
   .quad     .__forwardArrayCopy_dp
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardWordArrayCopy_dp
   .size     __forwardWordArrayCopy_dp,24
__forwardWordArrayCopy_dp:
   .quad     .__forwardWordArrayCopy_dp
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardHalfWordArrayCopy_dp
   .size     __forwardHalfWordArrayCopy_dp,24
__forwardHalfWordArrayCopy_dp:
   .quad     .__forwardHalfWordArrayCopy_dp
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardQuadWordArrayCopy_vsx
   .size     __forwardQuadWordArrayCopy_vsx,24
__forwardQuadWordArrayCopy_vsx:
   .quad     .__forwardQuadWordArrayCopy_vsx
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __quadWordArrayCopy_vsx
   .size     __quadWordArrayCopy_vsx,24
__quadWordArrayCopy_vsx:
   .quad     .__quadWordArrayCopy_vsx
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __forwardLEArrayCopy
   .size     __forwardLEArrayCopy,24
__forwardLEArrayCopy:
   .quad     .__forwardLEArrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

   .globl    __leArrayCopy
   .size     __leArrayCopy,24
__leArrayCopy:
   .quad     .__leArrayCopy
   .quad     .TOC.@tocbase
   .long     0x00000000
   .long     0x00000000

#endif
#endif


#if defined(AIXPPC)
   .csect    ArrayCopy_DATA{RW}
#elif defined(LINUX)
   .data
#elif defined (LINUXPPC64)
   .section ".data"
   .align   4
   .type __shortArrayCopyLabelTable,@object
   .size __shortArrayCopyLabelTable,264
#endif

__shortArrayCopyLabelTable:
    ADDR     L.0copy
    ADDR   L.1copy
    ADDR   L.2copy
    ADDR   L.3copy
    ADDR   L.4copy
    ADDR   L.5copy
    ADDR   L.6copy
    ADDR   L.7copy
    ADDR   L.8copy
    ADDR   L.9copy
    ADDR   L.10copy
    ADDR   L.11copy
    ADDR   L.12copy
    ADDR   L.13copy
    ADDR   L.14copy
    ADDR   L.15copy
    ADDR   L.16copy
    ADDR   L.17copy
    ADDR   L.18copy
    ADDR   L.19copy
    ADDR   L.20copy
    ADDR   L.21copy
    ADDR   L.22copy
    ADDR   L.23copy
    ADDR   L.24copy
    ADDR   L.25copy
    ADDR   L.26copy
    ADDR   L.27copy
    ADDR   L.28copy
    ADDR   L.29copy
    ADDR   L.30copy
    ADDR   L.31copy
    ADDR   L.32copy
! End   csect    ArrayCopy_DATA{RW}
