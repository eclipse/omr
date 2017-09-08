/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
