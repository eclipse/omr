/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
