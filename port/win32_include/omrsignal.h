/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef omrsignal_h
#define omrsignal_h

#include "omrcomp.h"

typedef struct J9Win32SignalInfo {
	uint32_t type;
	void *handlerAddress;
	void *handlerAddress2;
	struct _EXCEPTION_RECORD *ExceptionRecord;
	struct _CONTEXT *ContextRecord;
	void *moduleBaseAddress;
	uint32_t offsetInDLL;
	uint32_t threadId;
	char moduleName[_MAX_PATH];
} J9Win32SignalInfo;

#endif     /* omrsignal_h */


