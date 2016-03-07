/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015
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
