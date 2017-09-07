/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
