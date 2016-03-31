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
#include "omrgetjobid.h"

#define JOBID_STRING_LENGTH 9

/* Get the zOS job ID */
uintptr_t
omrget_jobid(struct OMRPortLibrary *portLibrary, char *jobid, uintptr_t length)
{
	/* See JSAB control block definition in z/OS MVS Data Areas Volume 4 */
	struct jsab {
		char _padding[20]; /* hex 0x14 */
		char jsabjbid[8]; /* job ID */
	} * __ptr32 jsab_ptr;
	/* See ASSB control block definition in z/OS MVS Data Areas Volume 1 */
	struct assb {
		char _padding[168]; /* hex 0xA8 */
		struct jsab *__ptr32 assbjsab;
	} * __ptr32 assb_ptr;
	/* See ASCB control block definition in z/OS MVS Data Areas Volume 1 */
	struct ascb {
		char _padding[336]; /* hex 0x150 */
		struct assb *__ptr32 ascbassb;
	} * __ptr32 ascb_ptr;
	/* See PSA control block definition in z/OS MVS Data Areas Volume 5 */
	struct psa {
		char _padding[548]; /* hex 0x224 */
		struct ascb *__ptr32 psaaold;
	} * psa_ptr;

	int32_t result;

	/* PSA control block is at address zero */
	psa_ptr = 0;
	/* Address of ASCB control block is PSAAOLD field in PSA control block */
	ascb_ptr = psa_ptr->psaaold;

	if (NULL != ascb_ptr) {
		/* Address of ASSB control block is ASCBASSB field in ASCB control block */
		assb_ptr = (struct assb *)ascb_ptr->ascbassb;

		if (NULL != assb_ptr) {
			/* Address of JSAB control block is ASSBJSAB field in ASSB control block */
			jsab_ptr = (struct jsab *)assb_ptr->assbjsab;

			if (NULL != jsab_ptr) {
				/* Job ID is JSABJBID field in JSAB control block. Convert from EBCDIC to internal UTF8 */
				result = omrstr_convert(portLibrary, J9STR_CODE_PLATFORM_RAW, J9STR_CODE_MUTF8,
									   jsab_ptr->jsabjbid, sizeof(jsab_ptr->jsabjbid), jobid, length);
				if (OMRPORT_ERROR_STRING_BUFFER_TOO_SMALL == result) {
					/* return the number of bytes required to hold the converted text */
					return omrstr_convert(portLibrary, J9STR_CODE_PLATFORM_RAW, J9STR_CODE_MUTF8,
										 jsab_ptr->jsabjbid, sizeof(jsab_ptr->jsabjbid), NULL, 0);
				}
			}
		}
	}
	return 0;
}
