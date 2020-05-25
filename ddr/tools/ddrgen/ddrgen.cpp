/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#include "ddr/config.hpp"

#include <stdlib.h>
#include <string.h>
#include <vector>

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
#include "atoe.h"
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

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
#include "ddr/ir/TextFile.hpp"

struct Options
{
	const char *macroFile;
	const char *supersetFile;
	const char *blobFile;
	const char *overrideListFile;
	vector<string> debugFiles;
	const char *blacklistFile;
	bool printEmptyTypes;
	bool showBlacklisted;

	Options()
		: macroFile(NULL)
		, supersetFile(NULL)
		, blobFile(NULL)
		, overrideListFile(NULL)
		, debugFiles()
		, blacklistFile(NULL)
		, printEmptyTypes(false)
		, showBlacklisted(false)
	{
	}

	DDR_RC configure(OMRPortLibrary *portLibrary, int argc, char *argv[]);

private:
	DDR_RC readFileList(OMRPortLibrary *portLibrary, const char *listFileName, vector<string> *fileNameList);
};

#undef DEBUG_PRINT_TYPES

#if defined(DEBUG_PRINT_TYPES)
#include "ddr/ir/TypePrinter.hpp"
#endif /* DEBUG_PRINT_TYPES */

int
main(int argc, char *argv[])
{
	omrthread_attach(NULL);

	OMRPortLibrary portLibrary;

	if (0 != omrport_init_library(&portLibrary, sizeof(portLibrary))) {
		fprintf(stderr, "failed to initalize port library\n");
		return 1;
	}

	DDR_RC rc = DDR_RC_OK;

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
	/* Convert EBCDIC to UTF-8 (ASCII) */
	if (-1 != iconv_init()) {
		/* translate argv strings to ASCII */
		for (int i = 0; i < argc; ++i) {
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
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

	/* Get options. */
	Options options;

	if (DDR_RC_OK == rc) {
		rc = options.configure(&portLibrary, argc, argv);
	}

	/* Create IR from input. */
#if defined(_MSC_VER)
	PdbScanner scanner;
#else /* defined(_MSC_VER) */
	DwarfScanner scanner;
#endif /* defined(_MSC_VER) */
	Symbol_IR ir(&portLibrary);
#if defined(DEBUG_PRINT_TYPES)
	const TypePrinter printer(&portLibrary, TypePrinter::FIELDS | TypePrinter::LITERALS | TypePrinter::MACROS);
#endif /* DEBUG_PRINT_TYPES */

	if ((DDR_RC_OK == rc) && !options.debugFiles.empty()) {
		rc = scanner.startScan(&portLibrary, &ir, &options.debugFiles, options.blacklistFile);

#if defined(DEBUG_PRINT_TYPES)
		OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);
		omrtty_printf("== scan results ==\n");
		for (vector<Type *>::const_iterator type = ir._types.begin(); type != ir._types.end(); ++type) {
			(*type)->acceptVisitor(printer);
		}
#endif /* DEBUG_PRINT_TYPES */
	}

	if (DDR_RC_OK == rc) {
		/* Remove duplicate types. */
		ir.removeDuplicates();

#if defined(DEBUG_PRINT_TYPES)
		OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);
		omrtty_printf("== after removing duplicates ==\n");
		for (vector<Type *>::const_iterator type = ir._types.begin(); type != ir._types.end(); ++type) {
			(*type)->acceptVisitor(printer);
		}
#endif /* DEBUG_PRINT_TYPES */
	}

	/* Read macros. */
	if ((DDR_RC_OK == rc) && (NULL != options.macroFile)) {
		MacroTool macroTool;

		rc = macroTool.getMacros(&portLibrary, options.macroFile);
		/* Add Macros to IR. */
		if (DDR_RC_OK == rc) {
			rc = macroTool.addMacrosToIR(&ir);
		}
	}

	/* Apply Type Overrides; must be after scanning and loading macros. */
	if ((DDR_RC_OK == rc) && (NULL != options.overrideListFile)) {
		rc = ir.applyOverridesList(options.overrideListFile);
	}

	if ((DDR_RC_OK == rc) && options.showBlacklisted) {
		OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);
		bool none = true;

		for (vector<Type *>::const_iterator type = ir._types.begin(); type != ir._types.end(); ++type) {
			if ((*type)->_blacklisted) {
				if (none) {
					none = false;
					omrtty_printf("Blacklisted types:\n");
				}
				omrtty_printf("  %s\n", (*type)->_name.c_str());
			}
		}

		if (none) {
			omrtty_printf("No blacklisted types.\n");
		}
	}

	/* Generate output. */
	if ((DDR_RC_OK == rc) && !ir._types.empty()) {
		rc = genBlob(&portLibrary, &ir, options.supersetFile, options.blobFile, options.printEmptyTypes);
	}

	portLibrary.port_shutdown_library(&portLibrary);
	omrthread_detach(NULL);
	omrthread_shutdown_library();

	return (DDR_RC_OK == rc) ? 0 : 1;
}

static bool
matchesEither(const char *string, const char *match1, const char *match2)
{
	return (0 == strcmp(string, match1)) || (0 == strcmp(string, match2));
}

DDR_RC
Options::configure(OMRPortLibrary *portLibrary, int argc, char *argv[])
{
	DDR_RC rc = DDR_RC_OK;
	bool showHelp = (argc < 2);
	bool showVersion = false;
	for (int i = 1; i < argc; ++i) {
		if (matchesEither(argv[i], "-f", "--filelist")) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				rc = readFileList(portLibrary, argv[++i], &debugFiles);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		} else if (matchesEither(argv[i], "-m", "--macrolist")) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				macroFile = argv[++i];
			}
		} else if (matchesEither(argv[i], "-s", "--superset")) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				supersetFile = argv[++i];
			}
		} else if (matchesEither(argv[i], "-b", "--blob")) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				blobFile = argv[++i];
			}
		} else if (matchesEither(argv[i], "-o", "--overrides")) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				overrideListFile = argv[++i];
			}
		} else if (matchesEither(argv[i], "-l", "--blacklist")) {
			if (argc < i + 2) {
				showHelp = true;
			} else {
				blacklistFile = argv[++i];
			}
		} else if (matchesEither(argv[i], "-e", "--show-empty")) {
			printEmptyTypes = true;
		} else if (matchesEither(argv[i], "-sb", "--show-blacklisted")) {
			showBlacklisted = true;
		} else if (matchesEither(argv[i], "-v", "--version")) {
			showVersion = true;
		} else if ('-' == argv[i][0]) {
			showHelp = true;
		} else {
			debugFiles.push_back(argv[i]);
		}
	}

	if (showHelp) {
		printf(
			"USAGE\n"
			"  ddrgen [OPTIONS] files ...\n"
			"OPTIONS\n"
			"  -h, --help\n"
			"      Prints this message.\n"
			"  -v, --version\n"
			"      Prints the version information.\n"
			"  -f FILE, --filelist FILE\n"
			"      Specify file containing list of input files\n"
			"  -m FILE, --macrolist FILE\n"
			"      Specify input list of macros. See macro tool for format.\n"
			"      Default is macroList in current directory.\n"
			"  -s FILE, --superset FILE\n"
			"      Output superset file.\n"
			"  -b FILE, --blob FILE\n"
			"      Output binary blob file.\n"
			"  -o FILE, --overrides FILE\n"
			"      Optional file containing a list of files which contain rules\n"
			"      modifying the default treatment of types.\n"
			"      A typedef is normally expanded unless named in an override\n"
			"        opaquetype=TypeName\n"
			"      Field names or their types can be overridden with\n"
			"        fieldoverride.TypeName.FieldName=NewFieldName\n"
			"      or\n"
			"        typeoverride.TypeName.FieldName=NewFieldType\n"
			"  -l FILE, --blacklist FILE\n"
			"      Optional file containing list of type names and source file paths to\n"
			"      ignore. Format is 'file:[filename]' or 'type:[typename]' on each line.\n"
			"  -e, --show-empty\n"
			"      Print structures, enums, and unions to the superset and blob even if\n"
			"      they do not contain any fields. The default behaviour is to hide them.\n"
			"  -sb, --show-blacklisted\n"
			"      Print names of types that were blacklisted.\n"
		  );
	} else if (showVersion) {
		printf("Version 0.1\n");
	}
	return rc;
}

DDR_RC
Options::readFileList(OMRPortLibrary *portLibrary, const char *listFileName, vector<string> *fileNameList)
{
	DDR_RC rc = DDR_RC_ERROR;

	/* Read list of debug files to scan from the input file. */
	TextFile filelist(portLibrary);

	if (!filelist.openRead(listFileName)) {
		ERRMSG("Failure attempting to open %s; exiting...", listFileName);
	} else {
		string fileName;

		while (filelist.readLine(fileName)) {
			if (!fileName.empty()) {
				fileNameList->push_back(fileName);
			}
		}

		filelist.close();
		rc = DDR_RC_OK;
	}

	return rc;
}
