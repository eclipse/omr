/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "codegen/InstOpCode.hpp"

const OMR::ARM64::InstOpCode::OpCodeBinaryEntry OMR::ARM64::InstOpCode::binaryEncodings[ARM64NumOpCodes] =
{
//		BINARY			Opcode    	Opcode		comments
/* UNALLOCATED */
		0x00000000,	/* BAD       	bad	invalid operation */
/* Branch,exception generation and system Instruction */
	/* Compare _ Branch (immediate) */
		0x34000000,	/* CBZ       	cbzw	 */
		0x35000000,	/* CBNZ      	cbnzw	 */
		0xB4000000,	/* CBZ       	cbzx	 */
		0xB5000000,	/* CBNZ      	cbnzx	 */
	/* Test bit & branch (immediate) */
		0x36000000,	/* TBZ       	tbz	 */
		0x37000000,	/* TBNZ      	tbnz	 */
	/* Conditional branch (immediate) */
		0x54000000,	/* B_cond    	b_cond	 */
	/* Exception generation */
		0xD4200000,	/* BRK       	brkarm64	AArch64 Specific BRK */
	/* System */
	/* Unconditional branch (register) */
		0xD61F0000,	/* BR        	br	 */
		0xD63F0000,	/* BLR       	blr	 */
		0xD65F0000,	/* RET       	ret	 */
	/* Unconditional branch (immediate) */
		0x14000000,	/* B         	b	 */
		0x94000000,	/* BL        	bl	 */
/* Loads and stores */
	/* Load/store exclusive */
		0x08000000,	/* STXRB     	stxrb	 */
		0x08008000,	/* STLXRB    	stlxrb	 */
		0x08400000,	/* LDXRB     	ldxrb	 */
		0x08408000,	/* LDAXRB    	ldaxrb	 */
		0x08808000,	/* STLRB     	stlrb	 */
		0x08C08000,	/* LDARB     	ldarb	 */
		0x48000000,	/* STXRH     	stxrh	 */
		0x48008000,	/* STLXRH    	stlxrh	 */
		0x48400000,	/* LDXRH     	ldxrh	 */
		0x48408000,	/* LDAXRH    	ldaxrh	 */
		0x48808000,	/* STLRH     	stlrh	 */
		0x48C08000,	/* LDARH     	ldarh	 */
		0x88000000,	/* STXR      	stxrw	 */
		0x88008000,	/* STLXR     	stlxrw	 */
		0x88200000,	/* STXP      	stxpw	 */
		0x88208000,	/* STLXP     	stlxpw	 */
		0x88400000,	/* LDXR      	ldxrw	 */
		0x88408000,	/* LDAXR     	ldaxrw	 */
		0x88600000,	/* LDXP      	ldxpw	 */
		0x88608000,	/* LDAXP     	ldaxpw	 */
		0x88808000,	/* STLR      	stlrw	 */
		0x88C08000,	/* LDAR      	ldarw	 */
		0xC8000000,	/* STXR      	stxrx	 */
		0xC8008000,	/* STLXR     	stlxrx	 */
		0xC8200000,	/* STXP      	stxpx	 */
		0xC8208000,	/* STLXP     	stlxpx	 */
		0xC8400000,	/* LDXR      	ldxrx	 */
		0xC8408000,	/* LDAXR     	ldaxrx	 */
		0xC8600000,	/* LDXP      	ldxpx	 */
		0xC8608000,	/* LDAXP     	ldaxpx	 */
		0xC8808000,	/* STLR      	stlrx	 */
		0xC8C08000,	/* LDAR      	ldarx	 */
	/* Load register (literal) */
		0x18000000,	/* LDR       	ldrw	 */
		0x1C000000,	/* LDR       	vldrs	 */
		0x58000000,	/* LDR       	ldrx	 */
		0x5C000000,	/* LDR       	vldrd	 */
		0x98000000,	/* LDRSW     	ldrsw	 */
		0x9C000000,	/* LDR       	vldrq	 */
		0xD8000000,	/* PRFM      	prfm	 */
	/* Load/store no-allocate pair (offset) */
		0x28000000,	/* STNP      	stnpw	 */
		0x28400000,	/* LDNP      	ldnpw	 */
		0x2C000000,	/* STNP      	vstnps	 */
		0x2C400000,	/* LDNP      	vldnps	 */
		0x6C000000,	/* STNP      	vstnpd	 */
		0x6C400000,	/* LDNP      	vldnpd	 */
		0xA8000000,	/* STNP      	stnpx	 */
		0xA8400000,	/* LDNP      	ldnpx	 */
		0xAC000000,	/* STNP      	vstnpq	 */
		0xAC400000,	/* LDNP      	vldnpq	 */
	/* Load/store register pair (post-indexed) */
		0x28800000,	/* STP       	stppostw	 */
		0x28C00000,	/* LDP       	ldppostw	 */
		0x2C800000,	/* STP       	vstpposts	 */
		0x2CC00000,	/* LDP       	vldpposts	 */
		0x68C00000,	/* LDPSW     	ldpswpost	 */
		0x6C800000,	/* STP       	vstppostd	 */
		0x6CC00000,	/* LDP       	vldppostd	 */
		0xA8800000,	/* STP       	stppostx	 */
		0xA8C00000,	/* LDP       	ldppostx	 */
		0xAC800000,	/* STP       	vstppostq	 */
		0xACC00000,	/* LDP       	vldppostq	 */
	/* Load/store register pair (offset) */
		0x29000000,	/* STP       	stpoffw	 */
		0x29400000,	/* LDP       	ldpoffw	 */
		0x2D000000,	/* STP       	vstpoffs	 */
		0x2D400000,	/* LDP       	vldpoffs	 */
		0x69400000,	/* LDPSW     	ldpswoff	 */
		0x6D000000,	/* STP       	vstpoffd	 */
		0x6D400000,	/* LDP       	vldpoffd	 */
		0xA9000000,	/* STP       	stpoffx	 */
		0xA9400000,	/* LDP       	ldpoffx	 */
		0xAD000000,	/* STP       	vstpoffq	 */
		0xAD400000,	/* LDP       	vldpoffq	 */
	/* Load/store register pair (pre-indexed) */
		0x29800000,	/* STP       	stpprew	 */
		0x29C00000,	/* LDP       	ldpprew	 */
		0x2D800000,	/* STP       	vstppres	 */
		0x2DC00000,	/* LDP       	vldppres	 */
		0x69C00000,	/* LDPSW     	ldpswpre	 */
		0x6D800000,	/* STP       	vstppred	 */
		0x6DC00000,	/* LDP       	vldppred	 */
		0xA9800000,	/* STP       	stpprex	 */
		0xA9C00000,	/* LDP       	ldpprex	 */
		0xAD800000,	/* STP       	vstppreq	 */
		0xADC00000,	/* LDP       	vldppreq	 */
	/* Load/store register (unscaled immediate) */
		0x38000000,	/* STURB     	sturb	 */
		0x38400000,	/* LDURB     	ldurb	 */
		0x38800000,	/* LDURSB    	ldursbx	 */
		0x38C00000,	/* LDURSB    	ldursbw	 */
		0x3C000000,	/* STUR      	vsturb	 */
		0x3C400000,	/* LDUR      	vldurb	 */
		0x3C800000,	/* STUR      	vsturq	 */
		0x3CC00000,	/* LDUR      	vldurq	 */
		0x78000000,	/* STURH     	sturh	 */
		0x78400000,	/* LDURH     	ldurh	 */
		0x78800000,	/* LDURSH    	ldurshx	 */
		0x78C00000,	/* LDURSH    	ldurshw	 */
		0x7C000000,	/* STUR      	vsturh	 */
		0x7C400000,	/* LDUR      	vldurh	 */
		0xB8000000,	/* STUR      	sturw	 */
		0xB8400000,	/* LDUR      	ldurw	 */
		0xB8800000,	/* LDURSW    	ldursw	 */
		0xBC000000,	/* STUR      	vsturs	 */
		0xBC400000,	/* LDUR      	vldurs	 */
		0xF8000000,	/* STUR      	sturx	 */
		0xF8400000,	/* LDUR      	ldurx	 */
		0xF8800000,	/* PRFUM     	prfum	 */
		0xFC000000,	/* STUR      	vsturd	 */
		0xFC400000,	/* LDUR      	vldurd	 */
	/* Load/store register (immediate post-indexed) */
		0x38000400,	/* STRB      	strbpost	 */
		0x38400400,	/* LDRB      	ldrbpost	 */
		0x38800400,	/* LDRSB     	ldrsbpostx	 */
		0x38C00400,	/* LDRSB     	ldrsbpostw	 */
		0x3C000400,	/* STR       	vstrpostb	 */
		0x3C400400,	/* LDR       	vldrpostb	 */
		0x3C800400,	/* STR       	vstrpostq	 */
		0x3CC00400,	/* LDR       	vldrpostq	 */
		0x78000400,	/* STRH      	strhpost	 */
		0x78400400,	/* LDRH      	ldrhpost	 */
		0x78800400,	/* LDRSH     	ldrshpostx	 */
		0x78C00400,	/* LDRSH     	ldrshpostw	 */
		0x7C000400,	/* STR       	vstrposth	 */
		0x7C400400,	/* LDR       	vldrposth	 */
		0xB8000400,	/* STR       	strpostw	 */
		0xB8400400,	/* LDR       	ldrpostw	 */
		0xB8800400,	/* LDRSW     	ldrswpost	 */
		0xBC000400,	/* STR       	vstrposts	 */
		0xBC400400,	/* LDR       	vldrposts	 */
		0xF8000400,	/* STR       	strpostx	 */
		0xF8400400,	/* LDR       	ldrpostx	 */
		0xFC000400,	/* STR       	vstrpostd	 */
		0xFC400400,	/* LDR       	vldrpostd	 */
	/* Load/store register (unprivileged) */
		0x38000800,	/* STTRB     	sttrb	 */
		0x38400800,	/* LDTRB     	ldtrb	 */
		0x38800800,	/* LDTRSB    	ldtrsbx	 */
		0x38C00800,	/* LDTRSB    	ldtrsbw	 */
		0x78000800,	/* STTRH     	sttrh	 */
		0x78400800,	/* LDTRH     	ldtrh	 */
		0x78800800,	/* LDTRSH    	ldtrshx	 */
		0x78C00800,	/* LDTRSH    	ldtrshw	 */
		0xB8000800,	/* STTR      	sttrw	 */
		0xB8400800,	/* LDTR      	ldtrw	 */
		0xB8800800,	/* LDTRSW    	ldtrsw	 */
		0xF8000800,	/* STTR      	sttrx	 */
		0xF8400800,	/* LDTR      	ldtrx	 */
	/* Load/store register (immediate pre-indexed) */
		0x38000C00,	/* STRB      	strbpre	 */
		0x38400C00,	/* LDRB      	ldrbpre	 */
		0x38800C00,	/* LDRSB     	ldrsbprex	 */
		0x38C00C00,	/* LDRSB     	ldrsbprew	 */
		0x3C000C00,	/* STR       	vstrpreb	 */
		0x3C400C00,	/* LDR       	vldrpreb	 */
		0x3C800C00,	/* STR       	vstrpreq	 */
		0x3CC00C00,	/* LDR       	vldrpreq	 */
		0x78000C00,	/* STRH      	strhpre	 */
		0x78400C00,	/* LDRH      	ldrhpre	 */
		0x78800C00,	/* LDRSH     	ldrshprex	 */
		0x78C00C00,	/* LDRSH     	ldrshprew	 */
		0x7C000C00,	/* STR       	vstrpreh	 */
		0x7C400C00,	/* LDR       	vldrpreh	 */
		0xB8000C00,	/* STR       	strprew	 */
		0xB8400C00,	/* LDR       	ldrprew	 */
		0xB8800C00,	/* LDRSW     	ldrswpre	 */
		0xBC000C00,	/* STR       	vstrpres	 */
		0xBC400C00,	/* LDR       	vldrpres	 */
		0xF8000C00,	/* STR       	strprex	 */
		0xF8400C00,	/* LDR       	ldrprex	 */
		0xFC000C00,	/* STR       	vstrpred	 */
		0xFC400C00,	/* LDR       	vldrpred	 */
	/* Load/store register (register offset) */
		0x38200800,	/* STRB      	strboff	 */
		0x38600800,	/* LDRB      	ldrboff	 */
		0x38A00800,	/* LDRSB     	ldrsboffx	 */
		0x38E00800,	/* LDRSB     	ldrsboffw	 */
		0x3C200800,	/* STR       	vstroffb	 */
		0x3C600800,	/* LDR       	vldroffb	 */
		0x3CA00800,	/* STR       	vstroffq	 */
		0x3CE00800,	/* LDR       	vldroffq	 */
		0x78200800,	/* STRH      	strhoff	 */
		0x78600800,	/* LDRH      	ldrhoff	 */
		0x78A00800,	/* LDRSH     	ldrshoffx	 */
		0x78E00800,	/* LDRSH     	ldrshoffw	 */
		0x7C200800,	/* STR       	vstroffh	 */
		0x7C600800,	/* LDR       	vldroffh	 */
		0xB8200800,	/* STR       	stroffw	 */
		0xB8600800,	/* LDR       	ldroffw	 */
		0xB8A00800,	/* LDRSW     	ldrswoff	 */
		0xBC200800,	/* STR       	vstroffs	 */
		0xBC600800,	/* LDR       	vldroffs	 */
		0xF8200800,	/* STR       	stroffx	 */
		0xF8600800,	/* LDR       	ldroffx	 */
		0xFC200800,	/* STR       	vstroffd	 */
		0xFC600800,	/* LDR       	vldroffd	 */
		0xF8A00800,	/* PRFM      	prfmoff	 */
	/* Load/store register (unsigned immediate) */
		0x39000000,	/* STRB      	strbimm	 */
		0x39400000,	/* LDRB      	ldrbimm	 */
		0x39800000,	/* LDRSB     	ldrsbimmx	 */
		0x39C00000,	/* LDRSB     	ldrsbimmw	 */
		0x3D000000,	/* STR       	vstrimmb	 */
		0x3D400000,	/* LDR       	vldrimmb	 */
		0x3D800000,	/* STR       	vstrimmq	 */
		0x3DC00000,	/* LDR       	vldrimmq	 */
		0x79000000,	/* STRH      	strhimm	 */
		0x79400000,	/* LDRH      	ldrhimm	 */
		0x79800000,	/* LDRSH     	ldrshimmx	 */
		0x79C00000,	/* LDRSH     	ldrshimmw	 */
		0x7D000000,	/* STR       	vstrimmh	 */
		0x7D400000,	/* LDR       	vldrimmh	 */
		0xB9000000,	/* STR       	strimmw	 */
		0xB9400000,	/* LDR       	ldrimmw	 */
		0xB9800000,	/* LDRSW     	ldrswimm	 */
		0xBD000000,	/* STR       	vstrimms	 */
		0xBD400000,	/* LDR       	vldrimms	 */
		0xF9000000,	/* STR       	strimmx	 */
		0xF9400000,	/* LDR       	ldrimmx	 */
		0xFD000000,	/* STR       	vstrimmd	 */
		0xFD400000,	/* LDR       	vldrimmd	 */
		0xF9800000,	/* PRFM      	prfmimm	 */
/* Data processing - Immediate */
	/* PC-rel. addressing */
		0x10000000,	/* ADR       	adr	 */
		0x90000000,	/* ADRP      	adrp	 */
	/* Add/subtract (immediate) */
		0x11000000,	/* ADD       	addimmw	 */
		0x31000000,	/* ADDS      	addsimmw	 */
		0x51000000,	/* SUB       	subimmw	 */
		0x71000000,	/* SUBS      	subsimmw	 */
		0x91000000,	/* ADD       	addimmx	 */
		0xB1000000,	/* ADDS      	addsimmx	 */
		0xD1000000,	/* SUB       	subimmx	 */
		0xF1000000,	/* SUBS      	subsimmx	 */
	/* Logical (immediate) */
		0x12000000,	/* AND       	andimmw	 */
		0x32000000,	/* ORR       	orrimmw	 */
		0x52000000,	/* EOR       	eorimmw	 */
		0x72000000,	/* ANDS      	andsimmw	 */
		0x92000000,	/* AND       	andimmx	 */
		0xB2000000,	/* ORR       	orrimmx	 */
		0xD2000000,	/* EOR       	eorimmx	 */
		0xF2000000,	/* ANDS      	andsimmx	 */
	/* Move wide (immediate) */
		0x12800000,	/* MOVN      	movnw	 */
		0x52800000,	/* MOVZ      	movzw	 */
		0x72800000,	/* MOVK      	movkw	 */
		0x92800000,	/* MOVN      	movnx	 */
		0xD2800000,	/* MOVZ      	movzx	 */
		0xF2800000,	/* MOVK      	movkx	 */
	/* Bitfield */
		0x13000000,	/* SBFM      	sbfmw	 */
		0x33000000,	/* BFM       	bfmw	 */
		0x53000000,	/* UBFM      	ubfmw	 */
		0x93400000,	/* SBFM      	sbfmx	 */
		0xB3400000,	/* BFM       	bfmx	 */
		0xD3400000,	/* UBFM      	ubfmx	 */
	/* Extract */
		0x13800000,	/* EXTR      	extrw	 */
		0x93C08000,	/* EXTR      	extrx	 */
/* Data Processing - register */
	/* Logical (shifted register) */
		0x0A000000,	/* AND       	andw	 */
		0x0A200000,	/* BIC       	bicw	 */
		0x2A000000,	/* ORR       	orrw	 */
		0x2A200000,	/* ORN       	ornw	 */
		0x4A000000,	/* EOR       	eorw	 */
		0x4A200000,	/* EON       	eonw	 */
		0x6A000000,	/* ANDS      	andsw	 */
		0x6A200000,	/* BICS      	bicsw	 */
		0x8A000000,	/* AND       	andx	 */
		0x8A200000,	/* BIC       	bicx	 */
		0xAA000000,	/* ORR       	orrx	 */
		0xAA200000,	/* ORN       	ornx	 */
		0xCA000000,	/* EOR       	eorx	 */
		0xCA200000,	/* EON       	eonx	 */
		0xEA000000,	/* ANDS      	andsx	 */
		0xEA200000,	/* BICS      	bicsx	 */
	/* Add/subtract (shifted register) */
		0x0B000000,	/* ADD       	addw	 */
		0x2B000000,	/* ADDS      	addsw	 */
		0x4B000000,	/* SUB       	subw	 */
		0x6B000000,	/* SUBS      	subsw	 */
		0x8B000000,	/* ADD       	addx	 */
		0xAB000000,	/* ADDS      	addsx	 */
		0xCB000000,	/* SUB       	subx	 */
		0xEB000000,	/* SUBS      	subsx	 */
	/* Add/subtract (extended register) */
		0x0B200000,	/* ADD       	addextw	 */
		0x2B200000,	/* ADDS      	addsextw	 */
		0x4B200000,	/* SUB       	subextw	 */
		0x6B200000,	/* SUBS      	subsextw	 */
		0x8B200000,	/* ADD       	addextx	 */
		0xAB200000,	/* ADDS      	addsextx	 */
		0xCB200000,	/* SUB       	subextx	 */
		0xEB200000,	/* SUBS      	subsextx	 */
	/* Add/subtract (with carry) */
		0x1A000000,	/* ADC       	adcw	 */
		0x3A000000,	/* ADCS      	adcsw	 */
		0x5A000000,	/* SBC       	sbcw	 */
		0x7A000000,	/* SBCS      	sbcsw	 */
		0x9A000000,	/* ADC       	adcx	 */
		0xBA000000,	/* ADCS      	adcsx	 */
		0xDA000000,	/* SBC       	sbcx	 */
		0xFA000000,	/* SBCS      	sbcsx	 */
	/* Conditional compare (register) */
		0x3A400000,	/* CCMN      	ccmnw	 */
		0xBA400000,	/* CCMN      	ccmnx	 */
		0x7A400000,	/* CCMP      	ccmpw	 */
		0xFA400000,	/* CCMP      	ccmpx	 */
	/* Conditional compare (immediate) */
		0x3A400800,	/* CCMN      	ccmnimmw	 */
		0xBA400800,	/* CCMN      	ccmnimmx	 */
		0x7A400800,	/* CCMP      	ccmpimmw	 */
		0xFA400800,	/* CCMP      	ccmpimmx	 */
	/* Conditional select */
		0x1A800000,	/* CSEL      	cselw	 */
		0x1A800400,	/* CSINC     	csincw	 */
		0x5A800000,	/* CSINV     	csinvw	 */
		0x5A800400,	/* CSNEG     	csnegw	 */
		0x9A800000,	/* CSEL      	cselx	 */
		0x9A800400,	/* CSINC     	csincx	 */
		0xDA800000,	/* CSINV     	csinvx	 */
		0xDA800400,	/* CSNEG     	csnegx	 */
	/* Data-processing (3 source) */
		0x1B000000,	/* MADD      	maddw	 */
		0x9B000000,	/* MADD      	maddx	 */
		0x9B200000,	/* SMADDL    	smaddl	 */
		0x9BA00000,	/* UMADDL    	umaddl	 */
		0x1B008000,	/* MSUB      	msubw	 */
		0x9B008000,	/* MSUB      	msubx	 */
		0x9B208000,	/* SMSUBL    	smsubl	 */
		0x9BA08000,	/* UMSUBL    	umsubl	 */
		0x9B400000,	/* SMULH     	smulh	 */
		0x9BC00000,	/* UMULH     	umulh	 */
	/* Data-processing (2 source) */
		0x9AC04C00,	/* CRC32X    	crc32x	 */
		0x9AC05C00,	/* CRC32CX   	crc32cx	 */
		0x1AC04000,	/* CRC32B    	crc32b	 */
		0x1AC05000,	/* CRC32CB   	crc32cb	 */
		0x1AC04400,	/* CRC32H    	crc32h	 */
		0x1AC05400,	/* CRC32CH   	crc32ch	 */
		0x1AC04800,	/* CRC32W    	crc32w	 */
		0x1AC05800,	/* CRC32CW   	crc32cw	 */
		0x1AC00800,	/* UDIV      	udivw	 */
		0x9AC00800,	/* UDIV      	udivx	 */
		0x1AC00C00,	/* SDIV      	sdivw	 */
		0x9AC00C00,	/* SDIV      	sdivx	 */
		0x1AC02000,	/* LSLV      	lslvw	 */
		0x9AC02000,	/* LSLV      	lslvx	 */
		0x1AC02400,	/* LSRV      	lsrvw	 */
		0x9AC02400,	/* LSRV      	lsrvx	 */
		0x1AC02800,	/* ASRV      	asrvw	 */
		0x9AC02800,	/* ASRV      	asrvx	 */
		0x1AC02C00,	/* RORV      	rorvw	 */
		0x9AC02C00,	/* RORV      	rorvx	 */
	/* Data-processing (1 source) */
		0x5AC00000,	/* RBIT      	rbitw	 */
		0xDAC00000,	/* RBIT      	rbitx	 */
		0x5AC01000,	/* CLZ       	clzw	 */
		0xDAC01000,	/* CLZ       	clzx	 */
		0x5AC01400,	/* CLS       	clsw	 */
		0xDAC01400,	/* CLS       	clsx	 */
		0x5AC00800,	/* REV       	revw	 */
		0xDAC00C00,	/* REV       	revx	 */
		0xDAC00400,	/* REV16     	rev16w	 */
		0x5AC00400,	/* REV16     	rev16x	 */
		0xDAC00800,	/* REV32     	rev32	 */
/* VFP instructions */
	/* Floating-Point Conversion */
		0x1E260000,	/* FMOV      	fmov_stow	 */
		0x1E270000,	/* FMOV      	fmov_wtos	 */
		0x9E660000,	/* FMOV      	fmov_dtox	 */
		0x9E670000,	/* FMOV      	fmov_xtod	 */
		0x1E22C000,	/* FCVT      	fcvt_stod	 */
		0x1E624000,	/* FCVT      	fcvt_dtos	 */
		0x1E380000,	/* FCVTZS    	fcvtzs_stow	 */
		0x1E780000,	/* FCVTZS    	fcvtzs_dtow	 */
		0x9E380000,	/* FCVTZS    	fcvtzs_stox	 */
		0x9E780000,	/* FCVTZS    	fcvtzs_dtox	 */
		0x1E220000,	/* SCVTF     	scvtf_wtos	 */
		0x1E620000,	/* SCVTF     	scvtf_wtod	 */
		0x9E220000,	/* SCVTF     	scvtf_xtos	 */
		0x9E620000,	/* SCVTF     	scvtf_xtod	 */
	/* Floating-Point Immediate */
		0x1E201000,	/* FMOV      	fmovimms	 */
		0x1E601000,	/* FMOV      	fmovimmd	 */
	/* Floating-Point Compare */
		0x1E202000,	/* FCMP      	fcmps	 */
		0x1E202008,	/* FCMP      	fcmps_zero	 */
		0x1E602000,	/* FCMP      	fcmpd	 */
		0x1E602008,	/* FCMP      	fcmpd_zero	 */
	/* Floating-Point Data-processing (1 source) */
		0x1E204000,	/* FMOV      	fmovs	 */
		0x1E604000,	/* FMOV      	fmovd	 */
		0x1E20C000,	/* FABS      	fabss	 */
		0x1E60C000,	/* FABS      	fabsd	 */
		0x1E214000,	/* FNEG      	fnegs	 */
		0x1E614000,	/* FNEG      	fnegd	 */
	/* Floating-Point Data-processing (2 source) */
		0x1E202800,	/* FADD      	fadds	 */
		0x1E602800,	/* FADD      	faddd	 */
		0x1E203800,	/* FSUB      	fsubs	 */
		0x1E603800,	/* FSUB      	fsubd	 */
		0x1E200800,	/* FMUL      	fmuls	 */
		0x1E600800,	/* FMUL      	fmuld	 */
		0x1E201800,	/* FDIV      	fdivs	 */
		0x1E601800,	/* FDIV      	fdivd	 */
		0x1E204800,	/* FMAX      	fmaxs	 */
		0x1E604800,	/* FMAX      	fmaxd	 */
		0x1E205800,	/* FMIN      	fmins	 */
		0x1E605800,	/* FMIN      	fmind	 */
};
