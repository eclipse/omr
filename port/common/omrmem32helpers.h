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

#ifndef omrmem32helpers_h
#define omrmem32helpers_h

#include "omrport.h"
#include "omrportpriv.h"

int32_t startup_memory32(struct OMRPortLibrary *portLibrary);
void shutdown_memory32(struct OMRPortLibrary *portLibrary);
void *allocate_memory32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite);
void free_memory32(struct OMRPortLibrary *portLibrary, void *memoryPointer);
uintptr_t ensure_capacity32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount);

#endif /* omrmemhelpers_h */


