/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 *******************************************************************************/

#ifndef omrsignal_h
#define omrsignal_h

#include "omrcomp.h"

typedef struct J9Win32SignalInfo {
	uint32_t systemType;
	uint32_t portLibType;
	void *handlerAddress;
	void *handlerAddress2;
	struct _EXCEPTION_RECORD *ExceptionRecord;
	struct _CONTEXT *ContextRecord;
	void *moduleBaseAddress;
	uintptr_t offsetInDLL;
	char moduleName[_MAX_PATH];
	BOOLEAN deferToTryExcept;
	BOOLEAN tryExceptHandlerIgnore;
} J9Win32SignalInfo;

#endif     /* omrsignal_h */


