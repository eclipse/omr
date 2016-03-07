/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 * that use the IARV64 macro in omrvmem.c
 *
 * This file is compiled manually using the METAL-C compiler that was
 * introduced in z/OS V1R9. The generated output (j9iarv64.s) is then
 * inserted into omrvmem_support_above_bar.s which is compiled by our makefiles.
 *
 * omrvmem_support_above_bar.s indicates where to put the contents of j9iarv64.s.
 * Search for:
 * 		Insert contents of j9iarv64.s below
 *
 * *******
 * NOTE!!!!! You must strip the line numbers from any pragma statements!
 * *******
 *
 * It should be obvious, however, just to be clear be sure to omit the
 * first two lines from j9iarv64.s which will look something like:
 *
 *          TITLE '5694A01 V1.9 z/OS XL C
 *                     ./j9iarv64.c'
 *
 * To compile:
 *  xlc -S -qmetal -Wc,lp64 -qlongname j9iarv64.c
 *
 * z/OS V1R9 z/OS V1R9.0 Metal C Programming Guide and Reference:
 * 		http://publibz.boulder.ibm.com/epubs/pdf/ccrug100.pdf
 */

#pragma prolog(j9allocate_1M_fixed_pages,"MYPROLOG")
#pragma epilog(j9allocate_1M_fixed_pages,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(lgetstor));

/*
 * Allocate 1MB fixed pages using IARV64 system macro.
 * Memory allocated is freed using j9free_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 * @params[in] use2To32GArea 1 if memory request in for 2G-32G range, 0 otherwise
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * j9allocate_1M_fixed_pages(int *numMBSegments, int *use2To32GArea, const char * ttkn) {
	long segments;
	long origin;
	long use2To32G = *use2To32GArea;
	int  iarv64_rc = 0;
	__asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));

	segments = *numMBSegments;
	wgetstor = lgetstor;

	if (0 == use2To32G) {
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\
				"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
	} else {
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\
				"CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\
				"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
	}
	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(j9allocate_1M_pageable_pages_above_bar,"MYPROLOG")
#pragma epilog(j9allocate_1M_pageable_pages_above_bar,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(ngetstor));

/*
 * Allocate 1MB pageable pages above 2GB bar using IARV64 system macro.
 * Memory allocated is freed using j9free_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 * @params[in] use2To32GArea 1 if memory request in for 2G-32G range, 0 otherwise
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * j9allocate_1M_pageable_pages_above_bar(int *numMBSegments, int *use2To32GArea, const char * ttkn) {
	long segments;
	long origin;
	long use2To32G = *use2To32GArea;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));

	segments = *numMBSegments;
	wgetstor = ngetstor;

	if (0 == use2To32G) {
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\
				"PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
	} else {
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTO32G=YES,"\
				"PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
	}

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(j9allocate_2G_pages,"MYPROLOG")
#pragma epilog(j9allocate_2G_pages,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(ogetstor));

/*
 * Allocate 2GB fixed pages using IARV64 system macro.
 * Memory allocated is freed using j9free_memory_above_bar().
 *
 * @params[in] num2GBUnits Number of 2GB units to be allocated
 * @params[in] use2To32GArea 1 if memory request in for 2G-32G range, 0 otherwise
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * j9allocate_2G_pages(int *num2GBUnits, int *use2To32GArea, const char * ttkn) {
	long units;
	long origin;
	long use2To32G = *use2To32GArea;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));

	units = *num2GBUnits;
	wgetstor = ogetstor;

	if (0 == use2To32G) {
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\
				"PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn));
	} else {
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTO32G=YES,"\
				"PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn));
	}

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(j9allocate_4K_pages_in_2to32G_area,"MYPROLOG")
#pragma epilog(j9allocate_4K_pages_in_2to32G_area,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(mgetstor));

/*
 * Allocate 4KB pages in 2G-32G range using IARV64 system macro.
 * Memory allocated is freed using j9free_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * j9allocate_4K_pages_in_2to32G_area(int *numMBSegments, const char * ttkn) {
	long segments;
	long origin;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));

	segments = *numMBSegments;
	wgetstor = mgetstor;

	__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\
			"CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\
			"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
			::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(j9allocate_4K_pages_above_bar,"MYPROLOG")
#pragma epilog(j9allocate_4K_pages_above_bar,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(rgetstor));

/*
 * Allocate 4KB pages using IARV64 system macro.
 * Memory allocated is freed using j9free_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * j9allocate_4K_pages_above_bar(int *numMBSegments, const char * ttkn) {
	long segments;
	long origin;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));

	segments = *numMBSegments;
	wgetstor = rgetstor;

	__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\
			"CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\
			"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
			::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(j9free_memory_above_bar,"MYPROLOG")
#pragma epilog(j9free_memory_above_bar,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(pgetstor));

/*
 * Free memory allocated using IARV64 system macro.
 *
 * @params[in] address pointer to memory region to be freed
 *
 * @return non-zero if memory is not freed successfully, 0 otherwise.
 */
int j9free_memory_above_bar(void *address, const char * ttkn){
	void * xmemobjstart;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));

	xmemobjstart = address;
	wgetstor = pgetstor;

	__asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%3),RETCODE=%0,MF=(E,(%1))"\
			::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(ttkn));
	return iarv64_rc;
}

#pragma prolog(j9discard_data,"MYPROLOG")
#pragma epilog(j9discard_data,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,QGETSTOR)":"DS"(qgetstor));

/* Used to pass parameters to IARV64 DISCARDDATA in j9discard_data(). */
struct rangeList {
	void *startAddr;
	long pageCount;
};

/*
 * Discard memory allocated using IARV64 system macro.
 *
 * @params[in] address pointer to memory region to be discarded
 * @params[in] numFrames number of frames to be discarded. Frame size is 4KB.
 *
 * @return non-zero if memory is not discarded successfully, 0 otherwise.
 */
int j9discard_data(void *address, int *numFrames) {
	struct rangeList range;
	void *rangePtr;
	int iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));

	range.startAddr = address;
	range.pageCount = *numFrames;
	rangePtr = (void *)&range;
	wgetstor = qgetstor;

	__asm(" IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,"\
			"RANGLIST=(%1),RETCODE=%0,MF=(E,(%2))"\
			::"m"(iarv64_rc),"r"(&rangePtr),"r"(&wgetstor));

	return iarv64_rc;
}
