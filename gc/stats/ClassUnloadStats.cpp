/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "ClassUnloadStats.hpp"

void
MM_ClassUnloadStats::clear()
{
	_classLoaderUnloadedCount = 0;
	_classLoaderCandidates = 0;
	_classesUnloadedCount = 0;
	_anonymousClassesUnloadedCount = 0;
	_startTime = 0;
	_endTime = 0;
	_startSetupTime = 0;
	_endSetupTime = 0;
	_startScanTime = 0;
	_endScanTime = 0;
	_startPostTime = 0;
	_endPostTime = 0;
	_classUnloadMutexQuiesceTime = 0;
};
