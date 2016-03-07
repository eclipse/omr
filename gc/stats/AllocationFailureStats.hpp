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

#if !defined(ALLOCATIONFAILURESTATS_HPP_)
#define ALLOCATIONFAILURESTATS_HPP_

#include "omrcomp.h"

#include "Base.hpp"

/**
 * @todo Provide class documentation
 * @ingroup GC_Stats
 */
class MM_AllocationFailureStats : public MM_Base 
{
public:
	uintptr_t subSpaceType;
	uintptr_t allocationFailureSize;
	uintptr_t allocationFailureCount;

	/**
	 * Create a AllocationFailureStats object.
	 */   
	MM_AllocationFailureStats() :
		MM_Base(),
		subSpaceType(0),
		allocationFailureSize(0),
		allocationFailureCount(0)
	{};
};

#endif /* ALLOCATIONFAILURESTATS_HPP_ */
