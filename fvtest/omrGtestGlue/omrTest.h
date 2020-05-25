/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#ifndef OMR_TEST_FRAMEWORK_H_
#define OMR_TEST_FRAMEWORK_H_

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
/* Gtest invokes some functions (e.g., getcwd() ) before main() is invoked.
 * We need to invoke iconv_init() before gtest to ensure a2e is initialized.
 */
extern int iconv_initialization(void);
static int iconv_init_static_variable = iconv_initialization();
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */


#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
/*  Gtest invokes xlocale, which has function definition for tolower and toupper.
 * This causes compilation issue since the a2e macros (tolower and toupper) automatically replace the function definitions.
 * So we explicitly include <ctype.h> and undefine the macros for gtest, after gtest we then define back the macros.
 */
#include <ctype.h>
#undef toupper
#undef tolower

#include "gtest/gtest.h"

#define toupper(c)     (islower(c) ? (c & _XUPPER_ASCII) : c)
#define tolower(c)     (isupper(c) ? (c | _XLOWER_ASCII) : c)

#else
#include "gtest/gtest.h"
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

using namespace testing;

class OMREventListener: public TestEventListener {

protected:
	TestEventListener* _eventListener;

public:
	bool _showTestCases;
	bool _showTests;
	bool _showSuccess;
	bool _showFailures;

	OMREventListener(TestEventListener* theEventListener) :
			_eventListener(theEventListener),
			_showTestCases(true),
			_showTests(true),
			_showSuccess(true),
			_showFailures(true)
	{
	}

	virtual ~OMREventListener() {
		delete _eventListener;
	}
	virtual void OnTestProgramStart(const UnitTest& unit_test) {
		_eventListener->OnTestProgramStart(unit_test);
	}
	virtual void OnTestIterationStart(const UnitTest& unit_test, int iteration) {
		_eventListener->OnTestIterationStart(unit_test, iteration);
	}
	virtual void OnEnvironmentsSetUpStart(const UnitTest& unit_test) {}
	virtual void OnEnvironmentsSetUpEnd(const UnitTest& unit_test) {}

	virtual void OnTestCaseStart(const TestCase& test_case) {
		if (_showTestCases) {
			_eventListener->OnTestCaseStart(test_case);
		}
	}
	virtual void OnTestStart(const TestInfo& test_info) {
		if (_showTests) {
			_eventListener->OnTestStart(test_info);
		}
	}
	virtual void OnTestPartResult(const TestPartResult& result) {
		_eventListener->OnTestPartResult(result);
	}
	virtual void OnTestEnd(const TestInfo& test_info) {
		if (test_info.result()->Failed()) {
			if (_showFailures) {
				_eventListener->OnTestEnd(test_info);
			}
		} else {
			if (_showSuccess) {
				_eventListener->OnTestEnd(test_info);
			}
		}
	}
	virtual void OnTestCaseEnd(const TestCase& test_case) {
		if (_showTestCases) {
			_eventListener->OnTestCaseEnd(test_case);
		}
	}
	virtual void OnEnvironmentsTearDownStart(const UnitTest& unit_test) {}
	virtual void OnEnvironmentsTearDownEnd(const UnitTest& unit_test) {}
	virtual void OnTestIterationEnd(const UnitTest& unit_test, int iteration) {
		_eventListener->OnTestIterationEnd(unit_test, iteration);
	}
	virtual void OnTestProgramEnd(const UnitTest& unit_test) {
		_eventListener->OnTestProgramEnd(unit_test);
	}

	static void setDefaultTestListener(bool showTestCases = true, bool showTests = false, bool showSuccess = false, bool showFailures = true) {
            if (!getenv("OMR_VERBOSE_TEST")) {
		TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
		TestEventListener* default_printer = listeners.Release(listeners.default_result_printer());
		OMREventListener *listener = new OMREventListener(default_printer);
		listener->_showTestCases = showTestCases;
		listener->_showTests = showTests;
		listener->_showSuccess = showSuccess;
		listener->_showFailures = showFailures;
		listeners.Append(listener);
            }
	}
};

#ifdef __cplusplus
extern "C" {
#endif

int omr_main_entry(int argc, char **argv, char **envp);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
