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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "StringUtils.hpp"

RCType
StringUtils::getPositiveIntValue(const char *line, const char *key, unsigned int *result)
{
	RCType rc = RC_FAILED;
	const char *vpos = StringUtils::containsUpperLower(line, key);
	if (NULL != vpos) {
		*result = atoi(vpos + strlen(key));
		rc = RC_OK;
	}
	return rc;
}

bool
StringUtils::startsWithUpperLower(const char *text, const char *prefix)
{
	return (0 == Port::strncasecmp(text, prefix, strlen(prefix)));
}

const char *
StringUtils::containsUpperLower(const char *text, const char *toFind)
{
	while ('\0' != *text) {
		if (tolower(*text) == tolower(*toFind)) {
			/* Start of search string (potentially) */
			if (startsWithUpperLower(text, toFind)) {
				return text;
			}
		}
		text += 1;
	}

	return NULL;
}
