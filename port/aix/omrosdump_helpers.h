/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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


