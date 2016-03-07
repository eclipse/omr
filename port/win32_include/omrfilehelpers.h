/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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


#ifndef omrfilehelpers_h
#define omrfilehelpers_h

#include <windows.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "portnls.h"
#include "ut_omrport.h"

extern omrthread_tls_key_t tlsKeyOverlappedHandle;

int32_t
findError(int32_t errorCode);

int32_t
omrfile_lock_bytes_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length, BOOLEAN async);

int32_t
omrfile_unlock_bytes_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length, BOOLEAN async);

HANDLE
omrfile_get_overlapped_handle_helper(struct OMRPortLibrary *portLibrary);

#endif     /* omrfilehelpers_h */
