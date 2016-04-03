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

/**
 * @internal @file
 * @ingroup Port
 * @brief iconv support helpers
 */

/* Use the version of nl_langinfo() that returns an EBCDIC string */
#define J9_USE_ORIG_EBCDIC_LANGINFO 1
#include "omriconvhelpers.h"
#include "ut_omrport.h"

#if defined(J9VM_PROVIDE_ICONV)
#include "omrportptb.h"
#include "omrportpriv.h"

#if defined(J9ZOS390)
#pragma convlit(suspend)
#endif
const char *utf8 = "UTF-8";
#if defined(J9ZOS390)
const char *utf16 = "01200"; /* z/OS does not accept "UTF-16" */
#elif defined(OSX)
const char *utf16 = "UTF-16LE";
#else
const char *utf16 = "UTF-16";
#endif
const char *ebcdic = "IBM-1047";
#if defined(J9ZOS390)
#pragma convlit(resume)
#endif

#if defined(J9ZOS390)
/*
 * Open a convertor object and update the table if successful.
 * @note use only the const strings above to denote UTF-8, UTF-16, and EBCDIC
 * @return TRUE on success, FALSE on failure
 */
static BOOLEAN
openIconvDescriptor(iconv_t *globalConverter, MUTEX *globalConverterMutex, uint32_t index, const char *toCode, const char *fromCode, uint32_t *initializedConvertors)
{
	BOOLEAN success = TRUE;
	iconv_t tmpConverter = iconv_open(toCode, fromCode);
	if (J9VM_INVALID_ICONV_DESCRIPTOR == tmpConverter) {
		const char *newToCode = toCode;
		const char *newFromCode = fromCode;
		/*
		 * CMVC 199980 - Not all locales have UTF-16 convertors.
		 * If the locale is not supported or invalid, revert to ebcdic.
		 */
		if (utf16 == toCode) { /* the fromCode will be the locale. */
			newFromCode = ebcdic;
		} else if (utf16 == fromCode) { /* the toCode will be the locale. */
			newToCode = ebcdic;
		}
		tmpConverter = iconv_open(newToCode, newFromCode);
		if (J9VM_INVALID_ICONV_DESCRIPTOR == tmpConverter) {
			success = FALSE;
		}
	}
	if (success) {
		if (!MUTEX_INIT(globalConverterMutex[index])) {
			iconv_close(globalConverter[index]);
			success = FALSE;
		} else {
			globalConverter[index] = tmpConverter;
			*initializedConvertors |= (1 << index);
		}
	}
	return success;
}
#endif

/* There are  converters that are permanently cached
 * within the portLibraryGlobal structure
 * The cached converters are serialized via their respective mutex. The mutex is freed in iconv_free()
 * Populate the converter cache within portLibGlobal struct and their respective mutexes.
 *
 * If this function fails, vm will exit.
 *
 * @return 0 if successful, 1 otherwise
 */
int32_t
iconv_global_init(struct OMRPortLibrary *portLibrary)
{
	int32_t rc = 0;
#if defined(J9ZOS390)
	iconv_t *globalConverter = PPG_global_converter;
	MUTEX *globalConverterMutex = PPG_global_converter_mutex;
	uint32_t initializedConvertors = 0;
	BOOLEAN success = TRUE;
	/* NOTE: using original nl_langinfo implementation that returns an EBCDIC string
	 * by defining J9_USE_ORIG_EBCDIC_LANGINFO which is checked in a2e/headers/langinfo.h
	 */
	char *langinfo = nl_langinfo(CODESET);

	Assert_PRT_true(UNCACHED_ICONV_DESCRIPTOR <= 32); /* ensure we have the right number of bits */
	success =
		openIconvDescriptor(globalConverter, globalConverterMutex, OMRPORT_UTF8_TO_EBCDIC_ICONV_DESCRIPTOR, ebcdic, utf8, &initializedConvertors)
		&& openIconvDescriptor(globalConverter, globalConverterMutex, OMRPORT_EBCDIC_TO_UTF8_ICONV_DESCRIPTOR, utf8, ebcdic, &initializedConvertors)
		&& openIconvDescriptor(globalConverter, globalConverterMutex, OMRPORT_UTF8_TO_LANG_ICONV_DESCRIPTOR, langinfo, utf8, &initializedConvertors)
		&& openIconvDescriptor(globalConverter, globalConverterMutex, OMRPORT_LANG_TO_UTF8_ICONV_DESCRIPTOR, utf8, langinfo, &initializedConvertors)
		&& openIconvDescriptor(globalConverter, globalConverterMutex, OMRPORT_UTF16_TO_LANG_ICONV_DESCRIPTOR, langinfo, utf16, &initializedConvertors)
		&& openIconvDescriptor(globalConverter, globalConverterMutex, OMRPORT_LANG_TO_UTF16_ICONV_DESCRIPTOR, utf16, langinfo, &initializedConvertors);

	if (success) {
		rc = 0;
	} else { /* clean up */
		uint32_t iconvIndex = 0;
		for (iconvIndex = OMRPORT_FIRST_ICONV_DESCRIPTOR; iconvIndex < UNCACHED_ICONV_DESCRIPTOR; ++iconvIndex) {
			if (J9_ARE_ANY_BITS_SET(initializedConvertors, 1 << iconvIndex)) {
				MUTEX_DESTROY(globalConverterMutex[iconvIndex]);
				iconv_close(globalConverter[iconvIndex]);
			}
		}
		rc = 1;
	}
#endif
	return rc;
}

/*
 *  Free/destroy all that was allocated in iconv_global_init()
 */
void
iconv_global_destroy(struct OMRPortLibrary *portLibrary)
{
#if defined(J9ZOS390)
	if (PPG_global_converter_enabled) {
		iconv_t *globalConverter = PPG_global_converter;
		MUTEX *globalConverterMutex = PPG_global_converter_mutex;

		uint32_t iconvIndex = 0;
		for (iconvIndex = OMRPORT_FIRST_ICONV_DESCRIPTOR; iconvIndex < UNCACHED_ICONV_DESCRIPTOR; ++iconvIndex) {
			iconv_close(globalConverter[iconvIndex]);
			MUTEX_DESTROY(globalConverterMutex[iconvIndex]);
		}
	}
#endif
}

/**
 * @internal
 *
 * Allocates a iconv descriptor that converts character sequences from the fromCode character
 * encoding to the toCode character encoding.  The iconv descriptor may be stored in a per-thread
 * cache.  Caching is determined by available memory and the value of the converterName argument:
 *		cache(converterName) = converterName < UNCACHED_ICONV_DESCRIPTOR
 *
 * @param[in] converterName A J9IconvName indicating the intended use of the iconv descriptor.
 * @param[in] toCode		The target character encoding.
 * @param[in] fromCode		The source character encoding.
 *
 * @return J9VM_INVALID_ICONV_DESCRIPTOR on failure, otherwise a valid iconv descriptor.
 */
iconv_t
iconv_get(struct OMRPortLibrary *portLibrary, J9IconvName converterName, const char *toCode, const char *fromCode)
{
	/* ptBuffer is NULL if the iconv descriptor should not be cached or the per-thread buffer could not be allocated.
	 * We attempt to allocate an uncached iconv descriptor in those cases, and use ptBuffer == NULL as a check for caching.
	 */
	PortlibPTBuffers_t ptBuffer = NULL;
	iconv_t converter = J9VM_INVALID_ICONV_DESCRIPTOR;
#if defined(J9ZOS390)

	if (PPG_global_converter_enabled) {
		/* NOTE: using original nl_langinfo implementation that returns an EBCDIC string
		 * by defining J9_USE_ORIG_EBCDIC_LANGINFO which is checked in a2e/headers/langinfo.h */
		char *langinfo = nl_langinfo(CODESET);
		iconv_t *globalConverter = PPG_global_converter;
		MUTEX *globalConverterMutex = PPG_global_converter_mutex;

		/* this part is only pertinent if -Xipt argument is not specified. It is a hack for CICS
		 * the logic is based on the toCode and fromCode. If it correspond to one of the four converters
		 * opened, then per-thread cache is not consulted.
		 */
		int32_t index = UNCACHED_ICONV_DESCRIPTOR;
		if (strcmp(fromCode, utf8) == 0) {
			if (strcmp(toCode, ebcdic) == 0) {
				index = OMRPORT_UTF8_TO_EBCDIC_ICONV_DESCRIPTOR;
			} else if (strcmp(toCode, langinfo) == 0) {
				index = OMRPORT_UTF8_TO_LANG_ICONV_DESCRIPTOR;
			}
		} else if (strcmp(fromCode, utf16) == 0) {
			if (strcmp(toCode, langinfo) == 0) {
				index = OMRPORT_UTF16_TO_LANG_ICONV_DESCRIPTOR;
			}
		} else if (strcmp(fromCode, ebcdic) == 0) {
			if ((strcmp(toCode, "UTF8") == 0) || (strcmp(toCode, utf8) == 0)) {
				index = OMRPORT_EBCDIC_TO_UTF8_ICONV_DESCRIPTOR;
			}
		} else if (strcmp(fromCode, langinfo) == 0) {
			if (strcmp(toCode, utf8) == 0) { /* requesting current codeset -> UTF8 converter */
				index = OMRPORT_LANG_TO_UTF8_ICONV_DESCRIPTOR;
			} else if (strcmp(toCode, utf16) == 0) { /* requesting current codeset -> UTF16 converter */
				index = OMRPORT_LANG_TO_UTF16_ICONV_DESCRIPTOR;
			}
		}
		if (UNCACHED_ICONV_DESCRIPTOR != index) { /* found a match */
			MUTEX_ENTER(globalConverterMutex[index]);
			return globalConverter[index];
		}
	}
#endif

	if (converterName < UNCACHED_ICONV_DESCRIPTOR) {
		ptBuffer = (PortlibPTBuffers_t) omrport_tls_get(portLibrary);
	}

	if (NULL != ptBuffer) {
		converter = ptBuffer->converterCache[converterName];
	}

	if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
		converter = iconv_open(toCode, fromCode);

		if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
			return converter;
		}

		if (NULL != ptBuffer) {
			ptBuffer->converterCache[converterName] = converter;
		}
	}
	return converter;
}

void
iconv_free(struct OMRPortLibrary *portLibrary, J9IconvName converterName, iconv_t converter)
{
#if defined(J9ZOS390)

	if (PPG_global_converter_enabled) {
		iconv_t *globalConverter = PPG_global_converter;
		MUTEX *globalConverterMutex = PPG_global_converter_mutex;
		int32_t index = UNCACHED_ICONV_DESCRIPTOR;
		/* this part is only pertinent if -Xipt argument is not specified. It is a hack for CICS
		 * the logic is based on comparing the converter to be freed to the existing converter,
		 * if a match is found within the cached converters, only the corresponding mutex is exited
		 */
		for (index = 0; index < UNCACHED_ICONV_DESCRIPTOR; ++index) {
			if (converter == globalConverter[index]) {
				MUTEX_EXIT(globalConverterMutex[index]);
				return;
			}
		}
	}
#endif
	if (converterName < UNCACHED_ICONV_DESCRIPTOR) {
		PortlibPTBuffers_t ptBuffer = (PortlibPTBuffers_t) omrport_tls_get(portLibrary);
		if (ptBuffer && (converter == ptBuffer->converterCache[converterName])) {
			/* Don't close the iconv descriptor if it is cached. */
			return;
		}
	}

	if (J9VM_INVALID_ICONV_DESCRIPTOR != converter) {
		iconv_close(converter);
	}
}

#endif /* J9VM_PROVIDE_ICONV */
