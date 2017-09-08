/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#if !defined(GCCODE_HPP_)
#define GCCODE_HPP_

#include "omrcomp.h"
#include "j9nongenerated.h"

class MM_GCCode {
/* member data */
private:
	uint32_t _gcCode; /**< the code described by this object. Must be one of the J9MMCONSTANT_* values from j9.h */

protected:
public:

/* member functions */
private:
protected:
public:
	MM_GCCode(uint32_t gcCode) :
		_gcCode(gcCode)
		{ }
	
	uint32_t getCode() const { return _gcCode; } 
	
	/**
	 * Determine if the GC is implicit or explicit (i.e. triggered externally).
	 * @return true if the gc code indicates an explicit GC
	 */
	bool isExplicitGC() const;
	
	/**
	 * Determine if the GC should aggressively try to recover native memory.
	 * @return true if native memory should be recovered aggressively
	 */
	bool shouldAggressivelyCompact() const;
	
	/**
	 * Determine if the GC should be aggressive.
	 * @return true if the gc code indicates an aggressive GC
	 */
	bool isAggressiveGC() const;
	
	/**
	 * Determine if it is a percolate GC call.
	 * @return true if it is a percolate call
	 */
	bool isPercolateGC() const;
	
	/**
	 * Determine if it is a GC request from a RAS dump agent.
	 * @return true if it is a RAS dump call
	 */	
	bool isRASDumpGC() const;
	
	/**
	 * Determine if the GC is going to throw OOM if enough memory is not collected.
	 * @return true if OOM can be thrown at the end of this GC
	 */
	bool isOutOfMemoryGC() const;
};

#endif /* GCCODE_HPP_ */
