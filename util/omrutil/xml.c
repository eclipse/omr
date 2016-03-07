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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#include <string.h>

#include "omrcfg.h"
#include "omrport.h"
#include "omrutil.h"

uintptr_t
escapeXMLString(OMRPortLibrary *portLibrary, char *outBuf, uintptr_t outBufLen, const char *string, uintptr_t stringLen)
{
	uintptr_t stringPos = 0;
	uintptr_t outBufPos = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (0 == outBufLen) {
		return 0;
	}
	/* null terminate in case we can't fit anything in the buffer */
	outBuf[0] = '\0';

	for (stringPos = 0; stringPos < stringLen; ++stringPos) {
		uintptr_t tmpBufLen = 0;
		char tmpBuf[8];

		switch (string[stringPos]) {
		case '<':
			strcpy(tmpBuf, "&lt;");
			break;
		case '>':
			strcpy(tmpBuf, "&gt;");
			break;
		case '&':
			strcpy(tmpBuf, "&amp;");
			break;
		case '\'':
			strcpy(tmpBuf, "&apos;");
			break;
		case '\"':
			strcpy(tmpBuf, "&quot;");
			break;
		default:
			if (((uint8_t)(string[stringPos])) < 0x20) {
				/* use XML escape sequence for characters below 0x20 */
				omrstr_printf(tmpBuf, sizeof(tmpBuf), "&#x%X;", (uint32_t)string[stringPos]);
			} else {
				tmpBuf[0] = string[stringPos];
				tmpBuf[1] = '\0';
			}
			break;
		}

		/* finish if the output buffer is full */
		tmpBufLen = strlen(tmpBuf);
		if (outBufPos + tmpBufLen > outBufLen - 1) {
			break;
		}

		strcpy(outBuf + outBufPos, tmpBuf);
		outBufPos += tmpBufLen;
	}
	return stringPos;
}
