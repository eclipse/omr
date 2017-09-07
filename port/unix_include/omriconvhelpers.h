/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 *   Multiple authors (IBM Corp.) - z/TPF platform initial port to OMR environment
 *******************************************************************************/

#ifndef omriconvhelpers_h_
#define omriconvhelpers_h_

#include <langinfo.h>
#include "omrport.h"
#include "omrmutex.h"

/* __STDC_ISO_10646__ indicates that the platform wchar_t encoding is Unicode */
/* but older versions of libc fail to set the flag, even though they are Unicode */
#if (!defined(__STDC_ISO_10646__) && !defined(LINUX) && !defined(OSX)) || defined(OMRZTPF)
#define J9VM_USE_ICONV
#endif /* !defined(__STDC_ISO_10646__) && !defined(LINUX) && !defined(OSX) */

#if defined(J9ZOS390)
#define J9VM_CACHE_ICONV_DESC
#include "atoe.h"
#endif
#include <iconv.h>
#define J9VM_INVALID_ICONV_DESCRIPTOR ((iconv_t)(-1))
#define J9VM_PROVIDE_ICONV

/* To add a new use of iconv, add a descriptor name to the J9IconvName enum.  Names before UNCACHED_ICONV_DESCRIPTOR
 * are cached and all others are uncached.  Usage pattern:
 *
 *	iconv_t converter = iconv_get(portLibrary, MY_ICONV_DESCRIPTOR, "UTF-8", nl_langinfo(CODESET));
 *	if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
 *		handle error
 *	}
 *	...
 *	iconv_free(portLibrary, MY_ICONV_DESCRIPTOR, converter);
 */

typedef enum {
#if defined(J9ZOS390)
	J9NLS_ICONV_DESCRIPTOR,
#endif /* J9ZOS390 */
	J9FILETEXT_ICONV_DESCRIPTOR,
	J9SL_ICONV_DESCRIPTOR,
	OMRPORT_UTF16_TO_LANG_ICONV_DESCRIPTOR,
	OMRPORT_LANG_TO_UTF16_ICONV_DESCRIPTOR,
#if defined(J9VM_CACHE_ICONV_DESC)
	/* These converters ARE redundant to the converters above,
	 * however, it only takes a few extra bytes of memory.
	 * These are useless if -Xipt is defined.
	 */
	OMRPORT_FIRST_ICONV_DESCRIPTOR, /* declare starting point for iteration */
	OMRPORT_UTF8_TO_EBCDIC_ICONV_DESCRIPTOR = OMRPORT_FIRST_ICONV_DESCRIPTOR,
	OMRPORT_UTF8_TO_LANG_ICONV_DESCRIPTOR,
	OMRPORT_EBCDIC_TO_UTF8_ICONV_DESCRIPTOR,
#endif
	OMRPORT_LANG_TO_UTF8_ICONV_DESCRIPTOR,
	/* Add iconv descriptors that should be cached here. */
	UNCACHED_ICONV_DESCRIPTOR,
	/* Add iconv descriptors that should not be cached here. */
	J9SYSINFO_ICONV_DESCRIPTOR
} J9IconvName;

int32_t iconv_global_init(struct OMRPortLibrary *portLibrary);
void iconv_global_destroy(struct OMRPortLibrary *portLibrary);
iconv_t iconv_get(struct OMRPortLibrary *portLibrary, J9IconvName converterName, const char *toCode, const char *fromCode);

void iconv_free(struct OMRPortLibrary *portLibrary, J9IconvName converterName, iconv_t converter);

#endif /*omriconvhelpers_h_*/
