/*******************************************************************************
 * Copyright (c) 2019, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrcfg.h"
#include "omrport.h"
#include "omrporterror.h"
#include "omrportsock.h"
#include "omrportsocktypes.h"
#include "testHelpers.hpp"

/**
 * Start a server which creates a socket, binds to an address/port and 
 * listens for clients.
 * 
 * @param[in] portLibrary
 * @param[in] addrStr The address of the server.
 * @param[in] port The server port.
 * @param[in] family Socket address family wanted.
 * @param[out] serverSocket A pointer to the server socket. 
 * @param[out] serverAddr The socket address of the server created.
 * 
 * @return on success, report an error otherwise.
 */ 
void
start_server(struct OMRPortLibrary *portLibrary, int32_t family, int32_t socktype, omrsock_socket_t *serverSocket, omrsock_sockaddr_t serverSockAddr) 
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	int32_t flag = 1;

	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, serverSocket, family, socktype, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, *serverSocket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, *serverSocket, serverSockAddr), 0);

	if (OMRSOCK_STREAM == (socktype & 0x00FF)) {
		ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, *serverSocket, 10), 0);
	}
	return;
}

/**
 * Create the client socket, and then connect to the server.
 *  
 * @param[in] portLibrary
 * @param[in] addrStr The address of the server.
 * @param[in] port The server port.
 * @param[in] family Socket address family wanted.
 * @param[in] sessionClientSocket A pointer to the client socket. 
 * @param[in] sessionClientAddr The socket address of the client created. 
 * 
 * @return on success, report an error otherwise.
 */ 
void
connect_client_to_server(struct OMRPortLibrary *portLibrary, const char *addrStr, const char *port, int32_t family, int32_t socktype, omrsock_socket_t *clientSocket, omrsock_sockaddr_t clientSockAddr, omrsock_sockaddr_t serverSockAddr) 
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	omrsock_addrinfo_t hints = NULL;
	OMRAddrInfoNode result;
	uint32_t length = 0;
	int32_t resultFamily;
	int32_t resultSocktype;
	int32_t resultProtocol;

	EXPECT_EQ(OMRPORTLIB->sock_getaddrinfo_create_hints(OMRPORTLIB, &hints, family, socktype, OMRSOCK_IPPROTO_DEFAULT, 0), 0);
	EXPECT_EQ(OMRPORTLIB->sock_getaddrinfo(OMRPORTLIB, addrStr, port, hints, &result), 0);
	EXPECT_EQ(OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, &result, &length), 0);
	EXPECT_NE(length, uint32_t(0));

	/* Create a socket with results. */
	for (uint32_t i = 0; i < length; i++) {
		EXPECT_EQ(OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, &result, i, &resultFamily), 0);
		EXPECT_EQ(OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, &result, i, &resultSocktype), 0);
		EXPECT_EQ(OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, &result, i, &resultProtocol), 0);

		if(0 == OMRPORTLIB->sock_socket(OMRPORTLIB, clientSocket, resultFamily, resultSocktype, resultProtocol)) {
			ASSERT_NE(clientSocket, (void *)NULL);
			EXPECT_EQ(OMRPORTLIB->sock_addrinfo_address(OMRPORTLIB, &result, i, clientSockAddr), 0);
			ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, *clientSocket, clientSockAddr), 0);
			break;
		}
	}
	EXPECT_EQ(OMRPORTLIB->sock_freeaddrinfo(OMRPORTLIB, &result), 0);

	if(OMRSOCK_STREAM == resultSocktype) {
		ASSERT_EQ(OMRPORTLIB->sock_connect(OMRPORTLIB, *clientSocket, serverSockAddr), 0);
	}
	return;
}

/**
 * Test if port library is properly set up to run OMRSock tests. 
 *
 * This will iterate through all OMRSock functions in @ref omrsock.c and simply check 
 * that the location of the function is not NULL.
 *
 * @note Errors such as an OMRSock function call with a NULL address will be reported.
 */
TEST(PortSockTest, library_function_pointers_not_null)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	EXPECT_NE(OMRPORTLIB->sock_getaddrinfo_create_hints, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_getaddrinfo, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_addrinfo_length, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_addrinfo_family, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_addrinfo_socktype, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_addrinfo_protocol, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_freeaddrinfo, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_socket, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_sockaddr_init, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_sockaddr_init6, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_socket_getfd, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_bind, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_listen, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_accept, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_send, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_sendto, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_recv, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_recvfrom, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_pollfd_init, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_get_pollfd_info, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_poll, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_fdset_zero, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_fdset_set, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_fdset_clr, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_fdset_isset, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_select, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_close, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_htons, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_htonl, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_inet_pton, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_fcntl, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_timeval_init, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_linger_init, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_setsockopt_int, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_setsockopt_linger, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_setsockopt_timeval, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_getsockopt_int, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_getsockopt_linger, (void *)NULL);
	EXPECT_NE(OMRPORTLIB->sock_getsockopt_timeval, (void *)NULL);
}

/**
 * Test the omrsock per thread buffer using @ref omrsock_getaddrinfo_create_hints
 * and all functions that extract details from the per thread buffer.
 * 
 * In this test, per thread buffer related functions set up a buffer to store
 * information such as the hints structures created in 
 * @ref omrsock_getaddrinfo_create_hints. Since the per thread buffer can't be easily 
 * directly tested, it will be tested with @ref omrsock_getaddrinfo_create_hints.
 *
 * @note Errors such as invalid arguments, and/or returning the wrong family or 
 * socket type from hints compared to the ones passed into hints, will be reported.
 */
TEST(PortSockTest, create_hints_and_element_extraction)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	omrsock_addrinfo_t hints = NULL;
	uint32_t length = 0;
	int32_t family = 0;
	int32_t sockType = 0;
	int32_t protocol = 0;

	int32_t hintsFamily = OMRSOCK_AF_UNSPEC;
	int32_t hintsSockType = OMRSOCK_STREAM;
	int32_t hintsProtocol = OMRSOCK_IPPROTO_TCP;
	int32_t hintsFlags = 0;

	OMRPORTLIB->sock_getaddrinfo_create_hints(OMRPORTLIB, &hints, hintsFamily, hintsSockType, hintsProtocol, hintsFlags);
	
	ASSERT_NE(hints, (void *)NULL);

	/* Testing invalid arguments: Pointer to OMRAddrInfoNode that is NULL */

	int32_t rc;
	rc = OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, NULL, &length);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	rc = OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, NULL, 0, &family);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	rc = OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, NULL, 0, &sockType);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	rc = OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, NULL, 0, &protocol);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	/* Testing invalid arguments: Index is bigger than the length when querying */

	rc = OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, hints, &length);
	EXPECT_EQ(rc, 0);

	rc = OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, hints, length, &family);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	rc = OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, hints, length, &sockType);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	rc = OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, hints, length, &protocol);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	/* Get and verify elements of the newly created hints. */

	rc = OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, hints, &length);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(length, uint32_t(1));

	rc = OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, hints, 0, &family);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(family, hintsFamily);

	rc = OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, hints, 0, &sockType);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(sockType, hintsSockType);

	rc = OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, hints, 0, &protocol);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(protocol, hintsProtocol);

	/* Recreate hints with different parameters, see if hints elements are overwriten properly. */

	hintsProtocol = OMRSOCK_IPPROTO_DEFAULT;
	hintsFlags = 6;

	OMRPORTLIB->sock_getaddrinfo_create_hints(OMRPORTLIB, &hints, hintsFamily, hintsSockType, hintsProtocol, hintsFlags);

	rc = OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, hints, &length);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(length, uint32_t(1));

	rc = OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, hints, 0, &family);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(family, hintsFamily);

	rc = OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, hints, 0, &sockType);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(sockType, hintsSockType);

	rc = OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, hints, 0, &protocol);
	EXPECT_EQ(rc, 0);
	EXPECT_EQ(protocol, hintsProtocol);

	/* Testing invalid arguments: User exposes API and changes length, then query. */

	hints->length = 5;

	rc = OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, hints, 3, &family);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	rc = OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, hints, 3, &sockType);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	rc = OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, hints, 3, &protocol);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);
}

/**
 * Test @ref omrsock_getaddrinfo, @ref omrsock_freeaddrinfo and all functions 
 * that extract details from @ref omrsock_getaddrinfo results.
 * 
 * In this test, the relevant variables are passed into  
 * @ref omrsock_getaddrinfo_create_hints. The generated hints are passed into 
 * @ref omrsock_getaddrinfo. The results generated are used to create a socket.
 *
 * Address families tested will include IPv4, IPv6(if supported on machine), 
 * unspecified family. Socket types tested will include Stream and Datagram.
 *
 * @note Errors such as failed function calls, and/or returning the wrong family or 
 * socket type from @ref omrsock_getaddrinfo compared to the ones passed into hints, 
 * will be reported.
 */
TEST(PortSockTest, getaddrinfo_and_freeaddrinfo)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRAddrInfoNode result;
	OMRSockAddrStorage sockAddr;
	omrsock_addrinfo_t hints = NULL;
	omrsock_socket_t socket = NULL;
	int32_t rc = 0;
	uint32_t length = 0;
	int32_t family = 0;
	int32_t sockType = 0;
	int32_t protocol = 0;

	int32_t hintsFamily = OMRSOCK_AF_INET;
	int32_t hintsSockType = OMRSOCK_STREAM;
	int32_t hintsProtocol = OMRSOCK_IPPROTO_DEFAULT;
	int32_t hintsFlags = 0;

	rc = OMRPORTLIB->sock_getaddrinfo_create_hints(OMRPORTLIB, &hints, hintsFamily, hintsSockType, hintsProtocol, hintsFlags);
	EXPECT_EQ(rc, 0);

	/* Testing invalid arguments: getaddrinfo pointer to result is NULL. */
	omrsock_addrinfo_t resultPtr = NULL;

	rc = OMRPORTLIB->sock_getaddrinfo(OMRPORTLIB, "localhost", NULL, hints, resultPtr);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	/* Testing invalid arguments: freeaddrinfo pointer to result is NULL. */
	rc = OMRPORTLIB->sock_freeaddrinfo(OMRPORTLIB, resultPtr);
	EXPECT_EQ(rc, OMRPORT_ERROR_INVALID_ARGUMENTS);

	/* Verify that omrsock_getaddrinfo and omrsock_freeaddrinfo works with NULL hints. */
	rc = OMRPORTLIB->sock_getaddrinfo(OMRPORTLIB, "localhost", NULL, NULL, &result);
	ASSERT_EQ(rc, 0);

	OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, &result, &length);
	ASSERT_NE(length, uint32_t(0));
	
	OMRPORTLIB->sock_freeaddrinfo(OMRPORTLIB, &result);

	/* Get and verify that omrsock_getaddrinfo and omrsock_freeaddrinfo works. */
	rc = OMRPORTLIB->sock_getaddrinfo(OMRPORTLIB, "localhost", NULL, hints, &result);
	ASSERT_EQ(rc, 0);

	OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, &result, &length);
	ASSERT_NE(length, uint32_t(0));

	for (uint32_t i = 0; i < length; i++) {
		rc = OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, &result, i, &family);
		EXPECT_EQ(rc, 0);
		EXPECT_EQ(family, hintsFamily);

		rc = OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, &result, i, &sockType);
		EXPECT_EQ(rc, 0);
		EXPECT_EQ(sockType, hintsSockType);

		rc = OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, &result, i, &protocol);
		EXPECT_EQ(rc, 0);
	}

	/* Try to create a socket with results. */
	for (uint32_t i = 0; i < length; i++) {
		EXPECT_EQ(OMRPORTLIB->sock_addrinfo_family(OMRPORTLIB, &result, i, &family), 0);
		EXPECT_EQ(OMRPORTLIB->sock_addrinfo_socktype(OMRPORTLIB, &result, i, &sockType), 0);
		EXPECT_EQ(OMRPORTLIB->sock_addrinfo_protocol(OMRPORTLIB, &result, i, &protocol), 0);

		rc = OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, family, sockType, protocol);
		if(0 == rc) {
			ASSERT_NE(socket, (void *)NULL);
			ASSERT_EQ(OMRPORTLIB->sock_addrinfo_address(OMRPORTLIB, &result, i, &sockAddr), 0);
			break;
		}
	}

	EXPECT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr), 0);
	EXPECT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, 10), 0);
	
	rc = OMRPORTLIB->sock_close(OMRPORTLIB, &socket);
	EXPECT_EQ(rc, 0);

	rc = OMRPORTLIB->sock_freeaddrinfo(OMRPORTLIB, &result);
	EXPECT_EQ(rc, 0);
}

/**
 * Test functions involving IPv4 and IPv6 socket addresses:
 * To Create a Socket Address with INADDR_ANY IPv4 address
 *
 * After creating the address, their ability to bind and listen will be tested.
 * 
 * Address families tested is IPv4.
 */
TEST(PortSockTest, create_addressany_IPv4_socket_address)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage sockAddr;
	omrsock_socket_t socket = NULL;
	uint16_t port = 4930;
	int32_t flag = 1;
	uint8_t addr[4];

	uint32_t inaddrAny = OMRPORTLIB->sock_htonl(OMRPORTLIB, OMRSOCK_INADDR_ANY);
	memcpy(addr, &inaddrAny, 4);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &sockAddr, OMRSOCK_AF_INET, addr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, socket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr), 0);
	ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, OMRSOCK_MAXCONN), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
}

/**
 * Test functions involving IPv4 and IPv6 socket addresses:
 * To Create a Socket Address with dotted-decimal IPv4 address.
 *
 * After creating the address, their ability to bind and listen will be tested.
 * 
 * Address families tested is IPv4.
 */
TEST(PortSockTest, create_dotted_decimal_IPv4_socket_address)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage sockAddr;
	omrsock_socket_t socket = NULL;
	uint16_t port = 4930;
	int32_t flag = 1;
	uint8_t addr[4];

	EXPECT_EQ(OMRPORTLIB->sock_inet_pton(OMRPORTLIB, OMRSOCK_AF_INET, "127.0.0.1", addr), 0);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &sockAddr, OMRSOCK_AF_INET, addr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, socket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr), 0);
	ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, 10), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
}

/**
 * Test functions involving IPv4 and IPv6 socket addresses:
 * To Create a Socket Address with the loopback IPv6 address.
 *
 * After creating the address, their ability to bind and listen will be tested.
 * 
 * Address families tested include IPv4 and IPv6.
 */
TEST(PortSockTest, create_IPv6_socket_address)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage sockAddr;
	omrsock_socket_t socket = NULL;
	uint16_t port = 4930;
	int32_t flag = 1;
	uint8_t addr6[16];

	EXPECT_EQ(OMRPORTLIB->sock_inet_pton(OMRPORTLIB, OMRSOCK_AF_INET6, "::1", addr6), 0);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init6(OMRPORTLIB,  &sockAddr, OMRSOCK_AF_INET6, addr6, OMRPORTLIB->sock_htons(OMRPORTLIB, port), 0, 0), 0);
	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET6, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, socket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	if (0 != (OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr))) {
		portTestEnv->log(LEVEL_ERROR, "WARNING: Cannot test IPV6: Failed to find to IPV6 interface. \n");
		EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
		return;
	}
	ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, 10), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
}

/**
 * Test functions involving IPv4 and IPv6 socket addresses:
 * To Create a Socket Address with the in6addr_any IPv6 address.
 *
 * After creating the address, their ability to bind and listen will be tested.
 * 
 * Address families tested include IPv4 and IPv6.
 */
TEST(PortSockTest, create_in6addrany_IPv6_socket_address)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage sockAddr;
	omrsock_socket_t socket = NULL;
	uint16_t port = 4930;
	int32_t flag = 1;
	uint8_t addr6[16];

	EXPECT_EQ(OMRPORTLIB->sock_inet_pton(OMRPORTLIB, OMRSOCK_AF_INET6, "::0", addr6), 0);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init6(OMRPORTLIB, &sockAddr, OMRSOCK_AF_INET6, addr6, OMRPORTLIB->sock_htons(OMRPORTLIB, port), 0, 0), 0);
	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET6, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, socket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr), 0);
	ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, OMRSOCK_MAXCONN), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
}

/**
 * Test functions involving IPv4 and IPv6 socket addresses:
 * To Create a Socket Address with IPv4-mapped IPv6 address.
 *
 * After creating the address, their ability to bind and listen will be tested.
 * 
 * Address families tested include IPv4 and IPv6.
 */
TEST(PortSockTest, create_IPv4_mapped_IPv6_Socket_Address)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage sockAddr;
	omrsock_socket_t socket = NULL;
	uint16_t port = 4930;
	int32_t flag = 1;
	uint8_t addr[4];

	EXPECT_EQ(OMRPORTLIB->sock_inet_pton(OMRPORTLIB, OMRSOCK_AF_INET, "127.0.0.1", addr), 0);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init6(OMRPORTLIB, &sockAddr, OMRSOCK_AF_INET, addr, OMRPORTLIB->sock_htons(OMRPORTLIB, port), 0, 0), 0);
	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET6, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, socket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr), 0);
	ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, 10), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
}

/**
 * Test functions to set up a stream connection by using two sockets, which talk to each other.
 *
 * First, the server starts and listens for connections. Then, the
 * client starts and sends a request to connect to the server. The messages are
 * sent both ways, and it is checked if they were sent correctly.
 * 
 * Address families tested include IPv4 and socket types tested is stream.
 *
 * @note Errors such as failed function calls, failure to create server and/or client, wrong 
 * message sent/received, will be reported.
 */
TEST(PortSockTest, two_socket_stream_communication)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage serverStreamSockAddr;
	omrsock_socket_t serverStreamSocket = NULL;
	uint16_t port = 4930;
	uint8_t serverAddr[4];

	/* To Create a Server Socket and Address */
	uint32_t inaddrAny = OMRPORTLIB->sock_htonl(OMRPORTLIB, OMRSOCK_INADDR_ANY);
	memcpy(serverAddr, &inaddrAny, 4);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &serverStreamSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_STREAM, &serverStreamSocket, &serverStreamSockAddr);

	/* To Create a Client Socket and Address */
	OMRSockAddrStorage clientStreamSockAddr;
	omrsock_socket_t clientStreamSocket = NULL;
	connect_client_to_server(OMRPORTLIB, "localhost", NULL, OMRSOCK_AF_INET, OMRSOCK_STREAM, &clientStreamSocket, &clientStreamSockAddr, &serverStreamSockAddr);

	/* Accept Connection */
	OMRSockAddrStorage connectedClientStreamSockAddr;
	omrsock_socket_t connectedClientStreamSocket = NULL;
	ASSERT_EQ(OMRPORTLIB->sock_accept(OMRPORTLIB, serverStreamSocket, &connectedClientStreamSockAddr, &connectedClientStreamSocket), 0);

	const char *msg = "This is an omrsock test for 2 socket stream communications.";
	int32_t bytesLeft = strlen(msg) + 1;
	uint8_t *cursor = (uint8_t *)msg;
	int32_t bytesSent = 0;
	while (0 != bytesLeft) {
		bytesSent = OMRPORTLIB->sock_send(OMRPORTLIB, connectedClientStreamSocket, cursor, bytesLeft, 0);
		ASSERT_GE(bytesSent, 0);
		bytesLeft -= bytesSent;
		cursor += bytesSent;
	}

	char buf[100] = {0};
	bytesLeft = strlen(msg) + 1;
	cursor = (uint8_t *)buf;
	int32_t bytesRecv = 0;
	while (0 != bytesLeft) {
		bytesRecv = OMRPORTLIB->sock_recv(OMRPORTLIB, clientStreamSocket, cursor, bytesLeft, 0);
		ASSERT_GE(bytesRecv, 0);
		bytesLeft -= bytesRecv;
		cursor += bytesRecv;
	}
	
	EXPECT_STREQ(msg, buf);

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &connectedClientStreamSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientStreamSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverStreamSocket), 0);
}

/**
 * Test functions to set up a datagram connection by using two sockets, which talk to each other.
 *
 * First, the server is set up, and then, the client is set up sends a request to 
 * connect to the server (this is optional). The messages are sent both ways, and 
 * it is checked if they were sent correctly.
 * 
 * Address families tested include IPv4 in this test case. Socket types
 * tested is datagram.
 *
 * @note Errors such as failed function calls, failure to create server and/or client, wrong 
 * message sent/received, will be reported.
 */
TEST(PortSockTest, two_socket_datagram_communication)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage serverSockAddr;
	omrsock_socket_t serverSocket = NULL;
	uint16_t port = 4930;
	uint8_t serverAddr[4];

	/* To Create a Server Socket and Address */
	EXPECT_EQ(OMRPORTLIB->sock_inet_pton(OMRPORTLIB, OMRSOCK_AF_INET, "127.0.0.1", serverAddr), 0);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB,  &serverSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	/* Datagram server sockets does not need to listen and accept */
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_DGRAM, &serverSocket, &serverSockAddr);

	OMRSockAddrStorage clientSockAddr;
	omrsock_socket_t clientSocket = NULL;
	/* Connect is optional for datagram clients */
	connect_client_to_server(OMRPORTLIB, "localhost", "16403", OMRSOCK_AF_INET, OMRSOCK_DGRAM, &clientSocket, &clientSockAddr, &serverSockAddr);

	/* Datagram Sendto and Recvfrom test. Sendto and Recvfrom are called multiple tests to ensure it has been
	*	sent and received.
	*/
	OMRTimeval timeSend;
	OMRTimeval timeRecv;
	EXPECT_EQ(OMRPORTLIB->sock_timeval_init(OMRPORTLIB, &timeSend, 2, 0), 0);
	EXPECT_EQ(OMRPORTLIB->sock_timeval_init(OMRPORTLIB, &timeRecv, 3, 0), 0);
	ASSERT_EQ(OMRPORTLIB->sock_setsockopt_timeval(OMRPORTLIB, serverSocket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_SNDTIMEO, &timeSend), 0);
	ASSERT_EQ(OMRPORTLIB->sock_setsockopt_timeval(OMRPORTLIB, clientSocket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_RCVTIMEO, &timeRecv), 0);

	/* Send 50 times. This should be enough for the message to be sent. */
	char msgDgram[100] = "This is an omrsock test for datagram communications.";
	int32_t	bytesTotal = strlen(msgDgram) + 1;

	for (int32_t i = 0; i < 50; i++) {
		OMRPORTLIB->sock_sendto(OMRPORTLIB, serverSocket, (uint8_t *)msgDgram, bytesTotal, 0, &clientSockAddr);
	}

	/* Receive */
	char bufDgram[100] = "";
	int32_t bytesRecv = 0;

	for (int32_t i = 0; i < 200; i++) {
		bytesRecv = OMRPORTLIB->sock_recvfrom(OMRPORTLIB, clientSocket, (uint8_t *)bufDgram, bytesTotal, 0, &serverSockAddr);
		if (bytesRecv == bytesTotal) {
			break;
		}
	}

	EXPECT_EQ(strcmp(msgDgram, bufDgram), 0);

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverSocket), 0);
}

/**
 * Equal operator to compare contents of OMRTimeval struct socket option values.
 */
bool
operator==(const OMRTimeval &lhs, const OMRTimeval &rhs)
{
	return (lhs.data.tv_sec == rhs.data.tv_sec) && (lhs.data.tv_usec == rhs.data.tv_usec);
}

/**
 * Equal operator to compare contents of OMRLinger struct socket option values.
 */
bool
operator==(const OMRLinger &lhs, const OMRLinger &rhs)
{
	if (lhs.data.l_onoff != 0) {
		return (rhs.data.l_onoff != 0) && (lhs.data.l_linger == rhs.data.l_linger);
	}
	return rhs.data.l_onoff == 0;
}

/**
 * Custom print for contents of OMRTimeval struct when comparing contents in test unit.
 */
void
PrintTo(const OMRTimeval &x, std::ostream *os)
{
	*os << "{" << x.data.tv_sec << ", " <<  x.data.tv_usec << "}";
}

/**
 * Custome print for contents of OMRLinger struct when comparing contents in test unit.
 */
void
PrintTo(const OMRLinger &x, std::ostream *os)
{
	*os << "{" << x.data.l_onoff << ", " <<  x.data.l_linger << "}";
}

/**
 * Test socket options functions by test setting and getting the socket options.
 *
 * First, the linger and timeval structs will be allocated and the corresponding
 * init functions will be called to set up those structs. Then, an arbitrary IPv4
 * stream socket will be opened and the socket options will be set and get. The 
 * test will check if the set and get option values are the same.
 */
TEST(PortSockTest, socket_options)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	OMRTimeval timeVal;
	OMRLinger lingerVal;
	omrsock_socket_t sock = NULL;

	EXPECT_EQ(OMRPORTLIB->sock_timeval_init(OMRPORTLIB, &timeVal, 2, 0), 0);
	EXPECT_EQ(OMRPORTLIB->sock_linger_init(OMRPORTLIB, &lingerVal, 1, 2), 0);
	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &sock, OMRSOCK_AF_INET, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);

	/* Call omrsock_setsockopt to set socket options. */
	int32_t flag = 1;
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_KEEPALIVE, &flag), 0);
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_linger(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_LINGER, &lingerVal), 0);
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_timeval(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_RCVTIMEO, &timeVal), 0);
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_timeval(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_SNDTIMEO, &timeVal), 0);
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, sock, OMRSOCK_IPPROTO_TCP, OMRSOCK_TCP_NODELAY, &flag), 0);

	/* Call omrsock_getsockopt to check if the results matches the inputs earlier. */
	OMRTimeval timeResult;
	OMRLinger lingerResult;
	int32_t flagResult;

	EXPECT_EQ(OMRPORTLIB->sock_getsockopt_int(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_KEEPALIVE, &flagResult), 0);
	EXPECT_NE(flagResult, 0);

	EXPECT_EQ(OMRPORTLIB->sock_getsockopt_linger(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_LINGER, &lingerResult), 0);
	EXPECT_EQ(lingerResult, lingerVal);

	EXPECT_EQ(OMRPORTLIB->sock_getsockopt_timeval(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_RCVTIMEO, &timeResult), 0);
	EXPECT_EQ(timeResult, timeVal);

	EXPECT_EQ(OMRPORTLIB->sock_getsockopt_timeval(OMRPORTLIB, sock, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_SNDTIMEO, &timeResult), 0);
	EXPECT_EQ(timeResult, timeVal);

	EXPECT_EQ(OMRPORTLIB->sock_getsockopt_int(OMRPORTLIB, sock, OMRSOCK_IPPROTO_TCP, OMRSOCK_TCP_NODELAY, &flagResult), 0);
	EXPECT_NE(flagResult, 0);

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &sock), 0);
}

TEST(PortSockTest, socket_flags)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	OMRSockAddrStorage sockAddr;
	omrsock_socket_t socket = NULL;
	uint32_t inaddrAny;
	uint16_t port = 4930;
	uint8_t addr[4];
	int32_t flag = 1;

	/* Set up address */
	inaddrAny = OMRPORTLIB->sock_htonl(OMRPORTLIB, OMRSOCK_INADDR_ANY);
	memcpy(addr, &inaddrAny, 4);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &sockAddr, OMRSOCK_AF_INET, addr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);

	/* Test setting non-blocking using OR in socktype field. */

	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET, OMRSOCK_STREAM | OMRSOCK_O_NONBLOCK, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, socket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr), 0);
	ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, 10), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);

	/* Test setting non-blocking using omrsock_fcntl. */

	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
	/** Set socket address to be reusable in case this address is in TIME_WAIT state. */
	EXPECT_EQ(OMRPORTLIB->sock_setsockopt_int(OMRPORTLIB, socket, OMRSOCK_SOL_SOCKET, OMRSOCK_SO_REUSEADDR, &flag), 0);
	EXPECT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, socket, OMRSOCK_O_NONBLOCK), 0);
	ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, socket, &sockAddr), 0);
	ASSERT_EQ(OMRPORTLIB->sock_listen(OMRPORTLIB, socket, 10), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
}

/**
 * Test nonblocking functionality by connecting two sockets while it is nonblocking.
 *
 * The server is started. The client uses getaddrinfo to create a nonblocking socket
 * The socket then tries to connect to server using nonblocking connect, which returns
 * OMRPORT_ERROR_SOCKET_INPROGRESS or OMRPORT_ERROR_SOCKET_ALREADY_CONNECTED.
 */
TEST(PortSockTest, socket_nonblock_connection)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage serverSockAddr;
	omrsock_socket_t serverSocket = NULL;
	uint16_t port = 4930;
	uint8_t serverAddr[4];
	int32_t rc = 0;

	/* To Create a Server Socket and Address */
	EXPECT_EQ(OMRPORTLIB->sock_inet_pton(OMRPORTLIB, OMRSOCK_AF_INET, "127.0.0.1", serverAddr), 0);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB,  &serverSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_STREAM, &serverSocket, &serverSockAddr);

	/* To Create a Client Socket and Address */
	OMRSockAddrStorage clientSockAddr;
	omrsock_socket_t clientSocket = NULL;
	omrsock_addrinfo_t hints = NULL;
	OMRAddrInfoNode result;
	uint32_t length = 0;

	EXPECT_EQ(OMRPORTLIB->sock_getaddrinfo_create_hints(OMRPORTLIB, &hints, OMRSOCK_AF_INET, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT, 0), 0);
	EXPECT_EQ(OMRPORTLIB->sock_getaddrinfo(OMRPORTLIB, "localhost", NULL, hints, &result), 0);
	EXPECT_EQ(OMRPORTLIB->sock_addrinfo_length(OMRPORTLIB, &result, &length), 0);
	ASSERT_NE(length, uint32_t(0));

	/* Create a nonblocking socket with results */
	for (uint32_t i = 0; i < length; i++) {
		if (0 == OMRPORTLIB->sock_socket(OMRPORTLIB, &clientSocket, OMRSOCK_AF_INET, OMRSOCK_STREAM | OMRSOCK_O_NONBLOCK, OMRSOCK_IPPROTO_DEFAULT)) {
			ASSERT_NE(clientSocket, (void *)NULL);
			EXPECT_EQ(OMRPORTLIB->sock_addrinfo_address(OMRPORTLIB, &result, i, &clientSockAddr), 0);
			ASSERT_EQ(OMRPORTLIB->sock_bind(OMRPORTLIB, clientSocket, &clientSockAddr), 0);
			break;
		}
	}
	ASSERT_NE(clientSocket, (void *)NULL);
	EXPECT_EQ(OMRPORTLIB->sock_freeaddrinfo(OMRPORTLIB, &result), 0);

	/* Client Attempts Connection */
	while (0 != (rc = OMRPORTLIB->sock_connect(OMRPORTLIB, clientSocket, &serverSockAddr))) {
		if (OMRPORT_ERROR_SOCKET_INPROGRESS == rc) {
			// Still in the middle of attempting connection.
			continue;
		}
		else if (OMRPORT_ERROR_SOCKET_ALREADY_CONNECTED == rc) {
			// Connected
			break;
		}
		else {
			FAIL() << "Error other than INPROGRESS and ALREADY_CONNECTED were returned for connect: " << ::testing::PrintToString(rc);
		}
	}

	/* Server Accepts Connection */
	OMRSockAddrStorage connectedServerSockAddr;
	omrsock_socket_t connectedServerSocket = NULL;
	ASSERT_EQ(OMRPORTLIB->sock_accept(OMRPORTLIB, serverSocket, &connectedServerSockAddr, &connectedServerSocket), 0);

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &connectedServerSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverSocket), 0);
}

/**
 * Test nonblocking functionality by two nonblocking socket communication.
 *
 * First, a connection is established between server and client. The server
 * and client is then set to be nonblocking. Communication between server and
 * client is then tested using OMRPORT_ERROR_SOCKET_WOULDBLOCK.
 */
TEST(PortSockTest, two_socket_nonblock_communication_basic)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage serverSockAddr;
	omrsock_socket_t serverSocket = NULL;
	OMRSockAddrStorage clientSockAddr;
	omrsock_socket_t clientSocket = NULL;
	OMRSockAddrStorage connectedServerSockAddr;
	omrsock_socket_t connectedServerSocket = NULL;
	uint16_t port = 4930;
	uint32_t inaddrAny;
	uint8_t serverAddr[4];

	/* To Create a Server Socket and Address */
	inaddrAny = OMRPORTLIB->sock_htonl(OMRPORTLIB, OMRSOCK_INADDR_ANY);
	memcpy(serverAddr, &inaddrAny, 4);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &serverSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_STREAM, &serverSocket, &serverSockAddr);

	/* To Create a Client Socket and Address */
	connect_client_to_server(OMRPORTLIB, "localhost", NULL, OMRSOCK_AF_INET, OMRSOCK_STREAM, &clientSocket, &clientSockAddr, &serverSockAddr);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, clientSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Accept Connection */
	ASSERT_EQ(OMRPORTLIB->sock_accept(OMRPORTLIB, serverSocket, &connectedServerSockAddr, &connectedServerSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, connectedServerSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Send and Recv using non-blocking sockets. */
	char buf[100] = {0};
	const char *msg = "This is an omrsock test for 2 socket stream communications.";
	uint8_t *cursor = NULL;
	int32_t bytesSent = 0;
	int32_t bytesRecv = 0;
	int32_t bytesLeft = 0;

	/* Trying to receive on client side when nothing has been sent from the server. */
	cursor = (uint8_t *)buf;
	bytesRecv = OMRPORTLIB->sock_recv(OMRPORTLIB, clientSocket, cursor, strlen(msg) + 1, 0);
	ASSERT_GE(bytesRecv, -1);
	ASSERT_EQ(OMRPORTLIB->error_last_error_number(OMRPORTLIB), OMRPORT_ERROR_SOCKET_WOULDBLOCK);

	/* Send from the server using non-blocking server socket. */
	cursor = (uint8_t *)msg;
	bytesLeft = strlen(msg) + 1;
	bytesSent = 0;
	while (0 != bytesLeft) {
		bytesSent = OMRPORTLIB->sock_send(OMRPORTLIB, connectedServerSocket, cursor, bytesLeft, 0);
		if (0 > bytesSent) {
			// If bytesSent is -1, then it must mean it is still waiting to send.
			ASSERT_EQ(OMRPORTLIB->error_last_error_number(OMRPORTLIB), OMRPORT_ERROR_SOCKET_WOULDBLOCK);
			bytesSent = 0;
		}
		bytesLeft -= bytesSent;
		cursor += bytesSent;
	}

	/* Receive on the client side using non-blocking client socket. */
	cursor = (uint8_t *)buf;
	bytesLeft = strlen(msg) + 1;
	bytesRecv = 0;
	while (0 != bytesLeft) {
		bytesRecv = OMRPORTLIB->sock_recv(OMRPORTLIB, clientSocket, cursor, bytesLeft, 0);
		if (0 >= bytesRecv) {
			// If bytesRecv is -1, then it must mean it is still waiting to receive the message.
			ASSERT_EQ(OMRPORTLIB->error_last_error_number(OMRPORTLIB), OMRPORT_ERROR_SOCKET_WOULDBLOCK);
			bytesRecv = 0;
		}
		bytesLeft -= bytesRecv;
		cursor += bytesRecv;
	}

	EXPECT_STREQ(msg, buf);

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &connectedServerSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverSocket), 0);
}

/**
 * Test nonblocking functionality by two nonblocking socket communication.
 * The nonblocking is assigned to server at the very beginning. Accept
 * nonblocking is performed using OMRPORT_ERROR_SOCKET_WOULDBLOCK.
 * Communication between server and client is then tested using 
 * OMRPORT_ERROR_SOCKET_WOULDBLOCK.
 */
TEST(PortSockTest, two_socket_nonblock_server_communication)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRSockAddrStorage serverSockAddr;
	omrsock_socket_t serverSocket = NULL;
	OMRSockAddrStorage clientSockAddr;
	omrsock_socket_t clientSocket = NULL;
	OMRSockAddrStorage connectedServerSockAddr;
	omrsock_socket_t connectedServerSocket = NULL;
	uint16_t port = 4930;
	uint32_t inaddrAny;
	uint8_t serverAddr[4];
	int32_t rc = 0;

	/* To Create a Server Socket and Address */
	inaddrAny = OMRPORTLIB->sock_htonl(OMRPORTLIB, OMRSOCK_INADDR_ANY);
	memcpy(serverAddr, &inaddrAny, 4);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &serverSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_STREAM | OMRSOCK_O_NONBLOCK, &serverSocket, &serverSockAddr);

	/* To Create a Client Socket and Address */
	connect_client_to_server(OMRPORTLIB, "localhost", NULL, OMRSOCK_AF_INET, OMRSOCK_STREAM, &clientSocket, &clientSockAddr, &serverSockAddr);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, clientSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Accept Connection */
	while (0 != (rc = OMRPORTLIB->sock_accept(OMRPORTLIB, serverSocket, &connectedServerSockAddr, &connectedServerSocket))) {
		if (OMRPORT_ERROR_SOCKET_WOULDBLOCK == rc) {
			// Still in the middle of accepting next connection request.
			continue;
		}
		else {
			FAIL() << "Error other than OMRPORT_ERROR_SOCKET_WOULDBLOCK was returned for accept: " << ::testing::PrintToString(rc);
		}
	}

	/* Send and Recv using non-blocking sockets. */
	char buf[100] = {0};
	const char *msg = "This is an omrsock test for 2 socket stream communications.";
	uint8_t *cursor = NULL;
	int32_t bytesSent = 0;
	int32_t bytesRecv = 0;
	int32_t bytesLeft = 0;

	/* Trying to receive on client side when nothing has been sent from the server. */
	cursor = (uint8_t *)buf;
	bytesRecv = OMRPORTLIB->sock_recv(OMRPORTLIB, clientSocket, cursor, strlen(msg) + 1, 0);
	ASSERT_GE(bytesRecv, -1);
	ASSERT_EQ(OMRPORTLIB->error_last_error_number(OMRPORTLIB), OMRPORT_ERROR_SOCKET_WOULDBLOCK);

	/* Send from the server using non-blocking server socket. */
	cursor = (uint8_t *)msg;
	bytesLeft = strlen(msg) + 1;
	bytesSent = 0;
	while (0 != bytesLeft) {
		bytesSent = OMRPORTLIB->sock_send(OMRPORTLIB, connectedServerSocket, cursor, bytesLeft, 0);
		if (0 > bytesSent) {
			// If bytesSent is -1, then it must mean it is still waiting to send.
			ASSERT_EQ(OMRPORTLIB->error_last_error_number(OMRPORTLIB), OMRPORT_ERROR_SOCKET_WOULDBLOCK);
			bytesSent = 0;
		}
		bytesLeft -= bytesSent;
		cursor += bytesSent;
	}

	/* Receive on the client side using non-blocking client socket. */
	cursor = (uint8_t *)buf;
	bytesLeft = strlen(msg) + 1;
	bytesRecv = 0;
	while (0 != bytesLeft) {
		bytesRecv = OMRPORTLIB->sock_recv(OMRPORTLIB, clientSocket, cursor, bytesLeft, 0);
		if (0 >= bytesRecv) {
			// If bytesRecv is -1, then it must mean it is still waiting to receive the message.
			ASSERT_EQ(OMRPORTLIB->error_last_error_number(OMRPORTLIB), OMRPORT_ERROR_SOCKET_WOULDBLOCK);
			bytesRecv = 0;
		}
		bytesLeft -= bytesRecv;
		cursor += bytesRecv;
	}

	EXPECT_STREQ(msg, buf);

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &connectedServerSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverSocket), 0);
}

/**
 * Test OMRFdSet functions by test setting, getting, and clearing the OMRFdSet.
 *
 * First, a socket and OMRFdSet is created. Then it is first zero-ed, and then set,
 * and checked if it is set. The fdset is then cleared and set many times to make sure
 * it works.
 */
TEST(PortSockTest, fdset_functionality)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	omrsock_socket_t socket = NULL;
	OMRFdSet fdsetStruct;
	omrsock_fdset_t fdset = &fdsetStruct;
	
	ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &socket, OMRSOCK_AF_INET, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
	/* Should always zero the OMRFdSet before setting */
	OMRPORTLIB->sock_fdset_zero(OMRPORTLIB, fdset);
	EXPECT_FALSE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, socket, fdset));

	/* Set the OMRFdSet and it should return TRUE when check if it is set */
	OMRPORTLIB->sock_fdset_set(OMRPORTLIB, socket, fdset);
	EXPECT_TRUE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, socket, fdset));

	/* Clear the OMRFdSet for the socket and it should return FALSE when check if socket is set */
	OMRPORTLIB->sock_fdset_clr(OMRPORTLIB, socket, fdset);
	EXPECT_FALSE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, socket, fdset));

	/* Zero the OMRFdSet and it should return FALSE when check if socket is set */
	OMRPORTLIB->sock_fdset_set(OMRPORTLIB, socket, fdset);
	OMRPORTLIB->sock_fdset_zero(OMRPORTLIB, fdset);
	EXPECT_FALSE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, socket, fdset));

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &socket), 0);
}

/**
 * Test select functionaility by testing omrsock_select using the omrsock_fdset functions.
 *
 * First, a nonblocking connection is established between the server and client. An OMRFdSet
 * structure is created and select OMRTimeVal is created and set to 2 seconds timeout. Zero the
 * OMRFdSet and add connected server socket and client socket to OMRFdSet using omrsock_fdset_set.
 * Call omrsock_select with the OMRFdSet as read_fds and write_fds. Check that the number of returned
 * sockets that are ready makes sense.
 */
TEST(PortSockTest, select_functionality)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	OMRSockAddrStorage serverSockAddr;
	omrsock_socket_t serverSocket = NULL;
	OMRSockAddrStorage clientSockAddr;
	omrsock_socket_t clientSocket = NULL;
	OMRSockAddrStorage connectedServerSockAddr;
	omrsock_socket_t connectedServerSocket = NULL;
	uint16_t port = 4930;
	uint8_t serverAddr[4];

	/* To Create a Server Socket and Address */
	EXPECT_EQ(OMRPORTLIB->sock_inet_pton(OMRPORTLIB, OMRSOCK_AF_INET, "127.0.0.1", serverAddr), 0);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB,  &serverSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_STREAM, &serverSocket, &serverSockAddr);

	/* To Create a Client Socket and Address */
	connect_client_to_server(OMRPORTLIB, (char *)"localhost", NULL, OMRSOCK_AF_INET, OMRSOCK_STREAM, &clientSocket, &clientSockAddr, &serverSockAddr);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, clientSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Accept Connection */
	ASSERT_EQ(OMRPORTLIB->sock_accept(OMRPORTLIB, serverSocket, &connectedServerSockAddr, &connectedServerSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, connectedServerSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Create an OMRFdSet and create a timeout for for the select function */
	OMRFdSet fdSet;
	OMRTimeval timeOut;
	EXPECT_EQ(OMRPORTLIB->sock_timeval_init(OMRPORTLIB, &timeOut, 2, 0), 0);

	/* Set OMRFdSet to check server and client sockets for communication */
	OMRPORTLIB->sock_fdset_zero(OMRPORTLIB, &fdSet);
	OMRPORTLIB->sock_fdset_set(OMRPORTLIB, connectedServerSocket, &fdSet);
	OMRPORTLIB->sock_fdset_set(OMRPORTLIB, clientSocket, &fdSet);

	/* Check if the server and client sockets are ready for write. Both should be ready. */
	ASSERT_EQ(OMRPORTLIB->sock_select(OMRPORTLIB, NULL, &fdSet, NULL, &timeOut), 2);
	EXPECT_TRUE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, connectedServerSocket, &fdSet));
	EXPECT_TRUE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, clientSocket, &fdSet));

	/* Send the message from server to client. It should be ready to send. */
	const char *msg = "This is an omrsock test for 2 socket stream communications.";
	ASSERT_GT(OMRPORTLIB->sock_send(OMRPORTLIB, connectedServerSocket, (uint8_t *)msg, strlen(msg) + 1, 0), 0);

	/* Check if server and client have anything to receive. Client should be ready to receive.*/
	ASSERT_EQ(OMRPORTLIB->sock_select(OMRPORTLIB, &fdSet, NULL, NULL, &timeOut), 1);
	EXPECT_FALSE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, connectedServerSocket, &fdSet));
	EXPECT_TRUE(OMRPORTLIB->sock_fdset_isset(OMRPORTLIB, clientSocket, &fdSet));

	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &connectedServerSocket), 0);
}

/**
 * Test poll functionaility by testing omrsock_poll using the OMRPollFd structure.
 *
 * First, a nonblocking connection is established between the server and client. An
 * OMRPollFd array is created and intialized with the I/O event(s) to watch for. After
 * omrsock_poll returns and server is ready to send, server sends a message to client.
 * omrsock_poll is called again and indicates when client is ready to receive. Client
 * should successfully receive the message.
 */
TEST(PortSockTest, poll_functionality_basic)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	OMRSockAddrStorage serverSockAddr;
	omrsock_socket_t serverSocket = NULL;
	OMRSockAddrStorage clientSockAddr;
	omrsock_socket_t clientSocket = NULL;
	OMRSockAddrStorage connectedServerSockAddr;
	omrsock_socket_t connectedServerSocket = NULL;
	uint16_t port = 4930;
	uint32_t inaddrAny;
	uint8_t serverAddr[4];
	int32_t rc = 0;

	/* To Create a Server Socket and Address */
	inaddrAny = OMRPORTLIB->sock_htonl(OMRPORTLIB, OMRSOCK_INADDR_ANY);
	memcpy(serverAddr, &inaddrAny, 4);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &serverSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_STREAM, &serverSocket, &serverSockAddr);

	/* To Create a Client Socket and Address */
	connect_client_to_server(OMRPORTLIB, (char *)"localhost", NULL, OMRSOCK_AF_INET, OMRSOCK_STREAM, &clientSocket, &clientSockAddr, &serverSockAddr);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, clientSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Accept Connection */
	ASSERT_EQ(OMRPORTLIB->sock_accept(OMRPORTLIB, serverSocket, &connectedServerSockAddr, &connectedServerSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, connectedServerSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Create pollFd array and initialize server and client sockets to watch for POLLIN. None should be ready. */
	OMRPollFd pollArray[2];
	EXPECT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[0], connectedServerSocket, OMRSOCK_POLLIN), 0);
	EXPECT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[1], clientSocket, OMRSOCK_POLLIN), 0);
	ASSERT_EQ(OMRPORTLIB->sock_poll(OMRPORTLIB, pollArray, 2, 1000), 0);

	/* Initialize server sockets to watch for  both POLLIN and POLLOUT. Server POLLOUT should be ready.
	 * Give poll up to 10 times to be ready.
	 */
	EXPECT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[0], connectedServerSocket, OMRSOCK_POLLIN | OMRSOCK_POLLOUT), 0);
	for (int32_t i = 0; i < 10; i++) {
		if (1 == (rc = OMRPORTLIB->sock_poll(OMRPORTLIB, pollArray, 2, 1000))) {
			break;
		}
	}
	ASSERT_EQ(rc, 1);

	/* SocketSet and revents are returned from @ref omrsock_get_pollfd_info. */
	omrsock_socket_t socketSet = NULL;
	int16_t revents = 0;

	/* Use poll to send a message from server to client. */
	const char *msg = "This is an omrsock test for 2 socket stream communications.\n";
	int32_t bytesSent = 0;
	char buf[100] = {0};
	int32_t bytesRecv = 0;

	for (int32_t i = 0; i < 2; i++) {
		OMRPORTLIB->sock_get_pollfd_info(OMRPORTLIB, &pollArray[i], &socketSet, &revents);
		if ((revents & OMRSOCK_POLLOUT) != 0) {
			ASSERT_EQ(connectedServerSocket, socketSet);
			bytesSent = OMRPORTLIB->sock_send(OMRPORTLIB, socketSet, (uint8_t *)msg, strlen(msg) + 1, 0);
		}
	}

	/* Use poll for client to received a message from server. Server POLLOUT and client POLLIN should be ready.
	 * Give poll up to 10 times to be ready.
	 */
	for (int32_t i = 0; i < 10; i++) {
		if (2 == (rc = OMRPORTLIB->sock_poll(OMRPORTLIB, pollArray, 2, 1000))) {
			break;
		}
	}
	ASSERT_EQ(rc, 2);

	for (int32_t i = 0; i < 2; i++) {
		OMRPORTLIB->sock_get_pollfd_info(OMRPORTLIB, &pollArray[i], &socketSet, &revents);
		if ((revents & OMRSOCK_POLLIN) != 0) {
			ASSERT_EQ(clientSocket, socketSet);
			bytesRecv = OMRPORTLIB->sock_recv(OMRPORTLIB, socketSet, (uint8_t *)buf, strlen(msg) + 1, 0);
		}
	}
	EXPECT_EQ(bytesSent, bytesRecv);

	EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverSocket), 0);
	EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientSocket), 0);
	EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &connectedServerSocket), 0);
}

/**
 * Test dynamic allocation in @ref omrsock_poll.
 *
 * First, a nonblocking connection is established between the server and client. An
 * OMRPollFd array and a socket array of 8 is created. The sockets are created to test
 * dynamic allocation in omrsock_poll in case where there are more than 8 OMRPollFd in
 * array, and thus more than 8 sockets to watch for. Communication is tested between
 * the two connected sockets: server and client.
 */
TEST(PortSockTest, poll_functionality_many_sockets)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	OMRSockAddrStorage serverSockAddr;
	omrsock_socket_t serverSocket = NULL;
	OMRSockAddrStorage clientSockAddr;
	omrsock_socket_t clientSocket = NULL;
	OMRSockAddrStorage connectedServerSockAddr;
	omrsock_socket_t connectedServerSocket = NULL;
	uint16_t port = 4930;
	uint32_t inaddrAny;
	uint8_t serverAddr[4];
	int32_t rc = 0;

	/* To Create a Server Socket and Address */
	inaddrAny = OMRPORTLIB->sock_htonl(OMRPORTLIB, OMRSOCK_INADDR_ANY);
	memcpy(serverAddr, &inaddrAny, 4);
	EXPECT_EQ(OMRPORTLIB->sock_sockaddr_init(OMRPORTLIB, &serverSockAddr, OMRSOCK_AF_INET, serverAddr, OMRPORTLIB->sock_htons(OMRPORTLIB, port)), 0);
	start_server(OMRPORTLIB, OMRSOCK_AF_INET, OMRSOCK_STREAM, &serverSocket, &serverSockAddr);

	/* To Create a Client Socket and Address */
	connect_client_to_server(OMRPORTLIB, (char *)"localhost", NULL, OMRSOCK_AF_INET, OMRSOCK_STREAM, &clientSocket, &clientSockAddr, &serverSockAddr);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, clientSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Accept Connection */
	ASSERT_EQ(OMRPORTLIB->sock_accept(OMRPORTLIB, serverSocket, &connectedServerSockAddr, &connectedServerSocket), 0);
	ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, connectedServerSocket, OMRSOCK_O_NONBLOCK), 0);

	/* Create pollFd array  of 10 pollfds and initialize. */
	OMRPollFd pollArray[10];
	omrsock_socket_t socketPoll = NULL;
	int16_t revents = 0;

	const uint8_t SERVER_SOCKET_POLL_IDX = 8;
	const uint8_t CLIENT_SOCKET_POLL_IDX = 9;

	/* Create an array of additional sockets for testing dynamic allocation in omrsock_poll. */
	omrsock_socket_t sockets[8];
	for (int32_t i = 0; i < 8; i++) {
		ASSERT_EQ(OMRPORTLIB->sock_socket(OMRPORTLIB, &sockets[i], OMRSOCK_AF_INET, OMRSOCK_STREAM, OMRSOCK_IPPROTO_DEFAULT), 0);
		ASSERT_EQ(OMRPORTLIB->sock_fcntl(OMRPORTLIB, sockets[i], OMRSOCK_O_NONBLOCK), 0);
#if defined(OMRSOCK_POLLHUP)
		ASSERT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[i], sockets[i], OMRSOCK_POLLIN | OMRSOCK_POLLHUP), 0);
#else /* defined(OMRSOCK_POLLHUP) */
		ASSERT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[i], sockets[i], OMRSOCK_POLLIN), 0);
#endif /* defined(OMRSOCK_POLLHUP) */
	}
	EXPECT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[SERVER_SOCKET_POLL_IDX], connectedServerSocket, OMRSOCK_POLLIN), 0);
	EXPECT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[CLIENT_SOCKET_POLL_IDX], clientSocket, OMRSOCK_POLLOUT), 0);

	/* Check that only client POLLOUT is ready. Give poll up to 10 times to be ready. */
	const char *msg = "This is an omrsock test for 2 socket stream communications.\n";
	int32_t bytesSent = 0;

	for (int32_t i = 0; i < 10; i++) {
		if (1 == (rc = OMRPORTLIB->sock_poll(OMRPORTLIB, pollArray, 10, 1000))) {
			break;
		}
	}
	ASSERT_EQ(rc, 1);

	/* The client socket should be ready to write. */
	OMRPORTLIB->sock_get_pollfd_info(OMRPORTLIB, &pollArray[CLIENT_SOCKET_POLL_IDX], &socketPoll, &revents);
	ASSERT_EQ(clientSocket, socketPoll);
	ASSERT_NE(revents & OMRSOCK_POLLOUT, 0);

	bytesSent = OMRPORTLIB->sock_send(OMRPORTLIB, clientSocket, (uint8_t *)msg, strlen(msg) + 1, 0);
	ASSERT_GT(bytesSent, 0);


	/* Ignore OMRSOCK_POLLOUT on client socket, since its state is indeterminate
	 * Listen for HUP, since an error is returned if sock_pollfd_init recieves an event arg of 0. */
#if defined(OMRSOCK_POLLHUP)
	EXPECT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[CLIENT_SOCKET_POLL_IDX], clientSocket, OMRSOCK_POLLHUP), 0);
#else /* defined(OMRSOCK_POLLHUP) */
	EXPECT_EQ(OMRPORTLIB->sock_pollfd_init(OMRPORTLIB, &pollArray[CLIENT_SOCKET_POLL_IDX], clientSocket, 0), 0);
#endif /* defined(OMRSOCK_POLLHUP) */
	/* Check that server POLLIN is ready. Give poll up to 10 times to be ready. */
	bool succeeded = false;
	for (int32_t i = 0; i < 10; i++) {
		rc = OMRPORTLIB->sock_poll(OMRPORTLIB, pollArray, 10, 1000);
		if (rc > 0) {
			OMRPORTLIB->sock_get_pollfd_info(OMRPORTLIB, &pollArray[SERVER_SOCKET_POLL_IDX], &socketPoll, &revents);
			ASSERT_EQ(socketPoll, connectedServerSocket);
			if (0 != (revents & OMRSOCK_POLLIN)) {
				succeeded = true;
				break;
			}
		}
	}
	ASSERT_TRUE(succeeded);

	EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &serverSocket), 0);
	EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &clientSocket), 0);
	EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &connectedServerSocket), 0);

	for (int32_t i = 0; i < 8; i++) {
		EXPECT_EQ(OMRPORTLIB->sock_close(OMRPORTLIB, &sockets[i]), 0);
	}
}
