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

#ifndef omrportpg_h
#define omrportpg_h

/*
 * @ddr_namespace: default
 */

/** Number of pageSizes supported.  There is always 1 for the default size, and 1 for the 0 terminator.
 * The number of large pages supported determines the remaining size.
 * Responsibility of the implementation of omrvmem to initialize this table correctly.
 */
#define OMRPORT_VMEM_PAGESIZE_COUNT 2

typedef struct OMRPortPlatformGlobals {
	uintptr_t vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of supported page sizes */
	uintptr_t vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of flags describing type of the supported page sizes */
	uintptr_t systemLoggingFlags;
} OMRPortPlatformGlobals;

#define PPG_vmem_pageSize (portLibrary->portGlobals->platformGlobals.vmem_pageSize)
#define PPG_vmem_pageFlags (portLibrary->portGlobals->platformGlobals.vmem_pageFlags)
#define PPG_syslog_flags (portLibrary->portGlobals->platformGlobals.systemLoggingFlags)
#endif /* omrportpg_h */
