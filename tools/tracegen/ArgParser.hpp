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

#ifndef ARGPARSER_HPP_
#define ARGPARSER_HPP_

#include "Port.hpp"
#include "TDFTypes.hpp"

class ArgParser
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
protected:
public:
	/**
	 * Parse command line options, command line arguments are reused from argv.
	 * @param argc Argument count
	 * @param argv array of arguments
	 * @param options Options data-structure to populate
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType parseOptions(int argc, char *argv[], J9TDFOptions *options);

	/**
	 * Free command line options structure
	 * @param options Options data-structure to free
	 */
	RCType freeOptions(J9TDFOptions *options);
};

#endif /* ARGPARSER_HPP_ */
