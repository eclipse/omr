/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
