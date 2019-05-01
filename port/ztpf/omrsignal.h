/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#ifndef _ZTPF_OMRSIGNAL_H_INCLUDED
#define _ZTPF_OMRSIGNAL_H_INCLUDED
#include <setjmp.h>
#include <tpf/i_jdib.h>

/*
 *	The following structure and user-defined type originally came from
 *	omrsignal.c; apparently custom-implemented for each different kind of
 *	platform. We have moved these here because these are custom for and	
 *	possibly unique to z/TPF at some later time.
 */

struct OMRSignalHandlerRecord {
	struct OMRSignalHandlerRecord *previous;
	struct OMRPortLibrary *portLibrary;
	omrsig_handler_fn handler;
	void *handler_arg;
	jmp_buf	mark;			
	uint32_t flags;
} OMRSignalHandlerRecord;

typedef struct OMRCurrentSignal {
	int	signal;
	siginfo_t *sigInfo;
	void *contextInfo;
	uintptr_t breakingEventAddr;
	uint32_t portLibSignalType;
	DIB	*ptrDIB;
} OMRCurrentSignal;

void masterSynchSignalHandler(int signal, siginfo_t * sigInfo, void *contextInfo, uintptr_t breakingEventAddr);

#endif
