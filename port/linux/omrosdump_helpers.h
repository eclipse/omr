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

/* TODO: move out struct to omrosdump_helpers.c to complete refactoring */
typedef struct MarkAllPagesWritableHeader {
	uint32_t eyeCatcher;
	uint32_t checksum;
	uintptr_t size;
	uintptr_t maxSize;
} MarkAllPagesWritableHeader;

uintptr_t renameDump(struct OMRPortLibrary *portLibrary, char *filename, pid_t pid, int signalNumber);
char *markAllPagesWritable(struct OMRPortLibrary *portLibrary);




