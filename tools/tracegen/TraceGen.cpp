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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(WIN32)
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#endif /* !defined(WIN32) */

#include "ArgParser.hpp"
#include "CFileWriter.hpp"
#include "DATFileWriter.hpp"
#include "EventTypes.hpp"
#include "FileUtils.hpp"
#include "StringUtils.hpp"
#include "TraceHeaderWriter.hpp"
#include "TraceGen.hpp"

#define TDF_FILE_EXT "tdf"

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
			printf("%s [-threshold num] [-w2cd] [-generateCfiles] [-treatWarningAsError] [-root rootDir] [-force]\n", argv[0]);
			printf("\t-threshold Ignore trace level below this threshold (default 1)\n");
			printf("\t-w2cd Write generated .C and .H files to current directory (default: generate in the same directory as the TDF file)\n");
			printf("\t-generateCfiles Generate C files (default false)\n");
			printf("\t-treatWarningAsError Abort parsing at the first TDF error encountered (default false)\n");
			printf("\t-root Comma-separated directories to start scanning for TDF files (default .)\n");
			printf("\t-force Do not check TDF file timestamp, always generate output files. (default false)\n");
			return RC_OK;
		}
	}
	return RC_FAILED;
}

void
TraceGen::tearDown()
{
	/* Clear processed files cache */
	Path *tmp = NULL;
	while (NULL != _visitedFile) {
		tmp = _visitedFile->next;
		Port::omrmem_free((void **)&_visitedFile);
		_visitedFile = tmp;
	}
}
RCType
startTraceGen(int argc, char *argv[])
{
	RCType rc = RC_OK;
	Path *dir = NULL;

	if (RC_OK == help(argc, argv)) {
		return RC_OK;
	}

	J9TDFOptions options;
	TraceGen tracegen;
	ArgParser argParser;

	rc = argParser.parseOptions(argc, argv, &options);
	if (RC_OK != rc) {
		FileUtils::printError("Failed to parse command line options\n");
		goto failed;
	}

	if (options.force) {
		printf("tracegen -force option is enabled. All files will be regenerated.\n");
	}

	dir = options.rootDirectory;
	while (NULL != dir) {
		/* Recursively visit all directories under dirName, processing TDF files */
		if (RC_OK != FileUtils::visitDirectory(&options, dir->path, TDF_FILE_EXT, &tracegen, TraceGen::generateCallBack)) {
			FileUtils::printError("Failed to generate trace files\n");
			goto failed;
		}
		dir = dir->next;
	}
	tracegen.tearDown();
	argParser.freeOptions(&options);
	return rc;
failed:
	tracegen.tearDown();
	argParser.freeOptions(&options);
	return RC_FAILED;
}

RCType
TraceGen::generate(J9TDFOptions *options, const char *currentTDFFile)
{
	RCType rc = RC_FAILED;
	unsigned int groupCount = 0;
	J9TDFGroup *groups = NULL;
	J9TDFFile *tdf = NULL;

	FileReader reader;
	TDFParser parser;

	Path *current = _visitedFile;
	char *realPath = NULL;

	/**
	 * User might specify multiple scan roots for example -root src,src/omr
	 * To avoid processing trace files multiple times keep the record of processed files.
	 * To handle relative paths omrfile_realpath is used to resolve canonicalized absolute pathname.
	 */
	if (NULL == (realPath = Port::omrfile_realpath(currentTDFFile))) {
		FileUtils::printError("Failed to resolve full path to file: %s\n", currentTDFFile);
		goto done;
	}
	while (NULL != current) {
		if (0 == strcmp(current->path, realPath)) {
			break;
		}
		current = current->next;
	}
	if (NULL == current) {
		Path *tmp = (Path *) Port::omrmem_calloc(1, sizeof(Path));
		tmp->path = realPath;
		tmp->next = _visitedFile;
		_visitedFile = tmp;
	} else if (options->debugOutput) {
		FileUtils::printError("File %s was already processed\n", realPath);
		goto done;
	}

	printf("Processing tdf file %s\n", currentTDFFile);

	rc = reader.init(currentTDFFile);
	if (RC_OK != rc) {
		FileUtils::printError("Failed to read from file: %s\n", currentTDFFile);
		goto done;
	}

	parser.init(&reader, options->treatWarningAsError);
	tdf = parser.parse();
	if (NULL == tdf) {
		rc = RC_FAILED;
		goto done;
	}

	tdf->fileName = strdup(currentTDFFile);
	if (NULL == tdf->fileName) {
		eprintf("Failed to allocate memory");
		rc = RC_FAILED;
		goto done;
	}

	groups = calculateGroups(tdf, &groupCount);
	if (NULL == groups) {
		FileUtils::printError("Failed to calculate tracepoint groups");
		rc = RC_FAILED;
		goto done;
	}

	TraceHeaderWriter hfw;
	DATFileWriter dfw;
	CFileWriter cfw;

	rc = hfw.writeOutputFiles(options, tdf);
	if (RC_OK != rc) {
		FileUtils::printError("Failed to generate header file for %s\n", currentTDFFile);
		goto done;
	}
	rc = dfw.writeOutputFiles(options, tdf);
	if (RC_OK != rc) {
		FileUtils::printError("Failed to generate DAT file for %s\n", currentTDFFile);
		goto done;
	}

	if (options->generateCFiles) {
		rc = cfw.writeOutputFiles(options, tdf, groups, groupCount);
		if (RC_OK != rc) {
			FileUtils::printError("Failed to generate C file for %s\n", currentTDFFile);
			goto done;
		}
	}

done:
	if (NULL != tdf) {
		Port::omrmem_free((void **)&tdf->fileName);
		Port::omrmem_free((void **)&tdf->header.datfilename);
		Port::omrmem_free((void **)&tdf->header.executable);
		Port::omrmem_free((void **)&tdf->header.submodules);

		J9TDFTracepoint *tp = tdf->tracepoints;
		while (NULL != tp) {
			J9TDFTracepoint *next = tp->nexttp;
			Port::omrmem_free((void **)&tp->name);
			if (NULL != tp->groups) {
				unsigned int groupIndex = 0;
				while (NULL != tp->groups[groupIndex]) {
					Port::omrmem_free((void **)&tp->groups[groupIndex]);
					groupIndex += 1;
				}
				tp->groups = NULL;
			}
			Port::omrmem_free((void **)&tp->format);
			Port::omrmem_free((void **)&tp->parameters);

			tp = next;
		}
	}

	if (NULL != groups) {
		freeGroups(groups);
		groups = NULL;
	}
	return rc;
}

J9TDFGroup *
TraceGen::getDefaultGroupSet(unsigned int *groupCount)
{
	J9TDFGroup *head = NULL;
	*groupCount = 0;

	/* pre-populate groups */
	J9TDFGroup *prePop = NULL;

	prePop = newGroup("Entry", groupCount);
	if (NULL == prePop) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	prePop->nextGroup = head;
	head = prePop;

	prePop = newGroup("Exit", groupCount);
	if (NULL == prePop) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	prePop->nextGroup = head;
	head = prePop;

	prePop = newGroup("Event", groupCount);
	if (NULL == prePop) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	prePop->nextGroup = head;
	head = prePop;

	prePop = newGroup("Exception", groupCount);
	if (NULL == prePop) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	prePop->nextGroup = head;
	head = prePop;

	prePop = newGroup("Debug", groupCount);
	if (NULL == prePop) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	prePop->nextGroup = head;
	head = prePop;

	return head;

failed:
	freeGroups(head);
	head = NULL;
	return NULL;
}

RCType
TraceGen::addTpIdToGroup(J9TDFGroup *group, unsigned int id)
{
	J9TDFGroupTp *newGroupTp = (J9TDFGroupTp *)Port::omrmem_calloc(1, sizeof(J9TDFGroupTp));
	if (NULL == newGroupTp) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	newGroupTp->id = id;

	if (NULL == group->groupTpIds) {
		newGroupTp->next = NULL;
		group->groupTpIds = newGroupTp;
	} else {
		J9TDFGroupTp *curr = group->groupTpIds;
		while (NULL != curr->next) {
			curr = curr->next;
		}
		newGroupTp->next = NULL;
		curr->next = newGroupTp;
	}
	return RC_OK;
failed:
	return RC_FAILED;
}

J9TDFGroup *
TraceGen::findGroupByName(J9TDFGroup *head, const char *groupName)
{
	J9TDFGroup *matchGroup = head;
	J9TDFGroup *result = NULL;
	while (NULL != matchGroup) {
		if (0 == strcmp(groupName, matchGroup->name)) {
			result = matchGroup;
			break;
		}
		matchGroup = matchGroup->nextGroup;
	}
	return result;
}



J9TDFGroup *
TraceGen::newGroup(const char *groupName, unsigned int *groupCount)
{
	J9TDFGroup *newGroup = (J9TDFGroup *)Port::omrmem_calloc(1, sizeof(J9TDFGroup));
	if (NULL == newGroup) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	newGroup->name = strdup(groupName);
	if (NULL == newGroup->name) {
		goto failed;
	}
	*groupCount += 1;
	return newGroup;

failed:
	if (NULL != newGroup) {
		Port::omrmem_free((void **)&newGroup->name);
		Port::omrmem_free((void **)&newGroup);
	}
	return NULL;
}

J9TDFGroup *
TraceGen::calculateGroups(J9TDFFile *tdf, unsigned int *groupCount)
{
	const char *EVENT_TYPES[] = {
		"Event",
		"Exception",
		"Entry",
		"Entry-Exception",
		"Exit",
		"Exit-Exception",
		"Mem",
		"MemException",
		"Debug",
		"Debug-Exception",
		"Perf",
		"Perf-Exception",
		"Assertion"
	};

	J9TDFTracepoint *tp = tdf->tracepoints;
	*groupCount = 0;
	unsigned int tpId = 0;

	J9TDFGroup *head = getDefaultGroupSet(groupCount);
	if (NULL == head) {
		goto failed;
	}


	printf("Calculating groups for %s\n", tdf->header.executable);

	/* Add tracepoint ID to tracegroup */
	while (NULL != tp) {
		unsigned int groupIndex = 0;

		/* add TP id to each group it belongs to. */
		if (NULL != tp->groups) {
			while (NULL != tp->groups[groupIndex]) {
				const char *groupName = tp->groups[groupIndex];
				J9TDFGroup *matchGroup = findGroupByName(head, groupName);
				if (NULL == matchGroup) {
					matchGroup = newGroup(groupName, groupCount);
					if (NULL == matchGroup) {
						goto failed;
					}
					if (NULL == head) {
						matchGroup->nextGroup = NULL;
						head = matchGroup;
					} else {
						matchGroup->nextGroup = head;
						head = matchGroup;
					}
				}

				if (RC_OK != addTpIdToGroup(matchGroup, tpId)) {
					goto failed;
				}
				groupIndex += 1;
			}
		}

		/* Add TP id to TP event type group */
		const char *eventType = EVENT_TYPES[tp->type];
		J9TDFGroup *matchGroup = findGroupByName(head, eventType);
		if (NULL == matchGroup) {
			matchGroup = newGroup(eventType, groupCount);
			if (NULL == matchGroup) {
				goto failed;
			}
			if (NULL == head) {
				matchGroup->nextGroup = NULL;
				head = matchGroup;
			} else {
				matchGroup->nextGroup = head;
				head = matchGroup;
			}
		}
		if (RC_OK != addTpIdToGroup(matchGroup, tpId)) {
			goto failed;
		}

		tpId += 1;
		tp = tp->nexttp;
	}
	return head;

failed:
	if (NULL != head) {
		freeGroups(head);
		head = NULL;
	}
	return NULL;
}

RCType
TraceGen::freeGroups(J9TDFGroup *head)
{
	J9TDFGroup *currGroup = head;
	while (NULL != currGroup) {
		J9TDFGroup *tmpGroup = currGroup->nextGroup;
		/* free trace group tracepoints */
		J9TDFGroupTp *currTp = currGroup->groupTpIds;

		while (NULL != currTp) {
			J9TDFGroupTp *tmpTp = currTp->next;
			Port::omrmem_free((void **)&currTp);
			currTp = tmpTp;
		}

		Port::omrmem_free((void **)&currGroup);
		currGroup = tmpGroup;
	}
	return RC_OK;
}


RCType
TraceGen::generateCallBack(void *targetObject, J9TDFOptions *options, const char *fileName)
{
	TraceGen *self = (TraceGen *) targetObject;

	printf("TraceGen found tdf file %s\n", fileName);

	return self->generate(options, fileName);
}
