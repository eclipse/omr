/*******************************************************************************
 *
2 * (c) Copyright IBM Corp. 2016
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

#if defined(AIXPPC) || defined(J9ZOS390)
#define __IBMCPP_TR1__ 1
#endif /* defined(AIXPPC) || defined(J9ZOS390) */

#include <stdlib.h>
#include <string.h>
#include <vector>

#include "omrport.h"
#include "thread_api.h"

#if defined(_MSC_VER)
#include "PdbScanner.hpp"
#else /* defined(_MSC_VER) */
#include "DwarfScanner.hpp"
#endif /* defined(_MSC_VER) */
#include "genBlob.hpp"
#include "MacroTool.hpp"
#include "Scanner.hpp"
#include "Symbol_IR.hpp"

DDR_RC getOptions(OMRPortLibrary *portLibrary, int argc, char *argv[], const char **macroFile, const char **supersetFile,
	const char **blobFile, const char **overrideFile, vector<string> *debugFiles);
DDR_RC readFileList(OMRPortLibrary *portLibrary, const char *debugFileList, vector<string> *debugFiles);

int
main(int argc, char *argv[])
{
	omrthread_attach(NULL);

	OMRPortLibrary portLibrary;
	omrport_init_library(&portLibrary, sizeof(portLibrary));
	DDR_RC rc = DDR_RC_OK;

	/* Get options. */
	const char *macroFile = "src/macros/test/macroList";
	const char *supersetFile = "superset.out";
	const char *blobFile = "blob.dat";
	const char *overrideFile = NULL;
	vector<string> debugFiles;
	rc = getOptions(&portLibrary, argc, argv, &macroFile, &supersetFile, &blobFile, &overrideFile, &debugFiles);

	/* Create IR from input. */
#if defined(_MSC_VER)
	PdbScanner scanner;
#else /* defined(_MSC_VER) */
	DwarfScanner scanner;
#endif /* defined(_MSC_VER) */
	Symbol_IR ir;
	if ((DDR_RC_OK == rc) && !debugFiles.empty()) {
		rc = scanner.startScan(&portLibrary, &ir, &debugFiles);
	}

	/* Compute field offsets for UDTs. */
	if (DDR_RC_OK == rc) {
		rc = ir.computeOffsets();
	}
#if !defined(AIXPPC)
	/* Remove duplicate types. */
	if (DDR_RC_OK == rc) {
		rc = ir.removeDuplicates();
	}
#endif /* !defined(AIXPPC) */
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
		rc = genBlob(&portLibrary, &ir, supersetFile, blobFile);
	}

	portLibrary.port_shutdown_library(&portLibrary);
	omrthread_detach(NULL);
	omrthread_shutdown_library();
	return 0;
}

DDR_RC
getOptions(OMRPortLibrary *portLibrary, int argc, char *argv[], const char **macroFile, const char **supersetFile,
	const char **blobFile, const char **overrideFile, vector<string> *debugFiles)
{
	DDR_RC rc = DDR_RC_OK;
	bool showHelp = (argc < 2);
	bool showVersion = false;
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
