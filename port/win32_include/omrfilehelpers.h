/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
