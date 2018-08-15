###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

omr_assert(TEST OMR_GC)

omr_assert(
	TEST OMR_GC_GLUE_TARGET
	MESSAGE "OMR_GC_GLUE_TARGET must be set."
)

omr_add_hookgen(
	INPUT include/omrmm.hdf
	PUBLIC_DIR "${omr_BINARY_DIR}"
	TARGETS omrgc_hookgen
)

omr_add_hookgen(
	INPUT base/omrmmprivate.hdf
	TARGETS omrgc_hookgen
)

add_custom_target(omrgc_hookgen
	DEPENDS
		"${omr_BINARY_DIR}/mmomrhook.h"
		"${CMAKE_CURRENT_BINARY_DIR}/mmprivatehook.h"
)

add_tracegen(base/j9mm.tdf)
add_tracegen(base/omrmm.tdf)
add_tracegen(verbose/j9vgc.tdf)

# Ideally We would handle the tracegen stuff like the hookgen stuff above
# But for whatever reason that breaks the ninja builds
add_library(omrgc_tracegen OBJECT
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9mm.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_omrmm.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9vgc.c
)

add_library(omrgc STATIC
	base/AddressOrderedListPopulator.cpp
	base/AllocationContext.cpp
	base/AllocationInterfaceGeneric.cpp
	base/BaseVirtual.cpp
	base/BumpAllocatedListPopulator.cpp
	base/CardTable.cpp
	base/Collector.cpp
	base/Configuration.cpp
	base/Dispatcher.cpp
	base/EmptyListPopulator.cpp
	base/EnvironmentBase.cpp
	base/Evacuator.cpp
	base/EvacuatorBase.cpp
	base/EvacuatorController.cpp
	base/EvacuatorParallelTask.cpp
	base/Forge.cpp
	base/GCCode.cpp
	base/GCExtensionsBase.cpp
	base/GlobalAllocationManager.cpp
	base/GlobalCollector.cpp
	base/Heap.cpp
	base/HeapMap.cpp
	base/HeapMapIterator.cpp
	base/HeapMemorySubSpaceIterator.cpp
	base/HeapRegionDescriptor.cpp
	base/HeapRegionIterator.cpp
	base/HeapRegionManager.cpp
	base/HeapRegionManagerTarok.cpp
	base/HeapVirtualMemory.cpp
	base/LightweightNonReentrantLock.cpp
	base/LightweightNonReentrantReaderWriterLock.cpp
	base/MarkedObjectPopulator.cpp
	base/MarkingScheme.cpp
	base/MarkMap.cpp
	base/MarkMapSegmentChunkIterator.cpp
	base/MasterGCThread.cpp
	base/Math.cpp
	base/MemoryManager.cpp
	base/MemoryPool.cpp
	base/MemoryPoolAddressOrderedList.cpp
	base/MemoryPoolAddressOrderedListBase.cpp
	base/MemoryPoolBumpPointer.cpp
	base/MemoryPoolHybrid.cpp
	base/MemoryPoolLargeObjects.cpp
	base/MemoryPoolSplitAddressOrderedList.cpp
	base/MemoryPoolSplitAddressOrderedListBase.cpp
	base/MemorySpace.cpp
	base/MemorySubSpace.cpp
	base/MemorySubSpaceChildIterator.cpp
	base/MemorySubSpaceFlat.cpp
	base/MemorySubSpaceGeneric.cpp
	base/MemorySubSpacePoolIterator.cpp
	base/MemorySubSpaceRegionIterator.cpp
	base/MemorySubSpaceUniSpace.cpp
	base/ModronAssertions.cpp
	base/NUMAManager.cpp
	base/NonVirtualMemory.cpp
	base/OMRVMInterface.cpp
	base/OMRVMThreadInterface.cpp
	base/ObjectAllocationInterface.cpp
	base/ObjectHeapBufferedIterator.cpp
	base/ObjectHeapIteratorAddressOrderedList.cpp
	base/Packet.cpp
	base/PacketList.cpp
	base/ParallelDispatcher.cpp
	base/ParallelHeapWalker.cpp
	base/ParallelObjectHeapIterator.cpp
	base/ParallelMarkTask.cpp
	base/ParallelTask.cpp
	base/PhysicalArena.cpp
	base/PhysicalArenaRegionBased.cpp
	base/PhysicalArenaVirtualMemory.cpp
	base/PhysicalSubArena.cpp
	base/PhysicalSubArenaRegionBased.cpp
	base/PhysicalSubArenaVirtualMemory.cpp
	base/PhysicalSubArenaVirtualMemoryFlat.cpp
	base/ReferenceChainWalkerMarkMap.cpp
	base/RegionPool.cpp
	base/RegionPoolGeneric.cpp
	base/StartupManager.cpp
	base/SweepHeapSectioning.cpp
	base/SweepPoolManager.cpp
	base/SweepPoolManagerAddressOrderedList.cpp
	base/SweepPoolManagerAddressOrderedListBase.cpp
	base/SweepPoolManagerHybrid.cpp
	base/SweepPoolManagerSplitAddressOrderedList.cpp
	base/SweepPoolState.cpp
	base/TLHAllocationInterface.cpp
	base/TLHAllocationSupport.cpp
	base/Task.cpp
	base/VirtualMemory.cpp
	base/WorkPacketOverflow.cpp
	base/WorkPackets.cpp
	base/WorkStack.cpp
	base/gcspinlock.cpp
	base/gcutils.cpp
	base/modronapicore.cpp

	startup/mminitcore.cpp
	startup/omrgcalloc.cpp
	startup/omrgcstartup.cpp

	stats/AllocationStats.cpp
	stats/CardCleaningStats.cpp
	stats/ClassUnloadStats.cpp

	stats/HeapResizeStats.cpp
	stats/LargeObjectAllocateStats.cpp
	stats/MarkStats.cpp
	stats/MetronomeStats.cpp
	stats/RootScannerStats.cpp
	stats/ScavengerStats.cpp # TODO only compile if scavenger or VLHGC. Is this actually used by VLHGC?
	stats/SweepStats.cpp

	structs/ForwardedHeader.cpp
	structs/HashTableIterator.cpp
	structs/OMRVMThreadListIterator.cpp
	structs/PoolIterator.cpp
	structs/SublistFragment.cpp
	structs/SublistIterator.cpp
	structs/SublistPool.cpp
	structs/SublistPuddle.cpp
	structs/SublistSlotIterator.cpp

	# verbose/j9vgc.tdf
	verbose/VerboseBuffer.cpp
	verbose/VerboseHandlerOutput.cpp
	verbose/VerboseManager.cpp
	verbose/VerboseWriter.cpp
	verbose/VerboseWriterChain.cpp
	verbose/VerboseWriterFileLogging.cpp
	verbose/VerboseWriterFileLoggingBuffered.cpp
	verbose/VerboseWriterFileLoggingSynchronous.cpp
	verbose/VerboseWriterHook.cpp
	verbose/VerboseWriterStreamOutput.cpp
	verbose/handler_standard/VerboseHandlerOutputStandard.cpp
	$<TARGET_OBJECTS:omrgc_tracegen>
)

if(OMR_GC_OBJECT_MAP)
	target_sources(omrgc
		PRIVATE
			base/ObjectMap.cpp
	)
endif()

if(OMR_GC_MODRON_STANDARD)
	target_sources(omrgc
		PRIVATE
			base/standard/ConfigurationFlat.cpp
			base/standard/ConfigurationStandard.cpp
			base/standard/CopyScanCacheChunk.cpp
			base/standard/CopyScanCacheChunkInHeap.cpp
			base/standard/EnvironmentStandard.cpp
			base/standard/HeapMemoryPoolIterator.cpp
			base/standard/HeapRegionDescriptorStandard.cpp
			base/standard/HeapRegionManagerStandard.cpp
			base/standard/HeapWalker.cpp
			base/standard/OverflowStandard.cpp
			base/standard/ParallelGlobalGC.cpp
			base/standard/ParallelSweepScheme.cpp
			base/standard/SweepHeapSectioningSegmented.cpp
			base/standard/WorkPacketsStandard.cpp
	)
	if (OMR_GC_MODRON_COMPACTION)
		target_sources(omrgc
			PRIVATE
				base/standard/CompactFixHeapForWalkTask.cpp
				base/standard/CompactScheme.cpp
				base/standard/ParallelCompactTask.cpp
				
				stats/CompactStats.cpp
		)
	endif()
	if(OMR_GC_MODRON_CONCURRENT_MARK)
		target_sources(omrgc
			PRIVATE
				base/standard/ConcurrentCardTable.cpp
				base/standard/ConcurrentCardTableForWC.cpp
				base/standard/ConcurrentClearNewMarkBitsTask.cpp
				base/standard/ConcurrentCompleteTracingTask.cpp
				base/standard/ConcurrentFinalCleanCardsTask.cpp
				base/standard/ConcurrentGC.cpp
				base/standard/ConcurrentOverflow.cpp
				base/standard/ConcurrentPrepareCardTableTask.cpp
				base/standard/ConcurrentSafepointCallback.cpp
				base/standard/WorkPacketsConcurrent.cpp
				
				stats/ConcurrentGCStats.cpp
		)
		if(OMR_GC_MODRON_SCAVENGER)
			target_sources(omrgc
				PRIVATE
					base/standard/ConcurrentScanRememberedSetTask.cpp
			)
		endif()
	endif()
	if(OMR_GC_CONCURRENT_SWEEP)
		target_sources(omrgc
			PRIVATE
				base/standard/ConcurrentSweepScheme.cpp
		)
	endif()
	if(OMR_GC_MODRON_SCAVENGER)
		target_sources(omrgc
			PRIVATE
				base/HeapSplit.cpp # TODO delete as this should not be used anymore!
				base/MemorySubSpaceGenerational.cpp
				base/MemorySubSpaceSemiSpace.cpp
				
				base/standard/ConfigurationGenerational.cpp
				base/standard/CopyScanCacheList.cpp
				base/standard/ParallelScavengeTask.cpp
				base/standard/PhysicalSubArenaVirtualMemorySemiSpace.cpp
				base/standard/RSOverflow.cpp
				base/standard/Scavenger.cpp
				
				stats/ScavengerCopyScanRatio.cpp
		)
		if(OMR_GC_CONCURRENT_SCAVENGER)
			ltarget_sources(omrgc
				PRIVATE
					base/standard/ConcurrentScavengeTask.cpp
			)
		endif()
	endif()
endif()

if(OMR_GC_SEGREGATED_HEAP)
	target_sources(omrgc
		PRIVATE
			base/segregated/AllocationContextSegregated.cpp
			base/segregated/ConfigurationSegregated.cpp
			base/segregated/GlobalAllocationManagerSegregated.cpp
			base/segregated/HeapRegionDescriptorSegregated.cpp
			base/segregated/LockingFreeHeapRegionList.cpp
			base/segregated/LockingHeapRegionQueue.cpp
			base/segregated/MemoryPoolAggregatedCellList.cpp
			base/segregated/MemoryPoolSegregated.cpp
			base/segregated/MemorySubSpaceSegregated.cpp
			base/segregated/ObjectHeapIteratorSegregated.cpp
			base/segregated/OverflowSegregated.cpp
			base/segregated/RegionPoolSegregated.cpp
			base/segregated/SegregatedAllocationInterface.cpp
			base/segregated/SegregatedAllocationTracker.cpp
			base/segregated/SegregatedGC.cpp
			base/segregated/SegregatedListPopulator.cpp
			base/segregated/SegregatedMarkingScheme.cpp
			base/segregated/SegregatedSweepTask.cpp
			base/segregated/SizeClasses.cpp
			base/segregated/SweepSchemeSegregated.cpp
			base/segregated/WorkPacketsSegregated.cpp
	)
endif()

add_dependencies(omrgc omrgc_hookgen)

if(OMR_WARNINGS_AS_ERRORS)
	target_compile_options(omrgc PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
endif()

if(OMR_ENHANCED_WARNINGS)
	target_compile_options(omrgc PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
	target_compile_options(omrgc PRIVATE -fno-strict-aliasing)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "XL|VisualAge")
	target_compile_options(omrgc PRIVATE -qalias=noansi)
endif()

target_include_directories(omrgc
	PUBLIC
		.
		${CMAKE_CURRENT_BINARY_DIR}  #Ideally this should be private, but the glue pulls this in
		base
		base/segregated
		base/standard
		include
		startup
		stats
		structs
		verbose
		verbose/handler_standard
	INTERFACE
		$<TARGET_PROPERTY:${OMR_GC_GLUE_TARGET},INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(omrgc
	PRIVATE
		${OMR_GC_GLUE_TARGET}
		omrutil
		omrcore
		${OMR_THREAD_LIB}
		${OMR_PORT_LIB}
		${OMR_HOOK_LIB}
)

set_target_properties(omrgc omrgc_hookgen omrgc_tracegen PROPERTIES FOLDER gc)
