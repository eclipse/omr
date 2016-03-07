/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(WIN32)
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#endif /* !defined(WIN32) */

#include "DATMerge.hpp"
#include "ArgParser.hpp"
#include "FileReader.hpp"
#include "FileUtils.hpp"
#include "StringUtils.hpp"
#include "Port.hpp"

#define TRACE_FILE_FRAGMENT_EXT "pdat"

static RCType help(int argc, char *argv[]);

/**
 * Print help message if request for help is found in command line arguments
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return RC_OK if help option was encountered, RC_FAILED if not.
 */
static RCType
help(int argc, char *argv[])
{
	for (int i = 0; i < argc; i++) {
		if (0 == strcmp(argv[i], "-help")
			|| 0 == strcmp(argv[i], "--help")
			|| 0 == strcmp(argv[i], "-h")
			|| 0 == strcmp(argv[i], "--h")
			|| 0 == strcmp(argv[i], "/h")
			|| 0 == strcmp(argv[i], "/help")
		) {
			printf("%s -majorversion num -minorversion num\n", argv[0]);
			printf("\t-majorversion (default 5)\n");
			printf("\t-minorversion (default 1)\n");
			printf("\t-root Comma-separated directories to start scanning for pdat files (default .)\n");
			return RC_OK;
		}
	}
	return RC_FAILED;
}

RCType
startTraceMerge(int argc, char *argv[])
{
	RCType rc = RC_OK;
	Path *dir = NULL;
	J9TDFOptions options;
	ArgParser argParser;

	if (RC_OK == help(argc, argv)) {
		return RC_OK;
	}

	rc = argParser.parseOptions(argc, argv, &options);
	if (RC_OK != rc) {
		goto failed;
	}

	DATMerge datMerge;

	dir = options.rootDirectory;
	while (NULL != dir) {
		if (0 != FileUtils::visitDirectory(&options, dir->path, TRACE_FILE_FRAGMENT_EXT, &datMerge, DATMerge::mergeCallback))	{
			FileUtils::printError("Failed to merge PDAT files\n");
			goto failed;
		}
		dir = dir->next;
	}

	argParser.freeOptions(&options);
	return rc;
failed:
	argParser.freeOptions(&options);
	return RC_FAILED;
}


RCType
DATMerge::merge(J9TDFOptions *options, const char *fromFileName)
{
	RCType rc = RC_FAILED;
	const char *component = NULL;
	const char *line = NULL;
	const char *toFileName = NULL;

	FILE *toFile;

	FileReader fromFilereader;
	FileReader toFilereader;
	if (RC_OK != fromFilereader.init(fromFileName)) {
		FileUtils::printError("Failed to open file %s\n", fromFileName);
		goto failed;
	}

	while (fromFilereader.hasNext()) {
		line = fromFilereader.next();
		if (0 == strlen(line)) {
			/* skip */
		} else {
			if (StringUtils::startsWithUpperLower(line, "datfilename=")) {
				toFileName = strdup(strrchr(line, '=') + 1);
			} else if (StringUtils::startsWithUpperLower(line, "executable=")) {
				component = strdup(strrchr(line, '=') + 1);
			}

			if (NULL != toFileName && NULL != component) {
				break;
			}
		}
	}

	if (NULL == toFileName) {
		goto failed;
	}

	toFile = Port::fopen(toFileName, "rb");

	if (toFile != NULL) {
		if (0 != fclose(toFile)) {
			perror("fclose error");
		}
		char *newToFileName = (char *)Port::omrmem_calloc(1, strlen(toFileName) + strlen(".new") + 1);
		sprintf(newToFileName, "%s.new", toFileName);

		char *buffer = (char *)Port::omrmem_calloc(1, 1024);

		if (NULL == newToFileName || NULL == buffer) {
			goto failed;
		}

		FILE *tmpFile = Port::fopen(newToFileName, "wb");

		/* Copy across the lines for every other component. */
		rc = toFilereader.init(toFileName);
		if (RC_OK != rc) {
			FileUtils::printError("Failed to open file %s\n", toFileName);
			goto failed;
		}

		const char *next = NULL;
		if (!toFilereader.hasNext()) {
			FileUtils::printError("header is missing\n");
			goto failed;
		}
		next = toFilereader.next();
		fprintf(tmpFile, "%s\n", next);
		Port::omrmem_free((void **)&next);

		while (toFilereader.hasNext()) {
			next = toFilereader.next();
			if (strlen(next) > 0) {
				const char *separator = strchr(next, '.');
				size_t len = (separator - next);

				if (strlen(component) > len) {
					len = strlen(component);
				}

				if (0 != strncmp(component, next, len)) {
					/* Important - the buffer will contain formatting strings like %s, %p etc
					 * if we just do fprintf(newDatFile, buffer) they will be expanded and
					 *  fprintf will use garbage off the stack to do it. Luckily escaping the
					 *  whole thing is trivial - just format a string.
					 */
					fprintf(tmpFile, "%s\r\n", next);
				}
			}
			Port::omrmem_free((void **)&next);
		}
		if (0 != fclose(tmpFile)) {
			perror("fclose error");
		}

		/* Remove the old DAT file. */
		if (0 != remove(toFileName)) {
			perror("failed to delete target file");
			goto failed;
		}

		/* Rename the new one and re-open for appending. */
		if (0 != Port::omrfile_move(newToFileName, toFileName)) {
			perror("Failed to rename temp file");
			goto failed;
		}

		Port::omrmem_free((void **)&newToFileName);
		Port::omrmem_free((void **)&buffer);

		toFile = Port::fopen(toFileName, "ab");
		if (NULL == toFile) {
			perror("fopen error");
			goto failed;
		}
	} else {
		/* Open a new file and write the header. */
		toFile = Port::fopen(toFileName, "wb");
		if (NULL == toFile) {
			perror("fopen error");
			goto failed;
		}
		printf("tracemerge creating dat file: %s\n", toFileName);

		fprintf(toFile, "%u.%u\n", options->rasMajorVersion, options->rasMinorVersion);
	}


	printf("Adding %s to dat file: %s\n", fromFileName, toFileName);

	while (fromFilereader.hasNext()) {
		const char *line = fromFilereader.next();
		/* Skip newlines */
		if (strlen(line) > 0) {
			fprintf(toFile, "%s\r\n", line);
		}
		Port::omrmem_free((void **)&line);
	}

	if (RC_OK != fclose(toFile)) {
		perror("fclose error");
		goto failed;
	}


	return RC_OK;

failed:
	return RC_FAILED;
}

RCType
DATMerge::mergeCallback(void *targetObject, J9TDFOptions *options, const char *fromFileName)
{
	DATMerge *self = (DATMerge *) targetObject;
	return self->merge(options, fromFileName);
}

