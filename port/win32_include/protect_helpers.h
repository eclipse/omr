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

#ifndef protect_helpers_h
#define protect_helpers_h

/**
 * @internal @file
 * @ingroup Port
 * @brief Helpers for protecting shared memory regions in the virtual address space (used by omrmmap and j9shmem).
 */

intptr_t protect_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags);
uintptr_t protect_region_granularity(struct OMRPortLibrary *portLibrary, void *address);
#endif /*  protect_helpers_h */
