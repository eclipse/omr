/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(CONCURRENTPHASESTATSBASE_HPP_)
#define CONCURRENTPHASESTATSBASE_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "Base.hpp"

/**
  * @ingroup GC_Stats
 */
class MM_ConcurrentPhaseStatsBase : public MM_Base
{
	/* Data Members */
private:
protected:
public:
	uintptr_t _cycleID;	/**< The "_id" of the corresponding cycle */
	uintptr_t _scanTargetInBytes;	/**< The number of bytes a given concurrent task was expected to scan before terminating */
	uintptr_t _bytesScanned;	/**< The number of bytes a given concurrent task did scan before it terminated (can be lower than _scanTargetInBytes if the termination was asynchronously requested) */
	bool _terminationWasRequested;	/**< todo: remove after downstream projects start using _terminationRequestType */
	enum TerminationRequestType {
		terminationRequest_None,
		terminationRequest_ByGC,
		terminationRequest_External
	};
	TerminationRequestType _terminationRequestType; /**< Reason for concurrent task termination, asynchronous external event or itself GC (work or survivor space exhausted etc). */

	/* Member Functions */
private:
protected:
public:

	bool isTerminationRequested() {
		return terminationRequest_None != _terminationRequestType;
	}
	bool isTerminationRequestExternal() {
		return terminationRequest_External == _terminationRequestType;
	}
	
	void clear() {
		_cycleID = 0;
		_scanTargetInBytes = 0;
		_bytesScanned = 0;
		_terminationWasRequested = false;
		_terminationRequestType = terminationRequest_None;
	}
	 
	MM_ConcurrentPhaseStatsBase()
		: MM_Base()
		, _cycleID(0)
		, _scanTargetInBytes(0)
		, _bytesScanned(0)
		, _terminationWasRequested(false)
		, _terminationRequestType(terminationRequest_None)
	{}
}; 

#endif /* CONCURRENTPHASESTATSBASE_HPP_ */
