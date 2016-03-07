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
 * @brief file
 */


#include <windows.h>
#include "omrport.h"
#include "omrutil.h"

char *
omrfile_read_text(struct OMRPortLibrary *portLibrary, intptr_t fd, char *buf, intptr_t nbytes)
{
	char temp[64];
	intptr_t count, i;
	char *cursor = buf;

	if (nbytes <= 0) {
		return 0;
	}

	/* discount 1 for the trailing NUL */
	nbytes -= 1;

	while (nbytes) {
		count = sizeof(temp) > nbytes ? nbytes : sizeof(temp);
		count = portLibrary->file_read(portLibrary, fd, temp, count);

		/* ignore translation for now */
		if (count < 0) {
			if (cursor == buf) {
				return NULL;
			} else {
				break;
			}
		}

		for (i = 0; i < count; i++) {
			char c = temp[i];

			if (c == '\r') { /* EOL */
				/* is this a bare CR, or part of a CRLF pair? */
				portLibrary->file_seek(portLibrary, fd, i - count + 1, EsSeekCur);
				count = portLibrary->file_read(portLibrary, fd, temp, 1);
				if (count && temp[0] == '\n') {
					/* matched CRLF pair */
					*cursor++ = '\n';
				} else {
					/* this was a bare CR -- back up */
					*cursor++ = '\r';
					portLibrary->file_seek(portLibrary, fd, -1, EsSeekCur);
				}
				*cursor = '\0';
				return buf;
			} else if (c == '\n') { /* this can onle be a bare LF */
				portLibrary->file_seek(portLibrary, fd, i - count + 1, EsSeekCur);
				*cursor++ = '\n';
				*cursor = '\0';
				return buf;
			} else {
				*cursor++ = c;
			}

		}
		nbytes -= count;
	}

	*cursor = '\0';
	return buf;
}


intptr_t
omrfile_write_text(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes)
{
	intptr_t result;
	intptr_t i;
	int newlines = 0, highchars = 0;
	char stackBuf[512];
	char *newBuf = stackBuf;
	intptr_t newLen;

	/* scan the buffer for any characters which need to be converted */
	for (i = 0; i < nbytes; i++) {
		if (buf[i] == '\n') {
			newlines += 1;
		} else if ((uint8_t)buf[i] & 0x80) {
			highchars += 1;
		}
	}

	/* if there are any non-ASCII chars, convert to Unicode and then to the local code page */
	if (highchars) {
		uint16_t wStackBuf[512];
		uint16_t *wBuf = wStackBuf;
		newLen = (nbytes + newlines) * 2;
		if (newLen > sizeof(wStackBuf)) {
			wBuf = portLibrary->mem_allocate_memory(portLibrary, newLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		}
		if (wBuf) {
			uint8_t *in = (uint8_t *)buf;
			uint8_t *end = in + nbytes;
			uint16_t *out = wBuf;

			while (in < end) {
				if (*in == '\n') {
					*out++ = (uint16_t)'\r';
					*out++ = (uint16_t)'\n';
					in += 1;
				} else {
					uint32_t numberU8Consumed = decodeUTF8CharN(in, out++, end - in);
					if (numberU8Consumed == 0) {
						break;
					}
					in += numberU8Consumed;
				}
			}
			/* in will be NULL if an error occurred */
			if (in) {
				UINT codePage = GetConsoleOutputCP();
				intptr_t wLen = out - wBuf;
				intptr_t mbLen = WideCharToMultiByte(codePage, 0, wBuf, (int)wLen, NULL, 0, NULL, NULL);
				if (mbLen > 0) {
					if (mbLen > sizeof(stackBuf)) {
						newBuf = portLibrary->mem_allocate_memory(portLibrary, mbLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
						/* if we couldn't allocate the buffer, just output the data the way it was */
					}
					if (newBuf) {
						WideCharToMultiByte(codePage, 0, wBuf, (int)wLen, newBuf, (int)mbLen, NULL, NULL);
						buf = newBuf;
						nbytes = mbLen;
					}
				}
			}
			if (wBuf != wStackBuf) {
				portLibrary->mem_free_memory(portLibrary, wBuf);
			}
		}
	} else if (newlines) {
		/* change any LFs to CRLFs */
		newLen = nbytes + newlines;
		if (newLen > sizeof(stackBuf)) {
			newBuf = portLibrary->mem_allocate_memory(portLibrary, newLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			/* if we couldn't allocate the buffer, just output the data the way it was */
		}
		if (newBuf) {
			char *cursor = newBuf;
			for (i = 0; i < nbytes; i++) {
				if (buf[i] == '\n') {
					*cursor++ = '\r';
				}
				*cursor++ = buf[i];
			}
			buf = newBuf;
			nbytes = newLen;
		}
	}

	result = portLibrary->file_write(portLibrary, fd, (void *)buf, nbytes);

	if (newBuf != stackBuf && newBuf != NULL) {
		portLibrary->mem_free_memory(portLibrary, newBuf);
	}

	return (result == nbytes) ? 0 : result;
}

int32_t
omrfile_get_text_encoding(struct OMRPortLibrary *portLibrary, char *charsetName, uintptr_t nbytes)
{
	CPINFOEX cpix;
	BOOL rc;
	char *c_ptr;

	if (charsetName == NULL) {
		return -1;
	}

	rc = GetCPInfoEx(GetConsoleOutputCP(), 0, &cpix);
	if (rc == FALSE) {
		return -2;
	}

	/* In case of very detailed text from OS truncate the string at first whitespace. */
	c_ptr = cpix.CodePageName;
	while (*c_ptr++ != '\0') {
		if (*c_ptr == ' ') {
			*c_ptr = '\0';
			break;
		}
	}

	if (nbytes <= strlen(cpix.CodePageName)) {
		return (int32_t)(strlen(cpix.CodePageName) + 1);
	}

	strcpy(charsetName, cpix.CodePageName);
	return 0;
}
