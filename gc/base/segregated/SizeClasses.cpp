/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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
#include "SizeClasses.hpp"

#include "EnvironmentBase.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/** 
 * Initial size classes. See Metronome.hpp for macro definitions.
 */
uintptr_t initialCellSizes[OMR_SIZECLASSES_NUM_SMALL+1] = SMALL_SIZECLASSES;

MM_SizeClasses*
MM_SizeClasses::newInstance(MM_EnvironmentBase* env)
{
	MM_SizeClasses* sizeClasses;
	
	sizeClasses = (MM_SizeClasses*)env->getForge()->allocate(sizeof(MM_SizeClasses), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sizeClasses) {
		new(sizeClasses) MM_SizeClasses(env);
		if (!sizeClasses->initialize(env)) {
			sizeClasses->kill(env);	
			sizeClasses = NULL;   			
		}
	}
	return sizeClasses;
}

void
MM_SizeClasses::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_SizeClasses::initialize(MM_EnvironmentBase *env)
{
	OMR_SizeClasses* sizeClasses = env->getOmrVM()->_sizeClasses;
	_smallCellSizes = sizeClasses->smallCellSizes;
	_smallNumCells = sizeClasses->smallNumCells;
	_sizeClassIndex = sizeClasses->sizeClassIndex;
	
	memcpy(_smallCellSizes, initialCellSizes, sizeof(initialCellSizes));
	
	_sizeClassIndex[0] = 0;
	_smallNumCells[0] = 0;
	for (uintptr_t szClass=OMR_SIZECLASSES_MIN_SMALL; szClass<=OMR_SIZECLASSES_MAX_SMALL; szClass++) {
		_smallNumCells[szClass] = env->getExtensions()->regionSize / _smallCellSizes[szClass];
		
		for (uintptr_t j=1+(getCellSize(szClass-1)/sizeof(uintptr_t)); j<=getCellSize(szClass)/sizeof(uintptr_t); j++) {
			_sizeClassIndex[j] = szClass;
		}
	}
	
	return true;
}

void
MM_SizeClasses::tearDown(MM_EnvironmentBase *envModron)
{
	
}

#endif /* OMR_GC_SEGREGATED_HEAP */
