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
