/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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

#ifndef THREADEXTENDEDTESTHELPERS_HPP_INCLUDED
#define THREADEXTENDEDTESTHELPERS_HPP_INCLUDED

#include <string.h>

#include "omrport.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "testEnvironment.hpp"

class ThreadExtendedTestEnvironment: public BaseEnvironment
{
/*
 * Data members
 */
private:
	OMRPortLibrary *_portLibrary;
protected:
public:
	uint32_t grindTime;

/*
* Function members
*/
private:
protected:
public:
	ThreadExtendedTestEnvironment(int argc, char **argv, OMRPortLibrary *portLibrary) :
		BaseEnvironment(argc, argv),
		grindTime(0)
	{
		for (int i = 1; i < argc; i++) {
			if (strncmp(argv[i], "-grind=", strlen("-grind=")) == 0) {
				sscanf(&argv[i][strlen("-grind=")], "%u", &grindTime);
			}
		}
		_portLibrary = portLibrary;
	}

	OMRPortLibrary *
	getPortLibrary()
	{
		return _portLibrary;
	}
};

/**
 * Initialize port library and attach thread
 */
void thrExtendedTestSetUp(OMRPortLibrary *portLibrary);

/**
 * Shutdown port library and detach thread
 */
void thrExtendedTestTearDown(OMRPortLibrary *portLibrary);

extern ThreadExtendedTestEnvironment *omrTestEnv;

#endif /* THREADEXTENDEDTESTHELPERS_HPP_INCLUDED */
