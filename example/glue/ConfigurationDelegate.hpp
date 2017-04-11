/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017
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

#ifndef CONFIGURATIONDELEGATE_HPP_
#define CONFIGURATIONDELEGATE_HPP_

#include "omrgcconsts.h"
#include "sizeclasses.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"

/**
 * Client language delegate for GC configuration must be implemented by all GC clients.
 */
class MM_ConfigurationDelegate
{
/*
 * Member data and types
 */
private:
	const MM_GCPolicy _gcPolicy;

protected:
public:

/*
 * Member functions
 */
private:
protected:

public:
	/**
	 * Initialize the delegate.
	 * @param env The calling thread environment
	 * @param writeBarrierType The write barrier type selected for the GC configuration
	 * @param allocationType The allocation type selected for the GC configuration
	 * @return false if initialization fails
	 */
	bool
	initialize(MM_EnvironmentBase* env, MM_GCWriteBarrierType writeBarrierType, MM_GCAllocationType allocationType)
	{
		return true;
	}

	/**
	 * Tear down the delegate, releasing any resources held.
	 * @param env The calling thread environment
	 */
	void
	tearDown(MM_EnvironmentBase* env)
	{
		return;
	}

	/**
	 * This method is called just after the heap has been initialized. Delegate may perform
	 * any additional heap-related initialization at this point.
	 * @param env The calling thread environment
	 * @return false if heap initialization fails
	 */
	bool
	heapInitialized(MM_EnvironmentBase* env)
	{
		return true;
	}

	/**
	 * The OMR GC preallocates a pool of MM_EnvironmentBase subclass instances on startup. This method is called
	 * to allow GC clients to determine the number of pooled instances to preallocate.
	 * @param env The calling thread environment
	 * @return The number of pooled instances to preallocate, or 0 to use default number
	 */
	uint32_t
	getInitialNumberOfPooledEnvironments(MM_EnvironmentBase* env)
	{
		return 0;
	}

	/**
	 * The OMR segregated GC requires GC clients to specify a distribution of segment sizes with which
	 * it will partition the heap. This method is called to retrieve the size distribution metadata. It
	 * is required only if the segregated heap is enabled.
	 * @param env The calling thread environment
	 * @return A pointer to segregated heap size distribution metadata, or NULL if not using segregated heap
	 */
#if defined(OMR_GC_SEGREGATED_HEAP)
	OMR_SizeClasses *getSegregatedSizeClasses(MM_EnvironmentBase *env)
	{
		return NULL;
	}
#endif /* defined(OMR_GC_SEGREGATED_HEAP) */

	/**
	 * Perform GC client-specific initialization for a new MM_EnvironmentBase subclass instance.
	 * @param env The thread environment to initialize
	 * @return false if environment initialization fails
	 */
	bool
	environmentInitialized(MM_EnvironmentBase* env)
	{
		return true;
	}

	/**
	 * The OMR GC preallocates a pool of collection helper threads. This method is called to allow GC
	 * clients to determine the maximum number of collection helper threads to run during GC cycles.
	 * @param env The calling thread environment
	 * @return The maximum number of GC threads to allocate for the runtime GC
	 */
	uintptr_t getMaxGCThreadCount(MM_EnvironmentBase* env)
	{
		return 1;
	}

	/**
	 * Return the GC policy preselected for the GC configuration.
	 */
	MM_GCPolicy getGCPolicy() { return _gcPolicy; }

	/**
	 * Constructor.
	 * @param gcPolicy The GC policy preselected for the GC configuration.
	 */
	MM_ConfigurationDelegate(MM_GCPolicy gcPolicy)
		: _gcPolicy(gcPolicy)
	{}
};

#endif /* CONFIGURATIONDELEGATE_HPP_ */
