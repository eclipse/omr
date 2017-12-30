###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
#############################################################################

set(OMR_WARNINGS_AS_ERRORS ON CACHE BOOL "Treat compile warnings as errors")
set(OMR_ENHANCED_WARNINGS ON CACHE BOOL "Enable enhanced compiler warnings")

###
### Major Feature Flags
###

set(OMR_COMPILER OFF CACHE BOOL "Enable the compiler")
set(OMR_DDR OFF CACHE BOOL "Enable DDR")
set(OMR_FVTEST OFF CACHE BOOL "Enable the FV Testing.")
set(OMR_GC ON CACHE BOOL "Enable the GC")
set(OMR_JITBUILDER OFF CACHE BOOL "Enable building JitBuilder")
set(OMR_OMRSIG ON CACHE BOOL "Enable the OMR signal compatibility library")
set(OMR_PORT ON CACHE BOOL "Enable portability library")
set(OMR_TEST_COMPILER OFF CACHE BOOL "Enable building the test compiler")
set(OMR_THREAD ON CACHE BOOL "Enable thread library")
set(OMR_TOOLS ON CACHE BOOL "Enable the native build tools")

###
### Tooling
###

set(OMR_TOOLS_IMPORTFILE "IMPORTFILE-NOTFOUND" CACHE FILEPATH
	"Point it to the ImportTools.cmake file of a native build"
)

###
### Library names
###

set(OMR_GC_LIB "omrgc" CACHE STRING "Name of the GC library to use")
set(OMR_HOOK_LIB "j9hookstatic" CACHE STRING "Name of the hook library to link against")
set(OMR_PORT_LIB "omrport" CACHE STRING "Name of the port library to link against")
set(OMR_THREAD_LIB "j9thrstatic" CACHE STRING "Name of the thread library to link against")
set(OMR_TRACE_LIB "omrtrace" CACHE STRING "Name of the trace library to link against")

###
### Boolean Feature Flags
###

# TODO: This is a pretty crazy list, can we move it to their subprojects?

set(OMR_GC_ALLOCATION_TAX ON CACHE BOOL "TODO: Document")
set(OMR_GC_ARRAYLETS ON CACHE BOOL "TODO: Document")
set(OMR_GC_BATCH_CLEAR_TLH ON CACHE BOOL "TODO: Document")
set(OMR_GC_COMBINATION_SPEC ON CACHE BOOL "TODO: Document")
set(OMR_GC_DEBUG_ASSERTS ON CACHE BOOL "TODO: Document")
set(OMR_GC_HEAP_CARD_TABLE ON CACHE BOOL "TODO: Document")
set(OMR_GC_LARGE_OBJECT_AREA ON CACHE BOOL "TODO: Document")
set(OMR_GC_MINIMUM_OBJECT_SIZE ON CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_STANDARD ON CACHE BOOL "TODO: Document")
set(OMR_GC_NON_ZERO_TLH ON CACHE BOOL "TODO: Document")
set(OMR_GC_THREAD_LOCAL_HEAP ON CACHE BOOL "TODO: Document")
set(OMR_GC_COMPRESSED_POINTERS OFF CACHE BOOL "TODO: Document")
set(OMR_GC_TLH_PREFETCH_FTA OFF CACHE BOOL "TODO: Document")
set(OMR_GC_OBJECT_MAP OFF CACHE BOOL "TODO: Document")
set(OMR_GC_DYNAMIC_CLASS_UNLOADING OFF CACHE BOOL "TODO: Document")
set(OMR_GC_LEAF_BITS OFF CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_COMPACTION OFF CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_CONCURRENT_MARK OFF CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_SCAVENGER OFF CACHE BOOL "TODO: Document")
set(OMR_GC_CONCURRENT_SCAVENGER OFF CACHE BOOL "TODO: Document")
set(OMR_GC_CONCURRENT_SWEEP OFF CACHE BOOL "TODO: Document")
set(OMR_GC_HYBRID_ARRAYLETS OFF CACHE BOOL "TODO: Document")
set(OMR_GC_IDLE_HEAP_MANAGER OFF CACHE BOOL "TODO: Document")
set(OMR_GC_OBJECT_ALLOCATION_NOTIFY OFF CACHE BOOL "TODO: Document")
set(OMR_GC_REALTIME OFF CACHE BOOL "TODO: Document")
set(OMR_GC_SEGREGATED_HEAP OFF CACHE BOOL "TODO: Document")
set(OMR_GC_STACCATO OFF CACHE BOOL "TODO: Document")
set(OMR_GC_VLHGC OFF CACHE BOOL "TODO: Document")

set(OMR_INTERP_HAS_SEMAPHORES ON CACHE BOOL "TODO: Document")
set(OMR_INTERP_COMPRESSED_OBJECT_HEADER OFF CACHE BOOL "TODO: Document")
set(OMR_INTERP_SMALL_MONITOR_SLOT OFF CACHE BOOL "TODO: Document")

set(OMR_RAS_TDF_TRACE ON CACHE BOOL "TODO: Document")

set(OMR_THR_ADAPTIVE_SPIN ON CACHE BOOL "TODO: Document")
set(OMR_THR_JLM_HOLD_TIMES ON CACHE BOOL "TODO: Document")
set(OMR_THR_LOCK_NURSERY ON CACHE BOOL "TODO: Document")
set(OMR_THR_FORK_SUPPORT OFF CACHE BOOL "TODO: Document")
set(OMR_THR_THREE_TIER_LOCKING OFF CACHE BOOL "TODO: Document")
set(OMR_THR_CUSTOM_SPIN_OPTIONS OFF CACHE BOOL "TODO: Document")
set(OMR_THR_SPIN_WAKE_CONTROL OFF CACHE BOOL "TODO: Document")
set(OMR_THR_YIELD_ALG OFF CACHE BOOL "TODO: Document")
#TODO set to disabled. Stuff fails to compile when its on
set(OMR_THR_TRACING OFF CACHE BOOL "TODO: Document")

#TODO this should maybe be a OMRTHREAD_LIB string variable?
set(OMRTHREAD_WIN32_DEFAULT OFF)
set(OMRTHREAD_UNIX_DEFAULT OFF)
set(OMRTHREAD_AIX_DEFAULT OFF)
set(OMRTHREAD_ZOS_DEFAULT OFF)

if(OMR_HOST_OS STREQUAL "win")
	set(OMRTHREAD_WIN32_DEFAULT ON)
elseif(OMR_HOST_OS STREQUAL "aix")
	set(OMRTHREAD_AIX_DEFAULT ON)
elseif(OMR_HOST_OS STREQUAL "zos")
	set(OMRTHREAD_ZOS_DEFAULT ON)
else()
	set(OMRTHREAD_UNIX_DEFAULT ON)
endif()

set(OMRTHREAD_LIB_AIX ${OMRTHREAD_AIX_DEFAULT} CACHE BOOL "TODO: Document")
set(OMRTHREAD_LIB_ZOS ${OMRTHREAD_ZOS_DEFAULT} CACHE BOOL "TODO: Document")
set(OMRTHREAD_LIB_WIN32 ${OMRTHREAD_WIN32_DEFAULT} CACHE BOOL "TODO: Document")
set(OMRTHREAD_LIB_UNIX ${OMRTHREAD_UNIX_DEFAULT} CACHE BOOL "TODO: Document")

set(OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS ON CACHE BOOL "TODO: Document")
set(OMR_PORT_NUMA_SUPPORT OFF CACHE BOOL "TODO: Document")
set(OMR_PORT_ALLOCATE_TOP_DOWN OFF CACHE BOOL "TODO: Document")
set(OMR_PORT_ZOS_CEEHDLRSUPPORT OFF CACHE BOOL "TODO: Document")
set(OMR_PORT_ASYNC_HANDLER OFF CACHE BOOL "TODO: Document")


set(OMR_NOTIFY_POLICY_CONTROL OFF CACHE BOOL "TODO: Document")

set(OMR_ENV_GCC OFF CACHE BOOL "TODO: Document")


set(OMR_OPT_CUDA OFF CACHE BOOL "TODO: Document")

set(OMR_SANITIZE OFF CACHE STRING "Sanitizer selection. Only has an effect on GNU or Clang") 
