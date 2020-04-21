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

#if !defined(OMRPORTSOCKTYPES_H_)
#define OMRPORTSOCKTYPES_H_

/**
 * WIN32_LEAN_AND_MEAN determines what is included in Windows.h. If it is
 * defined, some unneeded header files in Windows.h will not be included.
 */
#if defined(OMR_OS_WINDOWS) && !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif /* defined(OMR_OS_WINDOWS) && !defined(WIN32_LEAN_AND_MEAN) */

/**
 * To avoid WINSOCK redefinition errors.
 */
#if defined(OMR_OS_WINDOWS) && defined(_WINSOCKAPI_)
#undef _WINSOCKAPI_
#endif /* defined(OMR_OS_WINDOWS) && defined(_WINSOCKAPI_) */

#if defined(OMR_OS_WINDOWS)
/**
 * windows.h defined UDATA. Ignore its definition to avoid redefinition errors.
 */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib") 
#pragma comment(lib, "AdvApi32.lib") 
#else /* defined(OMR_OS_WINDOWS) */
#include <netinet/in.h>
#include <sys/socket.h>
#endif /* defined(OMR_OS_WINDOWS) */

#include <stdlib.h>
#include <stdio.h>

/**
 * Data types required for the socket API.
 */
#if defined(OMR_OS_WINDOWS)
typedef SOCKET omr_os_socket;
typedef struct sockaddr_storage omr_os_sockaddr_storage; /* For IPv4 or IPv6 addresses */
typedef struct addrinfoW omr_os_addrinfo;  /* addrinfo structure – Unicode, for IPv4 or IPv6 */
#else /* defined(OMR_OS_WINDOWS) */
typedef int omr_os_socket;
typedef struct sockaddr_storage omr_os_sockaddr_storage; /* For IPv4 or IPv6 addresses */
typedef struct addrinfo omr_os_addrinfo; /* addrinfo structure for IPv4 or IPv6*/
#endif /* defined(OMR_OS_WINDOWS) */

/**
 * A struct for storing socket address information. Big enough to store IPv4 and IPv6 addresses.
 */
typedef struct OMRSockAddrStorage {
	omr_os_sockaddr_storage data;
} OMRSockAddrStorage;

/**
 * A struct for storing socket information.
 */
typedef struct OMRSocket {
	omr_os_socket data;
} OMRSocket;

/**
 * A node in a linked-list of addrinfo. Filled in using @ref omr_getaddrinfo.
 */
typedef struct OMRAddrInfoNode {
	/**
	 * Pointer to the first addrinfo node in listed list. Defined differently depending on the operating system. 
	 */
	omr_os_addrinfo *addrInfo;
	
	/**
	 * Number of addrinfo nodes in linked list.
	 */
	uint32_t length;
} OMRAddrInfoNode;

#endif /* !defined(OMRPORTSOCKTYPES_H_) */
