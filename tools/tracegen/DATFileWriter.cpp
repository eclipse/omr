/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DATFileWriter.hpp"
#include "EventTypes.hpp"
#include "FileReader.hpp"
#include "FileUtils.hpp"
#include "StringUtils.hpp"


RCType
DATFileWriter::writeOutputFiles(J9TDFOptions *options, J9TDFFile *tdf)
{
	RCType rc = RC_FAILED;
	const char *datLineTemplate = "%s.%u %u %u %u %c %s \"%c%c%s\"\n";
	J9TDFTracepoint *tp = tdf->tracepoints;
	unsigned int id = 0;
	char exceptChar = ' ';
	char entryExitChar = ' ';
	char explicitChar = 'N';
	const char *format = NULL;

	const char *fileName = FileUtils::getTargetFileName(options, tdf->fileName, UT_FILENAME_PREFIX, tdf->header.executable, ".pdat");

	time_t sourceFileMtime = FileUtils::getMtime(tdf->fileName);
	time_t targetFileMtime = FileUtils::getMtime(fileName);

	if ((false == options->force) && (targetFileMtime > sourceFileMtime)) {
		if (options->verboseOutput) {
			printf("pdat file is already up-to-date: %s\n", fileName);
		}
		Port::omrmem_free((void **)&fileName);
		return RC_OK;
	}

	if (options->verboseOutput) {
		printf("Creating pdat file: %s\n", fileName);
	}

	FILE *datFile = Port::fopen(fileName, "w");
	if (NULL == datFile) {
		eprintf("Failed to open file %s", fileName);
		goto failed;
	}

	/* Partial DAT header */
	fprintf(datFile, "datfilename=%s\n", tdf->header.datfilename);
	fprintf(datFile, "executable=%s\n", tdf->header.executable);

	/* Partial DAT trace data */
	if (NULL != datFile) {
		while (NULL != tp) {
			/* Write to dat file. */
			switch (tp->type) {
			case UT_EXCEPTION_TYPE:
			case UT_ENTRY_EXCPT_TYPE:
			case UT_EXIT_EXCPT_TYPE:
			case UT_MEM_EXCPT_TYPE:
			case UT_DEBUG_EXCPT_TYPE:
			case UT_PERF_EXCPT_TYPE:
			case UT_ASSERT_TYPE:
				exceptChar = '*';
				break;
			default:
				exceptChar = ' ';
				break;
			}

			switch (tp->type) {
			case UT_ENTRY_TYPE:
			case UT_ENTRY_EXCPT_TYPE:
				entryExitChar = '>';
				break;
			case UT_EXIT_TYPE:
			case UT_EXIT_EXCPT_TYPE:
				entryExitChar = '<';
				break;
			default:
				entryExitChar = ' ';
				break;
			}

			if (UT_ASSERT_TYPE == tp->type) {
				format = (char *) "** ASSERTION FAILED ** at %s:%d: %s";
			} else {
				format = tp->format;
			}

			if (tp->isExplicit) {
				explicitChar = 'Y';
			}

			fprintf(datFile, datLineTemplate, tdf->header.executable, id, tp->type, tp->overhead, tp->level, explicitChar, tp->name, exceptChar, entryExitChar, format);
			tp = tp->nexttp;
			id += 1;
		}
		fclose(datFile);
		rc = RC_OK;
	}

	Port::omrmem_free((void **)&fileName);

	return rc;

failed:
	Port::omrmem_free((void **)&fileName);
	return rc;
}
