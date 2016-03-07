/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 * @ingroup GC_Structs
 */

#include "omrcfg.h"
#include "omrcomp.h"

#include "SublistFragment.hpp"

#include "EnvironmentBase.hpp"
#include "SublistPool.hpp"

extern "C" {

/**
 * Allocate a new area for the fragment from the parent sublist.
 * 
 * @return 0 on success, non-zero on failure 
 */
uintptr_t
allocateMemoryForSublistFragment(void *vmThreadRawPtr, J9VMGC_SublistFragment *fragmentPrimitive)
{
	OMR_VMThread *omrVMThread = (OMR_VMThread*) vmThreadRawPtr;
	MM_SublistFragment fragment(fragmentPrimitive);
	
	MM_SublistFragment::flush(fragmentPrimitive);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	
	bool result = ((MM_SublistPool *)fragmentPrimitive->parentList)->allocate(env, &fragment);
	if (result) {
		return 0;
	} else {
#if defined(OMR_GC_MODRON_SCAVENGER)
		env->getExtensions()->setRememberedSetOverflowState();
#endif /* OMR_GC_MODRON_SCAVENGER */
		return 1;
	}
}

} /* extern "C" */

/**
 * Allocate a new entry from a sublist fragment.
 * Returns the next available entry in the sublist fragment, allocating a new
 * fragment from the sublist as required.
 * 
 * @return Next free entry from the sublist.
 */
void *
MM_SublistFragment::allocate(MM_EnvironmentBase *env)
{	
	MM_SublistPool *parentList = (MM_SublistPool *)_fragment->parentList;
	
	/* Check if there is a free entry available in the fragment */
	if(_fragment->fragmentCurrent < _fragment->fragmentTop){			
		_fragment->count += 1;
		
		return _fragment->fragmentCurrent++;
	}
	

	/* There is no free entry available - attempt to allocate a new fragment from the sublist */
	if(parentList->allocate(env, this)) {
		_fragment->count += 1;
		
		/* A new fragment was obtained - return the next available slot */
		return _fragment->fragmentCurrent++;
	}

	/* Failed to allocate from the fragment/sublist */
	return NULL;
}

/**
 * Allocate a new entry from a sublist fragment (using allocate()),
 * and store <code>entry</code> into the new entry.
 * @return true if the operation completed successfully, false otherwise
 */
bool
MM_SublistFragment::add(MM_EnvironmentBase *env, uintptr_t entry)
{
	uintptr_t *dest = (uintptr_t *)allocate(env);
	if (dest != NULL) {
		*dest = entry;
		return true;	
	}
	return false;
}
