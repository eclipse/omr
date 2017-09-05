/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
#include <string.h>
#include "omrport.h"
#include "omrgetjobid.h"

#define JOBID_STRING "%jobid"
#define JOBID_STRING_LENGTH sizeof(JOBID_STRING)

/* Generic version of omrgetjobid() */
uintptr_t
omrget_jobid(struct OMRPortLibrary *portLibrary, char *jobid, uintptr_t length)
{
	/* Check that caller provided enough space for the string */
	if ((NULL == jobid) || (length < JOBID_STRING_LENGTH)) {
		return JOBID_STRING_LENGTH;
	}

	/* Default behaviour for platforms other than zOS, simply return the job ID string token */
	strcpy(jobid, JOBID_STRING);

	return 0;
}
