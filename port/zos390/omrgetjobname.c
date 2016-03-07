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
#include "omrport.h"
#include "omrgetjobname.h"
#include "atoe.h"

#define J9_MAX_JOBNAME 16

/**
 * Get the job name for a z/OS job.
 *
 * @param[in] portLibrary The port library.
 * @param[in/out] jobname Pointer to string to populate with the
 *       jobname.
 * @param[in] length The length of the data area addressed by
 *       jobname.
 *
 */
void
omrget_jobname(struct OMRPortLibrary *portLibrary, char *jobname, uintptr_t length)
{
	char *tmp_jobname = (char *)__malloc31(J9_MAX_JOBNAME);
	char *ascname;
	uintptr_t width;

	if (tmp_jobname) {
		memset(tmp_jobname, '\0', J9_MAX_JOBNAME);
		_JOBNAME(tmp_jobname);  /* requires <31bit address */
		ascname = e2a_func(tmp_jobname, strlen(tmp_jobname));

		if (ascname) {
			width = strcspn(ascname, " ");
			strncpy(jobname, ascname, width);
			jobname[width] = '\0';
			free(ascname);
		}
		free(tmp_jobname);

	} else {
		if (length >= 5) {
			strcpy(jobname, "%job");
		}
	}
}


