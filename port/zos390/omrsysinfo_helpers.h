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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMRSYSINFO_HELPERS_H_
#define OMRSYSINFO_HELPERS_H_

#include "omrport.h"

#define J9BYTES_PER_PAGE            4096		/* Size of main storage frame/virtual storage page/auxiliary storage slot */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
* End of portion extracted from the header "//'SYS1.SIEAHDR.H(IWMQVSH)'".
*/

/**
 * Retrieve z/Architecture facility bits.
 *
 * @param [in]  lastDoubleWord   Size of the bits array in number of uint64_t, minus 1.
 * @param [out] bits             Caller-supplied uint64_t array that gets populated with the facility bits.
 *
 * @return The index of the last valid uint64_t in the bits array.
 */
extern int getstfle(int lastDoubleWord, uint64_t *bits);

/**
 * Function retrieves and populates memory usage statistics on a z/OS platform.
 * @param [in] portLibrary The Port Library Handle.
 * @param[out] memInfo     Pointer to J9MemoryInfo struct which we populate with memory usage.
 * @return                 0 on success; negative value on failure.
 */
int32_t
retrieveZOSMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo);

/**
 * Function retrieves and populates processor usage statistics on a z/OS platform.
 * @param [in] portLibrary The Port Library Handle.
 * @param[out] procInfo    Pointer to J9ProcessorInfos struct that we populate with processor usage.
 * @return                 0 on success; negative value on failure.
 */
int32_t
retrieveZOSProcessorStats(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* OMRSYSINFO_HELPERS_H_ */
