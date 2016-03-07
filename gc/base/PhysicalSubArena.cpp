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


#include "PhysicalSubArena.hpp"

#include "Debug.hpp"
#include "PhysicalArena.hpp"

/**
 * Initialize the instances internal structures.
 * @return true if the initialization was successful, false otherwise.
 */
bool
MM_PhysicalSubArena::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Destroy and delete the instances internal structures.
 */
void
MM_PhysicalSubArena::tearDown(MM_EnvironmentBase *env)
{
}

/**
 * Determine whether the sub arena is allowed to expand.
 * The generic implementation forwards the request to the parent, or returns true if there is none.
 *
 * @return true if the expansion is allowed, false otherwise.
 */
bool
MM_PhysicalSubArena::canExpand(MM_EnvironmentBase *env)
{
	if(!_resizable) {
		return false;
	}

	if(_parent) {
		return _parent->canExpand(env, this);
	}

	return true;
}

/**
 * Contract the sub arenas physical memory by at least the size specified.
 *
 * @return The amount of heap actually contracted.
 */
uintptr_t
MM_PhysicalSubArena::contract(MM_EnvironmentBase *env, uintptr_t contractSize)
{
	assume0(false); // Should never get here
	return 0;
}

/**
 * Determine whether the sub arena is allowed to contract.
 *
 * @return true if the contraction is allowed, false otherwise.
 */
bool
MM_PhysicalSubArena::canContract(MM_EnvironmentBase *env)
{
	/* Normally we'd use the _resizable flag, but since contract is a stronger operation,
	 * we just default to false in the generic case.
	 */
	return false;
}

/**
 * Check if the receiver can complete a counter balancing with the requested expand size.
 * The check to complete a counter balance with an expand involves verifying address ranges are possible against virtually
 * adjusted address ranges of other PSA's, as well as checking the actual physical boundaries.  The expand size can be adjusted
 * by increments of the delta alignment that is supplied.
 * @return the actual physical size that the receiver can expand by, or 0 if there is no room or alignment restrictions cannot be met.
 */
uintptr_t
MM_PhysicalSubArena::checkCounterBalanceExpand(MM_EnvironmentBase *env, uintptr_t expandSizeDeltaAlignment, uintptr_t expandSize)
{
	return expandSize;
}

/**
 * Expand the receiver by the given size.
 * Expand by the given size with no safety checks - checking we've assured that the expand size will in fact fit and can
 * be properly distributed within the receivers parameters.
 * @return the size of the expansion (which should match the size supplied)
 */
uintptr_t
MM_PhysicalSubArena::expandNoCheck(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	assume0(0);  /* override - you can only call this if the receiver can in fact expand */
	return expandSize;
}

/**
 * Adjust the allocate and survivor spaces to have the sizes specified.
 * Force the tilting of the semispace to be of the exact sizes specified.  There are a number of assumptions made, including
 * the alignment of the sizes, the sizes are equal to the total semispace size, the sizes fall within ration limits, etc.
 * @note For non-semispace PSA's, do nothing (it means nothing to call this on it)
 */
void
MM_PhysicalSubArena::tilt(MM_EnvironmentBase *env, uintptr_t allocateSpaceSize, uintptr_t survivorSpaceSize)
{
}

/**
 * Slide the boundary between the two semi spaces to create sufficient free space to operate in both the
 * allocate and survivor subspaces.
 * @note For non-semispace PSA's, do nothing (it means nothing to call this on it)
 */
void
MM_PhysicalSubArena::tilt(MM_EnvironmentBase *env, uintptr_t survivorSpaceSizeRequested)
{
}

/**
 * Get the size of heap available for contraction.
 * Return the amount of heap available to be contracted, factoring in any potential allocate that may require the
 * available space.
 * @return The amount of heap available for contraction factoring in the size of the allocate (if applicable)
 */
uintptr_t
MM_PhysicalSubArena::getAvailableContractionSize(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace, MM_AllocateDescription *allocDescription)
{
	return 0;
}

