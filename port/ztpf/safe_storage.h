/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *   Multiple authors (IBM Corp.) - z/TPF platform initial port to OMR environment
 *******************************************************************************/

#ifndef _SAFE_STORAGE_H_INCLUDED
#define _SAFE_STORAGE_H_INCLUDED

#include <tpf/c_proc.h>
#include <sys/sigcontext.h>
#include "omrosdump_helpers.h"

/*
 * The only fields in safeStorage.argv that require setting by
 * the caller are dibPtr and portLibrary. argv.rc & argv.flags
 * are pre-cleared to zero. Function allocateDurableStorage()
 * has initialized all the pointers to point back into the
 * same static storage block. By passing &(safeStorage.argv)
 * to our thread-based functions (such as ztpfBuildCoreFile()
 * and translateInterruptContexts(), you've fulfilled the
 * very purpose of this block.
 *
 * All fields in "safeStorage" are cleared; callers are
 * responsible for completing them, if desired.
 */
typedef struct _durable_storage {
	U_8	workbuffer[PATH_MAX];
	U_8	filename[PATH_MAX];
	siginfo_t	siginfo;
	ucontext_t ucontext;
	struct sigcontext sigcontext;
	DIB	*pDIB;
	struct iproc *pPROC;
	args argv;
} safeStorage;

extern safeStorage *allocateDurableStorage( void );

#endif
