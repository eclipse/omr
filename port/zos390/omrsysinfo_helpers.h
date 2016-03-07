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
