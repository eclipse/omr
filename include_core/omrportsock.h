/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#if !defined(OMRPORTSOCK_H_)
#define OMRPORTSOCK_H_

/* Pointer to OMRAddInfoNode, a struct that contains addrinfo information. */
typedef struct OMRAddrInfoNode *omrsock_addrinfo_t;

/* Pointer to OMRSockAddrStorage, a struct that contains socket address
 * information. It has enough space for Ipv4 or IPv6 addresses. 
 */
typedef struct OMRSockAddrStorage *omrsock_sockaddr_t;

/* Pointer to OMRSocket, a struct that contains socket descriptor. */
typedef struct OMRSocket *omrsock_socket_t;

/* Bind to all available interfaces */
#define OMRSOCK_INADDR_ANY ((uint32_t)0)

/* Address Family */
#define OMRSOCK_AF_UNSPEC 0
#define OMRSOCK_AF_INET 1
#define OMRSOCK_AF_INET6 2

/* Socket types */
#define OMRSOCK_ANY 0
#define OMRSOCK_STREAM 1
#define OMRSOCK_DGRAM 2

/* Protocol Family */
#define OMRSOCK_IPPROTO_DEFAULT 0
#define OMRSOCK_IPPROTO_TCP 1
#define OMRSOCK_IPPROTO_UDP 2

#endif /* !defined(OMRPORTSOCK_H_) */
