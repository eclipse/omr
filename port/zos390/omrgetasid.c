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

#include <stdlib.h>
#include "omrport.h"
#include "omrgetasid.h"

#define ASID_STRING_LENGTH 5

/* Get the zOS address space ID (ASID) */
uintptr_t
omrget_asid(struct OMRPortLibrary *portLibrary, char *asid, uintptr_t length)
{
	/* See ASCB control block definition in z/OS MVS Data Areas Volume 1 */
	struct ascb {
		char _padding[36]; /* hex 0x24 */
		unsigned short ascbasid;
	};
	/* See PSA control block definition in z/OS MVS Data Areas Volume 5 */
	struct psa {
		char _padding[548]; /* hex 0x224 */
		struct ascb *__ptr32 psaaold;
	};

	/* Check that caller provided enough space for the string */
	if ((NULL == asid) || (length < ASID_STRING_LENGTH)) {
		return ASID_STRING_LENGTH;
	}
	asid[0] = '\0';

	/* z/OS PSA control block is at address zero */
	struct psa *psa_ptr = 0;
	/* Address of ASCB control block is PSAAOLD field in PSA control block */
	struct ascb *__ptr32 ascb_ptr = psa_ptr->psaaold;

	if (NULL != ascb_ptr) {
		/* ASID is 2-byte ASCBASID field in ASCB control block */
		portLibrary->str_printf(portLibrary, asid, length, "%.4X", ascb_ptr->ascbasid);
	}

	return 0;
}
