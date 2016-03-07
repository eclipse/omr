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


#if !defined(SWEEPSCHEMESECTIONING_HPP_)
#define SWEEPSCHEMESECTIONING_HPP_

#include "BaseVirtual.hpp"

#include "GCExtensionsBase.hpp"
#include "EnvironmentBase.hpp"

class MM_ParallelSweepChunk;
class MM_ParallelSweepChunkArray;

/**
 * Support for sectioning the heap into chunks useable by sweep (and compact).
 * 
 * @ingroup GC_Base_Core
 */
class MM_SweepHeapSectioning : public MM_BaseVirtual {
private:
protected:
	MM_ParallelSweepChunkArray* _head; /**< head pointer to a list of Chunk arrays */
	uintptr_t _totalUsed; /**< total elements used across all arrays */
	uintptr_t _totalSize; /**< total elements available across all arrays */

	MM_ParallelSweepChunkArray* _baseArray; /**< pointer to the base array allocated at initialization */
	MM_GCExtensionsBase* _extensions;

	virtual bool initialize(MM_EnvironmentBase* env);
	void tearDown(MM_EnvironmentBase* env);

	virtual uintptr_t estimateTotalChunkCount(MM_EnvironmentBase* env) = 0;
	virtual uintptr_t calculateActualChunkNumbers() const = 0;

	bool initArrays(uintptr_t);

	friend class MM_SweepHeapSectioningIterator;

public:
	virtual void kill(MM_EnvironmentBase* env);

	bool update(MM_EnvironmentBase* env);
	virtual uintptr_t reassignChunks(MM_EnvironmentBase* env) = 0;

	void* getBackingStoreAddress();
	uintptr_t getBackingStoreSize();

	MM_SweepHeapSectioning(MM_EnvironmentBase* env)
		: _head(NULL)
		, _totalUsed(0)
		, _totalSize(0)
		, _baseArray(NULL)
		, _extensions(env->getExtensions())
	{
		_typeId = __FUNCTION__;
	}
};

/**
 * Iterator to traverse all chunks in the heap built by heap sectioning.
 * 
 * @ingroup GC_Base
 */
class MM_SweepHeapSectioningIterator {
private:
	MM_ParallelSweepChunkArray* _currentArray; /**< Current chunk array being traversed */
	uint32_t _currentIndex; /**< Current index of chunk within the current array being traversed */

public:
	MM_SweepHeapSectioningIterator(const MM_SweepHeapSectioning* sweepHeapSectioning)
		: _currentArray(sweepHeapSectioning->_head)
		, _currentIndex(0)
	{
	}

	MM_SweepHeapSectioningIterator()
		: _currentArray(NULL)
		, _currentIndex(0)
	{
	}

	/**
	 * Reinitialize the heap iterator with the given heap sectioning.
	 */
	void restart(MM_SweepHeapSectioning* sweepHeapSectioning)
	{
		_currentArray = sweepHeapSectioning->_head;
		_currentIndex = 0;
	}

	MM_ParallelSweepChunk* nextChunk();
};

#endif /* SWEEPSCHEMESECTIONING_HPP_ */
