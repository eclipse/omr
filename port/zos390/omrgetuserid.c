/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief shared library
 */
#include <stdlib.h>
#include <string.h>
#include "atoe.h"
#include "omrgetuserid.h"

#define J9_MAX_USERID 16

/**
 * Get the userid for a z/OS job.
 *
 * @param[in/out] userid Pointer to string to populate with the
 *       userid.
 * @param[in] length The length of the data area addressed by
 *       userid. If length is too small to contain the user id,
 *       the user id string will be return as an empty string.
 *
 * @return 0 on success, size of required buffer on failure.
 */
uintptr_t
omrget_userid(char *userid, uintptr_t length)
{
	/* _USERID() requires that memory be <31bit address, allocating here for use in
	 * _USERID() call.  */
	char *tmp_userid = (char *)__malloc31(J9_MAX_USERID);
	uintptr_t result = 0;

	userid[0] = '\0';

	if (NULL != tmp_userid) {
		char *ascname = NULL;
		memset(tmp_userid, '\0', J9_MAX_USERID);
		_USERID(tmp_userid);  /* requires <31bit address */
#if !defined(OMR_EBCDIC)
		ascname = e2a_func(tmp_userid, strlen(tmp_userid));
#else /* !defined(OMR_EBCDIC) */
		ascname = tmp_userid;
#endif /* !defined(OMR_EBCDIC) */

		if (NULL != ascname) {
			uintptr_t width = strcspn(ascname, " ");
			if (width < length) {
				strncpy(userid, ascname, width);
				userid[width] = '\0';
			} else {
				/*
				 * Buffer was too small to hold user id, let the caller know
				 * how much space is needed.
				 */
				result = width;
			}
#if !defined(OMR_EBCDIC)
			free(ascname);
#endif /* !defined(OMR_EBCDIC) */
		}
		free(tmp_userid);
	}

	return result;
}
