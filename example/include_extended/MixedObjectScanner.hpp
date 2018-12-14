/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef MIXEDOBJECTSCANNER_HPP_
#define MIXEDOBJECTSCANNER_HPP_

#include "ObjectScanner.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectModel.hpp"

class GC_MixedObjectScanner : public GC_ObjectScanner
{
	/* Data Members */
private:
	fomrobject_t * const _endPtr;	/**< end scan pointer */
	fomrobject_t *_mapPtr;			/**< pointer to first slot in current scan segment */

protected:

public:

	/* Member Functions */
private:

protected:
	/**
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr the object to be processed
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_MixedObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, uintptr_t flags)
		: GC_ObjectScanner(env, objectPtr, (fomrobject_t *)objectPtr + 1, 0, flags, 0)
		, _endPtr((fomrobject_t *)((uint8_t*)objectPtr + MM_GCExtensionsBase::getExtensions(env->getOmrVM())->objectModel.getConsumedSizeInBytesWithHeader(objectPtr)))
		, _mapPtr(_scanPtr)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Subclasses must call this method to set up the instance description bits and description pointer.
	 * @param[in] env The scanning thread environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
		GC_ObjectScanner::initialize(env);

		intptr_t slotCount = _endPtr - _scanPtr;

		/* Initialize the slot map assuming all slots are reference slots or NULL */
		if (slotCount < _bitsPerScanMap) {
			_scanMap = (((uintptr_t)1) << slotCount) - 1;
			setNoMoreSlots();
		} else {
			_scanMap = ~((uintptr_t)0);
			if (slotCount == _bitsPerScanMap) {
				setNoMoreSlots();
			}
		}
	}

public:
	/**
	 * In-place instantiation and initialization for mixed obect scanner.
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr The object to scan
	 * @param[in] allocSpace Pointer to space for in-place instantiation (at least sizeof(GC_MixedObjectScanner) bytes)
	 * @param[in] flags Scanning context flags
	 * @return Pointer to GC_MixedObjectScanner instance in allocSpace
	 */
	MMINLINE static GC_MixedObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
	{
		GC_MixedObjectScanner *objectScanner = NULL;
		if (NULL != allocSpace) {
			new(allocSpace) GC_MixedObjectScanner(env, objectPtr, flags);
			objectScanner = (GC_MixedObjectScanner *)allocSpace;
			objectScanner->initialize(env);
		}
		return objectScanner;
	}

	MMINLINE uintptr_t getBytesRemaining() { return sizeof(fomrobject_t) * (_endPtr - _scanPtr); }

	/**
	 * @see GC_ObjectScanner::getNextSlotMap()
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t &slotMap, bool &hasNextSlotMap)
	{
		intptr_t slotCount = _endPtr - _scanPtr;

		/* Initialize the slot map assuming all slots are reference slots or NULL */
		if (slotCount < _bitsPerScanMap) {
			slotMap = (((uintptr_t)1) << slotCount) - 1;
			hasNextSlotMap = false;
		} else {
			slotMap = ~((uintptr_t)0);
			hasNextSlotMap = slotCount > _bitsPerScanMap;
		}

		_mapPtr += _bitsPerScanMap;
		return _mapPtr;
	}

#if defined(OMR_GC_LEAF_BITS)
	/**
	 * @see GC_ObjectScanner::getNextSlotMap(uintptr_t&, uintptr_t&, bool&)
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t &slotMap, uintptr_t &leafMap, bool &hasNextSlotMap)
	{
		leafMap = 0;
		return getNextSlotMap(slotMap, hasNextSlotMap);
	}
#endif /* OMR_GC_LEAF_BITS */
};

#endif /* MIXEDOBJECTSCANNER_HPP_ */
