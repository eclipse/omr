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

#include "omrcomp.h"
#include <string.h>

#define tolower(c) (((c) >= 'A' && (c) <= 'Z') ? ((c) - 'A' + 'a') : (c))
#define toupper(c) (((c) >= 'a' && (c) <= 'z') ? ((c) - 'a' + 'A') : (c))


int
j9_cmdla_tolower(int c)
{
	return tolower(c);
}

int
j9_ascii_tolower(int c)
{
	return tolower(c);
}

int
j9_cmdla_toupper(int c)
{
	return toupper(c);
}

int
j9_ascii_toupper(int c)
{
	return toupper(c);
}

int
j9_cmdla_stricmp(const char *s1, const char *s2)
{
	while (1) {
		char c1 = *s1++;
		char c2 = *s2++;
		int diff = tolower(c1) - tolower(c2);
		if (0 == diff) {
			if ('\0' == c1) {
				return 0;
			}
		} else {
			return diff;
		}
	}
	return 0;
}

int
j9_cmdla_strnicmp(const char *s1, const char *s2, size_t length)
{
	while (length-- > 0) {
		char c1 = *s1++;
		char c2 = *s2++;
		int diff = tolower(c1) - tolower(c2);
		if (0 == diff) {
			if ('\0' == c1) {
				return 0;
			}
		} else {
			return diff;
		}
	}
	return 0;
}

