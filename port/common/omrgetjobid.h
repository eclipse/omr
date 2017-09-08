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
