/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>

#if defined(J9ZOS390)
#if __CHARSET_LIB == 1
#include <_Nascii.h>
#else /* __CHARSET_LIB == 1 */
#include "atoe.h"
#endif /* __CHARSET_LIB == 1 */
#endif /* defined(J9ZOS390) && (__CHARSET_LIB != 1) */

#include "DATMerge.hpp"
#include "FileUtils.hpp"
#include "Port.hpp"

int
#if defined(OMR_OS_WINDOWS)
translated_main(int argc, char **argv, char **envp)
#else
main(int argc, char **argv, char **envp)
#endif /* defined(OMR_OS_WINDOWS) */
{
	RCType rc = RC_OK;
#if defined(J9ZOS390)
#if __CHARSET_LIB == 1
	/* enable auto conversion */
	__ae_autoconvert_state(_CVTSTATE_ON);
#else /* __CHARSET_LIB == 1 */
	int i = 0;
	if (-1 != iconv_init()) {
		/* translate argv strings to ascii */
		for (i = 0; i < argc; i++) {
			argv[i] = e2a_string(argv[i]);
			if (NULL == argv[i]) {
				FileUtils::printError("failed to convert argument #%d from EBCDIC to ASCII\n", i);
				rc = RC_FAILED;
				break;
			}
		}
	} else {
		FileUtils::printError("failed to initialize iconv\n");
		rc = RC_FAILED;
	}
#endif /* __CHARSET_LIB == 1 */
#endif /* defined(J9ZOS390) */
	if (RC_OK == rc) {
		rc = startTraceMerge(argc, argv);
	}
	return (RC_OK == rc) ? 0 : -1;
}

#if defined(OMR_OS_WINDOWS)
int
wmain(int argc, wchar_t **argv, wchar_t **envp)
{
	char **translated_argv = NULL;
	char **translated_envp = NULL;
	char *cursor;
	int i, length, envc;
	int rc = -1;

	/* Translate argv to UTF-8 */
	length = argc;	/* 1 null terminator per string */
	for (i = 0; i < argc; i++) {
		length += (int)(wcslen(argv[i]) * 3);
	}
	translated_argv = (char **)malloc(length + ((argc + 1) * sizeof(char *))); /* + array entries */
	cursor = (char *)&translated_argv[argc + 1];
	for (i = 0; i < argc; i++) {
		int utf8Length;

		translated_argv[i] = cursor;
		if (0 == (utf8Length = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, argv[i], -1, cursor, length, NULL, NULL))) {
			return -1;
		}
		cursor += utf8Length;
		*cursor++ = '\0';
		length -= utf8Length;
	}
	translated_argv[argc] = NULL;	/* NULL terminated the new argv */

	/* Translate argv to UTF-8 */
	if (envp) {
		envc = 0;
		while (NULL != envp[envc]) {
			envc++;
		}
		length = envc;	/* 1 null terminator per string */
		for (i = 0; i < envc; i++) {
			length += (int)(wcslen(envp[i]) * 3);
		}
		translated_envp = (char **)malloc(length + ((envc + 1) * sizeof(char *))); /* + array entries */
		cursor = (char *)&translated_envp[envc + 1];
		for (i = 0; i < envc; i++) {
			int utf8Length;
			translated_envp[i] = cursor;
			if (0 == (utf8Length = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, envp[i], -1, cursor, length, NULL, NULL))) {
				return -1;
			}
			cursor += utf8Length;
			*cursor++ = '\0';
			length -= utf8Length;
		}
		translated_envp[envc] = NULL;	/* NULL terminated the new envp */
	}

	rc = translated_main(argc, translated_argv, translated_envp);

	/* Free the translated strings */
	free(translated_argv);
	free(translated_envp);

	return rc;
}
#endif /* defined(OMR_OS_WINDOWS) */
