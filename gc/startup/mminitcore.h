/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef MMINITCORE_H_
#define MMINITCORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "omr.h"
intptr_t initializeMutatorModel(OMR_VMThread* omrVMThread);
void cleanupMutatorModel(OMR_VMThread* omrVMThread, uintptr_t flushCaches);

intptr_t gcOmrInitializeDefaults(OMR_VM* omrVM);

void gcOmrInitializeTrace(OMR_VMThread *omrVMThread);

#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* MMINITCORE_H_ */
