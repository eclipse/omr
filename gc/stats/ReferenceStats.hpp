/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
