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

/**
 * @file
 * @ingroup Port
 * @brief Sockets
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "omrsock.h"

/**
 * Returns hints as a double pointer to an OMRAddInfoNode structure.
 * 
 * This hints structure is used to modify the results returned by a call to @ref omrsock_getaddrinfo. 
 *
 * @param[in] portLibrary The port library.
 * @param[out] hints The filled-in hints structure
 * @param[in] family A address family type
 * @param[in] socktype A socket type
 * @param[in] protocol A protocol family
 * @param[in] flags Flags for modifying the result. If you wish to use multiple flags, perform the OR operation on the flags before passing it in.
 *
 * @return
 * @arg 0, if no errors occurred, otherwise the (negative) error code
 *
 * @note current supported family types are:
 * @arg OMRSOCK_AF_UNSPEC
 * @arg OMRSOCK_AF_INET4
 * @arg OMRSOCK_AF_INET6
 *
 * @note Works with both IPv4 and IPv6 support.
 */
int32_t
omrsock_getaddrinfo_create_hints(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t *hints, int family, int socktype, int protocol, int flags)
{
	return 0;
}

/**
 * Answers a list of addresses as an opaque struct in "result".
 * 
 * Use the following functions to extract the details:
 * @arg @ref omrsock_getaddrinfo_length
 * @arg @ref omrsock_getaddrinfo_family
 * @arg @ref omrsock_getaddrinfo_socktype
 * @arg @ref omrsock_getaddrinfo_protocol

 * @param[in] portLibrary The port library.
 * @param[in] node The name of the host in either host name format or in IPv4 or IPv6 accepted notations.
 * @param[in] service The port of the host in string form.
 * @param[in] hints Hints on what results are returned (can be NULL for default action). Use omrsock_getaddrinfo_create_hints to create the hints.
 * @param[out] result An opaque pointer to a list of results (OMRAddrInfoNode must be preallocated).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note you must free the "result" structure with @ref omrsock_freeaddrinfo to free up memory.
 */
int32_t
omrsock_getaddrinfo(struct OMRPortLibrary *portLibrary, char *node, char *service, omrsock_addrinfo_t hints, omrsock_addrinfo_t result)
{
	return 0;
}

/**
 * Answers the number of results returned from @ref omrsock_getaddrinfo.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref omrsock_getaddrinfo.
 * @param[out] length The number of results.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_getaddrinfo_length(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t hints, int32_t *length)
{
	return 0;
}


/**
 * Answers the family type of the address at "index" in the structure returned from @ref omrsock_getaddrinfo, indexed starting at 0.
 *
 * Currently the family types we support are OMRSOCK_AF_INET and OMRSOCK_AF_INET6.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref omrsock_getaddrinfo.
 * @param[out] family The family at "index".
 * @param[in] index The index into the structure returned by @ref omrsock_getaddrinfo.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_getaddrinfo_family(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t *handle, int32_t *family, int index)
{
	return 0;
}

/**
 * Answers the socket type of the address at "index" in the structure returned from @ref omrsock_getaddrinfo, indexed starting at 0.
 *
 * Currently the socket types we support are OMRSOCK_STREAM and OMRSOCK_DATAGRAM.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref omrsock_getaddrinfo.
 * @param[out] socktype The socket type at "index".
 * @param[in] index The index into the structure returned by @ref omrsock_getaddrinfo.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_getaddrinfo_socktype(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, int32_t *socktype, int index)
{
	return 0;
}

/**
 * Answers the protocol of the address at "index" in the structure returned from @ref omrsock_getaddrinfo, indexed starting at 0.
 *
 * Currently the protocols we support are OMRSOCK_IPPROTO_IP, OMRSOCK_IPPROTO_TCP, OMRSOCK_IPPROTO_IPV6, and OMRSOCK_IPPROTO_UDP. 
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref omrsock_getaddrinfo.
 * @param[out] protocol The protocol family at "index".
 * @param[in] index The index into the structure returned by @ref omrsock_getaddrinfo.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_getaddrinfo_protocol(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, int32_t *protocol, int index)
{
	return 0;
}

/**
 * Frees the memory created by the call to @ref omrsock_getaddrinfo().
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Hints on what results are returned for getaddrinfo.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 */

int32_t
omrsock_freeaddrinfo(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle)
{
	return 0;
}

/**
 * Creates a new socket descriptor and any related resources.
 *
 * @param[in] portLibrary The port library.
 * @param[out] sock Pointer to the omrsocket, to be allocated
 * @param[in] family The address family
 * @arg OMRSOCK_AF_INET, for a IPv4 socket
 * @arg OMRSOCK_AF_INET6, for a IPv6 socket
 * @param[in] socktype Specifies what type of socket is created
 * @arg OMRSOCK_STREAM, for a stream socket
 * @arg OMRSOCK_DGRAM, for a datagram socket
 * @param[in] protocol Type/family specific creation parameter
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_socket(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t family, int32_t socktype,  int32_t protocol)
{
	return 0;
}

/**
 * The bind function is used on an unconnected socket before subsequent 
 * calls to the connect or listen functions. When a socket is created with a 
 * call to the socket function, it exists in a name space (address family), but 
 * it has no name assigned to it. Use bind to establish the local association 
 * of the socket by assigning a local name to an unnamed socket.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock The socket that will be be associated with the specified name.
 * @param[in] addr Address to bind to socket.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_bind(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr)
{
	return 0;
}

/**
 * Set the socket to listen for incoming connection requests.  This call is made prior to accepting requests,
 * via the @ref omrsock_accept function.  The backlog specifies the maximum length of the queue of pending connections,
 * after which further requests are rejected.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket.
 * @param[in] backlog The maximum number of queued requests.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_listen(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t backlog)
{
	return 0;
}

/**
 * Establish a connection to a peer.
 *
 * For stream sockets, it first binds the socket if it hasn't already been done. Then, it
 * tries to set up a connection.
 * 
 * For datagram sockets, connect function will set up the peer information. No actual
 * connection is made. Subsequent calls to connect can be made to change destination address.
 *
 * It is not recommended that users call this
 * function multiple times to determine when the connection attempt has succeeded.
 * Instead, the omrsock_select() function can be used to determine when the socket 
 * is ready for reading or writing.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the unconnected local socket.
 * @param[in] addr Pointer to the sockaddr, specifying remote host/port.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_connect(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr)
{
	return 0;
}

/**
 * The accept function extracts the first connection on the queue of pending connections 
 * on socket serverSock. It then creates a new socket and returns a handle to the new socket. 
 * The newly created socket is the socket that will handle the actual the connection and 
 * has the same properties as socket serverSock.  
 *
 * The accept function can block the caller until a connection is present if no pending 
 * connections are present on the queue.
 *
 * @param[in] portLibrary The port library.
 * @param[in] serverSock  A omrsock_socket_t that tries to accept a connection.
 * @param[in] addrHandle An optional pointer to a buffer that receives the address of the connecting 
 * entity, as known to the communications layer. The exact format of the addr parameter 
 * is determined by the address family established when the socket was created
 * @param[out] sockHandle A pointer to a omrsock_socket_t which will point to the newly created 
 * socket once accept returns successfully
 *
* @return	0, if no errors occurred, otherwise the (negative) error code.
 */

int32_t
omrsock_accept(struct OMRPortLibrary *portLibrary, omrsock_socket_t serverSock, omrsock_sockaddr_t addrHandle, omrsock_socket_t *sockHandle)
{
	return 0;
}

/**
 * The send function sends data to a connected socket.  The successful completion of a send 
 * does not indicate that the data was successfully delivered.  If no buffer space is available 
 * within the transport system to hold the data to be transmitted, send will block.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to send on
 * @param[in] buf The bytes to be sent
 * @param[in] nbyte The number of bytes to send
 * @param[in] flags The flags to modify the send behavior
 *
 * @return	If no error occur, return the total number of bytes sent, which can be less than the 
 * 'nbyte' for nonblocking sockets, otherwise the (negative) error code
 */
int32_t
omrsock_send(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	return 0;
}

/**
 * The sendto function sends data to a datagram socket.  The successful completion of a writeto
 * does not indicate that the data was successfully delivered.  If no buffer space is available 
 * within the transport system to hold the data to be transmitted, sendto will block.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to send on
 * @param[in] buf The bytes to be sent
 * @param[in] nbyte The number of bytes to send
 * @param[in] flags The flags to modify the send behavior
 * @param [in] addrHandle The network address to send the datagram to
 *
 * @return	If no error occur, return the total number of bytes sent, otherwise the (negative) error code.
 */
int32_t
omrsock_sendto(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle)
{
	return 0;
}

/**
 * Receives data from a connected socket.  
 * 
 * This will return as much information as is currently available up to the size of the buffer supplied. 
 * 
 * We will implement in the future so that bbehaviour depending on the blocking characteristic of the socket.
 *
 *	Blocking socket:
 * 		- If no incoming data is available, the call blocks and waits for data to arrive. 
 * 
 * 	Non-blocking socket:
 * 		- Will be implemented in the future
 * 		- If no incoming data is immediately available, the negative portable error code OMRERROR_SOCKET_WOULDBLOCK is returned.
 * 
 * @param[in]	portLibrary The port library.
 * @param[in] 	sock Pointer to the socket to read on
 * @param[out] 	buf Pointer to the buffer where input bytes are written
 * @param[in] 	nbyte The length of buf
 * @param[in] 	flags The flags, to influence this read (in addition to the socket options)
 *
 * @return
 * @arg If no error occurs, return the number of bytes received.
 * @arg If the connection has been gracefully closed, return 0.
 * @arg Otherwise return the (negative) error code.
 */
int32_t
omrsock_recv(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	return 0;
}

/**
 * The recvfrom function receives data from a possibly connected socket.  Calling recvfrom will return as much 
 * information as is currently available up to the size of the buffer supplied.  If the information is too large
 * for the buffer, the excess will be discarded.  If no incoming  data is available at the socket, the recv call 
 * blocks and waits for data to arrive.  It the address argument is not null, the address will be updated with
 * address of the message sender.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to read on.
 * @param[out] buf Pointer to the buffer where input bytes are written.
 * @param[in] nbyte The length of buf.
 * @param[in] flags Tthe flags, to influence this read.
 * @param[out] addrHandle	if provided, the address to be updated with the sender information.
 *
 * @return
 * @arg If no error occurs, return the number of bytes received.
 * @arg If the connection has been gracefully closed, return 0.
 * @arg Otherwise return the (negative) error code.
 */
int32_t
omrsock_recvfrom(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle)
{
	return 0;
}

/**
 * The close function closes a socket. Use it to release the socket descriptor socket so 
 * further references to socket will fail.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock The socket that will be closed.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrsock_close(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock)
{
	return 0;
}

