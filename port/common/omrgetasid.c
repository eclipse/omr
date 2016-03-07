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
