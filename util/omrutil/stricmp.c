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

