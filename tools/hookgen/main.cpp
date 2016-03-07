/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015
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

#include <stdlib.h>
#include <string.h>

#if defined(J9ZOS390)
#include "atoe.h"
#endif /* defined(J9ZOS390) */
#if defined(WIN32)
#include <windows.h>
#endif /* WIN32 */

#include "HookGen.hpp"

/* On all platforms operate on UTF-8 encoding */
int
#if defined(WIN32)
translated_main(int argc, char **argv, char **envp)
#else /* defined(WIN32) */
main(int argc, char **argv, char **envp)
#endif /* defined(WIN32) */
{
	RCType rc = RC_OK;
#if defined(J9ZOS390)
	/* Convert EBCDIC to UTF-8 (ASCII) */
	if (-1 != iconv_init()) {
		/* translate argv strings to ascii */
		for (int i = 0; i < argc; i++) {
			argv[i] = e2a_string(argv[i]);
			if (NULL == argv[i]) {
				fprintf(stderr, "failed to convert argument #%d from EBCDIC to ASCII\n", i);
				rc = RC_FAILED;
				break;
			}
		}
	} else {
		fprintf(stderr, "failed to initialize iconv\n");
		rc = RC_FAILED;
	}
#endif /* defined(J9ZOS390) */
	if (RC_OK == rc) {
		rc = startHookGen(argc, argv);
	}
	return (RC_OK == rc) ? 0 : -1;
}

/* Convert Windows wide character encoding to UTF-8 */
#if defined(WIN32)
int
wmain(int argc, wchar_t **argv, wchar_t **envp)
{
	char **translated_argv = NULL;
	char **translated_envp = NULL;
	char *cursor = NULL;
	int i = 0;
	int length = argc;	/* 1 null terminator per string */
	int envc = 0;
	int rc = -1;

	/* Translate argv to UTF-8 */
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
	translated_argv[argc] = NULL;	/* NULL terminated the new argv */

	/* Translate argv to UTF-8 */
	if (NULL != envp) {
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
			if (0 == (utf8Length = WideCharToMultiByte(CP_UTF8, 0, envp[i], -1, cursor, length, NULL, NULL))) {
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
	free((void **)translated_argv);
	free((void **)translated_envp);

	return rc;
}
#endif /* defined(WIN32) */
