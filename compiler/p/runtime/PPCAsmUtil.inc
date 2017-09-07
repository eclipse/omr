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

#ifdef AIXPPC
	.lglobl   .__uni_lock_check
	.lglobl   .__uni_lock_update
	.lglobl   .__smp_lock_check
	.lglobl   .__smp_lock_update
	.lglobl   .__code_synchronization

	.globl	._tr_try_lock
	.globl	_tr_try_lock{DS}

	.globl	._tr_unlock
	.globl	_tr_unlock{DS}

	.globl	.jitPPCUniLockCheck
	.globl	jitPPCUniLockCheck{DS}

	.globl	.jitPPCUniLockUpdate
	.globl	jitPPCUniLockUpdate{DS}

	.globl	.jitPPCSmpLockCheck
	.globl	jitPPCSmpLockCheck{DS}

	.globl	.jitPPCSmpLockUpdate
	.globl	jitPPCSmpLockUpdate{DS}

	.globl	.jitPPCCodeSync
	.globl	jitPPCCodeSync{DS}

	.globl	.jitPPCSmpAtomicIncrement
	.globl	jitPPCSmpAtomicIncrement{DS}

	.globl    .jitPPCDataCacheBlockZero
	.globl    jitPPCDataCacheBlockZero{DS}
#elif defined(LINUXPPC64)
	.globl FUNC_LABEL(_tr_try_lock)
	.type  FUNC_LABEL(_tr_try_lock),@function

	.globl FUNC_LABEL(_tr_unlock)
	.type  FUNC_LABEL(_tr_unlock),@function

	.globl	FUNC_LABEL(jitPPCUniLockCheck)
	.type	FUNC_LABEL(jitPPCUniLockCheck),@function

	.globl	FUNC_LABEL(jitPPCUniLockUpdate)
	.type	FUNC_LABEL(jitPPCUniLockUpdate),@function

	.globl	FUNC_LABEL(jitPPCSmpLockCheck)
	.type	FUNC_LABEL(jitPPCSmpLockCheck),@function

	.globl	FUNC_LABEL(jitPPCSmpLockUpdate)
	.type	FUNC_LABEL(jitPPCSmpLockUpdate),@function

	.globl	FUNC_LABEL(jitPPCSmpAtomicIncrement)
	.type	FUNC_LABEL(jitPPCSmpAtomicIncrement),@function

	.globl	FUNC_LABEL(jitPPCCodeSync)
	.type	FUNC_LABEL(jitPPCCodeSync),@function

	.globl FUNC_LABEL(jitPPCDataCacheBlockZero)
	.type  FUNC_LABEL(jitPPCDataCacheBlockZero),@function
#elif defined(LINUX)
	.globl _tr_try_lock
	.globl _tr_unlock
	.globl jitPPCDataCacheBlockZero
	.globl jitPPCSmpAtomicIncrement
#endif

#ifdef AIXPPC
! .text section
	.csect    PicBuilder_TEXT{PR}
#elif defined(LINUXPPC64)
	.section  ".text"
	.align    2
#endif

#if defined(LINUXPPC64)
FUNC_LABEL(jitPPCUniLockCheck):
#else
.jitPPCUniLockCheck:
#endif
	mr	r8, r3
	mr	r9, r4
	mr	r10, r5
#ifdef AIXPPC
.__uni_lock_check:
	.function .__uni_lock_check,startproc.__uni_lock_check,16,0,(endproc.__uni_lock_check-startproc.__uni_lock_check)
#elif defined(LINUXPPC64)
.__uni_lock_check:
#else
__uni_lock_check:
.__uni_lock_check:
#endif
	startproc.__uni_lock_check:
	lwarx   r8, 0, r10
	cmpli   cr0, 0x0, r8, 0x0
	bclr    BO_IF_NOT, CR0_EQ
	stwcx.  r9, 0, r10
	bclr    BO_IF, CR0_EQ
	b       .__uni_lock_check
	endproc.__uni_lock_check:

#if defined(LINUXPPC64)
FUNC_LABEL(jitPPCUniLockUpdate):
#else
.jitPPCUniLockUpdate:
#endif
	mr	r9, r3
	mr	r10, r4
#ifdef AIXPPC
.__uni_lock_update:
	.function .__uni_lock_update,startproc.__uni_lock_update,16,0,(endproc.__uni_lock_update-startproc.__uni_lock_update)
#elif defined(LINUXPPC64)
.__uni_lock_update:
#else
__uni_lock_update:
.__uni_lock_update:
#endif
	startproc.__uni_lock_update:
	stw     r9, 0x0(r10)
	blr
	endproc.__uni_lock_update:

#if defined(LINUXPPC64)
FUNC_LABEL(jitPPCSmpLockCheck):
#else
.jitPPCSmpLockCheck:
#endif
	mr	r8, r3
	mr	r9, r4
	mr	r10, r5
#ifdef AIXPPC
.__smp_lock_check:
	.function .__smp_lock_check,startproc.__smp_lock_check,16,0,(endproc.__smp_lock_check-startproc.__smp_lock_check)
#elif defined(LINUXPPC64)
.__smp_lock_check:
#else
__smp_lock_check:
.__smp_lock_check:
#endif
	startproc.__smp_lock_check:
	lwarx   r8, 0, r10
	cmpli   cr0, 0x0, r8, 0x0
	bc      BO_IF_NOT, CR0_EQ, L.check_done
	stwcx.  r9, 0, r10
	bc      BO_IF_NOT, CR0_EQ, .__smp_lock_check
L.check_done:
	isync
	blr
	endproc.__smp_lock_check:

#if defined(LINUXPPC64)
FUNC_LABEL(jitPPCSmpLockUpdate):
#else
.jitPPCSmpLockUpdate:
#endif
	mr	r9, r3
	mr	r10, r4
#ifdef AIXPPC
.__smp_lock_update:
	.function .__smp_lock_update,startproc.__smp_lock_update,16,0,(endproc.__smp_lock_update-startproc.__smp_lock_update)
#elif defined(LINUXPPC64)
.__smp_lock_update:
#else
__smp_lock_update:
.__smp_lock_update:
#endif
	startproc.__smp_lock_update:
	sync
	stw     r9, 0x0(r10)
	blr
	endproc.__smp_lock_update:

#if defined(LINUXPPC64)
FUNC_LABEL(jitPPCCodeSync):
#else
.jitPPCCodeSync:
#endif
	mr	r9, r3
#ifdef AIXPPC
.__code_synchronization:
	.function .__code_synchronization,startproc.__code_synchronization,16,0,(endproc.__code_synchronization-startproc.__code_synchronization)
#elif defined(LINUXPPC64)
.__code_synchronization:
#else
__code_synchronization:
.__code_synchronization:
#endif
	startproc.__code_synchronization:
	dcbst   0, r9                          ! Write back d-cache block
	sync                                    ! Wait for it to complete
	icbi    0, r9                          ! Invalidate all i-cache blocks
	sync					!Required for G4 processors
	isync
	blr                                     ! Return
	endproc.__code_synchronization:

! prototype int jitPPCSmpAtomicIncrement(int32_t *value);
#ifdef AIXPPC
.jitPPCSmpAtomicIncrement:
	.function .jitPPCSmpAtomicIncrement,startproc.jitPPCSmpAtomicIncrement,16,0,(endproc.jitPPCSmpAtomicIncrement-startproc.jitPPCSmpAtomicIncrement)
#elif defined(LINUXPPC64)
FUNC_LABEL(jitPPCSmpAtomicIncrement):
#else
jitPPCSmpAtomicIncrement:
#endif
	startproc.jitPPCSmpAtomicIncrement:
	mr      r4, r3
L.tryNextValue:
	lwarx   r3, 0, r4
	addi    r3, r3, 1
	stwcx.  r3, 0, r4
	bclr    BO_IF, CR0_EQ
	b       L.tryNextValue
	endproc.jitPPCSmpAtomicIncrement:

#ifdef AIXPPC
.jitPPCDataCacheBlockZero:
	.function .jitPPCDataCacheBlockZero,startproc.jitPPCDataCacheBlockZero,16,0,(endproc.jitPPCDataCacheBlockZero-startproc.jitPPCDataCacheBlockZero)
#elif defined(LINUXPPC64)
FUNC_LABEL(jitPPCDataCacheBlockZero):
#else
jitPPCDataCacheBlockZero:
#endif
	startproc.jitPPCDataCacheBlockZero:
   dcbz    0, r3
   bclr    BO_ALWAYS, CR0_LT
   endproc.jitPPCDataCacheBlockZero:

#ifdef AIXPPC
._tr_try_lock:
	.function ._tr_try_lock, startproc._tr_try_lock, 16, 0, (endproc._tr_try_lock-startproc._tr_try_lock)
	startproc._tr_try_lock:
#elif defined(LINUXPPC64)
FUNC_LABEL(_tr_try_lock):
	startproc._tr_try_lock:
#else
_tr_try_lock:
._tr_try_lock:
#endif
	mr	r7, r3
	li	r3, 0
	lwarx	r6, r7, r3
	cmpl	cr0, 0, r6, r4
	bc	BO_IF_NOT, CR0_EQ, L.try_done
	stwcx.	r5, r7, r3
	bc	BO_IF_NOT, CR0_EQ, L.try_done
	li	r3, 1
L.try_done:
	isync
	blr
	endproc._tr_try_lock:

#ifdef AIXPPC
._tr_unlock:
	.function ._tr_unlock, startproc._tr_unlock, 16, 0, (endproc._tr_unlock-startproc._tr_unlock)
	startproc._tr_unlock:
#elif defined(LINUXPPC64)
FUNC_LABEL(_tr_unlock):
	startproc._tr_unlock:
#else
_tr_unlock:
._tr_unlock:
#endif
	sync
	stw     r5, 0(r3)
	li	r3, 1
	blr
	endproc._tr_unlock:

! .data section
#ifdef AIXPPC
	.toc

	.csect	  _tr_try_lock{DS}
	ADDR	  ._tr_try_lock
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  _tr_try_lock{DS}

	.csect	  _tr_unlock{DS}
	ADDR	  ._tr_unlock
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  _tr_unlock{DS}

	.csect	  jitPPCUniLockCheck{DS}
	ADDR	  .jitPPCUniLockCheck
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  jitPPCUniLockCheck{DS}

	.csect	  jitPPCUniLockUpdate{DS}
	ADDR	  .jitPPCUniLockUpdate
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  jitPPCUniLockUpdate{DS}

	.csect	  jitPPCSmpLockCheck{DS}
	ADDR	  .jitPPCSmpLockCheck
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  jitPPCSmpLockCheck{DS}

	.csect	  jitPPCSmpLockUpdate{DS}
	ADDR	  .jitPPCSmpLockUpdate
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  jitPPCSmpLockUpdate{DS}

	.csect	  jitPPCCodeSync{DS}
	ADDR	  .jitPPCCodeSync
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  jitPPCCodeSync{DS}

	.csect	  jitPPCSmpAtomicIncrement{DS}
	ADDR	  .jitPPCSmpAtomicIncrement
	ADDR 	  TOC{TC0}
	ADDR	  0x00000000
!End	csect	  jitPPCSmpAtomicIncrement{DS}

#elif defined(LINUXPPC64)
	.section ".toc", "wa"
	.align   4
.tocL_tr_try_lock:
        .tc       _tr_try_lock[TC], _tr_try_lock
.tocL_tr_unlock:
        .tc       _tr_unlock[TC], _tr_unlock
.tocL_jitPPCUniLockCheck:
        .tc       jitPPCUniLockCheck[TC], jitPPCUniLockCheck
.tocL_jitPPCUniLockUpdate:
        .tc       jitPPCUniLockUpdate[TC], jitPPCUniLockUpdate
.tocL_jitPPCSmpLockCheck:
        .tc       jitPPCSmpLockCheck[TC], jitPPCSmpLockCheck
.tocL_jitPPCSmpLockUpdate:
        .tc       jitPPCSmpLockUpdate[TC], jitPPCSmpLockUpdate
.tocL_jitPPCCodeSync:
        .tc       jitPPCCodeSync[TC], jitPPCCodeSync
.tocL_jitPPCSmpAtomicIncrement:
        .tc       jitPPCSmpAtomicIncrement[TC], jitPPCSmpAtomicIncrement
.tocL_jitPPCDataCacheBlockZero:
        .tc jitPPCDataCacheBlockZero[TC], jitPPCDataCacheBlockZero

#if !defined(__LITTLE_ENDIAN__)
        .section  ".opd", "wa"
        .align    3
	.globl    _tr_try_lock
	.size     _tr_try_lock,24
_tr_try_lock:
	.quad     ._tr_try_lock
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    _tr_unlock
	.size     _tr_unlock,24
_tr_unlock:
	.quad     ._tr_unlock
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    jitPPCUniLockCheck
	.size     jitPPCUniLockCheck,24
jitPPCUniLockCheck:
	.quad     .jitPPCUniLockCheck
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    jitPPCUniLockUpdate
	.size     jitPPCUniLockUpdate,24
jitPPCUniLockUpdate:
	.quad     .jitPPCUniLockUpdate
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    jitPPCSmpLockCheck
	.size     jitPPCSmpLockCheck,24
jitPPCSmpLockCheck:
	.quad     .jitPPCSmpLockCheck
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    jitPPCSmpLockUpdate
	.size     jitPPCSmpLockUpdate,24
jitPPCSmpLockUpdate:
	.quad     .jitPPCSmpLockUpdate
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    jitPPCCodeSync
	.size     jitPPCCodeSync,24
jitPPCCodeSync:
	.quad     .jitPPCCodeSync
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    jitPPCSmpAtomicIncrement
	.size     jitPPCSmpAtomicIncrement,24
jitPPCSmpAtomicIncrement:
	.quad     .jitPPCSmpAtomicIncrement
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

        .globl jitPPCDataCacheBlockZero
        .size jitPPCDataCacheBlockZero,24
jitPPCDataCacheBlockZero:
        .quad .jitPPCDataCacheBlockZero
        .quad .TOC.@tocbase
        .long 0x00000000
        .long 0x00000000
#endif
#endif
