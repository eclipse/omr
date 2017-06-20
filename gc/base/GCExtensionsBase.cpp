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
 ******************************************************************************/

#include "GCExtensionsBase.hpp"

#include <string.h>

#include "hookable_api.h"
#include "omrmemcategories.h"
#include "modronbase.h"

#include "CollectorLanguageInterface.hpp"
#include "EnvironmentBase.hpp"

MM_GCExtensionsBase*
MM_GCExtensionsBase::newInstance(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensionsBase* extensions;

	/* Avoid using MM_Forge to allocate memory for the extension, since MM_Forge itself has not been created! */
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
	/* Avoid using MM_Forge to free memory for the extension, since MM_Forge was not used to allocate the memory */
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	tearDown(env);
	omrmem_free_memory(this);
}

bool
MM_GCExtensionsBase::initialize(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t physicalMemory = 0;
	uint64_t memoryLimit = 0;
	uint64_t usableMemory = 0;
	uint64_t memoryToRequest = 0;
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
	if (!rememberedSet.initialize(env, MM_AllocationCategory::REMEMBERED_SET)) {
		goto failed;
	}
	rememberedSet.setGrowSize(J9_SCV_REMSET_SIZE);
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(J9MODRON_USE_CUSTOM_SPINLOCKS)
	lnrlOptions.spinCount1 = 256;
	lnrlOptions.spinCount2 = 32;
	lnrlOptions.spinCount3 = 45;
#endif /* J9MODRON_USE_CUSTOM_SPINLOCKS */

	/* there was no GC before VM birth, so this is the first moment we can get "end of previous GC" timestamp */
	excessiveGCStats.endGCTimeStamp = omrtime_hires_clock();
	excessiveGCStats.lastEndGlobalGCTimeStamp = excessiveGCStats.endGCTimeStamp;

	/* Set Xmx (heap default).  For most platforms the heap default is a fraction of
	 * the available physical memory, bounded by the build specs.
	 *
	 * 	Windows: half the REAL memory; with a min of 16MiB and a max of 2GiB.
	 *
	 * Linux, AIX, and z/OS:  half of OMR_MIN(physical memory, RLIMIT_AS) with a min of
	 * 16 MiB and a max of 512 MiB.
	 * -note that RLIMIT_AS is as extracted from getrlimit and represents the resouce
	 * limitation on address space.
	 */

	/* Initial physicalMemory as per system call. */
	physicalMemory = omrsysinfo_get_physical_memory();
	if (OMRPORT_LIMIT_LIMITED == omrsysinfo_get_limit(OMRPORT_RESOURCE_ADDRESS_SPACE, &memoryLimit)) {
		/* there is a limit on the memory we can use so take the minimum of this usable amount and the physical memory */
		usableMemory = OMR_MIN(memoryLimit, physicalMemory);
	} else {
		/* if there is no memory limit being imposed on us, we will use physical memory as our max */
		usableMemory = physicalMemory;
	}
	/* we are going to try to request a slice of half the usable memory */
	memoryToRequest = (usableMemory / 2);

	/* TODO: Only cap HRT to 64M heap.  Should this be removed? */
#define J9_PHYSICAL_MEMORY_MAX (uint64_t)(512 * 1024 * 1024)
#define J9_PHYSICAL_MEMORY_DEFAULT (16 * 1024 * 1024)

	if (0 == memoryToRequest) {
		memoryToRequest = J9_PHYSICAL_MEMORY_DEFAULT;
	}
	memoryToRequest = OMR_MIN(memoryToRequest, J9_PHYSICAL_MEMORY_MAX);

	/* Initialize Xmx, Xmdx */
	memoryMax = MM_Math::roundToFloor(heapAlignment, (uintptr_t)memoryToRequest);
	maxSizeDefaultMemorySpace = MM_Math::roundToFloor(heapAlignment, (uintptr_t)memoryToRequest);

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


	if (!_forge.initialize(env)) {
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

#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	if (!scavengerHotFieldStats.initialize(env)) {
		goto failed;
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */

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
#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	scavengerHotFieldStats.tearDown(env);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */

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

	_forge.tearDown(env);

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
