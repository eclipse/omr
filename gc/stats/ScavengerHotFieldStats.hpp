/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(SCAVENGERHOTFIELDSTATS_HPP_)
#define SCAVENGERHOTFIELDSTATS_HPP_

#include "omr.h"
#include "omrcfg.h"
#include "omrport.h"

#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)

#include "Base.hpp"
#include "Bits.hpp"
#include "ObjectModel.hpp"

class MM_EnvironmentBase;

#define ABS_UINTPTR_DISTANCE(a, b) ((uintptr_t) ((a) < (b) ? (b) - (a) : (a) - (b)))

/**
 * Storage for hot field statistics relevant to a scavenging (semi-space copying) collector.
 * @ingroup GC_Stats
 */
class MM_ScavengerHotFieldStats : public MM_Base
{
public:
	enum ScavengerObjectConnectorType {
		DiffSubSpace = 0, /* link is between different sub spaces */
		NewSubSpace, /* link is within the nursery subspace */
		TenSubSpace, /* link is within the tenure subspace */
		SizeScavengerObjectConnectorType /* how large an array to allocate to index connection types */
	};

	enum ScavengerHotness {
		Cold = 0, /* hotness of a field = cold */
		Hot, /* hotness of a field = hot */
		SizeScavengerHotness
	};

	/* the following fields are stored to eliminate overhead measured at 3% of scavenge times
	 * if the parameters are passed by argument */
	omrobjectptr_t _objectPtr; /**< current object being scanned, parent of current object being copied */
private:
	GC_ObjectModel *_objectModel; /**< pointer to object model (from GC extensions) */
	
	uint8_t _hotness; /**< hotness of current slot being scanned */

	/**
	 * count of connections between parent and child objects
	 * categorized by hotness and by connector type (nursery-nursery, nursery-tenured, tenured-tenured)
	 */
	uintptr_t _connectionCount[SizeScavengerHotness][SizeScavengerObjectConnectorType];

	/**
	 * accumulated distance between parent and child objects
	 * categorized by hotness and by connector type
	 */
#if !defined(OMR_ENV_DATA64)
	uint64_t _interObjectDistance[SizeScavengerHotness][SizeScavengerObjectConnectorType];
#else
	double _interObjectDistance[SizeScavengerHotness][SizeScavengerObjectConnectorType];
#endif
	/**
	 * histogram of logarithmic distance between parent and child objects
	 * categorized by hotness and by connector type
	 * histogram bin boundaries are powers of 2, hence uintptr_t*8 bins are required.
	 */
	uintptr_t _connectionHistgm[sizeof(uintptr_t)*8][SizeScavengerHotness][SizeScavengerObjectConnectorType];

public:
	/**
	 * When no information is known about the hotness of a field, then the default is hot.
	 * This clears to this default value
	 */
	MMINLINE void clearHotnessOfField() {
		_hotness = Hot;
	}

	/**
	 * clears statistics and all other fields.
	 */
	MMINLINE void clear() {
		_objectPtr = NULL;
		clearHotnessOfField();
		for (uintptr_t i=0; i < SizeScavengerHotness; i++) {
			for (uintptr_t j=0; j < SizeScavengerObjectConnectorType; j++) {
				_connectionCount[i][j] = 0;
				_interObjectDistance[i][j] = 0;
				for (uintptr_t k=0; k < sizeof(uintptr_t)*8; k++) {
					_connectionHistgm[k][i][j] = 0;
				}
			}
		}
	}
	
	MM_ScavengerHotFieldStats() { clear(); }
	
	bool initialize(MM_EnvironmentBase *env);
	
	void tearDown(MM_EnvironmentBase *env) {}
	
	/**
	 * Merges the given hot fields statistics into this one
	 * @param statsToMerge the statistics to merge
	 */
	void mergeStats(MM_ScavengerHotFieldStats* statsToMerge) {
		for (uintptr_t i=0; i < SizeScavengerHotness; i++) {
			for (uintptr_t j=0; j < SizeScavengerObjectConnectorType; j++) {
				_connectionCount[i][j] += statsToMerge->_connectionCount[i][j];
				_interObjectDistance[i][j] += statsToMerge->_interObjectDistance[i][j];
				for (uintptr_t k=0; k < sizeof(uintptr_t)*8; k++) {
					_connectionHistgm[k][i][j] += statsToMerge->_connectionHistgm[k][i][j];
				}
			}
		}
	}

	/**
	 * Determines the hotness of a reference slot from the instanceHotFieldDescription
	 * and record it in the receiver.
	 * @note _objectPtr pointer to current object in heap
	 * @param slotPtr pointer to slot in object
	 * @param hotFieldsDescriptor instance hot fields descriptor
	 */
	void setHotnessOfField(fomrobject_t *slotPtr, uintptr_t hotFieldsDescriptor) {
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		if ((NULL == _objectPtr) || _objectModel->isIndexable(_objectPtr)) {
			/* did not have a parent or cannot calculate hotness here, can only assume it is hot */
			clearHotnessOfField();
		} else {
			/* get the slot index into the hot bit vector */
			fomrobject_t *startOfObjectAfterHeader = (fomrobject_t*)(_objectPtr + 1);
			/* NOTE: pointer math means that slotIndex is calculated in terms of sizeof(fomrobject_t) */
			uintptr_t slotIndex = (uintptr_t) (slotPtr - startOfObjectAfterHeader);
	
			/* x >> y is undefined if y >= #bits in word, but we know it should be zero! This feature of right shift caught me out! */
			if (slotIndex >= sizeof(hotFieldsDescriptor)*8) {
				hotFieldsDescriptor = 0;
			} else {
				hotFieldsDescriptor >>= slotIndex;
			}
			_hotness = (uint8_t)(hotFieldsDescriptor & 1);
		}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
	}

	/**
	 * Update hot statistics for this connection. Assumes that initializeConnection has been
	 * called to initialize the rest of this connection.
	 * If _objectPtr is NULL no statistics are updated.
	 * @note _objectPtr is the parent object
	 * @note _hotness is the hotness of this reference
	 * @param isParentInNewSpace whether parent object is in the nursery
	 * @param isChildInNewSpace whether child object is in the nursery
	 * @param childPtr the child object (usually one being copied)
	 */
	void updateStats(bool isParentInNewSpace, bool isChildInNewSpace, omrobjectptr_t childPtr) {
		if (NULL == _objectPtr) {
			/* don't update statistics - if objectPtr is not set we don't update */
			return;
		}
		ScavengerObjectConnectorType connector;	
		if (!isParentInNewSpace == isChildInNewSpace) {
			/* not both in same space */
			connector = DiffSubSpace;
		} else {
			if (isParentInNewSpace) {
				connector = NewSubSpace;
			} else {
				connector = TenSubSpace;
			}
		}
		
		uintptr_t distance = ABS_UINTPTR_DISTANCE((uintptr_t) _objectPtr, (uintptr_t) childPtr);
		uintptr_t bin = 0;
		if (0 != distance) {
			/* undefined for distance = 0 */
			bin = sizeof(uintptr_t)*8 - MM_Bits::trailingZeroes(distance) - 1;
		}
		_connectionCount[_hotness][connector]++;
#if !defined(OMR_ENV_DATA64)
		_interObjectDistance[_hotness][connector] += distance;
#else
		_interObjectDistance[_hotness][connector] += (double) distance;
#endif
		_connectionHistgm[bin][_hotness][connector]++;
	}

	/**
	 * Reports hot field statistics
	 */
	void reportStats(OMR_VM *omrVM) {
		uintptr_t i;
		OMRPORT_ACCESS_FROM_OMRVM(omrVM);

		omrtty_printf("{ Hot Field Statistics nursery: begin }\n");
		omrtty_printf("{ hotCount                %19lu }\n", _connectionCount[Hot][NewSubSpace]);
#if !defined(OMR_ENV_DATA64)
		omrtty_printf("{ hotInterObjectDistance  %19llu }\n", _interObjectDistance[Hot][NewSubSpace]);
#else
		omrtty_printf("{ hotInterObjectDistance  %19.3g }\n", _interObjectDistance[Hot][NewSubSpace]);
#endif

		omrtty_printf("{ coldCount               %19lu }\n", _connectionCount[Cold][NewSubSpace]);
#if !defined(OMR_ENV_DATA64)
		omrtty_printf("{ coldInterObjectDistance %19llu }\n", _interObjectDistance[Cold][NewSubSpace]);
#else
		omrtty_printf("{ coldInterObjectDistance %19.3g }\n", _interObjectDistance[Cold][NewSubSpace]);
#endif
		omrtty_printf("{ hotHistgm               ");
		for (i=0; i < sizeof(uintptr_t)*8; i++) {
			omrtty_printf(" %9lu", _connectionHistgm[i][Hot][NewSubSpace]);
		}
		omrtty_printf(" }\n");
		omrtty_printf("{ coldHistgm              ");
		for (i=0; i < sizeof(uintptr_t)*8; i++) {
			omrtty_printf(" %9lu", _connectionHistgm[i][Cold][NewSubSpace]);
		}
		omrtty_printf(" }\n");
		omrtty_printf("{ Hot Field Statistics nursery: end }\n");

		omrtty_printf("{ Hot Field Statistics tenured: begin }\n");
		omrtty_printf("{ hotCount                %19lu }\n", _connectionCount[Hot][TenSubSpace]);
#if !defined(OMR_ENV_DATA64)
		omrtty_printf("{ hotInterObjectDistance  %19llu }\n", _interObjectDistance[Hot][TenSubSpace]);
#else
		omrtty_printf("{ hotInterObjectDistance  %19.3g }\n", _interObjectDistance[Hot][TenSubSpace]);
#endif

		omrtty_printf("{ coldCount               %19lu }\n", _connectionCount[Cold][TenSubSpace]);
#if !defined(OMR_ENV_DATA64)
		omrtty_printf("{ coldInterObjectDistance %19llu }\n", _interObjectDistance[Cold][TenSubSpace]);
#else
		omrtty_printf("{ coldInterObjectDistance %19.3g }\n", _interObjectDistance[Cold][TenSubSpace]);
#endif
		omrtty_printf("{ hotHistgm               ");
		for (i=0; i < sizeof(uintptr_t)*8; i++) {
			omrtty_printf(" %9lu", _connectionHistgm[i][Hot][TenSubSpace]);
		}
		omrtty_printf(" }\n");
		omrtty_printf("{ coldHistgm              ");
		for (i=0; i < sizeof(uintptr_t)*8; i++) {
			omrtty_printf(" %9lu", _connectionHistgm[i][Cold][TenSubSpace]);
		}
		omrtty_printf(" }\n");
		omrtty_printf("{ Hot Field Statistics tenured: end }\n");

		omrtty_printf("{ Hot Field Statistics nursery-tenured: begin }\n");
		omrtty_printf("{ hotCount                %19lu }\n", _connectionCount[Hot][DiffSubSpace]);
#if !defined(OMR_ENV_DATA64)
		omrtty_printf("{ hotInterObjectDistance  %19llu }\n", _interObjectDistance[Hot][DiffSubSpace]);
#else
		omrtty_printf("{ hotInterObjectDistance  %19.3g }\n", _interObjectDistance[Hot][DiffSubSpace]);
#endif
		omrtty_printf("{ coldCount               %19lu }\n", _connectionCount[Cold][DiffSubSpace]);
#if !defined(OMR_ENV_DATA64)
		omrtty_printf("{ coldInterObjectDistance %19llu }\n", _interObjectDistance[Cold][DiffSubSpace]);
#else
		omrtty_printf("{ coldInterObjectDistance %19.3g }\n", _interObjectDistance[Cold][DiffSubSpace]);
#endif
		omrtty_printf("{ hotHistgm               ");
		for (i=0; i < sizeof(uintptr_t)*8; i++) {
			omrtty_printf(" %9lu", _connectionHistgm[i][Hot][DiffSubSpace]);
		}
		omrtty_printf(" }\n");
		omrtty_printf("{ coldHistgm              ");
		for (i=0; i < sizeof(uintptr_t)*8; i++) {
			omrtty_printf(" %9lu", _connectionHistgm[i][Cold][DiffSubSpace]);
		}
		omrtty_printf(" }\n");
		omrtty_printf("{ Hot Field Statistics nursery-tenured: end }\n");
	}
};
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
#endif /* SCAVENGERHOTFIELDSTATS_HPP_ */
