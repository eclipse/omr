/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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


