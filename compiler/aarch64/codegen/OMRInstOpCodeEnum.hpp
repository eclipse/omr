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

/*
 * This file will be included within an enum. Only comments and enumerator
 * definitions are permitted.
 */

//		Opcode                                                         BINARY    	OPCODE    	comments
/* UNALLOCATED */
		bad,                                                    	/* 0x00000000	BAD       	invalid operation */
/* Branch,exception generation and system Instruction */
	/* Compare _ Branch (immediate) */
		cbzw,                                                   	/* 0x34000000	CBZ       	 */
		cbnzw,                                                  	/* 0x35000000	CBNZ      	 */
		cbzx,                                                   	/* 0xB4000000	CBZ       	 */
		cbnzx,                                                  	/* 0xB5000000	CBNZ      	 */
	/* Test bit & branch (immediate) */
		tbz,                                                    	/* 0x36000000	TBZ       	 */
		tbnz,                                                   	/* 0x37000000	TBNZ      	 */
	/* Conditional branch (immediate) */
		b_cond,                                                 	/* 0x54000000	B_cond    	 */
	/* Exception generation */
		brkarm64,                                               	/* 0xD4200000	BRK       	AArch64 Specific BRK */
	/* System */
	/* Unconditional branch (register) */
		br,                                                     	/* 0xD61F0000	BR        	 */
		blr,                                                    	/* 0xD63F0000	BLR       	 */
		ret,                                                    	/* 0xD65F0000	RET       	 */
	/* Unconditional branch (immediate) */
		b,                                                      	/* 0x14000000	B         	 */
		bl,                                                     	/* 0x94000000	BL        	 */
/* Loads and stores */
	/* Load/store exclusive */
		stxrb,                                                  	/* 0x08000000	STXRB     	 */
		stlxrb,                                                 	/* 0x08008000	STLXRB    	 */
		ldxrb,                                                  	/* 0x08400000	LDXRB     	 */
		ldaxrb,                                                 	/* 0x08408000	LDAXRB    	 */
		stlrb,                                                  	/* 0x08808000	STLRB     	 */
		ldarb,                                                  	/* 0x08C08000	LDARB     	 */
		stxrh,                                                  	/* 0x48000000	STXRH     	 */
		stlxrh,                                                 	/* 0x48008000	STLXRH    	 */
		ldxrh,                                                  	/* 0x48400000	LDXRH     	 */
		ldaxrh,                                                 	/* 0x48408000	LDAXRH    	 */
		stlrh,                                                  	/* 0x48808000	STLRH     	 */
		ldarh,                                                  	/* 0x48C08000	LDARH     	 */
		stxrw,                                                  	/* 0x88000000	STXR      	 */
		stlxrw,                                                 	/* 0x88008000	STLXR     	 */
		stxpw,                                                  	/* 0x88200000	STXP      	 */
		stlxpw,                                                 	/* 0x88208000	STLXP     	 */
		ldxrw,                                                  	/* 0x88400000	LDXR      	 */
		ldaxrw,                                                 	/* 0x88408000	LDAXR     	 */
		ldxpw,                                                  	/* 0x88600000	LDXP      	 */
		ldaxpw,                                                 	/* 0x88608000	LDAXP     	 */
		stlrw,                                                  	/* 0x88808000	STLR      	 */
		ldarw,                                                  	/* 0x88C08000	LDAR      	 */
		stxrx,                                                  	/* 0xC8000000	STXR      	 */
		stlxrx,                                                 	/* 0xC8008000	STLXR     	 */
		stxpx,                                                  	/* 0xC8200000	STXP      	 */
		stlxpx,                                                 	/* 0xC8208000	STLXP     	 */
		ldxrx,                                                  	/* 0xC8400000	LDXR      	 */
		ldaxrx,                                                 	/* 0xC8408000	LDAXR     	 */
		ldxpx,                                                  	/* 0xC8600000	LDXP      	 */
		ldaxpx,                                                 	/* 0xC8608000	LDAXP     	 */
		stlrx,                                                  	/* 0xC8808000	STLR      	 */
		ldarx,                                                  	/* 0xC8C08000	LDAR      	 */
	/* Load register (literal) */
		ldrw,                                                   	/* 0x18000000	LDR       	 */
		vldrs,                                                  	/* 0x1C000000	LDR       	 */
		ldrx,                                                   	/* 0x58000000	LDR       	 */
		vldrd,                                                  	/* 0x5C000000	LDR       	 */
		ldrsw,                                                  	/* 0x98000000	LDRSW     	 */
		vldrq,                                                  	/* 0x9C000000	LDR       	 */
		prfm,                                                   	/* 0xD8000000	PRFM      	 */
	/* Load/store no-allocate pair (offset) */
		stnpw,                                                  	/* 0x28000000	STNP      	 */
		ldnpw,                                                  	/* 0x28400000	LDNP      	 */
		vstnps,                                                 	/* 0x2C000000	STNP      	 */
		vldnps,                                                 	/* 0x2C400000	LDNP      	 */
		vstnpd,                                                 	/* 0x6C000000	STNP      	 */
		vldnpd,                                                 	/* 0x6C400000	LDNP      	 */
		stnpx,                                                  	/* 0xA8000000	STNP      	 */
		ldnpx,                                                  	/* 0xA8400000	LDNP      	 */
		vstnpq,                                                 	/* 0xAC000000	STNP      	 */
		vldnpq,                                                 	/* 0xAC400000	LDNP      	 */
	/* Load/store register pair (post-indexed) */
		stppostw,                                               	/* 0x28800000	STP       	 */
		ldppostw,                                               	/* 0x28C00000	LDP       	 */
		vstpposts,                                              	/* 0x2C800000	STP       	 */
		vldpposts,                                              	/* 0x2CC00000	LDP       	 */
		ldpswpost,                                              	/* 0x68C00000	LDPSW     	 */
		vstppostd,                                              	/* 0x6C800000	STP       	 */
		vldppostd,                                              	/* 0x6CC00000	LDP       	 */
		stppostx,                                               	/* 0xA8800000	STP       	 */
		ldppostx,                                               	/* 0xA8C00000	LDP       	 */
		vstppostq,                                              	/* 0xAC800000	STP       	 */
		vldppostq,                                              	/* 0xACC00000	LDP       	 */
	/* Load/store register pair (offset) */
		stpoffw,                                                	/* 0x29000000	STP       	 */
		ldpoffw,                                                	/* 0x29400000	LDP       	 */
		vstpoffs,                                               	/* 0x2D000000	STP       	 */
		vldpoffs,                                               	/* 0x2D400000	LDP       	 */
		ldpswoff,                                               	/* 0x69400000	LDPSW     	 */
		vstpoffd,                                               	/* 0x6D000000	STP       	 */
		vldpoffd,                                               	/* 0x6D400000	LDP       	 */
		stpoffx,                                                	/* 0xA9000000	STP       	 */
		ldpoffx,                                                	/* 0xA9400000	LDP       	 */
		vstpoffq,                                               	/* 0xAD000000	STP       	 */
		vldpoffq,                                               	/* 0xAD400000	LDP       	 */
	/* Load/store register pair (pre-indexed) */
		stpprew,                                                	/* 0x29800000	STP       	 */
		ldpprew,                                                	/* 0x29C00000	LDP       	 */
		vstppres,                                               	/* 0x2D800000	STP       	 */
		vldppres,                                               	/* 0x2DC00000	LDP       	 */
		ldpswpre,                                               	/* 0x69C00000	LDPSW     	 */
		vstppred,                                               	/* 0x6D800000	STP       	 */
		vldppred,                                               	/* 0x6DC00000	LDP       	 */
		stpprex,                                                	/* 0xA9800000	STP       	 */
		ldpprex,                                                	/* 0xA9C00000	LDP       	 */
		vstppreq,                                               	/* 0xAD800000	STP       	 */
		vldppreq,                                               	/* 0xADC00000	LDP       	 */
	/* Load/store register (unscaled immediate) */
		sturb,                                                  	/* 0x38000000	STURB     	 */
		ldurb,                                                  	/* 0x38400000	LDURB     	 */
		ldursbx,                                                	/* 0x38800000	LDURSB    	 */
		ldursbw,                                                	/* 0x38C00000	LDURSB    	 */
		vsturb,                                                 	/* 0x3C000000	STUR      	 */
		vldurb,                                                 	/* 0x3C400000	LDUR      	 */
		vsturq,                                                 	/* 0x3C800000	STUR      	 */
		vldurq,                                                 	/* 0x3CC00000	LDUR      	 */
		sturh,                                                  	/* 0x78000000	STURH     	 */
		ldurh,                                                  	/* 0x78400000	LDURH     	 */
		ldurshx,                                                	/* 0x78800000	LDURSH    	 */
		ldurshw,                                                	/* 0x78C00000	LDURSH    	 */
		vsturh,                                                 	/* 0x7C000000	STUR      	 */
		vldurh,                                                 	/* 0x7C400000	LDUR      	 */
		sturw,                                                  	/* 0xB8000000	STUR      	 */
		ldurw,                                                  	/* 0xB8400000	LDUR      	 */
		ldursw,                                                 	/* 0xB8800000	LDURSW    	 */
		vsturs,                                                 	/* 0xBC000000	STUR      	 */
		vldurs,                                                 	/* 0xBC400000	LDUR      	 */
		sturx,                                                  	/* 0xF8000000	STUR      	 */
		ldurx,                                                  	/* 0xF8400000	LDUR      	 */
		prfum,                                                  	/* 0xF8800000	PRFUM     	 */
		vsturd,                                                 	/* 0xFC000000	STUR      	 */
		vldurd,                                                 	/* 0xFC400000	LDUR      	 */
	/* Load/store register (immediate post-indexed) */
		strbpost,                                               	/* 0x38000400	STRB      	 */
		ldrbpost,                                               	/* 0x38400400	LDRB      	 */
		ldrsbpostx,                                             	/* 0x38800400	LDRSB     	 */
		ldrsbpostw,                                             	/* 0x38C00400	LDRSB     	 */
		vstrpostb,                                              	/* 0x3C000400	STR       	 */
		vldrpostb,                                              	/* 0x3C400400	LDR       	 */
		vstrpostq,                                              	/* 0x3C800400	STR       	 */
		vldrpostq,                                              	/* 0x3CC00400	LDR       	 */
		strhpost,                                               	/* 0x78000400	STRH      	 */
		ldrhpost,                                               	/* 0x78400400	LDRH      	 */
		ldrshpostx,                                             	/* 0x78800400	LDRSH     	 */
		ldrshpostw,                                             	/* 0x78C00400	LDRSH     	 */
		vstrposth,                                              	/* 0x7C000400	STR       	 */
		vldrposth,                                              	/* 0x7C400400	LDR       	 */
		strpostw,                                               	/* 0xB8000400	STR       	 */
		ldrpostw,                                               	/* 0xB8400400	LDR       	 */
		ldrswpost,                                              	/* 0xB8800400	LDRSW     	 */
		vstrposts,                                              	/* 0xBC000400	STR       	 */
		vldrposts,                                              	/* 0xBC400400	LDR       	 */
		strpostx,                                               	/* 0xF8000400	STR       	 */
		ldrpostx,                                               	/* 0xF8400400	LDR       	 */
		vstrpostd,                                              	/* 0xFC000400	STR       	 */
		vldrpostd,                                              	/* 0xFC400400	LDR       	 */
	/* Load/store register (unprivileged) */
		sttrb,                                                  	/* 0x38000800	STTRB     	 */
		ldtrb,                                                  	/* 0x38400800	LDTRB     	 */
		ldtrsbx,                                                	/* 0x38800800	LDTRSB    	 */
		ldtrsbw,                                                	/* 0x38C00800	LDTRSB    	 */
		sttrh,                                                  	/* 0x78000800	STTRH     	 */
		ldtrh,                                                  	/* 0x78400800	LDTRH     	 */
		ldtrshx,                                                	/* 0x78800800	LDTRSH    	 */
		ldtrshw,                                                	/* 0x78C00800	LDTRSH    	 */
		sttrw,                                                  	/* 0xB8000800	STTR      	 */
		ldtrw,                                                  	/* 0xB8400800	LDTR      	 */
		ldtrsw,                                                 	/* 0xB8800800	LDTRSW    	 */
		sttrx,                                                  	/* 0xF8000800	STTR      	 */
		ldtrx,                                                  	/* 0xF8400800	LDTR      	 */
	/* Load/store register (immediate pre-indexed) */
		strbpre,                                                	/* 0x38000C00	STRB      	 */
		ldrbpre,                                                	/* 0x38400C00	LDRB      	 */
		ldrsbprex,                                              	/* 0x38800C00	LDRSB     	 */
		ldrsbprew,                                              	/* 0x38C00C00	LDRSB     	 */
		vstrpreb,                                               	/* 0x3C000C00	STR       	 */
		vldrpreb,                                               	/* 0x3C400C00	LDR       	 */
		vstrpreq,                                               	/* 0x3C800C00	STR       	 */
		vldrpreq,                                               	/* 0x3CC00C00	LDR       	 */
		strhpre,                                                	/* 0x78000C00	STRH      	 */
		ldrhpre,                                                	/* 0x78400C00	LDRH      	 */
		ldrshprex,                                              	/* 0x78800C00	LDRSH     	 */
		ldrshprew,                                              	/* 0x78C00C00	LDRSH     	 */
		vstrpreh,                                               	/* 0x7C000C00	STR       	 */
		vldrpreh,                                               	/* 0x7C400C00	LDR       	 */
		strprew,                                                	/* 0xB8000C00	STR       	 */
		ldrprew,                                                	/* 0xB8400C00	LDR       	 */
		ldrswpre,                                               	/* 0xB8800C00	LDRSW     	 */
		vstrpres,                                               	/* 0xBC000C00	STR       	 */
		vldrpres,                                               	/* 0xBC400C00	LDR       	 */
		strprex,                                                	/* 0xF8000C00	STR       	 */
		ldrprex,                                                	/* 0xF8400C00	LDR       	 */
		vstrpred,                                               	/* 0xFC000C00	STR       	 */
		vldrpred,                                               	/* 0xFC400C00	LDR       	 */
	/* Load/store register (register offset) */
		strboff,                                                	/* 0x38200800	STRB      	 */
		ldrboff,                                                	/* 0x38600800	LDRB      	 */
		ldrsboffx,                                              	/* 0x38A00800	LDRSB     	 */
		ldrsboffw,                                              	/* 0x38E00800	LDRSB     	 */
		vstroffb,                                               	/* 0x3C200800	STR       	 */
		vldroffb,                                               	/* 0x3C600800	LDR       	 */
		vstroffq,                                               	/* 0x3CA00800	STR       	 */
		vldroffq,                                               	/* 0x3CE00800	LDR       	 */
		strhoff,                                                	/* 0x78200800	STRH      	 */
		ldrhoff,                                                	/* 0x78600800	LDRH      	 */
		ldrshoffx,                                              	/* 0x78A00800	LDRSH     	 */
		ldrshoffw,                                              	/* 0x78E00800	LDRSH     	 */
		vstroffh,                                               	/* 0x7C200800	STR       	 */
		vldroffh,                                               	/* 0x7C600800	LDR       	 */
		stroffw,                                                	/* 0xB8200800	STR       	 */
		ldroffw,                                                	/* 0xB8600800	LDR       	 */
		ldrswoff,                                               	/* 0xB8A00800	LDRSW     	 */
		vstroffs,                                               	/* 0xBC200800	STR       	 */
		vldroffs,                                               	/* 0xBC600800	LDR       	 */
		stroffx,                                                	/* 0xF8200800	STR       	 */
		ldroffx,                                                	/* 0xF8600800	LDR       	 */
		vstroffd,                                               	/* 0xFC200800	STR       	 */
		vldroffd,                                               	/* 0xFC600800	LDR       	 */
		prfmoff,                                                	/* 0xF8A00800	PRFM      	 */
	/* Load/store register (unsigned immediate) */
		strbimm,                                                	/* 0x39000000	STRB      	 */
		ldrbimm,                                                	/* 0x39400000	LDRB      	 */
		ldrsbimmx,                                              	/* 0x39800000	LDRSB     	 */
		ldrsbimmw,                                              	/* 0x39C00000	LDRSB     	 */
		vstrimmb,                                               	/* 0x3D000000	STR       	 */
		vldrimmb,                                               	/* 0x3D400000	LDR       	 */
		vstrimmq,                                               	/* 0x3D800000	STR       	 */
		vldrimmq,                                               	/* 0x3DC00000	LDR       	 */
		strhimm,                                                	/* 0x79000000	STRH      	 */
		ldrhimm,                                                	/* 0x79400000	LDRH      	 */
		ldrshimmx,                                              	/* 0x79800000	LDRSH     	 */
		ldrshimmw,                                              	/* 0x79C00000	LDRSH     	 */
		vstrimmh,                                               	/* 0x7D000000	STR       	 */
		vldrimmh,                                               	/* 0x7D400000	LDR       	 */
		strimmw,                                                	/* 0xB9000000	STR       	 */
		ldrimmw,                                                	/* 0xB9400000	LDR       	 */
		ldrswimm,                                               	/* 0xB9800000	LDRSW     	 */
		vstrimms,                                               	/* 0xBD000000	STR       	 */
		vldrimms,                                               	/* 0xBD400000	LDR       	 */
		strimmx,                                                	/* 0xF9000000	STR       	 */
		ldrimmx,                                                	/* 0xF9400000	LDR       	 */
		vstrimmd,                                               	/* 0xFD000000	STR       	 */
		vldrimmd,                                               	/* 0xFD400000	LDR       	 */
		prfmimm,                                                	/* 0xF9800000	PRFM      	 */
/* Data processing - Immediate */
	/* PC-rel. addressing */
		adr,                                                    	/* 0x10000000	ADR       	 */
		adrp,                                                   	/* 0x90000000	ADRP      	 */
	/* Add/subtract (immediate) */
		addimmw,                                                	/* 0x11000000	ADD       	 */
		addsimmw,                                               	/* 0x31000000	ADDS      	 */
		subimmw,                                                	/* 0x51000000	SUB       	 */
		subsimmw,                                               	/* 0x71000000	SUBS      	 */
		addimmx,                                                	/* 0x91000000	ADD       	 */
		addsimmx,                                               	/* 0xB1000000	ADDS      	 */
		subimmx,                                                	/* 0xD1000000	SUB       	 */
		subsimmx,                                               	/* 0xF1000000	SUBS      	 */
	/* Logical (immediate) */
		andimmw,                                                	/* 0x12000000	AND       	 */
		orrimmw,                                                	/* 0x32000000	ORR       	 */
		eorimmw,                                                	/* 0x52000000	EOR       	 */
		andsimmw,                                               	/* 0x72000000	ANDS      	 */
		andimmx,                                                	/* 0x92000000	AND       	 */
		orrimmx,                                                	/* 0xB2000000	ORR       	 */
		eorimmx,                                                	/* 0xD2000000	EOR       	 */
		andsimmx,                                               	/* 0xF2000000	ANDS      	 */
	/* Move wide (immediate) */
		movnw,                                                  	/* 0x12800000	MOVN      	 */
		movzw,                                                  	/* 0x52800000	MOVZ      	 */
		movkw,                                                  	/* 0x72800000	MOVK      	 */
		movnx,                                                  	/* 0x92800000	MOVN      	 */
		movzx,                                                  	/* 0xD2800000	MOVZ      	 */
		movkx,                                                  	/* 0xF2800000	MOVK      	 */
	/* Bitfield */
		sbfmw,                                                  	/* 0x13000000	SBFM      	 */
		bfmw,                                                   	/* 0x33000000	BFM       	 */
		ubfmw,                                                  	/* 0x53000000	UBFM      	 */
		sbfmx,                                                  	/* 0x93400000	SBFM      	 */
		bfmx,                                                   	/* 0xB3400000	BFM       	 */
		ubfmx,                                                  	/* 0xD3400000	UBFM      	 */
	/* Extract */
		extrw,                                                  	/* 0x13800000	EXTR      	 */
		extrx,                                                  	/* 0x93C08000	EXTR      	 */
/* Data Processing - register */
	/* Logical (shifted register) */
		andw,                                                   	/* 0x0A000000	AND       	 */
		bicw,                                                   	/* 0x0A200000	BIC       	 */
		orrw,                                                   	/* 0x2A000000	ORR       	 */
		ornw,                                                   	/* 0x2A200000	ORN       	 */
		eorw,                                                   	/* 0x4A000000	EOR       	 */
		eonw,                                                   	/* 0x4A200000	EON       	 */
		andsw,                                                  	/* 0x6A000000	ANDS      	 */
		bicsw,                                                  	/* 0x6A200000	BICS      	 */
		andx,                                                   	/* 0x8A000000	AND       	 */
		bicx,                                                   	/* 0x8A200000	BIC       	 */
		orrx,                                                   	/* 0xAA000000	ORR       	 */
		ornx,                                                   	/* 0xAA200000	ORN       	 */
		eorx,                                                   	/* 0xCA000000	EOR       	 */
		eonx,                                                   	/* 0xCA200000	EON       	 */
		andsx,                                                  	/* 0xEA000000	ANDS      	 */
		bicsx,                                                  	/* 0xEA200000	BICS      	 */
	/* Add/subtract (shifted register) */
		addw,                                                   	/* 0x0B000000	ADD       	 */
		addsw,                                                  	/* 0x2B000000	ADDS      	 */
		subw,                                                   	/* 0x4B000000	SUB       	 */
		subsw,                                                  	/* 0x6B000000	SUBS      	 */
		addx,                                                   	/* 0x8B000000	ADD       	 */
		addsx,                                                  	/* 0xAB000000	ADDS      	 */
		subx,                                                   	/* 0xCB000000	SUB       	 */
		subsx,                                                  	/* 0xEB000000	SUBS      	 */
	/* Add/subtract (extended register) */
		addextw,                                                	/* 0x0B200000	ADD       	 */
		addsextw,                                               	/* 0x2B200000	ADDS      	 */
		subextw,                                                	/* 0x4B200000	SUB       	 */
		subsextw,                                               	/* 0x6B200000	SUBS      	 */
		addextx,                                                	/* 0x8B200000	ADD       	 */
		addsextx,                                               	/* 0xAB200000	ADDS      	 */
		subextx,                                                	/* 0xCB200000	SUB       	 */
		subsextx,                                               	/* 0xEB200000	SUBS      	 */
	/* Add/subtract (with carry) */
		adcw,                                                   	/* 0x1A000000	ADC       	 */
		adcsw,                                                  	/* 0x3A000000	ADCS      	 */
		sbcw,                                                   	/* 0x5A000000	SBC       	 */
		sbcsw,                                                  	/* 0x7A000000	SBCS      	 */
		adcx,                                                   	/* 0x9A000000	ADC       	 */
		adcsx,                                                  	/* 0xBA000000	ADCS      	 */
		sbcx,                                                   	/* 0xDA000000	SBC       	 */
		sbcsx,                                                  	/* 0xFA000000	SBCS      	 */
	/* Conditional compare (register) */
		ccmnw,                                                  	/* 0x3A400000	CCMN      	 */
		ccmnx,                                                  	/* 0xBA400000	CCMN      	 */
		ccmpw,                                                  	/* 0x7A400000	CCMP      	 */
		ccmpx,                                                  	/* 0xFA400000	CCMP      	 */
	/* Conditional compare (immediate) */
		ccmnimmw,                                               	/* 0x3A400800	CCMN      	 */
		ccmnimmx,                                               	/* 0xBA400800	CCMN      	 */
		ccmpimmw,                                               	/* 0x7A400800	CCMP      	 */
		ccmpimmx,                                               	/* 0xFA400800	CCMP      	 */
	/* Conditional select */
		cselw,                                                  	/* 0x1A800000	CSEL      	 */
		csincw,                                                 	/* 0x1A800400	CSINC     	 */
		csinvw,                                                 	/* 0x5A800000	CSINV     	 */
		csnegw,                                                 	/* 0x5A800400	CSNEG     	 */
		cselx,                                                  	/* 0x9A800000	CSEL      	 */
		csincx,                                                 	/* 0x9A800400	CSINC     	 */
		csinvx,                                                 	/* 0xDA800000	CSINV     	 */
		csnegx,                                                 	/* 0xDA800400	CSNEG     	 */
	/* Data-processing (3 source) */
		maddw,                                                  	/* 0x1B000000	MADD      	 */
		maddx,                                                  	/* 0x9B000000	MADD      	 */
		smaddl,                                                 	/* 0x9B200000	SMADDL    	 */
		umaddl,                                                 	/* 0x9BA00000	UMADDL    	 */
		msubw,                                                  	/* 0x1B008000	MSUB      	 */
		msubx,                                                  	/* 0x9B008000	MSUB      	 */
		smsubl,                                                 	/* 0x9B208000	SMSUBL    	 */
		umsubl,                                                 	/* 0x9BA08000	UMSUBL    	 */
		smulh,                                                  	/* 0x9B400000	SMULH     	 */
		umulh,                                                  	/* 0x9BC00000	UMULH     	 */
	/* Data-processing (2 source) */
		crc32x,                                                 	/* 0x9AC04C00	CRC32X    	 */
		crc32cx,                                                	/* 0x9AC05C00	CRC32CX   	 */
		crc32b,                                                 	/* 0x1AC04000	CRC32B    	 */
		crc32cb,                                                	/* 0x1AC05000	CRC32CB   	 */
		crc32h,                                                 	/* 0x1AC04400	CRC32H    	 */
		crc32ch,                                                	/* 0x1AC05400	CRC32CH   	 */
		crc32w,                                                 	/* 0x1AC04800	CRC32W    	 */
		crc32cw,                                                	/* 0x1AC05800	CRC32CW   	 */
		udivw,                                                  	/* 0x1AC00800	UDIV      	 */
		udivx,                                                  	/* 0x9AC00800	UDIV      	 */
		sdivw,                                                  	/* 0x1AC00C00	SDIV      	 */
		sdivx,                                                  	/* 0x9AC00C00	SDIV      	 */
		lslvw,                                                  	/* 0x1AC02000	LSLV      	 */
		lslvx,                                                  	/* 0x9AC02000	LSLV      	 */
		lsrvw,                                                  	/* 0x1AC02400	LSRV      	 */
		lsrvx,                                                  	/* 0x9AC02400	LSRV      	 */
		asrvw,                                                  	/* 0x1AC02800	ASRV      	 */
		asrvx,                                                  	/* 0x9AC02800	ASRV      	 */
		rorvw,                                                  	/* 0x1AC02C00	RORV      	 */
		rorvx,                                                  	/* 0x9AC02C00	RORV      	 */
	/* Data-processing (1 source) */
		rbitw,                                                  	/* 0x5AC00000	RBIT      	 */
		rbitx,                                                  	/* 0xDAC00000	RBIT      	 */
		clzw,                                                   	/* 0x5AC01000	CLZ       	 */
		clzx,                                                   	/* 0xDAC01000	CLZ       	 */
		clsw,                                                   	/* 0x5AC01400	CLS       	 */
		clsx,                                                   	/* 0xDAC01400	CLS       	 */
		revw,                                                   	/* 0x5AC00800	REV       	 */
		revx,                                                   	/* 0xDAC00C00	REV       	 */
		rev16w,                                                 	/* 0xDAC00400	REV16     	 */
		rev16x,                                                 	/* 0x5AC00400	REV16     	 */
		rev32,                                                  	/* 0xDAC00800	REV32     	 */
/* VFP instructions */
	/* Floating-Point Conversion */
		fmov_stow,                                              	/* 0x1E260000	FMOV      	 */
		fmov_wtos,                                              	/* 0x1E270000	FMOV      	 */
		fmov_dtox,                                              	/* 0x9E660000	FMOV      	 */
		fmov_xtod,                                              	/* 0x9E670000	FMOV      	 */
		fcvt_stod,                                              	/* 0x1E22C000	FCVT      	 */
		fcvt_dtos,                                              	/* 0x1E624000	FCVT      	 */
		fcvtzs_stow,                                            	/* 0x1E380000	FCVTZS    	 */
		fcvtzs_dtow,                                            	/* 0x1E780000	FCVTZS    	 */
		fcvtzs_stox,                                            	/* 0x9E380000	FCVTZS    	 */
		fcvtzs_dtox,                                            	/* 0x9E780000	FCVTZS    	 */
		scvtf_wtos,                                             	/* 0x1E220000	SCVTF     	 */
		scvtf_wtod,                                             	/* 0x1E620000	SCVTF     	 */
		scvtf_xtos,                                             	/* 0x9E220000	SCVTF     	 */
		scvtf_xtod,                                             	/* 0x9E620000	SCVTF     	 */
	/* Floating-Point Immediate */
		fmovimms,                                               	/* 0x1E201000	FMOV      	 */
		fmovimmd,                                               	/* 0x1E601000	FMOV      	 */
	/* Floating-Point Compare */
		fcmps,                                                  	/* 0x1E202000	FCMP      	 */
		fcmps_zero,                                             	/* 0x1E202008	FCMP      	 */
		fcmpd,                                                  	/* 0x1E602000	FCMP      	 */
		fcmpd_zero,                                             	/* 0x1E602008	FCMP      	 */
	/* Floating-Point Data-processing (1 source) */
		fmovs,                                                  	/* 0x1E204000	FMOV      	 */
		fmovd,                                                  	/* 0x1E604000	FMOV      	 */
		fabss,                                                  	/* 0x1E20C000	FABS      	 */
		fabsd,                                                  	/* 0x1E60C000	FABS      	 */
		fnegs,                                                  	/* 0x1E214000	FNEG      	 */
		fnegd,                                                  	/* 0x1E614000	FNEG      	 */
	/* Floating-Point Data-processing (2 source) */
		fadds,                                                  	/* 0x1E202800	FADD      	 */
		faddd,                                                  	/* 0x1E602800	FADD      	 */
		fsubs,                                                  	/* 0x1E203800	FSUB      	 */
		fsubd,                                                  	/* 0x1E603800	FSUB      	 */
		fmuls,                                                  	/* 0x1E200800	FMUL      	 */
		fmuld,                                                  	/* 0x1E600800	FMUL      	 */
		fdivs,                                                  	/* 0x1E201800	FDIV      	 */
		fdivd,                                                  	/* 0x1E601800	FDIV      	 */
		fmaxs,                                                  	/* 0x1E204800	FMAX      	 */
		fmaxd,                                                  	/* 0x1E604800	FMAX      	 */
		fmins,                                                  	/* 0x1E205800	FMIN      	 */
		fmind,                                                  	/* 0x1E605800	FMIN      	 */
/* Internal OpCodes */
		proc,  // Entry to the method
		fence, // Fence
		retn,  // Return
		dd,    // Define word
		label, // Destination of a jump
		ARM64LastOp = label,
		ARM64NumOpCodes = ARM64LastOp+1,
