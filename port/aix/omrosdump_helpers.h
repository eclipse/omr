/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * @brief Dump formatting
 */

#include "omrport.h"
#include "omrportpriv.h"

/* TODO: these should be moved back to omrosdump_helpers.c once the refactoring of omrosdump.c is complete */
#include <core.h>
#define DEFAULT_CORE_FILE_NAME "core"
/* endTODO */

void findOSPreferredCoreDir(struct OMRPortLibrary *portLibrary, char *buffer);
uintptr_t getPEnvValue(struct OMRPortLibrary *portLibrary, char *envVar, char *infoString);
intptr_t genSystemCoreUsingGencore(struct OMRPortLibrary *portLibrary, char *filename);
void appendCoreName(OMRPortLibrary *portLibrary, char *corepath, intptr_t pid);


