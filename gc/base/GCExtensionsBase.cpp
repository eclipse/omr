/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "GCExtensionsBase.hpp"

#include <string.h>

#include "hookable_api.h"
#include "omrmemcategories.h"
#include "modronbase.h"

#include "CollectorLanguageInterface.hpp"
#include "EnvironmentBase.hpp"
#if defined(OMR_GC_MODRON_SCAVENGER)
#include "Scavenger.hpp"
#endif /* OMR_GC_MODRON_SCAVENGER */

MM_GCExtensionsBase*
MM_GCExtensionsBase::newInstance(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensionsBase* extensions;

	/* Avoid using OMR::GC::Forge to allocate memory for the extension, since OMR::GC::Forge itself has not been created! */
	extensions = static_cast<MM_GCExtensionsBase*>(omrmem_allocate_memory(sizeof(MM_GCExtensionsBase), OMRMEM_CATEGORY_MM));
	if (extensions) {
		new (extensions) MM_GCExtensionsBase();
		if (!extensions->initialize(env)) {
			extensions->kill(env);
			return NULL;
		}
	}
	return extensions;
}

void
MM_GCExtensionsBase::kill(MM_EnvironmentBase* env)
{
	/* Avoid using OMR::GC::Forge to free memory for the extension, since OMR::GC::Forge was not used to allocate the memory */
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	tearDown(env);
	omrmem_free_memory(this);
}

bool
MM_GCExtensionsBase::initialize(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;

	_omrVM = env->getOmrVM();

#if defined(OMR_GC_MODRON_STANDARD)
#if defined(OMR_GC_MODRON_SCAVENGER)
	configurationOptions._gcPolicy = gc_policy_gencon;
#else /* OMR_GC_MODRON_SCAVENGER */
	configurationOptions._gcPolicy = gc_policy_optthruput;
#endif /* OMR_GC_MODRON_SCAVENGER */
#elif defined(OMR_GC_REALTIME)
	configurationOptions._gcPolicy = gc_policy_metronome;
#elif defined(OMR_GC_VLHGC)
	configurationOptions._gcPolicy = gc_policy_balanced;
#else
#error Default GC policy cannot be determined
#endif /* OMR_GC_MODRON_STANDARD */


#if defined(OMR_GC_MODRON_SCAVENGER)
	if (!rememberedSet.initialize(env, OMR::GC::AllocationCategory::REMEMBERED_SET)) {
		goto failed;
	}
	rememberedSet.setGrowSize(OMR_SCV_REMSET_SIZE);
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(J9MODRON_USE_CUSTOM_SPINLOCKS)
	lnrlOptions.spinCount1 = 256;
	lnrlOptions.spinCount2 = 32;
	lnrlOptions.spinCount3 = 45;
#endif /* J9MODRON_USE_CUSTOM_SPINLOCKS */

	/* there was no GC before VM birth, so this is the first moment we can get "end of previous GC" timestamp */
	excessiveGCStats.endGCTimeStamp = omrtime_hires_clock();
	excessiveGCStats.lastEndGlobalGCTimeStamp = excessiveGCStats.endGCTimeStamp;

        /* Get usable physical memory from portlibrary. This is used by computeDefaultMaxHeap().
	 * It can also be used by downstream projects to compute project specific GC parameters.
	 */
	usablePhysicalMemory = omrsysinfo_get_addressable_physical_memory();

	computeDefaultMaxHeap(env);

	maxSizeDefaultMemorySpace = memoryMax;

	/* Set preferred page size/page flags for Heap and GC Metadata */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* Get preferred page parameters for Heap and GC Metadata, then validate them */
	requestedPageSize = pageSizes[0];
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	gcmetadataPageSize = pageSizes[0];
	gcmetadataPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

#define SIXTY_FOUR_KB	((uintptr_t)64 * 1024)
#define ONE_MB			((uintptr_t)1 * 1024 * 1024)
#define TWO_MB			((uintptr_t)2 * 1024 * 1024)

#if defined(AIXPPC)
	requestedPageSize = SIXTY_FOUR_KB; /* Use 64K pages for AIX-32 and AIX-64 */
#elif ((defined(LINUX) || defined(OSX)) && (defined(J9X86) || defined(J9HAMMER)))
	requestedPageSize = TWO_MB; /* Use 2M pages for Linux/OSX x86-64 */
#elif (defined(LINUX) && defined(S390))
	requestedPageSize = ONE_MB; /* Use 1M pages for zLinux-31 and zLinux-64 */
#elif defined(J9ZOS390)
	requestedPageSize = ONE_MB; /* Use 1M PAGEABLE for ZOS-31 and ZOS-64 */
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
#endif /* defined(AIXPPC) */

	if (!validateDefaultPageParameters(requestedPageSize, requestedPageFlags, pageSizes, pageFlags)) {
		requestedPageSize = pageSizes[0];
		requestedPageFlags = pageFlags[0];
	}

	if (!validateDefaultPageParameters(gcmetadataPageSize, gcmetadataPageFlags, pageSizes, pageFlags)) {
		gcmetadataPageSize = pageSizes[0];
		gcmetadataPageFlags = pageFlags[0];
	}


	if (!_forge.initialize(env->getPortLibrary())) {
		goto failed;
	}

	if (J9HookInitializeInterface(getPrivateHookInterface(), OMRPORTLIB, sizeof(privateHookInterface))) {
		goto failed;
	}

	if (J9HookInitializeInterface(getOmrHookInterface(), OMRPORTLIB, sizeof(omrHookInterface))) {
		goto failed;
	}

	if (omrthread_monitor_init_with_name(&gcExclusiveAccessMutex, 0, "GCExtensions::gcExclusiveAccessMutex")) {
		goto failed;
	}

	if (omrthread_monitor_init_with_name(&_lightweightNonReentrantLockPoolMutex, 0, "GCExtensions::_lightweightNonReentrantLockPoolMutex")) {
		goto failed;
	}

	if (!objectModel.initialize(this)) {
		goto failed;
	}

	if (!mixedObjectModel.initialize(this)) {
		goto failed;
	}

	if (!indexableObjectModel.initialize(this)) {
		goto failed;
	}

	return true;

failed:
	tearDown(env);
	return false;
}

bool
MM_GCExtensionsBase::validateDefaultPageParameters(uintptr_t pageSize, uintptr_t pageFlags, uintptr_t *pageSizesArray, uintptr_t *pageFlagsArray)
{
	bool result = false;

	if (0 != pageSize) {
		for (uintptr_t i = 0; 0 != pageSizesArray[i]; i++) {
			if ((pageSize == pageSizesArray[i]) && (pageFlags == pageFlagsArray[i])) {
				result = true;
				break;
			}
		}
	}
	return result;
}

void
MM_GCExtensionsBase::tearDown(MM_EnvironmentBase* env)
{
#if defined(OMR_GC_MODRON_SCAVENGER)
	rememberedSet.tearDown(env);
#endif /* OMR_GC_MODRON_SCAVENGER */

	objectModel.tearDown(this);
	mixedObjectModel.tearDown(this);
	indexableObjectModel.tearDown(this);

	if (NULL != collectorLanguageInterface) {
		collectorLanguageInterface->kill(env);
		collectorLanguageInterface = NULL;
	}

	if (NULL != environments) {
		pool_kill(environments);
		environments = NULL;
	}

	if (NULL != gcExclusiveAccessMutex) {
		omrthread_monitor_destroy(gcExclusiveAccessMutex);
		gcExclusiveAccessMutex = (omrthread_monitor_t) NULL;
	}

	if (NULL != _lightweightNonReentrantLockPoolMutex) {
		omrthread_monitor_destroy(_lightweightNonReentrantLockPoolMutex);
		_lightweightNonReentrantLockPoolMutex = (omrthread_monitor_t) NULL;
	}

	_forge.tearDown();

	J9HookInterface** tmpHookInterface = getPrivateHookInterface();
	if ((NULL != tmpHookInterface) && (NULL != *tmpHookInterface)) {
		(*tmpHookInterface)->J9HookShutdownInterface(tmpHookInterface);
		*tmpHookInterface = NULL; /* avoid issues with double teardowns */
	}

	J9HookInterface** tmpOmrHookInterface = getOmrHookInterface();
	if ((NULL != tmpOmrHookInterface) && (NULL != *tmpOmrHookInterface)) {
		(*tmpOmrHookInterface)->J9HookShutdownInterface(tmpOmrHookInterface);
		*tmpOmrHookInterface = NULL; /* avoid issues with double teardowns */
	}
}

bool
MM_GCExtensionsBase::isConcurrentScavengerInProgress()
{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	if (NULL != scavenger) {
		return scavenger->isConcurrentInProgress();
	} else {
		return false;
	}
#else
	return false;
#endif
}

void
MM_GCExtensionsBase::identityHashDataAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress)
{
	/* empty */
}

void
MM_GCExtensionsBase::identityHashDataRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress)
{
	/* empty */
}

/* Set Xmx (heap default). For most platforms the heap default is a fraction of
 * the usable physical memory - half of usable memory with a min of 16 MiB and a max of 512 MiB.
 */
void
MM_GCExtensionsBase::computeDefaultMaxHeap(MM_EnvironmentBase* env)
{
	/* we are going to try to request a slice of half the usable memory */
	uint64_t memoryToRequest = (usablePhysicalMemory / 2);

#define J9_PHYSICAL_MEMORY_MAX (uint64_t)(512 * 1024 * 1024)
#define J9_PHYSICAL_MEMORY_DEFAULT (16 * 1024 * 1024)

	if (0 == memoryToRequest) {
		memoryToRequest = J9_PHYSICAL_MEMORY_DEFAULT;
	}
	memoryToRequest = OMR_MIN(memoryToRequest, J9_PHYSICAL_MEMORY_MAX);

	/* Initialize Xmx, Xmdx */
	memoryMax = MM_Math::roundToFloor(heapAlignment, (uintptr_t)memoryToRequest);
}
