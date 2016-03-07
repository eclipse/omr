/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#if !defined(REFERENCESTATS_HPP_)
#define REFERENCESTATS_HPP_

class MM_ReferenceStats {
public:
	uintptr_t _candidates; /**< reference objects that are candidates to be transitioned and possibly enqueued */
	uintptr_t _cleared; /**< reference objects that are being transitioned into the cleared state */
	uintptr_t _enqueued; /**< reference objects that are being enqueued onto their associated reference queue */

public:
	/**
	 * Clear the receivers statistics to an initial state.
	 */
	void clear()
	{
		_candidates = 0;
		_cleared = 0;
		_enqueued = 0;
	}

	/**
	 * Merge the given stats structure values into the receiver.
	 * @note This method is NOT thread safe.
	 *
	 */
	void merge(MM_ReferenceStats* stats)
	{
		_candidates += stats->_candidates;
		_cleared += stats->_cleared;
		_enqueued += stats->_enqueued;
	}

	MM_ReferenceStats()
		: _candidates(0)
		, _cleared(0)
		, _enqueued(0) {};
};

#endif /* REFERENCESTATS_HPP_ */
