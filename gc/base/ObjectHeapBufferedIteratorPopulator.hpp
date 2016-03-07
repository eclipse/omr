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


#if !defined(OBJECTHEAPBUFFEREDITERATORPOPULATOR_HPP_)
#define OBJECTHEAPBUFFEREDITERATORPOPULATOR_HPP_

#include "objectdescription.h"

#include "BaseVirtual.hpp"

struct GC_ObjectHeapBufferedIteratorState;
class MM_HeapRegionDescriptor;

/**
 * This is a pure abstract class. 
 * Instances of subclasses of this class are used to populate the buffers of ObjectHeapBufferedIterators.
 * Instances of subclasses of this class are all singletons and should have no state.
 * Instances of subclasses of this class may be instantiated in debugger extensions to iterate over out of process regions.
 */
class MM_ObjectHeapBufferedIteratorPopulator : public MM_BaseVirtual
{
public:
	MM_ObjectHeapBufferedIteratorPopulator()
	{
		_typeId = __FUNCTION__;
	}
	
	/**
	 * Initialize the specified state object in preperation of beginning iteration over the specified region.
	 * The receiver may store any necessary information in the state. The state will not be modified by the
	 * iterator -- it is reserved for use by the ObjectHeapBufferedIteratorPopulator.
	 *
	 * @parm[in] region - the region which will be iterated over
	 * @parm[out] state - the state to be initialized
	 */
	virtual void initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const = 0;

	/**
	 * Populate the cache with more objects.
	 *
	 * @parm[out] cache - the cache array to be populated with J9Object pointers
	 * @parm[in] maxCount - the maximum number of objects to populate the cache with
	 * @parm[in/out] state - a state struct which the receiver may use to record persistent information.
	 * @return the number of objects written to the cache, or 0 if the iteration is finished
	 */
	virtual uintptr_t populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const = 0;
	
	virtual void advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const = 0;
	
	/**
	 * Reset populator with new base and top ranges.
	 * @parm[in] region Current region
	 * @parm[in] state Current iterator state
	 * @parm[in] base New region scan base
	 * @parm[in] top New region scan top
	 */
	virtual void reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const = 0;
};

#endif /*OBJECTHEAPBUFFEREDITERATORPOPULATOR_HPP_*/
