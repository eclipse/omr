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
