/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#if !defined(OMR_GC_MODRON_CORE_H_)
#define OMR_GC_MODRON_CORE_H_

/*
 * @ddr_namespace: map_to_type=J9ModroncoreConstants
 */

#include "omrcfg.h"
#include "omrcomp.h"

/**
 * Card state definitions
 * @note CARD_DIRTY 0x01 has been hardcoded in builder see J9VMObjectAccessBarrier>>#dirtyCard
 * @ingroup GC_Include
 * @{
 */
typedef U_8 Card;

enum {
	CARD_SIZE_SHIFT	= 9,	/* base2 log of CARD_SIZE, used to change division into shift */
	CARD_SIZE = 512	/* size of a "card" of the heap - in a sense:  the granule of deferred mark map update */
};

/*
 * The following definitions are duplicated in j9modron.h.
 * Both versions of each definition must match exactly.
 */

/* means that this card requires no special handling in the next collection as a result of previous collection or mutator activity */
#define CARD_CLEAN 0x00
/* means that this card has been changed by the mutator since the collector last processed it */
#define CARD_DIRTY 0x01
/* means that a PGC has viewed the card but the next GMP increment (or SYS/OOM fall-back) must also process the card (since it could have hidden objects) */
#define CARD_GMP_MUST_SCAN 0x02
/* means that a GMP has viewed the card but the next PGC increment must also process the card (since it might have unseen inter-region references - the GMP has a smaller live set so it wouldn't see them nor does it update the RSLists) */
#define CARD_PGC_MUST_SCAN 0x03
/* largely equivalent to CARD_PGC_MUST_SCAN but can only exist between RSCL flushing and the end of PGC mark and has the special connotation that the only objects, in this card, which refer to other regions have their remembered bit set */
#define CARD_REMEMBERED 0x04
/* largely equivalent to CARD_DIRTY but can only exist between RSCL flushing and the end of PGC mark and has the special connotation that the only objects, in this card, which refer to other regions have their remembered bit set */
#define CARD_REMEMBERED_AND_GMP_SCAN 0x05
/* used to state that this card contains objects which refer to other regions but all of those who do have their remembered bit set and are not yet in the region's RSCL - the compact treats this much like PGC_MUST_SCAN in that it may promote it to GMP_MUST_SCAN if a GMP is active */
#define CARD_MARK_COMPACT_TRANSITION 0x06
/* used when a value must be passed which is considered uninitialized or otherwise invalid */
#define CARD_INVALID 0xFF

/**
 * @}
 */

/**
 * The following definitions are duplicated in j9modron.h.
 * Both versions of each definition must match exactly.
 * 
 * #defines representing the vmState that should be set during various GC activities
 * @note J9VMSTATE_GC is the "major mask" representing a "GC" activity - we OR in a minor mask
 * representing the specific activity.
 * @ingroup GC_Include
 * @{
 */
#define J9VMSTATE_GC 0x20000
#define J9VMSTATE_GC_MARK (J9VMSTATE_GC | 0x0002)
#define J9VMSTATE_GC_SWEEP (J9VMSTATE_GC | 0x0003)
#define J9VMSTATE_GC_COMPACT (J9VMSTATE_GC | 0x0004)
#define J9VMSTATE_GC_CONCURRENT_MARK_PREPARE_CARD_TABLE (J9VMSTATE_GC | 0x0006)
#define J9VMSTATE_GC_COMPACT_FIX_HEAP_FOR_WALK (J9VMSTATE_GC | 0x0007)
#define J9VMSTATE_GC_CONCURRENT_MARK_CLEAR_NEW_MARKBITS (J9VMSTATE_GC | 0x0008)
#define J9VMSTATE_GC_CONCURRENT_MARK_SCAN_REMEMBERED_SET (J9VMSTATE_GC | 0x0009)
#define J9VMSTATE_GC_CONCURRENT_MARK_FINAL_CLEAN_CARDS (J9VMSTATE_GC | 0x000A)
#define J9VMSTATE_GC_CONCURRENT_SWEEP_COMPLETE_SWEEP (J9VMSTATE_GC | 0x000B)
#define J9VMSTATE_GC_CONCURRENT_SWEEP_FIND_MINIMUM_SIZE_FREE (J9VMSTATE_GC | 0x000C)
#define J9VMSTATE_GC_SCAVENGE (J9VMSTATE_GC | 0x000F)
#define J9VMSTATE_GC_CONCURRENT_MARK_TRACE (J9VMSTATE_GC | 0x0016)
#define J9VMSTATE_GC_CONCURRENT_SWEEP (J9VMSTATE_GC | 0x0017)
#define J9VMSTATE_GC_COLLECTOR_CONCURRENTGC (J9VMSTATE_GC | 0x0012)
#define J9VMSTATE_GC_COLLECTOR_GLOBALGC (J9VMSTATE_GC | 0x0014)
#define J9VMSTATE_GC_COLLECTOR_SCAVENGER (J9VMSTATE_GC | 0x0015)
#define J9VMSTATE_GC_ALLOCATE_OBJECT (J9VMSTATE_GC | 0x0019)
#define J9VMSTATE_GC_TLH_RESET (J9VMSTATE_GC | 0x001D)
#define J9VMSTATE_GC_CONCURRENT_MARK_COMPLETE_TRACING (J9VMSTATE_GC | 0x001E)
#define J9VMSTATE_GC_CHECK_RESIZE (J9VMSTATE_GC | 0x0020)
#define J9VMSTATE_GC_PERFORM_RESIZE (J9VMSTATE_GC | 0x0021)
#define J9VMSTATE_GC_DISPATCHER_IDLE (J9VMSTATE_GC | 0x0025)
#define J9VMSTATE_GC_CONCURRENT_SCAVENGER (J9VMSTATE_GC | 0x0026)
#define J9VMSTATE_GC_CARD_CLEANER_FOR_MARKING (J9VMSTATE_GC | 0x0101)

/**
 * @}
 */

#endif /* OMR_GC_MODRONCORE_H_ */
