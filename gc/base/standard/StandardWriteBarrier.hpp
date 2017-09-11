 /*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#ifndef STANDARDWRITEBARRIER_HPP_
#define STANDARDWRITEBARRIER_HPP_

#include "objectdescription.h"

#include "CardTable.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectModel.hpp"
#include "Scavenger.hpp"
#include "SlotObject.hpp"

struct OMR_VMThread;

/**
 * Out-of-line write barrier. In the absence of other (equivalent inline) write barrier, this method must
 * be called whenever a child reference is assigned to a parent slot.
 *
 * To support OMR concurrent marking and/or generational collectors, this method calls the necessary
 * concurrent and generational write barriers.
 *
 * @param omrThread The thread making the assignment of child reference into parent slot
 * @param parentObject the parent object
 * @param childObject THe child object reference
 */
MMINLINE void
standardWriteBarrier(OMR_VMThread *omrThread, omrobjectptr_t parentObject, omrobjectptr_t childObject)
{
#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_MODRON_CONCURRENT_MARK)
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();
#if defined(OMR_GC_MODRON_SCAVENGER)
	if (extensions->scavengerEnabled) {
		if (extensions->isOld(parentObject) && !extensions->isOld(childObject)) {
			if (extensions->objectModel.atomicSetRememberedState(parentObject, STATE_REMEMBERED)) {
				/* The object has been successfully marked as REMEMBERED - allocate an entry in the remembered set */
				extensions->scavenger->addToRememberedSetFragment((MM_EnvironmentStandard *)env, parentObject);
			}
		}
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	if (extensions->concurrentMark) {
		extensions->cardTable->dirtyCard(env, parentObject);
	}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_MODRON_CONCURRENT_MARK) */
}

/**
 * Convenience method to effect the assignment of a child reference to a parent slot and call
 * out-of-line write barrier.
 *
 * @param omrThread The thread making the assignment of child reference to parent slot
 * @param parentObject the parent object
 * @param parentSlot Points to the slot in the parent object that will receive the child reference
 * @param childObject THe child object reference
 * @see standardWriteBarrier(OMR_VMThread *, omrobjectptr_t, omrobjectptr_t)
 */
MMINLINE void
standardWriteBarrierStore(OMR_VMThread *omrThread, omrobjectptr_t parentObject, fomrobject_t *parentSlot, omrobjectptr_t childObject)
{
	GC_SlotObject slotObject(omrThread->_vm, parentSlot);
	slotObject.writeReferenceToSlot(childObject);

	standardWriteBarrier(omrThread, parentObject, childObject);
}

#endif /* STANDARDWRITEBARRIER_HPP_ */
