/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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

#if !defined(INDEXABLEOBJECTSCANNER_HPP_)
#define INDEXABLEOBJECTSCANNER_HPP_

#include "ObjectScanner.hpp"

class GC_IndexableObjectScanner : public GC_ObjectScanner
{
	/* Data Members */
private:

protected:
	fomrobject_t *_endPtr;	/**< pointer to end of last array element in scan segment */
	fomrobject_t *_basePtr;	/**< pointer to base array element */
	fomrobject_t *_limitPtr;/**< pointer to end of last array element */

public:

	/* Member Functions */
private:

protected:
	/**
	 * @param env The scanning thread environment
	 * @param[in] arrayPtr pointer to the array to be processed
	 * @param[in] basePtr pointer to the first contiguous array cell
	 * @param[in] limitPtr pointer to end of last contiguous array cell
	 * @param[in] scanPtr pointer to the array cell where scanning will start
	 * @param[in] endPtr pointer to the array cell where scanning will stop
	 * @param[in] flags scanning context flags
	 */
	GC_IndexableObjectScanner(
		MM_EnvironmentStandard *env
		, omrobjectptr_t arrayPtr
		, fomrobject_t *basePtr
		, fomrobject_t *limitPtr
		, fomrobject_t *scanPtr
		, fomrobject_t *endPtr
		, uintptr_t flags
	)
		: GC_ObjectScanner(
			env
			, arrayPtr
			, scanPtr
			, ((endPtr - scanPtr) < _bitsPerScanMap) ? (((uintptr_t)1 << (endPtr - scanPtr)) - 1) : UDATA_MAX
			, setNoMoreSlots(flags | GC_ObjectScanner::indexableObject, (endPtr - scanPtr) <= _bitsPerScanMap)
		)
		, _endPtr(endPtr)
		, _basePtr(basePtr)
		, _limitPtr(limitPtr)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Splitting constructor sets split scan and end pointers to appropriate scanning range
	 * @param[in] env current environment (per thread)
	 * @param[in] objectPtr the object to be scanned
	 * @param splitAmount The number of array elements to include
	 */
	GC_IndexableObjectScanner(MM_EnvironmentStandard *env, GC_IndexableObjectScanner *objectScanner, uintptr_t splitAmount)
		: GC_ObjectScanner(
			env
			, objectScanner->_parentObjectPtr
			, objectScanner->_endPtr
			, splitAmount < (uintptr_t)_bitsPerScanMap ? (((uintptr_t)1 << splitAmount) - 1) : UDATA_MAX
			, setNoMoreSlots(objectScanner->_flags, (intptr_t)splitAmount <= _bitsPerScanMap)
		)
		, _endPtr(objectScanner->_endPtr + splitAmount)
		, _basePtr(objectScanner->_basePtr)
		, _limitPtr(objectScanner->_limitPtr)
	{
		_typeId = __FUNCTION__;
		Assert_MM_true(0 <= (intptr_t)splitAmount);
	}

	/**
	 * Set up the scanner.
	 * @param[in] env current environment (per thread)
	 */
	MMINLINE void
	initialize(MM_EnvironmentStandard *env)
	{
		Assert_MM_true(_basePtr <= _scanPtr);
		Assert_MM_true(_scanPtr <= _endPtr);
		Assert_MM_true(_endPtr <= _limitPtr);
		GC_ObjectScanner::initialize(env);
	}

public:
	/**
	 * Get the maximal index for the array. Array indices are assumed to be zero-based.
	 */
	MMINLINE uintptr_t getIndexableRange() { return _limitPtr - _basePtr; }

	/**
	 * Reset truncated end pointer to force scanning to limit pointer (scan to end of indexable object). This
	 * must be called if this scanner cannot be split to hive off the tail
	 */
	MMINLINE void scanToLimit() { _endPtr = _limitPtr; }

	/**
	 * Split this instance and set split scan/end pointers to indicate split scan range.
	 *
	 * @param env The scanning thread environment
	 * @param allocSpace Pointer to memory where split scanner will be instantiated (in-place)
	 * @param splitAmount The maximum number of array elements to include
	 * @return Pointer to split scanner in allocSpace
	 */
	virtual GC_IndexableObjectScanner *splitTo(MM_EnvironmentStandard *env, void *allocSpace, uintptr_t splitAmount) = 0;
};

#endif /* INDEXABLEOBJECTSCANNER_HPP_ */
