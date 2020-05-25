/*******************************************************************************
 * Copyright (c) 2015, 2020 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief Port Library
 */
#include <string.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "ut_omrport.h"

/*
 * Create the port library initialization structure
 */

static OMRPortLibrary MasterPortLibraryTable = {
	NULL, /* portGlobals */
	omrport_shutdown_library, /* port_shutdown_library */
	omrport_isFunctionOverridden, /* port_isFunctionOverridden */
	omrport_tls_free, /* port_tls_free */
	omrerror_startup, /* error_startup */
	omrerror_shutdown, /* error_shutdown */
	omrerror_last_error_number, /* error_last_error_number */
	omrerror_last_error_message, /* error_last_error_message */
	omrerror_set_last_error, /* error_set_last_error */
	omrerror_set_last_error_with_message, /* error_set_last_error_with_message */
	omrerror_set_last_error_with_message_format, /* error_set_last_error_with_message_format */
	omrtime_startup, /* time_startup */
	omrtime_shutdown, /* time_shutdown */
	omrtime_msec_clock, /* time_msec_clock */
	omrtime_usec_clock, /* time_usec_clock */
	omrtime_current_time_nanos, /* time_current_time_nanos */
	omrtime_current_time_millis, /* time_current_time_millis */
	omrtime_nano_time, /* time_nano_time */
	omrtime_hires_clock, /* time_hires_clock */
	omrtime_hires_frequency, /* time_hires_frequency */
	omrtime_hires_delta, /* time_hires_delta */
	omrsysinfo_startup, /* sysinfo_startup */
	omrsysinfo_shutdown, /* sysinfo_shutdown */
	omrsysinfo_process_exists, /* sysinfo_process_exists */
	omrsysinfo_get_egid, /* sysinfo_get_egid */
	omrsysinfo_get_euid, /* sysinfo_get_euid */
	omrsysinfo_get_groups, /* sysinfo_get_groups */
	omrsysinfo_get_pid, /* sysinfo_get_pid */
	omrsysinfo_get_ppid, /* sysinfo_get_ppid */
	omrsysinfo_get_memory_info, /* sysinfo_get_memory_info */
	omrsysinfo_get_processor_info, /* sysinfo_get_processor_info */
	omrsysinfo_destroy_processor_info, /* sysinfo_destroy_processor_info */
	omrsysinfo_get_addressable_physical_memory, /* sysinfo_get_addressable_physical_memory */
	omrsysinfo_get_physical_memory, /* sysinfo_get_physical_memory */
	omrsysinfo_get_OS_version, /* sysinfo_get_OS_version */
	omrsysinfo_get_env, /* sysinfo_get_env */
	omrsysinfo_get_CPU_architecture, /* sysinfo_get_CPU_architecture */
	omrsysinfo_get_processor_description, /* omrsysinfo_get_processor_description */
	omrsysinfo_processor_has_feature, /* omrsysinfo_processor_has_feature */
	omrsysinfo_processor_set_feature, /* omrsysinfo_processor_set_feature */
	omrsysinfo_get_OS_type, /* sysinfo_get_OS_type */
	omrsysinfo_get_executable_name, /* sysinfo_get_executable_name */
	omrsysinfo_get_username, /* sysinfo_get_username */
	omrsysinfo_get_groupname, /* sysinfo_get_groupname */
	omrsysinfo_get_hostname, /* sysinfo_get_hostname */
	omrsysinfo_get_load_average, /* sysinfo_get_load_average */
	omrsysinfo_get_CPU_utilization, /* omrsysinfo_get_CPU_utilization */
	omrsysinfo_get_CPU_load, /* omrsysinfo_get_CPU_load */
	omrsysinfo_limit_iterator_init, /* sysinfo_limit_iterator_next */
	omrsysinfo_limit_iterator_hasNext, /* sysinfo_limit_iterator_hasNext */
	omrsysinfo_limit_iterator_next, /* sysinfo_limit_iterator_next */
	omrsysinfo_env_iterator_init, /* sysinfo_env_iterator_init */
	omrsysinfo_env_iterator_hasNext, /* sysinfo_env_iterator_hasNext */
	omrsysinfo_env_iterator_next, /* sysinfo_env_iterator_next */
	omrfile_startup, /* file_startup */
	omrfile_shutdown, /* file_shutdown */
	omrfile_write, /* file_write */
	omrfile_write_text, /* file_write_text */
	omrfile_get_text_encoding, /* file_get_text_encoding */
	omrfile_vprintf, /* file_vprintf */
	omrfile_printf, /* file_printf */
	omrfile_open, /* file_open */
	omrfile_close, /* file_close */
	omrfile_seek, /* file_seek */
	omrfile_read, /* file_read */
	omrfile_unlink, /* file_unlink */
	omrfile_attr, /* file_attr */
	omrfile_chmod, /* file_chmod */
	omrfile_chown, /* file_chown */
	omrfile_lastmod, /* file_lastmod */
	omrfile_length, /* file_length */
	omrfile_flength, /* file_flength */
	omrfile_set_length, /* file_set_length */
	omrfile_sync, /* file_sync */
	omrfile_fstat, /* file_fstat */
	omrfile_stat, /* file_stat */
	omrfile_stat_filesystem, /* file_stat_filesystem */
	omrfile_blockingasync_open, /* file_blockingasync_open */
	omrfile_blockingasync_close, /* file_blockingasync_close */
	omrfile_blockingasync_read, /* file_blockingasync_read */
	omrfile_blockingasync_write, /* file_blockingasync_write */
	omrfile_blockingasync_set_length, /* file_blockingasync_set_length */
	omrfile_blockingasync_flength, /* file_blockingasync_flength */
	omrfile_blockingasync_startup, /* file_blockingasync_startup */
	omrfile_blockingasync_shutdown, /* file_blockingasync_shutdown */
	omrfilestream_startup, /* filestream_startup */
	omrfilestream_shutdown, /* filestream_shutdown */
	omrfilestream_open, /* filestream_open */
	omrfilestream_close, /* filestream_close */
	omrfilestream_write, /* filestream_write */
	omrfilestream_write_text, /* filestream_write_text */
	omrfilestream_vprintf, /* filestream_vprintf */
	omrfilestream_printf, /* filestream_printf */
	omrfilestream_sync, /* filestream_sync */
	omrfilestream_setbuffer, /* filestream_setbuffer */
	omrfilestream_fdopen, /* filestream_fdopen */
	omrfilestream_fileno, /* filestream_fileno */
	omrsl_startup, /* sl_startup */
	omrsl_shutdown, /* sl_shutdown */
	omrsl_close_shared_library, /* sl_close_shared_library */
	omrsl_open_shared_library, /* sl_open_shared_library */
	omrsl_lookup_name, /* sl_lookup_name */
	omrtty_startup, /* tty_startup */
	omrtty_shutdown, /* tty_shutdown */
	omrtty_printf, /* tty_printf */
	omrtty_vprintf, /* tty_vprintf */
	omrtty_get_chars, /* tty_get_chars */
	omrtty_err_printf, /* tty_err_printf */
	omrtty_err_vprintf, /* tty_err_vprintf */
	omrtty_available, /* tty_available */
	omrtty_daemonize, /* tty_daemonize */
	omrheap_create, /* heap_create */
	omrheap_allocate, /* heap_allocate */
	omrheap_free, /* heap_free */
	omrheap_reallocate, /* heap_reallocate */
	omrmem_startup, /* mem_startup */
	omrmem_shutdown, /* mem_shutdown */
	omrmem_allocate_memory, /* mem_allocate_memory */
	omrmem_free_memory, /* mem_free_memory */
	omrmem_advise_and_free_memory, /* mem_advise_and_free_memory */
	omrmem_reallocate_memory, /* mem_reallocate_memory */
#if defined(OMR_ENV_DATA64)
	omrmem_allocate_memory32, /* mem_allocate_memory32 */
	omrmem_free_memory32, /* mem_free_memory32 */
#else
	omrmem_allocate_memory, /* mem_allocate_memory */
	omrmem_free_memory, /* mem_free_memory */
#endif
	omrmem_ensure_capacity32, /* mem_ensure_capacity32 */
	omrcpu_startup, /* cpu_startup */
	omrcpu_shutdown, /* cpu_shutdown */
	omrcpu_flush_icache, /* cpu_flush_icache */
	omrcpu_get_cache_line_size, /* cpu_get_cache_line_size */
	omrvmem_startup, /* vmem_startup */
	omrvmem_shutdown, /* vmem_shutdown */
	omrvmem_commit_memory, /* vmem_commit_memory */
	omrvmem_decommit_memory, /* vmem_decommit_memory */
	omrvmem_free_memory, /* vmem_free_memory */
	omrvmem_vmem_params_init, /* vmem_vmem_params_init */
	omrvmem_reserve_memory, /* vmem_reserve_memory */
	omrvmem_reserve_memory_ex, /* vmem_reserve_memory_ex */
	omrvmem_get_contiguous_region_memory, /* vmem_get_contiguous_region_memory */
	omrvmem_get_page_size, /* vmem_get_page_size */
	omrvmem_get_page_flags, /* omrvmem_get_page_flags */
	omrvmem_supported_page_sizes, /* vmem_supported_page_sizes */
	omrvmem_supported_page_flags, /* vmem_supported_page_flags */
	omrvmem_default_large_page_size_ex, /* vmem_default_large_page_size_ex */
	omrvmem_find_valid_page_size, /* vmem_find_valid_page_size */
	omrvmem_numa_set_affinity, /* vmem_numa_set_affinity */
	omrvmem_numa_get_node_details, /* vmem_numa_get_node_details */
	omrvmem_get_available_physical_memory, /* vmem_get_available_physical_memory */
	omrvmem_get_process_memory_size, /* vmem_get_process_memory_size */
	omrstr_startup, /* str_startup */
	omrstr_shutdown, /* str_shutdown */
	omrstr_printf, /* str_printf */
	omrstr_vprintf, /* str_vprintf */
	omrstr_create_tokens, /* str_create_token */
	omrstr_set_token, /* str_set_token */
	omrstr_subst_tokens, /* str_subst_tokens */
	omrstr_free_tokens, /* str_free_tokens */
	omrstr_set_time_tokens, /* str_set_time_tokens */
	omrstr_convert, /* str_convert */
	omrexit_startup, /* exit_startup */
	omrexit_shutdown, /* exit_shutdown */
	omrexit_get_exit_code, /* exit_get_exit_code */
	omrexit_shutdown_and_exit, /* exit_shutdown_and_exit */
	NULL, /* self_handle */
	omrdump_create, /* dump_create */
	omrdump_startup, /* dump_startup */
	omrdump_shutdown, /* dump_shutdown */
	j9nls_startup, /* nls_startup */
	j9nls_free_cached_data, /* nls_free_cached_data */
	j9nls_shutdown, /* nls_shutdown */
	j9nls_set_catalog, /* nls_set_catalog */
	j9nls_set_locale, /* nls_set_locale */
	j9nls_get_language, /* nls_get_language */
	j9nls_get_region, /* nls_get_region */
	j9nls_get_variant, /* nls_get_variant */
	j9nls_printf, /* nls_printf */
	j9nls_vprintf, /* nls_vprintf */
	j9nls_lookup_message, /* nls_lookup_message */
	omrport_control, /* port_control */
	omrsig_startup, /* sig_startup*/
	omrsig_shutdown, /* sig_shutdown */
	omrsig_protect,  /* sig_protect */
	omrsig_can_protect, /* sig_can_protect */
	omrsig_set_async_signal_handler, /* sig_set_async_signal_handler */
	omrsig_set_single_async_signal_handler, /* sig_set_single_async_signal_handler */
	omrsig_map_os_signal_to_portlib_signal, /* sig_map_os_signal_to_portlib_signal */
	omrsig_map_portlib_signal_to_os_signal, /* sig_map_portlib_signal_to_os_signal */
	omrsig_register_os_handler, /* sig_register_os_handler */
	omrsig_is_master_signal_handler, /* sig_is_master_signal_handler */
	omrsig_is_signal_ignored, /* sig_is_signal_ignored */
	omrsig_info, /* sig_info */
	omrsig_info_count, /* sig_info_count */
	omrsig_set_options, /* sig_set_options */
	omrsig_get_options, /* sig_get_options */
	omrsig_get_current_signal, /* sig_get_current_signal */
	omrsig_set_reporter_priority, /* sig_set_reporter_priority */
	omrfile_read_text, /* file_read_text */
	omrfile_mkdir, /* file_mkdir */
	omrfile_move, /* file_move */
	omrfile_unlinkdir, /* file_unlinkdir */
	omrfile_findfirst, /* file_findfirst */
	omrfile_findnext, /* file_findnext */
	omrfile_findclose, /* file_findclose */
	omrfile_error_message, /* file_error_message */
	omrfile_unlock_bytes, /* file_unlock_bytes */
	omrfile_lock_bytes, /* file_lock_bytes */
	omrfile_convert_native_fd_to_omrfile_fd, /* file_convert_native_fd_to_omrfile_fd */
	omrfile_convert_omrfile_fd_to_native_fd,
	omrfile_blockingasync_unlock_bytes, /* file_blockingasync_unlock_bytes */
	omrfile_blockingasync_lock_bytes, /* file_blockingasync_lock_bytes */
	omrstr_ftime, /* str_ftime */
	omrmmap_startup, /* mmap_startup */
	omrmmap_shutdown, /* mmap_shutdown */
	omrmmap_capabilities, /* mmap_capabilities */
	omrmmap_map_file, /* mmap_map_file */
	omrmmap_unmap_file, /* mmap_unmap_file */
	omrmmap_msync, /* mmap_msync */
	omrmmap_protect, /* mmap_protect */
	omrmmap_get_region_granularity, /* mmap_get_region_granularity */
	omrmmap_dont_need, /* mmap_dont_need */
	omrsysinfo_get_limit, /* sysinfo_get_limit */
	omrsysinfo_set_limit, /* sysinfo_set_limit */
	omrsysinfo_get_number_CPUs_by_type, /* sysinfo_get_number_CPUs_by_type */
	omrsysinfo_get_cwd, /* sysinfo_get_cwd */
	omrsysinfo_get_tmp, /* sysinfo_get_tmp */
	omrsysinfo_set_number_user_specified_CPUs, /* sysinfo_set_number_user_specified_CPUs */
	omrsysinfo_get_open_file_count, /* sysinfo_get_open_file_count */
	omrsysinfo_get_os_description, /* sysinfo_get_os_description */
	omrsysinfo_os_has_feature, /* sysinfo_os_has_feature */
	omrsysinfo_os_kernel_info, /* sysinfo_os_kernel_info */
	omrsysinfo_cgroup_is_system_available, /* sysinfo_cgroup_is_system_available */
	omrsysinfo_cgroup_get_available_subsystems, /* sysinfo_cgroup_get_available_subsystems */
	omrsysinfo_cgroup_are_subsystems_available, /* sysinfo_cgroup_are_subsystems_available */
	omrsysinfo_cgroup_get_enabled_subsystems, /* sysinfo_cgroup_get_enabled_subsystems */
	omrsysinfo_cgroup_enable_subsystems, /* sysinfo_cgroup_enable_subsystems */
	omrsysinfo_cgroup_are_subsystems_enabled, /* sysinfo_cgroup_are_subsystems_enabled */
	omrsysinfo_cgroup_get_memlimit, /* sysinfo_cgroup_get_memlimit */
	omrsysinfo_cgroup_is_memlimit_set, /* sysinfo_cgroup_is_memlimit_set */
	omrsysinfo_get_cgroup_subsystem_list, /* sysinfo_get_cgroup_entry_list */
	omrsysinfo_is_running_in_container, /* sysinfo_is_running_in_container */
	omrsysinfo_cgroup_subsystem_iterator_init, /* sysinfo_cgroup_subsystem_iterator_init */
	omrsysinfo_cgroup_subsystem_iterator_hasNext, /* sysinfo_cgroup_subsystem_iterator_hasNext */
	omrsysinfo_cgroup_subsystem_iterator_metricKey, /* sysinfo_cgroup_subsystem_iterator_metricKey */
	omrsysinfo_cgroup_subsystem_iterator_next, /* sysinfo_cgroup_subsystem_iterator_next */
	omrsysinfo_cgroup_subsystem_iterator_destroy, /* sysinfo_cgroup_subsystem_iterator_destroy */
	omrport_init_library, /* port_init_library */
	omrport_startup_library, /* port_startup_library */
	omrport_create_library, /* port_create_library */
	omrsyslog_write, /* syslog_write */
	omrintrospect_startup, /* introspect_startup */
	omrintrospect_shutdown, /* introspect_shutdown */
	omrintrospect_set_suspend_signal_offset, /* introspect_set_suspend_signal_offset */
	omrintrospect_threads_startDo, /* introspect_threads_startDo */
	omrintrospect_threads_startDo_with_signal, /* introspect_threads_startDo_with_signal */
	omrintrospect_threads_nextDo, /* introspect_threads_nextDo */
	omrintrospect_backtrace_thread, /* introspect_backtrace_thread */
	omrintrospect_backtrace_symbols, /* introspect_backtrace_symbols */
	omrsyslog_query, /* syslog_query */
	omrsyslog_set, /* syslog_set */
	omrmem_walk_categories, /* mem_walk_categories */
	omrmem_get_category, /* mem_get_category */
	omrmem_categories_increment_counters, /* mem_categories_increment_counters */
	omrmem_categories_decrement_counters, /* mem_categories_decrement_counters */
	omrheap_query_size, /* heap_query_size */
	omrheap_grow, /* heap_grow*/
#if defined(OMR_PORT_SOCKET_SUPPORT)
	omrsock_startup, /* sock_startup */
	omrsock_getaddrinfo_create_hints, /* sock_getaddrinfo_create_hints */
	omrsock_getaddrinfo, /* sock_getaddrinfo */
	omrsock_addrinfo_length, /* sock_addrinfo_length */
	omrsock_addrinfo_family, /* sock_addrinfo_family */
	omrsock_addrinfo_socktype, /* sock_addrinfo_socktype */
	omrsock_addrinfo_protocol, /* sock_addrinfo_protocol */
	omrsock_addrinfo_address, /* sock_addrinfo_address */
	omrsock_freeaddrinfo, /* sock_freeaddrinfo */
	omrsock_sockaddr_init, /* sock_sockaddr_init */
	omrsock_sockaddr_init6, /* omrsock_sockaddr_init6 */
	omrsock_socket, /* sock_socket */
	omrsock_bind, /* sock_bind */
	omrsock_listen, /* sock_listen */
	omrsock_connect, /* sock_connect */
	omrsock_accept, /* sock_accept */
	omrsock_send, /* sock_send */
	omrsock_sendto, /* sock_sendto */
	omrsock_recv, /* sock_recv */
	omrsock_recvfrom, /* sock_recvfrom */
	omrsock_close, /* sock_close */
	omrsock_shutdown, /* sock_shutdown */
	omrsock_htons, /* sock_htons */
	omrsock_htonl, /* sock_htonl */
	omrsock_inet_pton, /* sock_inet_pton */
#endif /* defined(OMR_PORT_SOCKET_SUPPORT) */
#if defined(OMR_OPT_CUDA)
	NULL, /* cuda_configData */
	omrcuda_startup, /* cuda_startup */
	omrcuda_shutdown, /* cuda_shutdown */
	omrcuda_deviceAlloc, /* cuda_deviceAlloc */
	omrcuda_deviceCanAccessPeer, /* cuda_deviceCanAccessPeer */
	omrcuda_deviceDisablePeerAccess, /* cuda_deviceDisablePeerAccess */
	omrcuda_deviceEnablePeerAccess, /* cuda_deviceEnablePeerAccess */
	omrcuda_deviceFree, /* cuda_deviceFree */
	omrcuda_deviceGetAttribute, /* cuda_deviceGetAttribute */
	omrcuda_deviceGetCacheConfig, /* cuda_deviceGetCacheConfig */
	omrcuda_deviceGetCount, /* cuda_deviceGetCount */
	omrcuda_deviceGetLimit, /* cuda_deviceGetLimit */
	omrcuda_deviceGetMemInfo, /* cuda_deviceGetMemInfo */
	omrcuda_deviceGetName, /* cuda_deviceGetName */
	omrcuda_deviceGetSharedMemConfig, /* cuda_deviceGetSharedMemConfig */
	omrcuda_deviceGetStreamPriorityRange, /* cuda_deviceGetStreamPriorityRange */
	omrcuda_deviceReset, /* cuda_deviceReset */
	omrcuda_deviceSetCacheConfig, /* cuda_deviceSetCacheConfig */
	omrcuda_deviceSetLimit, /* cuda_deviceSetLimit */
	omrcuda_deviceSetSharedMemConfig, /* cuda_deviceSetSharedMemConfig */
	omrcuda_deviceSynchronize, /* cuda_deviceSynchronize */
	omrcuda_driverGetVersion, /* cuda_driverGetVersion */
	omrcuda_eventCreate, /* cuda_eventCreate */
	omrcuda_eventDestroy, /* cuda_eventDestroy */
	omrcuda_eventElapsedTime, /* cuda_eventElapsedTime */
	omrcuda_eventQuery, /* cuda_eventQuery */
	omrcuda_eventRecord, /* cuda_eventRecord */
	omrcuda_eventSynchronize, /* cuda_eventSynchronize */
	omrcuda_funcGetAttribute, /* cuda_funcGetAttribute */
	omrcuda_funcMaxActiveBlocksPerMultiprocessor, /* cuda_funcMaxActiveBlocksPerMultiprocessor */
	omrcuda_funcMaxPotentialBlockSize, /* cuda_funcMaxPotentialBlockSize */
	omrcuda_funcSetCacheConfig, /* cuda_funcSetCacheConfig */
	omrcuda_funcSetSharedMemConfig, /* cuda_funcSetSharedMemConfig */
	omrcuda_getErrorString, /* cuda_getErrorString */
	omrcuda_hostAlloc, /* cuda_hostAlloc */
	omrcuda_hostFree, /* cuda_hostFree */
	omrcuda_launchKernel, /* cuda_launchKernel */
	omrcuda_linkerAddData, /* cuda_linkerAddData */
	omrcuda_linkerComplete, /* cuda_linkerComplete */
	omrcuda_linkerCreate, /* cuda_linkerCreate */
	omrcuda_linkerDestroy, /* cuda_linkerDestroy */
	omrcuda_memcpy2DDeviceToDevice, /* cuda_memcpy2DDeviceToDevice */
	omrcuda_memcpy2DDeviceToDeviceAsync, /* cuda_memcpy2DDeviceToDeviceAsync */
	omrcuda_memcpy2DDeviceToHost, /* cuda_memcpy2DDeviceToHost */
	omrcuda_memcpy2DDeviceToHostAsync, /* cuda_memcpy2DDeviceToHostAsync */
	omrcuda_memcpy2DHostToDevice, /* cuda_memcpy2DHostToDevice */
	omrcuda_memcpy2DHostToDeviceAsync, /* cuda_memcpy2DHostToDeviceAsync */
	omrcuda_memcpyDeviceToDevice, /* cuda_memcpyDeviceToDevice */
	omrcuda_memcpyDeviceToDeviceAsync, /* cuda_memcpyDeviceToDeviceAsync */
	omrcuda_memcpyDeviceToHost, /* cuda_memcpyDeviceToHost */
	omrcuda_memcpyDeviceToHostAsync, /* cuda_memcpyDeviceToHostAsync */
	omrcuda_memcpyHostToDevice, /* cuda_memcpyHostToDevice */
	omrcuda_memcpyHostToDeviceAsync, /* cuda_memcpyHostToDeviceAsync */
	omrcuda_memcpyPeer, /* cuda_memcpyPeer */
	omrcuda_memcpyPeerAsync, /* cuda_memcpyPeerAsync */
	omrcuda_memset8Async, /* cuda_memset8Async */
	omrcuda_memset16Async, /* cuda_memset16Async */
	omrcuda_memset32Async, /* cuda_memset32Async */
	omrcuda_moduleGetFunction, /* cuda_moduleGetFunction */
	omrcuda_moduleGetGlobal, /* cuda_moduleGetGlobal */
	omrcuda_moduleGetSurfaceRef, /* cuda_moduleGetSurfaceRef */
	omrcuda_moduleGetTextureRef, /* cuda_moduleGetTextureRef */
	omrcuda_moduleLoad, /* cuda_moduleLoad */
	omrcuda_moduleUnload, /* cuda_moduleUnload */
	omrcuda_runtimeGetVersion, /* cuda_runtimeGetVersion */
	omrcuda_streamAddCallback, /* cuda_streamAddCallback */
	omrcuda_streamCreate, /* cuda_streamCreate */
	omrcuda_streamCreateWithPriority, /* cuda_streamCreateWithPriority */
	omrcuda_streamDestroy, /* cuda_streamDestroy */
	omrcuda_streamGetFlags, /* cuda_streamGetFlags */
	omrcuda_streamGetPriority, /* cuda_streamGetPriority */
	omrcuda_streamQuery, /* cuda_streamQuery */
	omrcuda_streamSynchronize, /* cuda_streamSynchronize */
	omrcuda_streamWaitEvent, /* cuda_streamWaitEvent */
#endif /* OMR_OPT_CUDA */
};

/**
 * Initialize the port library.
 *
 * Given a pointer to a port library, populate the port
 * library table with the appropriate functions and then
 * call the startup function for the port library.
 *
 * @param[in] portLibrary The port library.
 * @param[in] size Size of the port library.
 *
 * @return 0 on success, negative return value on failure
 */
int32_t
omrport_init_library(struct OMRPortLibrary *portLibrary, uintptr_t size)
{
	int32_t rc = omrport_create_library(portLibrary, size);
	if (0 == rc) {
		rc = omrport_startup_library(portLibrary);
	}
	return rc;
}

/**
 * PortLibrary shutdown.
 *
 * Shutdown the port library, de-allocate resources required by the components of the portlibrary.
 * Any resources that were created by @ref omrport_startup_library should be destroyed here.
 *
 * @param[in] portLibrary The portlibrary.
 *
 * @return 0 on success, negative return code on failure
 */
int32_t
omrport_shutdown_library(struct OMRPortLibrary *portLibrary)
{
	/* Ensure that the current thread is attached.
	 * If it is not, there will be a crash trying to using
	 * thread functions in the following shutdown functions.
	 */
	omrthread_t attached_thread = NULL;
	intptr_t rc = omrthread_attach_ex(&attached_thread, J9THREAD_ATTR_DEFAULT);
	if (0 != rc) {
		return (int32_t)rc;
	}

#if defined(OMR_OPT_CUDA)
	portLibrary->cuda_shutdown(portLibrary);
#endif /* OMR_OPT_CUDA */
#if defined(OMR_PORT_SOCKET_SUPPORT)
	portLibrary->sock_shutdown(portLibrary);
#endif /* defined(OMR_PORT_SOCKET_SUPPORT) */
	portLibrary->introspect_shutdown(portLibrary);
	portLibrary->sig_shutdown(portLibrary);
	portLibrary->str_shutdown(portLibrary);
	/* vmem shutdown now requires sl support.*/
	portLibrary->vmem_shutdown(portLibrary);
	portLibrary->sl_shutdown(portLibrary);
	portLibrary->sysinfo_shutdown(portLibrary);
	portLibrary->exit_shutdown(portLibrary);
	portLibrary->dump_shutdown(portLibrary);
	portLibrary->time_shutdown(portLibrary);

	/* Shutdown the socket library before the tty, so it can write to the tty if required */
	portLibrary->nls_shutdown(portLibrary);
	portLibrary->mmap_shutdown(portLibrary);
	portLibrary->tty_shutdown(portLibrary);
	portLibrary->file_shutdown(portLibrary);
	portLibrary->file_blockingasync_shutdown(portLibrary);

	portLibrary->cpu_shutdown(portLibrary);
	portLibrary->error_shutdown(portLibrary);
	omrport_tls_shutdown(portLibrary);

	portLibrary->mem_shutdown(portLibrary);

	omrthread_detach(attached_thread);

	/* Last thing to do.  If this port library was self allocated free this memory */
	if (NULL != portLibrary->self_handle) {
		omrmem_deallocate_portLibrary(portLibrary);
	}

	return (int32_t)0;
}

/**
 * Create the port library.
 *
 * Given a pointer to a port library, populate the port library
 * table with the appropriate functions.
 *
 * @param[in] portLibrary The port library.
 * @param[in] size Size of the port library.
 *
 * @return 0 on success, negative return value on failure
 */

int32_t
omrport_create_library(struct OMRPortLibrary *portLibrary, uintptr_t size)
{
	uintptr_t portSize = omrport_getSize();

	if (portSize > size) {
		return OMRPORT_ERROR_INIT_OMR_WRONG_SIZE;
	}

	/* Null and initialize the table passed in */
	memset(portLibrary, 0, size);
	memcpy(portLibrary, &MasterPortLibraryTable, portSize);

	return 0;
}

/**
 * PortLibrary startup.
 *
 * Start the port library, allocate resources required by the components of the portlibrary.
 * All resources created here should be destroyed in @ref omrport_shutdown_library.
 *
 * @param[in] portLibrary The portlibrary.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_THREAD
 * \arg OMRPORT_ERROR_STARTUP_MEM
 * \arg OMRPORT_ERROR_STARTUP_TLS
 * \arg OMRPORT_ERROR_STARTUP_TLS_ALLOC
 * \arg OMRPORT_ERROR_STARTUP_TLS_MUTEX
 * \arg OMRPORT_ERROR_STARTUP_ERROR
 * \arg OMRPORT_ERROR_STARTUP_CPU
 * \arg OMRPORT_ERROR_STARTUP_VMEM
 * \arg OMRPORT_ERROR_STARTUP_FILE
 * \arg OMRPORT_ERROR_STARTUP_TTY
 * \arg OMRPORT_ERROR_STARTUP_TTY_HANDLE
 * \arg OMRPORT_ERROR_STARTUP_TTY_CONSOLE
 * \arg OMRPORT_ERROR_STARTUP_MMAP
 * \arg OMRPORT_ERROR_STARTUP_IPCMUTEX
 * \arg OMRPORT_ERROR_STARTUP_NLS
 * \arg OMRPORT_ERROR_STARTUP_SOCK
 * \arg OMRPORT_ERROR_STARTUP_TIME
 * \arg OMRPORT_ERROR_STARTUP_GP
 * \arg OMRPORT_ERROR_STARTUP_EXIT
 * \arg OMRPORT_ERROR_STARTUP_SYSINFO
 * \arg OMRPORT_ERROR_STARTUP_SL
 * \arg OMRPORT_ERROR_STARTUP_STR
 * \arg OMRPORT_ERROR_STARTUP_SHSEM
 * \arg OMRPORT_ERROR_STARTUP_SHMEM
 * \arg OMRPORT_ERROR_STARTUP_SIGNAL
 *
 * @note The port library memory is deallocated if it was created by @ref omrport_allocate_library
 */
int32_t
omrport_startup_library(struct OMRPortLibrary *portLibrary)
{
	int32_t rc = 0;

	Assert_PRT_true(omrthread_self() != NULL);

	/* Must not access anything in portGlobals, as this allocates them */
	rc = portLibrary->mem_startup(portLibrary, sizeof(OMRPortLibraryGlobalData));
	if (0 != rc) {
		goto cleanup;
	}

	/* Create the tls buffers as early as possible */
	rc = omrport_tls_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	/* Start error handling as early as possible, require TLS buffers */
	rc = portLibrary->error_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->cpu_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->file_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->file_blockingasync_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->tty_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->mmap_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->nls_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}


	rc = portLibrary->time_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->exit_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->sysinfo_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->sl_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->dump_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	/* vmem startup now requires sl support.*/
	rc = portLibrary->vmem_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->str_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->sig_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

	rc = portLibrary->introspect_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}

#if defined(OMR_PORT_SOCKET_SUPPORT)
	rc = portLibrary->sock_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}
#endif /* defined(OMR_PORT_SOCKET_SUPPORT) */

#if defined(OMR_OPT_CUDA)
	rc = portLibrary->cuda_startup(portLibrary);
	if (0 != rc) {
		goto cleanup;
	}
#endif /* OMR_OPT_CUDA */

	return rc;

cleanup:
	/* TODO: should call shutdown, but need to make all shutdown functions
	 *  safe if the corresponding startup was not called.  No worse than the existing
	 * code
	 */

	/* If this port library was self allocated free this memory */
	if (NULL != portLibrary->self_handle) {
		omrmem_deallocate_portLibrary(portLibrary);
	}
	return rc;
}

/**
 * Determine the size of the port library.
 *
 * Return the size of the structure in bytes required to be allocated.
 *
 * @return size of port library on success, zero on failure
 *
 * @note The portlibrary version must be compatible with the portlibrary that which we are compiled against
 */
uintptr_t
omrport_getSize(void)
{
	return sizeof(OMRPortLibrary);
}

/**
 * Query the port library.
 *
 * Given a pointer to the port library and an offset into the table determine if
 * the function at that offset has been overridden from the default value expected by J9.
 *
 * @param[in] portLibrary The port library.
 * @param[in] offset The offset of the function to be queried.
 *
 * @return 1 if the function is overriden, else 0.
 *
 * omrport_isFunctionOverridden(portLibrary, offsetof(OMRPortLibrary, mem_allocate_memory));
 */
int32_t
omrport_isFunctionOverridden(struct OMRPortLibrary *portLibrary, uintptr_t offset)
{
	uintptr_t requiredSize;

	requiredSize = omrport_getSize();
	if (requiredSize < offset) {
		return 0;
	}

	return *((uintptr_t *)&(((uint8_t *)portLibrary)[offset])) != *((uintptr_t *)&(((uint8_t *)&MasterPortLibraryTable)[offset]));
}
/**
 * Allocate a port library.
 *
 * Given a pointer to the port library, allocate and initialize the structure.
 * The startup function is not called (@ref omrport_startup_library) allowing the application to override
 * any functions they desire.  In the event @ref omrport_startup_library fails when called by the application
 * the port library memory will be freed.
 *
 * @param[out] portLibrary Pointer to the allocated port library table.
 *
 * @return 0 on success, negative return value on failure
 *
 * @note portLibrary will be NULL on failure
 * @note @ref omrport_shutdown_library will deallocate this memory as part of regular shutdown
 */
int32_t
omrport_allocate_library(struct OMRPortLibrary **portLibrary)
{
	uintptr_t size = omrport_getSize();
	OMRPortLibrary *portLib = NULL;
	int32_t rc;

	/* Allocate the memory */
	*portLibrary = NULL;
	if (0 == size) {
		return -1;
	} else {
		portLib = omrmem_allocate_portLibrary(size);
		if (NULL == portLib) {
			return -1;
		}
	}

	/* Initialize with default values */
	rc = omrport_create_library(portLib, size);
	if (0 == rc) {
		/* Record this was self allocated */
		portLib->self_handle = portLib;
		*portLibrary = portLib;
	} else {
		omrmem_deallocate_portLibrary(portLib);
	}
	return rc;
}
