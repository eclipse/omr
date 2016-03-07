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

/**
 * @file
 * @ingroup Port
 * @brief file
 */

#include <nl_types.h>
#include <langinfo.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrutil.h"

/* __STDC_ISO_10646__ indicates that the platform wchar_t encoding is Unicode */
/* but older versions of libc fail to set the flag, even though they are Unicode */
#if defined(__STDC_ISO_10646__) || defined(LINUX) || defined(OSX)
#define J9VM_USE_WCTOMB
#else /* defined(__STDC_ISO_10646__) || defined(LINUX) || defined(OSX) */
#include "omriconvhelpers.h"
#endif /* defined(__STDC_ISO_10646__) || defined(LINUX) || defined(OSX) */

/* Some older platforms (Netwinder) don't declare CODESET */
#ifndef CODESET
#define CODESET _NL_CTYPE_CODESET_NAME
#endif

/* a2e overrides nl_langinfo to return ASCII strings. We need the native EBCDIC string */
#if defined(J9ZOS390)
#include "atoe.h"
#if defined (nl_langinfo)
#undef nl_langinfo
#endif
#endif

#if defined(J9VM_USE_ICONV)
static int growBuffer(struct OMRPortLibrary *portLibrary, char *stackBuf, char **bufStart, char **cursor, size_t *bytesLeft, uintptr_t *bufLen);
static intptr_t file_write_using_iconv(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes);
#endif

#if defined(J9VM_USE_WCTOMB)
static intptr_t walkUTF8String(const uint8_t *buf, intptr_t nbytes);
static void translateUTF8String(const uint8_t *in, uint8_t *out, intptr_t nbytes);
static intptr_t file_write_using_wctomb(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes);
#endif


intptr_t
omrfile_write_text(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes)
{
	intptr_t result, i;
	int requiresTranslation = 0;

#ifdef J9ZOS390
#pragma convlit(suspend)
#endif
	const char *utf8Encoding = "UTF-8";
#ifdef J9ZOS390
#pragma convlit(resume)
#endif

#ifdef J9ZOS390
	/* z/OS always needs to translate to EBCDIC */
	requiresTranslation = 1;
#else
	/* we can short circuit if the string is all ASCII */
	for (i = 0; i < nbytes; i++) {
		if ((uint8_t)buf[i] >= 0x80) {
			requiresTranslation = 1;
			break;
		}
	}
#endif

	if (!requiresTranslation || strcmp(nl_langinfo(CODESET), utf8Encoding) == 0) {
		/* We're in luck! No transformations are necessary */
		result = portLibrary->file_write(portLibrary, fd, (void *)buf, nbytes);
		return (result == nbytes) ? 0 : result;
	}

#if defined(J9VM_USE_WCTOMB)
	return file_write_using_wctomb(portLibrary, fd, buf, nbytes);
#else
	return file_write_using_iconv(portLibrary, fd, buf, nbytes);
#endif

}

#if defined(J9VM_USE_WCTOMB)

/**
 * @internal walk the given UTF8 string, looking for non-ASCII characters.
 * @return  0 if none were found, or, if non-ASCII strings were found,
 * answer the length of the buffer if it were converted to platform
 * encoding
 *
 * @note this relies on the assumption that wide chars are Unicode.
 * If not, the platform will need different support for this
 */
static intptr_t
walkUTF8String(const uint8_t *buf, intptr_t nbytes)
{
	const uint8_t *end = buf + nbytes;
	const uint8_t *cursor = buf;
	intptr_t newLength = 0;
	int hasHighChars = 0;

	/* reset the shift state */
	wctomb(NULL, 0);

	while (cursor < end) {
		if ((*cursor & 0x80) == 0x80) {
			char temp[MB_CUR_MAX];
			int wcresult;
			uint16_t unicode;
			uint32_t numberU8Consumed = decodeUTF8CharN(cursor, &unicode, end - cursor);

			if (numberU8Consumed == 0) {
				/* an illegal encoding was encountered! Don't try to decode the string */
				return 0;
			}
			cursor += numberU8Consumed;

			/* calculate the encoded length of this character */
			wcresult = wctomb(temp, (wchar_t)unicode);
			if (wcresult == -1) {
				/* an un-encodable char was encountered */
				newLength += 1;
			} else {
				newLength += wcresult;
			}
			hasHighChars = 1;

		} else {
			newLength += 1;
			cursor += 1;
		}
	}

	return hasHighChars ? newLength : 0;
}

#endif /* J9VM_USE_WCTOMB */


#if defined(J9VM_USE_WCTOMB)

/**
 * @internal assumes that the input has already been validated by walkUTF8String
 */
static void
translateUTF8String(const uint8_t *in, uint8_t *out, intptr_t nbytes)
{
	const uint8_t *end = in + nbytes;
	const uint8_t *cursor = in;

	/* walk the string again, translating it */
	while (cursor < end) {
		uint32_t numberU8Consumed;
		if ((*cursor & 0x80) == 0x80) {
			uint16_t unicode;
			int wcresult;
			numberU8Consumed = decodeUTF8Char(cursor, &unicode);
			cursor += numberU8Consumed;
			wcresult = wctomb((char *)out, (wchar_t)unicode);
			if (wcresult == -1) {
				*out++ = '?';
			} else {
				out += wcresult;
			}
		} else {
			*out++ = *cursor++;
		}
	}
}

#endif /* J9VM_USE_WCTOMB */


#if defined(J9VM_USE_WCTOMB)

static intptr_t
file_write_using_wctomb(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes)
{
	intptr_t result;
	intptr_t newLength = 0;
	char stackBuf[512];
	char *newBuf = stackBuf;

	newLength = walkUTF8String((uint8_t *)buf, nbytes);

	if (newLength) {
		if (newLength > sizeof(stackBuf)) {
			newBuf = portLibrary->mem_allocate_memory(portLibrary, newLength, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		}
		if (newBuf) {
			translateUTF8String((uint8_t *)buf, (uint8_t *)newBuf, nbytes);
			buf = newBuf;
			nbytes = newLength;
		}
	}

	result = portLibrary->file_write(portLibrary, fd, (void *)buf, nbytes);

	if (newBuf != stackBuf && newBuf != NULL) {
		portLibrary->mem_free_memory(portLibrary, newBuf);
	}

	return (result == nbytes) ? 0 : result;
}

#endif /* J9VM_USE_WCTOMB */


#if defined(J9VM_USE_ICONV)

/* @internal
 *
 * Creates a new buffer whose size is 512 bytes larger than the original one.
 * Copies over the specified contents from the original buffer to the new one, and frees the original buffer.
 *
 * The new buffer must be freed using omrmem_free_memory.
 *
 * @param[in] portLibrary The port library.
 * @param[in] stackBuf Pointer to the stack buffer.
 * @param[in,out] bufStart Pointer to the beginning of the original buffer.
 * @param[in,out] cursor Contents of the original buffer up to, but not including cursor will be copied to the new buffer.
 * 							cursor is updated to have the same offset in the new buffer.
 * @param[in,out] bytesLeft Remaining bytes of the original buffer. Updated to be the remaining bytes of the newly allocated buffer
 * @param[in,out] outBufLen Size of the original buffer.
 *
 * @return 0 on success, -1 on failure (out of memory).
 */
static int
growBuffer(struct OMRPortLibrary *portLibrary, char *stackBuf, char **bufStart, char **cursor, size_t *bytesLeft, uintptr_t *bufLen)
{
#define SIZE_OF_INCREMENT 512

	char *newBuf;

	*bufLen = *bufLen + SIZE_OF_INCREMENT;
	newBuf = portLibrary->mem_allocate_memory(portLibrary, *bufLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

	if (newBuf == NULL) {
		return -1;
	}

	/* copy over the work we've already done */
	memcpy(newBuf, *bufStart, *cursor - *bufStart);

	if (*bufStart != stackBuf) {
		portLibrary->mem_free_memory(portLibrary, *bufStart);
	}

	/* set up the new buffer */
	*bytesLeft = *bufLen - (*cursor - *bufStart);
	*cursor = newBuf + (*cursor - *bufStart);
	*bufStart = newBuf;

	return 0;

#undef SIZE_OF_INCREMENT
}

static intptr_t
file_write_using_iconv(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes)
{
	intptr_t result;
	char stackBuf[512];
	char *bufStart;
	uintptr_t outBufLen = sizeof(stackBuf);

	iconv_t converter = J9VM_INVALID_ICONV_DESCRIPTOR;
	size_t inbytesleft, outbytesleft;
	char *inbuf, *outbuf;
	intptr_t bytesToWrite = 0;

#ifdef J9ZOS390

	/* LIR 1280 (z/OS only) - every failed call to iconv_open() is recorded on the operator console, so don't retry */
	if (FALSE == PPG_file_text_iconv_open_failed) {

		/* iconv_get is not an a2e function, so we need to pass it honest-to-goodness EBCDIC strings */
#pragma convlit(suspend)
#endif

		converter = iconv_get(portLibrary, J9FILETEXT_ICONV_DESCRIPTOR, nl_langinfo(CODESET), "UTF-8");

#ifdef J9ZOS390
#pragma convlit(resume)
		if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
			PPG_file_text_iconv_open_failed = TRUE;
		}

	}
#endif


	if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
		/* no converter available for this code set. Just dump the UTF-8 chars */
		result = portLibrary->file_write(portLibrary, fd, (void *)buf, nbytes);
		return (result == nbytes) ? 0 : result;
	}

	inbuf = (char *)buf; /* for some reason this argument isn't const */
	outbuf = bufStart = stackBuf;
	inbytesleft = nbytes;
	outbytesleft = sizeof(stackBuf);

	while ((size_t)-1 == iconv(converter, &inbuf, &inbytesleft, &outbuf, &outbytesleft)) {
		int tmp_errno = errno;

		if (inbytesleft == 0) {
			break;
		}

		if ((outbytesleft == 0) || (tmp_errno == E2BIG)) {
			/* input conversion stopped due to lack of space in the output buffer */

			if (growBuffer(portLibrary, stackBuf, &bufStart, &outbuf, &outbytesleft, &outBufLen) < 0) {
				/* failed to grow buffer, just output what we've got so far */
				break;
			}

		} else if (tmp_errno == EILSEQ) {
			/* input conversion stopped due to an input byte that does not belong to the input code set */

			const char *unicodeFormat = "\\u%04x";
#define J9FILETEXT_ESCAPE_STR_SIZE 6 /* max size of unicode format */
			char escapedStr[J9FILETEXT_ESCAPE_STR_SIZE];
			char *escapedStrStart = escapedStr;

			uint16_t unicodeC;
			size_t utf8Length;
			size_t escapedLength;

			utf8Length = decodeUTF8CharN((const uint8_t *)inbuf, &unicodeC, inbytesleft);

			if (utf8Length == 0) {
				/* invalid encoding, including 4-byte UTF-8 */
				utf8Length = 1;
				escapedLength = 1;
				escapedStr[0] = '?';
			} else {
				escapedLength = portLibrary->str_printf(portLibrary, escapedStr, J9FILETEXT_ESCAPE_STR_SIZE, unicodeFormat, (uintptr_t)unicodeC);
			}

			inbytesleft -= utf8Length;
			inbuf += utf8Length;

			if ((size_t)-1 == iconv(converter, &escapedStrStart, &escapedLength, &outbuf, &outbytesleft)) {
				/* not handling EILSEQ here because:
				 *  1. we can't do much if iconv() fails to convert ascii.
				 *  2. inbuf and inbytesleft have been explicitly updated so the while loop will get terminated after converting the rest of the characters.
				 */

				tmp_errno = errno;

				/* if the remaining outbuf is too small, then grow it before storing Unicode string representation */
				if (tmp_errno == E2BIG) {
					if (growBuffer(portLibrary, stackBuf, &bufStart, &outbuf, &outbytesleft, &outBufLen) < 0) {
						/* failed to grow buffer, just output what we've got so far */
						break;
					}
				}
			}

		} else {
			/* input conversion stopped due to an incomplete character or shift sequence at the end of the input buffer */
			break;
		}
	}

	iconv_free(portLibrary, J9FILETEXT_ICONV_DESCRIPTOR, converter);

	/* CMVC 152575 - the converted string is not necessarily the same length in bytes as the original string */
	bytesToWrite = outbuf - bufStart;
	result = portLibrary->file_write(portLibrary, fd, (void *)bufStart, bytesToWrite);

	if (bufStart != stackBuf) {
		portLibrary->mem_free_memory(portLibrary, bufStart);
	}

	return (result == bytesToWrite) ? 0 : result;
}

#endif /* J9VM_USE_ICONV */

char *
omrfile_read_text(struct OMRPortLibrary *portLibrary, intptr_t fd, char *buf, intptr_t nbytes)
{
#if (defined(J9ZOS390))
	const char eol = a2e_tab['\n'];
	char *tempStr = NULL;
#else
	const static char eol = '\n';
#endif /* defined(J9ZOS390) */
	char temp[64];
	char *cursor = buf;
	BOOLEAN foundEOL = FALSE;

	if (nbytes <= 0) {
		return NULL;
	}

	/* discount 1 for the trailing NUL */
	nbytes -= 1;

	while ((!foundEOL) && (nbytes > 0)) {
		intptr_t i = 0;
		intptr_t count = sizeof(temp) > nbytes ? nbytes : sizeof(temp);
		count = portLibrary->file_read(portLibrary, fd, temp, count);

		/* ignore translation for now, except on z/OS */
		if (count < 0) {
			if (cursor == buf) {
				return NULL;
			} else {
				break;
			}
		}

		for (i = 0; i < count; i++) {
			char c = temp[i];
			*cursor = c;
			cursor += 1;

			if (eol == c) { /* EOL */
				/*function will return on EOL, move the file pointer to the EOL, prepare for next read*/
				portLibrary->file_seek(portLibrary, fd, i - count + 1, EsSeekCur);
				foundEOL = TRUE;
				break;
			}
		}
		nbytes -= count;
	}

	*cursor = '\0';
#if (defined(J9ZOS390))
	tempStr = e2a_string(buf);
	if (NULL == tempStr) {
		return NULL;
	}

	memcpy(buf, tempStr, strlen(buf));
	free(tempStr);
#endif /* defined(J9ZOS390) */
	return buf;
}

int32_t
omrfile_get_text_encoding(struct OMRPortLibrary *portLibrary, char *charsetName, uintptr_t nbytes)
{
	char *codepage, *c_ptr;

	if (charsetName == NULL) {
		return -1;
	}

#ifdef J9ZOS390
	codepage = etoa_nl_langinfo(CODESET);
#else
	codepage = nl_langinfo(CODESET);
#endif

	/* nl_langinfo returns "" on failure */
	if (codepage[0] == '\0') {
#ifdef J9ZOS390
		free(codepage);
#endif
		return -2;
	}

	/* In case of very detailed text from OS truncate the string at first whitespace. */
	c_ptr = codepage;
	while (*c_ptr++ != '\0') {
		if (*c_ptr == ' ') {
			*c_ptr = '\0';
			break;
		}
	}

	if (nbytes <= strlen(codepage)) {
#ifdef J9ZOS390
		free(codepage);
#endif
		return (int32_t)(strlen(codepage) + 1);
	}

	strcpy(charsetName, codepage);
#ifdef J9ZOS390
	free(codepage);
#endif
	return 0;
}
