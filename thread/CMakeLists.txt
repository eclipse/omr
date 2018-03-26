###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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

include(OmrFindFiles)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_tracegen(j9thr.tdf)


set(OBJECTS "")
set(VPATHS "")
list(APPEND OBJECTS
	j9sem.c
	omrthread.c
	omrthreadattr.c
	omrthreaddebug.c
	omrthreaderror.c
	omrthreadinspect.c
	omrthreadmem.cpp
	omrthreadnuma.c
	omrthreadpriority.c
	omrthreadtls.c
	priority.c
	thrcreate.c
	threadhelpers.cpp
	thrprof.c
	thrdsup.c
	rasthrsup.c
	rwmutex.c
)

if(OMR_THR_JLM)
	list(APPEND OBJECTS omrthreadjlm.c)
endif(OMR_THR_JLM)

if(NOT OMR_HOST_OS STREQUAL "win")
	list(APPEND OBJECTS unixpriority.c)
else()
	list(APPEND OBJECTS dllmain.c)
	list(APPEND VPATHS win32)
endif()


include_directories(.)


if(OMR_HOST_OS STREQUAL "zos")
	list(APPEND VPATHS
		zos390
		unix
	)
	include_directories(zos390 unix)
	list(APPEND OBJECTS thrcputime.s)
endif()

if(OMR_HOST_OS STREQUAL "linux")
	list(APPEND VPATHS
		linux
		unix
	)
	include_directories(linux unix)

endif()

if(OMR_HOST_OS STREQUAL "osx")
	list(APPEND VPATHS osx unix)
	include_directories(osx unix)
endif()

if(OMR_HOST_OS STREQUAL "aix")
	list(APPEND VPATHS aix unix)
	include_directories(aix unix)
endif()

list(APPEND VPATHS common)

#TODO need to add warning flags
#relevant Makefile section:
# Disable some warnings
#ifeq ($(OMR_TOOLCHAIN),gcc)
#  MODULE_CFLAGS += -Wno-unused
#endif


#TODO need to port following symbol export stuff from Makefile:
#ifeq (zos,$(OMR_HOST_OS))
#define WRITE_ZOS_THREAD_EXPORTS
#@echo omrthread_get_os_errno2 >>$@
#endef
#endif

#ifeq (1,$(OMR_THR_JLM))
#define WRITE_JLM_THREAD_EXPORTS
#@echo omrthread_jlm_init >>$@
#@echo omrthread_jlm_get_gc_lock_tracing >>$@
#endef
#endif

#ifeq (1,$(OMR_THR_ADAPTIVE_SPIN))
#define WRITE_ADAPTIVE_SPIN_THREAD_EXPORTS
#@echo jlm_adaptive_spin_init >>$@
#endef
#endif

#ifeq (1,$(OMR_THR_TRACING))
#define WRITE_TRACING_THREAD_EXPORTS
#@echo omrthread_monitor_dump_trace >>$@
#@echo omrthread_monitor_dump_all >>$@
#@echo omrthread_dump_trace >>$@
#@echo omrthread_reset_tracing >>$@
#endef
#endif

#define WRITE_COMMON_THREAD_EXPORTS
#@echo j9sem_init >>$@
#@echo j9sem_post >>$@
#@echo j9sem_wait >>$@
#@echo j9sem_destroy >>$@
#@echo omrthread_init_library >>$@
#@echo omrthread_shutdown_library >>$@
#@echo omrthread_get_os_errno >>$@
#@echo omrthread_get_errordesc >>$@
#@echo omrthread_current_stack_free >>$@
#@echo omrthread_abort >>$@
#@echo omrthread_attach >>$@
#@echo omrthread_attach_ex >>$@
#@echo omrthread_create >>$@
#@echo omrthread_create_ex >>$@
#@echo omrthread_cancel >>$@
#@echo omrthread_join >>$@
#@echo omrthread_interrupt >>$@
#@echo omrthread_clear_interrupted >>$@
#@echo omrthread_interrupted >>$@
#@echo omrthread_priority_interrupt >>$@
#@echo omrthread_clear_priority_interrupted >>$@
#@echo omrthread_priority_interrupted >>$@
#@echo omrthread_monitor_destroy >>$@
#@echo omrthread_monitor_destroy_nolock >>$@
#@echo omrthread_monitor_flush_destroyed_monitor_list >>$@
#@echo omrthread_monitor_enter >>$@
#@echo omrthread_monitor_get_name >>$@
#@echo omrthread_monitor_enter_abortable_using_threadId >>$@
#@echo omrthread_monitor_enter_using_threadId >>$@
#@echo omrthread_monitor_try_enter >>$@
#@echo omrthread_monitor_try_enter_using_threadId >>$@
#@echo omrthread_monitor_exit >>$@
#@echo omrthread_monitor_exit_using_threadId >>$@
#@echo omrthread_monitor_owned_by_self >>$@
#@echo omrthread_monitor_init_with_name >>$@
#@echo omrthread_monitor_notify >>$@
#@echo omrthread_monitor_notify_all >>$@
#@echo omrthread_monitor_wait >>$@
#@echo omrthread_monitor_wait_timed >>$@
#@echo omrthread_monitor_wait_abortable >>$@
#@echo omrthread_monitor_wait_interruptable >>$@
#@echo omrthread_monitor_num_waiting >>$@
#@echo omrthread_resume >>$@
#@echo omrthread_self >>$@
#@echo omrthread_set_priority >>$@
#@echo omrthread_get_priority >>$@
#@echo omrthread_sleep >>$@
#@echo omrthread_sleep_interruptable >>$@
#@echo omrthread_suspend >>$@
#@echo omrthread_tls_alloc >>$@
#@echo omrthread_tls_alloc_with_finalizer >>$@
#@echo omrthread_tls_free >>$@
#@echo omrthread_tls_get >>$@
#@echo omrthread_tls_set >>$@
#@echo omrthread_yield >>$@
#@echo omrthread_yield_new >>$@
#@echo omrthread_exit >>$@
#@echo omrthread_detach >>$@
#@echo omrthread_global >>$@
#@echo omrthread_global_monitor >>$@
#@echo omrthread_get_flags >>$@
#@echo omrthread_get_state >>$@
#@echo omrthread_get_osId >>$@
#@echo omrthread_get_ras_tid >>$@
#@echo omrthread_get_stack_range >>$@
#@echo omrthread_monitor_init_walk >>$@
#@echo omrthread_monitor_walk >>$@
#@echo omrthread_monitor_walk_no_locking >>$@
#@echo omrthread_rwmutex_init >>$@
#@echo omrthread_rwmutex_destroy >>$@
#@echo omrthread_rwmutex_enter_read >>$@
#@echo omrthread_rwmutex_exit_read >>$@
#@echo omrthread_rwmutex_enter_write >>$@
#@echo omrthread_rwmutex_try_enter_write >>$@
#@echo omrthread_rwmutex_exit_write >>$@
#@echo omrthread_rwmutex_is_writelocked >>$@
#@echo omrthread_park >>$@
#@echo omrthread_unpark >>$@
#@echo omrthread_numa_get_max_node >>$@
#@echo omrthread_numa_set_enabled >>$@
#@echo omrthread_numa_set_node_affinity >>$@
#@echo omrthread_numa_get_node_affinity >>$@
#@echo omrthread_map_native_priority >>$@
#@echo omrthread_set_priority_spread >>$@
#@echo omrthread_set_name >>$@

#@echo omrthread_lib_enable_cpu_monitor >>$@
#@echo omrthread_lib_lock >>$@
#@echo omrthread_lib_try_lock >>$@
#@echo omrthread_lib_unlock >>$@
#@echo omrthread_lib_get_flags >>$@
#@echo omrthread_lib_set_flags >>$@
#@echo omrthread_lib_clear_flags >>$@
#@echo omrthread_lib_control >>$@
#@echo omrthread_lib_use_realtime_scheduling >>$@

#@echo omrthread_attr_init >>$@
#@echo omrthread_attr_destroy >>$@
#@echo omrthread_attr_set_name >>$@
#@echo omrthread_attr_set_schedpolicy >>$@
#@echo omrthread_attr_set_priority >>$@
#@echo omrthread_attr_set_stacksize >>$@
#@echo omrthread_attr_set_category >>$@

#@# for builder use only
#@echo omrthread_monitor_lock >>$@
#@echo omrthread_monitor_unlock >>$@

#@echo omrthread_monitor_pin >>$@
#@echo omrthread_monitor_unpin >>$@

#@echo omrthread_nanosleep >>$@
#@echo omrthread_nanosleep_supported >>$@
#@echo omrthread_nanosleep_to >>$@

#@echo omrthread_get_user_time >>$@
#@echo omrthread_get_self_user_time >>$@
#@echo omrthread_get_cpu_time >>$@
#@echo omrthread_get_cpu_time_ex >>$@
#@echo omrthread_get_self_cpu_time >>$@
#@echo omrthread_get_process_times >>$@

#@echo omrthread_get_handle >>$@
#@echo omrthread_get_stack_size >>$@
#@echo omrthread_get_os_priority >>$@

#@echo omrthread_get_stack_usage >>$@
#@echo omrthread_enable_stack_usage >>$@

#@# process-wide statistics
#@echo omrthread_get_process_cpu_time >>$@
#@echo omrthread_get_jvm_cpu_usage_info >>$@
#@echo omrthread_get_jvm_cpu_usage_info_error_recovery >>$@
#@echo omrthread_get_category >>$@
#@echo omrthread_set_category >>$@

#@# temp for the JIT
#@echo j9thread_self >>$@
#@echo j9thread_tls_get >>$@
#endef

#define WRITE_THREAD_EXPORTS
#$(WRITE_COMMON_THREAD_EXPORTS)
#$(WRITE_ZOS_THREAD_EXPORTS)
#$(WRITE_JLM_THREAD_EXPORTS)
#$(WRITE_ADAPTIVE_SPIN_THREAD_EXPORTS)
#$(WRITE_TRACING_THREAD_EXPORTS)
#endef

omr_find_files(resolvedPaths
	PATHS ${VPATHS}
	FILES ${OBJECTS}
)

include_directories(common)
#TODO also should be able to build dynamic lib
add_library(j9thr_obj OBJECT
	${resolvedPaths}
	ut_j9thr.c
)

add_library(j9thrstatic STATIC
	$<TARGET_OBJECTS:j9thr_obj>
)

if(OMR_WARNINGS_AS_ERRORS)
	target_compile_options(j9thrstatic PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
endif()

if(OMR_ENHANCED_WARNINGS)
	target_compile_options(j9thrstatic PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
endif()

target_include_directories(j9thrstatic
	INTERFACE
	.
)

target_link_libraries(j9thrstatic
	j9pool
	omrutil
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
	target_link_libraries(j9thrstatic -pthread)
elseif(CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
	target_link_libraries(j9thrstatic pthread)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	target_link_libraries(j9thrstatic Ws2_32.lib)
elseif(CMAKE_C_COMPILER_ID STREQUAL "XL")
	target_link_libraries(j9thrstatic -lpthread)
else()
	message(FATAL_ERROR "OMR: Unknown compiler ID: '${CMAKE_C_COMPILER_ID}'")
endif()
