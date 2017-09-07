/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#include <stdio.h>

#include "omr.h"
#include "omrutil.h"

#include "omrTest.h"

int
main(int argc, char **argv, char **envp)
{
	::testing::InitGoogleTest(&argc, argv);

	return RUN_ALL_TESTS();
}

TEST(UtilTest, detectVMDirectory)
{
#if defined(WIN32)
	void *token = NULL;
	ASSERT_EQ(OMR_ERROR_NONE, OMR_Glue_GetVMDirectoryToken(&token));

	wchar_t path[2048];
	size_t pathMax = 2048;
	wchar_t *pathEnd = NULL;
	ASSERT_EQ(OMR_ERROR_NONE, detectVMDirectory(path, pathMax, &pathEnd));
	ASSERT_TRUE((NULL == pathEnd) || (L'\0' == *pathEnd));
	wprintf(L"VM Directory: '%s'\n", path);

	if (NULL != pathEnd) {
		size_t length = pathMax - wcslen(path) - 2;
		_snwprintf(pathEnd, length, L"\\abc");
		wprintf(L"Mangled VM Directory: '%s'\n", path);
	}
#endif /* defined(WIN32) */
}
