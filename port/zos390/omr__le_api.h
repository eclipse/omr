/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef omr__le_api_h
#define omr__le_api_h

#include "omrcomp.h"

/* if this bit is set in the mch_flags field of j9_31bit_mch, the values in the hgprs field in j9_31bit_mch are valid */
/*
 * &NAME.HR_VALID   EQU   X'40'      High regs valid in REG_H field   @D3A
 */
#define J9MCH_FLAGS_HGPRS_VALID 0x40
/* if this bit is set in the mch_flags field of j9_31bit_mch, the values in the hgprs field in j9_31bit_mch are valid */
/*
 * &NAME.INT_SF_VALID   EQU X'20'    Interrupt stackframe valid in    @D5A
 *                                 INT_SF field
 */
#define J9MCH_FLAGS_INT_SF_VALID 0x20

/*
 * If this bit is set in the mch_flags field of the j9_31bit_mch, the values in the VR registers in j9_31bit_mch are valid
 *
 * &NAME.VR_VALID	EQU X'2'	Vector registers saved in MCH
 */
#define J9MCH_FLAGS_VR_VALID	0x2

/*
 * Derived from the assembler version, CEEMCH.COPY, as there is no C-mapping of the 31-bit (machine context)
 *
 * The 64-bit C mapping is in __le_api.h
 *
 * This structure is pointed to by the cib_machine field of struct _CEECIB as defined by leawi.h.
 *
 * struct _CEECIB is accessible from a condition handler via the LE service CEE3CIB
 *
 */
typedef struct j9_31bit_mch {
	/*
	 * &NAME.EYEC       DC    CL4'CMCH'          Eye catcher
	 * &NAME.SIZE       DC    H'0'               Size of area
	 * &NAME.LEVEL      DC    H'1'               Version of MACRO generation
	 */
	uint32_t padding1[2];

	/*
	 * &NAME.REG        DS   0CL(16*LEPTRLEN)    General regs 0-15        @G3C
	 * &NAME.GPR        DC  16&FLEN.'00'         Each reg, 0-15           @G3C
	 */
	uint32_t gprs[16];			/* gprs 0-15 (low word of 64-bit gprs, see hgprs below for high word) */

	/*
	 * &NAME.PSW        DC    XL(2*LEPTRLEN)'00' PSW at time of interrupt @G3C
	 */
	uint32_t psw0;
	uint32_t psw1;

	/*
	 * &NAME.INTI       DS   0F                  Basic extension of PSW
	 * &NAME.ILC        DC    H'0'                Instruction length code
	 * &NAME.IC         DS   0H                   Interrupt code
	 * &NAME.IC1        DC    X'00'                Part 1
	 * &NAME.IC2        DC    X'00'                Part 2
	 *                  DS    &PAD64             Padding in 64-bit mode   @G3A
	 * &NAME.PFT        DC    &PLEN.(0)          Page fault address       @G3C
	 */
	uint32_t padding2[2];

	/*
	 *	&NAME.FLT        DS   4D          Basic Floating point registers
	 *	                 ORG   &NAME.FLT
	 *	&NAME.FLT_0      DC    D'0'       FP reg 0
	 *	&NAME.FLT_2      DC    D'0'       FP reg 2
	 *	&NAME.FLT_4      DC    D'0'       FP reg 4
	 *	&NAME.FLT_6      DC    D'0'       FP reg 6
	 */

	double fprs_0246[4];	/* fprs 0,2,4,6 */

	/*
	 *            Language Environment area
	 *            -------------------------
	 *
	 *
	 * &NAME.RSV1       DC    XL44'00'   Reserved.                        @D5C
	 *
	 * &NAME.ZERO1      DC    XL20'00'   Area to be cleared in new MCHs   @D5C
	 *                	ORG   &NAME.ZERO1                                 @D3A
	 *                	&NAME.INT_SF     DC    AL4(0)     Interrupt stack frame            @D5A
	 *            		DC    XL11'00'   (reserved)                       @D5C
	 */

	uint32_t padding3[11];
	uint32_t interrupt_stack_frame;
	uint8_t padding3a[11];
	uint8_t mch_flags;
	uint32_t padding4[2];
	uint32_t bea;				/* Breaking Event Address */
	uint32_t padding4a[4];

	double fprs_1357[4];	/* fprs 1,3,5,7 */
	double fprs_8_15[8];	/* fprs 8 - 15 */
	uint32_t fpc;

	uint32_t padding5[3];

	uint32_t hgprs[16];			/* high word of 64-bit gprs */

	uint32_t padding6[32];

	U_128 vr[32];
} j9_31bit_mch;

#endif /* omr__le_api_h */
