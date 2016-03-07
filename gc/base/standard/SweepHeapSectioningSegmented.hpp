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

#if !defined(SWEEPSCHEMESECTIONINGSEGMENTED_HPP_)
#define SWEEPSCHEMESECTIONINGSEGMENTED_HPP_

#include "SweepHeapSectioning.hpp"

class MM_ParallelSweepChunkArray;
class MM_EnvironmentBase;

/**
 * Support for sectioning the heap into chunks useable by sweep (and compact).
 * 
 * @ingroup GC_Modron_Standard
 */
class MM_SweepHeapSectioningSegmented : public MM_SweepHeapSectioning
{
private:
protected:
	virtual uintptr_t estimateTotalChunkCount(MM_EnvironmentBase *env);
	virtual uintptr_t calculateActualChunkNumbers() const;

public:
	static MM_SweepHeapSectioningSegmented *newInstance(MM_EnvironmentBase *env);

	virtual uintptr_t reassignChunks(MM_EnvironmentBase *env);

	MM_SweepHeapSectioningSegmented(MM_EnvironmentBase *env)
		: MM_SweepHeapSectioning(env)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* SWEEPSCHEMESECTIONINGSEGMENTED_HPP_ */
