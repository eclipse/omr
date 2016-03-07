/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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

#ifndef TDFTYPES_HPP_
#define TDFTYPES_HPP_

#include "EventTypes.hpp"
#include "Port.hpp"

#define UT_ENV_PARAM "UT_THREAD(thr)"
#define UT_NOENV_PARAM "(void *)NULL"

/**
 * Define the ut_ prefix for all generated trace files.
 */
#define UT_FILENAME_PREFIX "ut_"

/* Linked list if TP ids for J9TDFGroup */
typedef struct J9TDFGroupTp {
	unsigned int id;
	struct J9TDFGroupTp *next;
} J9TDFGroupTp;


/* Trace group and link-list of TP ids that belong to this group*/
typedef struct J9TDFGroup {
	const char *name;
	J9TDFGroupTp *groupTpIds;
	struct J9TDFGroup *nextGroup;
	J9TDFGroup()
		: name(NULL)
		, groupTpIds(NULL)
		, nextGroup(NULL)
	{
	}
} J9TDFGroup;

/* TDF file header */
typedef struct J9TDFHeader {
	const char *executable;
	const char *submodules;
	char *datfilename;
	bool auxiliary;
	J9TDFHeader()
		: executable(NULL)
		, submodules(NULL)
		, datfilename(NULL)
		, auxiliary(false)
	{
	}
} J9TDFHeader;

/* TDF File TP structure */
typedef struct J9TDFTracepoint {
	TraceEventType type;
	unsigned int overhead;
	unsigned int level;
	unsigned int parmCount;
	bool hasEnv;
	bool obsolete;
	bool test;
	bool isExplicit;
	char *name;
	char **groups;
	char *format;
	char *parameters;
	struct J9TDFTracepoint *nexttp;
	J9TDFTracepoint()
		: type(UT_EVENT_TYPE)
		, overhead(0)
		, level(0)
		, parmCount(0)
		, hasEnv(false)
		, obsolete(false)
		, test(false)
		, isExplicit(false)
		, name(NULL)
		, groups(NULL)
		, format(NULL)
		, parameters(NULL)
		, nexttp(NULL)
	{
	}
} J9TDFTracepoint;

/* Parsed TDF file representation */
typedef struct J9TDFFile {
	const char *fileName;
	J9TDFHeader header;
	J9TDFTracepoint *tracepoints;
	J9TDFTracepoint *lasttp;
	J9TDFFile()
		: fileName(NULL)
		, tracepoints(NULL)
		, lasttp(NULL)
	{
	}
} J9TDFFile;

/* Location of a file or a directory in a file system */
typedef struct Path {
	const char *path;
	Path *next;
	Path()
		: path(NULL)
		, next(NULL)
	{
	}
} Path;

/* Command line options */
typedef struct J9TDFOptions {
	unsigned int rasMajorVersion;
	unsigned int rasMinorVersion;
	unsigned int threshold;

	bool force;
	bool generateCFiles;
	bool writeToCurrentDir;

	Path *rootDirectory;

	/* debug options */
	bool debugOutput;
	bool treatWarningAsError;

	J9TDFOptions()
		: rasMajorVersion(5)
		, rasMinorVersion(1)
		, threshold(1)
		, force(false)
		, generateCFiles(false)
		, writeToCurrentDir(false)
		, rootDirectory(NULL)
		, debugOutput(false)
		, treatWarningAsError(false)
	{
	}
} J9TDFOptions;

#endif /* TDFTYPES_HPP_ */
