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
#include "omrsock.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <string.h> 
#include <unistd.h>
#include <fcntl.h>

#include "omrport.h"
#include "omrporterror.h"
#include "omrsockptb.h"

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
#include "atoe.h"
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

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
	switch (omrFamily) {
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
	/* Mask sockType. It should be defined in this range. */
	omrSockType &= 0x00FF;

	switch (omrSockType) {
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
	switch (omrProtocol) {
	case OMRSOCK_IPPROTO_TCP:
		return OS_SOCK_IPPROTO_TCP;
	case OMRSOCK_IPPROTO_UDP:
		return OS_SOCK_IPPROTO_UDP;
	default:
		break;
	}
	return OS_SOCK_IPPROTO_DEFAULT;
}

/**
 * @internal Map OMRSOCK API user interface socket level to
 * OS socket level. Used to resolve the arguments of socket 
 * option functions. 
 * 
 * Socket Levels currently supported are:
 * \arg OMRSOCK_SOL_SOCKET, for most options.
 * \arg OMRSOCK_IPPROTO_TCP, for the TCP noDelay option.
 *
 * @param[in] socketLevel The OMR socket level to be converted.
 *
 * @return OS protocol on success, or OS_SOL_SOCKET if none exists. 
 */
static int32_t
get_os_socket_level(int32_t socketLevel)
{
	switch (socketLevel) {
	case OMRSOCK_SOL_SOCKET:
		return OS_SOL_SOCKET;
	case OMRSOCK_IPPROTO_TCP:
		return OS_SOCK_IPPROTO_TCP;
	default:
		break;
	}
	return OMRPORT_ERROR_SOCK_LEVEL_UNSUPPORTED;
}

/**
 * @internal Map OMRSOCK API user interface socket option to OS socket option.
 * Used to resolve the arguments of socket option functions.
 * Options currently in supported are:
 * \arg SO_KEEPALIVE, the keep-alive messages are enabled.
 * \arg SO_LINGER, the linger timeout.
 * \arg SO_REUSEADDR, the reusage of local address are allowed in bind.
 * \arg SO_RCVTIMEO, the receive timeout.
 * \arg SO_SNDTIMEO, the send timeout.
 * \arg TCP_NODELAY, the buffering scheme disabling Nagle's algorithm.
 *
 * @param[in] socketOption The portable socket option to convert.
 *
 * @return OS Socket Option on success, or OS_SOL_SOCKET if none exists. 
 */
static int32_t
get_os_socket_option(int32_t socketOption)
{
	switch (socketOption) {
	case OMRSOCK_SO_KEEPALIVE:
		return OS_SO_KEEPALIVE;
	case OMRSOCK_SO_LINGER:
		return OS_SO_LINGER;
	case OMRSOCK_SO_REUSEADDR:
		return OS_SO_REUSEADDR;
	case OMRSOCK_SO_RCVTIMEO:
		return OS_SO_RCVTIMEO;
	case OMRSOCK_SO_SNDTIMEO:
		return OS_SO_SNDTIMEO;
	case OMRSOCK_TCP_NODELAY:
		return OS_TCP_NODELAY;
	default:
		break;
	}
	return OMRPORT_ERROR_SOCK_OPTION_UNSUPPORTED;
}

/**
 * @internal Map OMRSOCK API user interface socket flag to the OS socket,
 * flag which may be defined differently depending on operating system.
 *
 * @param omrProtocol The OMR flag to be converted.
 *
 * @return OS socket flag on success, or 0 if none exists.
 */
static int32_t
get_os_socket_flag(int32_t omrFlag)
{
	/* Mask flag. It should be defined in this range. */
	omrFlag &= 0xFF00;

	switch (omrFlag) {
	case OMRSOCK_O_ASYNC:
		return OS_O_ASYNC;
	case OMRSOCK_O_NONBLOCK:
		return OS_O_NONBLOCK;
	default:
		break;
	}
	return 0;
}

/**
 * @internal Map OMRSOCK API user interface poll constant to the OS poll
 * constant which may be defined differently depending on operating system. 
 *
 * @param omrPollConstant The OMR poll constant to be converted.
 *
 * @return OS socket flag on success, or 0 if none exists.
 */
static int16_t
get_os_poll_constant(int16_t omrPollConstant)
{
	int16_t osPollConstant = 0;

	if (OMR_ARE_ANY_BITS_SET(omrPollConstant, OMRSOCK_POLLIN)) {
		osPollConstant |= OS_POLLIN;
	}
	if (OMR_ARE_ANY_BITS_SET(omrPollConstant, OMRSOCK_POLLOUT)) {
		osPollConstant |= OS_POLLOUT;
	}
#if !defined(AIXPPC)
	if (OMR_ARE_ANY_BITS_SET(omrPollConstant, OMRSOCK_POLLERR)) {
		osPollConstant |= OS_POLLERR;
	}
	if (OMR_ARE_ANY_BITS_SET(omrPollConstant, OMRSOCK_POLLNVAL)) {
		osPollConstant |= OS_POLLNVAL;
	}
	if (OMR_ARE_ANY_BITS_SET(omrPollConstant, OMRSOCK_POLLHUP)) {
		osPollConstant |= OS_POLLHUP;
	}
#endif /* !defined(AIXPPC) */

	return osPollConstant;
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
	switch (osFamily) {
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
	switch (osSockType) {
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
	switch (osProtocol) {
	case OS_SOCK_IPPROTO_TCP:
		return OMRSOCK_IPPROTO_TCP;
	case OS_SOCK_IPPROTO_UDP:
		return OMRSOCK_IPPROTO_UDP;
	default:
		break;
	}
	return OMRSOCK_IPPROTO_DEFAULT;
}

/**
 * @internal Map OS poll constant to the OMRSOCK API user interface poll constant.
 *
 * @param omrPollConstant The OMR poll constant to be converted.
 *
 * @return OS poll constant on success, or 0 if none exists.
 */
static int16_t
get_omr_poll_constant(int16_t osPollConstant)
{
	int16_t omrPollConstant = 0;

	if (OMR_ARE_ANY_BITS_SET(osPollConstant, OS_POLLIN)) {
		omrPollConstant |= OMRSOCK_POLLIN;
	}
	if (OMR_ARE_ANY_BITS_SET(osPollConstant, OS_POLLOUT)) {
		omrPollConstant |= OMRSOCK_POLLOUT;
	}
	if (OMR_ARE_ANY_BITS_SET(osPollConstant, OS_POLLERR)) {
		omrPollConstant |= OMRSOCK_POLLERR;
	}
	if (OMR_ARE_ANY_BITS_SET(osPollConstant, OS_POLLNVAL)) {
		omrPollConstant |= OMRSOCK_POLLNVAL;
	}
#if !defined(AIXPPC)
	if (OMR_ARE_ANY_BITS_SET(osPollConstant, OS_POLLHUP)) {
		omrPollConstant |= OMRSOCK_POLLHUP;
	}
#endif /* !defined(AIXPPC) */

	return omrPollConstant;
}

/**
 * @internal
 * Determine the proper omrsock error code to return given a errno error code.
 *
 * @param[in] osErrorCode The errno error code reported by the OS.
 *
 * @return omrsock error code.
 */
static int32_t
get_omr_error(int32_t osErrorCode)
{
	switch (osErrorCode) {
	case EACCES:
		return OMRPORT_ERROR_SOCKET_NO_ACCESS;
	case EADDRINUSE:
		return OMRPORT_ERROR_SOCKET_ADDRINUSE;
	case EADDRNOTAVAIL:
		return OMRPORT_ERROR_SOCKET_ADDRNOTAVAIL;
	case EAFNOSUPPORT:
		return OMRPORT_ERROR_SOCKET_AF_UNSUPPORTED;
	case EAGAIN:
#if defined(J9ZOS390)
	case EWOULDBLOCK:
#endif
		return OMRPORT_ERROR_SOCKET_WOULDBLOCK;
	case EALREADY:
	case EINPROGRESS:
		return OMRPORT_ERROR_SOCKET_INPROGRESS;
	case EBADF:
		return OMRPORT_ERROR_SOCKET_BAD_FILE_DESCRIPTOR;
	case ECONNABORTED:
		return OMRPORT_ERROR_SOCKET_CONNABORTED;
	case ECONNREFUSED:
		return OMRPORT_ERROR_SOCKET_CONNREFUSED;
	case ECONNRESET:
		return OMRPORT_ERROR_SOCKET_CONNRESET;
	case EDESTADDRREQ:
		return OMRPORT_ERROR_SOCKET_DESTADDRREQ;
	case EDOM:
		return OMRPORT_ERROR_SOCKET_DOMAIN;
	case EFAULT:
	case EISDIR:
		return OMRPORT_ERROR_SOCKET_BAD_ADDRESS;
	case EINTR:
		return OMRPORT_ERROR_SOCKET_INTERRUPTED;
	case EINVAL:
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	case EIO:
	case ENXIO:
		return OMRPORT_ERROR_SOCKET_IO;
	case EISCONN:
		return OMRPORT_ERROR_SOCKET_ALREADY_CONNECTED;
	case ELOOP:
		return OMRPORT_ERROR_SOCKET_PATH_NAME_LOOP;
	case EMSGSIZE:
		return OMRPORT_ERROR_SOCKET_MSGSIZE;
	case EMFILE:
		return OMRPORT_ERROR_SOCKET_MAX_FILE_OPENED_PROCESS;
	case ENAMETOOLONG:
		return OMRPORT_ERROR_SOCKET_NAMETOOLONG;
	case ENETDOWN:
		return OMRPORT_ERROR_SOCKET_NETDOWN;
	case ENETUNREACH:
		return OMRPORT_ERROR_SOCKET_NETUNREACH;
	case ENFILE:
		return OMRPORT_ERROR_SOCKET_MAX_FILE_OPENED_SYSTEM;
	case ENOBUFS:
		return OMRPORT_ERROR_SOCKET_NO_BUFFERS;
	case ENODEV:
		return OMRPORT_ERROR_SOCKET_NO_DEVICE;
	case ENOENT:
		return OMRPORT_ERROR_SOCKET_NO_FILE_ENTRY;
	case ENOMEM:
	case ENOSPC:
		return OMRPORT_ERROR_SOCKET_NOMEM;
	case ENOSYS:
		return OMRPORT_ERROR_SOCKET_SYSTEM_FUNCTION_NOT_IMPLEMENTED;
	case ENOTCONN:
		return OMRPORT_ERROR_SOCKET_NOT_CONNECTED;
	case ENOTDIR:
		return OMRPORT_ERROR_SOCKET_NOTDIR;
	case ENOTSOCK:
		return OMRPORT_ERROR_SOCKET_NOTSOCK;
	case EOPNOTSUPP:
		return OMRPORT_ERROR_SOCKET_INVALID_OPERATION;
	case EPERM:
		return OMRPORT_ERROR_SOCKET_OPERATION_NOT_PERMITTED;
	case EPIPE:
		return OMRPORT_ERROR_SOCKET_BROKEN_PIPE;
	case EPROTONOSUPPORT:
		return OMRPORT_ERROR_SOCKET_PROTOCOL_UNSUPPORTED;
	case EPROTOTYPE:
		return OMRPORT_ERROR_SOCKET_BAD_PROTOCOL;
	case EROFS:
		return OMRPORT_ERROR_SOCKET_ROFS;
	case ESOCKTNOSUPPORT:
		return OMRPORT_ERROR_SOCKET_SOCKTYPE_UNSUPPORTED;
	case ETIMEDOUT:
		return OMRPORT_ERROR_SOCKET_TIMEOUT;
	default:
		return OMRPORT_ERROR_OPFAILED;
	}
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
			return OMRPORT_ERROR_SYSTEMFULL;
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
omrsock_addrinfo_address(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, omrsock_sockaddr_t result)
{
	omr_os_addrinfo *info = NULL;
	uint32_t i = 0;

	if ((NULL == handle) || (NULL == handle->addrInfo) || (index >= handle->length)) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	info = (omr_os_addrinfo *)handle->addrInfo;

	for (i = 0; i < index; i++) {
		info = info->ai_next;
		if (NULL == info) {
			return OMRPORT_ERROR_INVALID_ARGUMENTS;
		}
	}
	memcpy(&result->data, info->ai_addr, info->ai_addrlen);

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
omrsock_sockaddr_init(struct OMRPortLibrary *portLibrary, omrsock_sockaddr_t handle, int32_t family, uint8_t *addrNetworkOrder, uint16_t portNetworkOrder)
{
	omr_os_sockaddr_in *sockAddr = (omr_os_sockaddr_in *)&handle->data;
	memset(handle, 0, sizeof(struct OMRSockAddrStorage));
	sockAddr->sin_family = get_os_family(family);
	sockAddr->sin_port = portNetworkOrder;
	memcpy(&(sockAddr->sin_addr.s_addr), addrNetworkOrder, 4);

	return 0;
}

int32_t
omrsock_sockaddr_init6(struct OMRPortLibrary *portLibrary, omrsock_sockaddr_t handle, int32_t family, uint8_t *addrNetworkOrder, uint16_t portNetworkOrder, uint32_t flowinfo, uint32_t scope_id)
{
	omr_os_sockaddr_in6 *sockAddr6 = (omr_os_sockaddr_in6*)&handle->data;
	memset(handle, 0, sizeof (struct OMRSockAddrStorage));

	if (OS_SOCK_AF_INET == get_os_family(family)) {
		/* To talk IPv4 on an IPv6 socket, we need to map the IPv4 address to an IPv6 format. */
		sockAddr6 = (omr_os_sockaddr_in6 *)&handle->data;
		memset(sockAddr6->sin6_addr.s6_addr, 0, 16);
		memcpy(&(sockAddr6->sin6_addr.s6_addr[12]), addrNetworkOrder, 4);
		/**
		 * Check if it is the INADDR_ANY address. We know the top 4 bytes of sockaddr_6->sin6_addr.s6_addr
		 * are 0's as we just cleared them, so we use them to do the check. If it isn't, set 16 bits of "1"s
		 * in front of the IPv4 address.
		 */
		if (0 != memcmp(sockAddr6->sin6_addr.s6_addr, addrNetworkOrder, 4)) {
			sockAddr6->sin6_addr.s6_addr[10] = 0xFF;
			sockAddr6->sin6_addr.s6_addr[11] = 0xFF;
		}
	} else {
		memcpy(&sockAddr6->sin6_addr.s6_addr, addrNetworkOrder, 16);
	}
	sockAddr6->sin6_port = portNetworkOrder;
	sockAddr6->sin6_family = OS_SOCK_AF_INET6;
	sockAddr6->sin6_scope_id = scope_id;
	sockAddr6->sin6_flowinfo = htonl(flowinfo);
	return 0;
}

int32_t
omrsock_socket_getfd(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock)
{
	if (NULL == sock) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
	return sock->data;
}

int32_t
omrsock_socket(struct OMRPortLibrary *portLibrary, omrsock_socket_t *sock, int32_t family, int32_t socktype, int32_t protocol)
{
	int32_t sockDescriptor = 0;
	int32_t osFamily = get_os_family(family);
	int32_t osSockType = get_os_socktype(socktype);
	int32_t osFlag = get_os_socket_flag(socktype);
	int32_t osProtocol = get_os_protocol(protocol);
	int32_t fdFlags = 0;
	
	/* Initialize return omrsock_socket_t to NULL. */
	*sock = NULL;

	if ((0 > osFamily) || (0 > osSockType) || (0 > osProtocol)) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

#if defined(LINUX)
	sockDescriptor = socket(osFamily, osSockType | osFlag, osProtocol);
#else /* define(OMR_OS_LINUX) */
	sockDescriptor = socket(osFamily, osSockType, osProtocol);
#endif /* define(OMR_OS_LINUX) */

	if (0 > sockDescriptor) {
		return portLibrary->error_set_last_error(portLibrary, errno,  get_omr_error(errno));
	}

	/* Tag this descriptor as being non-inheritable. */
	fdFlags = fcntl(sockDescriptor, F_GETFD, 0);
	fcntl(sockDescriptor, F_SETFD, fdFlags | FD_CLOEXEC);

#if !defined(LINUX)
	if (0 != osFlag) {
		fdFlags = fcntl(sockDescriptor, F_GETFL, 0);

		if (fcntl(sockDescriptor, F_SETFL, fdFlags | osFlag) < 0) {
			close(sockDescriptor);
			return portLibrary->error_set_last_error(portLibrary, errno, get_omr_error(errno));
		}
	}
#endif /* !defined(OMR_OS_LINUX) */

	/* Set up the socket structure. */
	*sock = (omrsock_socket_t)portLibrary->mem_allocate_memory(portLibrary, sizeof(struct OMRSocket), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == *sock) {
		close(sockDescriptor);
		return OMRPORT_ERROR_SYSTEMFULL; 
	}

	memset(*sock, 0, sizeof(struct OMRSocket));
	(*sock)->data = sockDescriptor;

	return 0;
}

int32_t
omrsock_bind(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr)
{
	socklen_t addrlength;

	if (OS_SOCK_AF_INET == (addr->data).ss_family) {
		addrlength = sizeof(omr_os_sockaddr_in);
	}
	else {
		addrlength = sizeof(omr_os_sockaddr_in6);
	}

	if (bind(sock->data, (struct sockaddr *)&addr->data, addrlength) < 0) {
		return portLibrary->error_set_last_error(portLibrary, errno,  get_omr_error(errno));
	}

	return 0;
}

int32_t
omrsock_listen(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t backlog)
{
 	if (listen(sock->data, backlog) < 0) {
		return portLibrary->error_set_last_error(portLibrary, errno,  get_omr_error(errno));
	}
	return 0;
}

int32_t
omrsock_connect(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr)
{
	socklen_t addrLength;

	if (NULL == addr || NULL == sock) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	if (OS_SOCK_AF_INET == addr->data.ss_family) {
		addrLength = sizeof(omr_os_sockaddr_in);
	}
	else {
		addrLength = sizeof(omr_os_sockaddr_in6);
	}

	if (connect(sock->data, (omr_os_sockaddr *)&addr->data, addrLength) < 0) {
		return portLibrary->error_set_last_error(portLibrary, errno,  get_omr_error(errno));
	}
	return 0;
}

int32_t
omrsock_accept(struct OMRPortLibrary *portLibrary, omrsock_socket_t serverSock, omrsock_sockaddr_t addrHandle, omrsock_socket_t *sockHandle)
{
	int32_t	connSocketDescriptor;

#if defined(OMR_OS_ZOS)
	int32_t addrLength = sizeof(omr_os_sockaddr_storage);
#else
	socklen_t addrLength = sizeof(omr_os_sockaddr_storage);
#endif

	if (NULL == serverSock || NULL == addrHandle) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	*sockHandle = NULL;

	connSocketDescriptor = accept(serverSock->data, (omr_os_sockaddr *)&addrHandle->data, &addrLength);
	if (connSocketDescriptor < 0) {
		return portLibrary->error_set_last_error(portLibrary, errno,  get_omr_error(errno));
	}

	*sockHandle = (omrsock_socket_t)portLibrary->mem_allocate_memory(portLibrary, sizeof(struct OMRSocket), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (*sockHandle == NULL) {
		close(connSocketDescriptor);
		return OMRPORT_ERROR_SYSTEMFULL; 
	}

	(*sockHandle)->data = connSocketDescriptor;
	return 0;
}

int32_t
omrsock_send(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	int32_t bytesSent = 0;

	if (NULL == sock || 0 >= nbyte) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	bytesSent = send(sock->data, buf, nbyte, flags);

	if (-1 == bytesSent) {
		portLibrary->error_set_last_error(portLibrary, errno, get_omr_error(errno));
	}
	return bytesSent;
}

int32_t
omrsock_sendto(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle)
{
	int32_t bytesSent = 0;

	if (NULL == sock || 0 >= nbyte || NULL == addrHandle) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	bytesSent = sendto(sock->data, buf, nbyte, flags, (omr_os_sockaddr *)&addrHandle->data, sizeof(omr_os_sockaddr_storage));

	if (-1 == bytesSent) {
		portLibrary->error_set_last_error(portLibrary, errno, get_omr_error(errno));
	}

	return bytesSent;
}

int32_t
omrsock_recv(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	int32_t bytesRecv = 0;

	if (NULL == sock || 0 >= nbyte) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	bytesRecv = recv(sock->data, buf, nbyte, flags);

	if (-1 == bytesRecv) {
		portLibrary->error_set_last_error(portLibrary, errno, get_omr_error(errno));
	}
	return bytesRecv;
}

int32_t
omrsock_recvfrom(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle)
{
	int32_t bytesRecv = 0;
	socklen_t addrLength = 0;
	
	if (NULL == sock || 0 >= nbyte) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	if (NULL == addrHandle) {
		/* If addrHandle is NULL, the source address will not be filled in by recvfrom call. */
		bytesRecv = recvfrom(sock->data, buf, nbyte, flags, NULL, NULL);
	} else {
		addrLength = sizeof(omr_os_sockaddr_storage);
		bytesRecv = recvfrom(sock->data, buf, nbyte, flags, (struct sockaddr*)&addrHandle->data, &addrLength);
	}
	if (-1 == bytesRecv) {
		portLibrary->error_set_last_error(portLibrary, errno, get_omr_error(errno));
	}
	
	return bytesRecv;
}

int32_t
omrsock_pollfd_init(struct OMRPortLibrary *portLibrary, omrsock_pollfd_t handle, omrsock_socket_t sock, int16_t events)
{
	if (NULL == handle || NULL == sock ) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
	memset(handle, 0, sizeof(OMRPollFd));
	handle->socket = sock;
	handle->data.fd = sock->data;
	handle->data.events = events;
	return 0;
}

int32_t
omrsock_get_pollfd_info(struct OMRPortLibrary *portLibrary, omrsock_pollfd_t handle, omrsock_socket_t *sock, int16_t *revents)
{
	if (NULL == handle || NULL == sock || NULL == revents) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
	memset(sock, 0, sizeof(omrsock_socket_t));

	*sock = handle->socket;
	*revents = handle->data.revents;
	return 0;
}

int32_t
omrsock_poll(struct OMRPortLibrary *portLibrary, omrsock_pollfd_t fds, uint32_t nfds, int32_t timeoutMs)
{
	int32_t numPollFdSet = 0;
	const uint32_t maxNumPollFd = 8;
	struct pollfd pfdsArray[maxNumPollFd];
	struct pollfd *pfds = NULL;
	int32_t i = 0;

#if defined(AIXPPC)
	if (FD_SETSIZE < nfds) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
#endif /* defined(AIXPPC) */

	if (NULL == fds || 0 >= nfds) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	if (maxNumPollFd >= nfds) {
		/* Use statically allocated array if nfds less than or equal to 8. */
		pfds = pfdsArray;
	} else {
		/* Dynamically allocate if nfds more than 8. */
		pfds = portLibrary->mem_allocate_memory(portLibrary, nfds * sizeof(struct pollfd), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == pfds) {
			return portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_SYSTEMFULL);
		}
	}

	/* Set up fds to pass into the poll function. */
	for (i = 0; nfds > i; i++) {
		pfds[i].fd = fds[i].data.fd;
		pfds[i].events = get_os_poll_constant(fds[i].data.events);
	}

	numPollFdSet = poll(pfds, nfds, timeoutMs);

	if (0 > numPollFdSet) {
		if (nfds > maxNumPollFd) {
			portLibrary->mem_free_memory(portLibrary, pfds);
		}
		return portLibrary->error_set_last_error(portLibrary, errno, get_omr_error(errno));
	}

	/* Set user's fds values to be same as pfds. */
	for (i = 0; nfds > i; i++) {
		fds[i].data.revents = get_omr_poll_constant(pfds[i].revents);
#if defined (LINUX)
		/* On Linux, all sockets with non-zero revents are included in numPollFdSet, but other systems do not include error revents. */
		if (0 != (fds[i].data.revents & (OMRSOCK_POLLERR | OMRSOCK_POLLNVAL | OMRSOCK_POLLHUP))) {
			/* Should not subtract if there are 0. This case should not happen. */
			numPollFdSet = (numPollFdSet == 0) ? 0 : numPollFdSet - 1;
		}
#endif /* defined (LINUX) */
	}

	if (maxNumPollFd < nfds) {
		portLibrary->mem_free_memory(portLibrary, pfds);
	}

#if defined (AIXPPC) || defined (J9ZOS390)
	return numPollFdSet & 0x0000FFFF;
#else /* defined (AIXPPC) || defined (J9ZOS390) */
	return numPollFdSet;
#endif /* defined (AIXPPC) || defined (J9ZOS390) */
}

void
omrsock_fdset_zero(struct OMRPortLibrary *portLibrary, omrsock_fdset_t fdset)
{
	fdset->maxFd = 0;
	FD_ZERO(&fdset->data);
}

void
omrsock_fdset_set(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_fdset_t fdset) 
{
	/* If socket fd is greater than fdset maxFd, overwrite it. */
	if (sock->data > fdset->maxFd) {
		fdset->maxFd = sock->data;
	}
	FD_SET(sock->data, &fdset->data);
}

void
omrsock_fdset_clr(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_fdset_t fdset) 
{
	FD_CLR(sock->data, &fdset->data);
}

BOOLEAN
omrsock_fdset_isset(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_fdset_t fdset) 
{
	if (0 != FD_ISSET(sock->data, &fdset->data)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

int32_t
omrsock_select(struct OMRPortLibrary *portLibrary, omrsock_fdset_t readfds, omrsock_fdset_t writefds, omrsock_fdset_t exceptfds, omrsock_timeval_t timeout)
{
	int32_t nfds = 0;
	int32_t readNfds = 0;
	int32_t writeNfds = 0;
	int32_t exceptNfds = 0;
	int32_t numFdSet = 0;

	if (NULL == timeout) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	/* Find maximum fd in the three fdsets. */

	readNfds = (NULL == readfds) ? 0 : readfds->maxFd;
	writeNfds = (NULL == writefds) ? 0 : writefds->maxFd;
	exceptNfds = (NULL == exceptfds) ? 0 : exceptfds->maxFd;

	nfds = OMR_MAX(OMR_MAX(readNfds,writeNfds),exceptNfds);

	if ((FD_SETSIZE - 1) < nfds) {
		return OMRPORT_ERROR_SOCKET_EXCEED_MAX_NFDS;
	}

	numFdSet = select(nfds + 1, readfds == NULL ? NULL : &readfds->data, writefds == NULL ? NULL : &writefds->data,
					exceptfds == NULL ? NULL : &exceptfds->data, timeout == NULL ? NULL : &timeout->data);
	if (-1 == numFdSet) {
		return portLibrary->error_set_last_error(portLibrary, errno, get_omr_error(errno));
	}

	return numFdSet;
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

uint16_t
omrsock_htons(struct OMRPortLibrary *portLibrary, uint16_t val)
{
	return htons(val);
}

uint32_t
omrsock_htonl(struct OMRPortLibrary *portLibrary, uint32_t val)
{
	return htonl(val);
}

int32_t
omrsock_inet_pton(struct OMRPortLibrary *portLibrary, int32_t addrFamily, const char *addr, uint8_t *result)
{
	int32_t rc;
#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
	char *addrA2e;
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

	if (NULL == result) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
	addrA2e = a2e(addr, strlen(addr));
	rc = inet_pton(get_os_family(addrFamily), addrA2e, (void *)result);
	free(addrA2e);
#else /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */
	rc = inet_pton(get_os_family(addrFamily), addr, (void *)result);
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

	if (rc == 0) {
		return OMRPORT_ERROR_SOCKET_BAD_ADDRESS;
	} else if (rc == -1) {
		return OMRPORT_ERROR_SOCKET_AF_UNSUPPORTED;
	}
	return 0;
}

int32_t
omrsock_fcntl(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t arg)
{
	int32_t existingFlags = 0;
	int32_t socketFlag = get_os_socket_flag(arg);

	if (NULL == sock || 0 == socketFlag){
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}

	existingFlags = fcntl(sock->data, F_GETFL, 0);

	if (fcntl(sock->data, F_SETFL, existingFlags | socketFlag) < 0) {
		return portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_FILE_OPFAILED);
	}
	return 0;
}

int32_t
omrsock_timeval_init(struct OMRPortLibrary *portLibrary, omrsock_timeval_t handle, uint32_t secTime, uint32_t uSecTime)
{
	if (NULL == handle) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
	memset(handle, 0, sizeof(OMRTimeval));
	handle->data.tv_sec = secTime;
	handle->data.tv_usec = uSecTime;
	return 0;
}

int32_t
omrsock_linger_init(struct OMRPortLibrary *portLibrary, omrsock_linger_t handle, int32_t enabled, uint16_t timeout)
{
	if (NULL == handle) {
		return OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
	memset(handle, 0, sizeof(OMRLinger));
	handle->data.l_onoff = enabled;
	handle->data.l_linger = (int32_t)timeout; /* Cast to in32_t because WIN system takes uint16_t*/
	return 0;
}

/**
 * @internal Set socket options for all option value types, since pointers to option values are casted to void types.  
 *
 * @param portLibrary The port library.
 * @param sock Pointer to the socket to set the option in.
 * @param optlevel The level within the IP stack at which the option is defined.
 * @param optname The name of the option to set.
 * @param optval Void type pointer to the option value to update the socket option with.
 * @param len Length of the option value.
 *
 * @return 0, if no errors occurred, otherwise return an error.
 */
int32_t
set_opt(struct OMRPortLibrary *portLibrary, omr_os_socket sock, int32_t optlevel, int32_t optname, void *optval, socklen_t len)
{
	int32_t osLevel = get_os_socket_level(optlevel);
	int32_t osOption = get_os_socket_option(optname);

	if (osLevel == OMRPORT_ERROR_SOCK_LEVEL_UNSUPPORTED) {
		return OMRPORT_ERROR_SOCK_LEVEL_UNSUPPORTED;
	}

	if (osOption == OMRPORT_ERROR_SOCK_OPTION_UNSUPPORTED) {
		return OMRPORT_ERROR_SOCK_OPTION_UNSUPPORTED;
	}

	if (0 != setsockopt(sock, osLevel, osOption, optval, len)) {
		return portLibrary->error_set_last_error(portLibrary, errno,  get_omr_error(errno));
	}

	return 0;
}

/**
 * @internal Get socket options for all option value types, since pointers to option values are casted to void types.
 * Opposite of @ref set_opt.
 *
 * @param portLibrary The port library.
 * @param sock Pointer to the socket to query for the option value.
 * @param optlevel The level within the IP stack at which the option is defined.
 * @param optname The name of the option to retrieve.
 * @param optval Void type pointer to the pre-allocated space to update with the option value.
 * @param len Length of the option value.
 *
 * @return 0, if no errors occurred, otherwise return an error.
 */
int32_t
get_opt(struct OMRPortLibrary *portLibrary, omr_os_socket sock, int32_t optlevel, int32_t optname, void *optval, socklen_t len)
{
	int32_t osLevel = get_os_socket_level(optlevel);
	int32_t osOption = get_os_socket_option(optname);
	socklen_t optlen = len;

	if (osLevel == OMRPORT_ERROR_SOCK_LEVEL_UNSUPPORTED) {
		return OMRPORT_ERROR_SOCK_LEVEL_UNSUPPORTED;
	}

	if (osOption == OMRPORT_ERROR_SOCK_OPTION_UNSUPPORTED) {
		return OMRPORT_ERROR_SOCK_OPTION_UNSUPPORTED;
	}
	
	if (0 != getsockopt(sock, osLevel, osOption, optval, &optlen)) {
		return portLibrary->error_set_last_error(portLibrary, errno,  get_omr_error(errno));
	}

	return 0;
}

int32_t
omrsock_setsockopt_int(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, int32_t *optval)
{
	return set_opt(portLibrary, handle->data, optlevel, optname, (void*)optval, sizeof(int32_t));
}

int32_t
omrsock_setsockopt_linger(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_linger_t optval)
{
	return set_opt(portLibrary, handle->data, optlevel, optname, (void*)&optval->data, sizeof(struct linger));
}

int32_t
omrsock_setsockopt_timeval(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_timeval_t optval)
{
	return set_opt(portLibrary, handle->data, optlevel, optname, (void*)&optval->data, sizeof(struct timeval));
}

int32_t
omrsock_getsockopt_int(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, int32_t *optval)
{
	return get_opt(portLibrary, handle->data, optlevel, optname, (void*)optval, sizeof(int32_t));
}

int32_t
omrsock_getsockopt_linger(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_linger_t optval)
{
	return get_opt(portLibrary, handle->data, optlevel, optname, (void*)&optval->data, sizeof(struct linger));
}

int32_t
omrsock_getsockopt_timeval(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_timeval_t optval)
{
	return get_opt(portLibrary, handle->data, optlevel, optname, (void*)&optval->data, sizeof(struct timeval));
}
