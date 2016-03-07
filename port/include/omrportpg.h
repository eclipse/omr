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

#ifndef omrportpg_h
#define omrportpg_h

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
