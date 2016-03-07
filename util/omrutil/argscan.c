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
#include "omrutil.h"
#include "omrport.h"

/* Answer non-zero on success
 */
uintptr_t
try_scan(char **scan_start, const char *search_string)
{
	char *scan_string = *scan_start;
	size_t search_length = strlen(search_string);

	if (strlen(scan_string) < search_length) {
		return 0;
	}

	if (0 == j9_cmdla_strnicmp(scan_string, search_string, search_length)) {
		*scan_start = &scan_string[search_length];
		return 1;
	}

	return 0;
}
