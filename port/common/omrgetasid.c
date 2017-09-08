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
#include "omrgetjobid.h"

#define ASID_STRING "%asid"
#define ASID_STRING_LENGTH sizeof(ASID_STRING)

/* Generic version of omrget_asid() */
uintptr_t
omrget_asid(struct OMRPortLibrary *portLibrary, char *asid, uintptr_t length)
{
	/* Check that caller provided enough space for the string */
	if ((NULL == asid) || (length < ASID_STRING_LENGTH)) {
		return ASID_STRING_LENGTH;
	}
	/* Default behaviour for platforms other than zOS, simply return the ASID string token */
	strcpy(asid, ASID_STRING);

	return 0;
}
