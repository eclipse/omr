/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef TDFPARSER_HPP_
#define TDFPARSER_HPP_

#include "FileReader.hpp"
#include "Port.hpp"
#include "TDFTypes.hpp"

class TDFParser
{
private:
	/*
	 * Data members
	 */
private:
	FileReader *_fileReader;
	bool _treatWarningAsError;
protected:
public:

	/*
	 * Function members
	 */
private:
	/**
	 * Find trace type number from trace name
	 * @param traceLine Trace line
	 * @return Trace type number or RC_FAILED if not found
	 */
	RCType findTraceTypeIndex(const char *traceLine, int *result);

	/**
	 * @param line
	 * @param tp
	 * @param module
	 * @param id
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType processTracePointDetail(const char *line, J9TDFTracepoint *tp, const char *module, unsigned int id, const char *fileName, unsigned int lineNumber);

	/**
	 * @param line
	 */
	bool isNoEnv(const char *line);

	/**
	 * @param line
	 */
	bool isObsolete(const char *line);

	/**
	 * @param line
	 */
	bool isTest(const char *line);

	/**
	 * @param line
	 */
	char **getGroups(const char *line);

	/**
	 * @param line
	 * @param key
	 */
	char *getTemplate(const char *line, const char *key);

	/**
	 * @param formatTemplate
	 * @param count
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType processAssertTemplate(const char *formatTemplate, unsigned int *count);

	/**
	 * @param formatTemplate
	 * @param str
	 * @param count
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType processTemplate(const char *formatTemplate, char *str, unsigned int *count, const char *fileName, unsigned int lineNumber);


	/**
	 * @param message
	 * @param parmCount
	 * @param formatTemplate
	 */
	void flagTdfSyntaxError(const char *message, unsigned int parmCount, const char *formatTemplate, const char *fileName, unsigned int lineNumber);
protected:
public:
	/*
	 * Process a tdf file one line at a time, create the
	 * structures we will later use to write out the various .h, .c and .dat
	 * files.
	 * @return parsed data from tdf file or NULL on failure
	 */
	J9TDFFile *parse();

	/**
	 * Initialize new instance of TDF parser
	 * @args reader reads input tdf file
	 * @args treatWarningAsError Fail parsing on any error
	 */
	void init(FileReader *reader, bool treatWarningAsError);
};

#endif /* TDFPARSER_HPP_ */
