/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

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

