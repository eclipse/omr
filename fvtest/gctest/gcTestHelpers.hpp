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

#if !defined(GCTESTHELPERS_HPP_INCLUDED)
#define GCTESTHELPERS_HPP_INCLUDED

#include "omrTest.h"
#include "omrport.h"

/* OMR Imports */
#include "omr.h"
#include "omrExampleVM.hpp"
#include "omrgcstartup.hpp"
#include "omrvm.h"
#include "StartupManagerImpl.hpp"
#include "testEnvironment.hpp"
#include <vector>

class GCTestEnvironment: public BaseEnvironment
{
	/*
	 * Data members
	 */
public:
	OMR_VM_Example exampleVM;
	OMRPortLibrary *portLib;
	std::vector<const char *> params;

	/*
	 * Function members
	 */
private:
	void initParams();
	void clearParams();

public:
	/*
	 * Initialization/Finalization for gctest can be performed only once per process, even with --gtest_repeat on.
	 */
	void GCTestSetUp();
	void GCTestTearDown();

public:
	GCTestEnvironment(int argc, char **argv)
	: BaseEnvironment(argc, argv), portLib(NULL)
	{
	}
};

/**
 * To help detect memory leaks, print out the amount of physical memory and virtual memory consumed by the test process.
 *
 * @param[in] The caller place
 * @param[in] portLib The port library
 */
void printMemUsed(const char *where, OMRPortLibrary *portLib);

extern GCTestEnvironment *gcTestEnv;

#endif /* GCTESTHELPERS_HPP_INCLUDED */
