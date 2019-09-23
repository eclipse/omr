/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
/******************************************************\
		Common data types required for using the omr socket API
\******************************************************/

#if !defined(OMRPORTSOCK_H_)
#define OMRPORTSOCK_H_

#if defined(OMR_OS_WINDOWS) && defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#undef _WINSOCKAPI_
#endif /* defined(OMR_OS_WINDOWS) && defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_) */

#if defined(OMR_OS_WINDOWS)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else /* defined(OMR_OS_WINDOWS) */
#include <sys/socket.h>
#include <netinet/in.h>
#endif /* defined(OMR_OS_WINDOWS) */


#if defined(OMR_OS_WINDOWS)
typedef SOCKET OMRSocket;
typedef SOCKADDR OMRSockAddr; /* for IPv4 */
typedef SOCKADDR_IN OMRSockAddrIn;	/* for IPv4 addresses*/
typedef SOCKADDR_IN6 OMRSockAddrIn6;  /* for IPv6 addresses*/
typedef struct sockaddr_storage OMRSockAddrStorage; /* Big enough storage for IPv4 or IPv6 addresses */
typedef struct addrinfoW OMRAddrInfo;  /* addrinfo structure – Unicode for both IPv4 and IPv6 */
#else /* defined(OMR_OS_WINDOWS) */
typedef int OMRSocket;
typedef struct sockaddr OMRSockAddr;
typedef struct sockaddr_in OMRSockAddrIn; /* for IPv4 addresses*/
typedef struct sockaddr_in6 OMRSockAddrIn6; /* for IPv6 addresses*/
typedef struct sockaddr_storage OMRSockAddrStorage; /* Big enough storage for IPv4 or IPv6 addresses */
typedef struct addrinfo OMRAddrInfo; /* addrinfo structure for both IPv4 and IPv6 */
#endif /* defined(OMR_OS_WINDOWS) */

/* Filled in using @omr_getaddrinfo. */
typedef struct OMRAddrInfoNode {
	OMRAddrInfo* addrInfo;		/* Defined differently depending on the operating system. Pointer to the first addrinfo node in listed list. */
	int length; 				/* Number of addrinfo nodes in linked list */
} OMRAddrInfoNode;
typedef OMRAddrInfoNode* omrsock_addrinfo_t;

/* Used for storage of ip addresses. If we are only supporting IPv4, then we allocate smaller storage space. */
#if defined(IPv6_FUNCTION_SUPPORT)
typedef struct sockaddr_storage OMRSockAddr; /* Big enough storage for IPv4 or IPv6 addresses */
#else /* defined(IPv6_FUNCTION_SUPPORT) */
typedef struct sockaddr OMRSockAddr;
#endif /* defined(IPv6_FUNCTION_SUPPORT) */
typedef OMRSockAddr* omrsock_sockaddr_t;


/* Used to store information about a host. Should never modify or free the content of hostent. */
typedef struct hostent OMRHostent;


/* Pointer to a socket descriptor */
typedef OMRSocket* omrsock_socket_t;	/* OMRSocket is SOCKET type for Windows, int for all other operating systems */

/* Socket Types */
#define OMRSOCK_ANY			 0                   /* for getaddrinfo hints */
#define OMRSOCK_STREAM     SOCK_STREAM              /* stream socket */
#define OMRSOCK_DGRAM      SOCK_DGRAM               /* datagram socket */
#if 0 /* Future Implementations*/
#define OMR_SOCK_RAW        SOCK_RAW                 /* raw-protocol interface */
#define OMR_SOCK_RDM        SOCK_RDM                 /* reliably-delivered message */
#define OMR_SOCK_SEQPACKET  SOCK_SEQPACKET           /* sequenced packet stream */
#endif /* Future Implementations*/

/* Address Family */
#define OMRSOCK_AF_UNSPEC       AF_UNSPEC
#define OMRSOCK_AF_INET4        AF_INET
#define OMRSOCK_PF_UNSPEC       PF_UNSPEC
#define OMRSOCK_PF_INET4        PF_INET
#define OMRSOCK_INET4_ADDRESS_LENGTH INET_ADDRSTRLEN
#define OMRSOCK_AF_INET6        AF_INET6
#define OMRSOCK_PF_INET6 PF_INET6
#define OMRSOCK_INET6_ADDRESS_LENGTH INET6_ADDRSTRLEN

/* Socket Flag */
#define OMRSOCK_AI_CANONNAME    AI_CANONNAME
#define OMRSOCK_AI_NUMERICHOST  AI_NUMERICHOST
#define OMRSOCK_AI_PASSIVE      AI_PASSIVE

/* Socket Options */
#define OMRSOCK_SO_KEEPALIVE    SO_KEEPALIVE
#define OMRSOCK_SO_REUSEADDR    SO_REUSEADDR
#if 0 /* Future Implementations */
#define OMRSOCK_SO_LINGER       SO_LINGER
#define OMRSOCK_SO_SNDBUF       SO_SNDBUF
#define OMRSOCK_SO_RCVBUF       SO_RCVBUF
#define OMRSOCK_SO_BROADCAST    SO_BROADCAST /*[PR1FLSKTU] Support datagram broadcasts */
#define OMRSOCK_SO_OOBINLINE    SO_OOBINLINE
#endif /* Future Implementations */

/* Socket Level */
#define OMRSOCK_SOL_SOCKET      SOL_SOCKET
/* Protocol Family */
#define OMRSOCK_IPPROTO_IP      IPPROTO_IP       /* Dummy protocol for TCP (default) */
#define OMRSOCK_IPPROTO_TCP     IPPROTO_TCP     /* Transmission Control Protocol */
#define OMRSOCK_IPPROTO_IPV6    IPPROTO_IPV6   /* IPv6 header */
#define OMRSOCK_IPPROTO_UDP     IPPROTO_UDP     /* User Datagram Protocol */

#endif /* !defined(OMRPORTSOCK_H_) */