###############################################################################
# Copyright IBM Corp. and others 2017
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
#############################################################################

set(OMR_WARNINGS_AS_ERRORS ON CACHE BOOL "Treat compile warnings as errors")
set(OMR_ENHANCED_WARNINGS ON CACHE BOOL "Enable enhanced compiler warnings")

###
### Built-in OMR Applications
###

set(OMR_EXAMPLE ON CACHE BOOL "Enable the Example application")

###
### Major Feature Flags
###

set(OMR_TOOLS ON CACHE BOOL "Enable the native build tools")
set(OMR_DDR OFF CACHE BOOL "Enable DDR")
set(OMR_RAS_TDF_TRACE ON CACHE BOOL "Enable trace engine")
set(OMR_FVTEST ON CACHE BOOL "Enable the FV Testing.")

set(OMR_PORT ON CACHE BOOL "Enable portability library")
set(OMR_OMRSIG ON CACHE BOOL "Enable the OMR signal compatibility library")
set(OMR_THREAD ON CACHE BOOL "Enable thread library")

set(OMR_COMPILER OFF CACHE BOOL "Enable the Compiler")
set(OMR_JITBUILDER OFF CACHE BOOL "Enable building JitBuilder")
set(OMR_TEST_COMPILER OFF CACHE BOOL "Enable building the test compiler")
set(OMR_LLJB OFF CACHE BOOL "Enable building LLJB")
set(OMR_SHARED_CACHE OFF CACHE BOOL "Enable the Shared Cache")

set(OMR_GC ON CACHE BOOL "Enable the GC")
set(OMR_GC_TEST ${OMR_GC} CACHE BOOL "Enable the GC tests.")

set(OMR_USE_NATIVE_ENCODING ON CACHE BOOL
	"Indicates that runtime components should use the systems native encoding (currently only defined for z/OS)"
)
## OMR_COMPILER is required for OMR_JITBUILDER, OMR_TEST_COMPILER and OMR_LLJB
if(NOT OMR_COMPILER)
	if(OMR_JITBUILDER)
		message(FATAL_ERROR "OMR_JITBUILDER is enabled but OMR_COMPILER is not enabled")
	else()
		if(OMR_LLJB)
			message(FATAL_ERROR "OMR_LLJB is enabled but OMR_COMPILER and OMR_JITBUILDER are not enabled")
		endif()
	endif()
	if(OMR_TEST_COMPILER)
		message(FATAL_ERROR "OMR_TEST_COMPILER is enabled but OMR_COMPILER is not enabled")
	endif()
endif()

if(OMR_LLJB)
	if(NOT OMR_JITBUILDER)
		message(FATAL_ERROR "OMR_LLJB is enabled but OMR_JITBUILDER is not enabled")
	endif()
	set(OMR_LLJB_TEST ON CACHE BOOL "Enable lljb tests")
endif()

## Enable OMR_JITBUILDER_TEST if OMR_JITBUILDER is enabled.
## Do NOT force it since it is explicitly disabled on Windows for now.
if(OMR_JITBUILDER)
	set(OMR_JITBUILDER_TEST ON CACHE BOOL "")

	# Enable additional JitBuilder tests if running on a supported platform
	# (which currently means 64-bit x86 platforms except for Windows).
	if((OMR_HOST_ARCH STREQUAL "x86" AND NOT OMR_OS_WINDOWS) AND OMR_TEMP_DATA_SIZE STREQUAL "64")
		set(OMR_JITBUILDER_TEST_EXTENDED ON)
	endif()
else()
	# if JitBuilder isn't enabled, the tests can't be built
	set(OMR_JITBUILDER_TEST OFF CACHE BOOL "" FORCE)
endif()

###
### Tooling
###

set(OMR_TOOLS_IMPORTFILE "IMPORTFILE-NOTFOUND" CACHE FILEPATH
	"Point it to the ImportTools.cmake file of a native build"
)

set(OMR_TOOLS_USE_NATIVE_ENCODING ON CACHE BOOL
	"Indicates if omr tooling should use system native character encoding (currently only defined for z/OS)"
)

###
### Library names
###

set(OMR_GC_LIB "omrgc" CACHE STRING "Name of the GC library to use")
set(OMR_GC_FULL_LIB "omrgc_full" CACHE STRING "Name of the Full References GC library to use for Mixed References Mode")
set(OMR_HOOK_LIB "j9hookstatic" CACHE STRING "Name of the hook library to link against")
set(OMR_PORT_LIB "omrport" CACHE STRING "Name of the port library to link against")
set(OMR_THREAD_LIB "j9thrstatic" CACHE STRING "Name of the thread library to link against")
set(OMR_TRACE_LIB "omrtrace" CACHE STRING "Name of the trace library to link against")

###
### Glue library names
###

set(OMR_GC_GLUE_TARGET "NOTFOUND" CACHE STRING "The gc glue target, must be interface library")
set(OMR_GC_GLUE_FULL_TARGET "NOTFOUND" CACHE STRING "The gc glue full target, must be interface library")
set(OMR_UTIL_GLUE_TARGET "NOTFOUND" CACHE STRING "The util glue target, must be interface library")
set(OMR_RAS_GLUE_TARGET "NOTFOUND" CACHE STRING "The ras glue target, must be interface library")
set(OMR_CORE_GLUE_TARGET "NOTFOUND" CACHE STRING "The core glue target, must be and interface library")

###
### Boolean Feature Flags
###

# TODO: This is a pretty crazy list, can we move it to their subprojects?

set(OMR_GC_ALLOCATION_TAX ON CACHE BOOL "TODO: Document")
set(OMR_GC_API OFF CACHE BOOL "Enable a high-level GC API")
set(OMR_GC_API_TEST OFF CACHE BOOL "Enable testing for the OMR GC API")
set(OMR_GC_BATCH_CLEAR_TLH ON CACHE BOOL "TODO: Document")
set(OMR_GC_COMBINATION_SPEC ON CACHE BOOL "TODO: Document")
set(OMR_GC_DEBUG_ASSERTS ON CACHE BOOL "TODO: Document")
set(OMR_GC_EXPERIMENTAL_CONTEXT OFF CACHE BOOL "An experimental set of APIs for the GC. Off by default")
set(OMR_GC_EXPERIMENTAL_OBJECT_SCANNER OFF CACHE BOOL "An experimental object scanner glue API.")
set(OMR_GC_LARGE_OBJECT_AREA ON CACHE BOOL "TODO: Document")
set(OMR_GC_MINIMUM_OBJECT_SIZE ON CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_STANDARD ON CACHE BOOL "TODO: Document")
set(OMR_GC_NON_ZERO_TLH ON CACHE BOOL "TODO: Document")
set(OMR_GC_THREAD_LOCAL_HEAP ON CACHE BOOL "TODO: Document")
set(OMR_GC_TLH_PREFETCH_FTA OFF CACHE BOOL "TODO: Document")
set(OMR_GC_OBJECT_MAP OFF CACHE BOOL "TODO: Document")
set(OMR_GC_DYNAMIC_CLASS_UNLOADING OFF CACHE BOOL "TODO: Document")
set(OMR_GC_LEAF_BITS OFF CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_COMPACTION OFF CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_CONCURRENT_MARK OFF CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_SCAVENGER OFF CACHE BOOL "TODO: Document")
set(OMR_GC_DOUBLE_MAP_ARRAYLETS OFF CACHE BOOL "TODO: Document")
set(OMR_GC_SPARSE_HEAP_ALLOCATION ON CACHE BOOL "TODO: Document")
set(OMR_GC_CONCURRENT_SCAVENGER OFF CACHE BOOL "TODO: Document")
set(OMR_GC_CONCURRENT_SWEEP OFF CACHE BOOL "TODO: Document")
set(OMR_GC_IDLE_HEAP_MANAGER OFF CACHE BOOL "TODO: Document")
set(OMR_GC_OBJECT_ALLOCATION_NOTIFY OFF CACHE BOOL "TODO: Document")
set(OMR_GC_REALTIME OFF CACHE BOOL "TODO: Document")
set(OMR_GC_SCAN_OBJECT_GLUE OFF CACHE BOOL "Implement ScanObject in glue code, not OMR core")
set(OMR_GC_SEGREGATED_HEAP OFF CACHE BOOL "TODO: Document")
set(OMR_GC_VLHGC OFF CACHE BOOL "TODO: Document")
set(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD OFF CACHE BOOL "Enable VLHGC concurrent copy forward")
set(OMR_INTERP_HAS_SEMAPHORES ON CACHE BOOL "TODO: Document")
set(OMR_GC_POINTER_MODE "full" CACHE STRING "The mode of pointers.")
set_property(CACHE OMR_GC_POINTER_MODE PROPERTY STRINGS "full" "compressed" "mixed")
if(OMR_GC_POINTER_MODE STREQUAL "full")
	set(OMR_GC_COMPRESSED_POINTERS OFF CACHE INTERNAL "")
	set(OMR_GC_FULL_POINTERS ON CACHE INTERNAL "")
	set(OMR_MIXED_REFERENCES_MODE_STATIC OFF CACHE INTERNAL "")
elseif(OMR_GC_POINTER_MODE STREQUAL "compressed")
	omr_assert(FATAL_ERROR TEST NOT OMR_ENV_DATA32 MESSAGE "OMR_GC_POINTER_MODE must be \"full\" on 32 bit platforms")
	set(OMR_GC_COMPRESSED_POINTERS ON CACHE INTERNAL "")
	set(OMR_GC_FULL_POINTERS OFF CACHE INTERNAL "")
	set(OMR_MIXED_REFERENCES_MODE_STATIC OFF CACHE INTERNAL "")
elseif(OMR_GC_POINTER_MODE STREQUAL "mixed")
	omr_assert(FATAL_ERROR TEST NOT OMR_ENV_DATA32 MESSAGE "OMR_GC_POINTER_MODE must be \"full\" on 32 bit platforms")
	set(OMR_GC_COMPRESSED_POINTERS ON CACHE INTERNAL "")
	set(OMR_GC_FULL_POINTERS ON CACHE INTERNAL "")
	if(OMR_MIXED_REFERENCES_MODE STREQUAL "static")
		set(OMR_MIXED_REFERENCES_MODE_STATIC ON CACHE INTERNAL "")
	else()
		set(OMR_MIXED_REFERENCES_MODE_STATIC OFF CACHE INTERNAL "")
	endif()
else()
	message(FATAL_ERROR "OMR_GC_FULL_POINTERS must be set to one of \"full\", \"compressed\", or \"mixed\"")
endif()

set(OMR_THR_ADAPTIVE_SPIN ON CACHE BOOL "TODO: Document")
set(OMR_THR_JLM ON CACHE BOOL "TODO: Document")
set(OMR_THR_JLM_HOLD_TIMES ON CACHE BOOL "TODO: Document")
set(OMR_THR_LOCK_NURSERY ON CACHE BOOL "TODO: Document")
set(OMR_THR_FORK_SUPPORT OFF CACHE BOOL "TODO: Document")
set(OMR_THR_THREE_TIER_LOCKING OFF CACHE BOOL "TODO: Document")
set(OMR_THR_CUSTOM_SPIN_OPTIONS OFF CACHE BOOL "TODO: Document")
set(OMR_THR_SPIN_WAKE_CONTROL OFF CACHE BOOL "TODO: Document")
set(OMR_THR_YIELD_ALG OFF CACHE BOOL "TODO: Document")
if(OMR_THR_YIELD_ALG)
	omr_assert(FATAL_ERROR
		TEST OMR_OS_LINUX OR OMR_OS_OSX OR OMR_OS_AIX
		MESSAGE "OMR_THR_YIELD_ALG enabled, but not supported on current platform"
	)
endif()
# TODO set to disabled. Stuff fails to compile when its on
set(OMR_THR_MCS_LOCKS OFF CACHE BOOL "Enable the usage of the MCS lock in the OMR thread monitor.")

#TODO this should maybe be a OMRTHREAD_LIB string variable?
set(OMRTHREAD_WIN32_DEFAULT OFF)
set(OMRTHREAD_UNIX_DEFAULT OFF)
set(OMRTHREAD_AIX_DEFAULT OFF)
set(OMRTHREAD_ZOS_DEFAULT OFF)

if(OMR_OS_WINDOWS)
	set(OMRTHREAD_WIN32_DEFAULT ON)
elseif(OMR_OS_AIX)
	set(OMRTHREAD_AIX_DEFAULT ON)
elseif(OMR_OS_ZOS)
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
set(OMRPORT_OMRSIG_SUPPORT OFF CACHE BOOL "TODO: Document")
set(OMR_PORT_ASYNC_HANDLER OFF CACHE BOOL "TODO: Document")

set(OMR_NOTIFY_POLICY_CONTROL OFF CACHE BOOL "TODO: Document")

set(OMR_OPT_CUDA OFF CACHE BOOL "Enable CUDA support in OMR. See also: OMR_CUDA_HOME in FindOmrCuda.cmake")

set(OMR_SANITIZE OFF CACHE STRING "Sanitizer selection. Only has an effect on GNU or Clang")

if(OMR_OS_WINDOWS)
	set(OMR_WINDOWS_NOMINMAX ON CACHE BOOL "Define NOMINMAX in every compilation unit; prevents Windows headers from polluting the global namespace with 'min' and 'max' macros.")
endif()

set(OMR_SEPARATE_DEBUG_INFO OFF CACHE BOOL "Maintain debug info in a separate file")
set(OMR_FLATTEN_DEBUG_INFO OFF CACHE BOOL "Enable generation of flattened debug info on osx")
if(OMR_OS_OSX)
	set(default_debug_ext ".dSYM")
else()
	set(default_debug_ext ".debuginfo")
endif()
set(OMR_DEBUG_INFO_OUTPUT_EXTENSION "${default_debug_ext}" CACHE STRING "Extension to use for separate debug info")
