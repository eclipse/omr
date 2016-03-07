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
#include "omrgetjobname.h"

/**
 * Generic version of omrgetjobname, to the job name for a job.
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
	if (jobname != NULL) {
		if (length >= 5) {
			strcpy(jobname, "%job");
		}
	}
}
