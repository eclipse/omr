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
 * @file
 * @ingroup Port
 * @brief Native language support
 * @deprecated NLS API is deprecated.
 */

#include "omrcomp.h"
#include "omrport.h"
#include "omrportpriv.h"
#include "omrthread.h"
#include "omrnlshelpers.h"
#include "portnls.h"
#include "omrstdarg.h"
#include "ut_omrport.h"

#include <stdlib.h>
#include <string.h>

#ifdef J9ZOS390
#include "omriconvhelpers.h"
#endif

#if defined(J9OS_I5)
#include "Xj9I5OSInterface.H"
#endif

static char *build_catalog_name(struct OMRPortLibrary *portLibrary, int32_t usePath, int32_t useDepth);
static void free_catalog(struct OMRPortLibrary *portLibrary);
char *read_from_catalog(struct OMRPortLibrary *portLibrary, intptr_t fd, char *buf, intptr_t bufsize);
static void open_catalog(struct OMRPortLibrary *portLibrary);

static const char *nlsh_lookup(struct OMRPortLibrary *portLibrary, uint32_t module_name, uint32_t message_num);
static J9NLSHashEntry *nls_allocateHashEntry(struct OMRPortLibrary *portLibrary, uint32_t module_name, uint32_t message_num, const char *message, uint32_t sizeOfMessage);
static const char *parse_catalog(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, const char *default_string);
static void nlsh_insert(struct OMRPortLibrary *portLibrary, J9NLSHashEntry *entry);
static void writeSyslog(struct OMRPortLibrary *portLibrary, uintptr_t flags, const char *format, va_list args);
static void convertModuleName(uint32_t module_name, uint8_t *module_str);
static uint32_t nlsh_hash(uint32_t module_name, uint32_t message_num);


/* a sample key */
#define J9NLS_EXEMPLAR "XXXX000"

#define BUF_SIZE 1024

/**
 * Set the language, region, and variant of the locale.
 *
 * @param[in] portLibrary The port library
 * @param[in] lang - the language of the locale (e.g., "en"), 2 characters or less
 * @param[in] region - the region of the locale (e.g., "US"), 2 characters or less
 * @param[in] variant - the variant of the locale (e.g., "boont"), 31 characters or less
 *
 * @deprecated NLS API is deprecated.
 */
void
j9nls_set_locale(struct OMRPortLibrary *portLibrary, const char *lang, const char *region, const char *variant)
{
	J9NLSDataCache *nls;
	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return;
	}
	nls = &portLibrary->portGlobals->nls_data;
#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_set_locale\n");
#endif

	omrthread_monitor_enter(nls->monitor);

	if (lang && strlen(lang) <= 2) {
		strcpy(nls->language, lang);
	}
	if (region && strlen(region) <= 2) {
		strcpy(nls->region, region);
	}
	if (variant && strlen(variant) <= 31) {
		strcpy(nls->variant, variant);
	}

	omrthread_monitor_exit(nls->monitor);
}

/**
 * Return the string representing the currently set language.
 *
 * @param[in] portLibrary The port library
 *
 * @return language string
 *
 * @deprecated NLS API is deprecated.
 */
const char *
j9nls_get_language(struct OMRPortLibrary *portLibrary)
{
	J9NLSDataCache *nls;
#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_get_language\n");
#endif
	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return "en";
	}
	nls = &portLibrary->portGlobals->nls_data;


	return nls->language;
}

/**
 * Return the string representing the currently set region.
 *
 * @param[in] portLibrary The port library
 *
 * @return region string
 *
 * @deprecated NLS API is deprecated.
 */
const char *
j9nls_get_region(struct OMRPortLibrary *portLibrary)
{
	J9NLSDataCache *nls;
#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_get_region\n");
#endif
	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return "US";
	}
	nls = &portLibrary->portGlobals->nls_data;

	return nls->region;

}
/**
 * Return the string representing the currently set variant.
 *
 * @param[in] portLibrary The port library
 *
 * @return variant string
 *
 * @deprecated NLS API is deprecated.
 */
const char *
j9nls_get_variant(struct OMRPortLibrary *portLibrary)
{
	J9NLSDataCache *nls;
#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_get_variant\n");
#endif
	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return "";
	}
	nls = &portLibrary->portGlobals->nls_data;

	return nls->variant;
}

/**
 * Write NLS message to the syslog.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - to indicate what type of message (e.g., ERROR) and whether a newline is required
 * @param[in] format - the format string
 * @param[in] args - arguments used in the NLS message format
 *
 * @deprecated NLS API is deprecated.
 */
static void
writeSyslog(struct OMRPortLibrary *portLibrary, uintptr_t flags, const char *format, va_list args)
{
	char outputBuffer[512];
	char *allocatedBuffer;
	uintptr_t numberWritten, length;
	va_list copyOfArgs;

	BOOLEAN options_info = ((PPG_syslog_flags & J9NLS_INFO) == J9NLS_INFO);
	BOOLEAN options_warn = ((PPG_syslog_flags & J9NLS_WARNING) == J9NLS_WARNING);
	BOOLEAN options_errr = ((PPG_syslog_flags & J9NLS_ERROR) == J9NLS_ERROR);
	BOOLEAN options_cnfg = ((PPG_syslog_flags & J9NLS_CONFIG) == J9NLS_CONFIG);
	BOOLEAN options_vitl = ((PPG_syslog_flags & J9NLS_VITAL) == J9NLS_VITAL);

	BOOLEAN message_info = ((flags & J9NLS_INFO) == J9NLS_INFO);
	BOOLEAN message_warn = ((flags & J9NLS_WARNING) == J9NLS_WARNING);
	BOOLEAN message_errr = ((flags & J9NLS_ERROR) == J9NLS_ERROR);
	BOOLEAN message_cnfg = ((flags & J9NLS_CONFIG) == J9NLS_CONFIG);
	BOOLEAN message_vitl = ((flags & J9NLS_VITAL) == J9NLS_VITAL);

	uintptr_t effectiveSyslogFlag = 0;

	BOOLEAN printable = 0;

	/* Normalise the log level to the most serious one given */
	if (message_errr) {
		effectiveSyslogFlag = J9NLS_ERROR;
	} else if (message_warn) {
		effectiveSyslogFlag = J9NLS_WARNING;
	} else if (message_info) {
		effectiveSyslogFlag = J9NLS_INFO;
	}

	if (effectiveSyslogFlag == 0) {
		return;
	}

	/* Determine if this message is of some interest to us */
	printable |= message_errr && options_errr;
	printable |= message_warn && options_warn;
	printable |= message_info && options_info;
	printable |= message_cnfg && options_cnfg;
	printable |= message_vitl && options_vitl;

	if (printable == 0) {
		return;
	}

	/* Attempt to write string to on-stack buffer */
	COPY_VA_LIST(copyOfArgs, args);
	numberWritten = portLibrary->str_vprintf(portLibrary, outputBuffer, sizeof(outputBuffer), format, copyOfArgs);

	/* str_vprintf returns number characters written excluding the null terminator */
	if (sizeof(outputBuffer) > (numberWritten + 1)) {
		portLibrary->syslog_write(portLibrary, effectiveSyslogFlag, outputBuffer);
		return;
	}

	/* Didn't fit into outputBuffer - find out how long the buffer needs to be */
	COPY_VA_LIST(copyOfArgs, args);
	length = portLibrary->str_vprintf(portLibrary, NULL, (uint32_t)(-1), format, copyOfArgs);
	length += 1; /* For the \0 */

	/* Allocate buffer from the heap */
	allocatedBuffer = portLibrary->mem_allocate_memory(portLibrary, length, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == allocatedBuffer) {
		/* Might as well write the truncated string out */
		portLibrary->syslog_write(portLibrary, effectiveSyslogFlag, outputBuffer);
		return;
	}

	portLibrary->str_vprintf(portLibrary, allocatedBuffer, length, format, args);

	portLibrary->syslog_write(portLibrary, effectiveSyslogFlag, allocatedBuffer);

	portLibrary->mem_free_memory(portLibrary, allocatedBuffer);
}

/**
 * Print a formatted NLS message.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - to indicate what type of message (e.g., ERROR) and whether a newline is required
 * @param[in] module_name - the module identifier of the NLS message
 * @param[in] message_num - the NLS message number within the module
 * @param[in] ... - arguments used in the NLS message format
 *
 * @deprecated NLS API is deprecated.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
j9nls_printf(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, ...)
{
	va_list args;
#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_printf\n");
#endif

	va_start(args, message_num);
	portLibrary->nls_vprintf(portLibrary, flags, module_name, message_num, args);
	va_end(args);
}

/**
 * Print a formatted NLS message.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - to indicate what type of message (e.g., ERROR) and whether a newline is required
 * @param[in] module_name - the module identifier of the NLS message
 * @param[in] message_num - the NLS message number within the module
 * @param[in] args - arguments used in the NLS message format
 *
 * @deprecated NLS API is deprecated.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
j9nls_vprintf(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, va_list args)
{
	const char *message;
	va_list argsForSyslog;

	/* take a copy of the args for use by the syslog processing */
	COPY_VA_LIST(argsForSyslog, args);

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_vprintf\n");
#endif

	message = portLibrary->nls_lookup_message(portLibrary, flags, module_name, message_num, NULL);

#if defined(NLS_DEBUG)
	portLibrary->tty_printf(portLibrary, "NLS - vprintf - message: %s\n", message);
#endif

	/* If tracepoint enabled, mirror NLS message to tracepoint omrport.657, tracepoint group omrport{nlsmessage] */
	if (TrcEnabled_Trc_PRT_j9nls_vprintf) {
		char buffer[1024];
		va_list argsForTrace;

		/* Format the message text for the tracepoint insert and issue tracepoint. Uses simple buffer for message
		 * text, limit of 1024 characters. NLS messages are typically less than 80 characters.
		 */
		COPY_VA_LIST(argsForTrace, args);
		portLibrary->str_vprintf(portLibrary, buffer, sizeof(buffer), message, argsForTrace);
		Trc_PRT_j9nls_vprintf(buffer);
	}

	if (flags & J9NLS_STDOUT) {
		portLibrary->file_vprintf(portLibrary, OMRPORT_TTY_OUT, message, args);
	} else {
		portLibrary->file_vprintf(portLibrary, OMRPORT_TTY_ERR, message, args);
#if defined(J9OS_I5)
		/* allow iSeries to send messages written to stderr to the job log */
		Xj9SendMsgToJobLog(flags, (char *)message, args);
#endif
	}

	writeSyslog(portLibrary, flags, message, argsForSyslog);
}
/**
 * Return the NLS string for the module name and message number.  If no string is found,
 * or a failure occurs, return the default_string.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - to indicate what type of message (e.g., ERROR) and whether a newline is required
 * @param[in] module_name - the module identifier of the NLS message
 * @param[in] message_num - the NLS message number within the module
 * @param[in] default_string - a default message, in case no NLS message is found
 *
 * @return NLS String
 *
 * @deprecated NLS API is deprecated.
 */
const char *
j9nls_lookup_message(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, const char *default_string)
{
	const char *message;
	J9NLSDataCache *nls;
#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_lookup_message\n");
#endif
	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return J9NLS_ERROR_MESSAGE(J9NLS_PORT_NLS_FAILURE, "NLS Failure\n");
	}
	nls = &portLibrary->portGlobals->nls_data;

	omrthread_monitor_enter(nls->monitor);

	if (!nls->catalog) {
		open_catalog(portLibrary);
	}

	message = nlsh_lookup(portLibrary, module_name, message_num);
	if (!message) {
		message = parse_catalog(portLibrary, flags, module_name, message_num, default_string);
		if (!message) {
			message = J9NLS_ERROR_MESSAGE(J9NLS_PORT_NLS_FAILURE, "NLS Failure\n");
		}
	}

	omrthread_monitor_exit(nls->monitor);
	return message;
}

/**
 * Setup the path to the NLS catalog.
 *
 * @param[in] portLibrary The port library
 * @param[in] paths - an array of directory paths where the NLS catalog may be found
 * @param[in] nPaths - the number of entries in the @ref paths array
 * @param[in] baseName - the lead name of the catalog file name (i.e., the "java" in java_en_US.properties)
 * @param[in] extension - the extension of the catalog file name (i.e., the "properties in java_en_US.properties)
 *
 * @deprecated NLS API is deprecated.
 */
void
j9nls_set_catalog(struct OMRPortLibrary *portLibrary, const char **paths, const int nPaths, const char *baseName, const char *extension)
{
	int i;
	char *p;
	J9NLSDataCache *nls;

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - j9nls_set_catalog\n");
#endif

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return;
	}
	nls = &portLibrary->portGlobals->nls_data;

	omrthread_monitor_enter(nls->monitor);

	if (!baseName || !extension) {
		goto clean_exit;
	}
	for (i = 0; i < nPaths; i++) {
		if (nls->baseCatalogPaths[i]) {
			portLibrary->mem_free_memory(portLibrary, nls->baseCatalogPaths[i]);
		}
		nls->baseCatalogPaths[i] = NULL;
	}
	nls->nPaths = 0;
	if (nls->baseCatalogName) {
		portLibrary->mem_free_memory(portLibrary, nls->baseCatalogName);
		nls->baseCatalogName = NULL;
	}
	if (nls->baseCatalogExtension) {
		portLibrary->mem_free_memory(portLibrary, nls->baseCatalogExtension);
		nls->baseCatalogExtension = NULL;
	}

	for (i = 0; i < nPaths; i++) {
		nls->baseCatalogPaths[i] = portLibrary->mem_allocate_memory(portLibrary, strlen(paths[i]) + 1, OMR_GET_CALLSITE() , OMRMEM_CATEGORY_PORT_LIBRARY);
		if (nls->baseCatalogPaths[i]) {
			strcpy(nls->baseCatalogPaths[i], paths[i]);
			p = strrchr(nls->baseCatalogPaths[i], DIR_SEPARATOR);
			if (p) {
				p[1] = '\0';
			}
			nls->nPaths++;
		}
	}

	nls->baseCatalogName = portLibrary->mem_allocate_memory(portLibrary, strlen(baseName) + 1, OMR_GET_CALLSITE() , OMRMEM_CATEGORY_PORT_LIBRARY);
	if (nls->baseCatalogName) {
		strcpy(nls->baseCatalogName, baseName);
	}

	nls->baseCatalogExtension = portLibrary->mem_allocate_memory(portLibrary, strlen(extension) + 1, OMR_GET_CALLSITE() , OMRMEM_CATEGORY_PORT_LIBRARY);
	if (nls->baseCatalogExtension) {
		strcpy(nls->baseCatalogExtension, extension);
	}

	if (nls->language[0] == 0 && nls->region[0] == 0 && nls->variant[0] == 0) {
		nls_determine_locale(portLibrary);
	}

clean_exit:
	omrthread_monitor_exit(nls->monitor);

}

static char *
build_catalog_name(struct OMRPortLibrary *portLibrary, int32_t usePath, int32_t useDepth)
{
	uintptr_t len = 1;
	char *catalog = NULL;
	char *defaultCatalog = "." DIR_SEPARATOR_STR;
	J9NLSDataCache *nls;

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - build_catalog_name\n");
#endif

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return NULL;
	}
	nls = &portLibrary->portGlobals->nls_data;

	if (!nls->nPaths) {
		portLibrary->nls_set_catalog(portLibrary, (const char **)&defaultCatalog, 1, "java", "properties");
		if (!nls->baseCatalogName) {
			goto _done;
		}
		if (nls->language[0] == 0 && nls->region[0] == 0 && nls->variant[0] == 0) {
			nls_determine_locale(portLibrary);
		}
	}

	if (useDepth > 0) {
		if (nls->language[0] == 0) {
			goto _done;
		}
		if (useDepth > 1) {
			if (nls->region[0] == 0) {
				goto _done;
			}
			if (useDepth > 2) {
				if (nls->variant[0] == 0) {
					goto _done;
				}
			}
		}
	}

	if ((nls->baseCatalogName == NULL) || (nls->baseCatalogExtension == NULL)) {
		goto _done;
	}

	len += strlen(nls->baseCatalogPaths[usePath]);
	len += strlen(nls->baseCatalogName);
	len += strlen(nls->baseCatalogExtension);
	len += 1; /* the . before the extension */
	len += strlen(nls->language) + 1; /* '_en' */
	len += strlen(nls->region) + 1;
	len += strlen(nls->variant) + 1;
	len += 1; /* null terminator */

	catalog = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE() , OMRMEM_CATEGORY_PORT_LIBRARY);
	if (!catalog) {
		goto _done;
	}
	strcpy(catalog, nls->baseCatalogPaths[usePath]);
	strcat(catalog, nls->baseCatalogName);
	if (useDepth > 0) {
		strcat(catalog, "_");
		strcat(catalog, nls->language);
		if (useDepth > 1) {
			strcat(catalog, "_");
			strcat(catalog, nls->region);
			if (useDepth > 2) {
				strcat(catalog, "_");
				strcat(catalog, nls->variant);
			}
		}
	}
	strcat(catalog, ".");
	strcat(catalog, nls->baseCatalogExtension);

_done:
	return catalog;
}

static void
open_catalog(struct OMRPortLibrary *portLibrary)
{
	char *catalog = NULL;
	intptr_t fd = -1;
	int32_t d, p;
	J9NLSDataCache *nls;

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - open_catalog\n");
#endif

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return;
	}
	nls = &portLibrary->portGlobals->nls_data;

	/* try the following way to look for the catalog:
	 * The base name will typically be "<path>/java.properties". Append as many locale descriptors to the file name as possible to find a file.
	 * e.g.
	 *	java_en_US_Texas.properties
	 *	java_en_US.properties
	 *	java_en.properties
	 *	java.properties
	 */

	for (p = 0; p < (int32_t)nls->nPaths; p++) {
		for (d = 3; d >= 0; d--) {
			if (catalog) {
				portLibrary->mem_free_memory(portLibrary, catalog);
			}
			catalog = build_catalog_name(portLibrary, p, d);
			if (!catalog) {
				continue;
			}
#if defined(NLS_DEBUG)
			portLibrary->tty_printf(portLibrary, "NLS - attempting to open: %s\n", catalog);
#endif
			fd = portLibrary->file_open(portLibrary, catalog, EsOpenRead, 0);
			if (fd != -1) {
				break;
			}
		}
		if (fd != -1) {
			break;
		}
	}

	if (fd == -1) {
#if defined(NLS_DEBUG)
		portLibrary->tty_printf(portLibrary, "NLS - failed to open the nls catalog\n");
#endif
		if (catalog) {
			portLibrary->mem_free_memory(portLibrary, catalog);
			catalog = NULL;
		}
		return;
	}

	nls->catalog = catalog;

	portLibrary->file_close(portLibrary, fd);

#if defined(NLS_DEBUG)
	portLibrary->tty_printf(portLibrary, "NLS - succesfully opened %s\n", catalog);
#endif

	free_catalog(portLibrary);
}

static const char *
parse_catalog(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, const char *default_string)
{
#define MSG_NONE 0
#define MSG_SLASH 1
#define MSG_UNICODE 2
#define MSG_CONTINUE 3
#define MSG_DONE 4
#define MSG_IGNORE 5

	uint8_t dataBuf[BUF_SIZE];
	uint8_t *charPointer = NULL;
	uint8_t *endPointer = NULL;
	int mode = MSG_NONE, count = 0, digit;
	uint32_t unicode = 0;
	char nextChar;
	uint32_t offset = 0, bufSize = BUF_SIZE, maxOffset = 0;
	int32_t keyLength = -1;
	char *buf, *newBuf;
	BOOLEAN firstChar = TRUE;
	intptr_t fd = -1;
	J9NLSHashEntry *entry = NULL;
	char convertedModuleEnum[5];
	/* calculate a size which is larger than we could possibly need by putting together all of the prefixes and suffixes */
	char prefix[
		sizeof(J9NLS_ERROR_PREFIX "" J9NLS_INFO_PREFIX "" J9NLS_WARNING_PREFIX
			   "" J9NLS_COMMON_PREFIX "" J9NLS_EXEMPLAR
			   "" J9NLS_ERROR_SUFFIX "" J9NLS_INFO_SUFFIX "" J9NLS_WARNING_SUFFIX
			   " \n")
	];
	char *searchKey;
	BOOLEAN newline = !(flags & J9NLS_DO_NOT_APPEND_NEWLINE);
	const char *format;
	J9NLSDataCache *nls;

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - parse_catalog\n");
#endif

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return J9NLS_ERROR_MESSAGE(J9NLS_PORT_NLS_FAILURE, "NLS Failure\n");
	}
	nls = &portLibrary->portGlobals->nls_data;

	convertModuleName(module_name, (uint8_t *)convertedModuleEnum);

	format = "" J9NLS_COMMON_PREFIX "%s%03u %s";

	if (0 == (flags & J9NLS_DO_NOT_PRINT_MESSAGE_TAG)) {
		if (flags & J9NLS_ERROR) {
			format = "" J9NLS_ERROR_PREFIX "" J9NLS_COMMON_PREFIX "%s%03u" J9NLS_ERROR_SUFFIX " %s";
		} else if (flags & J9NLS_WARNING) {
			format = "" J9NLS_WARNING_PREFIX "" J9NLS_COMMON_PREFIX "%s%03u" J9NLS_WARNING_SUFFIX " %s";
		} else if (flags & J9NLS_INFO) {
			format = "" J9NLS_INFO_PREFIX "" J9NLS_COMMON_PREFIX "%s%03u" J9NLS_INFO_SUFFIX " %s";
		}
	}
	portLibrary->str_printf(portLibrary, prefix, sizeof(prefix), format, convertedModuleEnum, message_num, newline ? "\n" : "");

	/* make sure the searchKey string starts at J9VM001 instead of (E)J9VM001 */
	searchKey = prefix + (strchr(format, '%') - format);

#if defined(NLS_DEBUG)
	portLibrary->tty_printf(portLibrary, "NLS - parse_catalog - searchKey: %s\n", searchKey);
#endif

	/* we do a lazy caching, populate the cache as we look up messages */

	if (nls->catalog) {
		fd = portLibrary->file_open(portLibrary, (char *)nls->catalog, EsOpenRead, 0);
	}
	if (fd == -1) {
		/* couldn't open the file, store the searchKey instead */
		char *tmpStr = prefix;
		if (default_string) {
			tmpStr = (char *)default_string;
		}
		entry = nls_allocateHashEntry(portLibrary, module_name, message_num,  tmpStr, (uint32_t)strlen(tmpStr));
		if (!entry) {
			return default_string;
		}
		nlsh_insert(portLibrary, entry);
		return entry->message;
	}


	if (!(buf = portLibrary->mem_allocate_memory(portLibrary, bufSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY))) {
		goto finished;
	}

	while (read_from_catalog(portLibrary, fd, (char *)dataBuf, BUF_SIZE) != NULL) {
		charPointer = dataBuf;
		endPointer = charPointer + strlen((char *)dataBuf);

		while (charPointer < endPointer) {
			nextChar = *charPointer++;

			if (offset + 2 >= bufSize) {
				bufSize <<= 1;
				if (!(newBuf = portLibrary->mem_allocate_memory(portLibrary, bufSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY))) {
					goto finished;
				}
				memcpy(newBuf, buf, offset);
				portLibrary->mem_free_memory(portLibrary, buf);
				buf = newBuf;
			}

			if (mode == MSG_UNICODE) {
				if (nextChar >= '0' && nextChar <= '9') {
					digit = nextChar - '0';
				} else if (nextChar >= 'a' && nextChar <= 'f') {
					digit = (nextChar - 'a') + 10;
				} else if (nextChar >= 'A' && nextChar <= 'F') {
					digit = (nextChar - 'A') + 10;
				} else {
					digit = -1;
				}
				if (digit >= 0) {
					unicode = (unicode << 4) + digit;
					if (++count < 4) {
						continue;
					}
				}
				mode = MSG_NONE;
				if (unicode >= 0x01 && unicode <= 0x7f) {
					buf[offset++] = unicode;
				} else if (unicode == 0 || (unicode >= 0x80 && unicode <= 0x7ff)) {
					buf[offset++] = ((unicode >> 6) & 0x1f) | 0xc0;
					buf[offset++] = (unicode        & 0x3f) | 0x80;
				} else if (unicode >= 0x800 && unicode <= 0xffff) {
					buf[offset++] = ((unicode >> 12) & 0x0f) | 0xe0;
					buf[offset++] = ((unicode >> 6)  & 0x3f) | 0x80;
					buf[offset++] = (unicode         & 0x3f) | 0x80;
				}
				if (nextChar != '\n') {
					continue;
				}
			}

			if (mode == MSG_SLASH) {
				mode = MSG_NONE;
				switch (nextChar) {
				case '\r':
					mode = MSG_CONTINUE; /* Look for a following 'n */
					continue;
				case '\n':
					mode = MSG_IGNORE; /* Ignore whitespace on the next line */
					continue;
				case 'b':
					nextChar = '\b';
					break;
				case 'f':
					nextChar = '\f';
					break;
				case 'n':
					nextChar = '\n';
					break;
				case 'r':
					nextChar = '\r';
					break;
				case 't':
					nextChar = '\t';
					break;
				case 'u':
					mode = MSG_UNICODE;
					unicode = count = 0;
					continue;
				}
			} else {
				switch (nextChar) {
				case '#':
				case '!':
					if (firstChar) {

						while (1) {
							if (charPointer >= endPointer) {
								if (read_from_catalog(portLibrary, fd, (char *)dataBuf, BUF_SIZE) != NULL) {
									charPointer = dataBuf;
									endPointer = charPointer + strlen((char *)charPointer);
								}
							}
							if (charPointer >= endPointer) {
								break;
							}
							nextChar = *charPointer++;
							if (nextChar == '\r' || nextChar == '\n') {
								break;
							}
						}
						continue;
					}
					break;
				case '\n':
					if (mode == MSG_CONTINUE) { /* Part of a \r\n sequence */
						mode = MSG_IGNORE;
						continue;
					}
					/* fall into next case */
				case '\r':
					mode = MSG_NONE;
					firstChar = TRUE;
makeStrings:
					if (keyLength >= 0) {
#if defined(NLS_DEBUG)
						portLibrary->tty_printf(portLibrary, "NLS - parse_catalog - keyLength: %d -- buf: %20.20s\n", keyLength, buf);
#endif
						if (strncmp(searchKey, buf, sizeof(J9NLS_EXEMPLAR) - 1) == 0) {
#if defined(NLS_DEBUG)
							portLibrary->tty_printf(portLibrary, "NLS - parse_catalog - key match\n");
#endif
							/* we have the exact message */
							if (flags & J9NLS_DO_NOT_PRINT_MESSAGE_TAG) {
								entry = nls_allocateHashEntry(portLibrary, module_name, message_num,  buf + keyLength, offset - keyLength + 1);
								if (entry) {
									entry->message[offset - keyLength] = '\0';
									if (newline) {
										strcat(entry->message, "\n");
									}
								}
							} else {
								entry = nls_allocateHashEntry(portLibrary, module_name, message_num,  prefix, offset - keyLength + (uint32_t)strlen(prefix));
								if (entry) {
									/* null terminate and trim the \n if required */
									entry->message[strlen(prefix) - (newline ? 1 : 0)] = '\0';
									strncat(entry->message, buf + keyLength, offset - keyLength);
									if (newline) {
										strcat(entry->message, "\n");
									}
								}
							}
							goto finished;
						}
						keyLength = -1;
					}
					if (charPointer >= endPointer) {
						if (read_from_catalog(portLibrary, fd, (char *)dataBuf, BUF_SIZE) != NULL) {
							charPointer = dataBuf;
							endPointer = charPointer + strlen((char *)charPointer);
						}
					}
					if (charPointer >= endPointer) {
finished:
						if (buf) {
							portLibrary->mem_free_memory(portLibrary, buf);
						}
#if defined(NLS_DEBUG)
						portLibrary->tty_printf(portLibrary, "NLS - parse_catalog - inserting message\n");
#endif
						if (!entry) {
							char *tmpStr = prefix;
							if (default_string) {
								tmpStr = (char *)default_string;
							}
							entry = nls_allocateHashEntry(portLibrary, module_name, message_num,  tmpStr, (uint32_t)strlen(tmpStr));
							if (!entry) {
								portLibrary->file_close(portLibrary, fd);
								return default_string;
							}
						}
						nlsh_insert(portLibrary, entry);
						portLibrary->file_close(portLibrary, fd);
						return entry->message;
					}
					if (offset > maxOffset) {
						maxOffset = offset;
					}
					offset = 0;
					continue;
				case '\\':
					mode = MSG_SLASH;
					continue;
				case ':':
				case '=':
					if (keyLength == -1) { /* if parsing the key */
						keyLength = offset;
						continue;
					}
					break;
				}
				if ((nextChar >= 0x1c && nextChar <= ' ') || (nextChar >= 9 && nextChar <= 0xd)) {
					if (mode == MSG_CONTINUE) {
						mode = MSG_IGNORE;
					}
					/* if key length == 0 or value length == 0 */
					if (offset == 0 || offset == (uint32_t)keyLength || mode == MSG_IGNORE) {
						continue;
					}
					if (keyLength == -1) { /* if parsing the key */
						mode = MSG_DONE;
						continue;
					}
				}
				if (mode == MSG_IGNORE || mode == MSG_CONTINUE) {
					mode = MSG_NONE;
				}
			}
			firstChar = FALSE;
			if (mode == MSG_DONE) {
				keyLength = offset;
				mode = MSG_NONE;
			}
			buf[offset++] = nextChar;
		}
	}
	goto makeStrings;

#undef MSG_NONE
#undef MSG_SLASH
#undef MSG_UNICODE
#undef MSG_CONTINUE
#undef MSG_DONE
#undef MSG_IGNORE

}

static void
free_catalog(struct OMRPortLibrary *portLibrary)
{
	uint32_t i;
	J9NLSDataCache *nls;

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - free_catalog\n");
#endif

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return;
	}
	nls = &portLibrary->portGlobals->nls_data;

	for (i = 0; i < J9NLS_NUM_HASH_BUCKETS; i++) {
		J9NLSHashEntry *entry  = nls->hash_buckets[i];
		if (entry) {
			while (entry->next) {
				entry = entry->next;
			}
			entry->next = nls->old_hashEntries;
			nls->old_hashEntries = nls->hash_buckets[i];
			nls->hash_buckets[i] = NULL;
		}
	}
}

static uint32_t
nlsh_hash(uint32_t module_name, uint32_t message_num)
{
	return (module_name ^ message_num);
}

static const char *
nlsh_lookup(struct OMRPortLibrary *portLibrary, uint32_t module_name, uint32_t message_num)
{
	uint32_t hashKey = nlsh_hash(module_name, message_num);
	uint32_t index;
	J9NLSHashEntry *entry;
	J9NLSDataCache *nls;

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - nlsh_lookup\n");
#endif

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return J9NLS_ERROR_MESSAGE(J9NLS_PORT_NLS_FAILURE, "NLS Failure\n");
	}
	nls = &portLibrary->portGlobals->nls_data;

	index = hashKey % J9NLS_NUM_HASH_BUCKETS;
	entry = nls->hash_buckets[index];

	while (entry) {
		if (entry->module_name == module_name && entry->message_num == message_num) {
			return entry->message;
		}
		entry = entry->next;
	}

	return NULL;
}
static void
nlsh_insert(struct OMRPortLibrary *portLibrary, J9NLSHashEntry *entry)
{
	uint32_t hashKey = nlsh_hash(entry->module_name, entry->message_num);
	uint32_t index;
	J9NLSDataCache *nls;

#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - nlsh_insert\n");
#endif

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return;
	}
	nls = &portLibrary->portGlobals->nls_data;

	index = hashKey % J9NLS_NUM_HASH_BUCKETS;
	entry->next = nls->hash_buckets[index];
	nls->hash_buckets[index] = entry;
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the NLS library operations may be created here.  All resources created here should be destroyed
 * in @ref j9nls_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_NLS
 *
 * @deprecated NLS API is deprecated.
 */
int32_t
j9nls_startup(struct OMRPortLibrary *portLibrary)
{
	J9NLSDataCache *nls;

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return (int32_t) OMRPORT_ERROR_STARTUP_NLS;
	}
	nls = &portLibrary->portGlobals->nls_data;

	if (0 != omrthread_monitor_init_with_name(&nls->monitor, 0, "NLS hash table")) {
		return (int32_t) OMRPORT_ERROR_STARTUP_NLS;
	}

	nls_determine_locale(portLibrary);

	return (int32_t) 0;
}
/**
 * Free dynamically cached data (allocated and cached since @ref j9nls_startup was called).  This should be
 * called whenever the port library memory allocation routines are changed, to ensure that allocation and
 * deallocation routines are correctly paired ( @ref omrmem_allocate_memory and @ref omrmem_free_memory ).
 *
 * @param[in] portLibrary The port library
 *
 * @deprecated NLS API is deprecated.
 */
void
j9nls_free_cached_data(struct OMRPortLibrary *portLibrary)
{
	J9NLSHashEntry *entry;
	uint32_t i;
	J9NLSDataCache *nls;

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return;
	}
	nls = &portLibrary->portGlobals->nls_data;

	omrthread_monitor_enter(nls->monitor);

	for (i = 0; i < J9NLS_NUM_HASH_BUCKETS; i++) {
		entry = nls->hash_buckets[i];
		while (entry) {
			J9NLSHashEntry *next = entry->next;
			portLibrary->mem_free_memory(portLibrary, entry);
			entry = next;
		}
		nls->hash_buckets[i] = NULL;
	}

	/* catalog is never free'd without this flag */
	entry = nls->old_hashEntries;
	while (entry) {
		J9NLSHashEntry *next = entry->next;
		portLibrary->mem_free_memory(portLibrary, entry);
		entry = next;
	}
	nls->old_hashEntries = NULL;

	if (nls->catalog) {
		portLibrary->mem_free_memory(portLibrary, nls->catalog);
		nls->catalog = NULL;
	}

	omrthread_monitor_exit(nls->monitor);
}
/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by
 * @ref j9nls_startup should be destroyed here.  Any dynamically cached data should also be
 * freed here.  This function must be called only once.
 *
 * @param[in] portLibrary The port library
 *
 * @deprecated NLS API is deprecated.
 */
void
j9nls_shutdown(struct OMRPortLibrary *portLibrary)
{
	uint32_t i;
	J9NLSDataCache *nls;

	if (NULL == portLibrary->portGlobals) {
		/* CMVC 114135: failure to allocate portGlobals */
		return;
	}
	nls = &portLibrary->portGlobals->nls_data;

	portLibrary->nls_free_cached_data(portLibrary);

	/* Free the baseCatalogPaths allocated in j9nls_set_catalog */
	for (i = 0; i < nls->nPaths; i++) {
		if (nls->baseCatalogPaths[i]) {
			portLibrary->mem_free_memory(portLibrary, nls->baseCatalogPaths[i]);
			nls->baseCatalogPaths[i] = NULL;
		}
	}

	if (nls->baseCatalogExtension) {
		portLibrary->mem_free_memory(portLibrary, nls->baseCatalogExtension);
		nls->baseCatalogExtension = NULL;
	}

	/* catalog is never free'd without this flag */
	if (nls->baseCatalogName) {
		portLibrary->mem_free_memory(portLibrary, nls->baseCatalogName);
		nls->baseCatalogName = NULL;
	}

	omrthread_monitor_destroy(nls->monitor);
}
static J9NLSHashEntry *
nls_allocateHashEntry(struct OMRPortLibrary *portLibrary, uint32_t module_name, uint32_t message_num, const char *message, uint32_t sizeOfMessage)
{
	J9NLSHashEntry *entry = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9NLSHashEntry) + sizeOfMessage + 1 - sizeof(entry->message), OMR_GET_CALLSITE() , OMRMEM_CATEGORY_PORT_LIBRARY);
#if defined(NLS_DEBUG_TRACE)
	portLibrary->tty_printf(portLibrary, "NLS - nls_allocateHashEntry\n");
#endif

	if (!entry) {
		return NULL;
	}
	entry->module_name = module_name;
	entry->message_num = message_num;
	entry->next = NULL;
	memcpy(entry->message, message, sizeOfMessage);
	entry->message[sizeOfMessage] = '\0';
#if defined(NLS_DEBUG)
	portLibrary->tty_printf(portLibrary, "NLS - nls_allocateHashEntry - message: %s - sizeOfMessage: %d\n", entry->message, sizeOfMessage);
#endif
	return entry;
}
static void
convertModuleName(uint32_t module_name, uint8_t *module_str)
{
	module_str[0] = (uint8_t)((module_name >> 24) & 0xff);
	module_str[1] = (uint8_t)((module_name >> 16) & 0xff);
	module_str[2] = (uint8_t)((module_name >> 8) & 0xff);
	module_str[3] = (uint8_t)((module_name) & 0xff);
	module_str[4] = 0;
}

char *
read_from_catalog(struct OMRPortLibrary *portLibrary, intptr_t fd, char *buf, intptr_t bufsize)
{
	char temp[BUF_SIZE];
	intptr_t count, nbytes = bufsize;
	char *cursor = buf;
#ifdef J9ZOS390
	iconv_t converter;
	size_t inbytesleft, outbytesleft;
	char *inbuf, *outbuf;
#endif

	if (nbytes <= 0) {
		return 0;
	}

	/* discount 1 for the trailing NUL */
	nbytes -= 1;


#ifdef J9ZOS390
	/* iconv_get is not an a2e function, so we need to pass it honest-to-goodness EBCDIC strings */
#pragma convlit(suspend)
	converter = iconv_get(portLibrary, J9NLS_ICONV_DESCRIPTOR, "UTF-8", "IBM-1047");
#pragma convlit(resume)
	if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
		return NULL;
	}
#endif


	while (nbytes) {
		count = BUF_SIZE > nbytes ? nbytes : BUF_SIZE;
		count = portLibrary->file_read(portLibrary, fd, temp, count);

		if (count < 0) {
#ifdef J9ZOS390
			iconv_free(portLibrary, J9NLS_ICONV_DESCRIPTOR, converter);
#endif
			/* if we've made it through a successful read, return the buf. */
			if (nbytes + 1 != bufsize) {
				return buf;
			}
			return NULL;
		}

#ifdef J9ZOS390
		inbuf = temp;
		inbytesleft = count;
		outbuf = cursor;
		outbytesleft = nbytes;
		if ((size_t) - 1 == iconv(converter, &inbuf, &inbytesleft, &outbuf, &outbytesleft) || inbytesleft == count) {
			/* conversion failed */
			iconv_free(portLibrary, J9NLS_ICONV_DESCRIPTOR, converter);
			portLibrary->file_seek(portLibrary, fd, -1 * count, EsSeekCur);
			return NULL;
		}
		if (inbytesleft > 0) {
			portLibrary->file_seek(portLibrary, fd, inbytesleft - count, EsSeekCur);
		}
		nbytes -= count - inbytesleft;
		cursor += count - inbytesleft;
#else
		memcpy(cursor, temp, count);
		cursor += count;
		nbytes -= count;
#endif
	}

	*cursor = '\0';

#ifdef J9ZOS390
	iconv_free(portLibrary, J9NLS_ICONV_DESCRIPTOR, converter);
#endif

	return buf;
}
