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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(COPYSCANCACHECHUNK_HPP_)
#define COPYSCANCACHECHUNK_HPP_

#include "BaseVirtual.hpp"
#include "EnvironmentStandard.hpp" 

class MM_CopyScanCacheStandard;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_CopyScanCacheChunk : public MM_BaseVirtual
{
private:

protected:
	MM_CopyScanCacheStandard *_baseCache;		/**< pointer to base cache in chunk */
	MM_CopyScanCacheChunk *_nextChunk;	/**< pointer to next chunk in list */

public:

private:

protected:
	void tearDown(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env, uintptr_t cacheEntryCount, MM_CopyScanCacheChunk *nextChunk, uintptr_t flags, MM_CopyScanCacheStandard **sublistTail);

public:
	MMINLINE MM_CopyScanCacheStandard *getBase() const { return _baseCache; }
	MMINLINE MM_CopyScanCacheChunk *getNext() const { return _nextChunk; }
	MMINLINE void setNext(MM_CopyScanCacheChunk *nextChunk)  { _nextChunk = nextChunk; }
	
	static MM_CopyScanCacheChunk *newInstance(MM_EnvironmentBase *env, uintptr_t cacheEntryCount, MM_CopyScanCacheChunk *nextChunk, MM_CopyScanCacheStandard **sublistTail);
	virtual void kill(MM_EnvironmentBase *env);

	/**
	 * Create a CopyScanCacheChunk object.
	 */
	MM_CopyScanCacheChunk() :
		MM_BaseVirtual(),
		_baseCache(NULL),
		_nextChunk(NULL)
	{
		_typeId = __FUNCTION__;
	};
	
};

#endif /* COPYSCANCACHECHUNK_HPP_ */

