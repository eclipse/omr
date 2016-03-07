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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef leconditionhandler_h
#define leconditionhandler_h

#include <leawi.h>
#include <setjmp.h>
#include <ceeedcct.h>
#include "edcwccwi.h"
#include "omrport.h"

typedef struct J9ZOSLEConditionHandlerRecord {
	struct OMRPortLibrary *portLibrary;
	omrsig_handler_fn handler;
	void *handler_arg;
	sigjmp_buf returnBuf;
	struct __jumpinfo farJumpInfo;
	uint32_t flags;
	uint32_t recursiveCheck; /* if this is set to 1, the handler corresponding to this record has been invoked recursively */
} J9ZOSLEConditionHandlerRecord;

#endif /* leconditionhandler_h */
