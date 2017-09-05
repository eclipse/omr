/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/*
 * This file is used to generate the HLASM corresponding to the C calls
 * that use the STORAGE macro in omrvmem.c
 *
 * This file is compiled manually using the METAL-C compiler that was
 * introduced in z/OS V1R9. The generated output (omrstorage.s) is then
 * inserted into omrvmem_support_below_bar_[31|64].s which is compiled by our makefiles.
 *
 * omrvmem_support_below_bar_[31|64].s indicates where to put the contents of omrstorage.s.
 * Search for:
 * 		Insert contents of omrstorage.s below
 *
 * *******
 * NOTE!!!!! You must strip the line numbers from any pragma statements!
 * 		(CMVC 140560)
 * *******
 *
 * It should be obvious, however, just to be clear be sure to omit the
 * first two lines from omrstorage.s which will look something like:
 *
 *          TITLE '5694A01 V1.9 z/OS XL C
 *                     ./omrstorage.c'
 *
 * To compile for 64-bit use:
 *  xlc -S -qmetal -Wc,lp64 -qlongname omrstorage.c
 *
 * To compile for 31-bit use:
 *  xlc -S -qmetal -qlongname omrstorage.c
 *
 * z/OS V1R9 elements and features:
 * 		http://www-03.ibm.com/systems/z/os/zos/bkserv/r9pdf/index.html
 * 			- One stop shopping for all things z/OS.
 *
 * z/OS V1R9 z/OS V1R9.0 Metal C Programming Guide and Reference:
 * 		http://publibz.boulder.ibm.com/epubs/pdf/ccrug100.pdf
 *
 */

#pragma prolog(omrallocate_1M_pageable_pages_below_bar,"MYPROLOG")
#pragma epilog(omrallocate_1M_pageable_pages_below_bar,"MYEPILOG")

/*
 * Allocate 1MB pageable pages below 2GB bar using STORAGE system macro.
 * Memory allocated is freed using omrfree_memory_below_bar().
 *
 * @params[in] numBytes Number of bytes to be allocated
 * @params[in] subpool subpool number to be used
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void *
omrallocate_1M_pageable_pages_below_bar(long *numBytes, int *subpool)
{
	long length;
	long sp;
	long addr;
	int rc = 0;

	length = *numBytes;
	sp = *subpool;

	__asm(" STORAGE OBTAIN,COND=YES,LOC=(31,PAGEFRAMESIZE1MB),"\
			"LENGTH=(%2),RTCD=(%0),ADDR=(%1),SP=(%3)"\
			:"=r"(rc),"=r"(addr):"r"(length),"r"(sp));

	if (0 == rc) {
		return (void *)addr;
	} else {
		return (void *)0;
	}
}

#pragma prolog(omrallocate_4K_pages_below_bar,"MYPROLOG")
#pragma epilog(omrallocate_4K_pages_below_bar,"MYEPILOG")

/*
 * Allocate 4K pages below 2GB bar using STORAGE system macro.
 * Memory allocated is freed using omrfree_memory_below_bar().
 *
 * @params[in] numBytes Number of bytes to be allocated
 * @params[in] subpool subpool number to be used
 *
 * @return pointer to memory allocated, NULL on failure. Returned value is guaranteed to be page aligned.
 */
void *
omrallocate_4K_pages_below_bar(long *numBytes, int *subpool)
{
	long length;
	long sp;
	long addr;
	int rc = 0;

	length = *numBytes;
	sp = *subpool;

	__asm(" STORAGE OBTAIN,COND=YES,LOC=(31,64),BNDRY=PAGE,"\
			"LENGTH=(%2),RTCD=(%0),ADDR=(%1),SP=(%3)"\
			:"=r"(rc),"=r"(addr):"r"(length),"r"(sp));

	if (0 == rc) {
		return (void *)addr;
	} else {
		return (void *)0;
	}
}

#pragma prolog(omrfree_memory_below_bar,"MYPROLOG")
#pragma epilog(omrfree_memory_below_bar,"MYEPILOG")

/*
 * Free memory allocated using STORAGE system macro.
 *
 * @params[in] address pointer to memory region to be freed
 *
 * @return non-zero if memory is not freed successfully, 0 otherwise.
 */
int
omrfree_memory_below_bar(void *address, long *length, int *subpool)
{
	int rc = 0;
	void *addr;
	long len;
	long sp;

	addr = address;
	len = *length;
	sp = *subpool;

	__asm(" STORAGE RELEASE,COND=YES,ADDR=(%1),LENGTH=(%2),SP=(%3),RTCD=(%0)"\
			:"=r"(rc):"r"(addr),"r"(len),"r"(sp));

	return rc;

}
