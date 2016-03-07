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

#include "omrutil.h"

#include "../omrutil/ut_j9utilcore.h"

/**
 * Decode the UTF8 character, assuming a valid encoding.
 *
 * Decode the input UTF8 character and stores it into result.
 *
 * @param[in] input The UTF8 character
 * @param[out] result buffer for unicode characters
 *
 * @return The number of UTF8 characters consumed (1,2,3)
 */
uint32_t
decodeUTF8Char(const uint8_t *input, uint16_t *result)
{
	uint8_t c;
	uint16_t unicodeC;

	c = input[0];
	if ((c & 0x80) == 0x00) {
		/* one byte encoding */

		*result = (uint16_t)c;
		return 1;

	} else if ((c & 0xE0) == 0xC0) {
		/* two byte encoding */

		unicodeC = ((uint16_t)c & 0x1F) << 6;
		c = input[1];
		unicodeC += (uint16_t)c & 0x3F;

		*result = unicodeC;
		return 2;
	}

	/* three byte encoding */

	unicodeC = ((uint16_t)c & 0x0F) << 12;
	c = input[1];
	unicodeC += ((uint16_t)c & 0x3F) << 6;
	c = input[2];
	unicodeC += (uint16_t)c & 0x3F;

	*result = unicodeC;
	return 3;
}


/**
 * Decode the UTF8 character.
 *
 * Decode the input UTF8 character and stores it into result.
 *
 * @param[in] input The UTF8 character
 * @param[out] result buffer for unicode characters
 * @param[in] bytesRemaining number of bytes remaining in input
 *
 * @return The number of UTF8 characters consumed (1,2,3) on success, 0 on failure
 * @note Don't read more than bytesRemaining characters.
 * @note  If morecharacters are required to fully decode the character, return failure
 */
uint32_t
decodeUTF8CharN(const uint8_t *input, uint16_t *result, uintptr_t bytesRemaining)
{
	uint8_t c;
	const uint8_t *cursor = input;

	if (bytesRemaining < 1) {
		return 0;
	}

	c = *cursor++;
	if (c == 0x00) {
		/* illegal NUL encoding */

		return 0;

	} else if ((c & 0x80) == 0x00) {
		/* one byte encoding */

		*result = (uint16_t)c;
		return 1;

	} else if ((c & 0xE0) == 0xC0) {
		/* two byte encoding */
		uint16_t unicodeC;

		if (bytesRemaining < 2) {
			Trc_Utilcore_decodeUTF8CharN_Truncated();
			return 0;
		}
		unicodeC = ((uint16_t)c & 0x1F) << 6;

		c = *cursor;
		unicodeC += (uint16_t)c & 0x3F;
		if ((c & 0xC0) != 0x80) {
			Trc_Utilcore_decodeUTF8CharN_Invalid2ByteEncoding(c);
			return 0;
		}

		*result = unicodeC;
		return 2;

	} else if ((c & 0xF0) == 0xE0) {
		/* three byte encoding */
		uint16_t unicodeC;

		if (bytesRemaining < 3) {
			Trc_Utilcore_decodeUTF8CharN_Truncated();
			return 0;
		}
		unicodeC = ((uint16_t)c & 0x0F) << 12;

		c = *cursor++;
		unicodeC += ((uint16_t)c & 0x3F) << 6;
		if ((c & 0xC0) != 0x80) {
			Trc_Utilcore_decodeUTF8CharN_Invalid3ByteEncoding(c);
			return 0;
		}

		c = *cursor;
		unicodeC += (uint16_t)c & 0x3F;
		if ((c & 0xC0) != 0x80) {
			Trc_Utilcore_decodeUTF8CharN_Invalid3ByteEncoding(c);
			return 0;
		}

		*result = unicodeC;
		return 3;
	} else {
		/* illegal encoding (i.e. would decode to a char > 0xFFFF) */
		Trc_Utilcore_decodeUTF8CharN_EncodingTooLarge(c);
		return 0;
	}
}
