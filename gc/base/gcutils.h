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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(GCUTILS_H_)
#define GCUTILS_H_

#include "omrcfg.h"
#include "modronbase.h"
#include "j9nongenerated.h"

/**
 * @}
 */
#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

void qualifiedSize(uintptr_t *byteSize, const char **qualifier);

#if defined(OMR_GC_MODRON_COMPACTION)
const char *getCompactionReasonAsString(CompactReason reason);
const char *getCompactionPreventedReasonAsString(CompactPreventedReason reason);
#endif /*OMR_GC_MODRON_COMPACTION*/

#if defined(OMR_GC_REALTIME)
const char *getGCReasonAsString(GCReason reason);
#endif /* OMR_GC_REALTIME */

#if defined(OMR_GC_MODRON_SCAVENGER)
const char *getPercolateReasonAsString(PercolateReason mode);
#endif /* OMR_GC_MODRON_SCAVENGER */

const char *getExpandReasonAsString(ExpandReason reason);
const char *getContractReasonAsString(ContractReason reason);
const char *getLoaResizeReasonAsString(LoaResizeReason reason);

const char *getSystemGCReasonAsString(uint32_t gcCode);

#ifdef __cplusplus
} /* extern "C" { */
#endif  /* __cplusplus */

#endif /* GCUTILS_H_ */

