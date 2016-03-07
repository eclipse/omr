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


#ifndef TESTENVIRONMENT_HPP_
#define TESTENVIRONMENT_HPP_

/**
 * Google test's Environment class provides virtual SetUp() and TearDown() that are invoked before the
 * first test case is run and after the last test case completes. This complements Google test's Test
 * class, which provides virtual SetUp() and TearDown() that are invoked before and after each test case.
 *
 * The BaseEnvironment and PortEnvironment classes can be used directly or subclassed and
 * specialized as required. Other generic Environment subclasses, eg GCEnvironment, might belong here.
 *
 * Some Environment subclasses may require that SetUp() and TearDown() must be called on the same
 * thread. This should not be a problem, since the Google test framework runs each test suite on a
 * single thread, ie, the same test thread executes:
 *
 * 		Environment::SetUp() (Test::SetUp() TEST Test::TearDown())* Environment::TearDown()
 *
 */

#include <stdio.h>
#include "omrport.h"
#include "omrthread.h"
#include "omrTest.h"

/**
 * BaseEnvironment provides access to command line arguments for all tests run from main().
 */
class BaseEnvironment: public ::testing::Environment
{
	/*
	 * Data members
	 */
private:
protected:
public:
	int _argc;
	char **_argv;

	/*
	 * Function members
	 */
private:
protected:
	virtual void SetUp() { }
	virtual void TearDown() { }

public:
	BaseEnvironment(int argc, char **argv)
		: ::testing::Environment()
		, _argc(argc)
		, _argv(argv)
	{
	}
};

/*
 * Thread library initialization can be performed only once per process, so it cannot be placed
 * in an Environment subclass without precluding use of the --gtest_repeat option, which is
 * sometimes useful for debugging tests that fail intermittently and unpredictably. These macros
 * can be used before and after RUN_ALL_TESTS() to attach and detach the main test thread.
 *
 * Note that Googletest's ASSERT_*() macros cannot be used in this context. Those macros can
 * only be used in methods that return void.
 */
#define ATTACH_J9THREAD() \
	do { \
		intptr_t rc = omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT); \
		if (0 != rc) { \
			fprintf(stderr, "omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT) failed, rc=%d\n", (int)rc); \
		} \
	} while (0)

#define DETACH_J9THREAD() omrthread_detach(NULL)


/**
 * PortEnvironment provides BaseEnvironment and initializes/finalizes the j9port
 * library for all tests run from main(). As a prerequisite, the omrthread library must be
 * initialized and the main test thread attached before instantiating this class and calling
 * RUN_ALL_TESTS() and the main test thread must be detached and the omrthread library shut
 * down after RUN_ALL_TESTS(). The ATTACH_J9THREAD() and DETACH_J9THREAD() macros can be used
 * for this purpose.
 */
class PortEnvironment: public BaseEnvironment
{
	/*
	 * Data members
	 */
private:
	OMRPortLibrary _portLibrary;
protected:
public:

	/*
	 * Function members
	 */
private:
protected:
	virtual void
	SetUp()
	{
		BaseEnvironment::SetUp();

		/* Use portlibrary version which we compiled against, and have allocated space
		 * for on the stack.  This version may be different from the one in the linked DLL.
		 */
		int rc = (int)omrport_init_library(&_portLibrary, sizeof(OMRPortLibrary));
		ASSERT_EQ(0, rc) << "omrport_init_library(&_OMRPortLibrary, sizeof(OMRPortLibrary)) failed, %d\n" << rc;
	}

	virtual void
	TearDown()
	{
		_portLibrary.port_shutdown_library(&_portLibrary);

		BaseEnvironment::TearDown();
	}
public:
	PortEnvironment(int argc, char **argv)
		: BaseEnvironment(argc, argv)
	{
	}

	OMRPortLibrary *
	getPortLibrary()
	{
		return &_portLibrary;
	}
};

#endif /* TESTENVIRONMENT_HPP_ */
