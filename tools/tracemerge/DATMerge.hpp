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

#ifndef DATMERGE_HPP_
#define DATMERGE_HPP_

#include "TDFTypes.hpp"

RCType startTraceMerge(int argc, char *argv[]);

class DATMerge
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
	 * Merge partial DAT file into the target DAT
	 * @param options command-line options
	 * @param fromFileName Name of the partial DAT file
	 * @return RC_OK on success.
	 */
	RCType merge(J9TDFOptions *options, const char *fromFileName);
protected:
public:
	/**
	 * Start merging process, visit all subdirectories and merge all partial DAT files.
	 * @return RC_OK on success
	 */
	RCType start(J9TDFOptions *options);

	static RCType mergeCallback(void *targetObject, J9TDFOptions *options, const char *fromFileName);
};

#endif /* DATMERGE_HPP_ */
