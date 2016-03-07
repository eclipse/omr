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

