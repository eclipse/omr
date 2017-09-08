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
#if !defined(SIZECLASSES_HPP_)
#define SIZECLASSES_HPP_

#include "omrcfg.h"
#include "modronbase.h"
#include "sizeclasses.h"

#include "BaseVirtual.hpp"
#include "Debug.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_EnvironmentBase;

class MM_SizeClasses : public MM_BaseVirtual
{
/* Data members & types */
public:
protected:
private:
	uintptr_t* _smallCellSizes; /**< Array mapping size classes to the cell size of that size class. The array actually lives in the J9JavaVM. */
	uintptr_t* _smallNumCells; /**< Array mapping size classes to the number of cells on a region of that size class. The array actually lives in the J9JavaVM. */
	uintptr_t* _sizeClassIndex; /**< maps size request to size classes. The array actually lives in the OMR vm. */
	
/* Methods */
public:
	static MM_SizeClasses* newInstance(MM_EnvironmentBase* env);
	virtual void kill(MM_EnvironmentBase *env);
	
	MMINLINE uintptr_t getCellSize(uintptr_t sizeClass) const
	{
		assume(sizeClass >= OMR_SIZECLASSES_MIN_SMALL && sizeClass <= OMR_SIZECLASSES_MAX_SMALL, "getCellSize: invalid sizeclass");
		return _smallCellSizes[sizeClass];
	}

	MMINLINE uintptr_t getNumCells(uintptr_t sizeClass) const
	{
		assume(sizeClass >= OMR_SIZECLASSES_MIN_SMALL && sizeClass <= OMR_SIZECLASSES_MAX_SMALL, "getNumCells: invalid sizeclass");
		return _smallNumCells[sizeClass];
	}
	
	MMINLINE uintptr_t getSizeClassSmall(uintptr_t sizeInBytes) const 
	{
		return _sizeClassIndex[sizeInBytes / sizeof(uintptr_t)];
	}
	
	MMINLINE uintptr_t getSizeClass(uintptr_t sizeInBytes) const
	{
		if (sizeInBytes > OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES) {
			return OMR_SIZECLASSES_LARGE;
		}
		return _sizeClassIndex[sizeInBytes / sizeof(uintptr_t)];
	}
	
protected:
	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	MM_SizeClasses(MM_EnvironmentBase* env)
	{
		_typeId = __FUNCTION__;
	};
	
private:
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* SIZECLASSES_HPP_ */
