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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief shared library
 */
#include <stdlib.h>
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
 *
 */
uintptr_t
omrget_userid(char *userid, uintptr_t length)
{
	/* _USERID() requires that memory be <31bit address, allocating here for use in
	 * _USERID() call.  */
	char *tmp_userid = (char *)__malloc31(J9_MAX_USERID);
	char *ascname;
	uintptr_t width;
	uintptr_t result = 0;

	userid[0] = '\0';

	if (tmp_userid) {
		memset(tmp_userid, '\0', J9_MAX_USERID);
		_USERID(tmp_userid);  /* requires <31bit address */
		ascname = e2a_func(tmp_userid, strlen(tmp_userid));

		if (ascname) {
			width = strcspn(ascname, " ");
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
			free(ascname);
		}
		free(tmp_userid);
	}

	return result;
}


