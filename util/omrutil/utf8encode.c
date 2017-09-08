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

#include "omrutil.h"

/**
 * Encode the Unicode character.
 *
 * Encodes the input Unicode character and stores it into result.
 *
 * @param[in] unicode The unicode character
 * @param[in,out] result buffer for UTF8 character
 * @param[in] bytesRemaining available space in result buffer
 *
 * @return Size of encoding (1,2,3) on success, 0 on failure
 *
 * @note Don't write more than bytesRemaining characters
 * @note If result is NULL then bytesRemaining is ignored and the number
 * of characters required to encode the unicode character is returned.
 */
uint32_t
encodeUTF8CharN(uintptr_t unicode, uint8_t *result, uint32_t bytesRemaining)
{
	if (unicode >= 0x01 && unicode <= 0x7f) {
		if (result) {
			if (bytesRemaining < 1) {
				return 0;
			}
			*result = (uint8_t)unicode;
		}
		return 1;
	} else if (unicode == 0 || (unicode >= 0x80 && unicode <= 0x7ff)) {
		if (result) {
			if (bytesRemaining < 2) {
				return 0;
			}
			*result++ = (uint8_t)(((unicode >> 6) & 0x1f) | 0xc0);
			*result = (uint8_t)((unicode & 0x3f) | 0x80);
		}
		return 2;
	} else if (unicode >= 0x800 && unicode <= 0xffff) {
		if (result) {
			if (bytesRemaining < 3) {
				return 0;
			}
			*result++ = (uint8_t)(((unicode >> 12) & 0x0f) | 0xe0);
			*result++ = (uint8_t)(((unicode >> 6) & 0x3f) | 0x80);
			*result = (uint8_t)((unicode & 0x3f) | 0x80);
		}
		return 3;
	} else {
		return 0;
	}
}


/**
 * Encode the Unicode character.
 *
 * Encodes the input Unicode character and stores it into result.
 *
 * @param[in] unicode The unicode character
 * @param[in,out] result buffer for UTF8 character
 *
 * @return Size of encoding (1,2,3) on success, 0 on failure
 *
 * @note If result is NULL then the number of characters required to
 * encode the character is returned.
 */
uint32_t
encodeUTF8Char(uintptr_t unicode, uint8_t *result)
{
	return encodeUTF8CharN(unicode, result, 3);
}




