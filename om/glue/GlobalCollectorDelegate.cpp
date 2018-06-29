/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "GlobalCollectorDelegate.hpp"

#include <OMR/Om/CellOperations.hpp>
#include <OMR/Om/RootRef.hpp>

#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "MarkingScheme.hpp"
#include "ObjectHeapIterator.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include <iostream>

using namespace OMR::Om;

#if defined(OMR_GC_PAINT_HEAP)

constexpr std::uint8_t POISON = 0x5E;

void poison(Cell* cell, std::size_t size) {
#if defined(OMR_OM_TRACE) || 1
		std::cerr << "(poison " << cell << ":size " << size << ")" << std::endl;
#endif
	memset(cell, POISON, size);
	MM_HeapLinkedFreeHeader::fillWithHoles(cell, size);
}

void MM_GlobalCollectorDelegate::poison(Cell* cell) {
	std::size_t size = _extensions->objectModel.getConsumedSizeInBytesWithHeader(cell);
	::poison(cell, size);
}

void MM_GlobalCollectorDelegate::poisonUnmarkedObjectsInRegion(GC_ObjectHeapIterator& iter) {
	for (Cell* cell = iter.nextObject(); cell != nullptr; cell = iter.nextObject()) {
		bool marked = _markingScheme->isMarked(cell);
		// Shapes cannot be painted. In order for the heap walk to work, we need to know
		// object sizes, which comes from shapes.
		if (cellKind(cell) == CellKind::SHAPE) {
			continue;
		}
		if (!marked) {
			poison(cell);
		}
	}
}

void MM_GlobalCollectorDelegate::poisonUnmarkedObjects(MM_EnvironmentBase* env) {
	/* this puts the heap into the state required to walk it */
	// flushCachesForGC(env);

	MM_HeapRegionManager* regionManager = _extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);

	MM_HeapRegionDescriptor* hrd = NULL;
	while (NULL != (hrd = regionIterator.nextRegion())) {
		GC_ObjectHeapIteratorAddressOrderedList it(_extensions, hrd, false);
		poisonUnmarkedObjectsInRegion(it);
	}
}

#endif // OMR_GC_PAINT_HEAP

void MM_GlobalCollectorDelegate::postMarkProcessing(MM_EnvironmentBase* env) {
#if defined(OMR_GC_PAINT_HEAP)
	poisonUnmarkedObjects(env);
#endif
}
