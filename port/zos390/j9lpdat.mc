/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2013
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

/**
 * @file j9lpdat.mc
 * @ingroup Port
 * @brief Contains Metal-C code that invokes SYSEVENT to query for LPAR Data.
 * The file must be saved with the extension ".mc" so that build automation can
 * recognize this and appropriately compile the source.
 */

#pragma prolog(j9req_lpdatlen, "MYPROLOG")
#pragma epilog(j9req_lpdatlen, "MYEPILOG")

/**
 * Function returns the size of a buffer that must be used to
 * populate and retrieve LPAR data.
 *
 * @return Size of LPAR data area. -1 in case of failure.
 */
int
j9req_lpdatlen(void)
{
	int lpdatlen = 0;
	int rc = 0;

	/* When SYSEVENT REQLPDAT is invoked with a parameter area length of 0 or
	 * smaller than the actual parameter area size, the error code (byte) 0x04
	 * is returned. Else, it succeeds. Here, simply query the size by passing a
	 * 0 and use this size for allocating a buffer next time SYSEVENT REQLPDAT
	 * is invoked.
	 */
	__asm(" LA 1,%1\n"
		" SYSEVENT REQLPDAT,ENTRY=UNAUTHPC\n"
		" ST 15,%0\n" : "=m"(rc) , "+m"(lpdatlen) : : "r1", "r15");
	if (0x04 == rc) {
		/* An error code of 0x04 indicates that actual length has been received
		 * in the memory clobbered operand 'lpdatlen'.
		 */
		return lpdatlen;
	} else {
		return -1;
	}
}

#pragma prolog(j9req_lpdat, "MYPROLOG")
#pragma epilog(j9req_lpdat, "MYEPILOG")

/**
 * Function fills in the buffer area provided with LPAR data.
 *
 * @param lpbuf A pointer to the buffer area.
 *
 * @return Error value. 0 on success.
 */
int
j9req_lpdat(char* lpbuf)
{
	int rc = 0;
	char *bufp;

	bufp = lpbuf;

	/* Invoking SYSEVENT REQLPDAT with appropriately sized up parameter area 
	 * fills in all data with error code 0 (indicating success). Only possible
	 * error is 0x04, when the buffer is not sufficiently large.
	 */
	/* LGR is needed for 64bit and LR for 32bit */ 
#ifdef _LP64
	__asm(" LGR 1,%1\n"
		" SYSEVENT REQLPDAT,ENTRY=UNAUTHPC\n"
		" ST 15,%0\n" : "=m"(rc) : "r"(bufp) : "r1", "r15");
#else
	__asm(" LR 1,%1\n"
		" SYSEVENT REQLPDAT,ENTRY=UNAUTHPC\n"
		" ST 15,%0\n" : "=m"(rc) : "r"(bufp) : "r1", "r15");
#endif

	return rc;
}
