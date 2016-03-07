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

#ifndef omrgetjobid_h
#define omrgetjobid_h

#include "omrcomp.h"

/**
 * Get the zOS job ID.
 *
 * @param[in] portLibrary The port library.
 * @param[in/out] jobid Pointer to string to populate with the job ID.
 * 		an empty string is returned if the job ID is not available
 * @param[in] length Length of the data area addressed by jobid.
 *
 * @return 0 on success, size of required buffer on failure.
 */
uintptr_t omrget_jobid(struct OMRPortLibrary *portLibrary, char *jobid, uintptr_t length);

#endif     /* omrgetjobid_h */
