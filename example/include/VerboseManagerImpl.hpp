/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(VERBOSEMANAGEREXAMPLE_HPP_)
#define VERBOSEMANAGEREXAMPLE_HPP_

#include "mmhook_common.h"

#include "VerboseManager.hpp"
#include "VerboseWriter.hpp"

class MM_EnvironmentBase;
class MM_VerboseHandlerOutputStandardRuby;

class MM_VerboseManagerImpl : public MM_VerboseManager
{
	/*
	 * Data members
	 */
private:

protected:
	char *filename;
	uintptr_t fileCount;
	uintptr_t iterations;

public:

	/*
	 * Function members
	 */
private:

protected:
	virtual void tearDown(MM_EnvironmentBase *env);

public:

	/* Interface for Dynamic Configuration */
	virtual bool configureVerboseGC(OMR_VM *vm, char* filename, uintptr_t fileCount, uintptr_t iterations);

	virtual bool reconfigureVerboseGC(OMR_VM *vm);

	virtual MM_VerboseHandlerOutput *createVerboseHandlerOutputObject(MM_EnvironmentBase *env);

	static MM_VerboseManagerImpl *newInstance(MM_EnvironmentBase *env, OMR_VM* vm);

	MM_VerboseManagerImpl(OMR_VM *omrVM)
		: MM_VerboseManager(omrVM)
		,filename(NULL)
		,fileCount(1)
		,iterations(0)
	{
	}
};

#endif /* VERBOSEMANAGEREXAMPLE_HPP_ */
