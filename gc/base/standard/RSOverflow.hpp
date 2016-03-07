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

#if !defined(RSOVERFLOW_HPP_)
#define RSOVERFLOW_HPP_

#include "omrcfg.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkMap.hpp"
#include "HeapMapIterator.hpp"

class MM_RSOverflow
{
public:
protected:
private:
	MM_GCExtensionsBase *_extensions;			/**< GC Extensions */
	MM_MarkMap *_markMap;						/**< Mark Map stolen from Global Collector */
	bool _searchMode;							/**< mode switch: initialized in Modify mode, after first nextObject call is switched to Search mode */
	MM_HeapMapIterator _markedObjectIterator;	/**< Iterator to walk marked objects */

public:

	/*
	 * Mark object as Remembered
	 * All Mark work should be completed before first nextObject call.
	 * After this any modifications in mark map are forbidden
	 * @param objectPtr address of object to mark
	 */
	MMINLINE void addObject(omrobjectptr_t objectPtr)
	{
		if (!_searchMode) {
			/* Set bit to mark map only if switch in Modify mode */
			_markMap->setBit(objectPtr);
		} else {
			/* Update of mark map in Search mode is forbidden */
			Assert_MM_unreachable();
		}
	}

	/*
	 * Get next object marked as Remembered
	 * First call will disable modifications in mark map and reinitialize iterator
	 * @return address of next object or NULL
	 */
	MMINLINE omrobjectptr_t nextObject()
	{
		if (!_searchMode) {
			/* First call in Search mode - reinitialize iterator with actual values */
			_markedObjectIterator.reset(_markMap, (uintptr_t *)_extensions->heapBaseForBarrierRange0, (uintptr_t *)((uintptr_t)_extensions->heapBaseForBarrierRange0 + _extensions->heapSizeForBarrierRange0));
			_searchMode = true;
		}
		return _markedObjectIterator.nextObject();
	}

	/**
	 * Construct a new RSOverflow
	 */
	MMINLINE MM_RSOverflow(MM_EnvironmentBase *env)
		: _extensions(env->getExtensions())
		, _markMap(NULL)
		, _searchMode(false)
		, _markedObjectIterator(env->getExtensions())
	{
		initialize(env);
	}

protected:
private:

	/*
	 * Do initial settings
	 * @param env - Environment
	 */
	void initialize(MM_EnvironmentBase *env);

};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* RSOVERFLOW_HPP_ */
