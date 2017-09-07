/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
