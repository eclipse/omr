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

#ifndef omrgetasid_h
#define omrgetasid_h

#include "omrcomp.h"

/**
 * Get the zOS address space ID (ASID). The ASID is a 2-byte number, usually
 * displayed as 4 hex digits.
 *
 * @param[in] portLibrary The port library.
 * @param[in/out] asid Pointer to string to populate with the ASID.
 * 		an empty string is returned if the ASID is not available
 * @param[in] length The length of the data area addressed by asid.
 *
 * @return 0 on success, size of required buffer on failure.
 */
uintptr_t omrget_asid(struct OMRPortLibrary *portLibrary, char *asid, uintptr_t length);

#endif     /* omrgetasid_h */
