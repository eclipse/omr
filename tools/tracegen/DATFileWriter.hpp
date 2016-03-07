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

#ifndef DATFILEWRITER_HPP_
#define DATFILEWRITER_HPP_

#include "Port.hpp"
#include "TDFTypes.hpp"

class DATFileWriter
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
	 * Output trace DAT file.
	 * @param options Command line options
	 * @param tdf Parsed TDF file
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeOutputFiles(J9TDFOptions *options, J9TDFFile *tdf);
};
#endif /* DATFILEWRITER_HPP_ */
