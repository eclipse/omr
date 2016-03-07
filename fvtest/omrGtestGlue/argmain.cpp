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


/*
 * Perform platform-specific conversions on (argc, argv) before invoking the generic
 * test main function.
 *
 * This file needs to be included as source in the test executable's build. It can't
 * be built into a utility library because Windows does not link the wmain function
 * correctly when it is located in a library.
 */

#if defined(WIN32)
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>

#if defined(J9ZOS390)
#include "atoe.h"
#endif

/* Define a macro for the name of the main function that takes char args */
#if defined(WIN32)
#define CHARMAIN translated_main
#else
#define CHARMAIN main
#endif

extern "C" int testMain(int argc, char **argv, char **envp);

int
CHARMAIN(int argc, char **argv, char **envp)
{
#if defined(J9ZOS390)
	/* translate argv strings to ascii */
	iconv_init();
	{
		int i;
		for (i = 0; i < argc; i++) {
			argv[i] = e2a_string(argv[i]);
		}
	}
#endif /* defined(J9ZOS390) */

	return testMain(argc, argv, envp);
}

#if defined(WIN32)
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
#endif /* defined(WIN32) */
