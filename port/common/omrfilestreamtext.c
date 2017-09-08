/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file omrfilestreamtext.c
 * @ingroup Port
 * @brief Buffered file text input and output through streams.
 *
 * This file implements stream based file IO with the ability to convert to different file
 * encodings.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include "omriconvhelpers.h"
#include "omrport.h"
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"

#if defined(J9ZOS390)
/* Needed for a translation table from ascii -> ebcdic */
#include "atoe.h"

/* a2e overrides many functions to use ASCII strings.
 * We need the native EBCDIC string
 */
#if defined (nl_langinfo)
#undef nl_langinfo
#endif

#endif /* defined(J9ZOS390) */


/**
 * CRLFNEWLINES will be defined when we require changing newlines from '\n' to '\r\n'
 */
#undef CRLFNEWLINES
#if defined(WIN32) || defined(WIN64)
#define CRLFNEWLINES
#endif /* defined(WIN32) || defined(WIN64) */

static intptr_t
write_all(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *buf, intptr_t nbytes);

static intptr_t
transliterate_write_text(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *buf, intptr_t nbytes, int32_t toEncoding);

/**
 * Write formatted text to a file.
 *
 * Writes formatted output to the filestream. All text will be transliterated from modifed UTF-8
 * to the platform encoding.
 *
 * @param[in] portLibrary The port library
 * @param[in] fileStream Filestream to write to
 * @param[in] format The format string.
 * @param[in] args Variable argument list.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled
 * "PortLibrary printf" in the "Inside J9" Lotus Notes database.
 */
void
omrfilestream_vprintf(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *format, va_list args)
{
	char localBuffer[512];
	char *writeBuffer = NULL;
	uintptr_t bufferSize = 0;
	uintptr_t stringSize = 0;

	Trc_PRT_filestream_vprintf_Entry(fileStream, format);

	if ((NULL == fileStream) || (NULL == format)) {
		Trc_PRT_filestream_vprintf_invalidArgs(fileStream, format);
		Trc_PRT_filestream_vprintf_Exit();
		return;
	}

	/* str_vprintf(..,NULL,..) result is size of buffer required including the null terminator */
	bufferSize = portLibrary->str_vprintf(portLibrary, NULL, 0, format, args);

	/* use local buffer if possible, allocate a buffer from system memory if local buffer not large enough */
	if (sizeof(localBuffer) >= bufferSize) {
		writeBuffer = localBuffer;
	} else {
		Trc_PRT_filestream_vprintf_stackBufferNotBigEnough(fileStream, format, bufferSize);
		writeBuffer = portLibrary->mem_allocate_memory(portLibrary, bufferSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	}

	/* format and write out the buffer (truncate into local buffer as last resort) */
	if (NULL != writeBuffer) {
		stringSize = portLibrary->str_vprintf(portLibrary, writeBuffer, bufferSize, format, args);
		portLibrary->filestream_write_text(portLibrary, fileStream, writeBuffer, stringSize, J9STR_CODE_PLATFORM_RAW);
		/* dispose of buffer if not on local */
		if (writeBuffer != localBuffer) {
			portLibrary->mem_free_memory(portLibrary, writeBuffer);
		}
	} else {
		Trc_PRT_filestream_vprintf_failedMalloc(fileStream, format, bufferSize);
		portLibrary->nls_printf(portLibrary, J9NLS_ERROR, J9NLS_PORT_FILE_MEMORY_ALLOCATE_FAILURE);
		stringSize = portLibrary->str_vprintf(portLibrary, localBuffer, sizeof(localBuffer), format, args);
		portLibrary->filestream_write_text(portLibrary, fileStream, localBuffer, stringSize, J9STR_CODE_PLATFORM_RAW);
	}

	Trc_PRT_filestream_vprintf_Exit();
}

/**
 * Write formatted text to a filestream.
 *
 * Writes formatted output  to the filestream. All text will be transliterated from modifed UTF-8
 * to the platform encoding.
 *
 * @param[in] portLibrary The port library
 * @param[in] fileStream Filestream to write to
 * @param[in] format The format string to be output.
 * @param[in] ... arguments for format.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled
 * "PortLibrary printf" in the "Inside J9" Lotus Notes database.
 */
void
omrfilestream_printf(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *format, ...)
{
	va_list args;

	Trc_PRT_filestream_printf_Entry(fileStream, format);

	va_start(args, format);
	portLibrary->filestream_vprintf(portLibrary, fileStream, format, args);
	va_end(args);

	Trc_PRT_filestream_printf_Exit();
}

/**
 * Write text to a filestream. This function will take a MUTF-8 encoded string, and convert it into
 * the specified encoding. This makes it suitable for printing human readable text to the console
 * or a file.
 *
 * Writes up to nbytes from the provided buffer to the file referenced by the filestream.
 *
 * @param[in] portLibrary The port library
 * @param[in] fileStream Filestream to write.
 * @param[in] buf Buffer to be written.
 * @param[in] nbytes Size of buffer. Must fit in a size_t. Must be positive.
 * @param[in] toEncoding The desired output encoding.The desired output encoding. May be any of the
 * output encodings accepted by omrstr_convert().
 *
 * @return Number of bytes written from buf, negative portable error code on failure. This may not
 * be the number of bytes actually written to the file, which is locale specific.
 *
 * @note If an error occurred, the file position is unknown.
 */
intptr_t
omrfilestream_write_text(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *buf, intptr_t nbytes, int32_t toEncoding)
{
	intptr_t rc = 0;
#if defined(CRLFNEWLINES)
	uintptr_t i = 0;
	uintptr_t j = 0;
	uintptr_t newlineCount = 0;
	char stackNewlineBuffer[512];
	char *newlineBuffer = stackNewlineBuffer;
#endif /* defined(CRLFNEWLINES) */

	/* The buffer must be MUTF8 */
	int32_t fromEncoding = J9STR_CODE_MUTF8;

	Trc_PRT_filestream_write_text_Entry(fileStream, buf, nbytes, toEncoding);

	if ((nbytes < 0) || (NULL == buf) || (NULL == fileStream)) {
		Trc_PRT_filestream_write_text_invalidArgs(fileStream, buf, nbytes, toEncoding);
		Trc_PRT_filestream_write_text_Exit(OMRPORT_ERROR_FILE_INVAL);
		return OMRPORT_ERROR_FILE_INVAL;
	} else if (0 == nbytes) {
		Trc_PRT_filestream_write_text_Exit(0);
		return 0;
	}

#if defined(CRLFNEWLINES)
	/* We have to change the line endings.  This must be done entirely before transliteration. */

	/* Count the newlines */
	for (i = 0; i < ((uintptr_t) nbytes); i++) {
		if (buf[i] == '\n') {
			newlineCount += 1;
		}
	}

	if (0 < newlineCount) {
		/* Every newline now requires 2 bytes */
		uintptr_t bufferSize = ((uintptr_t) nbytes) + newlineCount;

		/* malloc a bigger buffer if it is needed */
		if (bufferSize > sizeof(stackNewlineBuffer)) {
			newlineBuffer = portLibrary->mem_allocate_memory(portLibrary, bufferSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == newlineBuffer) {
				Trc_PRT_filestream_write_text_failedMalloc(fileStream, buf, nbytes, rc);
				Trc_PRT_filestream_write_text_Exit(OMRPORT_ERROR_FILE_OPFAILED);
				return OMRPORT_ERROR_FILE_OPFAILED;
			}
		}

		/* replace the newlines */
		for (i = 0, j = 0; i < bufferSize; i++, j++) {
			if (buf[j] == '\n') {
				newlineBuffer[i] = '\r';
				i += 1;
			}
			newlineBuffer[i] = buf[j];
		}
		
		buf = newlineBuffer;
		nbytes = bufferSize;
	}

#endif /* defined(CRLFNEWLINES) */

	if (fromEncoding == toEncoding) {
		/* omrstr_convert() does not support MUTF-8 -> MUTF-8 conversion, so do not use it.
		 * This has the side effect of not validating the string */
		rc = write_all(portLibrary, fileStream, buf, nbytes);
	} else {
		rc = transliterate_write_text(portLibrary, fileStream, buf, nbytes, toEncoding);
	}

#if defined(CRLFNEWLINES)
	/* free the buffer only if we had to malloc one, otherwise it is on the stack */
	if (newlineBuffer != stackNewlineBuffer) {
		portLibrary->mem_free_memory(portLibrary, newlineBuffer);
	}
#endif /* defined(CRLFNEWLINES) */

	/* if rc is not an error code, then we assume we have written everything from the buffer.  At
	 * this point, we want to return the number of characters consumed from the input buffer, not the
	 * bytes output to the file.
	 */
	if (rc > 0) {
		rc = nbytes;
	}

	Trc_PRT_filestream_write_text_Exit(rc);
	return rc;
}

/**
 * @internal
 * Write text to a file by transliterating it.
 * No parameter error checking.
 */
static intptr_t
transliterate_write_text(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *buf, intptr_t nbytes, int32_t toEncoding)
{
	char stackExpandedBuffer[512];
	char *expandedBuffer = stackExpandedBuffer;
	uintptr_t convertedSize = 0;
	intptr_t rc = -1;

	/* The buffer must be MUTF8 */
	int32_t fromEncoding = J9STR_CODE_MUTF8;

	/* Calculate the size of the buffer needed for the converted string.
	 * If our buffer is not big enough, allocate a new one.
	 */
	rc = portLibrary->str_convert(portLibrary, fromEncoding, toEncoding, buf, nbytes, NULL, 0);
	if (rc < 0) {
		Trc_PRT_filestream_write_text_failedStringTransliteration(fileStream, buf, nbytes, toEncoding, (int32_t) rc);
		return OMRPORT_ERROR_FILE_OPFAILED;
	} else if ((size_t) rc > sizeof(stackExpandedBuffer)) {
		expandedBuffer = portLibrary->mem_allocate_memory(portLibrary, rc, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == expandedBuffer) {
			Trc_PRT_filestream_write_text_failedMalloc(fileStream, buf, nbytes, rc);
			return OMRPORT_ERROR_FILE_OPFAILED;
		}
	}
	convertedSize = rc;

	/* Fill the buffer with the new string */
	rc = portLibrary->str_convert(portLibrary, fromEncoding, toEncoding, buf, nbytes, expandedBuffer, convertedSize);
	if (rc < 0) {
		Trc_PRT_filestream_write_text_failedStringTransliteration(fileStream, buf, nbytes, toEncoding, (int32_t) rc);
		rc = OMRPORT_ERROR_FILE_OPFAILED;
	} else {
		/* Write the converted bytes to the file, we will return whatever error/bytecount */
		convertedSize = rc;
		rc = write_all(portLibrary, fileStream, expandedBuffer, convertedSize);
		if (rc < 0) {
			Trc_PRT_filestream_write_text_failedToWrite(fileStream, expandedBuffer, convertedSize, rc);
		}
	}

	/* free the buffer only if we had to malloc one, otherwise it is on the stack */
	if (stackExpandedBuffer != expandedBuffer) {
		portLibrary->mem_free_memory(portLibrary, expandedBuffer);
	}

	return rc;
}

/**
 * @internal
 * Write all text to a filestream.  This will write in a loop to ensure that all text is written.
 */
static intptr_t
write_all(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *buf, intptr_t nbytes)
{
	const char *from = buf;
	intptr_t remaining = nbytes;
	intptr_t rc = 0;
	intptr_t result = nbytes;

	while (remaining > 0) {
		rc = portLibrary->filestream_write(portLibrary, fileStream, from, remaining);
		if (rc < 0) {
			result = rc;
			break;
		}
		remaining -= rc;
		from += rc;
	}

	return result;
}
