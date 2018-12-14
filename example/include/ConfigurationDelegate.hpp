/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef CONFIGURATIONDELEGATE_HPP_
#define CONFIGURATIONDELEGATE_HPP_

#include "omrgcconsts.h"
#include "sizeclasses.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"

class MM_HeapRegionDescriptor;

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
	 * For standard gc policies, an opaque (to OMR) object may be attached to each heap region to maintain language-specific
	 * per region metadata.
	 *
	 * If the delegate allocates a region extension object in this method, it must set region->_heapRegionDescriptorExtension
	 * to point to the object.
	 *
	 * @param env The calling thread environment
	 * @param region the region to attach the region extension to
	 * @return true unless a required region extension could not be initialized
	 */
	bool initializeHeapRegionDescriptorExtension(MM_EnvironmentBase* env, MM_HeapRegionDescriptor *region) { return true; }

	/**
	 * For standard gc policies, an opaque (to OMR) object may be attached to each heap region to maintain language-specific
	 * per region metadata.
	 *
	 * If the delegate allocated a region extension object in region->_heapRegionDescriptorExtension, it must be deallocated
	 * in this method and region->_heapRegionDescriptorExtension should be cleared (NULL).
	 *
	 * @param env The calling thread environment
	 * @param region the region to deallocate the region extension from
	 */
	void teardownHeapRegionDescriptorExtension(MM_EnvironmentBase* env, MM_HeapRegionDescriptor *region) {}

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
	 * This method is called to determine when to start tracking heap fragmentation, which is initially inhibited to
	 * allow the heap to grow to a stable operational size. Frequent transitions true->false will limit the quality
	 * of heap fragmentation stats.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @return true to allow heap fragmentation tracking to start or continue
	 */
	bool canCollectFragmentationStats(MM_EnvironmentBase *env) { return false; }

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
