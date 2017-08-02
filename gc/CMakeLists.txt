###############################################################################
#
# (c) Copyright IBM Corp. 2017
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

project(libomrgc VERSION 0.0.1 LANGUAGES CXX)


add_hookgen(include/omrmm.hdf
	${CMAKE_CURRENT_SOURCE_DIR}/../include_core/mmomrhook.h
	${CMAKE_CURRENT_SOURCE_DIR}/base/mmomrhook_internal.h
)

add_hookgen(base/omrmmprivate.hdf
	${CMAKE_CURRENT_SOURCE_DIR}/base/mmprivatehook.h
	${CMAKE_CURRENT_SOURCE_DIR}/base/mmprivatehook_internal.h
)

add_tracegen(base/j9mm.tdf)
add_tracegen(base/omrmm.tdf)
add_tracegen(verbose/j9vgc.tdf)
add_custom_target(omrgc_hookgen
		DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/../include_core/mmomrhook.h
		${CMAKE_CURRENT_SOURCE_DIR}/base/mmomrhook_internal.h
		${CMAKE_CURRENT_SOURCE_DIR}/base/mmprivatehook.h
		${CMAKE_CURRENT_SOURCE_DIR}/base/mmprivatehook_internal.h
)

# Ideally We would handle the tracegen stuff like the hookgen stuff above
# But for whatever reason that breaks the ninja builds
add_library(omrgc_tracegen OBJECT 
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9mm.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_omrmm.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9vgc.c
)

add_library(omrgc
	base/AddressOrderedListPopulator.cpp
	base/AllocationContext.cpp
	base/AllocationInterfaceGeneric.cpp
	base/BaseVirtual.cpp
	base/BumpAllocatedListPopulator.cpp
	base/CardTable.cpp
	base/Collector.cpp
	base/CollectorLanguageInterface.cpp
	base/Configuration.cpp
	base/CopyScanCache.cpp
	base/CycleState.cpp
	base/Dispatcher.cpp
	base/EmptyListPopulator.cpp
	base/EnvironmentBase.cpp
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
	base/HeapSplit.cpp
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
	base/MemorySubSpaceGenerational.cpp
	base/MemorySubSpaceGeneric.cpp
	base/MemorySubSpacePoolIterator.cpp
	base/MemorySubSpaceRegionIterator.cpp
	base/MemorySubSpaceSemiSpace.cpp
	base/MemorySubSpaceUniSpace.cpp
	base/ModronAssertions.cpp
	base/NUMAManager.cpp
	base/NonVirtualMemory.cpp
	base/OMRVMInterface.cpp
	base/OMRVMThreadInterface.cpp
	base/ObjectAllocationInterface.cpp
	base/ObjectHeapBufferedIterator.cpp
	base/ObjectHeapIteratorAddressOrderedList.cpp
	base/ObjectMap.cpp
	base/Packet.cpp
	base/PacketList.cpp
	base/ParallelDispatcher.cpp
	base/ParallelHeapWalker.cpp
	base/ParallelObjectHeapIterator.cpp
	base/ParallelMarkTask.cpp
	base/ParallelSweepChunk.cpp
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
	base/Validator.cpp
	base/VirtualMemory.cpp
	base/WorkPacketOverflow.cpp
	base/WorkPackets.cpp
	base/WorkStack.cpp
	base/ZOSLinkage.cpp
	base/gcspinlock.cpp
	base/gcutils.cpp
	base/modronapicore.cpp
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
	base/standard/CompactFixHeapForWalkTask.cpp
	base/standard/CompactScheme.cpp
	base/standard/ConcurrentCardTable.cpp
	base/standard/ConcurrentCardTableForWC.cpp
	base/standard/ConcurrentClearNewMarkBitsTask.cpp
	base/standard/ConcurrentCompleteTracingTask.cpp
	base/standard/ConcurrentFinalCleanCardsTask.cpp
	base/standard/ConcurrentGC.cpp
	base/standard/ConcurrentOverflow.cpp
	base/standard/ConcurrentPrepareCardTableTask.cpp
	base/standard/ConcurrentSafepointCallback.cpp
	base/standard/ConcurrentScanRememberedSetTask.cpp
	base/standard/ConcurrentScavengeTask.cpp
	base/standard/ConcurrentSweepScheme.cpp
	base/standard/ConfigurationFlat.cpp
	base/standard/ConfigurationGenerational.cpp
	base/standard/ConfigurationStandard.cpp
	base/standard/CopyScanCacheChunk.cpp
	base/standard/CopyScanCacheChunkInHeap.cpp
	base/standard/CopyScanCacheList.cpp
	base/standard/CopyScanCacheStandard.cpp
	base/standard/EnvironmentStandard.cpp
	base/standard/HeapMemoryPoolIterator.cpp
	base/standard/HeapRegionDescriptorStandard.cpp
	base/standard/HeapRegionManagerStandard.cpp
	base/standard/HeapWalker.cpp
	base/standard/OverflowStandard.cpp
	base/standard/ParallelCompactTask.cpp
	base/standard/ParallelGlobalGC.cpp
	base/standard/ParallelGlobalGCTask.cpp
	base/standard/ParallelScavengeTask.cpp
	base/standard/ParallelSweepScheme.cpp
	base/standard/PhysicalSubArenaVirtualMemorySemiSpace.cpp
	base/standard/RSOverflow.cpp
	base/standard/Scavenger.cpp
	base/standard/SweepHeapSectioningSegmented.cpp
	base/standard/WorkPacketsConcurrent.cpp
	base/standard/WorkPacketsStandard.cpp

	startup/mminitcore.cpp
	startup/omrgcalloc.cpp
	startup/omrgcstartup.cpp

	stats/AllocationFailureStats.cpp
	stats/AllocationStats.cpp
	stats/CardCleaningStats.cpp
	stats/ClassUnloadStats.cpp
	stats/CompactStats.cpp
	stats/ConcurrentCardTableStats.cpp
	stats/ConcurrentGCStats.cpp
	stats/ConcurrentSweepStats.cpp
	stats/CopyForwardStatsCore.cpp
	stats/HeapMemorySnapshot.cpp
	stats/HeapResizeStats.cpp
	stats/HeapStats.cpp
	stats/LargeObjectAllocateStats.cpp
	stats/MarkStats.cpp
	stats/MetronomeStats.cpp
	stats/PercolateStats.cpp
	stats/RootScannerStats.cpp
	stats/ScavengerCopyScanRatio.cpp
	stats/ScavengerHotFieldStats.cpp
	stats/ScavengerStats.cpp
	stats/SweepStats.cpp

	structs/ForwardedHeader.cpp
	structs/HashTableIterator.cpp
	structs/OMRVMThreadListIterator.cpp
	structs/ObjectHeapIterator.cpp
	structs/ObjectIteratorState.cpp
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
omr_set_target_flags(omrgc)

add_dependencies(omrgc omrgc_hookgen)

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
)

target_link_libraries(omrgc
	PUBLIC
		omrcore
		omrglue
		j9hookstatic
)
