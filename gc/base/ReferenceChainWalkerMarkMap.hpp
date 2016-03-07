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


#if !defined(REFERENCECHAINWALKERMARKMAP_HPP_)
#define REFERENCECHAINWALKERMARKMAP_HPP_

#include "omrcomp.h"

#include "HeapMap.hpp"

class MM_EnvironmentBase;

class MM_ReferenceChainWalkerMarkMap : public MM_HeapMap
{
private:
	/**
	 * Clear Mark Map for Reference Chain Walker:
	 * commit memory if necessary and zero it
	 * @param env current thread environment
	 * @param commit if true memory should be commited first
	 * @return true if initialization is successful
	 */
	bool clearMapForRegions(MM_EnvironmentBase *env, bool commit);

protected:
/**
 * Initialize Mark Map for Reference Chain Walker:
 * create mark map correspondent with each region: commit memory and zero it
 * @param env current thread environment
 * @return true if initialization is successful
 */
bool initialize(MM_EnvironmentBase *env);

public:
	/**
	 * Create a new instance of Reference Chain Walker Mark Map object
	 */
 	static MM_ReferenceChainWalkerMarkMap *newInstance(MM_EnvironmentBase *env, uintptr_t maxHeapSize);

 	/**
 	 * Clear Mark Map
 	 */
 	void clearMap(MM_EnvironmentBase *env);

	/**
	 * Create a MarkMap for Reference Chain Walker object.
	 */
 	MM_ReferenceChainWalkerMarkMap(MM_EnvironmentBase *env, uintptr_t maxHeapSize) :
		MM_HeapMap(env, maxHeapSize, env->getExtensions()->isMetronomeGC())
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* REFERENCECHAINWALKERMARKMAP_HPP_ */
