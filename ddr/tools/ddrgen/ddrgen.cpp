/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#if defined(AIXPPC) || defined(J9ZOS390)
#define __IBMCPP_TR1__ 1
#endif /* defined(AIXPPC) || defined(J9ZOS390) */

#include <stdlib.h>
#include <string.h>
#include <vector>

#if defined(J9ZOS390)
#include "atoe.h"
#endif /* defined(J9ZOS390) */

#include "omrport.h"
#include "thread_api.h"

#if defined(_MSC_VER)
#include "ddr/scanner/pdb/PdbScanner.hpp"
#else /* defined(_MSC_VER) */
#include "ddr/scanner/dwarf/DwarfScanner.hpp"
#endif /* defined(_MSC_VER) */
#include "ddr/blobgen/genBlob.hpp"
#include "ddr/macros/MacroTool.hpp"
#include "ddr/scanner/Scanner.hpp"
#include "ddr/ir/Symbol_IR.hpp"

DDR_RC getOptions(OMRPortLibrary *portLibrary, int argc, char *argv[], const char **macroFile, const char **supersetFile,
	const char **blobFile, const char **overrideFile, vector<string> *debugFiles, const char **blacklistFile, bool *printEmptyTypes);
DDR_RC readFileList(OMRPortLibrary *portLibrary, const char *debugFileList, vector<string> *debugFiles);

int
main(int argc, char *argv[])
{
	omrthread_attach(NULL);

	OMRPortLibrary portLibrary;
	omrport_init_library(&portLibrary, sizeof(portLibrary));
	DDR_RC rc = DDR_RC_OK;

#if defined(J9ZOS390)
	/* Convert EBCDIC to UTF-8 (ASCII) */
	if (-1 != iconv_init()) {
		/* translate argv strings to ascii */
		for (int i = 0; i < argc; i++) {
			argv[i] = e2a_string(argv[i]);
			if (NULL == argv[i]) {
				fprintf(stderr, "failed to convert argument #%d from EBCDIC to ASCII\n", i);
				rc = DDR_RC_ERROR;
				break;
			}
		}
	} else {
		fprintf(stderr, "failed to initialize iconv\n");
		rc = DDR_RC_ERROR;
	}
#endif /* defined(J9ZOS390) */

	/* Get options. */
	const char *macroFile = NULL;
	const char *supersetFile = "superset.out";
	const char *blobFile = "blob.dat";
	const char *overrideFile = NULL;
	const char *blacklistFile = NULL;
	bool printEmptyTypes;
	vector<string> debugFiles;
	if (DDR_RC_OK == rc) {
		rc = getOptions(&portLibrary, argc, argv, &macroFile, &supersetFile,
			&blobFile, &overrideFile, &debugFiles, &blacklistFile, &printEmptyTypes);
	}

	/* Create IR from input. */
#if defined(_MSC_VER)
	PdbScanner scanner;
#else /* defined(_MSC_VER) */
	DwarfScanner scanner;
#endif /* defined(_MSC_VER) */
	Symbol_IR ir;
	if ((DDR_RC_OK == rc) && !debugFiles.empty()) {
		string blacklistPath = "";
		if (blacklistFile != NULL) {
			blacklistPath = string(blacklistFile);
		}
		rc = scanner.startScan(&portLibrary, &ir, &debugFiles, blacklistPath);
	}

	if (DDR_RC_OK == rc) {
		/* Compute field offsets for UDTs. */
		ir.computeOffsets();
		/* Remove duplicate types. */
		ir.removeDuplicates();
	}
	MacroTool macroTool;
	/* Read macros. */
	if ((DDR_RC_OK == rc) && (NULL != macroFile)) {
		rc = macroTool.getMacros(macroFile);
	}
	/* Add Macros to IR. */
	if (DDR_RC_OK == rc) {
		rc = macroTool.addMacrosToIR(&ir);
	}

	/* Apply Type Overrides; must be after scanning and loading macros. */
	if ((DDR_RC_OK == rc) && (NULL != overrideFile)) {
		rc = ir.applyOverrideList(&portLibrary, overrideFile);
	}

	/* Generate output. */
	if ((DDR_RC_OK == rc) && !ir._types.empty()) {
		rc = genBlob(&portLibrary, &ir, supersetFile, blobFile, printEmptyTypes);
	}

	portLibrary.port_shutdown_library(&portLibrary);
	omrthread_detach(NULL);
	omrthread_shutdown_library();
	return 0;
}

DDR_RC
getOptions(OMRPortLibrary *portLibrary, int argc, char *argv[], const char **macroFile, const char **supersetFile,
	const char **blobFile, const char **overrideFile, vector<string> *debugFiles, const char **blacklistFile,
	bool *printEmptyTypes)
{
	DDR_RC rc = DDR_RC_OK;
	bool showHelp = (argc < 2);
	bool showVersion = false;
	*printEmptyTypes = false;
	for (int i = 1; i < argc; i += 1) {
		if ((0 == strcmp(argv[i], "--filelist"))
			|| (0 == strcmp(argv[i], "-f"))
		) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				rc = readFileList(portLibrary, argv[++i], debugFiles);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		} else if ((0 == strcmp(argv[i], "--macrolist"))
			|| (0 == strcmp(argv[i], "-m"))
		) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				*macroFile = argv[++i];
			}
		} else if ((0 == strcmp(argv[i], "--superset"))
			|| (0 == strcmp(argv[i], "-s"))
		) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				*supersetFile = argv[++i];
			}
		} else if ((0 == strcmp(argv[i], "--blob"))
			|| (0 == strcmp(argv[i], "-b"))
		) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				*blobFile = argv[++i];
			}
		} else if ((0 == strcmp(argv[i], "--overrides"))
			|| (0 == strcmp(argv[i], "-o"))
		) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				*overrideFile = argv[++i];
			}
		} else if ((0 == strcmp(argv[i], "--blacklist"))
			|| (0 == strcmp(argv[i], "-l"))
		) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				*blacklistFile = argv[++i];
			}
		} else if ((0 == strcmp(argv[i], "--show-empty"))
			|| (0 == strcmp(argv[i], "-e"))
		) {
			*printEmptyTypes = true;
		} else if ((0 == strcmp(argv[i], "--help"))
			|| (0 == strcmp(argv[i], "-h"))
		) {
			showHelp = true;
		} else if ((0 == strcmp(argv[i], "--version"))
			|| (0 == strcmp(argv[i], "-v"))
		) {
			showVersion = true;
		} else {
			debugFiles->push_back(argv[i]);
		}
	}

	if (showHelp) {
		printf(
			"USAGE\n"
			"	ddrgen [OPTIONS] files ...\n"
			"OPTIONS\n"
			"	-h, --help\n"
			"		Prints this message\n"
			"	-v, --version\n"
			"		Prints the version information.\n"
			"	-f FILE, --filelist FILE\n"
			"		Specify file containing list of input files\n"
			"	-m FILE, --macrolist FILE\n"
			"		Specify input list of macros. See macro tool for format. Default is macroList in current directory.\n"
			"	-s FILE, --superset FILE\n"
			"		Output superset file. Default is superset.out in current directory.\n"
			"	-b FILE, --blob FILE\n"
			"		Output binary blob file. Default is blob.dat in current directory.\n"
			"	-o FILE, --overrides FILE\n"
			"		Optional. File containing a list of files which contain type and field overrides for specific fields.\n"
			"		Overrides are of the format 'typeoverride.STRUCT_NAME.FIELD_NAME=TypeToChangeTo' or\n"
			"		'fieldoverride.STRUCT_NAME.FIELD_NAME=NewFieldName'\n"
			"	-l FILE, --blacklist FILE\n"
			"		Optional. File containing list of type names and source file paths to ignore.\n"
			"		Format is 'file:[filename]' or 'type:[typename]' on each line.\n"
			"	-e, --show-empty\n"
			"		Print structures, enums, and unions to the superset and blob even if they do not contain any fields.\n"
			"		The default behaviour is to hide them.\n"
		);
	} else if (showVersion) {
		printf("Version 0.1\n");
	}
	return rc;
}

DDR_RC
readFileList(OMRPortLibrary *portLibrary, const char *debugFileList, vector<string> *debugFiles)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	DDR_RC rc = DDR_RC_OK;

	/* Read list of debug files to scan from the input file. */
	intptr_t fd = omrfile_open(debugFileList,  EsOpenRead, 0660);
	if (0 > fd) {
		ERRMSG("Failure attempting to open %s\nExiting...\n", debugFileList);
		rc = DDR_RC_ERROR;
	} else {
		char *buff = NULL;
		int64_t offset = omrfile_seek(fd, 0, SEEK_END);
		if (-1 != offset) {
			buff = (char *)malloc(offset + 1);
			memset(buff, 0, offset + 1);
			omrfile_seek(fd, 0, SEEK_SET);

			if (0 < omrfile_read(fd, buff, offset)) {
				char *fileName = strtok(buff, "\n");
				while (NULL != fileName) {
					debugFiles->push_back(string(fileName));
					fileName = strtok(NULL, "\n");
				}
			}
			free(buff);
		}
	}
	return rc;
}
