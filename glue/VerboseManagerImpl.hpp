/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
