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

#if !defined(TESTHELPER_HPP_INCLUDED)
#define TESTHELPER_HPP_INCLUDED

#include "omrTest.h"
#include "omrTestHelpers.h"
#include "testEnvironment.hpp"

class ThreadTestEnvironment: public PortEnvironment
{
/*
 * Data members
 */
private:
protected:
public:
	bool realtime;

/*
* Function members
*/
private:
protected:
public:
	ThreadTestEnvironment(int argc, char **argv)
		: PortEnvironment(argc, argv)
		, realtime(false)
	{
		for (int i = 1; i < argc; i++) {
			if (0 == strcmp(argv[i], "-realtime")) {
				realtime = true;
				break;
			}
		}
	}
};

extern ThreadTestEnvironment *omrTestEnv;

#endif /* TESTHELPER_HPP_INCLUDED */
