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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


#ifndef MODRONAPICORE_HPP_
#define MODRONAPICORE_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "pool_api.h"

#ifdef __cplusplus
extern "C" {
#endif
const char* omrgc_get_version(OMR_VM *omrVM);
extern uintptr_t omrgc_condYieldFromGC(OMR_VMThread *omrVMThread, uintptr_t componentType);
void *omrgc_walkLWNRLockTracePool(void *omrVM, pool_state *state);
#ifdef __cplusplus
}
#endif

#endif /* MODRONAPICORE_HPP_ */
