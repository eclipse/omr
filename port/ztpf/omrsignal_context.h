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
#ifndef _OMR_SIGNAL_CONTEXT_H_INCLUDED
#define _OMR_SIGNAL_CONTEXT_H_INCLUDED

#include "omrport.h"
#include <errno.h>
#include "omrosdump_helpers.h"

#include <unistd.h>
#include <sys/ucontext.h>
#include "sys/sigcontext.h"

#define MAX_UNIX_SIGNAL_TYPES  MNSIG

#define __USE_GNU 1
#include <dlfcn.h>
#undef __USE_GNU

typedef struct OMRPlatformSignalInfo {
	ucontext_t *context;
	uintptr_t breakingEventAddr;
	Dl_info	dl_info;
} OMRPlatformSignalInfo;

typedef struct OMRUnixSignalInfo {
	struct OMRPlatformSignalInfo	platformSignalInfo;
	uint32_t portLibrarySignalType;
	void* handlerAddress;
	void* handlerAddress2;
	siginfo_t *sigInfo;
} OMRUnixSignalInfo;


/*
 * The following structure is the element represented in our table
 * of PIC-to-si_signo/si_code values.
 */
typedef struct {
	int	si_signo;
	int	si_code;
} sigvpair_t;

/*
 *	By "impossible", what is meant is that either (a) the exception pertains
 *	to vmm modes that z/TPF doesn't support (any except primary-space), (b)
 *	the exception pertains to an unused floating point format, such as HFP,
 *	(c) some other circumstance which is not possible in TPF user-space,
 *	or (d) an unassigned PIC value. The signal number was chosen to be 
 *	SIGABND just because it's so unusual, and the SI_KERNEL code means that
 *	this signal was emitted by the (impostor) kernel. That's us!!
 */
#define IMPOSSIBLE { SIGABND, SI_KERNEL }

/* 
 * We use only the two defined ILC bits, which are already set in the 
 * correct positions to indicate their possible domain of values, 
 * the numbers 2, 4, or 6. We ensure this through masking out all other
 * bits, just in case. The 2-byte ILC for program interrupts resides at
 * address 00000008C. The 2-byte PIC follows at address 0000008E.
 */
/**
 * This 'macro' picks up the ILC from its location in an array
 * of characters meant to represent offset zero in low memory
 * and normalizes it to only its meaningful bits.
 */
static const inline uint16_t s390FetchILC(uint8_t* b) { uint16_t* q=(uint16_t*)(b+0x8c);
												return (*q & 0x06); }
/**
 * This 'macro' picks up the PIC from its location in an array
 * of characters meant to represent offset zero in low memory,
 * and normalizes it to only its meaningful bits. We don't care
 * if the interrupt was concurrent with a PER event or a trans-
 * action context.
 */
static const inline uint16_t s390FetchPIC(uint8_t* b) { uint16_t* q=(uint16_t*)(b+0x8e);
												 return (*q & 0x13f); }

/**
 * This 'macro' picks up the DXC from its location in an array
 * of characters meant to represent offset zero in low memory.
 */
static const inline uint8_t  s390FetchDXC(uint8_t* b) { return *(b+0x93); }

/*
 * Decls for externally-exported entry points
 */
#ifndef J9SIGNAL_CONTEXT
void translateInterruptContexts( args *argv ) __attribute__((nonnull));
void ztpfDeriveSiginfo( siginfo_t *build ) __attribute__((nonnull));
#endif
uint32_t infoForFPR(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo* info, int32_t index, const char **name, void **value)
 __attribute__((nonnull(1,2,4,5)));

uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo* info, int32_t index, const char **name, void **value) __attribute__((nonnull(1,2,4,5)));

uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo* info, int32_t index, const char **name, void **value) __attribute__((nonnull(1,2,4,5)));

uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo* info, int32_t index, const char **name, void **value) __attribute__((nonnull(1,2,4,5)));

uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo* info, int32_t index, const char **name, void **value) __attribute__((nonnull(1,2,4,5)));

void fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct OMRUnixSignalInfo *signalInfo)  __attribute__((nonnull));
#endif
