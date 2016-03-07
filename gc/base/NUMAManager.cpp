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
 * @ingroup GC_Base_Core
 */

#include <string.h>

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "ModronAssertions.h"
#include "NUMAManager.hpp"

void
MM_NUMAManager::shouldEnablePhysicalNUMA(bool numaEnabled)
{
	_physicalNumaEnabled = numaEnabled;
	_simulatedNodeCount = 0;
}

/**
 * Helper function used by J9_SORT to sort affinity leaders array.
 */
static int
compareNodeNumberFunc(const void *element1, const void *element2)
{
	J9MemoryNodeDetail *node1 = (J9MemoryNodeDetail *)element1;
	J9MemoryNodeDetail *node2 = (J9MemoryNodeDetail *)element2;

	if(node1->j9NodeNumber == node2->j9NodeNumber) {
		return 0;
	} else if(node1->j9NodeNumber > node2->j9NodeNumber) {
		return 1;
	} else {
		return -1;
	}
}

bool
MM_NUMAManager::recacheNUMASupport(MM_EnvironmentBase *env)
{
	bool result = true;
	if (NULL != _activeNodes) {
		env->getForge()->free(_activeNodes);
		_activeNodes = NULL;
		_activeNodeCount = 0;
	}
	if (NULL != _affinityLeaders) {
		env->getForge()->free(_affinityLeaders);
		_affinityLeaders = NULL;
		_affinityLeaderCount = 0;
	}
	if (NULL != _freeProcessorPoolNodes) {
		env->getForge()->free(_freeProcessorPoolNodes);
		_freeProcessorPoolNodes = NULL;
		_freeProcessorPoolNodeCount = 0;
	}
	_maximumNodeNumber = 0;

	uintptr_t nodeCount = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	if (_physicalNumaEnabled) {
		intptr_t detailResult = omrvmem_numa_get_node_details(NULL, &nodeCount);
		if (0 != detailResult) {
			/* something went wrong in the underlying call so ignore any NUMA count data we might have received */
			nodeCount = 0;
		}
	} else {
		nodeCount = _simulatedNodeCount;
	}
	if (0 != nodeCount) {
		/* we want to support NUMA either via the machine's physical NUMA or our simulated (aka "purely logical") NUMA */ 
		uintptr_t nodeArraySize = sizeof(J9MemoryNodeDetail) * nodeCount;
		_activeNodes = (J9MemoryNodeDetail *)env->getForge()->allocate(nodeArraySize, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
		if (NULL == _activeNodes) {
			result = false;
		} else {
			memset(_activeNodes, 0x0, nodeArraySize);
			_activeNodeCount = nodeCount;
			if (_physicalNumaEnabled) {
				/* we want REAL NUMA so ask the underlying system for node details */
				intptr_t detailResult = omrvmem_numa_get_node_details(_activeNodes, &_activeNodeCount);
				Assert_MM_true(0 == detailResult);
				Assert_MM_true(_activeNodeCount == nodeCount);
			} else {
				/* we want to use "fake" NUMA so synthesize the array data */
				for (uintptr_t i = 0; i < nodeCount; i++) {
					_activeNodes[i].j9NodeNumber = i + 1;
					_activeNodes[i].memoryPolicy = J9NUMA_PREFERRED;
					/* NOTE:  if we ever start counting these resources to determine GC thread counts, this will have to be something other than "1" */
					_activeNodes[i].computationalResourcesAvailable = 1;
				}
			}
			
			/* Sorting the array in j9NodeNumber ascending order.
			 * This sorting is not really required, but gives us ability to make some stronger assertions later in the code 
			 */
			J9_SORT(_activeNodes, _activeNodeCount, sizeof(J9MemoryNodeDetail), compareNodeNumberFunc);

			/* now that we have the total list of active nodes, identify the affinity leaders */
			uintptr_t preferredWithCPU = 0;
			uintptr_t allowedWithCPU = 0;
			/* if "preferred" nodes exist, we will use them as the leaders and fall back to "allowed" if there aren't any so count both and choose them after the loop */
			for (uintptr_t activeNodeIndex = 0; activeNodeIndex < _activeNodeCount; activeNodeIndex++) {
				uintptr_t cpu = _activeNodes[activeNodeIndex].computationalResourcesAvailable;
				if (0 != cpu) {
					J9MemoryState policy = _activeNodes[activeNodeIndex].memoryPolicy;
					if (J9NUMA_PREFERRED == policy) {
						preferredWithCPU += 1;
					} else if (J9NUMA_ALLOWED == policy) {
						allowedWithCPU += 1;
					} else {
						/* nodes with CPUs but DENIED bindings go into the free processor pool */
						_freeProcessorPoolNodeCount += 1;
					}
				}
				/* since we are walking all the nodes, this is also the time to update the maximum node number */
				_maximumNodeNumber = OMR_MAX(_maximumNodeNumber, _activeNodes[activeNodeIndex].j9NodeNumber);
			}
			J9MemoryState policyType = J9NUMA_PREFERRED;
			_affinityLeaderCount = preferredWithCPU;
			if (0 == _affinityLeaderCount) {
				_affinityLeaderCount = allowedWithCPU;
				policyType = J9NUMA_ALLOWED;
			}
			
			/* Affinity Leader array allocation and construction */
			if (0 != _affinityLeaderCount) {
				uintptr_t affinityLeaderArraySize = sizeof(J9MemoryNodeDetail) * _affinityLeaderCount;
				_affinityLeaders = (J9MemoryNodeDetail *)env->getForge()->allocate(affinityLeaderArraySize, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
				if (NULL == _affinityLeaders) {
					result = false;
				} else {
					memset(_affinityLeaders, 0x0, affinityLeaderArraySize);
					uintptr_t nextIndex = 0;
					for (uintptr_t activeNodeIndex = 0; activeNodeIndex < _activeNodeCount; activeNodeIndex++) {
						if ((0 != _activeNodes[activeNodeIndex].computationalResourcesAvailable) && (policyType == _activeNodes[activeNodeIndex].memoryPolicy)) {
							Assert_MM_true(nextIndex < _affinityLeaderCount);
							_affinityLeaders[nextIndex] = _activeNodes[activeNodeIndex];
							nextIndex += 1;
						}
					}
					Assert_MM_true(nextIndex == _affinityLeaderCount);
				}
			}
			
			/* Free Processor Pool array allocation and construction */
			if (0 != _freeProcessorPoolNodeCount) {
				/* allocate a free processor pool */
				uintptr_t processorPoolArraySize = sizeof(J9MemoryNodeDetail) * _freeProcessorPoolNodeCount;
				_freeProcessorPoolNodes = (J9MemoryNodeDetail *)env->getForge()->allocate(processorPoolArraySize, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
				if (NULL == _freeProcessorPoolNodes) {
					result = false;
				} else {
					memset(_freeProcessorPoolNodes, 0x0, processorPoolArraySize);
					uintptr_t nextIndex = 0;
					for (uintptr_t activeNodeIndex = 0; activeNodeIndex < _activeNodeCount; activeNodeIndex++) {
						if ((0 != _activeNodes[activeNodeIndex].computationalResourcesAvailable) && (J9NUMA_DENIED == _activeNodes[activeNodeIndex].memoryPolicy)) {
							Assert_MM_true(nextIndex < _freeProcessorPoolNodeCount);
							_freeProcessorPoolNodes[nextIndex] = _activeNodes[activeNodeIndex];
							nextIndex += 1;
						}
					}
					Assert_MM_true(nextIndex == _freeProcessorPoolNodeCount);
				}
			}
		}
	}

	return result;
}

void
MM_NUMAManager::setSimulatedNodeCountForFVTest(uintptr_t simulatedNodeCount)
{
	_simulatedNodeCount = simulatedNodeCount;
	_physicalNumaEnabled = false;
}

uintptr_t
MM_NUMAManager::getAffinityLeaderCount() const
{
	return _affinityLeaderCount;
}

uintptr_t
MM_NUMAManager::getMaximumNodeNumber() const
{
	return _maximumNodeNumber;
}

J9MemoryNodeDetail const*
MM_NUMAManager::getAffinityLeaders(uintptr_t *arrayLength) const
{
	*arrayLength = _affinityLeaderCount;
	return _affinityLeaders;
}

J9MemoryNodeDetail const*
MM_NUMAManager::getFreeProcessorPool(uintptr_t *arrayLength) const
{
	*arrayLength = _freeProcessorPoolNodeCount;
	return _freeProcessorPoolNodes;
}

uintptr_t
MM_NUMAManager::getComputationalResourcesAvailableForAllNodes() const
{
	uintptr_t computationalResourceCount = 0;

	for (uintptr_t activeNodeIndex = 0; activeNodeIndex < _activeNodeCount; activeNodeIndex++) {
		computationalResourceCount += _activeNodes[activeNodeIndex].computationalResourcesAvailable;
	}

	return computationalResourceCount;
}

bool
MM_NUMAManager::isPhysicalNUMASupported() const
{
	return (0 != _physicalNumaEnabled);
}

void
MM_NUMAManager::shutdownNUMASupport(MM_EnvironmentBase *env)
{
	_physicalNumaEnabled = false;
	_simulatedNodeCount = 0;
	/* just call recache since it knows how to clean up when NUMA has been disabled */
	recacheNUMASupport(env);
}
