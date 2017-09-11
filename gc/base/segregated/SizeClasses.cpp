/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
