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
