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

#ifdef TR_TARGET_64BIT
#define laddr	ld
#define laddrx	ldx
#define laddru	ldu
#define staddr	std
#define staddrx stdx
#define staddru stdu
#define ShiftLI sldi
#define ShiftRI srdi
#define ADDR	.llong
#define CmpAddr	1
#define ALen	8
#define FlagOffsetModifier 0

#else
#define laddr	lwz
#define laddrx	lwzx
#define laddru	lwzu
#define staddr	stw
#define staddrx stwx
#define staddru stwu
#define ShiftLI slwi
#define ShiftRI srwi
#define ADDR	.long
#define CmpAddr	0
#define ALen	4
#define FlagOffsetModifier 0

#endif

#define FPREG(x) x
#define GPREG(x) x
#define CRREG(x) x
#define VPREG(x) x

# define fp0 FPREG(0)
# define fp1 FPREG(1)
# define fp2 FPREG(2)
# define fp3 FPREG(3)
# define fp4 FPREG(4)
# define fp5 FPREG(5)
# define fp6 FPREG(6)
# define fp7 FPREG(7)
# define fp8 FPREG(8)
# define fp9 FPREG(9)
# define fp10 FPREG(10)
# define fp11 FPREG(11)
# define fp12 FPREG(12)
# define fp13 FPREG(13)
# define fp14 FPREG(14)
# define fp15 FPREG(15)
# define fp16 FPREG(16)
# define fp17 FPREG(17)
# define fp18 FPREG(18)
# define fp19 FPREG(19)
# define fp20 FPREG(20)
# define fp21 FPREG(21)
# define fp22 FPREG(22)
# define fp23 FPREG(23)
# define fp24 FPREG(24)
# define fp25 FPREG(25)
# define fp26 FPREG(26)
# define fp27 FPREG(27)
# define fp28 FPREG(28)
# define fp29 FPREG(29)
# define fp30 FPREG(30)
# define fp31 FPREG(31)

# define r0 GPREG(0)
# define r1 GPREG(1)
! Used only when r2 is not actually the TOC
# define r2 GPREG(2)
# define r3 GPREG(3)
# define r4 GPREG(4)
# define r5 GPREG(5)
# define r6 GPREG(6)
# define r7 GPREG(7)
# define r8 GPREG(8)
# define r9 GPREG(9)
# define r10 GPREG(10)
# define r11 GPREG(11)
# define r12 GPREG(12)
# define r13 GPREG(13)
# define r14 GPREG(14)
# define r15 GPREG(15)
# define r16 GPREG(16)
# define r17 GPREG(17)
# define r18 GPREG(18)
# define r19 GPREG(19)
# define r20 GPREG(20)
# define r21 GPREG(21)
# define r22 GPREG(22)
# define r23 GPREG(23)
# define r24 GPREG(24)
# define r25 GPREG(25)
# define r26 GPREG(26)
# define r27 GPREG(27)
# define r28 GPREG(28)
# define r29 GPREG(29)
# define r30 GPREG(30)
# define r31 GPREG(31)

# define cr0 CRREG(0)
# define cr1 CRREG(1)
# define cr2 CRREG(2)
# define cr3 CRREG(3)
# define cr4 CRREG(4)
# define cr5 CRREG(5)
# define cr6 CRREG(6)
# define cr7 CRREG(7)

# define vr0 VPREG(0)
# define vr1 VPREG(1)
# define vr2 VPREG(2)
# define vr3 VPREG(3)
# define vr4 VPREG(4)
# define vr5 VPREG(5)
# define vr6 VPREG(6)
# define vr7 VPREG(7)
# define vr8 VPREG(8)
# define vr9 VPREG(9)
# define vr10 VPREG(10)
# define vr11 VPREG(11)
# define vr12 VPREG(12)
# define vr13 VPREG(13)
# define vr14 VPREG(14)
# define vr15 VPREG(15)
# define vr16 VPREG(16)
# define vr17 VPREG(17)
# define vr18 VPREG(18)
# define vr19 VPREG(19)
# define vr20 VPREG(20)
# define vr21 VPREG(21)
# define vr22 VPREG(22)
# define vr23 VPREG(23)
# define vr24 VPREG(24)
# define vr25 VPREG(25)
# define vr26 VPREG(26)
# define vr27 VPREG(27)
# define vr28 VPREG(28)
# define vr29 VPREG(29)
# define vr30 VPREG(30)
# define vr31 VPREG(31)

# define vs0 0
# define vs1 1
# define vs2 2
# define vs3 3
# define vs4 4
# define vs5 5
# define vs6 6
# define vs7 7
# define vs8 8
# define vs9 9
# define vs10 10
# define vs11 11
# define vs12 12
# define vs13 13
# define vs14 14
# define vs15 15
# define vs16 16
# define vs17 17
# define vs18 18
# define vs19 19
# define vs20 20
# define vs21 21
# define vs22 22
# define vs23 23
# define vs24 24
# define vs25 25
# define vs26 26
# define vs27 27
# define vs28 28
# define vs29 29
# define vs30 30
# define vs31 31
# define vs32 32
# define vs33 33
# define vs34 34
# define vs35 35
# define vs36 36
# define vs37 37
# define vs38 38
# define vs39 39
# define vs40 40
# define vs41 41
# define vs42 42
# define vs43 43
# define vs44 44
# define vs45 45
# define vs46 46
# define vs47 47
# define vs48 48
# define vs49 49
# define vs50 50
# define vs51 51
# define vs52 52
# define vs53 53
# define vs54 54
# define vs55 55
# define vs56 56
# define vs57 57
# define vs58 58
# define vs59 59
# define vs60 60
# define vs61 61
# define vs62 62
# define vs63 63

	.set RTOC_SAVE_OFFSET,8;

! Branch constants
	.set BO_dCTR_NZERO_AND_NOT,0; .set BO_dCTR_NZERO_AND_NOT_1,1
	.set BO_dCTR_ZERO_AND_NOT,2; .set BO_dCTR_ZERO_AND_NOT_1,3
	.set BO_IF_NOT,4; .set BO_IF_NOT_1,5; .set BO_IF_NOT_2,6
	.set BO_IF_NOT_3,7; .set BO_dCTR_NZERO_AND,8; .set BO_dCTR_NZERO_AND_1,9
	.set BO_dCTR_ZERO_AND,10; .set BO_dCTR_ZERO_AND_1,11; .set BO_IF,12
	.set BO_IF_1,13; .set BO_IF_2,14; .set BO_IF_3,15; .set BO_dCTR_NZERO,16
	.set BO_dCTR_NZERO_1,17; .set BO_dCTR_ZERO,18; .set BO_dCTR_ZERO_1,19
	.set BO_ALWAYS,20; .set BO_ALWAYS_1,21; .set BO_ALWAYS_2,22
	.set BO_ALWAYS_3,23; .set BO_dCTR_NZERO_8,24; .set BO_dCTR_NZERO_9,25
	.set BO_dCTR_ZERO_8,26; .set BO_dCTR_ZERO_9,27; .set BO_ALWAYS_8,28
	.set BO_ALWAYS_9,29; .set BO_ALWAYS_10,30; .set BO_ALWAYS_11,31
	.set CR0_LT,0; .set CR0_GT,1; .set CR0_EQ,2; .set CR0_SO,3
	.set CR1_FX,4; .set CR1_FEX,5; .set CR1_VX,6; .set CR1_OX,7
	.set CR2_LT,8; .set CR2_GT,9; .set CR2_EQ,10; .set CR2_SO,11
	.set CR3_LT,12; .set CR3_GT,13; .set CR3_EQ,14; .set CR3_SO,15
	.set CR4_LT,16; .set CR4_GT,17; .set CR4_EQ,18; .set CR4_SO,19
	.set CR5_LT,20; .set CR5_GT,21; .set CR5_EQ,22; .set CR5_SO,23
	.set CR6_LT,24; .set CR6_GT,25; .set CR6_EQ,26; .set CR6_SO,27
	.set CR7_LT,28; .set CR7_GT,29; .set CR7_EQ,30; .set CR7_SO,31

! VM Register Use
#ifdef TR_TARGET_64BIT
	#define J9_BP r15
	#define J9VM_STRUCT r15
	#define J9SP r14
	#define RTOC r2
	#define J9_TOC r16
#else
	#define J9_BP r15
	#define J9VM_STRUCT r13
	#define J9SP r14
#if defined(LINUXPPC)
	#define RTOC r12
#else
	#define RTOC r2
#endif
	#define J9_TOC r16
#endif

! Special Registers
#define LR 8
#define CTR 9

#define SP r1
#define RSTACK SP


#define LABEL_DIFF(x) x

#ifdef LINUX
		! In case we do not have assembler support for VMX/VSX instructions, use these macros to generate them
		! These are grouped according to their respective categories.

        .macro lvsl     vrt, ra, rb
        .int 0x7c00000c | \vrt << 21 | \ra << 16 | \rb << 11
        .endm
        .macro lvsr      vrt, ra, rb
        .int 0x7C00004C | \vrt << 21 | \ra << 16 | \rb << 11
        .endm

        .macro lvx      vrt, ra, rb
        .int 0x7c0000ce | \vrt << 21 | \ra << 16 | \rb << 11
        .endm
        .macro lxvw4x   xt, ra, rb
        .int 0x7c000618 | (\xt & 31) << 21 | \ra << 16 | \rb << 11 | (\xt & 32) >> 5
        .endm
        .macro lxvw4ux  xt, ra, rb
        .int 0x7c000658 | (\xt & 31) << 21 | \ra << 16 | \rb << 11 | (\xt & 32) >> 5
        .endm
        .macro stxvw4x  xt, ra, rb
        .int 0x7c000718 | (\xt & 31) << 21 | \ra << 16 | \rb << 11 | (\xt & 32) >> 5
        .endm
        .macro stxvw4ux xt, ra, rb
        .int 0x7c000758 | (\xt & 31) << 21 | \ra << 16 | \rb << 11 | (\xt & 32) >> 5
        .endm
        .macro stxsdx   xs, ra, rb
        .int 0x7c000598 | (\xs & 31) << 21 | \ra << 16 | \rb << 11 | (\xs & 32) >> 5
        .endm

        .macro vaddubs vrt, vra, vrb
        .int 0x10000200 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm

        .macro vpkuhus  vrt, vra, vrb
        .int 0x1000008e | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vupkhsb vrt, vrb
        .int 0x1000020E | \vrt << 21 | \vrb << 11
        .endm
        .macro vupklsb vrt, vrb
        .int 0x1000028E | \vrt << 21 | \vrb << 11
        .endm

        .macro vmrghb   vrt, vra, vrb
        .int 0x1000000C | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vmrglb   vrt, vra, vrb
        .int 0x1000010C | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm

        .macro vperm    vrt, vra, vrb, vrc
        .int 0x1000002b | \vrt << 21 | \vra << 16 | \vrb << 11 | \vrc << 6
        .endm
        .macro vsel    vrt, vra, vrb, vrc
        .int 0x1000002a | \vrt << 21 | \vra << 16 | \vrb << 11 | \vrc << 6
        .endm

        .macro vcmpequw_r    vrt, vra, vrb
        .int 0x10000086 | 0x400 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vcmpequh_r    vrt, vra, vrb
        .int 0x10000046 | 0x400 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vcmpgtub vrt, vra, vrb
        .int 0x10000206 | \vrt << 21 | \vra << 16 | \vrb << 11 | 0 << 10
        .endm

        .macro vslb     vrt, vra, vrb
        .int 0x10000104 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vslh     vrt, vra, vrb
        .int 0x10000144 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vrlb     vrt, vra, vrb
        .int 0x10000004 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vrlw     vrt, vra, vrb
        .int 0x10000084 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vrlh     vrt, vra, vrb
        .int 0x10000044 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm

        .macro vspltb vrt, vrb, uim
        .int 0x1000020C | \vrt << 21 | (\uim & 15) << 16 | \vrb << 11
        .endm
        .macro vsplth vrt, vrb, uim
        .int 0x1000024C | \vrt << 21 | (\uim &  7) << 16 | \vrb << 11
        .endm
        .macro vspltisb vrt, sim
        .int 0x1000030C | \vrt << 21 | (\sim & 31) << 16
        .endm
        .macro vspltish vrt, sim
        .int 0x1000034C | \vrt << 21 | (\sim & 31) << 16
        .endm
        .macro vspltisw vrt, sim
        .int 0x1000038C | \vrt << 21 | (\sim & 31) << 16
        .endm

        .macro vand      vrt, ra, rb
        .int 0x10000404 | \vrt << 21 | \ra << 16 | \rb << 11
        .endm
        .macro vandc      vrt, ra, rb
        .int 0x10000444 | \vrt << 21 | \ra << 16 | \rb << 11
        .endm
        .macro vor      vrt, ra, rb
        .int 0x10000484 | \vrt << 21 | \ra << 16 | \rb << 11
        .endm
        .macro vxor      vrt, ra, rb
        .int 0x100004C4 | \vrt << 21 | \ra << 16 | \rb << 11
        .endm

        .macro xxlor    xt, xa, xb
        .int 0xf0000490 | (\xt & 31) << 21 | (\xa & 31) << 16 | (\xb & 31) << 11 | (\xa & 32) >> 3 | (\xb & 32) >> 4 | (\xt & 32) >> 5
        .endm
        .macro xxspltw  xt, xb, uim
        .int 0xf0000290 | (\xt & 31) << 21 | (\uim & 3) << 16 | (\xb & 31) << 11 | (\xb & 32) >> 4 | (\xt & 32) >> 5
        .endm
        .macro xxpermdi xt, xa, xb, dm
        .int 0xf0000050 | (\xt & 31) << 21 | (\xa & 31) << 16 | (\xb & 31) << 11 | \dm << 8 | (\xa & 32) >> 3 | (\xb & 32) >> 4 | (\xt & 32) >> 5
        .endm

        .macro vcipher   vrt, vra, vrb
        .int 0x10000508 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vcipherlast   vrt, vra, vrb
        .int 0x10000509 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vncipher   vrt, vra, vrb
        .int 0x10000548 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vncipherlast   vrt, vra, vrb
        .int 0x10000549 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vsbox   vrt, vra
        .int 0x100005c8 | \vrt << 21 | \vra << 16
        .endm

        .macro lxvd2x   xt, ra, rb
        .int 0x7C000698 | (\xt & 31) << 21 | \ra << 16 | \rb << 11 | (\xt & 32) >> 5
        .endm
        .macro stxvd2x   xt, ra, rb
        .int 0x7C000798 | (\xt & 31) << 21 | \ra << 16 | \rb << 11 | (\xt & 32) >> 5
        .endm
        .macro vspltw   vrt, vra, uim
        .int 0x1000028C | \vrt << 21 | (\uim & 3) << 16 | \vra << 11
        .endm
        .macro vnor      vrt, vra, vrb
        .int 0x10000504 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vadduws vrt, vra, vrb
        .int 0x10000280 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vadduwm vrt, vra, vrb
        .int 0x10000080 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vaddudm vrt, vra, vrb
        .int 0x100000C0 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vmrgew vrt, vra, vrb
        .int 0x1000078C | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vshasigmad vrt, vra, st, six
        .int 0x100006C2 | \vrt << 21 | \vra << 16 | (\st & 1) << 15 | (\six & 15) << 11
        .endm
        .macro vshasigmaw vrt, vra, st, six
        .int 0x10000682 | \vrt << 21 | \vra << 16 | (\st & 1) << 15 | (\six & 15) << 11
        .endm

        .macro vmulouw vrt, vra, vrb
        .int 0x10000088 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vmuleuw vrt, vra, vrb
        .int 0x10000288 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm

        .macro vadduqm vrt, vra, vrb
        .int 0x10000100 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vaddcuq vrt, vra, vrb
        .int 0x10000140 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm

        .macro vaddeuqm vrt, vra, vrb, vrc
        .int 0x1000003C | \vrt << 21 | \vra << 16 | \vrb << 11 | \vrc << 6
        .endm
        .macro vaddecuq vrt, vra, vrb, vrc
        .int 0x1000003D | \vrt << 21 | \vra << 16 | \vrb << 11 | \vrc << 6
        .endm

        .macro vsubuqm vrt, vra, vrb
        .int 0x10000500 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm
        .macro vsubcuq vrt, vra, vrb
        .int 0x10000540 | \vrt << 21 | \vra << 16 | \vrb << 11
        .endm

        .macro vsubeuqm vrt, vra, vrb, vrc
        .int 0x1000003E | \vrt << 21 | \vra << 16 | \vrb << 11 | \vrc << 6
        .endm
        .macro vsubecuq vrt, vra, vrb, vrc
        .int 0x1000003F | \vrt << 21 | \vra << 16 | \vrb << 11 | \vrc << 6
        .endm

        .macro vpmsumw  vrt, vra, vrb
        .int 0x10000488 | (\vrt & 31) << 21 | (\vra & 31) << 16 | (\vrb & 31) << 11 | (\vra & 32) >> 3 | (\vrb & 32) >> 4 | (\vrt & 32) >> 5
        .endm
        .macro vpmsumd  vrt, vra, vrb
        .int 0x100004c8 | (\vrt & 31) << 21 | (\vra & 31) << 16 | (\vrb & 31) << 11 | (\vra & 32) >> 3 | (\vrb & 32) >> 4 | (\vrt & 32) >> 5
        .endm

        .macro mfvsrd   ra, vrs
        .int 0x7c000066 | \ra << 16 | (\vrs & 31) << 21 | (\vrs & 32) >> 5
        .endm
        .macro mtvsrd   vrt, ra
        .int 0x7c000166 | \ra << 16 | (\vrt & 31) << 21 | (\vrt & 32) >> 5
        .endm
        .macro mtvsrwz  vrt, ra
        .int 0x7c0001e6 | \ra << 16 | (\vrt & 31) << 21 | (\vrt & 32) >> 5
        .endm
#endif

!The record form instructions break the Linux Assembler, this is the workaround for it
#ifdef AIXPPC
#define vcmpequw_r vcmpequw.
#define vcmpequh_r vcmpequh.
#define andi_r andi.
#endif

#ifdef AIXPPC
#define VCIPHER(vrt,vra,vrb)            .long 0x10000508 | vrt < 21 | vra < 16 | vrb < 11
#define VCIPHERLAST(vrt,vra,vrb)        .long 0x10000509 | vrt < 21 | vra < 16 | vrb < 11
#define VNCIPHER(vrt,vra,vrb)           .long 0x10000548 | vrt < 21 | vra < 16 | vrb < 11
#define VNCIPHERLAST(vrt,vra,vrb)       .long 0x10000549 | vrt < 21 | vra < 16 | vrb < 11
#define VSBOX(vrt,vra)                  .long 0x100005c8 | vrt < 21 | vra < 16
#define VSPLTISB(vrt,sim)               .long 0x1000030C | vrt < 21 | sim < 16
#define VSHASIGMAD(vrt,vra,st,six)      .long 0x100006C2 | vrt < 21 | vra < 16 | (st & 1) < 15 | (six & 15) < 11
#define VSHASIGMAW(vrt,vra,st,six)      .long 0x10000682 | vrt < 21 | vra < 16 | (st & 1) < 15 | (six & 15) < 11
#define LXVD2X(xt, ra, rb)              .long 0x7C000698 | (xt & 31) < 21 | ra < 16 | rb < 11 | (xt & 32) > 5
#define STXVD2X(xt, ra, rb)             .long 0x7C000798 | (xt & 31) < 21 | ra < 16 | rb < 11 | (xt & 32) > 5
#define VADDUDM(vrt, vra, vrb)          .long 0x100000C0 | vrt < 21 | vra < 16 | vrb < 11
#define VMRGEW(vrt, vra, vrb)           .long 0x1000078C | vrt < 21 | vra < 16 | vrb < 11
#define VMULOUW(vrt, vra, vrb)          .long 0x10000088 | vrt < 21 | vra < 16 | vrb < 11
#define VMULEUW(vrt, vra, vrb)          .long 0x10000288 | vrt < 21 | vra < 16 | vrb < 11
#define VADDUQM(vrt, vra, vrb)          .long 0x10000100 | vrt < 21 | vra < 16 | vrb < 11
#define VADDCUQ(vrt, vra, vrb)          .long 0x10000140 | vrt < 21 | vra < 16 | vrb < 11
#define VADDEUQM(vrt, vra, vrb, vrc)    .long 0x1000003C | vrt < 21 | vra < 16 | vrb < 11 | vrc < 6
#define VADDECUQ(vrt, vra, vrb, vrc)    .long 0x1000003D | vrt < 21 | vra < 16 | vrb < 11 | vrc < 6
#define VSUBUQM(vrt, vra, vrb)          .long 0x10000500 | vrt < 21 | vra < 16 | vrb < 11
#define VSUBCUQ(vrt, vra, vrb)          .long 0x10000540 | vrt < 21 | vra < 16 | vrb < 11
#define VSUBEUQM(vrt, vra, vrb, vrc)    .long 0x1000003E | vrt < 21 | vra < 16 | vrb < 11 | vrc < 6
#define VSUBECUQ(vrt, vra, vrb, vrc)    .long 0x1000003F | vrt < 21 | vra < 16 | vrb < 11 | vrc < 6
#define VPMSUMW(vrt,vra,vrb)            .long 0x10000488 |(vrt & 31) < 21 |(vra & 31) < 16 |(vrb & 31) < 11 |(vra & 32) > 3 |(vrb & 32) > 4 |(vrt & 32) > 5
#define VPMSUMD(vrt,vra,vrb)            .long 0x100004c8 |(vrt & 31) < 21 |(vra & 31) < 16 |(vrb & 31) < 11 |(vra & 32) > 3 |(vrb & 32) > 4 |(vrt & 32) > 5
#define MFVSRD(ra,vrs)                  .long 0x7c000066 | ra < 16 | (vrs & 31) < 21 | (vrs & 32) > 5
#define MTVSRD(vrt,ra)                  .long 0x7c000166 | ra < 16 | (vrt & 31) < 21 | (vrt & 32) > 5
#define MTVSRWZ(vrt,ra)                 .long 0x7c0001e6 | ra < 16 | (vrt & 31) < 21 | (vrt & 32) > 5
#else
#define VCIPHER(vrt,vra,vrb)            vcipher         vrt, vra, vrb
#define VCIPHERLAST(vrt,vra,vrb)        vcipherlast     vrt, vra, vrb
#define VNCIPHER(vrt,vra,vrb)           vncipher        vrt, vra, vrb
#define VNCIPHERLAST(vrt,vra,vrb)       vncipherlast    vrt, vra, vrb
#define VSBOX(vrt,vra)                  vsbox           vrt, vra, vrb
#define VSPLTISB(vrt,sim)               vspltisb         vrt, sim
#define VSHASIGMAD(vrt,vra,st,six)      vshasigmad       vrt, vra, st, six
#define VSHASIGMAW(vrt,vra,st,six)      vshasigmaw       vrt, vra, st, six
#define LXVD2X(xt, ra, rb)              lxvd2x   xt, ra, rb
#define STXVD2X(xt, ra, rb)             stxvd2x   xt, ra, rb
#define VADDUDM(vrt, vra, vrb)          vaddudm vrt, vra, vrb
#define VMRGEW(vrt, vra, vrb)           vmrgew vrt, vra, vrb
#define VMULOUW(vrt, vra, vrb)          vmulouw vrt, vra, vrb
#define VMULEUW(vrt, vra, vrb)          vmuleuw vrt, vra, vrb
#define VADDUQM(vrt, vra, vrb)          vadduqm vrt, vra, vrb
#define VADDCUQ(vrt, vra, vrb)          vaddcuq vrt, vra, vrb
#define VADDEUQM(vrt, vra, vrb, vrc)    vaddeuqm vrt, vra, vrb, vrc
#define VADDECUQ(vrt, vra, vrb, vrc)    vaddecuq vrt, vra, vrb, vrc
#define VSUBUQM(vrt, vra, vrb)          vsubuqm vrt, vra, vrb
#define VSUBCUQ(vrt, vra, vrb)          vsubcuq vrt, vra, vrb
#define VSUBEUQM(vrt, vra, vrb, vrc)    vsubeuqm vrt, vra, vrb, vrc
#define VSUBECUQ(vrt, vra, vrb, vrc)    vsubecuq vrt, vra, vrb, vrc
#define VPMSUMW(vrt,vra,vrb)            vpmsumw         vrt, vra, vrb
#define VPMSUMD(vrt,vra,vrb)            vpmsumd         vrt, vra, vrb
#define MFVSRD(ra,vrs)                  mfvsrd          ra, vrs
#define MTVSRD(vrt,ra)                  mtvsrd          vrt, ra
#define MTVSRWZ(vrt,ra)                 mtvsrwz         vrt, ra
#endif

#define VSHASIGMA_256_s0(vrt,vra)   VSHASIGMAW(vrt,vra,0,0)
#define VSHASIGMA_256_s1(vrt,vra)   VSHASIGMAW(vrt,vra,0,15)
#define VSHASIGMA_256_S0(vrt,vra)   VSHASIGMAW(vrt,vra,1,0)
#define VSHASIGMA_256_S1(vrt,vra)   VSHASIGMAW(vrt,vra,1,15)

#define VSHASIGMA_512_s0(vrt,vra)   VSHASIGMAD(vrt,vra,0,0)
#define VSHASIGMA_512_s1(vrt,vra)   VSHASIGMAD(vrt,vra,0,15)
#define VSHASIGMA_512_S0(vrt,vra)   VSHASIGMAD(vrt,vra,1,0)
#define VSHASIGMA_512_S1(vrt,vra)   VSHASIGMAD(vrt,vra,1,15)

#if defined(LINUXPPC64)
#if defined(__LITTLE_ENDIAN__)
#define FUNC_LABEL(x)	x
#else
#define FUNC_LABEL(x)	.x
#endif
#endif
