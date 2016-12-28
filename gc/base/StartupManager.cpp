/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


#include <string.h>

#include "StartupManager.hpp"
//#if defined(OMR_GC)
//#include "GCExtensionsBase.hpp"
//#include "ConfigurationFlat.hpp"
//#endif /* OMR_GC */
//
//#define OMR_GC_BUFFER_SIZE 256
//#define OMR_XMS "-Xms"
//#define OMR_XMS_LENGTH 4
//#define OMR_XMX "-Xmx"
//#define OMR_XMX_LENGTH 4
//#if defined(OMR_GC_MODRON_COMPACTION)
//#define OMR_XCOMPACTGC "-Xcompactgc"
//#define OMR_XCOMPACTGC_LENGTH 11
//#endif /* OMR_GC_MODRON_COMPACTION */
//#if defined(OMR_GC_MODRON_SCAVENGER)
//#define OMR_XGCPOLICY "-Xgcpolicy:"
//#define OMR_XGCPOLICY_LENGTH 11
//#define OMR_GCPOLICY_GENCON "gencon"
//#define OMR_GCPOLICY_GENCON_LENGTH 6
//#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
//#define OMR_XVERBOSEGCLOG "-Xverbosegclog:"
//#define OMR_XVERBOSEGCLOG_LENGTH 15
//#define OMR_XGCBUFFERED_LOGGING "-Xgc:bufferedLogging"
//#define OMR_XGCBUFFERED_LOGGING_LENGTH 20
//#define OMR_XGCTHREADS "-Xgcthreads"
//#define OMR_XGCTHREADS_LENGTH 11

uintptr_t
MM_StartupManager::getUDATAValue(char *option, uintptr_t *outputValue)
{
	char buffer[OMR_GC_BUFFER_SIZE];
	int count = 0;
	size_t optionLength = strlen(option);

	/* make sure not to buffer overflow */
	if (optionLength > (OMR_GC_BUFFER_SIZE - 1)) {
		return -1;
	}

	while (true) {
		char current = option[count];
		if (('0' <= current) && ('9' >= current)) {
			buffer[count] = current;
		} else {
			break;
		}
		count += 1;
	}
	buffer[count] = '\0';

	*outputValue = (uintptr_t) atoi(buffer);

	return count;
}

bool
MM_StartupManager::getUDATAMemoryValue(char *option, uintptr_t *convertedValue)
{
	size_t count = 0;
	size_t optionLength = strlen(option);
	uintptr_t actualValue = 0;

	count = getUDATAValue(option, &actualValue);

	/* if there was not at least one numeric value return failure */
	if (count < 1) {
		return false;
	}

	/* if there is more than one character after the last numeric value
	then this is a malformed option */
	if (optionLength > (count + 1)) {
		return false;
	}

	switch (option[count]) {
	case 'g':
	case 'G':
		/* intentional fall through */
		actualValue *= 1024;
	case 'm':
	case 'M':
		/* intentional fall through */
		actualValue *= 1024;
	case 'k':
	case 'K':
		/* intentional fall through */
		actualValue *= 1024;
	case 'b':
	case 'B':
		/* nothing */
		break;
	default:
		return false;
	}

	*convertedValue = actualValue;
	return true;
}

bool
MM_StartupManager::parseGcOptions(MM_GCExtensionsBase *extensions)
{
	OMRPORT_ACCESS_FROM_OMRVM(extensions->getOmrVM());
	char *gcOptions = getOptions();
	bool result = true;
	if (NULL != gcOptions) {
		size_t end = 0;
		char *options = gcOptions;

		do {
			char option[OMR_GC_BUFFER_SIZE];
			char *nextSpace = strchr(options, ' ');
			if (NULL == nextSpace) {
				/* handle last option */
				end = strlen(options);
				if (0 == end) {
					/* ending in space so exit */
					break;
				}
			} else {
				end = (size_t)(nextSpace - options);
				if (0 == end) {
					/* starting with a space or multiple spaces in a row so skip to next character */
					options += 1;
					continue;
				}
			}
			if (end > (OMR_GC_BUFFER_SIZE - 1)) {
				omrtty_printf("Error parsing OMR GC options: '%s'\n", gcOptions);
				result = false;
				break;
			}
			strncpy(option, options, end);
			option[end] = '\0';
			if(!handleOption(extensions, option)) {
				omrtty_printf("Error parsing OMR GC options: '%s'\n", gcOptions);
				result = false;
				break;
			}
			options = options + end;
			if ('\0' == options[0]) {
				/* finished string */
				break;
			}
			/* move past the space */
			options += 1;
		} while (true);
	}

	if (result) {
		result = parseLanguageOptions(extensions);
	}

	return result;
}

bool
MM_StartupManager::loadGcOptions(MM_GCExtensionsBase *extensions)
{
	OMRPORT_ACCESS_FROM_OMRVM(extensions->getOmrVM());

#if defined(OMR_GC_MODRON_COMPACTION)
	/* Disable compact by default. */
	extensions->noCompactOnGlobalGC = 1;
	extensions->compactOnGlobalGC = 0;
	extensions->nocompactOnSystemGC = 1;
	extensions->compactOnSystemGC = 0;
#endif /* OMR_GC_MODRON_COMPACTION */

	uintptr_t *pageSizes = omrvmem_supported_page_sizes();
	uintptr_t *pageFlags = omrvmem_supported_page_flags();

	extensions->requestedPageSize = pageSizes[0];
	extensions->requestedPageFlags = pageFlags[0];

#if defined(OMR_ENV_DATA64)
#define HEAP_ALIGNMENT 1024
#else
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#define HEAP_ALIGNMENT 512
#else
#define HEAP_ALIGNMENT 256
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#endif /* OMR_ENV_DATA64 */

	extensions->heapAlignment = HEAP_ALIGNMENT;

	assert(0 != defaultMinHeapSize);
	assert(0 != defaultMaxHeapSize);
	assert(defaultMinHeapSize <= defaultMaxHeapSize);

	/* Set defaults to support Standard GC in OMR */
	extensions->initialMemorySize = defaultMinHeapSize;
	extensions->minNewSpaceSize = 0;
	extensions->newSpaceSize = 0;
	extensions->maxNewSpaceSize = 0;
	extensions->minOldSpaceSize = defaultMinHeapSize;
	extensions->oldSpaceSize = defaultMinHeapSize;
	extensions->maxOldSpaceSize = defaultMaxHeapSize;
	extensions->memoryMax = defaultMaxHeapSize;
	extensions->maxSizeDefaultMemorySpace = defaultMaxHeapSize;

	/* Now override defaults with specified settings, if any */
	bool result = parseGcOptions(extensions);

	return result;
}

//bool
//MM_StartupManager::handleOption(MM_GCExtensionsBase *extensions, char *option)
//{
//	OMRPORT_ACCESS_FROM_OMRVM(extensions->getOmrVM());
//	bool result = true;
//	if (0 == strncmp(option, OMR_XMS, OMR_XMS_LENGTH)) {
//		uintptr_t value = 0;
//		if (!getUDATAMemoryValue(option + OMR_XMS_LENGTH, &value)) {
//			result = false;
//		} else {
//			extensions->initialMemorySize = value;
//			extensions->minOldSpaceSize = value;
//			extensions->oldSpaceSize = value;
//		}
//	} else if (0 == strncmp(option, OMR_XMX, OMR_XMX_LENGTH)) {
//		uintptr_t value = 0;
//		if (!getUDATAMemoryValue(option + OMR_XMX_LENGTH, &value)) {
//			result = false;
//		} else {
//			extensions->maxOldSpaceSize = value;
//			extensions->memoryMax = value;
//			extensions->maxSizeDefaultMemorySpace = value;
//		}
//	}
//#if defined(OMR_GC_MODRON_COMPACTION)
//	else if (0 == strncmp(option, OMR_XCOMPACTGC, OMR_XCOMPACTGC_LENGTH)) {
//		extensions->noCompactOnGlobalGC = 0;
//		extensions->compactOnGlobalGC = 0;
//		extensions->nocompactOnSystemGC = 0;
//		extensions->compactOnSystemGC = 0;
//	}
//#endif /* OMR_GC_MODRON_COMPACTION */
//	else if (0 == strncmp(option, OMR_XVERBOSEGCLOG, OMR_XVERBOSEGCLOG_LENGTH)) {
//		verboseFileName = (char *) omrmem_allocate_memory(strlen(option+OMR_XVERBOSEGCLOG_LENGTH)+1, OMRMEM_CATEGORY_MM);
//		if (NULL == verboseFileName) {
//			result = false;
//		} else {
//			strcpy(verboseFileName, option + OMR_XVERBOSEGCLOG_LENGTH);
//		}
//	}
//	else if (0 == strncmp(option, OMR_XGCBUFFERED_LOGGING, OMR_XGCBUFFERED_LOGGING_LENGTH)) {
//		extensions->bufferedLogging = true;
//	}
//#if defined(OMR_GC_MORDON_SCAVENGER)
//	else if (0 == strncmp(option, OMR_XGCPOLICY, OMR_XGCPOLICY_LENGTH)) {
//		char *gcpolicy = option + OMR_XGCPOLICY_LENGTH;
//		if (0 == strncmp(gcpolicy, OMR_GCPOLICY_GENCON, OMR_GCPOLICY_GENCON_LENGTH)) {
//			/* this is disabled by default -- enable scavenger here */
//			extensions->scavengerEnabled = true;
//		} else {
//			result = false;
//		}
//	}
//#endif /* defined(OMR_GC_MORDON_SCAVENGER) */
//	else if (0 == strncmp(option, OMR_XGCTHREADS, OMR_XGCTHREADS_LENGTH)) {
//		uintptr_t forcedThreadCount = 0;
//		if (0 >= getUDATAValue(option + OMR_XGCTHREADS_LENGTH, &forcedThreadCount)) {
//			result = false;
//		} else {
//			extensions->gcThreadCount = forcedThreadCount;
//			extensions->gcThreadCountForced = true;
//		}
//	} else {
//		/* unknown option */
//		result = false;
//	}
//
//	return result;
//}

void
MM_StartupManager::tearDown(void)
{
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	if (NULL != verboseFileName) {
		omrmem_free_memory(verboseFileName);
		verboseFileName = NULL;
	}
}

bool
MM_StartupManager::isVerboseEnabled(void)
{
	return (NULL != verboseFileName);
}

char *
MM_StartupManager::getVerboseFileName(void)
{
	return verboseFileName;
}
//
//MM_Configuration *
//MM_StartupManager::createConfiguration(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface *cli)
//{
//	/* When multiple configurations are supported, this call can return different
//	 * concrete implementations based on arguments previously parsed by handleOption().
//	 */
//	return MM_ConfigurationFlat::newInstance(env, cli);
//}
