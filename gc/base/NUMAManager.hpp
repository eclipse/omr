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

#if !defined(NUMAMANAGER_HPP_)
#define NUMAMANAGER_HPP_

#include "omrcomp.h"
#include "omrport.h"

class MM_EnvironmentBase;
struct J9MemoryNodeDetail;

class MM_NUMAManager
{
	/* Data Members */
public:
protected:
private:
	bool _physicalNumaEnabled;	/**< True if physical (as opposed to logical-only as is the case of simulated NUMA) NUMA support is enabled in the receiver */
	uintptr_t _simulatedNodeCount;	/**< The number of nodes being logically simulated by an fvtest option (changes affinity leaders, not physical nodes, and only honoured if NUMA is disabled) */
	uintptr_t _maximumNodeNumber;	/**< The highest j9NodeNumber of all elements in the _activeNodes array or 0 if NUMA isn't available or being simulated */
	J9MemoryNodeDetail *_activeNodes;	/**< The array of node details for the active (having either usable memory or CPUs) NUMA nodes on the system (might be simulated) */
	uintptr_t _activeNodeCount;	/**< The number of nodes available (used for both affinity leaders and physical nodes - only non-zero if NUMA is enabled or being simulated) */
	J9MemoryNodeDetail *_affinityLeaders;	/**< A subset of the nodes in _activeNodes which represent the nodes which are offering both physical memory and CPU */
	uintptr_t _affinityLeaderCount;	/**< The number of elements in the _affinityLeaders array */
	J9MemoryNodeDetail *_freeProcessorPoolNodes;	/**< A subset of the nodes in _activeNodes which represent the nodes which are offering ONLY CPU */
	uintptr_t _freeProcessorPoolNodeCount;	/**< The number of elements in the _freeProcessorPoolNodes array */

	/* Member Functions */
private:
protected:
public:
	/**
	 * Sets whether or not NUMA should be enabled (note that recacheNUMASupport needs to be called after this to change internal NUMA caches).
	 * If this is explicitly set to true, it implicitly disables any "simulated NUMA"
	 * @param numaEnabled[in] True if we should honour the NUMA state of the machine.
	 */
	void shouldEnablePhysicalNUMA(bool numaEnabled);

	bool isPhysicalNUMAEnabled() const { return _physicalNumaEnabled; }

	/**
	 * Get low-level NUMA node number from logical node ID.
	 * @param numaNodeID starting from 1
	 * If NUMA is explicitly disabled, or not available return 0.
	 */
	uintptr_t getJ9NodeNumber(uintptr_t numaNodeID) {
		uintptr_t j9NodeNumber = 0;

		if (_physicalNumaEnabled && (numaNodeID > 0)) {
			j9NodeNumber = _affinityLeaders[numaNodeID - 1].j9NodeNumber;
		}

		return j9NodeNumber;
	}

	/**
	 * Called to update internal NUMA caches (could be due to a change in the machine's NUMA state or a change in whether or not we want to enable NUMA (either real or simulated))
	 * @param env[in] The master GC thread
	 * @return True if the recache failed, false otherwise (typically implies an allocation failure in a caching data structure)
	 */
	bool recacheNUMASupport(MM_EnvironmentBase *env);

	/**
	 * Sets the number of affinity leaders to simulate for testing logical decision-making on non-NUMA hardware.
	 * This implicitly disables any "real" NUMA support.
	 * Simulating NUMA causes us to setup a "fake" affinity leader array (as seen by getAffinityLeaders) but the node numbers given in these structures should be ignored for any
	 * calls into Port or Thread since we are faking them (call "isPhysicalNUMASupported()" to determine if the affinity leaders have physical mappings or just logical)
	 * @param simulatedNodeCount[in] The number of affinity leaders to simulate
	 */
	void setSimulatedNodeCountForFVTest(uintptr_t simulatedNodeCount);

	/**
	 * @return The number of NUMA nodes which act as "affinity leaders" in the system (that is, they have both memory and CPU) - returns zero if NUMA not enabled
	 */
	uintptr_t getAffinityLeaderCount() const;

	/**
	 * @return The highest j9NodeNumber of all NUMA nodes currently known to the receiver or 0 if NUMA is not enabled or available
	 */
	uintptr_t getMaximumNodeNumber() const;

	/**
	 * Returns access to the array of nodes which the receiver has determined are to act as affinity leaders.
	 * NOTE:  The returned array should NOT be modified!
	 * NOTE:  If recacheNUMASupport is called, the returned array will be invalidated
	 * @param arrayLength[out] The size of the array returned, in elements
	 * @return A pointer to the internal array describing affinity leader nodes (invalidated by any call to recacheNUMASupport)
	 */
	J9MemoryNodeDetail const *getAffinityLeaders(uintptr_t *arrayLength) const;

	/**
	 * Returns access to the array of nodes which the receiver has determined make up the "free processor pool" (that is, nodes with CPU resources
	 * but no memory).  Typical running environments do not have a free processor pool.
	 * NOTE:  The returned array should NOT be modified!
	 * NOTE:  If recacheNUMASupport is called, the returned array will be invalidated
	 * @param arrayLength[out] The size of the array returned, in elements
	 * @return A pointer to the internal array describing free processor pool's nodes (invalidated by any call to recacheNUMASupport)
	 */
	J9MemoryNodeDetail const *getFreeProcessorPool(uintptr_t *arrayLength) const;

	/**
	 * Returns the sum of Computational Resources Available across all nodes
	 * @return the sum of Computational Resources Available across all nodes
	 */
	uintptr_t getComputationalResourcesAvailableForAllNodes() const;

	/**
	 * @return True if NUMA is enabled and the underlying system exposes NUMA capabilities (false will be returned if we are simulating NUMA since that isn't "physical")
	 */
	bool isPhysicalNUMASupported() const;

	/**
	 * Called to disable NUMA support and clear any caches allocated to track NUMA support
	 * @param env[in] The master GC thread
	 */
	void shutdownNUMASupport(MM_EnvironmentBase *env);

	/**
	 * Create NUMAManager object.
	 */
	MM_NUMAManager()
		: _physicalNumaEnabled(false)
		, _simulatedNodeCount(0)
		, _maximumNodeNumber(0)
		, _activeNodes(NULL)
		, _activeNodeCount(0)
		, _affinityLeaders(NULL)
		, _affinityLeaderCount(0)
		, _freeProcessorPoolNodes(NULL)
		, _freeProcessorPoolNodeCount(0)
	{}
};

#endif /* OMR_GC_VLHGC */

