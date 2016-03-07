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
 ******************************************************************************/

#if !defined(HEAPMAPITERATOR_HPP_)
#define HEAPMAPITERATOR_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#include "HeapMap.hpp"

class MM_GCExtensionsBase;

/**
 * @name Heap map platform dependent definitions
 * @{
 */
#if defined(OMR_ENV_DATA64)
#define J9MODRON_HMI_SLOT_EMPTY ((uintptr_t)0x0000000000000000)
#else /* OMR_ENV_DATA64 */
#define J9MODRON_HMI_SLOT_EMPTY ((uintptr_t)0x00000000)
#endif /* OMR_ENV_DATA64 */

#define J9MODRON_HMI_HEAPMAP_ALIGNMENT (J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT * sizeof(uintptr_t))	

/**
 * Iterate over objects in a chunk of heap using a heap map.
 * @note This iterator relies on the appropriate heap map being valid
 * @todo Should this implement @ref GC_ObjectHeapIterator?
 * @ingroup GC_Base
 */
class MM_HeapMapIterator {
	
private:	
	uintptr_t *_heapSlotCurrent;  /**< Current heap slot that corresponds to the heap map bit index being scanned */
	uintptr_t *_heapChunkTop;  /**< Ending heap slot to scan */
	uintptr_t *_heapMapSlotCurrent;  /**< Current heap map slot that contains the bits to scan for the corresponding heap */
	uintptr_t _bitIndexHead;  /**< Current bit index in heap map slot that is being scanned */
	uintptr_t _heapMapSlotValue;  /**< Cached heap map slot value to avoid memory cache polution */
	MM_GCExtensionsBase * const _extensions; /**< The GC extensions for the JVM */
	bool _useLargeObjectOptimization;	/**< Set to true if we want to read objects from the heap and determine their size in order to skip mark map bits which are inside the object.  If this is set to false, we will blindly return the addresses representing the set bits in the mark map */
	
public:
	omrobjectptr_t nextObject();

	bool setHeapMap(MM_HeapMap *heapMap);

	bool reset(MM_HeapMap *heapMap, uintptr_t *heapChunkBase, uintptr_t *heapChunkTop);

	MM_HeapMapIterator(MM_GCExtensionsBase *extensions, MM_HeapMap *heapMap, uintptr_t *heapChunkBase, uintptr_t *heapChunkTop, bool useLargeObjectOptimization = true)
		: _extensions(extensions)
		, _useLargeObjectOptimization(useLargeObjectOptimization)
	{
		reset(heapMap, heapChunkBase, heapChunkTop);
	}

	/**
	 * @note heapMap, heapChunkBase and heapChunkTop are not specified, so reset()
	 *  must be called explicitly by the caller of this constructor.
	 */ 
	MM_HeapMapIterator(MM_GCExtensionsBase *extensions)
		: _heapSlotCurrent(NULL)
		, _heapChunkTop(NULL)
		, _heapMapSlotCurrent(NULL)
		, _bitIndexHead(0)
		, _heapMapSlotValue(0)
		, _extensions(extensions)
		, _useLargeObjectOptimization(true)
	{}

};

#endif /* HEAPMAPITERATOR_HPP_ */
