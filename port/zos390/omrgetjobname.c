/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief shared library
 */
#include <stdlib.h>
#include <string.h>
#include "omrport.h"
#include "omrgetjobname.h"
#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
#include "atoe.h"
#endif

#define J9_MAX_JOBNAME 16

/**
 * Get the job name for a z/OS job.
 *
 * @param[in] portLibrary The port library.
 * @param[in/out] jobname Pointer to string to populate with the
 *       jobname.
 * @param[in] length The length of the data area addressed by
 *       jobname.
 */
void
omrget_jobname(struct OMRPortLibrary *portLibrary, char *jobname, uintptr_t length)
{
	char *tmp_jobname = (char *)__malloc31(J9_MAX_JOBNAME);
	extern void _JOBNAME(char *); /* defined in omrjobname.s */

	if (NULL != tmp_jobname) {
		char *ascname = NULL;
		memset(tmp_jobname, '\0', J9_MAX_JOBNAME);
		_JOBNAME(tmp_jobname);  /* requires <31bit address */
#if !defined(OMR_EBCDIC)
		ascname = e2a_func(tmp_jobname, strlen(tmp_jobname));
#else /* !defined(OMR_EBCDIC) */
		ascname = tmp_jobname;
#endif /* !defined(OMR_EBCDIC) */

		if (NULL != ascname) {
			uintptr_t width = strcspn(ascname, " ");
			strncpy(jobname, ascname, width);
			jobname[width] = '\0';
#if !defined(OMR_EBCDIC)
			free(ascname);
#endif /* !defined(OMR_EBCDIC) */
		}
		free(tmp_jobname);
	} else {
		if (length >= 5) {
			strcpy(jobname, "%job");
		}
	}
}
