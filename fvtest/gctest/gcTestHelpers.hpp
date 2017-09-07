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
	std::vector<const char *> params;
	bool keepLog;

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
	: BaseEnvironment(argc, argv), keepLog(false)
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
