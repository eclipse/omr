/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
 * @brief Sockets
 */

#include "omrcfg.h"
#if defined(OMR_PORT_SOCKET_SUPPORT)
#include "omrsock.h"

#include <netdb.h>
#include <string.h> 
#include <unistd.h>
#include <fcntl.h>

#include "omrport.h"
#include "omrporterror.h"
#include "omrsockptb.h"

/* Internal: OMRSOCK user interface constants TO OS dependent constants mapping. */

/**
 * @internal Map OMRSOCK API user interface address family names to the OS address 
 * family, which may be defined differently depending on operating system. 
 *
 * @param omrFamily The OMR address family to be converted.
 *
 * @return OS address family, or OS_SOCK_AF_UNSPEC if none exists.
 */
static int32_t
get_os_family(int32_t omrFamily)
{
	switch(omrFamily)
	{
		case OMRSOCK_AF_INET:
			return OS_SOCK_AF_INET;
		case OMRSOCK_AF_INET6:
			return OS_SOCK_AF_INET6;
		default:
			break;
	}
	return OS_SOCK_AF_UNSPEC;
}

/**
 * @internal Map OMRSOCK API user interface socket type to the OS socket type, 
 * which may be defined differently depending on operating system. 
 *
 * @param omrSockType The OMR socket type to be converted.
 *
 * @return OS socket type on success, or OS_SOCK_ANY if none exists.
 */
static int32_t
get_os_socktype(int32_t omrSockType)
{
	switch(omrSockType)
	{
		case OMRSOCK_STREAM:
			return OS_SOCK_STREAM;
		case OMRSOCK_DGRAM:
			return OS_SOCK_DGRAM;
		default:
			break;
	}
	return OS_SOCK_ANY;
}

/**
 * @internal Map OMRSOCK API user interface protocol to the OS protocols, 
 * which may be defined differently depending on operating system. 
 *
 * @param omrProtocol The OMR protocol to be converted.
 *
 * @return OS protocol on success, or OS_SOCK_IPPROTO_DEFAULT 
 * if none exists.
 */
static int32_t
get_os_protocol(int32_t omrProtocol)
{
	switch(omrProtocol)
	{
		case OMRSOCK_IPPROTO_TCP:
			return OS_SOCK_IPPROTO_TCP;
		case OMRSOCK_IPPROTO_UDP:
			return OS_SOCK_IPPROTO_UDP;
		default:
			break;
	}
	return OS_SOCK_IPPROTO_DEFAULT;
}

/* Internal: OS dependent constants TO OMRSOCK user interface constants mapping. */

/**
 * @internal Map OS address family to OMRSOCK API user interface address
 * family names. 
 *
 * @param osFamily The OS address family to be converted.
 *
 * @return OMR address family, or OMRSOCK_AF_UNSPEC if none exists.

 */
static int32_t
get_omr_family(int32_t osFamily)
{
	switch(osFamily)
	{
		case OS_SOCK_AF_INET:
			return OMRSOCK_AF_INET;
		case OS_SOCK_AF_INET6:
			return OMRSOCK_AF_INET6;
		default:
			break;
	}
	return OMRSOCK_AF_UNSPEC;
}

/**
 * @internal Map OS socket type to OMRSOCK API user interface socket 
 * types.
 *
 * @param osSockType The OS socket type to be converted.
 *
 * @return OMR socket type on success, or OMRSOCK_ANY if none exists.
 */
static int32_t
get_omr_socktype(int32_t osSockType)
{
	switch(osSockType)
	{
		case OMRSOCK_STREAM:
			return OS_SOCK_STREAM;
		case OMRSOCK_DGRAM:
			return OS_SOCK_DGRAM;
		default:
			break;
	}
	return OMRSOCK_ANY;
}

/**
 * @internal Map OS protocol to OMRSOCK API user interface protocol.  
 *
 * @param osProtocol The OS protocol to be converted.
 *
 * @return OMRSOCK user interface protocol on success, or 
 * OMRSOCK_IPPROTO_DEFAULT if none exists.
 */
static int32_t
get_omr_protocol(int32_t osProtocol)
{
	switch(osProtocol)
	{
		case OS_SOCK_IPPROTO_TCP:
			return OMRSOCK_IPPROTO_TCP;
		case OS_SOCK_IPPROTO_UDP:
			return OMRSOCK_IPPROTO_UDP;
		default:
			break;
	}
	return OMRSOCK_IPPROTO_DEFAULT;
}

int32_t
omrsock_startup(struct OMRPortLibrary *portLibrary)
{
	return omrsock_ptb_init(portLibrary);
}

int32_t
omrsock_getaddrinfo_create_hints(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t *hints, int32_t family, int32_t socktype, int32_t protocol, int32_t flags)
{
	*hints = NULL;
	omrsock_ptb_t ptBuffer = NULL;
	omr_os_addrinfo *ptbHints = NULL;

	/* Initialized the pt buffers if necessary */
	ptBuffer = omrsock_ptb_get(portLibrary);
	if (NULL == ptBuffer) {
		return OMRPORT_ERROR_SOCK_PTB_FAILED;
	}
    
	ptbHints = (ptBuffer->addrInfoHints).addrInfo;
	if (NULL == ptbHints) {
		ptbHints = portLibrary->mem_allocate_memory(portLibrary, sizeof(omr_os_addrinfo), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == ptbHints) {
			return OMRPORT_ERROR_SOCK_SYSTEM_FULL;
		}
	}
	memset(ptbHints, 0, sizeof(omr_os_addrinfo));

	ptbHints->ai_flags = flags;
	ptbHints->ai_family = get_os_family(family);
	ptbHints->ai_socktype = get_os_socktype(socktype);
	ptbHints->ai_protocol = get_os_protocol(protocol);

	ptBuffer->addrInfoHints.addrInfo = ptbHints;
	ptBuffer->addrInfoHints.length = 1;
	*hints = &ptBuffer->addrInfoHints;
	return 0;
}

int32_t
omrsock_getaddrinfo(struct OMRPortLibrary *portLibrary, const char *node, const char *service, omrsock_addrinfo_t hints, omrsock_addrinfo_t result)
{
	omr_os_addrinfo *addrInfoResults = NULL;
	omr_os_addrinfo *addrInfoHints = NULL;
	uint32_t count = 0;
	
	if (NULL == result) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
	
	memset(result, 0, sizeof(struct OMRAddrInfoNode));

	if (NULL != hints) {
		addrInfoHints = (omr_os_addrinfo *)hints->addrInfo;
	}

	if (0 != getaddrinfo(node, service, addrInfoHints, &addrInfoResults)) {
		return OMRPORT_ERROR_SOCK_ADDRINFO_FAILED;
	}

	result->addrInfo = addrInfoResults;

	/* There is at least one addrinfo on successful call. Account the first entry. */
	count = 1;
	
	while (NULL != addrInfoResults->ai_next) {
		count++;
		addrInfoResults = addrInfoResults->ai_next;
	}
	result->length = count;
	
	return 0;
}

int32_t
omrsock_addrinfo_length(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t *result)
{
	if (NULL == handle) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	*result = handle->length;
	return 0;
}

int32_t
omrsock_addrinfo_family(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, int32_t *result)
{	
	omr_os_addrinfo *info = NULL;
	uint32_t i = 0;

	if ((NULL == handle) || (NULL == handle->addrInfo) || (index >= handle->length)) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	info = handle->addrInfo;

	for (i = 0; i < index; i++) {
		info = info->ai_next;
		if (NULL == info) {
			return OMRPORT_ERROR_INVALID_ARGUMENTS;
		}
	}

	*result = get_omr_family(info->ai_family);
	return 0;
}

int32_t
omrsock_addrinfo_socktype(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, int32_t *result)
{
	omr_os_addrinfo *info = NULL;
	uint32_t i = 0;

	if ((NULL == handle) || (NULL == handle->addrInfo) || (index >= handle->length)) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	info = handle->addrInfo;

	for (i = 0; i < index; i++) {
		info = info->ai_next;
		if (NULL == info) {
			return OMRPORT_ERROR_INVALID_ARGUMENTS;
		}
	}

	*result = get_omr_socktype(info->ai_socktype);
	return 0;
}

int32_t
omrsock_addrinfo_protocol(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, int32_t *result)
{
	omr_os_addrinfo *info = NULL;
	uint32_t i = 0;

	if ((NULL == handle) || (NULL == handle->addrInfo) || (index >= handle->length)) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	info = handle->addrInfo;

	for (i = 0; i < index; i++) {
		info = info->ai_next;
		if (NULL == info) {
			return OMRPORT_ERROR_INVALID_ARGUMENTS;
		}
	}

	*result = get_omr_protocol(info->ai_protocol);
	return 0;
}

int32_t
omrsock_freeaddrinfo(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle)
{
	if (NULL == handle) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
	
	freeaddrinfo((omr_os_addrinfo *)handle->addrInfo);

	handle->addrInfo = NULL;
	handle->length = 0;

	return 0;
}

int32_t
omrsock_socket(struct OMRPortLibrary *portLibrary, omrsock_socket_t *sock, int32_t family, int32_t socktype, int32_t protocol)
{
	int32_t sockDescriptor = 0;
	int32_t osFamily = get_os_family(family);
	int32_t osSockType = get_os_socktype(socktype);
	int32_t osProtocol = get_os_protocol(protocol);
	
	/* Initialize return omrsock_socket_t to NULL. */
	*sock = NULL;

	if ((osFamily < 0) || (osSockType < 0) || (osProtocol < 0)) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	sockDescriptor = socket(osFamily, osSockType, osProtocol);

	if (sockDescriptor < 0) {
		return OMRPORT_ERROR_SOCK_SOCKET_CREATION_FAILED;
	}

	/* Tag this descriptor as being non-inheritable. */
	int32_t fdflags = fcntl(sockDescriptor, F_GETFD, 0);
	fcntl(sockDescriptor, F_SETFD, fdflags | FD_CLOEXEC);

	/* Set up the socket structure. */
	*sock = (omrsock_socket_t)portLibrary->mem_allocate_memory(portLibrary, sizeof(struct OMRSocket), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == *sock) {
		close(sockDescriptor);
		return OMRPORT_ERROR_SOCK_SYSTEM_FULL; 
	}

	memset(*sock, 0, sizeof(struct OMRSocket));
	(*sock)->data = sockDescriptor;

	return 0;
}

int32_t
omrsock_bind(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_listen(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t backlog)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_connect(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_accept(struct OMRPortLibrary *portLibrary, omrsock_socket_t serverSock, omrsock_sockaddr_t addrHandle, omrsock_socket_t *sockHandle)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_send(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_sendto(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_recv(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_recvfrom(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

int32_t
omrsock_close(struct OMRPortLibrary *portLibrary, omrsock_socket_t *sock)
{
	if (NULL == *sock) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	if(0 != close((*sock)->data)) {
		return OMRPORT_ERROR_SOCK_SOCKET_CLOSE_FAILED;
	}
	portLibrary->mem_free_memory(portLibrary, *sock);
	*sock = NULL;

	return 0;
}

int32_t
omrsock_shutdown(struct OMRPortLibrary *portLibrary)
{
	return omrsock_ptb_shutdown(portLibrary);
}

#endif /* defined(OMR_PORT_SOCKET_SUPPORT) */
