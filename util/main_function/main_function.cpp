/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "omrcfg.h"

/*
 * Perform platform-specific conversions on (argc, argv) before invoking the generic
 * test main function.
 *
 * This file needs to be included as source in the test executable's build. It can't
 * be built into a utility library because Windows does not link the wmain function
 * correctly when it is located in a library.
 */

#if defined(OMR_OS_WINDOWS)
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
#include "atoe.h"
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

/* Define a macro for the name of the main function that takes char args */
#if defined(OMR_OS_WINDOWS)
#define CHARMAIN translated_main
#else
#define CHARMAIN main
#endif

extern "C" int omr_main_entry(int argc, char **argv, char **envp);

int
CHARMAIN(int argc, char **argv, char **envp)
{
#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
	/* translate argv strings to ascii */
	iconv_init();
	{
		int i;
		for (i = 0; i < argc; i++) {
			argv[i] = e2a_string(argv[i]);
		}
	}
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

	return omr_main_entry(argc, argv, envp);
}

#if defined(OMR_OS_WINDOWS)
int
wmain(int argc, wchar_t **argv, wchar_t **envp)
{
	char **translated_argv = NULL;
	char **translated_envp = NULL;
	char *cursor = NULL;
	int i, length, envc;
	int rc;

	/* Translate argv to UTF-8 */
	length = argc; /* 1 null terminator per string */
	for (i = 0; i < argc; i++) {
		length += (int)(wcslen(argv[i]) * 3);
	}
	translated_argv = (char **)malloc(length + ((argc + 1) * sizeof(char *))); /* + array entries */
	cursor = (char *)&translated_argv[argc + 1];
	for (i = 0; i < argc; i++) {
		int utf8Length;

		translated_argv[i] = cursor;
		if (0 == (utf8Length = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, cursor, length, NULL, NULL))) {
			return -1;
		}
		cursor += utf8Length;
		*cursor++ = '\0';
		length -= utf8Length;
	}
	translated_argv[argc] = NULL; /* NULL terminated the new argv */

	/* Translate argv to UTF-8 */
	if (envp) {
		envc = 0;
		while (NULL != envp[envc]) {
			envc++;
		}
		length = envc; /* 1 null terminator per string */
		for (i = 0; i < envc; i++) {
			length += (int)(wcslen(envp[i]) * 3);
		}
		translated_envp = (char **)malloc(length + ((envc + 1) * sizeof(char *))); /* + array entries */
		cursor = (char *)&translated_envp[envc + 1];
		for (i = 0; i < envc; i++) {
			int utf8Length;
			translated_envp[i] = cursor;
			if (0 == (utf8Length = WideCharToMultiByte(CP_UTF8, 0, envp[i], -1, cursor, length, NULL, NULL))) {
				return -1;
			}
			cursor += utf8Length;
			*cursor++ = '\0';
			length -= utf8Length;
		}
		translated_envp[envc] = NULL; /* NULL terminated the new envp */
	}

	rc = CHARMAIN(argc, translated_argv, translated_envp);

	/* Free the translated strings */
	free(translated_argv);
	free(translated_envp);

	return rc;
}
#endif /* defined(OMR_OS_WINDOWS) */
