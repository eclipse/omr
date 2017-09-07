/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * that use the IARV64 macro in omrvmem.c
 *
 * This file is compiled manually using the METAL-C compiler that was
 * introduced in z/OS V1R9. The generated output (omriarv64.s) is then
 * inserted into omrvmem_support_above_bar.s which is compiled by our makefiles.
 *
 * omrvmem_support_above_bar.s indicates where to put the contents of omriarv64.s.
 * Search for:
 * 		Insert contents of omriarv64.s below
 *
 * *******
 * NOTE!!!!! You must strip the line numbers from any pragma statements!
 * *******
 *
 * It should be obvious, however, just to be clear be sure to omit the
 * first two lines from omriarv64.s which will look something like:
 *
 *          TITLE '5694A01 V1.9 z/OS XL C
 *                     ./omriarv64.c'
 *
 * To compile:
 *  xlc -S -qmetal -Wc,lp64 -qlongname omriarv64.c
 *
 * z/OS V1R9 z/OS V1R9.0 Metal C Programming Guide and Reference:
 * 		http://publibz.boulder.ibm.com/epubs/pdf/ccrug100.pdf
 */

#include "omriarv64.h"

#pragma prolog(omrallocate_1M_fixed_pages,"MYPROLOG")
#pragma epilog(omrallocate_1M_fixed_pages,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(lgetstor));

/*
 * Allocate 1MB fixed pages using IARV64 system macro.
 * Memory allocated is freed using omrfree_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0 - general, 1 - 2G-32G range, 2 - 2G-64G range
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * omrallocate_1M_fixed_pages(int *numMBSegments, int *userExtendedPrivateAreaMemoryType, const char * ttkn) {
	long segments;
	long origin;
	long useMemoryType = *userExtendedPrivateAreaMemoryType;
	int  iarv64_rc = 0;
	__asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));

	segments = *numMBSegments;
	wgetstor = lgetstor;

	switch (useMemoryType) {
	case ZOS64_VMEM_ABOVE_BAR_GENERAL:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\
				"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	case ZOS64_VMEM_2_TO_32G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\
				"CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\
				"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	case ZOS64_VMEM_2_TO_64G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\
				"CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\
				"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	}

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(omrallocate_1M_pageable_pages_above_bar,"MYPROLOG")
#pragma epilog(omrallocate_1M_pageable_pages_above_bar,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(ngetstor));

/*
 * Allocate 1MB pageable pages above 2GB bar using IARV64 system macro.
 * Memory allocated is freed using omrfree_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0 - general, 1 - 2G-32G range, 2 - 2G-64G range
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * omrallocate_1M_pageable_pages_above_bar(int *numMBSegments, int *userExtendedPrivateAreaMemoryType, const char * ttkn) {
	long segments;
	long origin;
	long useMemoryType = *userExtendedPrivateAreaMemoryType;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));

	segments = *numMBSegments;
	wgetstor = ngetstor;

	switch (useMemoryType) {
	case ZOS64_VMEM_ABOVE_BAR_GENERAL:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\
				"PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	case ZOS64_VMEM_2_TO_32G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTO32G=YES,"\
				"PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	case ZOS64_VMEM_2_TO_64G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTO64G=YES,"\
				"PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	}

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(omrallocate_2G_pages,"MYPROLOG")
#pragma epilog(omrallocate_2G_pages,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(ogetstor));

/*
 * Allocate 2GB fixed pages using IARV64 system macro.
 * Memory allocated is freed using omrfree_memory_above_bar().
 *
 * @params[in] num2GBUnits Number of 2GB units to be allocated
 * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0 - general, 1 - 2G-32G range, 2 - 2G-64G range
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * omrallocate_2G_pages(int *num2GBUnits, int *userExtendedPrivateAreaMemoryType, const char * ttkn) {
	long units;
	long origin;
	long useMemoryType = *userExtendedPrivateAreaMemoryType;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));

	units = *num2GBUnits;
	wgetstor = ogetstor;

	switch (useMemoryType) {
	case ZOS64_VMEM_ABOVE_BAR_GENERAL:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\
				"PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn));
		break;
	case ZOS64_VMEM_2_TO_32G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTO32G=YES,"\
				"PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn));
		break;
	case ZOS64_VMEM_2_TO_64G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTO64G=YES,"\
				"PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\
				"ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn));
		break;
	}

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYPROLOG")
#pragma epilog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(mgetstor));

/*
 * Allocate 4KB pages in 2G-32G range using IARV64 system macro.
 * Memory allocated is freed using omrfree_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0 - general, 1 - 2G-32G range, 2 - 2G-64G range
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * omrallocate_4K_pages_in_userExtendedPrivateArea(int *numMBSegments, int *userExtendedPrivateAreaMemoryType, const char * ttkn) {
	long segments;
	long origin;
	long useMemoryType = *userExtendedPrivateAreaMemoryType;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));

	segments = *numMBSegments;
	wgetstor = mgetstor;

	switch (useMemoryType) {
	case ZOS64_VMEM_ABOVE_BAR_GENERAL:
		break;
	case ZOS64_VMEM_2_TO_32G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\
				"CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\
				"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	case ZOS64_VMEM_2_TO_64G:
		__asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\
				"CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\
				"SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\
				::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(ttkn));
		break;
	}

	if (0 != iarv64_rc) {
		return (void *)0;
	}
	return (void *)origin;
}

#pragma prolog(omrallocate_4K_pages_above_bar,"MYPROLOG")
#pragma epilog(omrallocate_4K_pages_above_bar,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(rgetstor));

/*
 * Allocate 4KB pages using IARV64 system macro.
 * Memory allocated is freed using omrfree_memory_above_bar().
 *
 * @params[in] numMBSegments Number of 1MB segments to be allocated
 *
 * @return pointer to memory allocated, NULL on failure.
 */
void * omrallocate_4K_pages_above_bar(int *numMBSegments, const char * ttkn) {
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

#pragma prolog(omrfree_memory_above_bar,"MYPROLOG")
#pragma epilog(omrfree_memory_above_bar,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(pgetstor));

/*
 * Free memory allocated using IARV64 system macro.
 *
 * @params[in] address pointer to memory region to be freed
 *
 * @return non-zero if memory is not freed successfully, 0 otherwise.
 */
int omrfree_memory_above_bar(void *address, const char * ttkn){
	void * xmemobjstart;
	int  iarv64_rc = 0;

	__asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));

	xmemobjstart = address;
	wgetstor = pgetstor;

	__asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%3),RETCODE=%0,MF=(E,(%1))"\
			::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(ttkn));
	return iarv64_rc;
}

#pragma prolog(omrdiscard_data,"MYPROLOG")
#pragma epilog(omrdiscard_data,"MYEPILOG")

__asm(" IARV64 PLISTVER=MAX,MF=(L,QGETSTOR)":"DS"(qgetstor));

/* Used to pass parameters to IARV64 DISCARDDATA in omrdiscard_data(). */
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
int omrdiscard_data(void *address, int *numFrames) {
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
