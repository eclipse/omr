/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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

/*
 * This file is used to generate the HLASM corresponding to the C calls
 * that use the STORAGE macro in omrvmem.c
 *
 * This file is compiled manually using the METAL-C compiler that was
 * introduced in z/OS V1R9. The generated output (j9storage.s) is then
 * inserted into omrvmem_support_below_bar_[31|64].s which is compiled by our makefiles.
 *
 * omrvmem_support_below_bar_[31|64].s indicates where to put the contents of j9storage.s.
 * Search for:
 * 		Insert contents of j9storage.s below
 *
 * *******
 * NOTE!!!!! You must strip the line numbers from any pragma statements!
 * 		(CMVC 140560)
 * *******
 *
 * It should be obvious, however, just to be clear be sure to omit the
 * first two lines from j9storage.s which will look something like:
 *
 *          TITLE '5694A01 V1.9 z/OS XL C
 *                     ./j9storage.c'
 *
 * To compile for 64-bit use:
 *  xlc -S -qmetal -Wc,lp64 -qlongname j9storage.c
 *
 * To compile for 31-bit use:
 *  xlc -S -qmetal -qlongname j9storage.c
 *
 * z/OS V1R9 elements and features:
 * 		http://www-03.ibm.com/systems/z/os/zos/bkserv/r9pdf/index.html
 * 			- One stop shopping for all things z/OS.
 *
 * z/OS V1R9 z/OS V1R9.0 Metal C Programming Guide and Reference:
 * 		http://publibz.boulder.ibm.com/epubs/pdf/ccrug100.pdf
 *
 */

#pragma prolog(j9allocate_1M_pageable_pages_below_bar,"MYPROLOG")
#pragma epilog(j9allocate_1M_pageable_pages_below_bar,"MYEPILOG")

/*
 * Allocate 1MB pageable pages below 2GB bar using STORAGE system macro.
 * Memory allocated is freed using j9free_memory_below_bar().
 *
 * @params[in] numBytes Number of bytes to be allocated
 * @params[in] subpool subpool number to be used
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void *
j9allocate_1M_pageable_pages_below_bar(long *numBytes, int *subpool)
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

#pragma prolog(j9allocate_4K_pages_below_bar,"MYPROLOG")
#pragma epilog(j9allocate_4K_pages_below_bar,"MYEPILOG")

/*
 * Allocate 4K pages below 2GB bar using STORAGE system macro.
 * Memory allocated is freed using j9free_memory_below_bar().
 *
 * @params[in] numBytes Number of bytes to be allocated
 * @params[in] subpool subpool number to be used
 *
 * @return pointer to memory allocated, NULL on failure. Returned value is guaranteed to be page aligned.
 */
void *
j9allocate_4K_pages_below_bar(long *numBytes, int *subpool)
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

#pragma prolog(j9free_memory_below_bar,"MYPROLOG")
#pragma epilog(j9free_memory_below_bar,"MYEPILOG")

/*
 * Free memory allocated using STORAGE system macro.
 *
 * @params[in] address pointer to memory region to be freed
 *
 * @return non-zero if memory is not freed successfully, 0 otherwise.
 */
int
j9free_memory_below_bar(void *address, long *length, int *subpool)
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
