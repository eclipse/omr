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

#include <stdlib.h>
#include <string.h>

#include "ArgParser.hpp"
#include "FileUtils.hpp"
#include "StringUtils.hpp"

RCType
ArgParser::parseOptions(int argc, char *argv[], J9TDFOptions *options)
{
	RCType rc = RC_OK;

	for (int i = 1; i < argc; i++) {
		if (StringUtils::startsWithUpperLower(argv[i], "-root")) {
			i++;
			if (i >= argc) {
				FileUtils::printError("Root directory not specified\n");
				rc = RC_FAILED;
				goto done;
			} else {
				/* Construct linked list of root directories. */
				char *roots = strdup(argv[i]);
				char *dirToken = NULL;
				if (NULL == roots) {
					rc = RC_FAILED;
					goto done;
				}
				dirToken = strtok(roots, ",");
				Path **current = &(options->rootDirectory);

				while (NULL != dirToken) {
					*current = (Path *)Port::omrmem_calloc(1, sizeof(Path));
					if (NULL == *current) {
						rc = RC_FAILED;
						goto done;
					}
					(*current)->path = strdup(dirToken);
					(*current)->next = NULL;
					current = &(*current)->next;

					dirToken = strtok(NULL, ",");
				}
				Port::omrmem_free((void **)&roots);
			}
		} else if (StringUtils::startsWithUpperLower(argv[i], "-threshold")) {
			i++;
			if (i >= argc) {
				FileUtils::printError("TraceGen: threshold specified with no number aborting\n");
				rc = RC_FAILED;
				goto done;
			} else {
				options->threshold = atoi(argv[i]);
			}
		} else if (StringUtils::startsWithUpperLower(argv[i], "-majorversion")) {
			i++;
			if (i >= argc) {
				FileUtils::printError("TraceGen: majorversion specified with no number aborting\n");
				rc = RC_FAILED;
				goto done;
			} else {
				options->rasMajorVersion = atoi(argv[i]);
			}
		} else if (StringUtils::startsWithUpperLower(argv[i], "-minorversion")) {
			i++;
			if (i >= argc) {
				FileUtils::printError("TraceGen: minorversion specified with no number aborting\n");
				rc = RC_FAILED;
				goto done;
			} else {
				options->rasMinorVersion = atoi(argv[i]);
			}
		} else if (StringUtils::startsWithUpperLower(argv[i], "-debug")) {
			options->debugOutput = true;
		} else if (StringUtils::startsWithUpperLower(argv[i], "-w2cd"))	{
			options->writeToCurrentDir = true;
		} else if (StringUtils::startsWithUpperLower(argv[i], "-generateCfiles")) {
			options->generateCFiles = true;
		} else if (StringUtils::startsWithUpperLower(argv[i], "-treatWarningAsError")) {
			options->treatWarningAsError = true;
		} else if (StringUtils::startsWithUpperLower(argv[i], "-force")) {
			options->force = true;
		} else {
			FileUtils::printError("Unknown option: %s\n", argv[i]);
			rc = RC_FAILED;
			goto done;
		}
	}

	if (NULL == options->rootDirectory) {
		options->rootDirectory = (Path *)Port::omrmem_calloc(1, sizeof(Path));
		if (NULL == options->rootDirectory) {
			rc = RC_FAILED;
			goto done;
		}
		options->rootDirectory->path = strdup(".");
		options->rootDirectory->next = NULL;
	}

done:
	return rc;
}

RCType
ArgParser::freeOptions(J9TDFOptions *options)
{
	Path *dir = options->rootDirectory;
	Path *tmpDir = NULL;
	while (NULL != dir) {
		tmpDir = dir->next;
		Port::omrmem_free((void **)&(dir->path));
		Port::omrmem_free((void **)&dir);
		dir = tmpDir;
	}
	return RC_OK;
}
