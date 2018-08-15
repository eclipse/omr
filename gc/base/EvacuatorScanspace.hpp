/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef EVACUATORSCANSPACE_HPP_
#define EVACUATORSCANSPACE_HPP_

#include "Base.hpp"
#include "EvacuatorCopyspace.hpp"
#include "EvacuatorWhitelist.hpp"
#include "IndexableObjectScanner.hpp"
#include "ObjectScanner.hpp"
#include "ObjectScannerState.hpp"

/**
 * Extends copyspace to allow scanning.
 */
class MM_EvacuatorScanspace : public MM_EvacuatorCopyspace
{
/*
 * Data members
 */
private:
	GC_ObjectScannerState _objectScannerState;	/* space reserved for instantiation of object scanner for current object */
	GC_ObjectScanner *_objectScanner;			/* points to _objectScannerState after scanner is instantiated, NULL if object scanner not instantiated */
	uint8_t *_scan;								/* scan head points to object being scanned */
	uint8_t *_limit;							/* can copy object of size S in current stack frame as long as _copy<_limit and S<=(_end-_copy) */

protected:
	/* enumeration of flag bits may be extended past scanEndFlag by subclasses */
	typedef enum scanspaceFlags {
		isRememberedFlag = copyEndFlag << 1		/* remembered state of object at scan head */
		, isSplitArrayFlag = copyEndFlag << 2	/* indicates that scanspace contains a slit array segment when set */
		, scanEndFlag = isSplitArrayFlag		/* marks end of scanspace flags */
	} scanspaceFlags;

public:
/*
 * Function members
 */
private:

protected:

public:
	/**
	 * Basic array constructor obviates need for stdlibc++ linkage in gc component libraries. Array
	 * is allocated from forge as contiguous block sized to contain requested number of elements and
	 * must be freed using MM_Forge::free() when no longer needed. See MM_Evacuator::tearDown().
	 *
	 * @param count the number of aray elements to instantiate
	 * @return a pointer to instantiated array
	 */
	static MM_EvacuatorScanspace *
	newInstanceArray(MM_Forge *forge, uintptr_t count)
	{
		MM_EvacuatorScanspace *scanspace = (MM_EvacuatorScanspace *)forge->allocate(sizeof(MM_EvacuatorScanspace) * count, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
		if (NULL != scanspace) {
			for (uintptr_t i = 0; i < count; i += 1) {
				MM_EvacuatorScanspace *space = new(scanspace + i) MM_EvacuatorScanspace();
				if (NULL == space) {
					return NULL;
				}
			}
		}
		return scanspace;
	}

	/*
	 * Current or latent object scanner is instantiated in space reserved within containing scanspace.
	 *
	 * @see MM_EvacuatorDelegate::getObjectScanner()
	 */
	MMINLINE GC_ObjectScannerState *getObjectScannerState() { return &_objectScannerState; }

	/**
	 *  Set pointer to instantiated object scanner. Non-null inputs will be overridden with scanspace
	 *  own object scanner state.
	 */
	MMINLINE void 
	setObjectScanner(GC_ObjectScanner *objectScanner)
	{
		_objectScanner = (NULL != objectScanner) ? (GC_ObjectScanner *)getObjectScannerState() : NULL;
	}

	/**
	 * Return pointer to instantiated object scanner, or NULL if object scanner not instantiated.
	 */
	MMINLINE GC_ObjectScanner *getObjectScanner() { return _objectScanner; }

	/**
	 * Return the position of the scan head
	 */
	MMINLINE uint8_t *getScanHead() { return _scan; }

	/**
	 * Return the position of the copy limit. No more objects can be laid down once the limit
	 * has been reached or passed.
	 */
	MMINLINE uint8_t *getCopyLimit() { return _limit; }

	/**
	 * Return the number of bytes of remaining whitespace between copy head and limit
	 */
	MMINLINE uintptr_t getLimitedSize() { return (_limit > _copy) ? (uintptr_t)(_limit - _copy) : 0; }

	/**
	 * Return the number of bytes remaining to be scanned
	 */
	MMINLINE uintptr_t getUnscannedSize() { return (uintptr_t)(_copy - _scan); }

	/**
	 * Clear the remembered state (before starting to scan a new object)
	 */
	MMINLINE void clearRememberedState() { _flags &= ~(uintptr_t)isRememberedFlag; }

	/**
	 * Get the remembered state of the current object
	 */
	MMINLINE bool getRememberedState() { return isRememberedFlag == (_flags & (uintptr_t)isRememberedFlag); }

	/**
	 * Set the remembered state of the current object if it is tenured and has a referent in new space
	 *
	 * @param referentInSurvivor set this to true if the referent is in new space
	 */
	MMINLINE void 
	updateRememberedState(bool referentInSurvivor)
	{
		if (referentInSurvivor) {
			_flags |= (uintptr_t)isRememberedFlag;
		}
	}

	/**
	 * Test whether scanspace contains a split array segment
	 */
	MMINLINE bool isSplitArraySegment() { return isSplitArrayFlag == (_flags & (uintptr_t)isSplitArrayFlag); }

	/**
	 * Load whitespace or unscanned work to scan into this scanspace. If there is work it
	 * may be followed by additional whitespace. In either case the copy limit will be set at the next
	 * page boundary or truncated at the copy head.
	 *
	 * @param base points to start of unscanned work
	 * @param copy points to whitespace at copy head
	 * @param length extent in bytes of unscanned work at base
	 */
	MMINLINE void
	setScanspace(uint8_t *base, uint8_t *copy, uintptr_t length, bool isLOA = false)
	{
		setCopyspace(base, copy, length, isLOA);
		_scan = _base;
		if (NULL != _scan) {
			/* set the copy limit at next page boundary */
			_limit = (uint8_t *)((uintptr_t)(_scan + MM_EvacuatorBase::inside_copy_size + MM_EvacuatorBase::max_inside_object_size) & ~MM_EvacuatorBase::inside_copy_mask);
			/* adjust limit to copy/end bounds */
			if (_limit < _copy) {
				_limit = _copy;
			} else if (_limit > _end) {
				_limit = _end;
			}
		} else {
			_limit = NULL;
		}
		Debug_MM_true((_copy <= _limit) && (_limit <= _end));

		clearRememberedState();
		_objectScanner = NULL;
	}

	MMINLINE void
	pullWhitespace(MM_EvacuatorScanspace *fromspace)
	{
		/* pull whitespace at end of fromspace into this scanspace */
		setScanspace(fromspace->_copy, fromspace->_copy, fromspace->getWhiteSize(), fromspace->isLOA());

		/* trim tail of fromspace and leave base, scan and flags as they are */
		fromspace->_end = fromspace->_limit = fromspace->_copy;
	}

	/**
	 * Load a split array segment into this scanspace.
	 *
	 * @param base points to indexable object containing the segment
	 * @param base points after end of contiguous indexable object containing the segment
	 * @param scanner the object scanner preset to scan the array segment
	 */
	MMINLINE void
	setSplitArrayScanspace(uint8_t *base, uint8_t *end, GC_IndexableObjectScanner *scanner)
	{
		setCopyspace(base, end, (uintptr_t)(end - base));
		_scan = _end;
		_limit = _end;
		_flags |= isSplitArrayFlag;
		
		Debug_MM_true((base == _base) && (_limit == _scan) && (_scan == _copy) &&(_copy == _end) && (_end == end));

		clearRememberedState();
		_objectScanner = scanner;
	}

	/**
	 * Advance the scan pointer to next unscanned object.
	 *
	 * @param scannedBytes number of bytes scanned (size of scanned object)
	 */
	MMINLINE void
	advanceScanHead(uintptr_t scannedBytes)
	{
		/* split array segment or current object, if any, has been completely scanned */
		if (_scan < _copy) {
			/* scan head for split arrays is preset to copy head so this is necessary only for scalars and non-split arrays */
			_scan += scannedBytes;
		}
		/* done with current object scanner */
		_objectScanner = NULL;
	}

	/**
	 * This overloads copyspace trim() to set scanspace limit at copy head.
	 */
	MMINLINE MM_EvacuatorWhitespace *
	clip()
	{
		MM_EvacuatorWhitespace *whitespace = trim();
		if (_limit > _copy) {
			_limit = _copy;
		}
		return whitespace;
	}

	/**
	 * Constructor
	 */
	MM_EvacuatorScanspace()
		: MM_EvacuatorCopyspace()
		, _objectScanner(NULL)
		, _scan(NULL)
		, _limit(NULL)
	{ }
};

#endif /* EVACUATORSCANSPACE_HPP_ */
