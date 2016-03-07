/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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


#if !defined(OMRSIGTESTHELPERS_H_INCLUDED)
#define OMRSIGTESTHELPERS_H_INCLUDED

#include "omrsig.h"
#include "omrTest.h"

#include "testEnvironment.hpp"

extern PortEnvironment *omrTestEnv;

void createThread(omrthread_t *newThread, uintptr_t suspend, omrthread_detachstate_t detachstate,
				  omrthread_entrypoint_t entryProc, void *entryArg);
intptr_t joinThread(omrthread_t threadToJoin);
bool handlerIsFunction(sighandler_t handler);
#if !defined(WIN32)
bool handlerIsFunction(const struct sigaction *act);
#endif /* !defined(WIN32) */

#endif /* OMRSIGTESTHELPERS_H_INCLUDED */
