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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(GLOBALVLHGCSTATS_HPP_)
#define GLOBALVLHGCSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#if defined(OMR_GC_VLHGC)

/**
 * Storage for statistics relevant to global garbage collections
 * @ingroup GC_Stats
 */
class MM_GlobalVLHGCStats
{
public:
	uintptr_t gcCount;  /**< Count of the number of GC cycles that have occurred */
	uintptr_t incrementCount;	/**< The number of incremental taxation entry point collection operations which have been completed since the VM started */

	MM_GlobalVLHGCStats() :
		gcCount(0)
		,incrementCount(0)
	{};
};

#endif /* OMR_GC_VLHGC */
#endif /* GLOBALVLHGCSTATS_HPP_ */
