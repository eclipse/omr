###############################################################################
# Copyright (c) 2019, 2019 IBM Corp. and others
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

omr_add_exports(j9thr_obj
	j9sem_init
	j9sem_post
	j9sem_wait
	j9sem_destroy
	omrthread_init_library
	omrthread_shutdown_library
	omrthread_get_os_errno
	omrthread_get_errordesc
	omrthread_current_stack_free
	omrthread_abort
	omrthread_attach
	omrthread_attach_ex
	omrthread_create
	omrthread_create_ex
	omrthread_cancel
	omrthread_join
	omrthread_interrupt
	omrthread_clear_interrupted
	omrthread_interrupted
	omrthread_priority_interrupt
	omrthread_clear_priority_interrupted
	omrthread_priority_interrupted
	omrthread_monitor_destroy
	omrthread_monitor_destroy_nolock
	omrthread_monitor_flush_destroyed_monitor_list
	omrthread_monitor_enter
	omrthread_monitor_get_name
	omrthread_monitor_enter_abortable_using_threadId
	omrthread_monitor_enter_using_threadId
	omrthread_monitor_try_enter
	omrthread_monitor_try_enter_using_threadId
	omrthread_monitor_exit
	omrthread_monitor_exit_using_threadId
	omrthread_monitor_owned_by_self
	omrthread_monitor_init_with_name
	omrthread_monitor_notify
	omrthread_monitor_notify_all
	omrthread_monitor_wait
	omrthread_monitor_wait_timed
	omrthread_monitor_wait_abortable
	omrthread_monitor_wait_interruptable
	omrthread_monitor_num_waiting
	omrthread_resume
	omrthread_self
	omrthread_set_priority
	omrthread_get_priority
	omrthread_sleep
	omrthread_sleep_interruptable
	omrthread_suspend
	omrthread_tls_alloc
	omrthread_tls_alloc_with_finalizer
	omrthread_tls_free
	omrthread_tls_get
	omrthread_tls_set
	omrthread_yield
	omrthread_yield_new
	omrthread_exit
	omrthread_detach
	omrthread_global
	omrthread_global_monitor
	omrthread_get_flags
	omrthread_get_state
	omrthread_get_osId
	omrthread_get_ras_tid
	omrthread_get_stack_range
	omrthread_monitor_init_walk
	omrthread_monitor_walk
	omrthread_monitor_walk_no_locking
	omrthread_rwmutex_init
	omrthread_rwmutex_destroy
	omrthread_rwmutex_enter_read
	omrthread_rwmutex_exit_read
	omrthread_rwmutex_enter_write
	omrthread_rwmutex_try_enter_write
	omrthread_rwmutex_exit_write
	omrthread_rwmutex_is_writelocked
	omrthread_park
	omrthread_unpark
	omrthread_numa_get_max_node
	omrthread_numa_set_enabled
	omrthread_numa_set_node_affinity
	omrthread_numa_get_node_affinity
	omrthread_map_native_priority
	omrthread_set_priority_spread
	omrthread_set_name

	omrthread_lib_enable_cpu_monitor
	omrthread_lib_lock
	omrthread_lib_try_lock
	omrthread_lib_unlock
	omrthread_lib_get_flags
	omrthread_lib_set_flags
	omrthread_lib_clear_flags
	omrthread_lib_control
	omrthread_lib_use_realtime_scheduling

	omrthread_attr_init
	omrthread_attr_destroy
	omrthread_attr_set_name
	omrthread_attr_set_schedpolicy
	omrthread_attr_set_priority
	omrthread_attr_set_stacksize
	omrthread_attr_set_category
	omrthread_attr_set_detachstate

	# for builder use only
	omrthread_monitor_lock
	omrthread_monitor_unlock

	omrthread_monitor_pin
	omrthread_monitor_unpin

	omrthread_nanosleep
	omrthread_nanosleep_supported
	omrthread_nanosleep_to

	omrthread_get_user_time
	omrthread_get_self_user_time
	omrthread_get_cpu_time
	omrthread_get_cpu_time_ex
	omrthread_get_self_cpu_time
	omrthread_get_process_times

	omrthread_get_handle
	omrthread_get_stack_size
	omrthread_get_os_priority

	omrthread_get_stack_usage
	omrthread_enable_stack_usage

	# process-wide statistics
	omrthread_get_process_cpu_time
	omrthread_get_jvm_cpu_usage_info
	omrthread_get_jvm_cpu_usage_info_error_recovery
	omrthread_get_category
	omrthread_set_category

	# temp for the JIT
	j9thread_self
	j9thread_tls_get
)

if(OMR_OS_ZOS)
	omr_add_exports(j9thr_obj
		omrthread_get_os_errno2
	)
endif()

if(OMR_THR_JLM)
	omr_add_exports(j9thr_obj
		omrthread_jlm_init
		omrthread_jlm_get_gc_lock_tracing
	)
endif()

if(OMR_THR_ADAPTIVE_SPIN)
	omr_add_exports(j9thr_obj
		jlm_adaptive_spin_init
	)
endif()


if(OMR_THR_TRACING)
	omr_add_exports(j9thr_obj
		omrthread_monitor_dump_trace
		omrthread_monitor_dump_all
		omrthread_dump_trace
		omrthread_reset_tracing
	)
endif()

# also apply the exports to j9thrstatic
get_target_property(thread_exports j9thr_obj EXPORTED_SYMBOLS)
omr_add_exports(j9thrstatic ${thread_exports})
