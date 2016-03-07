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

#ifndef TRACEHEADERWRITER_HPP_
#define TRACEHEADERWRITER_HPP_

#include "Port.hpp"
#include "TDFTypes.hpp"

class TraceHeaderWriter
{
	/*
	 * Data members
	 */
private:
protected:
public:

	/*
	 * Function members
	 */
private:
	/**
	 * Output trace point
	 * @param fd Output stream
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType tpTemplate(FILE *fd, unsigned int overhead, unsigned int test, const char *name, const char *module, unsigned int id, unsigned int envparam, const char *format, unsigned int formatParamCount, unsigned int auxiliary);

	/**
	 *  Output assertion
	 *  @param fd Output stream
	 *  @return RC_OK on success, RC_FAILED on failure
	 */
	RCType tpAssert(FILE *fd, unsigned int overhead, unsigned int test, const char *name, const char *module, unsigned int id, unsigned int envparam, const char *format, unsigned int formatParamCount);

	/**
	 * Output file header
	 * @param fd Output stream
	 * @param moduleName
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType headerTemplate(J9TDFOptions *options, FILE *fd, const char *moduleName);

	/**
	 * Output file footer
	 * @param fd Output stream
	 * @param moduleName
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType footerTemplate(FILE *fd, const char *moduleName);
protected:
public:
	/**
	 * Output header file
	 * @param options Command line options
	 * @param tdf Parsed TDF data
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeOutputFiles(J9TDFOptions *options, J9TDFFile *tdf);
};

#endif /* TRACEHEADERWRITER_HPP_ */
