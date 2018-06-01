/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "omr.h"

/* This glue function is implemented in a different file from LanguageVMGlue so
 * that it can be used without requiring all the dependencies of LanguageVMGlue.
 * (Since LanguageVMGlue interacts with OMR_VM initialization, it prereqs all
 * GC/RAS/OMR core modules.)
 */
#if defined(WIN32)
omr_error_t OMR_Glue_GetVMDirectoryToken(void** token) {
	/* NULL means the runtime will look in the current executable's directory */
	*token = NULL;
	return OMR_ERROR_NONE;
}
#endif /* defined(WIN32) */

/**
 * Provides the thread name to be used when no name is given.
 */
char* OMR_Glue_GetThreadNameForUnamedThread(OMR_VMThread* vmThread) { return "(unnamed thread)"; }
