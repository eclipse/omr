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
