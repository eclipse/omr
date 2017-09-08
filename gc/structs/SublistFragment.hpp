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

/** 
 * @file
 * @ingroup GC_Structs
 */

#if !defined(SUBLISTFRAGMENT_HPP_)
#define SUBLISTFRAGMENT_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "j9nongenerated.h"

#include "SublistPool.hpp"

class MM_EnvironmentBase;

/**
 * A reserved block of memory which is part of an MM_SublistPool.
 * @see MM_SublistPool
 * @see MM_SublistPuddle
 * @ingroup GC_Structs
 */
class MM_SublistFragment
{
	J9VMGC_SublistFragment *_fragment;

public:
	void *allocate(MM_EnvironmentBase *env);
	bool add(MM_EnvironmentBase *env, uintptr_t entry);
	
	/** 
	 * Update the fragment information to the new location 
	 */
	MMINLINE void update(uintptr_t *base, uintptr_t *top)
	{
		_fragment->fragmentCurrent = base;
		_fragment->fragmentTop = top;
	}

	/** 
	 * Return the allocated size of the fragment 
	 */
	MMINLINE uintptr_t getFragmentSize()
	{
		return _fragment->fragmentSize;
	}

	/**
	 * Clear the remaining entries in the fragment.
	 * Disconnects the fragment from the reserved area in the sublist.  New allocates will
	 * require a refresh of the fragment.  Allows the sublist to be manipulated by other routines
	 * (no cached pointers).
	 */
	MMINLINE static void flush(J9VMGC_SublistFragment *sublistFragment)
	{
		/* Pool count needs to be incremented */
		((MM_SublistPool *)sublistFragment->parentList)->incrementCount(sublistFragment->count);
		sublistFragment->count = 0;
		
		sublistFragment->fragmentCurrent = NULL;
		sublistFragment->fragmentTop = NULL;
	}

	/**
	 * Create a SublistFragment object.
	 */
	MM_SublistFragment(J9VMGC_SublistFragment *fragment) :
		_fragment(fragment)
	{};
};

#endif /* SUBLISTFRAGMENT_HPP_ */
