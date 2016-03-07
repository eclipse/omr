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
